#pragma once

#include "SamplingCache.h"
#include "Animation.h"
#include "AnimationIdentifier.h"

#include <Animation/3rdparty/ozz/animation/runtime/blending_job.h>
#include <Animation/3rdparty/ozz/base/maths/soa_transform.h>

#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Optional.h>
#include <Common/Memory/Variant.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Storage/Identifier.h>
#include <Common/Function/ForwardDeclarations/Function.h>
#include <Common/Serialization/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

#include <Common/Memory/Containers/ArrayView.h>
#include <Common/Math/MultiplicativeInverse.h>
#include <Common/Time/Duration.h>

namespace ngine
{
	struct FrameTime;

	namespace Asset
	{
		struct Manager;
	}

	namespace Serialization
	{
		struct Reader;
		struct Writer;
	}
}

namespace ngine::Animation
{
	struct SkeletonInstance;
	struct SkeletonComponent;
	struct AnimationCache;

	namespace Internal
	{
		struct Blendspace1DBase
		{
			Blendspace1DBase() = default;
			Blendspace1DBase(const uint16 sampledTransformCount, const uint8 blendedLayerCount);
			explicit Blendspace1DBase(const Blendspace1DBase&) = default;
			Blendspace1DBase& operator=(const Blendspace1DBase&) = delete;
			Blendspace1DBase(Blendspace1DBase&&) = default;
			Blendspace1DBase& operator=(Blendspace1DBase&&) = default;

			[[nodiscard]] float GetBlendRatio() const
			{
				return m_blendRatio;
			}

			void SetBlendRatio(const float ratio)
			{
				m_blendRatio = ratio;
				OnBlendRatioChanged();
			}

			[[nodiscard]] uint16 GetRequiredTransformCount(const uint16 sampledTransformCount) const
			{
				return sampledTransformCount * m_layers.GetSize();
			}

			[[nodiscard]] ArrayView<ozz::math::SoaTransform, uint32> GetTransforms()
			{
				// TODO: Move sample transforms out and make it the responsibility of the caller
				// Mostly to save on using multiple copies while we in many cases could edit in place
				return m_sampledTransforms;
			}

			void Process(
				const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses,
				const ArrayView<ozz::math::SoaTransform, uint16> outTransforms
			);
			void Initialize(const uint16 sampledTransformCount, const uint8 blendedLayerCount);
		protected:
			void OnBlendRatioChanged();
		protected:
			float m_blendRatio = 0.f;
			Vector<ozz::animation::BlendingJob::Layer, uint8> m_layers;
			Vector<ozz::math::SoaTransform, uint32> m_sampledTransforms;
		};
	}

	struct SampleableAnimation
	{
		using LoadedCallback = Function<void(), 24>;
		void Load(LoadedCallback&& callback);
		void OnLoaded(const Optional<Animation*> pAnimation);
		void Unload();
		void OnSkeletonChanged(const uint16 jointCount);

		bool Serialize(const Serialization::Reader reader);
		bool Serialize(Serialization::Writer writer) const;

		void Advance(const Time::Durationf frameTime);
		void SetTimeRatio(const Math::Ratiof timeRatio);

		void SampleAtTimeRatio(const Math::Ratiof timeRatio, const ArrayView<ozz::math::SoaTransform, uint16> outTransforms) const;
		void SampleAtCurrentTimeRatio(const ArrayView<ozz::math::SoaTransform, uint16> outTransforms) const;

		Optional<Animation*> m_pAnimation;
		AnimationIdentifier m_identifier;
		Time::Durationf m_duration;
		double m_inverseDuration;
		float m_playbackSpeed;
		Math::Ratiof m_timeRatio;
		mutable SamplingCache m_samplingCache;
	};

	struct Blendspace1D : public Internal::Blendspace1DBase
	{
		Blendspace1D() = default;
		Blendspace1D(const Serialization::Reader animationsReader, SkeletonComponent& skeletonComponent);
		Blendspace1D(const Optional<Serialization::Reader> animationsReader, SkeletonComponent& skeletonComponent);
		struct EntryInitializer
			: public Variant<ReferenceWrapper<Blendspace1D>, Serialization::Reader, ReferenceWrapper<const SampleableAnimation>>
		{
			using BaseType = Variant<ReferenceWrapper<Blendspace1D>, Serialization::Reader, ReferenceWrapper<const SampleableAnimation>>;
			using BaseType::BaseType;
			EntryInitializer(Blendspace1D& blendspace)
				: BaseType(ReferenceWrapper<Blendspace1D>{blendspace})
			{
			}
			EntryInitializer(const SampleableAnimation& animation)
				: BaseType(ReferenceWrapper<const SampleableAnimation>{animation})
			{
			}
		};
		Blendspace1D(const ArrayView<const EntryInitializer, uint8> entryInitializers, SkeletonComponent& skeletonComponent);

		explicit Blendspace1D(const Blendspace1D& other) = default;
		Blendspace1D& operator=(const Blendspace1D&) = delete;
		Blendspace1D(Blendspace1D&&) = delete;
		Blendspace1D& operator=(Blendspace1D&&) = delete;
		~Blendspace1D();

		void Assign(const ArrayView<const EntryInitializer, uint8> entryInitializers, SkeletonComponent& skeletonComponent);

		bool Serialize(const Serialization::Reader animationsReader, SkeletonComponent& skeletonComponent);
		bool Serialize(Serialization::Writer writer) const;

		void AllocateAnimations(const uint16 sampledTransformCount, const uint8 animationCount);
		void LoadAnimations(SkeletonComponent& skeletonComponent);

		[[nodiscard]] bool HasAnimation(const uint8 index) const
		{
			if (index < m_entries.GetSize())
			{
				if (const Optional<const SampleableAnimation*> pAnimation = m_entries[index].Get<SampleableAnimation>())
				{
					return pAnimation->m_pAnimation.IsValid();
				}
				else
				{
					return false;
				}
			}
			else
			{
				return false;
			}
		}

		[[nodiscard]] uint8 GetEntryCount() const
		{
			return m_entries.GetSize();
		}

		[[nodiscard]] Optional<Animation*> GetAnimation(const uint8 index) const
		{
			if (index < m_entries.GetSize())
			{
				if (const Optional<const SampleableAnimation*> pAnimation = m_entries[index].Get<SampleableAnimation>())
				{
					return pAnimation->m_pAnimation.Get();
				}
				else
				{
					return Invalid;
				}
			}
			else
			{
				return Invalid;
			}
		}

		[[nodiscard]] Optional<const SampleableAnimation*> GetSampleableAnimation(const uint8 index) const
		{
			if (index < m_entries.GetSize())
			{
				return m_entries[index].Get<SampleableAnimation>();
			}
			else
			{
				return Invalid;
			}
		}
		[[nodiscard]] Optional<SampleableAnimation*> GetSampleableAnimation(const uint8 index)
		{
			if (index < m_entries.GetSize())
			{
				return m_entries[index].Get<SampleableAnimation>();
			}
			else
			{
				return Invalid;
			}
		}

		[[nodiscard]] bool IsValid() const
		{
			return AreAllAnimationsLoaded();
		}

		void Advance(
			const Time::Durationf frameTime,
			const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses,
			const uint16 sampledTransformCount
		);
		void SetTimeRatio(
			const Math::Ratiof timeRatio,
			const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses,
			const uint16 sampledTransformCount
		);

		void OnSkeletonChanged(const uint16 jointCount, const uint16 sampledTransformCount);

		void SetBlendRatio(const Math::Ratiof ratio)
		{
			m_blendRatio = ratio;
			OnBlendRatioChanged();
		}
		[[nodiscard]] float GetBlendRatio() const
		{
			return m_blendRatio;
		}

		[[nodiscard]] Time::Durationf GetCurrentLoopDuration() const;
	protected:
		[[nodiscard]] uint8 GetCurrentSamplerIndex() const;
		void OnBlendRatioChanged();
		void Sample(const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses, const uint16 sampledTransformCount);
		void OnChildTransformsChanged(
			const uint8 childEntryIndex, const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses, const uint16 sampledTransformCount
		);

		void OnAnimationsLoaded(SkeletonComponent& skeletonComponent);

		[[nodiscard]] bool AreAllAnimationsLoaded() const
		{
			return m_entries.GetView().All(
				[](const Entry& entry)
				{
					if (const Optional<const SampleableAnimation*> pAnimation = entry.Get<SampleableAnimation>())
					{
						return pAnimation->m_pAnimation.IsValid() && pAnimation->m_pAnimation->IsValid();
					}
					else if (const Optional<const ReferenceWrapper<Blendspace1D>*> pBlendspace = entry.Get<ReferenceWrapper<Blendspace1D>>())
					{
						const Blendspace1D& blendspace = *pBlendspace;
						return blendspace.AreAllAnimationsLoaded();
					}
					else
					{
						return true;
					}
				}
			);
		}
	protected:
		struct Entry : public Variant<SampleableAnimation, ReferenceWrapper<Blendspace1D>>
		{
			using BaseType = Variant<SampleableAnimation, ReferenceWrapper<Blendspace1D>>;
			using BaseType::BaseType;

			Entry(const Entry& other) = default;
			Entry& operator=(const Entry&) = delete;
			Entry(Entry&&) = default;
			Entry& operator=(Entry&&) = default;

			bool Serialize(const Serialization::Reader reader);
			bool Serialize(Serialization::Writer writer) const;

			[[nodiscard]] Time::Durationf GetCurrentLoopDuration() const;
		};

		Vector<Entry, uint8> m_entries;
		inline static constexpr uint8 InvalidEntryIndex = Math::NumericLimits<uint8>::Max;
		Optional<Blendspace1D*> m_pParent;
		uint8 m_parentEntryIndex{InvalidEntryIndex};
	};
};
