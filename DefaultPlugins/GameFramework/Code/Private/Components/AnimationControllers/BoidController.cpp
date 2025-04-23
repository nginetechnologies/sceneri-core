#include "Components/AnimationControllers/BoidController.h"
#include "Components/Boid.h"
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
	Boid::Boid(Initializer&& initializer)
		: Controller(Forward<Initializer>(initializer))
		, m_owner(initializer.GetParent())
	{
	}

	Boid::Boid(const Boid& templateComponent, const Cloner& cloner)
		: Controller(templateComponent, cloner)
		, m_movingBlendspace(templateComponent.m_movingBlendspace)
		, m_dyingBlendspace(templateComponent.m_dyingBlendspace)
		, m_finalBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 2>{m_movingBlendspace, m_dyingBlendspace}, m_skeletonComponent
			)
		, m_owner(cloner.GetParent())
		, m_state(templateComponent.m_state)
	{
		m_movingBlendspace.LoadAnimations(m_skeletonComponent);
		m_dyingBlendspace.LoadAnimations(m_skeletonComponent);

		if (templateComponent.GetDeathAudioAsset().IsValid())
		{
			SetDeathAudioAsset(templateComponent.GetDeathAudioAsset());
		}

		SetStateFromBoid(cloner.GetParent());

		if (const Optional<const ngine::Animation::Skeleton*> pSkeleton = m_skeletonComponent.GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged();
			m_skeletonComponent.TryEnableUpdate();
		}
	}

	Boid::Boid(const Deserializer& deserializer)
		: Controller(deserializer)
		, m_movingBlendspace(deserializer.m_reader.FindSerializer("moving_animations"), m_skeletonComponent)
		, m_dyingBlendspace(deserializer.m_reader.FindSerializer("dying_animations"), m_skeletonComponent)
		, m_finalBlendspace(
				Array<const ngine::Animation::Blendspace1D::EntryInitializer, 2>{m_movingBlendspace, m_dyingBlendspace}, m_skeletonComponent
			)
		, m_owner(deserializer.GetParent())
	{
		SetStateFromBoid(deserializer.GetParent());

		if (const Optional<const ngine::Animation::Skeleton*> pSkeleton = m_skeletonComponent.GetSkeletonInstance().GetSkeleton();
		    pSkeleton.IsValid() && pSkeleton->IsValid())
		{
			OnSkeletonChanged();
			m_skeletonComponent.TryEnableUpdate();
		}
	}

	void Boid::UpdateDeathAnimation(const float deltaTime)
	{
		if (m_state != State::Dying)
		{
			m_finalBlendspace.SetBlendRatio(0.0f);
			return;
		}

		const float blendOutOneshotTime = 0.5f;
		if (Math::SignNonZero(m_deathAnimationTime) > 0.f)
		{
			const float blendToOneshotTime = 0.1f;
			m_deathAnimationTime += deltaTime;
			const float blendToOneshotRatio = Math::Min(m_deathAnimationTime / blendToOneshotTime, 1.f);
			m_finalBlendspace.SetBlendRatio(blendToOneshotRatio);
			const Optional<ngine::Animation::Animation*> pOneShotAnimation = m_dyingBlendspace.GetAnimation(0);
			const Time::Durationf oneshotDuration = pOneShotAnimation.IsValid() ? pOneShotAnimation->GetDuration() : Time::Durationf{0_seconds};
			if (m_deathAnimationTime >= (oneshotDuration.GetSeconds() - blendOutOneshotTime))
			{
				float time = m_deathAnimationTime - (oneshotDuration.GetSeconds() - blendOutOneshotTime);
				const float blendToDeathTime = 0.5f;
				const float blendToDeathRatio = Math::Min(time / blendToDeathTime, 1.f);

				m_dyingBlendspace.SetBlendRatio(blendToDeathRatio);
				if (blendToDeathRatio >= 1.0f)
				{
					m_state = State::Dead;
				}
			}
		}
	}

	void Boid::SetStateFromBoid(Entity::Component3D& owner)
	{
		if (Optional<GameFramework::Boid*> pBoid = owner.FindFirstDataComponentOfTypeInParents<GameFramework::Boid>())
		{
			switch (pBoid->GetState())
			{
				case GameFramework::Boid::StartState::Idle:
					m_state = State::Idle;
					m_finalBlendspace.SetBlendRatio(0.0f);
					m_movingBlendspace.SetBlendRatio(0.0f);
					break;
				case GameFramework::Boid::StartState::Moving:
					m_state = State::Moving;
					m_finalBlendspace.SetBlendRatio(0.0f);
					m_movingBlendspace.SetBlendRatio(1.0f);
					break;
			}
		}
		else
		{
			m_state = State::Idle;
		}
	}

	void Boid::StartDeath()
	{
		if (m_state != State::Dying && m_state != State::Dead)
		{
			m_deathAnimationTime = 0.f;

			if (m_pDeathSoundComponent)
			{
				m_pDeathSoundComponent->Play();
			}

			const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses =
				m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointBindPoses();
			m_dyingBlendspace
				.SetTimeRatio(0.f, jointBindPoses, m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetStructureOfArraysJointCount());

			m_dyingBlendspace.SetBlendRatio(0.0f);
			m_state = State::Dying;
		}
	}

	void Boid::OnSimulationResumed(Entity::Component3D& owner)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (!owner.HasAnyDataComponentsImplementingType<Data::Reset>(sceneRegistry))
		{
			owner.CreateDataComponent<Data::Reset>(Data::Reset::Initializer{
				Entity::Data::Component3D::DynamicInitializer{owner, sceneRegistry},
				[this](Entity::Component3D& owner)
				{
					m_deathAnimationTime = 0.f;
					SetStateFromBoid(owner);
				},
				owner
			});
		}

		if (Optional<ProjectileTarget*> pTarget = owner.FindFirstDataComponentOfTypeInParents<ProjectileTarget>())
		{
			pTarget->OnHit.Add(
				*this,
				[](Boid& boid, ProjectileTarget::HitSettings settings)
				{
					if (auto componentResult = settings.pComponent->GetParentSceneComponent()->GetParent().FindFirstDataComponentOfTypeInChildrenRecursive<SplineMovementComponent>())
					{
						if (Optional<Entity::ComponentTypeSceneData<SplineMovementComponent>*> pSceneData = componentResult.m_pDataComponentOwner->GetSceneRegistry().FindComponentTypeData<SplineMovementComponent>())
						{
							pSceneData->DisableAfterPhysicsUpdate(*componentResult.m_pDataComponent);
						}
					}

					if (auto componentResult = settings.pComponent->GetParentSceneComponent()->FindFirstDataComponentOfTypeInChildrenRecursive<Physics::Data::Body>())
					{
						if (componentResult.m_pDataComponent && componentResult.m_pDataComponentOwner)
						{
							Physics::Data::Scene& physicsScene =
								*componentResult.m_pDataComponentOwner->GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
							Physics::Data::Body& body = *componentResult.m_pDataComponent;
							body.SetType(physicsScene, Physics::BodyType::Dynamic);
							body.SetMassOverride(physicsScene, 4_kilograms);
						}
					}

					boid.StartDeath();
				}
			);
		}
	}

	void Boid::OnSimulationPaused(Entity::Component3D& owner)
	{
		if (Optional<ProjectileTarget*> pTarget = owner.FindFirstDataComponentOfTypeInParents<ProjectileTarget>())
		{
			pTarget->OnHit.Remove(this);
		}
	}

	bool Boid::ShouldUpdate() const
	{
		return m_movingBlendspace.IsValid() && m_dyingBlendspace.IsValid();
	}

	void Boid::Update()
	{
		const FrameTime frameTime = m_skeletonComponent.GetCurrentFrameTime();

		Assert(ShouldUpdate());
		const ArrayView<const ozz::math::SoaTransform, uint16> jointBindPoses =
			m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointBindPoses();
		const uint16 sampledTransformCount = m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetStructureOfArraysJointCount();
		const ArrayView<ozz::math::SoaTransform, uint16> sampledTransforms = m_skeletonComponent.GetSkeletonInstance().GetSampledTransforms();

		if (m_state == State::Dying)
		{
			UpdateDeathAnimation(frameTime);
		}

		m_finalBlendspace.Advance(frameTime, jointBindPoses, sampledTransformCount);
		m_finalBlendspace.Process(jointBindPoses, sampledTransforms);
	}

	void Boid::OnSkeletonChanged()
	{
		const uint16 jointCount = m_skeletonComponent.GetSkeletonInstance().GetSkeleton()->GetJointCount();
		const uint16 sampledTransformCount = m_skeletonComponent.GetSkeletonInstance().GetSampledTransforms().GetSize();
		m_movingBlendspace.OnSkeletonChanged(jointCount, sampledTransformCount);
		m_dyingBlendspace.OnSkeletonChanged(jointCount, sampledTransformCount);
		m_finalBlendspace.Initialize(sampledTransformCount, 2);
	}

	void Boid::DeserializeCustomData(const Optional<Serialization::Reader> serializer, Entity::Component3D& parent)
	{
		if (serializer.IsValid())
		{
			ngine::Animation::SkeletonComponent& skeletonComponent = static_cast<ngine::Animation::SkeletonComponent&>(parent);

			serializer->Serialize("moving_animations", m_movingBlendspace, skeletonComponent);
			serializer->Serialize("dying_animations", m_dyingBlendspace, skeletonComponent);

			m_movingBlendspace.LoadAnimations(skeletonComponent);
			m_dyingBlendspace.LoadAnimations(skeletonComponent);
		}
	}

	bool Boid::SerializeCustomData(Serialization::Writer, const Entity::Component3D&) const
	{
		return true;
	}

	void Boid::SpawnOrSetSoundSpotAsset(Optional<Audio::SoundSpotComponent*>& pComponent, const Asset::Picker asset)
	{
		if (asset.IsValid())
		{
			if (!pComponent)
			{
				Entity::ComponentTypeSceneData<Audio::SoundSpotComponent>& soundSpotSceneData =
					*m_owner.GetSceneRegistry().GetOrCreateComponentTypeData<Audio::SoundSpotComponent>();

				pComponent = soundSpotSceneData.CreateInstance(
					Audio::SoundSpotComponent::Initializer(Entity::Component3D::Initializer{m_owner}, {}, asset.GetAssetGuid())
				);
			}
			else
			{
				pComponent->SetAudioAsset(asset);
			}
		}
	}

	Asset::Picker Boid::GetAssetFromSoundSpot(Optional<Audio::SoundSpotComponent*> pComponent) const
	{
		if (pComponent)
		{
			return pComponent->GetAudioAsset();
		}

		return {Guid(), Audio::AssetFormat.assetTypeGuid};
	}

	void Boid::SetDeathAudioAsset(const Asset::Picker asset)
	{
		SpawnOrSetSoundSpotAsset(m_pDeathSoundComponent, asset);
	}

	Asset::Picker Boid::GetDeathAudioAsset() const
	{
		return GetAssetFromSoundSpot(m_pDeathSoundComponent);
	}

	[[maybe_unused]] const bool wasBoidControllerRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Boid>>::Make());
	[[maybe_unused]] const bool wasBoidControllerTypeRegistered = Reflection::Registry::RegisterType<Boid>();
}
