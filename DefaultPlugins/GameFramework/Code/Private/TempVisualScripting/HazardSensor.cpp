#include "TempVisualScripting/HazardSensor.h"
#include "Tags.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Tag/TagRegistry.h>

#include <Renderer/Assets/StaticMesh/MeshAssetType.h>

#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include <Components/SceneRules/SceneRules.h>
#include <Components/SceneRules/Modules/SpawningModule.h>
#include <Components/SceneRules/Modules/HealthModule.h>
#include <Components/SceneRules/Modules/FinishModule.h>
#include <Components/Player/Player.h>
#include <Components/SpawnPoint.h>

namespace ngine::GameFramework
{
	HazardSensor::HazardSensor(Initializer&& initializer)
		: SensorComponent(SensorComponent::Initializer(
				Forward<SensorComponent::BaseType::Initializer>(initializer),
				Tag::Mask(System::Get<Tag::Registry>().FindOrRegister(Tags::PlayerTagGuid))
			))
	{
	}

	HazardSensor::HazardSensor(const HazardSensor& templateComponent, const Cloner& cloner)
		: SensorComponent(templateComponent, cloner)
		, m_customSpawnPoint(
				templateComponent.m_customSpawnPoint,
				Entity::ComponentSoftReference::Cloner{cloner.GetTemplateSceneRegistry(), cloner.GetParent()->GetSceneRegistry()}
			)
		, m_sceneTemplateIdentifier(templateComponent.m_sceneTemplateIdentifier)
	{
	}

	void HazardSensor::OnCreated()
	{
		SensorComponent::OnCreated();
		OnComponentDetected.Add(*this, &HazardSensor::Restart);

		if (m_sceneTemplateIdentifier.IsValid() && !GetRootScene().IsTemplate())
		{
			SpawnMesh();
		}
	}

	void HazardSensor::Restart(SensorComponent&, Optional<Entity::Component3D*> pComponent)
	{
		if (const Optional<SceneRules*> pSceneRules = SceneRules::Find(pComponent->GetRootSceneComponent());
		    pSceneRules.IsValid() && pSceneRules->IsHost())
		{
			Entity::Component3D& playerSceneComponent = *pComponent->GetParentSceneComponent();

			if (const Optional<Player*> pPlayerComponent = playerSceneComponent.FindDataComponentOfType<Player>())
			{
				if (const Optional<HealthModule*> pHealthModule = pSceneRules->FindDataComponentOfType<HealthModule>())
				{
					pHealthModule->AddHealth(*pSceneRules, pPlayerComponent->GetClientIdentifier(), -1);

					if (pHealthModule->GetHealth(*pSceneRules, pPlayerComponent->GetClientIdentifier()) > 0)
					{
						if (Optional<SpawningModule*> pSpawningModule = pSceneRules->FindDataComponentOfType<SpawningModule>())
						{
							if (Optional<Entity::Component3D*> pSpawnPoint = m_customSpawnPoint.Find<Entity::Component3D>(GetSceneRegistry()))
							{
								pSpawningModule->RespawnPlayer(*pSceneRules, pPlayerComponent->GetClientIdentifier(), pSpawnPoint->GetWorldTransform());
							}
							else if (pSpawnPoint = pSpawningModule->GetDefaultSpawnPoint(*pSceneRules, pPlayerComponent->GetClientIdentifier());
							         pSpawnPoint.IsValid())
							{
								pSpawningModule->RespawnPlayer(*pSceneRules, pPlayerComponent->GetClientIdentifier(), pSpawnPoint->GetWorldTransform());
							}
						}
					}
				}
				else if (const Optional<FinishModule*> pFinishModule = pSceneRules->FindDataComponentOfType<FinishModule>();
				         pFinishModule.IsValid() && !pFinishModule->HasPlayerFinished(pPlayerComponent->GetClientIdentifier()))
				{
					pFinishModule->NotifyPlayerFinished(*pSceneRules, pPlayerComponent->GetClientIdentifier(), GameRulesFinishResult::Failure);
				}
			}
		}
	}

	void HazardSensor::SetCustomSpawnPoint(Entity::Component3DPicker spawnPointPicker)
	{
		spawnPointPicker.SetAllowedComponentTypeGuids(Array{Reflection::GetTypeGuid<GameFramework::SpawnPointComponent>()});
		m_customSpawnPoint = *static_cast<Entity::ComponentSoftReference*>(&spawnPointPicker);
	}

	Entity::Component3DPicker HazardSensor::GetCustomSpawnPoint() const
	{
		Entity::Component3DPicker picker{m_customSpawnPoint, GetSceneRegistry()};
		picker.SetAllowedComponentTypeGuids(Array{Reflection::GetTypeGuid<GameFramework::SpawnPointComponent>()});
		return picker;
	}

	void HazardSensor::SetSceneAsset(const ScenePicker asset)
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		m_sceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(asset.m_asset.GetAssetGuid());

		if (m_pMeshComponent.IsValid())
		{
			m_pMeshComponent->Destroy(GetSceneRegistry());
			m_pMeshComponent = Invalid;
		}

		SpawnMesh();
	}

	HazardSensor::ScenePicker HazardSensor::GetSceneAsset() const
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		return {
			Asset::Reference{
				m_sceneTemplateIdentifier.IsValid() ? sceneTemplateCache.GetAssetGuid(m_sceneTemplateIdentifier) : Asset::Guid{},
				MeshSceneAssetType::AssetFormat.assetTypeGuid
			},
			Asset::Types{Array<Asset::TypeGuid, 1>{MeshSceneAssetType::AssetFormat.assetTypeGuid}}
		};
	}

	void HazardSensor::SpawnMesh()
	{
		Assert(m_pMeshComponent.IsInvalid());
		Assert(m_sceneTemplateIdentifier.IsValid());

		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Threading::JobBatch jobBatch = Entity::ComponentValue<Entity::Component3D>::DeserializeAsync(
			*this,
			sceneRegistry,
			sceneTemplateCache.GetAssetGuid(m_sceneTemplateIdentifier),
			[this, &sceneRegistry](const Optional<Entity::Component3D*> pComponent, Threading::JobBatch&& jobBatch) mutable
			{
				m_pMeshComponent = pComponent;
				if (pComponent.IsValid())
				{
					pComponent->DisableSaveToDisk(sceneRegistry);
					pComponent->DisableCloning(sceneRegistry);

					// Disable physics on the body
					if (Entity::Component3D::DataComponentResult<Physics::Data::Body> bodyComponent = pComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Physics::Data::Body>(sceneRegistry))
					{
						bodyComponent.m_pDataComponentOwner->Disable(sceneRegistry);
					}
				}

				if (jobBatch.IsValid())
				{
					Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
				}
			}
		);
		if (jobBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
		}
	}

	[[maybe_unused]] const bool wasRestartGameRulesRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<HazardSensor>>::Make());
	[[maybe_unused]] const bool wasRestartGameRulesTypeRegistered = Reflection::Registry::RegisterType<HazardSensor>();
}
