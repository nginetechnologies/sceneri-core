#include "Components/Controllers/LoopingAnimationController.h"
#include "Components/SkeletonComponent.h"

#include "Plugin.h"
#include "AnimationCache.h"
#include "Animation.h"
#include "AnimationAssetType.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Threading/JobManager.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Math/Mod.h>
#include <Common/Memory/UniquePtr.h>

namespace ngine::Animation
{
	[[nodiscard]] uint16 GetJointCount(const SkeletonInstance& skeletonInstance)
	{
		return skeletonInstance.GetSkeleton().IsValid() ? skeletonInstance.GetSkeleton()->GetJointCount() : 0;
	}

	LoopingAnimationController::LoopingAnimationController(const LoopingAnimationController& templateComponent, const Cloner& cloner)
		: Controller(templateComponent, cloner)
		, m_animationIdentifier(templateComponent.m_animationIdentifier)
		, m_pAnimation(templateComponent.m_pAnimation)
		, m_timeRatio(templateComponent.m_timeRatio)
		, m_samplingCache(templateComponent.m_samplingCache)
	{
		if (m_animationIdentifier.IsValid())
		{
			AnimationCache& animationCache = Plugin::GetInstance()->GetAnimationCache();
			Threading::Job* pLoadAnimationJob = animationCache.TryLoadAnimation(
				m_animationIdentifier,
				System::Get<Asset::Manager>(),
				AnimationCache::AnimationLoadListenerData{
					*this,
					[this](const LoopingAnimationController&, const AnimationIdentifier)
					{
						AnimationCache& animationCache = Plugin::GetInstance()->GetAnimationCache();
						m_pAnimation = animationCache.GetAnimation(m_animationIdentifier);
						Assert(m_pAnimation != nullptr);

						Assert(ShouldUpdate());
						m_skeletonComponent.TryEnableUpdate();
					}
				}
			);
			if (pLoadAnimationJob != nullptr)
			{
				pLoadAnimationJob->Queue(System::Get<Threading::JobManager>());
			}
		}

		if (const Optional<const Skeleton*> pSkeleton = m_skeletonComponent.GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged();
		}
	}

	LoopingAnimationController::LoopingAnimationController(Initializer&& initializer)
		: Controller(Forward<Initializer>(initializer))
		, m_samplingCache(GetJointCount(initializer.GetParent().AsExpected<SkeletonComponent>().GetSkeletonInstance()))
	{
		if (initializer.m_animationIdentifier.IsValid())
		{
			SetAnimation(initializer.m_animationIdentifier);
		}

		if (const Optional<const Skeleton*> pSkeleton = m_skeletonComponent.GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged();
		}
	}

	LoopingAnimationController::LoopingAnimationController(const Deserializer& __restrict deserializer)
		: Controller(deserializer)
		, m_samplingCache(GetJointCount(deserializer.GetParent().AsExpected<SkeletonComponent>().GetSkeletonInstance()))
	{
		const Guid animationGuid = deserializer.m_reader.ReadWithDefaultValue<Guid>("animation", Guid{});
		if (animationGuid.IsValid())
		{
			SetAnimationAsset({animationGuid, AnimationAssetType::AssetFormat.assetTypeGuid});
		}

		if (const Optional<const Skeleton*> pSkeleton = m_skeletonComponent.GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged();
		}
	}

	LoopingAnimationController::~LoopingAnimationController()
	{
		if (m_animationIdentifier.IsValid())
		{
			AnimationCache& animationCache = Plugin::GetInstance()->GetAnimationCache();
			[[maybe_unused]] const bool wasDeregistered = animationCache.RemoveAnimationListener(m_animationIdentifier, this);
			Assert(wasDeregistered);
		}
	}

	void LoopingAnimationController::SetAnimationAsset(const AnimationAssetPicker asset)
	{
		AnimationCache& animationCache = Plugin::GetInstance()->GetAnimationCache();
		const AnimationIdentifier identifier = animationCache.FindOrRegisterAsset(asset.GetAssetGuid());
		if (identifier != m_animationIdentifier)
		{
			SetAnimation(identifier);
		}
	}

	void LoopingAnimationController::SetAnimation(const AnimationIdentifier identifier)
	{
		Assert(identifier != m_animationIdentifier);
		m_animationIdentifier = identifier;

		if (identifier.IsValid())
		{
			AnimationCache& animationCache = Plugin::GetInstance()->GetAnimationCache();
			Threading::Job* pLoadAnimationJob = animationCache.TryLoadAnimation(
				m_animationIdentifier,
				System::Get<Asset::Manager>(),
				AnimationCache::AnimationLoadListenerData{
					*this,
					[this](const LoopingAnimationController&, const AnimationIdentifier)
					{
						AnimationCache& animationCache = Plugin::GetInstance()->GetAnimationCache();
						m_pAnimation = animationCache.GetAnimation(m_animationIdentifier);
						Assert(m_pAnimation != nullptr);

						m_skeletonComponent.TryEnableUpdate();
					}
				}
			);
			if (pLoadAnimationJob != nullptr)
			{
				pLoadAnimationJob->Queue(System::Get<Threading::JobManager>());
			}
			else
			{
				m_skeletonComponent.TryEnableUpdate();
			}
		}
	}

	LoopingAnimationController::AnimationAssetPicker LoopingAnimationController::GetAnimation() const
	{
		return {
			m_animationIdentifier.IsValid() ? Plugin::GetInstance()->GetAnimationCache().GetAssetGuid(m_animationIdentifier) : Asset::Guid{},
			AnimationAssetType::AssetFormat.assetTypeGuid
		};
	}

	bool LoopingAnimationController::ShouldUpdate() const
	{
		SkeletonInstance& skeletonInstance = m_skeletonComponent.GetSkeletonInstance();
		return m_pAnimation != nullptr && m_pAnimation->IsValid() &&
		       m_pAnimation->CanSampleAnimationAtTimeRatio(
						 m_timeRatio,
						 const_cast<SamplingCache&>(m_samplingCache),
						 skeletonInstance.GetSampledTransforms()
					 ) &&
		       skeletonInstance.IsValid();
	}

	void LoopingAnimationController::Update()
	{
		Assert(ShouldUpdate());
		m_timeRatio += m_skeletonComponent.GetCurrentFrameTime() / m_pAnimation->GetDuration().GetSeconds();
		m_timeRatio = Math::Mod(m_timeRatio, 1.f);

		SkeletonInstance& skeletonInstance = m_skeletonComponent.GetSkeletonInstance();
		// Samples transforms at time in the animation
		m_pAnimation->SampleAnimationAtTimeRatio(m_timeRatio, m_samplingCache, skeletonInstance.GetSampledTransforms());
	}

	void LoopingAnimationController::OnSkeletonChanged()
	{
		m_samplingCache.Resize(GetJointCount(m_skeletonComponent.GetSkeletonInstance()));
	}

	void LoopingAnimationController::ApplyAnimation(const Asset::Guid assetGuid)
	{
		SetAnimationAsset({assetGuid, AnimationAssetType::AssetFormat.assetTypeGuid});
	}

	void LoopingAnimationController::IterateAnimations(const Function<Memory::CallbackResult(ConstAnyView), 36>& callback)
	{
		const Asset::Reference animation = Asset::Reference{GetAnimation().GetAssetGuid(), AnimationAssetType::AssetFormat.assetTypeGuid};
		callback(animation);
	}

	[[maybe_unused]] const bool wasControllerTypeRegistered = Reflection::Registry::RegisterType<Controller>();
	[[maybe_unused]] const bool wasLoopingControllerRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<LoopingAnimationController>>::Make());
	[[maybe_unused]] const bool wasLoopingControllerTypeRegistered = Reflection::Registry::RegisterType<LoopingAnimationController>();
}
