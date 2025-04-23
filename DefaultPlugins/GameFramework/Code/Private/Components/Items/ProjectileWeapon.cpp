#include <GameFramework/Components/Items/ProjectileWeapon.h>
#include <GameFramework/Components/Items/Projectile.h>
#include <GameFramework/Components/Items/FirearmProperties.h>

#include <GameFramework/Components/Player/Ammunition.h>

#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/ComponentValue.inl>
#include <Engine/Entity/Primitives/SphereComponent.h>

#include <Renderer/Renderer.h>

#include <PhysicsCore/Components/Data/BodyComponent.h>
#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/SphereColliderComponent.h>

#include <AudioCore/Components/SoundSpotComponent.h>
#include <AudioCore/AudioAssetType.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	ProjectileWeapon::ProjectileWeapon(const Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
		, m_firearmProperties(Data::FirearmProperties::MakeAutomaticRifle())
		, m_projectileProperties(Data::ProjectileProperties::MakeAutomaticRifle())
	{
		m_ammunitionCount = m_firearmProperties.m_magazineCapacity;
	}

	ProjectileWeapon::ProjectileWeapon(const ProjectileWeapon& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_firearmProperties(templateComponent.m_firearmProperties)
		, m_projectileProperties(templateComponent.m_projectileProperties)
	{
	}

	ProjectileWeapon::ProjectileWeapon(Initializer&& initializer)
		: m_owner(initializer.GetParent())
		, m_firearmProperties(Move(initializer.m_firearmProperties))
		, m_projectileProperties(Move(initializer.m_projectileProperties))
	{
		m_ammunitionCount = m_firearmProperties.m_magazineCapacity;
	}

	ProjectileWeapon::FireResult ProjectileWeapon::Fire(
		Entity::Component3D& owner, Entity::Component3D& shooter, const Math::WorldLine line, const ArrayView<const JPH::BodyID> ignoredBodies
	)
	{
		if (m_ammunitionCount == 0)
		{
			if (m_flags.IsSet(Flags::IsFiring))
			{
				if (m_soundSpotComponents[Sound::EmptyFire].IsValid())
				{
					m_soundSpotComponents[Sound::EmptyFire]->Play();
				}

				m_flags.Clear(Flags::IsFiring);
				m_flags |= Flags::ShouldAutoReload;
				return FireResult::OutOfAmmoSingle;
			}
			return FireResult::OutOfAmmoBurst;
		}
		if (!CanFireInternal())
		{
			if (m_flags.IsSet(Flags::IsFiring))
			{
				m_flags.Clear(Flags::IsFiring);
				return FireResult::NotReadySingle;
			}
			return FireResult::NotReadyBurst;
		}

		if (m_soundSpotComponents[Sound::Fire].IsValid())
		{
			m_soundSpotComponents[Sound::Fire]->Play();
		}

		if (m_soundSpotComponents[Sound::Casing].IsValid())
		{
			m_soundSpotComponents[Sound::Casing]->PlayDelayed(0.4_seconds);
		}

		m_lastShotTime = Time::Timestamp::GetCurrent();
		m_burstCount += 1;
		m_ammunitionCount -= 1;

		if (const Optional<Ammunition*> pAmmunitionComponent = shooter.FindDataComponentOfType<Ammunition>())
		{
			pAmmunitionComponent->Remove(1);
		}

		const Math::WorldTransform projectileTransform{Math::Quaternionf{line.GetDirection(), owner.GetWorldUpDirection()}, line.GetStart()};
		if (const Optional<Entity::Component3D*> pProjectileOwner = CreateProjectile(owner, projectileTransform))
		{
			Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
			Projectile& projectile = *pProjectileOwner->FindDataComponentOfType<Projectile>(sceneRegistry);
			projectile.Fire(line.GetDirection() * m_projectileProperties.m_speed.GetMetersPerSecond(), Math::Zero, ignoredBodies);
		}

		m_flags.Clear(Flags::IsFiring);

		return FireResult::Fired;
	}

	bool ProjectileWeapon::CanFireInternal() const
	{
		const bool canBurstFire = m_burstCount < m_firearmProperties.m_burstSize;
		const Time::Timestamp timeSinceLastShot = Time::Timestamp::GetCurrent() - m_lastShotTime;
		const bool isBurstReady = timeSinceLastShot.GetDuration() >= m_firearmProperties.m_fireRate.GetDuration();

		return canBurstFire && isBurstReady;
	}

	bool ProjectileWeapon::CanFire() const
	{
		return CanFireInternal() && m_flags.AreAnySet(Flags::IsTriggerHeld | Flags::IsFiring);
	}

	void ProjectileWeapon::HoldTrigger()
	{
		if (m_flags.IsNotSet(Flags::IsTriggerHeld))
		{
			m_flags |= Flags::IsFiring;
		}
		m_flags |= Flags::IsTriggerHeld;
	}

	void ProjectileWeapon::ReleaseTrigger()
	{
		if (m_flags.IsSet(Flags::IsTriggerHeld))
		{
			m_burstCount = 0;
		}
		m_flags.Clear(Flags::IsTriggerHeld);
	}

	void ProjectileWeapon::StartReload()
	{
		m_flags |= Flags::IsReloading;

		if (m_soundSpotComponents[Sound::Reload])
		{
			m_soundSpotComponents[Sound::Reload]->Play();
		}
	}

	void ProjectileWeapon::FinishReload([[maybe_unused]] Entity::Component3D& owner, Entity::Component3D& shooter)
	{
		const Optional<Ammunition*> pAmmunitionComponent = shooter.FindDataComponentOfType<Ammunition>();

		// Take out the remaining ammunition
		if (pAmmunitionComponent.IsValid())
		{
			pAmmunitionComponent->Remove(m_ammunitionCount);
		}

		m_ammunitionCount = m_firearmProperties.m_magazineCapacity;

		if (pAmmunitionComponent.IsValid())
		{
			pAmmunitionComponent->Add(m_ammunitionCount);
		}

		m_flags.Clear(Flags::IsReloading | Flags::ShouldAutoReload);
	}

	void ProjectileWeapon::SetSoundAsset(
		Entity::Component3D& owner,
		Entity::SceneRegistry& sceneRegistry,
		const Sound sound,
		const Asset::Guid assetGuid,
		const Math::Radiusf soundRadius
	)
	{
		Assert(assetGuid.IsValid());
		if (m_soundSpotComponents[sound].IsInvalid())
		{
			Entity::ComponentTypeSceneData<Audio::SoundSpotComponent>& soundSpotSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Audio::SoundSpotComponent>();

			m_soundSpotComponents[sound] = soundSpotSceneData.CreateInstance(Audio::SoundSpotComponent::Initializer(
				Entity::Component3D::Initializer{owner},
				Audio::SoundSpotComponent::Flags{},
				assetGuid,
				Audio::Volume{100_percent},
				soundRadius
			));
		}
		else
		{
			m_soundSpotComponents[sound]->SetAudioAsset(Asset::Picker{assetGuid, Audio::AssetFormat.assetTypeGuid});
		}
	}

	Optional<Entity::Component3D*>
	ProjectileWeapon::CreateProjectile(Entity::Component3D& owner, const Math::WorldTransform worldTransform) const
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();

		Entity::ComponentValue<Entity::Component3D> pProjectileComponent{
			sceneRegistry,
			Entity::Component3D::Initializer{owner.GetRootSceneComponent(), worldTransform}
		};

		if (Ensure(pProjectileComponent.IsValid()))
		{
			Physics::Data::Body& physicsBody = *pProjectileComponent->CreateDataComponent<Physics::Data::Body>(Physics::Data::Body::Initializer{
				Entity::Data::Component3D::DynamicInitializer{*pProjectileComponent, sceneRegistry},
				Physics::BodySettings{
					Physics::BodyType::Dynamic,
					Physics::Layer::Dynamic,
					Physics::BodySettings::DefaultMaximumAngularVelocity,
					0_kilograms,
					100_percent,
					Physics::BodyFlags::IsSensorOnly | Physics::BodyFlags::LinearCast
				}
			});

			const Math::Radiusf radius = Math::Max(m_projectileProperties.m_radius, (Math::Radiusf)0.001_meters);
			Entity::ComponentValue<Physics::SphereColliderComponent> pSphereColliderComponent{
				sceneRegistry,
				Physics::SphereColliderComponent::Initializer{
					Physics::ColliderComponent::Initializer{
						Entity::Component3D::Initializer{*pProjectileComponent, worldTransform},
						Optional<const Physics::Material*>{},
						physicsBody,
						*pProjectileComponent
					},
					radius
				}
			};

			Entity::ComponentValue<Entity::StaticMeshComponent> pSphereComponent{
				sceneRegistry,
				Entity::StaticMeshComponent::Initializer{
					Entity::RenderItemComponent::Initializer{
						Entity::Component3D::Initializer{
							*pSphereColliderComponent,
							Math::LocalTransform{Math::Identity, Math::Zero, Math::Vector3f{radius.GetMeters()}}
						},
						Rendering::RenderItemStageMask{}
					},
					m_projectileProperties.m_staticMeshIdentifier,
					m_projectileProperties.m_materialInstanceIdentifier
				}
			};

			pProjectileComponent->CreateDataComponent<Projectile>(Projectile::Initializer{
				Entity::Data::Component3D::DynamicInitializer{*pProjectileComponent, sceneRegistry},
				owner,
				Data::ProjectileProperties{m_projectileProperties}
			});
		}

		return pProjectileComponent;
	}

	[[maybe_unused]] const bool wasProjectileWeaponRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ProjectileWeapon>>::Make());
	[[maybe_unused]] const bool wasProjectileWeaponTypeRegistered = Reflection::Registry::RegisterType<ProjectileWeapon>();
}
