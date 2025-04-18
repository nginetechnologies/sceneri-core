#include "PhysicsCore/Components/KinematicBodyComponent.h"
#include "PhysicsCore/Components/Data/SceneComponent.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include <3rdparty/jolt/Jolt.h>
#include <3rdparty/jolt/Physics/PhysicsSystem.h>

namespace ngine::Physics
{
	KinematicBodyComponent::KinematicBodyComponent(Initializer&& initializer)
		: BodyComponent(BodyComponent::Initializer{
				Component3D::Initializer(initializer),
				BodyComponent::Settings(
					Type::Kinematic,
					Layer::Dynamic,
					BodyComponent::Settings().m_maximumAngularVelocity,
					BodyComponent::Settings().m_overriddenMass,
					BodyComponent::Settings().m_gravityScale,
					BodyComponent::Settings().m_flags
				)
			})
	{
	}

	[[maybe_unused]] const bool wasKinematicBodyRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<KinematicBodyComponent>>::Make());
	[[maybe_unused]] const bool wasKinematicBodyTypeRegistered = Reflection::Registry::RegisterType<KinematicBodyComponent>();
}
