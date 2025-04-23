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
	struct RotatingMovementComponent final : public Entity::Data::Component3D
	{
		enum class Mode : uint8
		{
			Single,
			Loop,
			PingPong,
		};

		using BaseType = Entity::Data::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using Initializer = BaseType::DynamicInitializer;

		RotatingMovementComponent(const RotatingMovementComponent& templateComponent, const Cloner& cloner);
		RotatingMovementComponent(const Deserializer& deserializer);
		RotatingMovementComponent(Initializer&& initializer);
		virtual ~RotatingMovementComponent();

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

		friend struct Reflection::ReflectedType<RotatingMovementComponent>;

		Entity::Component3D& m_owner;
		Math::Anglef m_currentRotation = Math::Zero;

		Math::ClampedValuef m_velocity = {16.f, -4096.f, 4096.f};
		Math::Vector3f m_rotationAxis = Math::Up;
		Math::Rangef m_rotationLimit{Math::Rangef::MakeStartToEnd(0.f, 0.f)};
		Mode m_mode = Mode::Loop;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::RotatingMovementComponent::Mode>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::RotatingMovementComponent::Mode>(
			"80c4d005-980c-48fb-a145-2354d3dc86af"_guid,
			MAKE_UNICODE_LITERAL("Rotating Movement Mode"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{GameFramework::RotatingMovementComponent::Mode::Single, MAKE_UNICODE_LITERAL("Single")},
				Reflection::EnumTypeEntry{GameFramework::RotatingMovementComponent::Mode::Loop, MAKE_UNICODE_LITERAL("Loop")},
				Reflection::EnumTypeEntry{GameFramework::RotatingMovementComponent::Mode::PingPong, MAKE_UNICODE_LITERAL("Ping Pong")}
			}}
		);
	};

	template<>
	struct ReflectedType<GameFramework::RotatingMovementComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::RotatingMovementComponent>(
			"821f5c81-6446-4230-b4c9-6ea6b37c24b1"_guid,
			MAKE_UNICODE_LITERAL("Rotating Movement"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Velocity"),
					"velocity",
					"{F285F484-64F2-443A-9B7F-DD2F471C5B48}"_guid,
					MAKE_UNICODE_LITERAL("Rotating Movement"),
					&GameFramework::RotatingMovementComponent::m_velocity
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Rotation Axis"),
					"rotationAxis",
					"{A80DFE38-BD3A-44FC-86B8-AB48C3A2D9DF}"_guid,
					MAKE_UNICODE_LITERAL("Rotating Movement"),
					&GameFramework::RotatingMovementComponent::m_rotationAxis
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Rotation Limit"),
					"rotationLimit",
					"{6E733EA7-E84F-4F51-8686-0BCF8AA1183B}"_guid,
					MAKE_UNICODE_LITERAL("Rotating Movement"),
					&GameFramework::RotatingMovementComponent::m_rotationLimit
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Mode"),
					"mode",
					"{0FAF3D09-D08D-475C-93A9-DAA0B6948858}"_guid,
					MAKE_UNICODE_LITERAL("Rotating Movement"),
					&GameFramework::RotatingMovementComponent::m_mode
				)
			},
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
