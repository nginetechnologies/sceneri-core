#include "TempVisualScripting/FinishGameRulesSensor.h"
#include "Tags.h"

#include <Components/SceneRules/SceneRules.h>
#include <Components/SceneRules/Modules/FinishModule.h>
#include <Components/Player/Player.h>

#include <Renderer/Assets/StaticMesh/MeshAssetType.h>

#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Entity/Serialization/ComponentReference.h>

#include <AudioCore/AudioAssetType.h>
#include <AudioCore/Components/SoundSpotComponent.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	FinishGameRulesSensor::FinishGameRulesSensor(Initializer&& initializer)
		: SensorComponent(SensorComponent::Initializer(
				Forward<SensorComponent::BaseType::Initializer>(initializer),
				Tag::Mask(System::Get<Tag::Registry>().FindOrRegister(Tags::PlayerTagGuid))
			))
	{
	}

	FinishGameRulesSensor::FinishGameRulesSensor(const FinishGameRulesSensor& templateComponent, const Cloner& cloner)
		: SensorComponent(templateComponent, cloner)
		, m_finishResult(templateComponent.m_finishResult)
		, m_sceneTemplateIdentifier(templateComponent.m_sceneTemplateIdentifier)
	{
		if (templateComponent.GetAudioAsset().IsValid())
		{
			SetAudioAsset(templateComponent.GetAudioAsset());
		}
	}

	void FinishGameRulesSensor::OnCreated()
	{
		SensorComponent::OnCreated();
		OnComponentDetected.Add(*this, &FinishGameRulesSensor::OnPlayerReachedFinish);

		if (m_sceneTemplateIdentifier.IsValid() && !GetRootScene().IsTemplate())
		{
			SpawnMesh();
		}
	}

	void FinishGameRulesSensor::OnPlayerReachedFinish(SensorComponent&, Optional<Entity::Component3D*> pComponent)
	{
		if (pComponent != nullptr)
		{
			if (const Optional<SceneRules*> pSceneRules = SceneRules::Find(pComponent->GetRootSceneComponent());
			    pSceneRules.IsValid() && pSceneRules->IsHost())
			{
				if (const Optional<Player*> pPlayerComponent = pComponent->GetParentSceneComponent()->FindDataComponentOfType<Player>())
				{
					if (const Optional<FinishModule*> pFinishModule = pSceneRules->FindDataComponentOfType<FinishModule>();
					    pFinishModule.IsValid() && !pFinishModule->HasPlayerFinished(pPlayerComponent->GetClientIdentifier()))
					{
						pFinishModule->NotifyPlayerFinished(*pSceneRules, pPlayerComponent->GetClientIdentifier(), m_finishResult);
					}
				}
			}
		}

		if (m_pSoundSpotComponent)
		{
			m_pSoundSpotComponent->Play();
		}
	}

	void FinishGameRulesSensor::SetAudioAsset(const Asset::Picker asset)
	{
		if (asset.IsValid())
		{
			if (!m_pSoundSpotComponent)
			{
				Entity::ComponentTypeSceneData<Audio::SoundSpotComponent>& soundSpotSceneData =
					*GetSceneRegistry().GetOrCreateComponentTypeData<Audio::SoundSpotComponent>();

				m_pSoundSpotComponent = soundSpotSceneData.CreateInstance(
					Audio::SoundSpotComponent::Initializer(Entity::Component3D::Initializer{*this}, {}, asset.GetAssetGuid())
				);
			}
			else
			{
				m_pSoundSpotComponent->SetAudioAsset(asset);
			}
		}
	}

	Asset::Picker FinishGameRulesSensor::GetAudioAsset() const
	{
		if (m_pSoundSpotComponent)
		{
			return m_pSoundSpotComponent->GetAudioAsset();
		}

		return {Guid(), Audio::AssetFormat.assetTypeGuid};
	}

	void FinishGameRulesSensor::SetSceneAsset(const ScenePicker asset)
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

	FinishGameRulesSensor::ScenePicker FinishGameRulesSensor::GetSceneAsset() const
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

	void FinishGameRulesSensor::SpawnMesh()
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

	[[maybe_unused]] const bool wasFinishGameRulesRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<FinishGameRulesSensor>>::Make());
	[[maybe_unused]] const bool wasFinishGameRulesTypeRegistered = Reflection::Registry::RegisterType<FinishGameRulesSensor>();
}
