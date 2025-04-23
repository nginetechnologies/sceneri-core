#include "TempVisualScripting/CollectibleComponent.h"
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
#include <GameFramework/Components/SceneRules/Modules/ScoreModule.h>

#include <AudioCore/Components/SoundSpotComponent.h>
#include <AudioCore/AudioAssetType.h>

#include <NetworkingCore/Components/BoundComponent.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	CollectibleComponent::CollectibleComponent(Initializer&& initializer)
		: SensorComponent(SensorComponent::Initializer(
				Forward<SensorComponent::BaseType::Initializer>(initializer),
				Tag::Mask(System::Get<Tag::Registry>().FindOrRegister(Tags::PlayerTagGuid))
			))
	{
		CreateDataComponent<Network::Session::BoundComponent>(
			Network::Session::BoundComponent::Initializer{*this, initializer.GetSceneRegistry()}
		);
	}

	CollectibleComponent::CollectibleComponent(const CollectibleComponent& templateComponent, const Cloner& cloner)
		: SensorComponent(templateComponent, cloner)
		, m_scoreReward(templateComponent.m_scoreReward)
		, m_audioAssetGuid(templateComponent.m_audioAssetGuid)
		, m_sceneTemplateIdentifier(templateComponent.m_sceneTemplateIdentifier)
	{
		CreateDataComponent<Network::Session::BoundComponent>(
			Network::Session::BoundComponent::Initializer{*this, cloner.GetParent()->GetSceneRegistry()}
		);
	}

	void CollectibleComponent::OnCreated()
	{
		SensorComponent::OnCreated();
		OnComponentDetected.Add(*this, &CollectibleComponent::Collect);

		if (m_sceneTemplateIdentifier.IsValid() && !GetRootScene().IsTemplate())
		{
			SpawnMesh();
		}
	}

	void CollectibleComponent::Collect(SensorComponent&, Optional<Entity::Component3D*> pCollectorComponent)
	{
		const Optional<SceneRules*> pSceneRules = SceneRules::Find(GetRootSceneComponent());

		if (m_scoreReward != 0 && pCollectorComponent.IsValid() && pSceneRules.IsValid())
		{
			if (Optional<Entity::SceneComponent*> pCollectorSceneComponent = pCollectorComponent->GetParentSceneComponent())
			{
				if (const Optional<ScoreModule*> pScoreModule = pSceneRules->FindDataComponentOfType<ScoreModule>())
				{
					if (pSceneRules->IsHost())
					{
						if (const Optional<Player*> pPlayerDataComponent = pCollectorSceneComponent->FindDataComponentOfType<Player>())
						{
							pScoreModule->AddScore(*pSceneRules, pPlayerDataComponent->GetClientIdentifier(), m_scoreReward);
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
						->BroadcastToAllClients<&CollectibleComponent::ClientOnCollectedOnHost>(*this, GetSceneRegistry(), Network::Channel{0});
				}
			}
			else
			{
				Assert(!m_revertLocalCollectJobHandle.IsValid());

				Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
				m_revertLocalCollectJobHandle = System::Get<Threading::JobManager>().ScheduleAsync(
					500_milliseconds,
					[softComponentReference = Entity::ComponentSoftReference{*this, sceneRegistry}, &sceneRegistry](Threading::JobRunnerThread&)
					{
						if (const Optional<CollectibleComponent*> pCollectible = softComponentReference.Find<CollectibleComponent>(sceneRegistry))
						{
							if (!pCollectible->m_collectedOnHost)
							{
								pCollectible->GetOwner().EnableWithChildren();
							}
						}
					},
					Threading::JobPriority::LowPriorityAsyncNetworkingRequests
				);
			}
		}

		if (GetOwner().IsEnabled())
		{
			OnCollectedInternal();
		}
	}

	void CollectibleComponent::ClientOnCollectedOnHost(Network::Session::BoundComponent&, Network::LocalClient&)
	{
		m_collectedOnHost = true;
		if (m_revertLocalCollectJobHandle.IsValid())
		{
			System::Get<Threading::JobManager>().CancelAsyncJob(m_revertLocalCollectJobHandle);
			m_revertLocalCollectJobHandle = {};
		}

		if (GetOwner().IsEnabled())
		{
			OnCollectedInternal();
		}
	}

	void CollectibleComponent::OnCollectedInternal()
	{
		GetOwner().DisableWithChildren();

		CreateDataComponent<Data::Reset>(Data::Reset::Initializer{
			Entity::Data::Component3D::DynamicInitializer{*this, GetSceneRegistry()},
			[this](Entity::Component3D&)
			{
				m_collectedOnHost = false;
				if (m_revertLocalCollectJobHandle.IsValid())
				{
					System::Get<Threading::JobManager>().CancelAsyncJob(m_revertLocalCollectJobHandle);
					m_revertLocalCollectJobHandle = {};
				}

				GetOwner().EnableWithChildren();
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

	void CollectibleComponent::SetAudioAsset(const AudioAssetPicker asset)
	{
		if (asset.IsValid())
		{
			m_audioAssetGuid = asset.GetAssetGuid();
		}
	}

	CollectibleComponent::AudioAssetPicker CollectibleComponent::GetAudioAsset() const
	{
		return {m_audioAssetGuid, Audio::AssetFormat.assetTypeGuid};
	}

	void CollectibleComponent::SetSceneAsset(const ScenePicker asset)
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

	CollectibleComponent::ScenePicker CollectibleComponent::GetSceneAsset() const
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

	void CollectibleComponent::SpawnMesh()
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

	Entity::Component3D& CollectibleComponent::GetOwner()
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

	[[maybe_unused]] const bool wasCollectibleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<CollectibleComponent>>::Make());
	[[maybe_unused]] const bool wasCollectibleTypeRegistered = Reflection::Registry::RegisterType<CollectibleComponent>();
}
