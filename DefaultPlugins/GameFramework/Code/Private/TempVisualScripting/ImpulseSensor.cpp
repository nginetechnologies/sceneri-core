#include "TempVisualScripting/ImpulseSensor.h"
#include "Tags.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>

#include <Renderer/Assets/StaticMesh/MeshAssetType.h>

#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>
#include <PhysicsCore/Components/CharacterBase.h>

namespace ngine::GameFramework
{
	ImpulseSensor::ImpulseSensor(Initializer&& initializer)
		: SensorComponent(SensorComponent::Initializer(Forward<SensorComponent::BaseType::Initializer>(initializer), Tag::Mask()))
	{
	}

	ImpulseSensor::ImpulseSensor(const ImpulseSensor& templateComponent, const Cloner& cloner)
		: SensorComponent(templateComponent, cloner)
		, m_impulse(templateComponent.m_impulse)
		, m_sceneTemplateIdentifier(templateComponent.m_sceneTemplateIdentifier)
	{
	}

	void ImpulseSensor::OnCreated()
	{
		SensorComponent::OnCreated();
		SensorComponent::OnComponentDetected.Add(*this, &ImpulseSensor::OnComponentDetected);

		if (m_sceneTemplateIdentifier.IsValid() && !GetRootScene().IsTemplate())
		{
			SpawnMesh();
		}
	}

	void ImpulseSensor::OnComponentDetected(SensorComponent&, Optional<Entity::Component3D*> pComponent)
	{
		if (pComponent != nullptr)
		{
			if (pComponent->Implements<Physics::CharacterBase>())
			{
				Physics::CharacterBase& character = pComponent->AsExpected<Physics::CharacterBase>();
				Optional<Physics::Data::Scene*> pPhysicsScene = pComponent->GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
				Assert(pPhysicsScene.IsValid());
				if (LIKELY(pPhysicsScene.IsValid()))
				{
					Optional<Physics::Data::Body*> pPhysicsBody = pComponent->FindDataComponentOfType<Physics::Data::Body>();
					Assert(pPhysicsBody.IsValid());
					if (LIKELY(pPhysicsBody.IsValid()))
					{
						if (Optional<Math::Massf> characterMass = pPhysicsBody->GetMass(*pPhysicsScene))
						{
							character.AddImpulse(GetWorldRotation().TransformDirection(m_impulse) / characterMass->GetKilograms());
						}
						else
						{
							character.AddImpulse(GetWorldRotation().TransformDirection(m_impulse));
						}
					}
				}
			}
			else
			{
				Optional<Physics::Data::Scene*> pPhysicsScene = pComponent->GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
				Assert(pPhysicsScene.IsValid());
				if (LIKELY(pPhysicsScene.IsValid()))
				{
					Optional<Physics::Data::Body*> pPhysicsBody = pComponent->FindDataComponentOfType<Physics::Data::Body>();
					Assert(pPhysicsBody.IsValid());
					if (LIKELY(pPhysicsBody.IsValid()))
					{
						if (pPhysicsBody->GetActiveType(*pPhysicsScene) == Physics::Data::Body::Type::Dynamic)
						{
							pPhysicsBody->AddImpulse(*pPhysicsScene, GetWorldRotation().TransformDirection(m_impulse));
						}
					}
				}
			}
		}
	}

	void ImpulseSensor::SetSceneAsset(const ScenePicker asset)
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

	ImpulseSensor::ScenePicker ImpulseSensor::GetSceneAsset() const
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

	void ImpulseSensor::SpawnMesh()
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

	[[maybe_unused]] const bool wasImpulseRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ImpulseSensor>>::Make());
	[[maybe_unused]] const bool wasImpulseTypeRegistered = Reflection::Registry::RegisterType<ImpulseSensor>();
}
