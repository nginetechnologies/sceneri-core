#pragma once

#include "BodyComponent.h"
#include <Common/Threading/AtomicBool.h>

namespace ngine::Physics
{
	struct SphereColliderComponent;
	struct ScenePhysicsPostSimulateJob;
	struct ScenePhysicsSimulateJob;
	struct HandleComponent;

	struct KinematicBodyComponent : public BodyComponent
	{
		static constexpr Guid TypeGuid = "a0a81477-424d-42ac-9228-875046a4c283"_guid;
		using BaseType = BodyComponent;
		using InstanceIdentifier = TIdentifier<uint32, 10>;

		using BodyComponent::BodyComponent;
		KinematicBodyComponent(Initializer&& initializer);
	private:
		friend ScenePhysicsPostSimulateJob;
		friend ScenePhysicsSimulateJob;
		friend HandleComponent;
		friend struct Reflection::ReflectedType<Physics::KinematicBodyComponent>;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::KinematicBodyComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::KinematicBodyComponent>(
			Physics::KinematicBodyComponent::TypeGuid, MAKE_UNICODE_LITERAL("Kinematic Physics Body")
		);
	};
}
