#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/CameraController.h>
#include <Common/Math/LinearInterpolate.h>
#include <Common/Math/ClampedValue.h>

#include <Common/EnumFlags.h>

namespace ngine::GameFramework::Camera
{
	struct ThirdPerson : public Entity::CameraController
	{
		enum class Flags : uint8
		{
			//! Ignore orientation of tracked component
			IgnoreTrackedComponentOrientation = 1 << 0,
		};

		using BaseType = Entity::CameraController;
		using InstanceIdentifier = TIdentifier<uint32, 3>;
		using Initializer = BaseType::DynamicInitializer;

		ThirdPerson(const Deserializer& deserializer);
		ThirdPerson(const ThirdPerson& templateComponent, const Cloner& cloner);
		ThirdPerson(Initializer&& initializer);
		virtual ~ThirdPerson() = default;

		void OnDestroying(Entity::Component3D& parent);

		void SetVerticalLocation(const float location)
		{
			m_verticalLocation = location;
		}

		void SetTrackedComponent(Entity::Component3D& component);

		void SetRotationalDirection(const Math::Vector2f direction)
		{
			m_rotationalDirection = direction;
		}

		void ResetTrackedComponent()
		{
			m_pTrackedComponent = Invalid;
		}

		void AfterPhysicsUpdate();

		void SetInitialCameraLocationAndRotation();
		void UpdateCameraLocationAndRotation(float deltaTime);
	protected:
		virtual void OnBecomeActive() override;
		virtual void OnBecomeInactive() override;
	private:
		friend struct Reflection::ReflectedType<ThirdPerson>;

		void FinaliseCameraLocationAndRotation(Math::WorldCoordinate targetCameraLocation, const Math::WorldCoordinate targetLookAtLocation);

		static constexpr float RotationSpeedDivisor = 0.35f;

		Optional<Entity::Component3D*> m_pTrackedComponent;

		Entity::CameraComponent* m_pCameraComponent = nullptr;

		float m_verticalLocation{0.f};
		Math::Vector2f m_rotationalDirection = Math::Zero;
		Math::WorldCoordinate m_lastTrackedPosition = Math::Zero;
		Math::WorldCoordinate m_interpolatedTrackedTargetLocation = Math::Zero;

		Math::Vector2f m_orbitPosition{Math::Zero};
		Math::Vector3f m_cameraOffset{Math::Zero};
		Math::EulerAnglesf m_initialOrbitAngles{Math::Zero};

		Math::ClampedValuef m_rotationSpeed{2.0f, 0.01f, 100.f};
		Math::ClampedValuef m_positionSpeed{2.0f, 0.01f, 100.f};
		Math::ClampedValuef m_verticalStep{1.0f, 0.f, 100.f};
		Math::ClampedValuef m_minCatchupDistance{2.f, 0.f, 10.f};
		Math::ClampedValuef m_minPitchAngle{-60, -89, 0};
		Math::ClampedValuef m_maxPitchAngle{60, 0, 89};
		Math::ClampedValuef m_yawDeadzone{0.01f, 0.f, 1.f};
		Math::ClampedValuef m_pitchDeadzone{0.25f, 0.f, 1.f};

		EnumFlags<Flags> m_flags;
	};

	ENUM_FLAG_OPERATORS(ThirdPerson::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Camera::ThirdPerson>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Camera::ThirdPerson>(
			"{0D3920F0-883F-41FC-BF30-2079A383CB27}"_guid,
			MAKE_UNICODE_LITERAL("Third Person Camera Controller"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Rotation Speed"),
					"rotationSpeed",
					"{277DFE85-F0C8-48C3-A5F1-F2EE6D2693A5}"_guid,
					MAKE_UNICODE_LITERAL("Camera Controller"),
					&GameFramework::Camera::ThirdPerson::m_rotationSpeed
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Position Speed"),
					"positionSpeed",
					"{F874E992-9208-4728-8882-47EEBA977E4F}"_guid,
					MAKE_UNICODE_LITERAL("Camera Controller"),
					&GameFramework::Camera::ThirdPerson::m_positionSpeed
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Vertical Step"),
					"verticalStep",
					"{6D4610D8-E3C8-482C-B2F1-517519F410D8}"_guid,
					MAKE_UNICODE_LITERAL("Camera Controller"),
					&GameFramework::Camera::ThirdPerson::m_verticalStep
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Min Catchup Distance"),
					"minCatchupDistance",
					"{1648DAE6-F389-4B72-86CD-A0CC85826B9C}"_guid,
					MAKE_UNICODE_LITERAL("Camera Controller"),
					&GameFramework::Camera::ThirdPerson::m_minCatchupDistance
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Min Pitch Angle"),
					"minPitchAngle",
					"{C5F759A4-BBEC-484F-8BF0-FDE58F0B461B}"_guid,
					MAKE_UNICODE_LITERAL("Camera Controller"),
					&GameFramework::Camera::ThirdPerson::m_minPitchAngle
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Max Pitch Angle"),
					"maxPitchAngle",
					"{47780A26-11E5-467E-AD5F-565C0C72CE22}"_guid,
					MAKE_UNICODE_LITERAL("Camera Controller"),
					&GameFramework::Camera::ThirdPerson::m_maxPitchAngle
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Yaw Deadzone"),
					"yawDeadzone",
					"{EC6FEDE4-122E-4F74-9E66-D8EA59C66AAD}"_guid,
					MAKE_UNICODE_LITERAL("Camera Controller"),
					&GameFramework::Camera::ThirdPerson::m_yawDeadzone
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Pitch Deadzone"),
					"pitchDeadzone",
					"{C4B38758-A9C9-47EC-9DC1-C464AF257E40}"_guid,
					MAKE_UNICODE_LITERAL("Camera Controller"),
					&GameFramework::Camera::ThirdPerson::m_pitchDeadzone
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Flags"),
					"flags",
					"{8096DF01-D308-4194-B8A7-36F0FC18FA17}"_guid,
					MAKE_UNICODE_LITERAL("Camera Controller"),
					&GameFramework::Camera::ThirdPerson::m_flags
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
