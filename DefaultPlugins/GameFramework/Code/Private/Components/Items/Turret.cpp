#include <GameFramework/Components/Items/Turret.h>
#include <GameFramework/Components/Items/ProjectileWeapon.h>
#include <GameFramework/Components/Items/ProjectileWeapon.h>

#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/ComponentValue.inl>
#include <Common/Reflection/Registry.inl>

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>
#include <Renderer/Assets/Material/MaterialInstanceAssetType.h>

#include <AudioCore/Components/SoundSpotComponent.h>
#include <AudioCore/AudioAsset.h>

namespace ngine::GameFramework
{
	Turret::Turret(const Deserializer& deserializer)
		: m_owner{deserializer.GetParent()}
	{
		deserializer.GetParent().CreateDataComponent<ProjectileWeapon>(ProjectileWeapon::Initializer{
			Entity::Data::Component3D::DynamicInitializer{deserializer.GetParent(), deserializer.GetSceneRegistry()},
			Data::FirearmProperties::MakeAutomaticRifle(),
			Data::ProjectileProperties::MakeAutomaticRifle()
		});
	}

	Turret::Turret(const Turret&, const Cloner& cloner)
		: m_owner{cloner.GetParent()}
	{
		Threading::JobBatch jobBatch;
		cloner.GetParent().CreateDataComponent<ProjectileWeapon>(
			*cloner.GetTemplateParent().FindDataComponentOfType<ProjectileWeapon>(cloner.GetTemplateSceneRegistry()),
			ProjectileWeapon::Cloner{
				jobBatch,
				cloner.GetParent(),
				cloner.GetTemplateParent(),
				cloner.GetSceneRegistry(),
				cloner.GetTemplateSceneRegistry()
			}
		);
		Assert(jobBatch.IsInvalid());
	}

	Turret::Turret(Initializer&& initializer)
		: m_owner{initializer.GetParent()}
	{
		initializer.GetParent().CreateDataComponent<ProjectileWeapon>(ProjectileWeapon::Initializer{
			Entity::Data::Component3D::DynamicInitializer{initializer.GetParent(), initializer.GetSceneRegistry()},
			Data::FirearmProperties::MakeAutomaticRifle(),
			Data::ProjectileProperties::MakeAutomaticRifle()
		});
	}

	void Turret::OnCreated(Entity::Component3D& owner)
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		// TODO: Decide when to start firing
		projectileWeapon.HoldTrigger();

		if (owner.IsEnabled() && owner.IsSimulationActive())
		{
			RegisterForUpdate(owner);
		}
	}

	void Turret::OnDestroying(Entity::Component3D&)
	{
	}

	void Turret::RegisterForUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<Turret>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<Turret>();
		sceneData.EnableUpdate(*this);
	}

	void Turret::DeregisterUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<Turret>& sceneData = *owner.GetSceneRegistry().FindComponentTypeData<Turret>();
		sceneData.DisableUpdate(*this);
	}

	void Turret::OnEnable(Entity::Component3D& owner)
	{
		if (owner.IsSimulationActive())
		{
			RegisterForUpdate(owner);
		}
	}

	void Turret::OnDisable(Entity::Component3D& owner)
	{
		DeregisterUpdate(owner);
	}

	void Turret::OnSimulationResumed(Entity::Component3D& owner)
	{
		if (owner.IsEnabled())
		{
			RegisterForUpdate(owner);
		}
	}

	void Turret::OnSimulationPaused(Entity::Component3D& owner)
	{
		DeregisterUpdate(owner);
	}

	void Turret::Update()
	{
		Entity::Component3D& owner = m_owner;
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(sceneRegistry);

		if (m_remainingReloadTime > 0_seconds)
		{
			m_remainingReloadTime =
				Math::Max(m_remainingReloadTime - Time::Durationf::FromSeconds(owner.GetCurrentFrameTime()), (Time::Durationf)0_seconds);
			if (m_remainingReloadTime > 0_seconds)
			{
				return;
			}
			else
			{
				projectileWeapon.FinishReload(owner, owner);
			}
		}

		Array<JPH::BodyID, 1> bodiesFilter{};
		if (const Optional<Physics::Data::Body*> pBody = owner.FindDataComponentOfType<Physics::Data::Body>(sceneRegistry))
		{
			JPH::BodyID bodyID = pBody->GetIdentifier();
			bodiesFilter[0] = bodyID;
		}

		const Math::WorldTransform worldTransform = owner.GetWorldTransform();
		const Math::WorldLine line{worldTransform.GetLocation(), worldTransform.GetLocation() + worldTransform.GetForwardColumn() * 100};

		switch (projectileWeapon.Fire(owner, owner, line, bodiesFilter))
		{
			case ProjectileWeapon::FireResult::OutOfAmmoSingle:
			case ProjectileWeapon::FireResult::OutOfAmmoBurst:
			{
				projectileWeapon.StartReload();
				m_remainingReloadTime = m_reloadTime;
				if (m_remainingReloadTime == 0_seconds)
				{
					projectileWeapon.FinishReload(owner, owner);
				}
			}
			break;
			case ProjectileWeapon::FireResult::NotReadySingle:
			case ProjectileWeapon::FireResult::NotReadyBurst:
				projectileWeapon.ReleaseTrigger();
				projectileWeapon.HoldTrigger();
				break;
			case ProjectileWeapon::FireResult::Fired:
			{
				// TODO: Apply recoil here

				if (m_fireSoundAssetGuid.IsValid())
				{
					Entity::ComponentValue<Audio::SoundSpotComponent> soundSpotComponent{
						sceneRegistry,
						Audio::SoundSpotComponent::Initializer{
							Entity::Component3D::Initializer{
								owner.GetRootSceneComponent(),
								Math::WorldTransform{Math::Identity, owner.GetWorldLocation(sceneRegistry)}
							},
							Audio::SoundSpotComponent::Flags{},
							m_fireSoundAssetGuid,
							Audio::Volume{100_percent},
							m_fireSoundRadius
						}
					};
				}
			}
			break;
		}
	}

	int32 Turret::GetBurstSize(Entity::Component3D& owner) const
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		const Data::FirearmProperties firearmProperties = projectileWeapon.GetFirearmProperties();
		return firearmProperties.m_burstSize;
	}

	void Turret::SetBurstSize(Entity::Component3D& owner, const int32 value)
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		Data::FirearmProperties firearmProperties = projectileWeapon.GetFirearmProperties();
		firearmProperties.m_burstSize = value;
		projectileWeapon.SetFirearmProperties(Data::FirearmProperties{firearmProperties});
	}

	Math::Frequencyf Turret::GetFireRate(Entity::Component3D& owner) const
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		const Data::FirearmProperties firearmProperties = projectileWeapon.GetFirearmProperties();
		return firearmProperties.m_fireRate;
	}

	void Turret::SetFireRate(Entity::Component3D& owner, const Math::Frequencyf value)
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		Data::FirearmProperties firearmProperties = projectileWeapon.GetFirearmProperties();
		firearmProperties.m_fireRate = value;
		projectileWeapon.SetFirearmProperties(Data::FirearmProperties{firearmProperties});
	}

	Asset::Picker Turret::GetFireSoundAsset(Entity::Component3D&) const
	{
		return {m_fireSoundAssetGuid, Audio::AssetFormat.assetTypeGuid};
	}
	void Turret::SetFireSoundAsset(Entity::Component3D&, const Asset::Picker value)
	{
		m_fireSoundAssetGuid = value.GetAssetGuid();
	}

	int32 Turret::GetMagazineCapacity(Entity::Component3D& owner) const
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		const Data::FirearmProperties firearmProperties = projectileWeapon.GetFirearmProperties();
		return firearmProperties.m_magazineCapacity;
	}

	void Turret::SetMagazineCapacity(Entity::Component3D& owner, const int32 value)
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		Data::FirearmProperties firearmProperties = projectileWeapon.GetFirearmProperties();
		firearmProperties.m_magazineCapacity = value;
		projectileWeapon.SetFirearmProperties(Data::FirearmProperties{firearmProperties});
	}

	Time::Durationf Turret::GetReloadTime(Entity::Component3D&) const
	{
		return m_reloadTime;
	}

	void Turret::SetReloadTime(Entity::Component3D&, const Time::Durationf value)
	{
		m_reloadTime = value;
	}

	Math::Speedf Turret::GetProjectileSpeed(Entity::Component3D& owner) const
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		const Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		return projectileProperties.m_speed;
	}
	void Turret::SetProjectileSpeed(Entity::Component3D& owner, const Math::Speedf value)
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		projectileProperties.m_speed = value;
		projectileWeapon.SetProjectileProperties(Data::ProjectileProperties{projectileProperties});
	}

	float Turret::GetProjectileDamage(Entity::Component3D& owner) const
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		const Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		return projectileProperties.m_damage;
	}
	void Turret::SetProjectileDamage(Entity::Component3D& owner, const float value)
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		projectileProperties.m_damage = value;
		projectileWeapon.SetProjectileProperties(Data::ProjectileProperties{projectileProperties});
	}

	Math::Radiusf Turret::GetProjectileRadius(Entity::Component3D& owner) const
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		const Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		return projectileProperties.m_radius;
	}
	void Turret::SetProjectileRadius(Entity::Component3D& owner, const Math::Radiusf value)
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		projectileProperties.m_radius = value;
		projectileWeapon.SetProjectileProperties(Data::ProjectileProperties{projectileProperties});
	}

	Asset::Picker Turret::GetProjectileMeshAsset(Entity::Component3D& owner) const
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		const Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		const Guid meshAssetGuid = renderer.GetMeshCache().GetAssetGuid(projectileProperties.m_staticMeshIdentifier);
		return {meshAssetGuid, MeshSceneAssetType::AssetFormat.assetTypeGuid};
	}
	void Turret::SetProjectileMeshAsset(Entity::Component3D& owner, const Asset::Picker value)
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		projectileProperties.m_staticMeshIdentifier = renderer.GetMeshCache().FindOrRegisterAsset(value.GetAssetGuid());
		projectileWeapon.SetProjectileProperties(Data::ProjectileProperties{projectileProperties});
	}

	Asset::Picker Turret::GetProjectileMaterial(Entity::Component3D& owner) const
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		const Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		const Guid materialInstanceAssetGuid =
			renderer.GetMaterialCache().GetInstanceCache().GetAssetGuid(projectileProperties.m_materialInstanceIdentifier);
		return {materialInstanceAssetGuid, MaterialInstanceAssetType::AssetFormat.assetTypeGuid};
	}
	void Turret::SetProjectileMaterial(Entity::Component3D& owner, const Asset::Picker value)
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		projectileProperties.m_materialInstanceIdentifier =
			renderer.GetMaterialCache().GetInstanceCache().FindOrRegisterAsset(value.GetAssetGuid());
		projectileWeapon.SetProjectileProperties(Data::ProjectileProperties{projectileProperties});
	}

	Asset::Picker Turret::GetProjectileImpactSoundAsset(Entity::Component3D& owner) const
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		const Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		return {projectileProperties.m_impactSoundAssetGuid, Audio::AssetFormat.assetTypeGuid};
	}
	void Turret::SetProjectileImpactSoundAsset(Entity::Component3D& owner, const Asset::Picker value)
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		projectileProperties.m_impactSoundAssetGuid = value.GetAssetGuid();
		projectileWeapon.SetProjectileProperties(Data::ProjectileProperties{projectileProperties});
	}

	Math::Radiusf Turret::GetProjectileImpactSoundRadius(Entity::Component3D& owner) const
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		const Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		return projectileProperties.m_impactSoundRadius;
	}
	void Turret::SetProjectileImpactSoundRadius(Entity::Component3D& owner, const Math::Radiusf value)
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		projectileProperties.m_impactSoundRadius = value;
		projectileWeapon.SetProjectileProperties(Data::ProjectileProperties{projectileProperties});
	}

	Asset::Picker Turret::GetProjectileImpactDecalAsset(Entity::Component3D& owner) const
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		const Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		return {projectileProperties.m_impactDecalMaterialAssetGuid, MaterialInstanceAssetType::AssetFormat.assetTypeGuid};
	}
	void Turret::SetProjectileImpactDecalAsset(Entity::Component3D& owner, const Asset::Picker value)
	{
		ProjectileWeapon& projectileWeapon = *owner.FindDataComponentOfType<ProjectileWeapon>(owner.GetSceneRegistry());
		Data::ProjectileProperties projectileProperties = projectileWeapon.GetProjectileProperties();
		projectileProperties.m_impactDecalMaterialAssetGuid = value.GetAssetGuid();
		projectileWeapon.SetProjectileProperties(Data::ProjectileProperties{projectileProperties});
	}

	[[maybe_unused]] const bool wasTurretRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Turret>>::Make());
	[[maybe_unused]] const bool wasTurretTypeRegistered = Reflection::Registry::RegisterType<Turret>();
}
