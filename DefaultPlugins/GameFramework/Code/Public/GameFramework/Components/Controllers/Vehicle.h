#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Input/Actions/BinaryAction.h>
#include <Engine/Input/Actions/AnalogAction.h>
#include <Engine/Input/Actions/Vector3Action.h>
#include <Engine/Input/Actions/Surface/TapAction.h>
#include <Engine/Input/Actions/Surface/FloatingDialAction.h>
#include <Engine/Input/Actions/SmoothedVectorAction.h>

#include <Common/Math/ClampedValue.h>
#include <Common/Time/Duration.h>
#include <Common/EnumFlags.h>
#include <Common/Reflection/CoreTypes.h>

namespace ngine
{
	namespace Physics
	{
		struct Vehicle;
	}

	namespace Entity
	{
		struct CameraComponent;
	}
}

namespace ngine::GameFramework
{
	namespace Camera
	{
		struct ThirdPerson;
	}

	struct VehicleController final : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "024630c0-b538-4b7a-8c20-d794ff8e7919"_guid;

		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		using Component3D::Component3D;
		using Component3D::Serialize;

		VehicleController(const VehicleController& templateComponent, const Cloner& cloner);
		virtual ~VehicleController();

		void OnDestroying();
		void OnCreated();

		void FixedPhysicsUpdate();
		void AfterPhysicsUpdate();

		void OnInputAssigned(Input::ActionMonitor& actionMonitor);
		void OnInputDisabled(Input::ActionMonitor& actionMonitor);
	protected:
		void OnHandbrake(Input::DeviceIdentifier, const bool);
		void UpdateInputs(const FrameTime deltaTime);
		void UpdateSteering();
		void UpdateAcceleration();
	protected:
		friend struct Reflection::ReflectedType<GameFramework::VehicleController>;

		Physics::Vehicle* m_pVehiclePhysics = nullptr;
		Camera::ThirdPerson* m_pCameraControllerComponent = nullptr;
		Entity::CameraComponent* m_pCameraComponent = nullptr;

		Input::BinaryAction m_handbrakeAction;
		Input::Vector3Action m_steerAction;
		Input::Vector3Action m_steerActionAnalog;
		Input::AnalogAction m_accelerationAction;
		Input::AnalogAction m_brakeAction;

		float m_steerLockDirection = 0.f;
		float m_currentSteer = 0.f;
		float m_currentAccelerator = 0.f;
		float m_previousDirection = 1.f;

		Math::Ratiof m_steerAssistRatio = 1.f;
		float m_steeringResponsivness = 1.f;
		float m_steeringReleaseResponsivness = 10.f;
		float m_accelerationResponsivness = 5.f;
		float m_accelerationReleaseResponsivness = 20.f;
		Math::Anglef m_maximumPitchRollAngle = Math::PI;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::VehicleController>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::VehicleController>(
			GameFramework::VehicleController::TypeGuid,
			MAKE_UNICODE_LITERAL("Vehicle Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Steer Assist Ratio"),
					"steerAssistRatio",
					"{CAD014BD-56D7-40D0-827B-F00FD22461EC}"_guid,
					MAKE_UNICODE_LITERAL("Vehicle Controller"),
					&GameFramework::VehicleController::m_steerAssistRatio
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Steering Responsivness"),
					"steeringResponsivness",
					"{3468132F-7D98-4D12-84B5-C298CC4A5C0A}"_guid,
					MAKE_UNICODE_LITERAL("Vehicle Controller"),
					&GameFramework::VehicleController::m_steeringResponsivness
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Steering Release Responsivness"),
					"steeringReleaseResponsivness",
					"{B75047E7-8BE1-4417-B50E-3A99346CE2F4}"_guid,
					MAKE_UNICODE_LITERAL("Vehicle Controller"),
					&GameFramework::VehicleController::m_steeringReleaseResponsivness
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Acceleration Responsivness"),
					"accelerationResponsivness",
					"{00A61D49-5047-4310-B3C6-8F7A6A1BAFE0}"_guid,
					MAKE_UNICODE_LITERAL("Vehicle Controller"),
					&GameFramework::VehicleController::m_accelerationResponsivness
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Acceleration Release Responsivness"),
					"accelerationReleaseResponsivness",
					"{80D3056C-7615-4F79-864E-C33E61DC94C1}"_guid,
					MAKE_UNICODE_LITERAL("Vehicle Controller"),
					&GameFramework::VehicleController::m_accelerationReleaseResponsivness
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Maximum Pitch/Roll Angle"),
					"maxPitchRollAngle",
					"{6C91A9D9-BEAE-45FB-ACCA-00EAABF2909B}"_guid,
					MAKE_UNICODE_LITERAL("Vehicle Controller"),
					&GameFramework::VehicleController::m_maximumPitchRollAngle
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "01947304-c7d6-7a8c-7cc4-5e62e714c3fb"_asset, "a8ee45e9-2d23-4f9e-9027-ca21cf6ffaef"_guid
			}}
		);
	};
}
