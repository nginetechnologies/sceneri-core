#include "TempVisualScripting/RespawnSensor.h"
#include "Tags.h"

#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Tag/TagRegistry.h>

#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include <Components/SceneRules/SceneRules.h>
#include <Components/SceneRules/Modules/SpawningModule.h>
#include <Components/Player/Player.h>
#include <Components/SpawnPoint.h>

namespace ngine::GameFramework
{
	RespawnSensor::RespawnSensor(Initializer&& initializer)
		: SensorComponent(SensorComponent::Initializer(
				Forward<SensorComponent::BaseType::Initializer>(initializer),
				Tag::Mask(System::Get<Tag::Registry>().FindOrRegister(Tags::PlayerTagGuid))
			))
	{
	}

	RespawnSensor::RespawnSensor(const RespawnSensor& templateComponent, const Cloner& cloner)
		: SensorComponent(templateComponent, cloner)
		, m_customSpawnPoint(
				templateComponent.m_customSpawnPoint,
				Entity::ComponentSoftReference::Cloner{cloner.GetTemplateSceneRegistry(), cloner.GetParent()->GetSceneRegistry()}
			)
	{
	}

	void RespawnSensor::OnCreated()
	{
		SensorComponent::OnCreated();
		OnComponentDetected.Add(*this, &RespawnSensor::Respawn);
	}

	void RespawnSensor::Respawn(SensorComponent&, Optional<Entity::Component3D*> pComponent)
	{
		if (const Optional<Player*> pPlayerComponent = pComponent->GetParentSceneComponent()->FindDataComponentOfType<Player>())
		{
			if (const Optional<SceneRules*> pSceneRules = SceneRules::Find(pComponent->GetRootSceneComponent());
			    pSceneRules.IsValid() && pSceneRules->IsHost())
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
	}

	void RespawnSensor::SetCustomSpawnPoint(Entity::Component3DPicker spawnPointPicker)
	{
		spawnPointPicker.SetAllowedComponentTypeGuids(Array{Reflection::GetTypeGuid<GameFramework::SpawnPointComponent>()});
		m_customSpawnPoint = spawnPointPicker;
	}

	Entity::Component3DPicker RespawnSensor::GetCustomSpawnPoint() const
	{
		Entity::Component3DPicker picker{m_customSpawnPoint, GetSceneRegistry()};
		picker.SetAllowedComponentTypeGuids(Array{Reflection::GetTypeGuid<GameFramework::SpawnPointComponent>()});
		return picker;
	}

	[[maybe_unused]] const bool wasRespawnSensorRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<RespawnSensor>>::Make());
	[[maybe_unused]] const bool wasRespawnSensorTypeRegistered = Reflection::Registry::RegisterType<RespawnSensor>();
}
