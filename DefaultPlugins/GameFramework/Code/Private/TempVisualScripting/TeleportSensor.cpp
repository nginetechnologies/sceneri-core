#include "TempVisualScripting/TeleportSensor.h"
#include "Tags.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Tag/TagRegistry.h>

#include <Renderer/Assets/StaticMesh/MeshAssetType.h>

#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include <Components/Player/Player.h>

namespace ngine::GameFramework
{
	TeleportSensor::TeleportSensor(const TeleportSensor& templateComponent, const Cloner& cloner)
		: SensorComponent(templateComponent, cloner)
		, m_target(
				templateComponent.m_target,
				Entity::ComponentSoftReference::Cloner{cloner.GetTemplateSceneRegistry(), cloner.GetParent()->GetSceneRegistry()}
			)
		, m_sceneTemplateIdentifier(templateComponent.m_sceneTemplateIdentifier)
	{
	}

	TeleportSensor::TeleportSensor(Initializer&& initializer)
		: SensorComponent(SensorComponent::Initializer(
				Forward<SensorComponent::BaseType::Initializer>(initializer),
				Tag::Mask(System::Get<Tag::Registry>().FindOrRegister(Tags::PlayerTagGuid))
			))
	{
	}

	void TeleportSensor::OnCreated()
	{
		SensorComponent::OnCreated();
		OnComponentDetected.Add(*this, &TeleportSensor::Teleport);

		if (m_sceneTemplateIdentifier.IsValid() && !GetRootScene().IsTemplate())
		{
			SpawnMesh();
		}
	}

	void TeleportSensor::Teleport(SensorComponent&, Optional<Entity::Component3D*> pComponent)
	{
		if (pComponent != nullptr)
		{
			if (const Optional<Entity::Component3D*> pTarget = m_target.Find<Entity::Component3D>(pComponent->GetSceneRegistry()))
			{
				pComponent->GetParentSceneComponent()->SetWorldTransform(pTarget->GetWorldTransform());
				pComponent->SetWorldTransform(pTarget->GetWorldTransform());
			}
		}
	}

	void TeleportSensor::SetTarget(const Entity::Component3DPicker receiver)
	{
		m_target = static_cast<Entity::ComponentSoftReference>(receiver);
	}

	Entity::Component3DPicker TeleportSensor::GetTarget() const
	{
		Entity::Component3DPicker picker{m_target, GetSceneRegistry()};
		picker.SetAllowedComponentTypeGuids(Array{Reflection::GetTypeGuid<TeleportSensor>()});
		return picker;
	}

	void TeleportSensor::SetSceneAsset(const ScenePicker asset)
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

	TeleportSensor::ScenePicker TeleportSensor::GetSceneAsset() const
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

	void TeleportSensor::SpawnMesh()
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

	[[maybe_unused]] const bool wasTeleportSensorRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<TeleportSensor>>::Make());
	[[maybe_unused]] const bool wasTeleportSensorTypeRegistered = Reflection::Registry::RegisterType<TeleportSensor>();
}
