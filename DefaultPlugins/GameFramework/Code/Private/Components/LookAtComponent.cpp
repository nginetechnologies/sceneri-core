#include "Components/LookAtComponent.h"
#include "Components/Controllers/SplineMovementComponent.h"

#include <Common/Reflection/Registry.inl>

#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Splines/SplineComponent.h>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>

namespace ngine::GameFramework
{
	LookAtComponent::LookAtComponent(const LookAtComponent& templateComponent, const Cloner& cloner)
		: Entity::Data::Component3D(templateComponent, cloner)
		, m_owner(cloner.GetParent())
		, m_lookAt(
				templateComponent.m_lookAt,
				Entity::ComponentSoftReference::Cloner{cloner.GetTemplateParent().GetSceneRegistry(), cloner.GetSceneRegistry()}
			)
	{
		if (cloner.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(cloner.GetParent());
		}
	}

	LookAtComponent::LookAtComponent(Initializer&& initializer)
		: Entity::Data::Component3D(initializer)
		, m_owner(initializer.GetParent())
	{
		if (initializer.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(initializer.GetParent());
		}
	}

	LookAtComponent::LookAtComponent(const Entity::Data::Component3D::Deserializer& deserializer)
		: Entity::Data::Component3D(deserializer)
		, m_owner(deserializer.GetParent())
		, m_lookAt(deserializer.m_reader.ReadWithDefaultValue<Entity::ComponentSoftReference>(
				"lookAt", Entity::ComponentSoftReference{}, deserializer.GetSceneRegistry()
			))
	{
		if (deserializer.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(deserializer.GetParent());
		}
	}

	LookAtComponent::~LookAtComponent() = default;

	void LookAtComponent::RegisterForUpdate(Entity::Component3D& owner)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (Optional<Entity::ComponentTypeSceneData<LookAtComponent>*> pSceneData = sceneRegistry.FindComponentTypeData<LookAtComponent>())
		{
			if (const Optional<Entity::Component3D*> pLookAtComponent = m_lookAt.Find<Entity::Component3D>(sceneRegistry))
			{
				pSceneData->EnableAfterPhysicsUpdate(*this);

				Entity::ComponentTypeSceneData<SplineMovementComponent>& splineMovementTypeSceneData =
					*sceneRegistry.GetOrCreateComponentTypeData<SplineMovementComponent>();

				splineMovementTypeSceneData.GetAfterPhysicsUpdateStage()
					->AddSubsequentStage(*pSceneData->GetAfterPhysicsUpdateStage(), sceneRegistry);
			}
		}
	}

	void LookAtComponent::DeregisterUpdate(Entity::Component3D& owner)
	{
		if (Optional<Entity::ComponentTypeSceneData<LookAtComponent>*> pSceneData = owner.GetSceneRegistry().FindComponentTypeData<LookAtComponent>())
		{
			pSceneData->DisableAfterPhysicsUpdate(*this);
		}
	}

	void LookAtComponent::OnDestroying()
	{
		if (m_owner.IsSimulationActive())
		{
			DeregisterUpdate(m_owner);
		}
	}

	void LookAtComponent::OnSimulationResumed(Entity::Component3D& owner)
	{
		RegisterForUpdate(owner);
	}

	void LookAtComponent::OnSimulationPaused(Entity::Component3D& owner)
	{
		DeregisterUpdate(owner);
	}

	void LookAtComponent::SetLookAtComponent(Entity::Component3D&, Entity::Component3DPicker lookAtPicker)
	{
		m_lookAt = lookAtPicker;
	}

	Entity::Component3DPicker LookAtComponent::GetLookAtComponent(Entity::Component3D& owner) const
	{
		Entity::Component3DPicker picker{m_lookAt, owner.GetSceneRegistry()};
		picker.SetAllowedComponentTypeGuids(
			Array{Reflection::GetTypeGuid<Entity::Component3D>(), Reflection::GetTypeGuid<Entity::SceneComponent>()}
		);
		return picker;
	}

	void LookAtComponent::AfterPhysicsUpdate()
	{
		const Optional<Entity::Component3D*> pLookAtComponent = m_lookAt.Find<Entity::Component3D>(m_owner.GetSceneRegistry());
		if (LIKELY(pLookAtComponent.IsValid()))
		{
			const Math::WorldCoordinate currentLocation = pLookAtComponent->GetWorldLocation();
			const Math::Quaternionf ownerRotation = m_owner.GetWorldRotation();
			if (!m_previousRotation.IsEquivalentTo(ownerRotation) || !currentLocation.IsEquivalentTo(m_previousPosition))
			{
				const Math::WorldCoordinate lookAtWorldCoordinate = pLookAtComponent->GetWorldLocation();
				const Math::WorldCoordinate ownerWorldCoordinate = m_owner.GetWorldLocation();

				const Math::WorldLine line(ownerWorldCoordinate, lookAtWorldCoordinate);
				const Math::Vector3f direction = line.GetDirection();
				const Math::Quaternionf rotation(direction, m_owner.GetWorldUpDirection());

				m_owner.SetWorldRotation(rotation);
				m_previousRotation = rotation;
				m_previousPosition = currentLocation;
			}
		}
	}

	[[maybe_unused]] const bool wasLookAtRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<LookAtComponent>>::Make());
	[[maybe_unused]] const bool wasLookAtTypeRegistered = Reflection::Registry::RegisterType<LookAtComponent>();
}
