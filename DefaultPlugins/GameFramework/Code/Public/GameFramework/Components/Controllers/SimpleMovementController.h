#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Input/Actions/SmoothedVectorAction.h>

#include <Common/Math/ClampedValue.h>
#include <Common/EnumFlags.h>

namespace ngine::GameFramework
{
	struct CameraControllerComponent;

	struct SimpleMovementController final : public Entity::Data::Component3D
	{
		enum class Flags : uint8
		{
			MovementLimitedXAxis = 1 << 0,
			MovementLimitedYAxis = 1 << 1,
			MovementLimitedZAxis = 1 << 2,
		};

		using BaseType = Entity::Data::Component3D;
		using Initializer = BaseType::DynamicInitializer;

		SimpleMovementController(const SimpleMovementController& templateComponent, const Cloner& cloner);
		SimpleMovementController(const Deserializer& deserializer);
		SimpleMovementController(Initializer&& initializer);
		virtual ~SimpleMovementController();

		void OnCreated();
		void OnDestroying(Entity::Component3D& owner);

		void RegisterForUpdate();
		void FixedPhysicsUpdate();
	protected:
		void OnInputAssigned(Input::ActionMonitor& actionMonitor);
		void OnInputDisabled(Input::ActionMonitor& actionMonitor);
	protected:
		friend struct Reflection::ReflectedType<GameFramework::SimpleMovementController>;

		Entity::Component3D& m_owner;

		Input::SmoothedVectorAction<Math::Vector3f> m_moveAction;
		Math::Vector3f m_accumulatedMovement = Math::Zero;

		EnumFlags<Flags> m_flags;

		Math::ClampedValuef m_speed = {4.f, 0.0f, 100.f};
		Math::ClampedValuef m_momentum = {1.f, 0.01f, 1000.f};

		Math::Range<float> m_movementLimitX = Math::Range<float>::MakeStartToEnd(0.f, 0.f);
		Math::Range<float> m_movementLimitY = Math::Range<float>::MakeStartToEnd(0.f, 0.f);
		Math::Range<float> m_movementLimitZ = Math::Range<float>::MakeStartToEnd(0.f, 0.f);
	};

	ENUM_FLAG_OPERATORS(SimpleMovementController::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::SimpleMovementController>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::SimpleMovementController>(
			"56e91d5a-202e-44b8-960b-cdf8d73511fb"_guid,
			MAKE_UNICODE_LITERAL("Simple Movement Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Flags"),
					"flags",
					"{79D91703-761B-49B8-AF69-7E3636D654F2}"_guid,
					MAKE_UNICODE_LITERAL("Simple Movement Controller"),
					&GameFramework::SimpleMovementController::m_flags
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Speed"),
					"speed",
					"{3D3B2117-A4E3-410D-A08F-4E571BB06C05}"_guid,
					MAKE_UNICODE_LITERAL("Simple Movement Controller"),
					&GameFramework::SimpleMovementController::m_speed
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Momentum"),
					"momentum",
					"{A84F0A54-2D56-4AEB-BA4E-2FDE486C6555}"_guid,
					MAKE_UNICODE_LITERAL("Simple Movement Controller"),
					&GameFramework::SimpleMovementController::m_momentum
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Movement Limit X"),
					"movementLimitX",
					"{CFE4D76F-F860-4CFE-B6BD-15517634728A}"_guid,
					MAKE_UNICODE_LITERAL("Simple Movement Controller"),
					&GameFramework::SimpleMovementController::m_movementLimitX
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Movement Limit Y"),
					"movementLimitY",
					"{97F5FDFA-A30F-4AF6-B0DF-67F2FB7A55F1}"_guid,
					MAKE_UNICODE_LITERAL("Simple Movement Controller"),
					&GameFramework::SimpleMovementController::m_movementLimitY
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Movement Limit Z"),
					"movementLimitZ",
					"{0CF9E9B0-799A-42D4-99BC-FB7368849438}"_guid,
					MAKE_UNICODE_LITERAL("Simple Movement Controller"),
					&GameFramework::SimpleMovementController::m_movementLimitZ
				)
			}
		);
	};
}
