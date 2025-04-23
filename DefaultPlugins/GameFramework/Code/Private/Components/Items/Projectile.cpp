#include <GameFramework/Components/Items/Projectile.h>
#include <GameFramework/Components/Items/ProjectileTarget.h>

#include <GameFramework/Components/Player/Player.h>

#include "Components/LifetimeComponent.h"

#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Primitives/PlaneComponent.h>
#include <Engine/Entity/ComponentValue.inl>

#include <Renderer/Renderer.h>

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>
#include <PhysicsCore/Contact.h>

#include <AudioCore/Components/SoundSpotComponent.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	Projectile::Projectile(const Deserializer& deserializer)
		: m_owner{deserializer.GetParent()}
		, m_shooter{deserializer.GetParent()}
	{
	}

	Projectile::Projectile(const Projectile& templateComponent, const Cloner& cloner)
		: m_owner{cloner.GetParent()}
		, m_shooter{cloner.GetParent()}
		, m_properties(templateComponent.m_properties)
	{
	}

	Projectile::Projectile(Initializer&& initializer)
		: m_owner{initializer.GetParent()}
		, m_shooter{initializer.m_shooter}
		, m_properties(Move(initializer.m_properties))
	{
	}

	void Projectile::OnCreated(Entity::Component3D& owner)
	{
		if (const Optional<Physics::Data::Body*> pBody = owner.FindDataComponentOfType<Physics::Data::Body>())
		{
			pBody->OnContactFound.Add<&Projectile::OnBeginContactInternal>(*this);
		}
	}

	void Projectile::OnDestroying(Entity::Component3D& owner)
	{
		if (const Optional<Physics::Data::Body*> pBody = owner.FindDataComponentOfType<Physics::Data::Body>())
		{
			pBody->OnContactFound.Remove(this);
		}
	}

	void
	Projectile::Fire(const Math::Vector3f velocity, const Math::Angle3f angularVelocity, const ArrayView<const JPH::BodyID> ignoredBodies)
	{
		m_ignoredBodies = ignoredBodies;

		Entity::Component3D& owner = m_owner;
		if (const Optional<Physics::Data::Scene*> pPhysicsScene = owner.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>())
		{
			if (const Optional<Physics::Data::Body*> pBody = owner.FindDataComponentOfType<Physics::Data::Body>())
			{
				pBody->SetVelocity(*pPhysicsScene, velocity);
				pBody->SetAngularVelocity(*pPhysicsScene, angularVelocity);

				pBody->Wake(*pPhysicsScene);
			}
		}
	}

	void Projectile::OnBeginContactInternal(const Physics::Contact& contact)
	{
		Entity::Component3D& owner = m_owner;
		Entity::Component3D& shooter = m_owner;
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();

		if (contact.otherComponent.IsValid() && contact.otherComponent->IsChildOfRecursive(shooter))
		{
			return;
		}

		Physics::Data::Scene& physicsScene = *owner.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
		if (contact.otherBody.IsValid())
		{
			if (contact.otherBody->GetActiveType(physicsScene) == Physics::Data::Body::Type::Dynamic)
			{
				contact.otherBody->AddImpulse(physicsScene, -contact.normal * m_properties.m_impulseFactor);
			}
		}

		bool shouldSpawnDecal = m_properties.m_impactDecalMaterialAssetGuid.IsValid();
		if (contact.otherComponent.IsValid())
		{
			if (Optional<ProjectileTarget*> pProjectileTarget = contact.otherComponent->GetParentSceneComponent()->FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<ProjectileTarget>(sceneRegistry))
			{
				ClientIdentifier shooterClientIdentifier;
				if (const Optional<Entity::Component3D*> pParentSceneComponent = shooter.GetParentSceneComponent())
				{
					if (const Optional<Player*> pPlayerDataComponent = pParentSceneComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Player>(sceneRegistry))
					{
						shooterClientIdentifier = pPlayerDataComponent->GetClientIdentifier();
					}
				}

				ProjectileTarget::HitSettings hitSettings{
					shooterClientIdentifier,
					contact.otherContactPoints[0],
					m_properties.m_damage,
					contact.otherComponent,
					contact.otherBody
				};

				pProjectileTarget->HitTarget(hitSettings);
				shouldSpawnDecal = false;
			}
		}

		if (m_properties.m_impactSoundAssetGuid.IsValid())
		{
			Entity::ComponentValue<Audio::SoundSpotComponent> soundSpotComponent{
				sceneRegistry,
				Audio::SoundSpotComponent::Initializer{
					Entity::Component3D::Initializer{
						owner.GetRootSceneComponent(),
						Math::WorldTransform{Math::Identity, contact.otherContactPoints[0]}
					},
					Audio::SoundSpotComponent::Flags{},
					m_properties.m_impactSoundAssetGuid,
					Audio::Volume{100_percent},
					m_properties.m_impactSoundRadius
				}
			};
			if (Ensure(soundSpotComponent.IsValid()))
			{
				soundSpotComponent->PlayDelayed(0.05_seconds);

				soundSpotComponent->CreateDataComponent<LifetimeComponent>(
					LifetimeComponent::Initializer{Entity::Data::Component3D::DynamicInitializer{*soundSpotComponent, sceneRegistry}, 10_seconds}
				);
			}
		}

		if (shouldSpawnDecal)
		{
			Entity::ComponentTypeSceneData<Entity::Primitives::PlaneComponent>& primitiveSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Entity::Primitives::PlaneComponent>();
			Rendering::MaterialInstanceCache& materialInstanceCache = System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache();
			const Rendering::MaterialInstanceIdentifier materialInstanceId =
				materialInstanceCache.FindOrRegisterAsset(m_properties.m_impactDecalMaterialAssetGuid);

			const Rendering::SceneRenderStageIdentifier stageIdentifier =
				System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier("bba6ad40-e1ee-4f1e-baf8-608ff9e7d77f"_asset);
			const Rendering::RenderItemStageMask stageMask(stageIdentifier);

			Optional<Entity::Component3D*> pParent = owner.GetRootSceneComponent();
			if (contact.otherComponent.IsValid())
			{
				pParent = contact.otherComponent;
			}

			const Math::WorldCoordinate spawnLocation = contact.otherContactPoints[0] +
			                                            -contact.normal * Math::NumericLimits<float>::Epsilon * 8.f;
			if (Optional<Entity::Primitives::PlaneComponent*> pComponent =
		primitiveSceneData.CreateInstance(Entity::Primitives::PlaneComponent::Initializer{
				Entity::RenderItemComponent::Initializer{
					Entity::Component3D::Initializer{
						*pParent,
						Math::WorldTransform(
							Math::Quaternionf(-contact.normal).TransformRotation(Math::Quaternionf(Math::Down, Math::Forward)),
							spawnLocation, Math::Vector3f(0.1f, 0.1f, 1.0f))
						},
					stageMask},
				materialInstanceId}))
			{
				pComponent->DisableSaveToDisk(sceneRegistry);
				pComponent->CreateDataComponent<LifetimeComponent>(
					LifetimeComponent::Initializer{Entity::Data::Component3D::DynamicInitializer{*pComponent, sceneRegistry}, 10_seconds}
				);
			}
		}

		owner.Destroy();
	}

	[[maybe_unused]] const bool wasProjectileRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Projectile>>::Make());
	[[maybe_unused]] const bool wasProjectileTypeRegistered = Reflection::Registry::RegisterType<Projectile>();
}
