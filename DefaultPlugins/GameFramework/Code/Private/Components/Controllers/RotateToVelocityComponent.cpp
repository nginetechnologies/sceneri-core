#include "Components/Controllers/RotateToVelocityComponent.h"
#include "Components/Controllers/SplineMovementComponent.h"
#include "Components/Controllers/ConstantMovementComponent.h"

#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Data/BodyComponent.h>

#include <Common/Math/Serialization/Range.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::GameFramework
{
	RotateToVelocityComponent::RotateToVelocityComponent(const RotateToVelocityComponent& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_speed(templateComponent.m_speed)
	{
		if (cloner.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(cloner.GetParent());
		}
	}

	RotateToVelocityComponent::RotateToVelocityComponent(Initializer&& initializer)
		: m_owner(initializer.GetParent())
	{
		if (initializer.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(initializer.GetParent());
		}
	}

	RotateToVelocityComponent::RotateToVelocityComponent(const Entity::Data::Component3D::Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
	{
		if (deserializer.GetParent().IsSimulationActive())
		{
			RegisterForUpdate(deserializer.GetParent());
		}
	}

	RotateToVelocityComponent::~RotateToVelocityComponent() = default;

	void RotateToVelocityComponent::OnCreated(Entity::Component3D&)
	{
	}

	void RotateToVelocityComponent::OnDestroying()
	{
		if (m_owner.IsSimulationActive())
		{
			DeregisterUpdate(m_owner);
		}
	}

	void RotateToVelocityComponent::RegisterForUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<RotateToVelocityComponent>& sceneData =
			*owner.GetSceneRegistry().FindComponentTypeData<RotateToVelocityComponent>();
		sceneData.EnableAfterPhysicsUpdate(*this);
	}

	void RotateToVelocityComponent::DeregisterUpdate(Entity::Component3D& owner)
	{
		Entity::ComponentTypeSceneData<RotateToVelocityComponent>& sceneData =
			*owner.GetSceneRegistry().FindComponentTypeData<RotateToVelocityComponent>();
		sceneData.DisableAfterPhysicsUpdate(*this);
	}

	void RotateToVelocityComponent::OnEnable(Entity::Component3D&)
	{
	}
	void RotateToVelocityComponent::OnDisable(Entity::Component3D&)
	{
	}

	void RotateToVelocityComponent::OnSimulationResumed(Entity::Component3D& owner)
	{
		RegisterForUpdate(owner);
	}
	void RotateToVelocityComponent::OnSimulationPaused(Entity::Component3D& owner)
	{
		DeregisterUpdate(owner);
	}

	void RotateToVelocityComponent::AfterPhysicsUpdate()
	{
		if (m_speed != 0.f)
		{
			Entity::Component3D& owner = m_owner;
			const FrameTime deltaTime = owner.GetCurrentFrameTime();

			const Math::Quaternionf oldRotation = owner.GetWorldRotation();
			Math::Quaternionf targetRotation{oldRotation};

			if (Entity::Component3D::DataComponentResult<SplineMovementComponent> pSplineMovementComponent = owner.FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<SplineMovementComponent>(owner.GetSceneRegistry()))
			{
				const Math::Vector3f velocityDirection = pSplineMovementComponent->GetVelocityDirection();
				if (!velocityDirection.IsZero())
				{
					targetRotation = Math::Quaternionf{velocityDirection.GetNormalized(), oldRotation.GetUpColumn()};
				}
			}
			else if (Entity::Component3D::DataComponentResult<ConstantMovementComponent> pConstantMovementComponent = owner.FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<ConstantMovementComponent>(owner.GetSceneRegistry()))
			{
				const Math::Vector3f velocity = pConstantMovementComponent->GetMovementVector();
				if (!velocity.IsZero())
				{
					targetRotation = Math::Quaternionf{velocity.GetNormalized(), oldRotation.GetUpColumn()};
				}
			}
			else if (Entity::Component3D::DataComponentResult<Physics::Data::Body> pPhysicsBody = owner.FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Physics::Data::Body>(owner.GetSceneRegistry()))
			{
				Physics::Data::Scene& physicsScene = *pPhysicsBody.m_pDataComponentOwner->GetRootSceneComponent()
				                                        .FindDataComponentOfType<Physics::Data::Scene>(owner.GetSceneRegistry());
				Physics::Data::Body& body = *pPhysicsBody.m_pDataComponent;

				if (body.GetActiveType(physicsScene) == Physics::BodyType::Dynamic)
				{
					const Math::Vector3f velocity = body.GetVelocity(physicsScene);
					if (!velocity.IsZero())
					{
						targetRotation = Math::Quaternionf{velocity.GetNormalized(), oldRotation.GetUpColumn()};
					}
				}
			}

			const Math::EulerAnglesf rotationDelta = Math::Quaternionf{oldRotation}.InverseTransformRotation(targetRotation).GetEulerAngles();
			const float alpha = Math::Min(m_speed * deltaTime, 1.f);
			const Math::Quaternionf newRotation = Math::Quaternionf{oldRotation}.TransformRotation(Math::Quaternionf{rotationDelta * alpha});

			owner.SetWorldRotation(newRotation);
		}
	}

	[[maybe_unused]] const bool wasRotateToVelocityRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<RotateToVelocityComponent>>::Make());
	[[maybe_unused]] const bool wasRotateToVelocityTypeRegistered = Reflection::Registry::RegisterType<RotateToVelocityComponent>();
}
