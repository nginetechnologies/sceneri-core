#include "Components/Controllers/ConstantRotationComponent.h"

#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Common/Math/Serialization/Angle3.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	ConstantRotationComponent::ConstantRotationComponent(const ConstantRotationComponent& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_rotation(templateComponent.m_rotation)
	{
		if (cloner.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(cloner.GetParent());
		}
	}

	ConstantRotationComponent::ConstantRotationComponent(Initializer&& initializer)
		: m_owner(initializer.GetParent())
	{
		if (initializer.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(initializer.GetParent());
		}
	}

	ConstantRotationComponent::ConstantRotationComponent(const Entity::Data::Component3D::Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
		, m_rotation(*deserializer.m_reader.Read<Math::YawPitchRollf>("rotation"))
	{
		if (deserializer.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(deserializer.GetParent());
		}
	}

	ConstantRotationComponent::~ConstantRotationComponent() = default;

	void ConstantRotationComponent::OnCreated(Entity::Component3D&)
	{
	}

	void ConstantRotationComponent::OnDestroying()
	{
		if (m_owner.IsSimulationActive())
		{
			DeregisterUpdate(m_owner);
		}
	}

	void ConstantRotationComponent::RegisterForUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<ConstantRotationComponent>& sceneData =
			*owner.GetSceneRegistry().FindComponentTypeData<ConstantRotationComponent>();
		sceneData.EnableAfterPhysicsUpdate(*this);
	}

	void ConstantRotationComponent::DeregisterUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<ConstantRotationComponent>& sceneData =
			*owner.GetSceneRegistry().FindComponentTypeData<ConstantRotationComponent>();
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	void ConstantRotationComponent::OnEnable(Entity::Component3D&)
	{
	}
	void ConstantRotationComponent::OnDisable(Entity::Component3D&)
	{
	}

	void ConstantRotationComponent::OnSimulationResumed(Entity::Component3D& owner)
	{
		RegisterForUpdate(owner);
	}
	void ConstantRotationComponent::OnSimulationPaused(Entity::Component3D& owner)
	{
		DeregisterUpdate(owner);
	}

	void ConstantRotationComponent::AfterPhysicsUpdate()
	{
		const FrameTime deltaTime = m_owner.GetCurrentFrameTime();

		const Math::Quaternionf oldRotation = m_owner.GetWorldRotation();
		const Math::YawPitchRollf deltaRotation = Math::YawPitchRollf((m_rotation * deltaTime).zxy());
		const Math::Quaternionf finalRotation = Math::Quaternionf(deltaRotation);

		Math::Quaternionf newRotation = oldRotation.TransformRotation(finalRotation);
		newRotation.m_vector.Normalize();
		m_owner.SetWorldRotation(newRotation);
	}

	[[maybe_unused]] const bool wasConstantRotationComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<ConstantRotationComponent>>::Make());
	[[maybe_unused]] const bool wasConstantRotationComponentTypeRegistered = Reflection::Registry::RegisterType<ConstantRotationComponent>();
}
