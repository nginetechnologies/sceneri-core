#pragma once

#include <Engine/Entity/Component3D.h>

#include <PhysicsCore/ConstraintIdentifier.h>
#include <PhysicsCore/3rdparty/jolt/Physics/Body/BodyID.h>

namespace ngine::Physics
{
	namespace Data
	{
		struct Body;
	}

	// TODO: Set up with component picker, and expose to add components

	struct HandleComponent : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "ceda41d9-514c-438e-9913-eff1edf49a1c"_guid;

		using BaseType = Entity::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		HandleComponent(const HandleComponent& templateComponent, const Cloner& cloner);
		HandleComponent(const Deserializer& deserializer);
		HandleComponent(Initializer&& initializer);

		// Component3D
		virtual void OnChildAttached(
			HierarchyComponentBase& newChildComponent, const ChildIndex index, const Optional<ChildIndex> preferredChildIndex
		) override final;
		virtual void OnChildDetached(HierarchyComponentBase& childComponent) override final;
		// ~Component3D

		void GrabBody(Data::Body& otherBody, const Entity::Component3D& otherBodyComponent);
		void ReleaseBody(const Data::Body& body);
	protected:
		friend struct Reflection::ReflectedType<Physics::HandleComponent>;
		JPH::BodyID m_bodyID;

		struct Constraint
		{
			ConstraintIdentifier m_identifier;
			JPH::BodyID m_grabbedBodyIdentifier;
		};

		Vector<Constraint, uint16> m_constraints;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::HandleComponent>
	{
		inline static constexpr auto Type =
			Reflection::Reflect<Physics::HandleComponent>(Physics::HandleComponent::TypeGuid, MAKE_UNICODE_LITERAL("Physics Handle"));
	};
}
