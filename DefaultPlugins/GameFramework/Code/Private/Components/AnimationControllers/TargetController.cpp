#include "Components/AnimationControllers/TargetController.h"
#include "Components/Items/ProjectileTarget.h"

#include <Common/Reflection/Registry.inl>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Asset/AssetManager.h>

#include <Animation/Plugin.h>
#include <Animation/Components/SkeletonComponent.h>

#include <GameFramework/Reset/ResetComponent.h>

#include <Engine/Entity/Scene/SceneComponent.h>
#include "Components/Controllers/SplineMovementComponent.h"

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>

#include <AudioCore/Components/SoundSpotComponent.h>
#include <AudioCore/AudioAsset.h>

namespace ngine::GameFramework::Animation::Controller
{
	Target::Target(Initializer&& initializer)
		: Controller(Forward<Initializer>(initializer))
		, m_owner(initializer.GetParent())
	{
	}

	Target::Target(const Target& templateComponent, const Cloner& cloner)
		: Controller(templateComponent, cloner)
		, m_upBlendspace(templateComponent.m_upBlendspace)
		, m_downBlendspace(templateComponent.m_downBlendspace)
		, m_finalBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 2>{m_upBlendspace, m_downBlendspace}, m_skeletonComponent
			)
		, m_owner(cloner.GetParent())
		, m_state(templateComponent.m_state)
		, m_resetTime(templateComponent.m_resetTime)
	{
		m_upBlendspace.LoadAnimations(m_skeletonComponent);
		m_downBlendspace.LoadAnimations(m_skeletonComponent);

		if (templateComponent.GetUpAudioAsset().IsValid())
		{
			SetUpAudioAsset(templateComponent.GetUpAudioAsset());
		}
		if (templateComponent.GetDownAudioAsset().IsValid())
		{
			SetDownAudioAsset(templateComponent.GetDownAudioAsset());
		}

		if (const Optional<const ngine::Animation::Skeleton*> pSkeleton = m_skeletonComponent.GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged();
			m_skeletonComponent.TryEnableUpdate();
		}
	}

	Target::Target(const Deserializer& deserializer)
		: Controller(deserializer)
		, m_upBlendspace(deserializer.m_reader.FindSerializer("up_animations"), m_skeletonComponent)
		, m_downBlendspace(deserializer.m_reader.FindSerializer("down_animations"), m_skeletonComponent)
		, m_finalBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 2>{m_upBlendspace, m_downBlendspace}, m_skeletonComponent
			)
		, m_owner(deserializer.GetParent())
	{
		if (const Optional<const ngine::Animation::Skeleton*> pSkeleton = m_skeletonComponent.GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged();
			m_skeletonComponent.TryEnableUpdate();
		}
	}

	void Target::SpawnOrSetSoundSpotAsset(Optional<Audio::SoundSpotComponent*>& pComponent, const Asset::Guid asset)
	{
		if (asset.IsValid())
		{
			if (!pComponent)
			{
				Entity::ComponentTypeSceneData<Audio::SoundSpotComponent>& soundSpotSceneData =
					*m_owner.GetSceneRegistry().GetOrCreateComponentTypeData<Audio::SoundSpotComponent>();

				pComponent =
					soundSpotSceneData.CreateInstance(Audio::SoundSpotComponent::Initializer(Entity::Component3D::Initializer{m_owner}, {}, asset));
			}
			else
			{
				pComponent->SetAudioAsset({asset, Audio::AssetFormat.assetTypeGuid});
			}
		}
	}

	Asset::Picker Target::GetAssetFromSoundSpot(Optional<Audio::SoundSpotComponent*> pComponent) const
	{
		if (pComponent)
		{
			return pComponent->GetAudioAsset();
		}

		return {Guid(), Audio::AssetFormat.assetTypeGuid};
	}

	void Target::UpdateAnimation(const float deltaTime)
	{
		if (Math::SignNonZero(m_animationTime) > 0.f)
		{
			State targetState = State::None;
			const float blendToOneshotTime = 0.1f;
			m_animationTime += deltaTime;

			float blendToOneshotRatio = 0.f;
			Optional<ngine::Animation::Animation*> pOneShotAnimation;
			if (m_state == State::GoingDown)
			{
				targetState = State::Down;
				pOneShotAnimation = m_downBlendspace.GetAnimation(0);
				blendToOneshotRatio = Math::Min(m_animationTime / blendToOneshotTime, 1.f);
			}
			else if (m_state == State::GoingUp)
			{
				targetState = State::Up;
				pOneShotAnimation = m_upBlendspace.GetAnimation(0);
				blendToOneshotRatio = 1 - Math::Min(m_animationTime / blendToOneshotTime, 1.f);
			}
			m_finalBlendspace.SetBlendRatio(blendToOneshotRatio);
			const Time::Durationf oneshotDuration = pOneShotAnimation.IsValid() ? pOneShotAnimation->GetDuration() : Time::Durationf{0_seconds};
			if (m_animationTime >= oneshotDuration.GetSeconds() - 0.05)
			{
				m_state = targetState;

				if (m_state == State::Down && m_resetTime > 0.0f)
				{
					System::Get<Threading::JobManager>().ScheduleAsync(
						Time::Durationf::FromSeconds(m_resetTime),
						[this](Threading::JobRunnerThread&)
						{
							StartReerect();
						},
						Threading::JobPriority::LoadAudio
					);
				}

				m_skeletonComponent.Disable();
			}
		}
	}

	void Target::SubscribeToProjectileTarget(Entity::Component3D& owner)
	{
		if (Optional<ProjectileTarget*> pTarget = owner.FindFirstDataComponentOfTypeInParents<ProjectileTarget>())
		{
			pTarget->OnHit.Add(
				*this,
				[](Target& target, ProjectileTarget::HitSettings)
				{
					target.StartHit();
				}
			);
		}
	}

	void Target::StartHit()
	{
		if (m_state == State::Up)
		{
			m_skeletonComponent.Enable();

			if (m_pDownSoundComponent)
			{
				m_pDownSoundComponent->Play();
			}

			m_animationTime = 0.f;

			const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses =
				m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointBindPoses();
			m_downBlendspace
				.SetTimeRatio(0.f, jointBindPoses, m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetStructureOfArraysJointCount());

			m_downBlendspace.SetBlendRatio(0.0f);
			m_finalBlendspace.SetBlendRatio(1.0f);
			m_state = State::GoingDown;
		}
	}

	void Target::StartReerect()
	{
		if (m_state == State::Down)
		{
			m_skeletonComponent.Enable();

			m_animationTime = 0.f;

			if (m_pUpSoundComponent)
			{
				m_pUpSoundComponent->Play();
			}

			if (Optional<ProjectileTarget*> pTarget = m_owner.FindFirstDataComponentOfTypeInParents<ProjectileTarget>())
			{
				pTarget->Enable();
			}

			const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses =
				m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointBindPoses();
			m_upBlendspace
				.SetTimeRatio(0.f, jointBindPoses, m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetStructureOfArraysJointCount());

			m_upBlendspace.SetBlendRatio(0.0f);
			m_finalBlendspace.SetBlendRatio(0.0f);
			m_state = State::GoingUp;
		}
	}

	void Target::OnSimulationResumed(Entity::Component3D& owner)
	{
		SubscribeToProjectileTarget(owner);

		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (!owner.HasAnyDataComponentsImplementingType<Data::Reset>(sceneRegistry))
		{
			owner.CreateDataComponent<Data::Reset>(Data::Reset::Initializer{
				Entity::Data::Component3D::DynamicInitializer{owner, sceneRegistry},
				[this](Entity::Component3D&)
				{
					m_animationTime = 0.f;
					m_skeletonComponent.Enable();
					StartReerect();
				},
				owner
			});
		}
	}

	void Target::OnSimulationPaused(Entity::Component3D& owner)
	{
		if (Optional<ProjectileTarget*> pTarget = owner.FindFirstDataComponentOfTypeInParents<ProjectileTarget>())
		{
			pTarget->OnHit.Remove(this);
		}
	}

	bool Target::ShouldUpdate() const
	{
		return m_upBlendspace.IsValid() && m_downBlendspace.IsValid();
	}

	void Target::Update()
	{
		FrameTime frameTime = m_skeletonComponent.GetCurrentFrameTime();

		if (m_state == State::Up || m_state == State::Down)
		{
			frameTime = FrameTime(0_milliseconds);
		}

		Assert(ShouldUpdate());
		const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses =
			m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointBindPoses();
		const uint16 sampledTransformCount = m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetStructureOfArraysJointCount();
		const ArrayView<ozz::math::SoaTransform, uint16> sampledTransforms = m_skeletonComponent.GetSkeletonInstance().GetSampledTransforms();

		if (m_state == State::GoingDown || m_state == State::GoingUp)
		{
			UpdateAnimation(frameTime);
		}

		m_finalBlendspace.Advance(frameTime, jointBindPoses, sampledTransformCount);
		m_finalBlendspace.Process(jointBindPoses, sampledTransforms);
	}

	void Target::OnSkeletonChanged()
	{
		const uint16 jointCount = m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointCount();
		const uint16 sampledTransformCount = m_skeletonComponent.GetSkeletonInstance().GetSampledTransforms().GetSize();
		m_upBlendspace.OnSkeletonChanged(jointCount, sampledTransformCount);
		m_downBlendspace.OnSkeletonChanged(jointCount, sampledTransformCount);
		m_finalBlendspace.Initialize(sampledTransformCount, 2);
	}

	void Target::DeserializeCustomData(const Optional<Serialization::Reader> serializer, Entity::Component3D& parent)
	{
		if (serializer.IsValid())
		{
			ngine::Animation::SkeletonComponent& skeletonComponent = static_cast<ngine::Animation::SkeletonComponent&>(parent);

			serializer->Serialize("up_animations", m_upBlendspace, skeletonComponent);
			serializer->Serialize("down_animations", m_downBlendspace, skeletonComponent);

			m_upBlendspace.LoadAnimations(skeletonComponent);
			m_downBlendspace.LoadAnimations(skeletonComponent);
		}
	}

	bool Target::SerializeCustomData(Serialization::Writer, const Entity::Component3D&) const
	{
		return true;
	}

	void Target::SetUpAudioAsset(const Asset::Picker asset)
	{
		SpawnOrSetSoundSpotAsset(m_pUpSoundComponent, asset.GetAssetGuid());
	}

	Asset::Picker Target::GetUpAudioAsset() const
	{
		return GetAssetFromSoundSpot(m_pUpSoundComponent);
	}

	void Target::SetDownAudioAsset(const Asset::Picker asset)
	{
		SpawnOrSetSoundSpotAsset(m_pDownSoundComponent, asset.GetAssetGuid());
	}

	Asset::Picker Target::GetDownAudioAsset() const
	{
		return GetAssetFromSoundSpot(m_pDownSoundComponent);
	}

	[[maybe_unused]] const bool wasTargetControllerRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Target>>::Make());
	[[maybe_unused]] const bool wasTargetControllerTypeRegistered = Reflection::Registry::RegisterType<Target>();
}
