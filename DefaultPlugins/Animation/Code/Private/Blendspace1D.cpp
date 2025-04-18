#include "Blendspace1D.h"
#include "SkeletonInstance.h"
#include "AnimationCache.h"
#include "Plugin.h"
#include "Components/SkeletonComponent.h"

#include <Engine/Threading/JobManager.h>
#include <Engine/Asset/AssetManager.h>

#include <Common/Serialization/Guid.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>
#include <Common/Memory/Containers/Serialization/Vector.h>
#include <Common/Memory/Containers/Serialization/ArrayView.h>
#include <Common/Memory/AddressOf.h>
#include <Common/System/Query.h>

namespace ngine::Animation
{
	namespace Internal
	{
		Blendspace1DBase::Blendspace1DBase(const uint16 sampledTransformCount, const uint8 blendedLayerCount)
			: m_layers(Memory::ConstructWithSize, Memory::DefaultConstruct, blendedLayerCount)
			, m_sampledTransforms(
					Memory::ConstructWithSize,
					Memory::InitializeAll,
					GetRequiredTransformCount(sampledTransformCount),
					ozz::math::SoaTransform::identity()
				)
		{
			ArrayView<ozz::math::SoaTransform, uint32> sampledTransforms = m_sampledTransforms;
			for (ozz::animation::BlendingJob::Layer& blendLayer : m_layers)
			{
				blendLayer.transform = ozz::span<ozz::math::SoaTransform>{sampledTransforms.GetData(), sampledTransforms.GetSize()};
				sampledTransforms += sampledTransformCount;
			}
		}

		void Blendspace1DBase::Process(
			const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses, const ArrayView<ozz::math::SoaTransform, uint16> outTransforms
		)
		{
			constexpr float bindPoseBlendThreshold = 0.1f;

			if (m_layers.GetSize() == 1)
			{
				outTransforms.CopyFrom(m_sampledTransforms.GetView());
			}
			else
			{
				ozz::animation::BlendingJob blendingJob;
				blendingJob.threshold = bindPoseBlendThreshold;
				blendingJob.layers = ozz::span<const ozz::animation::BlendingJob::Layer>{m_layers.GetData(), m_layers.GetSize()};
				blendingJob.bind_pose = ozz::span<const ozz::math::SoaTransform>{jointBindPoses.GetData(), jointBindPoses.GetSize()};
				blendingJob.output = ozz::span<ozz::math::SoaTransform>{outTransforms.GetData(), outTransforms.GetSize()};
				blendingJob.Run();
			}
		}

		void Blendspace1DBase::Initialize(const uint16 sampledTransformCount, const uint8 blendedLayerCount)
		{
			m_layers.Resize(blendedLayerCount);
			m_sampledTransforms.Resize(sampledTransformCount * blendedLayerCount);
			for (ozz::math::SoaTransform& transform : m_sampledTransforms)
			{
				transform = ozz::math::SoaTransform::identity();
			}

			ArrayView<ozz::math::SoaTransform, uint32> sampledTransforms = m_sampledTransforms;
			for (ozz::animation::BlendingJob::Layer& blendLayer : m_layers)
			{
				blendLayer.transform = ozz::span<ozz::math::SoaTransform>{sampledTransforms.GetData(), sampledTransforms.GetSize()};
				sampledTransforms += sampledTransformCount;
			}
		}

		void Blendspace1DBase::OnBlendRatioChanged()
		{
			// Calculate blend weights
			const float blendRatio = m_blendRatio;
			const ArrayView<ozz::animation::BlendingJob::Layer, uint8> layers = m_layers;
			Assert(layers.HasElements());
			const float intervalCount = float(layers.GetSize() - 1);
			const float intervalRatio = Math::MultiplicativeInverse(intervalCount);
			for (uint8 i = 0, n = layers.GetSize(); i < n; ++i)
			{
				const float med = i * intervalRatio;
				const float x = blendRatio - med;
				const float y = ((x < 0.f ? x : -x) + intervalRatio) * intervalCount;
				layers[i].weight = Math::Max(y, 0.f);
			}
		}
	}

	Blendspace1D::Blendspace1D(const Serialization::Reader animationsReader, SkeletonComponent& skeletonComponent)
	{
		Serialize(animationsReader, skeletonComponent);
	}

	Blendspace1D::Blendspace1D(const Optional<Serialization::Reader> animationsReader, SkeletonComponent& skeletonComponent)
	{
		if (animationsReader.IsValid())
		{
			Serialize(*animationsReader, skeletonComponent);
		}
		else if (const Optional<const Skeleton*> pSkeleton = skeletonComponent.GetSkeletonInstance().GetSkeleton();
		         pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged(pSkeleton->GetJointCount(), skeletonComponent.GetSkeletonInstance().GetSampledTransforms().GetSize());
		}
	}

	Blendspace1D::Blendspace1D(const ArrayView<const EntryInitializer, uint8> entryInitializers, SkeletonComponent& skeletonComponent)
		: Blendspace1DBase(skeletonComponent.GetSkeletonInstance().GetSampledTransforms().GetSize(), entryInitializers.GetSize())
		, m_entries(Memory::Reserve, entryInitializers.GetSize())
	{
		Assign(entryInitializers, skeletonComponent);
	}

	bool Blendspace1D::Entry::Serialize(const Serialization::Reader reader)
	{
		if (const Optional<SampleableAnimation*> pAnimation = Get<SampleableAnimation>())
		{
			return reader.SerializeInPlace(*pAnimation);
		}
		else
		{
			return true;
		}
	}
	bool Blendspace1D::Entry::Serialize(Serialization::Writer writer) const
	{
		if (const Optional<const SampleableAnimation*> pAnimation = Get<SampleableAnimation>())
		{
			return writer.SerializeInPlace(*pAnimation);
		}
		else
		{
			return true;
		}
	}

	Time::Durationf Blendspace1D::Entry::GetCurrentLoopDuration() const
	{
		if (const Optional<const SampleableAnimation*> pAnimation = Get<SampleableAnimation>())
		{
			return pAnimation->m_duration;
		}
		else if (const Optional<const ReferenceWrapper<Blendspace1D>*> pBlendspace = Get<ReferenceWrapper<Blendspace1D>>())
		{
			Blendspace1D& blendspace = *pBlendspace;
			return blendspace.GetCurrentLoopDuration();
		}
		else
		{
			ExpectUnreachable();
		}
	}

	void Blendspace1D::Assign(const ArrayView<const EntryInitializer, uint8> entryInitializers, SkeletonComponent& skeletonComponent)
	{
		AllocateAnimations(skeletonComponent.GetSkeletonInstance().GetSampledTransforms().GetSize(), entryInitializers.GetSize());

		for (Entry& entry : m_entries)
		{
			if (const Optional<SampleableAnimation*> pAnimation = entry.Get<SampleableAnimation>())
			{
				pAnimation->Unload();
			}
		}

		uint8 nextIndex = 0;
		for (const EntryInitializer& entryInitializer : entryInitializers)
		{
			if (const Optional<const ReferenceWrapper<Blendspace1D>*> pBlendspace = entryInitializer.Get<ReferenceWrapper<Blendspace1D>>())
			{
				Blendspace1D& blendspace = *pBlendspace;
				blendspace.m_pParent = this;
				blendspace.m_parentEntryIndex = 0;
				m_entries[nextIndex] = ReferenceWrapper<Blendspace1D>(blendspace);
			}
			else if (const Optional<const Serialization::Reader*> animationReader = entryInitializer.Get<Serialization::Reader>())
			{
				m_entries[nextIndex] = SampleableAnimation{};
				animationReader->SerializeInPlace(m_entries[nextIndex].GetExpected<SampleableAnimation>());
			}
			else if (const Optional<const ReferenceWrapper<const SampleableAnimation>*> pAnimation = entryInitializer.Get<ReferenceWrapper<const SampleableAnimation>>())
			{
				const SampleableAnimation& animation = **pAnimation;
				m_entries[nextIndex] = SampleableAnimation{animation};
			}
			else
			{
				ExpectUnreachable();
			}
			nextIndex++;
		}

		if (const Optional<const Skeleton*> pSkeleton = skeletonComponent.GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged(pSkeleton->GetJointCount(), skeletonComponent.GetSkeletonInstance().GetSampledTransforms().GetSize());
		}
	}

	bool Blendspace1D::Serialize(const Serialization::Reader animationsReader, SkeletonComponent& skeletonComponent)
	{
		AllocateAnimations(skeletonComponent.GetSkeletonInstance().GetSampledTransforms().GetSize(), (uint8)animationsReader.GetArraySize());

		for (Entry& entry : m_entries)
		{
			if (const Optional<SampleableAnimation*> pAnimation = entry.Get<SampleableAnimation>())
			{
				pAnimation->Unload();
			}
		}

		bool readEntries = true;
		uint8 animationIndex = 0;
		for (const Serialization::Reader animationReader : animationsReader.GetArrayView())
		{
			if (!m_entries[animationIndex].Is<SampleableAnimation>())
			{
				m_entries[animationIndex] = SampleableAnimation{};
			}

			if (!animationReader.SerializeInPlace(m_entries[animationIndex].GetExpected<SampleableAnimation>()))
			{
				readEntries = false;
			}
			animationIndex++;
		}

		if (const Optional<const Skeleton*> pSkeleton = skeletonComponent.GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged(pSkeleton->GetJointCount(), skeletonComponent.GetSkeletonInstance().GetSampledTransforms().GetSize());
		}

		return readEntries;
	}

	Blendspace1D::~Blendspace1D()
	{
		for (Entry& entry : m_entries)
		{
			if (const Optional<SampleableAnimation*> pAnimation = entry.Get<SampleableAnimation>())
			{
				pAnimation->Unload();
			}
		}
	}

	bool Blendspace1D::Serialize(Serialization::Writer writer) const
	{
		return writer.SerializeInPlace(m_entries);
	}

	void Blendspace1D::AllocateAnimations(const uint16 sampledTransformCount, const uint8 animationCount)
	{
		Blendspace1DBase::Initialize(sampledTransformCount, animationCount);
		m_entries.Resize(animationCount);
	}

	void Blendspace1D::LoadAnimations(SkeletonComponent& skeletonComponent)
	{
		for (Entry& entry : m_entries)
		{
			if (const Optional<SampleableAnimation*> pAnimation = entry.Get<SampleableAnimation>())
			{
				pAnimation->Load(
					[this, &skeletonComponent]()
					{
						if (AreAllAnimationsLoaded())
						{
							OnAnimationsLoaded(skeletonComponent);
						}
					}
				);
			}
			else if (const Optional<ReferenceWrapper<Blendspace1D>*> pBlendspace = entry.Get<ReferenceWrapper<Blendspace1D>>())
			{
				Blendspace1D& blendspace = *pBlendspace;
				blendspace.LoadAnimations(skeletonComponent);
			}
		}
	}

	void Blendspace1D::Advance(
		const Time::Durationf frameTime,
		const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses,
		const uint16 sampledTransformCount
	)
	{
		for (uint8 i = 0, n = m_entries.GetSize(); i < n; ++i)
		{
			Entry& entry = m_entries[i];
			if (const Optional<SampleableAnimation*> pAnimation = entry.Get<SampleableAnimation>())
			{
				pAnimation->Advance(frameTime);
			}
			else if (const Optional<ReferenceWrapper<Blendspace1D>*> pBlendspace = entry.Get<ReferenceWrapper<Blendspace1D>>())
			{
				Blendspace1D& blendspace = *pBlendspace;
				blendspace.Advance(frameTime, jointBindPoses, sampledTransformCount);
			}
		}

		Sample(jointBindPoses, sampledTransformCount);

		if (m_pParent.IsValid())
		{
			m_pParent->OnChildTransformsChanged(m_parentEntryIndex, jointBindPoses, sampledTransformCount);
		}
	}

	void Blendspace1D::SetTimeRatio(
		const Math::Ratiof timeRatio, const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses, const uint16 sampledTransformCount
	)
	{
		for (uint8 i = 0, n = m_entries.GetSize(); i < n; ++i)
		{
			Entry& entry = m_entries[i];
			if (const Optional<SampleableAnimation*> pAnimation = entry.Get<SampleableAnimation>())
			{
				pAnimation->SetTimeRatio(timeRatio);
			}
			else if (const Optional<ReferenceWrapper<Blendspace1D>*> pBlendspace = entry.Get<ReferenceWrapper<Blendspace1D>>())
			{
				Blendspace1D& blendspace = *pBlendspace;
				blendspace.SetTimeRatio(timeRatio, jointBindPoses, sampledTransformCount);
			}
		}

		if (AreAllAnimationsLoaded())
		{
			Sample(jointBindPoses, sampledTransformCount);
		}

		if (m_pParent.IsValid())
		{
			m_pParent->OnChildTransformsChanged(m_parentEntryIndex, jointBindPoses, sampledTransformCount);
		}
	}

	void Blendspace1D::OnSkeletonChanged(const uint16 jointCount, const uint16 sampledTransformCount)
	{
		Blendspace1DBase::Initialize(sampledTransformCount, m_entries.GetSize());
		for (Entry& entry : m_entries)
		{
			if (const Optional<SampleableAnimation*> pAnimation = entry.Get<SampleableAnimation>())
			{
				pAnimation->OnSkeletonChanged(jointCount);
			}
			else if (const Optional<ReferenceWrapper<Blendspace1D>*> pBlendspace = entry.Get<ReferenceWrapper<Blendspace1D>>())
			{
				Blendspace1D& blendspace = *pBlendspace;
				blendspace.OnSkeletonChanged(jointCount, sampledTransformCount);
			}
		}
	}

	uint8 Blendspace1D::GetCurrentSamplerIndex() const
	{
		const uint8 layerCount = m_layers.GetSize();
		// Select the 2 samplers that define interval that contains m_blendRatio.
		// Uses a maximum value smaller that 1.f (-epsilon) to ensure that (relevant_sampler + 1) is always valid.
		const uint8 relevantSampler = static_cast<uint8>((m_blendRatio - 1e-3f) * float(layerCount - 1));
		return relevantSampler;
	}

	Time::Durationf Blendspace1D::GetCurrentLoopDuration() const
	{
		const uint8 relevantSampler = GetCurrentSamplerIndex();
		if (LIKELY(relevantSampler < m_entries.GetSize()))
		{
			const Entry& relevantEntry = m_entries[relevantSampler];
			const uint8 nextSampler = relevantSampler + 1;
			if (nextSampler < m_entries.GetSize())
			{
				const Entry& nextEntry = m_entries[nextSampler];

				// Interpolates animation durations using their respective weights, to
				// find the loop cycle duration that matches blend_ratio_.
				return relevantEntry.GetCurrentLoopDuration() * m_layers[relevantSampler].weight +
				       nextEntry.GetCurrentLoopDuration() * m_layers[nextSampler].weight;
			}
			else
			{
				return relevantEntry.GetCurrentLoopDuration();
			}
		}
		else
		{
			return 0_seconds;
		}
	}

	void Blendspace1D::OnBlendRatioChanged()
	{
		Blendspace1DBase::OnBlendRatioChanged();

		// Synchronize animations by calculating playback speed
		const Time::Durationf loopDuration = GetCurrentLoopDuration();
		const uint8 layerCount = m_layers.GetSize();
		Assert(m_entries.GetSize() == layerCount);
		const float inverseLoopDuration = Math::MultiplicativeInverse(loopDuration.GetSeconds());
		for (uint8 i = 0; i < layerCount; ++i)
		{
			if (const Optional<SampleableAnimation*> pAnimation = m_entries[i].Get<SampleableAnimation>())
			{
				pAnimation->m_playbackSpeed = pAnimation->m_duration.GetSeconds() * inverseLoopDuration;
			}
		}
	}

	void Blendspace1D::Sample(const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses, const uint16 sampledTransformCount)
	{

		const uint8 relevantSampler = GetCurrentSamplerIndex();
		const uint8 nextSampler = relevantSampler + 1;

		if (LIKELY(relevantSampler < m_entries.GetSize()))
		{
			const Entry& relevantEntry = m_entries[relevantSampler];
			const ArrayView<ozz::math::SoaTransform, uint32> sampledTransforms = m_sampledTransforms.GetView() +
			                                                                     sampledTransformCount * relevantSampler;
			if (const Optional<const SampleableAnimation*> pAnimation = relevantEntry.Get<SampleableAnimation>())
			{
				pAnimation->SampleAtCurrentTimeRatio(sampledTransforms);
			}
			else if (const Optional<const ReferenceWrapper<Blendspace1D>*> pBlendspace = relevantEntry.Get<ReferenceWrapper<Blendspace1D>>())
			{
				Blendspace1D& blendspace = *pBlendspace;
				blendspace.Process(jointBindPoses, sampledTransforms);
			}
		}

		if (nextSampler < m_entries.GetSize())
		{
			const Entry& nextEntry = m_entries[nextSampler];
			const ArrayView<ozz::math::SoaTransform, uint32> sampledTransforms = m_sampledTransforms.GetView() +
			                                                                     sampledTransformCount * nextSampler;
			if (const Optional<const SampleableAnimation*> pAnimation = nextEntry.Get<SampleableAnimation>())
			{
				pAnimation->SampleAtCurrentTimeRatio(sampledTransforms);
			}
			else if (const Optional<const ReferenceWrapper<Blendspace1D>*> pBlendspace = nextEntry.Get<ReferenceWrapper<Blendspace1D>>())
			{
				Blendspace1D& blendspace = *pBlendspace;
				blendspace.Process(jointBindPoses, sampledTransforms);
			}
		}
	}

	void Blendspace1D::OnChildTransformsChanged(
		const uint8 childEntryIndex, const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses, const uint16 sampledTransformCount
	)
	{
		const uint8 relevantSampler = GetCurrentSamplerIndex();
		if (childEntryIndex == relevantSampler || childEntryIndex == relevantSampler + 1)
		{
			if (AreAllAnimationsLoaded())
			{
				Sample(jointBindPoses, sampledTransformCount);
			}
		}
	}

	void Blendspace1D::OnAnimationsLoaded(SkeletonComponent& skeletonComponent)
	{
		OnSkeletonChanged(
			skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointCount(),
			skeletonComponent.GetSkeletonInstance().GetSampledTransforms().GetSize()
		);

		skeletonComponent.TryEnableUpdate();
	}

	void SampleableAnimation::Load(LoadedCallback&& callback)
	{
		Asset::Manager& assetManager = System::Get<Asset::Manager>();

		AnimationCache& animationCache = Plugin::GetInstance()->GetAnimationCache();
		Threading::Job* pLoadAnimationJob = animationCache.TryLoadAnimation(
			m_identifier,
			assetManager,
			AnimationCache::AnimationLoadListenerData{
				*this,
				[this, callback = Forward<LoadedCallback>(callback)](const SampleableAnimation&, const AnimationIdentifier animationIdentifier)
				{
					AnimationCache& animationCache = Plugin::GetInstance()->GetAnimationCache();
					OnLoaded(animationCache.GetAnimation(animationIdentifier));
					callback();
				}
			}
		);
		if (pLoadAnimationJob != nullptr)
		{
			pLoadAnimationJob->Queue(System::Get<Threading::JobManager>());
		}
	}

	void SampleableAnimation::OnLoaded(const Optional<Animation*> pAnimation)
	{
		m_pAnimation = pAnimation;

		const Time::Durationf duration = pAnimation->GetDuration();
		m_duration = duration;
		m_inverseDuration = Math::MultiplicativeInverse((double)duration.GetSeconds());
	}

	void SampleableAnimation::Unload()
	{
		if (m_identifier.IsValid())
		{
			AnimationCache& animationCache = Plugin::GetInstance()->GetAnimationCache();
			[[maybe_unused]] const bool wasDeregistered = animationCache.RemoveAnimationListener(m_identifier, this);
		}
	}

	void SampleableAnimation::OnSkeletonChanged(const uint16 jointCount)
	{
		m_samplingCache.Resize(jointCount);
	}

	bool SampleableAnimation::Serialize(const Serialization::Reader reader)
	{
		AnimationCache& animationCache = Plugin::GetInstance()->GetAnimationCache();
		const Guid animationAssetGuid = *reader.ReadInPlace<Guid>();
		m_identifier = animationCache.FindOrRegisterAsset(animationAssetGuid);
		return true;
	}

	bool SampleableAnimation::Serialize(Serialization::Writer writer) const
	{
		AnimationCache& animationCache = Plugin::GetInstance()->GetAnimationCache();
		return writer.SerializeInPlace(animationCache.GetAssetGuid(m_identifier));
	}

	void
	SampleableAnimation::SampleAtTimeRatio(const Math::Ratiof timeRatio, const ArrayView<ozz::math::SoaTransform, uint16> outTransforms) const
	{
		m_pAnimation->SampleAnimationAtTimeRatio(timeRatio, m_samplingCache, outTransforms);
	}

	void SampleableAnimation::SampleAtCurrentTimeRatio(const ArrayView<ozz::math::SoaTransform, uint16> outTransforms) const
	{
		SampleAtTimeRatio(m_timeRatio, outTransforms);
	}

	void SampleableAnimation::Advance(const Time::Durationf frameTime)
	{
		float timeRatio = m_timeRatio;
		timeRatio += float(frameTime.GetSeconds() * m_inverseDuration);
		timeRatio -= Math::Floor(timeRatio);
		m_timeRatio = timeRatio;
	}

	void SampleableAnimation::SetTimeRatio(const Math::Ratiof timeRatio)
	{
		m_timeRatio = timeRatio;
	}
}
