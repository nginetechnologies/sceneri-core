#include "TempVisualScripting/DamageSensor.h"
#include "Tags.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Tag/TagRegistry.h>

#include <Renderer/Assets/StaticMesh/MeshAssetType.h>

#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <GameFramework/Reset/ResetComponent.h>
#include <GameFramework/Components/Player/Player.h>
#include <GameFramework/Components/SceneRules/SceneRules.h>
#include <GameFramework/Components/SceneRules/Modules/HealthModule.h>

#include <AudioCore/Components/SoundSpotComponent.h>
#include <AudioCore/AudioAssetType.h>

#include <NetworkingCore/Components/BoundComponent.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	DamageSensorComponent::DamageSensorComponent(Initializer&& initializer)
		: SensorComponent(SensorComponent::Initializer(
				Forward<SensorComponent::BaseType::Initializer>(initializer),
				Tag::Mask(System::Get<Tag::Registry>().FindOrRegister(Tags::PlayerTagGuid))
			))
	{
	}

	DamageSensorComponent::DamageSensorComponent(const DamageSensorComponent& templateComponent, const Cloner& cloner)
		: SensorComponent(templateComponent, cloner)
		, m_damage(templateComponent.m_damage)
		, m_audioAssetGuid(templateComponent.m_audioAssetGuid)
		, m_sceneTemplateIdentifier(templateComponent.m_sceneTemplateIdentifier)
	{
	}

	void DamageSensorComponent::OnCreated()
	{
		SensorComponent::OnCreated();
		OnComponentDetected.Add(*this, &DamageSensorComponent::Collect);

		if (m_sceneTemplateIdentifier.IsValid() && !GetRootScene().IsTemplate())
		{
			SpawnMesh();
		}
	}

	void DamageSensorComponent::Collect(SensorComponent&, Optional<Entity::Component3D*> pCollectorComponent)
	{
		const Optional<SceneRules*> pSceneRules = SceneRules::Find(GetRootSceneComponent());

		if (m_damage != 0 && pCollectorComponent.IsValid() && pSceneRules.IsValid())
		{
			if (Optional<Entity::SceneComponent*> pCollectorSceneComponent = pCollectorComponent->GetParentSceneComponent())
			{
				if (const Optional<HealthModule*> pHealthModule = pSceneRules->FindDataComponentOfType<HealthModule>())
				{
					if (pSceneRules->IsHost())
					{
						if (const Optional<Player*> pPlayerDataComponent = pCollectorSceneComponent->FindDataComponentOfType<Player>())
						{
							pHealthModule->AddHealth(*pSceneRules, pPlayerDataComponent->GetClientIdentifier(), -m_damage);
						}
					}
				}
			}
		}

		if (pSceneRules.IsValid())
		{
			if (pSceneRules->IsHost())
			{
				if (const Optional<Network::Session::BoundComponent*> pBoundComponent = FindDataComponentOfType<Network::Session::BoundComponent>())
				{
					pBoundComponent
						->BroadcastToAllClients<&DamageSensorComponent::ClientOnTriggeredOnHost>(*this, GetSceneRegistry(), Network::Channel{0});
				}
			}
		}

		if (GetOwner().IsEnabled())
		{
			OnTriggeredInternal();
		}
	}

	void DamageSensorComponent::ClientOnTriggeredOnHost(Network::Session::BoundComponent&, Network::LocalClient&)
	{
		if (GetOwner().IsEnabled())
		{
			OnTriggeredInternal();
		}
	}

	void DamageSensorComponent::OnTriggeredInternal()
	{
		CreateDataComponent<Data::Reset>(Data::Reset::Initializer{
			Entity::Data::Component3D::DynamicInitializer{*this, GetSceneRegistry()},
			[this](Entity::Component3D&)
			{
				if (m_pSoundSpotComponent.IsValid())
				{
					m_pSoundSpotComponent->Destroy(GetSceneRegistry());
				}
			},
			*this
		});

		if (m_audioAssetGuid.IsValid())
		{
			Entity::ComponentTypeSceneData<Audio::SoundSpotComponent>& soundSpotSceneData =
				*GetSceneRegistry().GetOrCreateComponentTypeData<Audio::SoundSpotComponent>();
			m_pSoundSpotComponent = soundSpotSceneData.CreateInstance(Audio::SoundSpotComponent::Initializer(
				Entity::Component3D::Initializer{GetRootSceneComponent(), GetWorldTransform()},
				{Audio::SoundSpotComponent::Flags::Autoplay},
				m_audioAssetGuid
			));
		}
	}

	void DamageSensorComponent::SetAudioAsset(const AudioAssetPicker asset)
	{
		if (asset.IsValid())
		{
			m_audioAssetGuid = asset.GetAssetGuid();
		}
	}

	DamageSensorComponent::AudioAssetPicker DamageSensorComponent::GetAudioAsset() const
	{
		return {m_audioAssetGuid, Audio::AssetFormat.assetTypeGuid};
	}

	void DamageSensorComponent::SetSceneAsset(const ScenePicker asset)
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

	DamageSensorComponent::ScenePicker DamageSensorComponent::GetSceneAsset() const
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

	void DamageSensorComponent::SpawnMesh()
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

	Entity::Component3D& DamageSensorComponent::GetOwner()
	{
		if (const Optional<Entity::Component3D*> pSceneComponent = GetParentSceneComponent())
		{
			if (!pSceneComponent->IsRootSceneComponent())
			{
				return *pSceneComponent;
			}
		}
		return *this;
	}

	[[maybe_unused]] const bool wasDamageSensorRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<DamageSensorComponent>>::Make());
	[[maybe_unused]] const bool wasDamageSensorTypeRegistered = Reflection::Registry::RegisterType<DamageSensorComponent>();
}
