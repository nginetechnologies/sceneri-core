#include "TempVisualScripting/Receivers/DispenserReceiver.h"
#include "Reset/ResetComponent.h"
#include "Components/LifetimeComponent.h"

#include <Engine/Entity/Manager.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Data/Component.inl>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/ComponentValue.inl>
#include <Engine/Scene/Scene3DAssetType.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Entity/Data/Tags.h>

#include <Renderer/Assets/StaticMesh/MeshAssetType.h>

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework::Signal::Receivers
{
	Dispenser::Dispenser(const Dispenser& templateComponent, const Cloner& cloner)
		: Receiver(templateComponent, cloner)
	{
	}
	Dispenser::Dispenser(const Deserializer& deserializer)
		: Receiver(deserializer)
	{
	}
	Dispenser::Dispenser(Initializer&& initializer)
		: Receiver(Forward<Initializer>(initializer))
	{
	}

	void Dispenser::OnCreated(Entity::Component3D& owner)
	{
		if (owner.IsSimulationActive())
		{
			switch (m_mode)
			{
				case Mode::Latch:
					break;
				case Mode::InverseLatch:
					SpawnComponent(owner);
					break;
				case Mode::Relay:
					break;
				case Mode::InverseRelay:
					SpawnComponent(owner);
					break;
			}
		}
	}

	void Dispenser::OnSimulationResumed(Entity::Component3D& owner)
	{
		switch (m_mode)
		{
			case Mode::Latch:
				break;
			case Mode::InverseLatch:
				SpawnComponent(owner);
				break;
			case Mode::Relay:
				break;
			case Mode::InverseRelay:
				SpawnComponent(owner);
				break;
		}
	}

	void Dispenser::OnSimulationPaused(Entity::Component3D& owner)
	{
		switch (m_mode)
		{
			case Mode::Latch:
				break;
			case Mode::InverseLatch:
				DestroySpawnedComponents(owner);
				break;
			case Mode::Relay:
				break;
			case Mode::InverseRelay:
				DestroySpawnedComponents(owner);
				break;
		}
	}

	void Dispenser::Activate(Entity::Component3D& owner)
	{
		SpawnComponent(owner);
	}

	void Dispenser::Deactivate(Entity::Component3D& owner)
	{
		DestroySpawnedComponents(owner);
	}

	void Dispenser::SpawnComponent(Entity::Component3D& owner)
	{
		if (m_spawnedComponentTemplateIdentifier.IsValid())
		{
			Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();

			Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
			Threading::JobBatch jobBatch = Entity::ComponentValue<Entity::Component3D>::DeserializeAsync(
				owner,
				sceneRegistry,
				sceneTemplateCache.GetAssetGuid(m_spawnedComponentTemplateIdentifier),
				[this, &owner, &sceneRegistry](const Optional<Entity::Component3D*> pComponent, Threading::JobBatch&& jobBatch) mutable
				{
					if (pComponent.IsValid())
					{
						pComponent->DisableSaveToDisk(sceneRegistry);
						pComponent->DisableCloning(sceneRegistry);

						pComponent->CreateDataComponent<Data::Reset>(Data::Reset::Initializer{
							Entity::Data::Component3D::Initializer{},
							[](Entity::Component3D& component)
							{
								// TODO: Create a generic destroy reset component function so we can batch delete
								component.Destroy(component.GetSceneRegistry());
							},
							*pComponent
						});

						if (m_spawnedComponentLifetime > 0_seconds)
						{
							pComponent->CreateDataComponent<LifetimeComponent>(LifetimeComponent::Initializer{
								Entity::Data::Component3D::DynamicInitializer{*pComponent, sceneRegistry},
								m_spawnedComponentLifetime
							});
						}

						Physics::Data::Scene& physicsScene =
							*pComponent->GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);

						if (const Entity::Component3D::DataComponentResult<Physics::Data::Body> physicsQueryResult = pComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Physics::Data::Body>(sceneRegistry))
						{
							if (!m_spawnedComponentVelocity.IsZero())
							{
								physicsQueryResult.m_pDataComponent
									->AddImpulse(physicsScene, owner.GetWorldTransform().TransformDirection(m_spawnedComponentVelocity));
							}

							physicsQueryResult.m_pDataComponent->SetType(physicsScene, Physics::BodyType::Dynamic);
						}

						if (m_spawnedComponentTags.AreAnySet())
						{
							if (const Entity::Component3D::DataComponentResult<Entity::Data::Tags> tagsQueryResult = pComponent->FindFirstDataComponentOfTypeInChildrenRecursive<Entity::Data::Tags>(sceneRegistry))
							{
								tagsQueryResult.m_pDataComponent
									->SetTags(tagsQueryResult.m_pDataComponentOwner->GetIdentifier(), sceneRegistry, m_spawnedComponentTags);
							}
						}

						switch (m_mode)
						{
							case Mode::Latch:
								break;
							case Mode::InverseLatch:
							case Mode::Relay:
							case Mode::InverseRelay:
								m_spawnedComponents.EmplaceBack(*pComponent, sceneRegistry);
								break;
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
	}

	void Dispenser::DestroySpawnedComponents(Entity::Component3D& owner)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		for (Entity::ComponentSoftReference& softReference : m_spawnedComponents)
		{
			if (const Optional<Entity::Component3D*> pComponent = softReference.Find<Entity::Component3D>(sceneRegistry))
			{
				pComponent->Destroy(sceneRegistry);
			}
		}
		m_spawnedComponents.Clear();
	}

	void Dispenser::SetSceneAsset(const ScenePicker asset)
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		const Entity::ComponentTemplateIdentifier newSceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(asset.GetAssetGuid());
		m_spawnedComponentTemplateIdentifier = newSceneTemplateIdentifier;
	}

	Dispenser::ScenePicker Dispenser::GetSceneAsset() const
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		return {
			Asset::Reference{
				m_spawnedComponentTemplateIdentifier.IsValid() ? sceneTemplateCache.GetAssetGuid(m_spawnedComponentTemplateIdentifier)
																											 : Asset::Guid{},
				Scene3DAssetType::AssetFormat.assetTypeGuid
			},
			Asset::Types{Array<Asset::TypeGuid, 1>{MeshSceneAssetType::AssetFormat.assetTypeGuid}}
		};
	}

	void Dispenser::SetSpawnedComponentTags(const Tag::ModifiableMaskProperty tags)
	{
		m_spawnedComponentTags = tags.m_mask;
	}

	Tag::ModifiableMaskProperty Dispenser::GetSpawnedComponentTags() const
	{
		return Tag::ModifiableMaskProperty{m_spawnedComponentTags, System::Get<Tag::Registry>()};
	}

	[[maybe_unused]] const bool wasDispenserReceiverRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Dispenser>>::Make());
	[[maybe_unused]] const bool wasDispenserReceiverTypeRegistered = Reflection::Registry::RegisterType<Dispenser>();
}
