#include "Reset/ResetSystem.h"
#include "Reset/ResetComponent.h"
#include "Reset/SimulationComponent.h"
#include "Reset/TransformResetComponent.h"

#include <Engine/Engine.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/ComponentMask.h>
#include <Engine/Entity/Data/WorldTransform.h>
#include <Engine/Entity/Data/LocalTransform3D.h>
#include <Engine/Entity/Data/ParentComponent.h>
#include <Engine/Entity/Data/TypeIndex.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Component3D.inl>

#include <PhysicsCore/Components/Data/BodyComponent.h>
#include <PhysicsCore/Components/ColliderComponent.h>
#include <PhysicsCore/Components/Vehicles/Vehicle.h>
#include <PhysicsCore/Components/Vehicles/Axle.h>
#include <PhysicsCore/Components/Vehicles/Wheel.h>

#include <Common/Reflection/Registry.inl>
#include <Common/System/Query.h>
#include <Common/Threading/Jobs/AsyncJob.h>

namespace ngine::GameFramework
{
	ResetSystem::ResetSystem(Scene& scene)
		: m_scene(scene)
	{
	}

	void ResetSystem::Capture()
	{
		Entity::SceneRegistry& sceneRegistry = m_scene.GetEntitySceneRegistry();
		if (const Optional<Entity::ComponentTypeSceneData<Physics::Data::Body>*> pBodySceneData = sceneRegistry.FindComponentTypeData<Physics::Data::Body>())
		{
			Entity::ComponentTypeSceneData<Data::CachedWorldTransform>& cachedWorldTransformSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::CachedWorldTransform>();
			Entity::ComponentTypeSceneData<Data::CachedOwner>& cachedOwnerSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Data::CachedOwner>();

			Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData =
				*sceneRegistry.GetOrCreateComponentTypeData<Entity::Data::WorldTransform>();

			m_storedComponents.ClearAll();

			for (Physics::Data::Body& bodyComponent : pBodySceneData->GetAllocatedView())
			{
				if (pBodySceneData->IsComponentActive(bodyComponent) && bodyComponent.GetType() != Physics::BodyType::Static)
				{
					const Entity::ComponentIdentifier componentIdentifier = pBodySceneData->GetSparseComponentIdentifier(bodyComponent);
					m_storedComponents.Set(componentIdentifier);

					Entity::Component3D& bodyOwner = bodyComponent.GetOwner();
					cachedOwnerSceneData.CreateInstance(
						componentIdentifier,
						Invalid,
						Data::CachedOwner::Initializer{Entity::Data::Component3D::Initializer{}, bodyOwner}
					);

					bodyComponent.VisitColliders(
						[&storedComponents = m_storedComponents, &cachedOwnerSceneData](Physics::ColliderComponent& colliderComponent)
						{
							const Entity::ComponentIdentifier componentIdentifier = colliderComponent.GetIdentifier();
							storedComponents.Set(componentIdentifier);

							cachedOwnerSceneData.CreateInstance(
								componentIdentifier,
								Invalid,
								Data::CachedOwner::Initializer{Entity::Data::Component3D::Initializer{}, colliderComponent}
							);
						}
					);

					if (const Optional<Physics::Vehicle*> pVehicle = bodyOwner.As<Physics::Vehicle>(sceneRegistry))
					{
						RegisterSimulatedComponent(*pVehicle);

						bodyOwner.IterateChildrenOfType<Physics::Axle>(
							sceneRegistry,
							[this, &sceneRegistry, &cachedWorldTransformSceneData, &cachedOwnerSceneData, &vehicle = *pVehicle](Physics::Axle& axle)
							{
								const Entity::ComponentIdentifier componentIdentifier = axle.GetIdentifier();
								m_storedComponents.Set(componentIdentifier);

								cachedOwnerSceneData.CreateInstance(
									componentIdentifier,
									Invalid,
									Data::CachedOwner::Initializer{Entity::Data::Component3D::Initializer{}, axle}
								);

								axle.IterateChildrenOfType<Physics::Wheel>(
									sceneRegistry,
									[this, &cachedWorldTransformSceneData, &cachedOwnerSceneData, &vehicle](Physics::Wheel& wheel)
									{
										const Entity::ComponentIdentifier componentIdentifier = wheel.GetIdentifier();
										m_storedComponents.Set(componentIdentifier);

										const Math::WorldTransform defaultWheelWorldTransform = vehicle.GetDefaultWheelTransform(wheel.GetIndex());
										cachedWorldTransformSceneData.CreateInstance(
											componentIdentifier,
											Invalid,
											Data::CachedWorldTransform::Initializer{Entity::Data::Component3D::Initializer{}, defaultWheelWorldTransform}
										);

										cachedOwnerSceneData.CreateInstance(
											componentIdentifier,
											Invalid,
											Data::CachedOwner::Initializer{Entity::Data::Component3D::Initializer{}, wheel}
										);

										return Memory::CallbackResult::Continue;
									}
								);
								return Memory::CallbackResult::Continue;
							}
						);
					}
				}
			}

			for (Entity::ComponentSoftReference& simulatedComponentReference : m_simulatedComponents)
			{
				if (const Optional<Entity::Component3D*> pSimulatedComponent = simulatedComponentReference.Find<Entity::Component3D>(sceneRegistry))
				{
					const Entity::ComponentIdentifier componentIdentifier = pSimulatedComponent->GetIdentifier();
					if (!m_storedComponents.IsSet(componentIdentifier))
					{
						m_storedComponents.Set(componentIdentifier);

						cachedOwnerSceneData.CreateInstance(
							componentIdentifier,
							Invalid,
							Data::CachedOwner::Initializer{Entity::Data::Component3D::Initializer{}, *pSimulatedComponent}
						);
					}
				}
			}

			for (const Entity::ComponentIdentifier::IndexType componentIdentifierIndex : m_storedComponents.GetSetBitsIterator())
			{
				const Entity::ComponentIdentifier componentIdentifier = Entity::ComponentIdentifier::MakeFromValidIndex(componentIdentifierIndex);
				const Math::WorldTransform componentWorldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(componentIdentifier
				);
				cachedWorldTransformSceneData.CreateInstance(
					componentIdentifier,
					Invalid,
					Data::CachedWorldTransform::Initializer{Entity::Data::Component3D::Initializer{}, componentWorldTransform}
				);
			}
		}
	}

	void ResetSystem::ResumeSimulation()
	{
		if (m_isSimulationActive)
		{
			return;
		}

		m_isSimulationActive = true;
		Entity::SceneRegistry& sceneRegistry = m_scene.GetEntitySceneRegistry();
		for (Entity::ComponentSoftReference& simulatedComponentReference : m_simulatedComponents)
		{
			if (const Optional<Entity::Component3D*> pSimulatedComponent = simulatedComponentReference.Find<Entity::Component3D>(sceneRegistry))
			{
				bool isAutostart = true;
				if (Optional<Data::Simulation*> pSimulation = pSimulatedComponent->FindFirstDataComponentImplementingType<Data::Simulation>())
				{
					isAutostart = pSimulation->IsAutostart();
				}
				if (isAutostart)
				{
					pSimulatedComponent->ResumeSimulation(sceneRegistry);
				}
			}
		}
	}

	void ResetSystem::PauseSimulation()
	{
		if (!m_isSimulationActive)
		{
			return;
		}

		m_isSimulationActive = false;
		Entity::SceneRegistry& sceneRegistry = m_scene.GetEntitySceneRegistry();
		for (Entity::ComponentSoftReference& simulatedComponentReference : m_simulatedComponents)
		{
			if (const Optional<Entity::Component3D*> pSimulatedComponent = simulatedComponentReference.Find<Entity::Component3D>(sceneRegistry))
			{
				pSimulatedComponent->PauseSimulation(sceneRegistry);
			}
		}
	}

	void ResetSystem::RegisterSimulatedComponent(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = m_scene.GetEntitySceneRegistry();
		if (m_isSimulationActive)
		{
			component.ResumeSimulation(sceneRegistry);
		}
		else
		{
			component.PauseSimulation(sceneRegistry);
		}

		m_simulatedComponents.EmplaceBack(component, sceneRegistry);
	}
	void ResetSystem::DeregisterSimulatedComponent(Entity::Component3D& component)
	{
		m_simulatedComponents.Remove(m_simulatedComponents.Find(Entity::ComponentSoftReference{component, m_scene.GetEntitySceneRegistry()}));
	}

	void ResetSystem::Reset()
	{
		if (Optional<Entity::ComponentTypeSceneData<Data::Reset>*> pGameFrameworkReset = m_scene.GetEntitySceneRegistry().FindComponentTypeData<Data::Reset>())
		{
			for (Data::Reset& resetComponent : pGameFrameworkReset->GetAllocatedView())
			{
				if (pGameFrameworkReset->IsComponentActive(resetComponent))
				{
					// TODO: Make this more specific. Current use case is Disable. Should have a Disable / Enable reset component
					resetComponent.TriggerReset();
				}
			}
		}

		System::Get<Engine>().OnBeforeStartFrame.Add(
			*this,
			[](ResetSystem& resetSystem)
			{
				resetSystem.ResetTransforms();
				resetSystem.Clear();
			}
		);
	}

	void ResetSystem::ResetTransforms()
	{
		using CachedWorldTransformSceneData = Entity::ComponentTypeSceneData<Data::CachedWorldTransform>;
		using CachedOwnerSceneData = Entity::ComponentTypeSceneData<Data::CachedOwner>;

		Entity::SceneRegistry& sceneRegistry = m_scene.GetEntitySceneRegistry();
		if (Optional<CachedWorldTransformSceneData*> pCachedWorldTransformSceneData = sceneRegistry.FindComponentTypeData<Data::CachedWorldTransform>())
		{
			// Restore world transforms
			Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& worldTransformSceneData =
				*sceneRegistry.FindComponentTypeData<Entity::Data::WorldTransform>();
			Entity::ComponentTypeSceneData<Entity::Data::LocalTransform3D>& localTransformSceneData =
				*sceneRegistry.FindComponentTypeData<Entity::Data::LocalTransform3D>();
			Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = *sceneRegistry.FindComponentTypeData<Entity::Data::Flags>();
			Entity::ComponentTypeSceneData<Entity::Data::Parent>& parentSceneData = *sceneRegistry.FindComponentTypeData<Entity::Data::Parent>();
			CachedOwnerSceneData& cachedOwnerSceneData = *sceneRegistry.FindComponentTypeData<Data::CachedOwner>();

			// Clear now invalid components
			const Entity::ComponentTypeIdentifier worldTransformTypeIdentifier = worldTransformSceneData.GetIdentifier();
			const Entity::ComponentTypeIdentifier cachedOwnerTypeIdentifier = cachedOwnerSceneData.GetIdentifier();
			for (const Entity::ComponentIdentifier::IndexType componentIdentifierIndex : m_storedComponents.GetSetBitsIterator())
			{
				const Entity::ComponentIdentifier componentIdentifier = Entity::ComponentIdentifier::MakeFromValidIndex(componentIdentifierIndex);
				if (!sceneRegistry.HasDataComponentOfType(componentIdentifier, worldTransformTypeIdentifier) || !sceneRegistry.HasDataComponentOfType(componentIdentifier, cachedOwnerTypeIdentifier))
				{
					m_storedComponents.Clear(componentIdentifier);
				}
			}

			// Restore world transform
			for (const Entity::ComponentIdentifier::IndexType componentIdentifierIndex : m_storedComponents.GetSetBitsIterator())
			{
				const Entity::ComponentIdentifier componentIdentifier = Entity::ComponentIdentifier::MakeFromValidIndex(componentIdentifierIndex);

				const Data::CachedWorldTransform& __restrict cachedWorldTransform =
					pCachedWorldTransformSceneData->GetComponentImplementationUnchecked(componentIdentifier);
				Entity::Data::WorldTransform& __restrict worldTransform =
					worldTransformSceneData.GetComponentImplementationUnchecked(componentIdentifier);
				worldTransform = cachedWorldTransform.GetTransform();
			}

			// Update local transform
			for (const Entity::ComponentIdentifier::IndexType componentIdentifierIndex : m_storedComponents.GetSetBitsIterator())
			{
				const Entity::ComponentIdentifier componentIdentifier = Entity::ComponentIdentifier::MakeFromValidIndex(componentIdentifierIndex);

				Entity::Data::Parent& __restrict parent = parentSceneData.GetComponentImplementationUnchecked(componentIdentifier);
				const Entity::ComponentIdentifier parentIdentifier = Entity::ComponentIdentifier::MakeFromValidIndex(parent.Get());

				const Math::WorldTransform worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(componentIdentifier);
				const Math::WorldTransform parentWorldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(parentIdentifier);
				const Math::LocalTransform newLocalTransform = parentWorldTransform.GetTransformRelativeToAsLocal(worldTransform);

				Entity::Data::LocalTransform3D& __restrict localTransform =
					localTransformSceneData.GetComponentImplementationUnchecked(componentIdentifier);
				localTransform = newLocalTransform;
			}

			for (const Entity::ComponentIdentifier::IndexType componentIdentifierIndex : m_storedComponents.GetSetBitsIterator())
			{
				const Entity::ComponentIdentifier componentIdentifier = Entity::ComponentIdentifier::MakeFromValidIndex(componentIdentifierIndex);
				const Data::CachedOwner& __restrict cachedOwner = cachedOwnerSceneData.GetComponentImplementationUnchecked(componentIdentifier);
				const Math::WorldTransform worldTransform = worldTransformSceneData.GetComponentImplementationUnchecked(componentIdentifier);

				// TODO: Update all children based on parent transform
				// TODO: Need some way of accessing children without component pointer in order to do it data driven
				Entity::Component3D& component = cachedOwner.Get();
				component.OnWorldTransformChanged(Entity::TransformChangeFlags::ChangedByTransformReset);
				component.OnWorldTransformChangedEvent(Entity::TransformChangeFlags::ChangedByTransformReset);

				for (Entity::Component3D& child : component.GetChildren())
				{
					child.OnParentWorldTransformChanged(
						worldTransform,
						worldTransformSceneData,
						localTransformSceneData,
						flagsSceneData,
						Entity::TransformChangeFlags::ChangedByTransformReset
					);
				}

				Assert(component.IsRegisteredInTree());
				if (LIKELY(component.IsRegisteredInTree()))
				{
					component.GetRootSceneComponent().OnComponentWorldLocationOrBoundsChanged(component, sceneRegistry);
				}
			}

			// TODO: Need to notify the render item component (make render item a data component?)
		}
	}

	template<typename DataComponentType>
	void DestroyAllDataComponents(Scene& scene)
	{
		Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		Optional<Entity::ComponentTypeSceneData<DataComponentType>*> pSceneData = sceneRegistry.FindComponentTypeData<DataComponentType>();

		if (LIKELY(pSceneData.IsValid()))
		{
			Entity::ComponentMask componentMask;
			for (DataComponentType& component : pSceneData->GetAllocatedView())
			{
				if (pSceneData->IsComponentActive(component))
				{
					const Entity::ComponentIdentifier componentIdentifier = pSceneData->GetSparseComponentIdentifier(component);
					componentMask.Set(componentIdentifier);
				}
			}

			const Entity::ComponentTypeIdentifier componentTypeIdentifier = pSceneData->GetIdentifier();

			// First iterate over components to see which ones we are able to start removing
			for (const Entity::ComponentIdentifier::IndexType componentIndex :
			     componentMask.GetSetBitsIterator(0, sceneRegistry.GetMaximumUsedElementCount()))
			{
				const Entity::ComponentIdentifier componentIdentifier = Entity::ComponentIdentifier::MakeFromValidIndex(componentIndex);
				const bool wasRemoved = sceneRegistry.OnDataComponentRemoved(componentIdentifier, componentTypeIdentifier);
				// Remove component from mask if deletion failed (indicates the component was already deleted)
				if (!wasRemoved)
				{
					componentMask.Clear(componentIdentifier);
				}
			}

			// Now destroy the instances
			for (const typename Entity::ComponentIdentifier::IndexType componentIndex :
			     componentMask.GetSetBitsIterator(0, sceneRegistry.GetMaximumUsedElementCount()))
			{
				const Entity::ComponentIdentifier componentIdentifier = Entity::ComponentIdentifier::MakeFromValidIndex(componentIndex);

				pSceneData->OnBeforeRemoveInstance(pSceneData->GetDataComponentUnsafe(componentIdentifier), Invalid);
				pSceneData->RemoveInstance(pSceneData->GetDataComponentUnsafe(componentIdentifier), Invalid);
			}
		}
	}

	void ResetSystem::Clear()
	{
		DestroyAllDataComponents<Data::CachedWorldTransform>(m_scene);
		DestroyAllDataComponents<Data::CachedOwner>(m_scene);

		DestroyAllDataComponents<Data::Reset>(m_scene);
	}

	[[maybe_unused]] const bool wasResetRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::Reset>>::Make()
	);
	[[maybe_unused]] const bool wasResetTypeRegistered = Reflection::Registry::RegisterType<Data::Reset>();

	[[maybe_unused]] const bool wasSimulationRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::Simulation>>::Make());
	[[maybe_unused]] const bool wasSimulationTypeRegistered = Reflection::Registry::RegisterType<Data::Simulation>();

	[[maybe_unused]] const bool wasCachedWorldTransformRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::CachedWorldTransform>>::Make());
	[[maybe_unused]] const bool wasCachedWorldTransformTypeRegistered = Reflection::Registry::RegisterType<Data::CachedWorldTransform>();

	[[maybe_unused]] const bool wasCachedOwnerRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::CachedOwner>>::Make());
	[[maybe_unused]] const bool wasCachedOwnerTypeRegistered = Reflection::Registry::RegisterType<Data::CachedOwner>();
}
