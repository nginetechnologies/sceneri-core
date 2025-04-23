#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/ComponentPicker.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Asset/Picker.h>
#include <Common/Math/Vector3.h>
#include <Common/Math/ClampedValue.h>
#include <Common/Reflection/EnumTypeExtension.h>

#include <Common/Reflection/CoreTypes.h>
#include <Common/Reflection/Event.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework
{
	struct SplineMovementComponent final : public Entity::Data::Component3D
	{
		using BaseType = Entity::Data::Component3D;
		using Initializer = BaseType::DynamicInitializer;

		enum class Mode : uint8
		{
			Single,
			Loop,
			PingPong,
		};

		SplineMovementComponent(const SplineMovementComponent& templateComponent, const Cloner& cloner);
		SplineMovementComponent(const Deserializer& deserializer);
		SplineMovementComponent(Initializer&& initializer);
		virtual ~SplineMovementComponent();

		void OnCreated(Entity::Component3D& owner);
		void OnDestroying();
		void OnEnable(Entity::Component3D& owner);
		void OnDisable(Entity::Component3D& owner);
		void OnSimulationResumed(Entity::Component3D& owner);
		void OnSimulationPaused(Entity::Component3D& owner);

		void SetSplineFromReference(Entity::Component3D& owner, const Entity::ComponentSoftReference spline);
		void SetSpline(Entity::Component3D& owner, const Entity::Component3DPicker spline);
		Entity::Component3DPicker GetSpline(Entity::Component3D& owner) const;
		void AfterPhysicsUpdate();

		[[nodiscard]] Math::Vector3f GetVelocityDirection() const
		{
			return m_velocityDirection;
		}
	protected:
		void InitializeSpline(Entity::Component3D& owner, Entity::Component3D& splineComponent);
		void RegisterForUpdate(Entity::Component3D& owner);
		void DeregisterUpdate(Entity::Component3D& owner);
		void StopPhysicsBody(Entity::Component3D& owner);
	protected:
		friend struct Reflection::ReflectedType<SplineMovementComponent>;

		Entity::Component3D& m_owner;

		// TODO: Switch this to a type safe picker that only allows picking spline components
		Entity::ComponentSoftReference m_spline;

		Math::ClampedValuef m_velocity = {1.f, -1000.f, 1000.f};
		Mode m_mode{Mode::PingPong};
		int8 m_travelDirection{1};
		int32 m_currentIndex{0};
		Math::Ratiof m_currentSegmentRatio{0_percent};
		Math::Vector3f m_initialLocationRelativeToSpline{Math::Zero};
		Math::Vector3f m_velocityDirection{Math::Zero};
		bool m_overrideRelativePosition = false;
		bool m_applyRotation = false;

		// TODO: Expose into a property?
		static constexpr uint32 BezierSubdivisions = 32;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::SplineMovementComponent::Mode>
	{
		static constexpr auto Type = Reflection::Reflect<GameFramework::SplineMovementComponent::Mode>(
			"0c0112ab-f0ae-4bde-88c6-016a91afddb7"_guid,
			MAKE_UNICODE_LITERAL("Spline Movement Mode"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{GameFramework::SplineMovementComponent::Mode::Single, MAKE_UNICODE_LITERAL("Single")},
				Reflection::EnumTypeEntry{GameFramework::SplineMovementComponent::Mode::Loop, MAKE_UNICODE_LITERAL("Loop")},
				Reflection::EnumTypeEntry{GameFramework::SplineMovementComponent::Mode::PingPong, MAKE_UNICODE_LITERAL("Ping Pong")}
			}}
		);
	};

	template<>
	struct ReflectedType<GameFramework::SplineMovementComponent>
	{
		static constexpr auto Type = Reflection::Reflect<GameFramework::SplineMovementComponent>(
			"a8afbfa1-eba8-4228-b269-86b8ada445ec"_guid,
			MAKE_UNICODE_LITERAL("Spline Movement Component"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Spline"),
					"spline",
					"{42DF61C7-E728-43FF-A606-7988B737F9A4}"_guid,
					MAKE_UNICODE_LITERAL("Spline Movement"),
					&GameFramework::SplineMovementComponent::SetSpline,
					&GameFramework::SplineMovementComponent::GetSpline
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Velocity"),
					"velocity",
					"{683F7634-DF63-4A4B-A15D-A36227946F0F}"_guid,
					MAKE_UNICODE_LITERAL("Spline Movement"),
					&GameFramework::SplineMovementComponent::m_velocity
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Override Position"),
					"override",
					"{D3AC2BE4-3743-4DAE-BAD6-D3BA66ABDE2D}"_guid,
					MAKE_UNICODE_LITERAL("Spline Movement"),
					&GameFramework::SplineMovementComponent::m_overrideRelativePosition
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Apply Rotation"),
					"applyRotation",
					"{277224D9-252A-4111-8299-F62B00E0F822}"_guid,
					MAKE_UNICODE_LITERAL("Spline Movement"),
					&GameFramework::SplineMovementComponent::m_applyRotation
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Mode"),
					"mode",
					"{B23777F9-7A16-40ED-A8E6-2C76D19B4D66}"_guid,
					MAKE_UNICODE_LITERAL("Spline Movement"),
					&GameFramework::SplineMovementComponent::m_mode
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
