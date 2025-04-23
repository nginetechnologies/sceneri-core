#pragma once

#include "Common/Math/CoreNumericTypes.h"
#include <Engine/Entity/CameraController.h>
#include <Common/Math/ClampedValue.h>
#include <Engine/Input/ActionHandle.h>
#include <Common/Math/Vector2.h>

namespace ngine::Input
{
	struct ActionMonitor;
}

namespace ngine::GameFramework::Camera
{
	struct FirstPerson : public Entity::CameraController
	{
		inline static constexpr Guid TypeGuid = "BB014790-6702-495A-BD47-265420CA2F87"_guid;

		using BaseType = Entity::CameraController;
		using InstanceIdentifier = TIdentifier<uint32, 3>;
		using Initializer = BaseType::DynamicInitializer;

		enum class Flags : uint8
		{
			CameraInvertX = 1 << 0,
			CameraInvertY = 1 << 1,
		};

		explicit FirstPerson(const Deserializer& deserializer);
		FirstPerson(const FirstPerson& templateComponent, const Cloner& cloner);
		explicit FirstPerson(Initializer&& initializer);

		virtual ~FirstPerson() = default;

		void OnCreated();
		void OnParentCreated(Entity::Component3D&);
		void OnDestroying();
		void OnEnable();
		void OnDisable();
		void OnInputAssigned(Input::ActionMonitor&);
		void OnInputDisabled(Input::ActionMonitor&);

		void BeforePhysicsUpdate();
		void AfterPhysicsUpdate();

		void Shake(const Math::Vector2f& horizontalLimits, const Math::Vector2f& verticalLimits);
	private:
		friend struct Reflection::ReflectedType<FirstPerson>;

		void SetMouseSensitivity(const Math::Vector2f sensitivity);
		void SetGamepadSensitivity(const Math::Vector2f sensitivity);

		Entity::CameraComponent* m_pCameraComponent{nullptr};

		Input::ActionHandle m_moveCameraAction;
		Input::ActionHandle m_mouseLookAction;

		Math::Vector2f m_currentAngles{Math::Zero};
		Math::Vector2f m_targetAngles{Math::Zero};
		Math::Vector2f m_inputAngles{Math::Zero};
		Math::Vector2f m_mouseInput{Math::Zero};

		Math::Vector2f m_mouseSensitivity{0.01f};
		Math::Vector2f m_gamepadSensitivity{4.f, 2.f};
		EnumFlags<Flags> m_flags{Flags::CameraInvertY};

		Math::ClampedValue<float> m_minPitchAngle{-60, -89, 0};
		Math::ClampedValue<float> m_maxPitchAngle{60, 0, 89};
		Math::Vector2f m_rotationVelocity{1.0f, 1.0f};
		float m_interpolationSpeed{20.f};
	};

	ENUM_FLAG_OPERATORS(FirstPerson::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Camera::FirstPerson>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Camera::FirstPerson>(
			GameFramework::Camera::FirstPerson::TypeGuid,
			MAKE_UNICODE_LITERAL("First Person Camera Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Min Pitch Angle"),
					"minPitchAngle",
					"{E3F2CDF9-3525-401B-9379-39DA583F3FC6}"_guid,
					MAKE_UNICODE_LITERAL("First Person Camera Controller"),
					&GameFramework::Camera::FirstPerson::m_minPitchAngle
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Max Pitch Angle"),
					"maxPitchAngle",
					"{504F074F-84AF-478A-B445-720EE80B7AE3}"_guid,
					MAKE_UNICODE_LITERAL("First Person Camera Controller"),
					&GameFramework::Camera::FirstPerson::m_maxPitchAngle
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Rotation Velocity"),
					"rotationVelocity",
					"{EE65D90A-631B-4B46-A6D2-55ABCEEF8A0D}"_guid,
					MAKE_UNICODE_LITERAL("First Person Camera Controller"),
					&GameFramework::Camera::FirstPerson::m_rotationVelocity
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "5b417da8-d03a-5493-9990-4cf7954aeae9"_asset, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_guid
			}}
		);
	};
}
