#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/ComponentPicker.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Math/Angle3.h>
#include <Common/Math/Range.h>
#include <Common/Math/ClampedValue.h>
#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework
{
	struct RotateToVelocityComponent final : public Entity::Data::Component3D
	{
		using BaseType = Entity::Data::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using Initializer = BaseType::DynamicInitializer;

		RotateToVelocityComponent(const RotateToVelocityComponent& templateComponent, const Cloner& cloner);
		RotateToVelocityComponent(const Deserializer& deserializer);
		RotateToVelocityComponent(Initializer&& initializer);
		virtual ~RotateToVelocityComponent();

		void OnCreated(Entity::Component3D& owner);
		void OnDestroying();
		void OnEnable(Entity::Component3D& owner);
		void OnDisable(Entity::Component3D& owner);
		void OnSimulationResumed(Entity::Component3D& owner);
		void OnSimulationPaused(Entity::Component3D& owner);

		void AfterPhysicsUpdate();
	protected:
		void RegisterForUpdate(Entity::Component3D& owner);
		void DeregisterUpdate(Entity::Component3D& owner);

		friend struct Reflection::ReflectedType<RotateToVelocityComponent>;

		Entity::Component3D& m_owner;
		Math::Anglef m_currentRotation = Math::Zero;

		Math::ClampedValuef m_speed = {16.f, -4096.f, 4096.f};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::RotateToVelocityComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::RotateToVelocityComponent>(
			"2518ba48-9990-40e3-9297-b6b70b59e099"_guid,
			MAKE_UNICODE_LITERAL("Rotate to Velocity"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Speed"),
				"speed",
				"{5519a1c0-0568-4cde-bbfa-068d467d2761}"_guid,
				MAKE_UNICODE_LITERAL("Rotating Movement"),
				&GameFramework::RotateToVelocityComponent::m_speed
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "bc0ced52-0a3f-d6d4-fe89-c2d733476f3d"_asset, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_guid
				},
				Entity::IndicatorTypeExtension{EnumFlags<Entity::IndicatorTypeExtension::Flags>{Entity::IndicatorTypeExtension::Flags::RequiresGhost
		    }}
			}
		);
	};
}
