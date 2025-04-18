#include "PhysicsCore/Components/Data/BodyComponent.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"
#include "PhysicsCore/Components/Data/BodyComponentFunctions.h"

#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/RootSceneComponent.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Physics
{
	// TODO: Expose Change type (static, kinematic etc)
	// - Requires enums

	Optional<Physics::Data::Body*> FindNestedBody(Entity::Component3D& component, Entity::SceneRegistry& sceneRegistry)
	{
		if (const Optional<Physics::Data::Body*> pBody = component.FindDataComponentOfType<Physics::Data::Body>(sceneRegistry))
		{
			return pBody;
		}

		if (!component.IsScene(sceneRegistry))
		{
			for (Entity::Component3D& child : component.GetChildren())
			{
				if (const Optional<Physics::Data::Body*> pBody = FindNestedBody(child, sceneRegistry))
				{
					return pBody;
				}
			}
		}

		return Invalid;
	}

	[[nodiscard]] Optional<Physics::Data::Body*> FindPhysicsBody(Entity::Component3D& component, Entity::SceneRegistry& sceneRegistry)
	{
		if (component.GetFlags(sceneRegistry).IsSet(Entity::ComponentFlags::IsMeshScene))
		{
			return component.FindFirstDataComponentOfTypeInSelfAndChildrenRecursive<Physics::Data::Body>(sceneRegistry);
		}
		else if (component.IsScene(sceneRegistry))
		{
			if (const Optional<Physics::Data::Body*> pBody = component.FindDataComponentOfType<Physics::Data::Body>(sceneRegistry))
			{
				return pBody;
			}
			for (Entity::Component3D& child : component.GetChildren())
			{
				if (const Optional<Physics::Data::Body*> pBody = FindNestedBody(child, sceneRegistry))
				{
					return pBody;
				}
			}
			return Invalid;
		}
		else
		{
			return component.FindDataComponentOfType<Physics::Data::Body>(sceneRegistry);
		}
	}

	void AddForce(Entity::Component3D& component, const Math::Vector3f force)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			pBody->AddForce(physicsScene, force);
		}
	}

	void AddForceAtLocation(Entity::Component3D& component, const Math::Vector3f force, const Math::WorldCoordinate location)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			pBody->AddForceAtLocation(physicsScene, force, location);
		}
	}

	void AddImpulse(Entity::Component3D& component, const Math::Vector3f force)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			pBody->AddImpulse(physicsScene, force);
		}
	}

	void AddImpulseAtLocation(Entity::Component3D& component, const Math::Vector3f force, const Math::WorldCoordinate location)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			pBody->AddImpulseAtLocation(physicsScene, force, location);
		}
	}

	void AddTorque(Entity::Component3D& component, const Math::Vector3f torque)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			pBody->AddTorque(physicsScene, torque);
		}
	}

	void AddAngularImpulse(Entity::Component3D& component, const Math::Vector3f angularVelocity)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			pBody->AddAngularImpulse(physicsScene, angularVelocity);
		}
	}

	void SetVelocity(Entity::Component3D& component, const Math::Vector3f velocity)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			pBody->SetVelocity(physicsScene, velocity);
		}
	}

	[[nodiscard]] Math::Vector3f GetVelocity(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			return pBody->GetVelocity(physicsScene);
		}
		return Math::Zero;
	}

	[[nodiscard]] Math::Vector3f GetVelocityAtPoint(Entity::Component3D& component, const Math::WorldCoordinate coordinate)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			return pBody->GetVelocityAtPoint(physicsScene, coordinate);
		}
		return Math::Zero;
	}

	void SetAngularVelocity(Entity::Component3D& component, const Math::Angle3f angularVelocity)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			pBody->SetAngularVelocity(physicsScene, angularVelocity);
		}
	}

	[[nodiscard]] Math::Angle3f GetAngularVelocity(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			return pBody->GetAngularVelocity(physicsScene);
		}
		return Math::Zero;
	}

	void Wake(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			pBody->Wake(physicsScene);
		}
	}

	void Sleep(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			pBody->Sleep(physicsScene);
		}
	}

	[[nodiscard]] bool IsAwake(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			Data::Scene& physicsScene = *component.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>(sceneRegistry);
			return pBody->IsAwake(physicsScene);
		}
		return false;
	}

	[[nodiscard]] bool IsSleeping(Entity::Component3D& component)
	{
		return !IsAwake(component);
	}

	[[nodiscard]] TypeTraits::MemberType<decltype(&Physics::Data::Body::OnContactFound)>*
	GetOnContactFoundEvent(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			return &pBody->OnContactFound;
		}
		return nullptr;
	}

	[[nodiscard]] TypeTraits::MemberType<decltype(&Physics::Data::Body::OnContactFound)>* GetOnContactLostEvent(Entity::Component3D& component
	)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Physics::Data::Body*> pBody = FindPhysicsBody(component, sceneRegistry))
		{
			return &pBody->OnContactLost;
		}
		return nullptr;
	}

	[[maybe_unused]] inline static const bool wasAddForceReflected = Reflection::Registry::RegisterGlobalFunction<&AddForce>();
	[[maybe_unused]] inline static const bool wasAddForceAtLocationReflected =
		Reflection::Registry::RegisterGlobalFunction<&AddForceAtLocation>();
	[[maybe_unused]] inline static const bool wasAddImpulseReflected = Reflection::Registry::RegisterGlobalFunction<&AddImpulse>();
	[[maybe_unused]] inline static const bool wasAddImpulseAtLocationReflected =
		Reflection::Registry::RegisterGlobalFunction<&AddImpulseAtLocation>();
	[[maybe_unused]] inline static const bool wasAddTorqueReflected = Reflection::Registry::RegisterGlobalFunction<&AddTorque>();
	[[maybe_unused]] inline static const bool wasAddAngularImpulseReflected =
		Reflection::Registry::RegisterGlobalFunction<&AddAngularImpulse>();
	[[maybe_unused]] inline static const bool wasSetVelocityReflected = Reflection::Registry::RegisterGlobalFunction<&SetVelocity>();
	[[maybe_unused]] inline static const bool wasGetVelocityReflected = Reflection::Registry::RegisterGlobalFunction<&GetVelocity>();
	[[maybe_unused]] inline static const bool wasGetVelocityAtPointReflected =
		Reflection::Registry::RegisterGlobalFunction<&GetVelocityAtPoint>();
	[[maybe_unused]] inline static const bool wasSetAngularVelocityReflected =
		Reflection::Registry::RegisterGlobalFunction<&SetAngularVelocity>();
	[[maybe_unused]] inline static const bool wasGetAngularVelocityReflected =
		Reflection::Registry::RegisterGlobalFunction<&GetAngularVelocity>();
	[[maybe_unused]] inline static const bool wasWakeReflected = Reflection::Registry::RegisterGlobalFunction<&Wake>();
	[[maybe_unused]] inline static const bool wasSleepReflected = Reflection::Registry::RegisterGlobalFunction<&Sleep>();
	[[maybe_unused]] inline static const bool IsAwakeReflected = Reflection::Registry::RegisterGlobalFunction<&IsAwake>();
	[[maybe_unused]] inline static const bool IsSleepingReflected = Reflection::Registry::RegisterGlobalFunction<&IsSleeping>();

	[[maybe_unused]] inline static const bool IsOnContactFoundEventReflected =
		Reflection::Registry::RegisterGlobalEvent<&GetOnContactFoundEvent>();
	[[maybe_unused]] inline static const bool IsOnContactLostEventReflected =
		Reflection::Registry::RegisterGlobalEvent<&GetOnContactLostEvent>();
}
