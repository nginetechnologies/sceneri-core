#include "Components/Controllers/ConstantMovementComponent.h"

#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/HierarchyComponent.inl>

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Engine/Entity/Component3D.inl>
#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	ConstantMovementComponent::ConstantMovementComponent(const ConstantMovementComponent& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_movementVector(templateComponent.m_movementVector)
	{
		if (cloner.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(cloner.GetParent());
		}
	}

	ConstantMovementComponent::ConstantMovementComponent(Initializer&& initializer)
		: m_owner(initializer.GetParent())
		, m_movementVector(Math::Zero)
	{
		if (initializer.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(initializer.GetParent());
		}
	}

	ConstantMovementComponent::ConstantMovementComponent(const Entity::Data::Component3D::Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
		, m_movementVector(*deserializer.m_reader.Read<Math::Vector3f>("movement"))
	{
		if (deserializer.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(deserializer.GetParent());
		}
	}

	void ConstantMovementComponent::OnCreated(Entity::Component3D&)
	{
	}

	void ConstantMovementComponent::OnDestroying()
	{
		if (m_owner.IsSimulationActive())
		{
			DeregisterUpdate(m_owner);
		}
	}

	void ConstantMovementComponent::RegisterForUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<ConstantMovementComponent>& sceneData =
			*owner.GetSceneRegistry().FindComponentTypeData<ConstantMovementComponent>();
		sceneData.EnableAfterPhysicsUpdate(*this);
	}

	void ConstantMovementComponent::DeregisterUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<ConstantMovementComponent>& sceneData =
			*owner.GetSceneRegistry().FindComponentTypeData<ConstantMovementComponent>();
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	void ConstantMovementComponent::OnDisable(Entity::Component3D& owner)
	{
		DeregisterUpdate(owner);
	}

	void ConstantMovementComponent::OnSimulationResumed(Entity::Component3D& owner)
	{
		RegisterForUpdate(owner);
	}
	void ConstantMovementComponent::OnSimulationPaused(Entity::Component3D& owner)
	{
		DeregisterUpdate(owner);
	}

	void ConstantMovementComponent::AfterPhysicsUpdate()
	{
		const FrameTime deltaTime = m_owner.GetCurrentFrameTime();

		const Math::WorldCoordinate oldPosition = m_owner.GetWorldLocation();
		const Math::WorldCoordinate deltaMovement = m_movementVector * deltaTime;
		const Math::WorldCoordinate newPosition = oldPosition + deltaMovement;

		if (!newPosition.IsEquivalentTo(oldPosition))
		{
			m_owner.SetWorldLocation(newPosition);
		}
	}

	[[maybe_unused]] const bool wasConstantMovementRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ConstantMovementComponent>>::Make());
	[[maybe_unused]] const bool wasConstantMovementTypeRegistered = Reflection::Registry::RegisterType<ConstantMovementComponent>();
}
