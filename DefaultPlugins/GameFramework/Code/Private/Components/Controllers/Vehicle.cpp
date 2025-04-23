#include "Components/Controllers/Vehicle.h"
#include "Components/Camera/ThirdPersonCameraController.h"
#include "Tags.h"

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Physics/PhysicsSystem.h>
#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Components/Vehicles/Vehicle.h>

#include <Engine/Entity/SpringComponent.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Tag/TagRegistry.h>

#include <Engine/Entity/InputComponent.h>
#include <Engine/Input/ActionMapBinding.inl>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>

#include "Engine/Entity/ComponentType.h"
#include <Common/Reflection/Registry.inl>
#include <Common/Math/LinearInterpolate.h>
#include <Common/Math/LinearInterpolate.h>

#include <VisualDebug/Plugin.h>

namespace ngine::GameFramework
{
	VehicleController::VehicleController(const VehicleController& templateComponent, const Cloner& cloner)
		: Entity::Component3D(templateComponent, cloner)
		, m_steerAssistRatio(templateComponent.m_steerAssistRatio)
		, m_steeringResponsivness(templateComponent.m_steeringResponsivness)
		, m_steeringReleaseResponsivness(templateComponent.m_steeringReleaseResponsivness)
		, m_accelerationResponsivness(templateComponent.m_accelerationResponsivness)
		, m_accelerationReleaseResponsivness(templateComponent.m_accelerationReleaseResponsivness)
		, m_maximumPitchRollAngle(templateComponent.m_maximumPitchRollAngle)
	{
	}

	VehicleController::~VehicleController()
	{
	}

	void VehicleController::OnDestroying()
	{
		Optional<Entity::InputComponent*> pInputComponent = FindDataComponentOfType<Entity::InputComponent>();
		if (pInputComponent.IsValid())
		{
			if (Optional<Input::ActionMonitor*> pActionMonitor = pInputComponent->GetMonitor())
			{
				OnInputDisabled(*pActionMonitor);
			}

			pInputComponent->OnAssignedActionMonitor.Remove(this);
			pInputComponent->OnInputEnabled.Remove(this);
			pInputComponent->OnInputDisabled.Remove(this);
		}

		Entity::ComponentTypeSceneData<VehicleController>& sceneData =
			static_cast<Entity::ComponentTypeSceneData<VehicleController>&>(*GetTypeSceneData());
		sceneData.DisableAfterPhysicsUpdate(*this);
		sceneData.DisableFixedPhysicsUpdate(*this);
	}

	void VehicleController::OnInputDisabled(Input::ActionMonitor& actionMonitor)
	{
		// Unbind input
		Input::Manager& inputManager = System::Get<Input::Manager>();

		const Input::KeyboardDeviceType& keyboardDeviceType =
			inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		m_handbrakeAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::Space));
		m_handbrakeAction.UnbindInput(actionMonitor, gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::A));

		m_steerAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::A));
		m_steerAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::D));
		m_steerAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::W));
		m_steerAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::S));

		m_steerActionAnalog.UnbindInput(actionMonitor, gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Axis::LeftThumbstick));
		m_accelerationAction.UnbindInput(actionMonitor, gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Analog::RightTrigger));
		m_brakeAction.UnbindInput(actionMonitor, gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Analog::LeftTrigger));
	}

	void VehicleController::OnInputAssigned(Input::ActionMonitor& actionMonitor)
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();

		const Input::KeyboardDeviceType& keyboardDeviceType =
			inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());
		m_handbrakeAction.BindInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::Space));
		m_handbrakeAction.BindInput(actionMonitor, gamepadDeviceType, gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Button::A));

		m_steerAction
			.BindAxisInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::A), Math::Left);
		m_steerAction
			.BindAxisInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::D), Math::Right);
		m_steerAction
			.BindAxisInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::W), Math::Forward);
		m_steerAction
			.BindAxisInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::S), Math::Backward);

		m_steerActionAnalog
			.Bind2DAxisInput(actionMonitor, gamepadDeviceType, gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Axis::LeftThumbstick));

		m_accelerationAction
			.BindInput(actionMonitor, gamepadDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::LeftShift));
		m_accelerationAction
			.BindInput(actionMonitor, keyboardDeviceType, gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Analog::RightTrigger));

		m_brakeAction
			.BindInput(actionMonitor, gamepadDeviceType, gamepadDeviceType.GetInputIdentifier(Input::GamepadInput::Analog::LeftTrigger));
	}

	void VehicleController::OnCreated()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		m_pCameraComponent = GetParentSceneComponent()->FindFirstChildOfTypeRecursive<Entity::CameraComponent>(sceneRegistry).Get();
		m_pVehiclePhysics = FindFirstChildImplementingTypeRecursive<Physics::Vehicle>(sceneRegistry).Get();

		if (LIKELY(m_pVehiclePhysics != nullptr))
		{
			// Add player tag
			Optional<Entity::Data::Tags*> pTagComponent = m_pVehiclePhysics->FindDataComponentOfType<Entity::Data::Tags>(sceneRegistry);
			if (pTagComponent.IsInvalid())
			{
				pTagComponent = m_pVehiclePhysics->CreateDataComponent<Entity::Data::Tags>(
					sceneRegistry,
					Entity::Data::Tags::Initializer{Entity::Data::Component3D::DynamicInitializer{*m_pVehiclePhysics, sceneRegistry}}
				);
			}

			pTagComponent
				->SetTag(m_pVehiclePhysics->GetIdentifier(), sceneRegistry, System::Get<Tag::Registry>().FindOrRegister(Tags::PlayerTagGuid));
		}

		// Set up camera
		if (LIKELY(m_pCameraComponent != nullptr))
		{
			m_pCameraControllerComponent = m_pCameraComponent->FindDataComponentOfType<Camera::ThirdPerson>(sceneRegistry).Get();

			if (LIKELY(m_pCameraControllerComponent != nullptr && m_pVehiclePhysics != nullptr))
			{
				m_pCameraControllerComponent->SetTrackedComponent(*m_pVehiclePhysics);
			}
		}

		m_handbrakeAction.OnChanged.Bind(*this, &VehicleController::OnHandbrake);

		Optional<Entity::InputComponent*> pInputComponent = FindDataComponentOfType<Entity::InputComponent>(sceneRegistry);
		if (pInputComponent.IsValid())
		{
			pInputComponent->OnAssignedActionMonitor.Add(*this, &VehicleController::OnInputAssigned);
			pInputComponent->OnInputEnabled.Add(*this, &VehicleController::OnInputAssigned);
			pInputComponent->OnInputDisabled.Add(*this, &VehicleController::OnInputDisabled);
		}

		// Make sure that vehicle updates after us
		if (LIKELY(m_pVehiclePhysics != nullptr))
		{
			Entity::ComponentTypeSceneData<VehicleController>& sceneData =
				static_cast<Entity::ComponentTypeSceneData<VehicleController>&>(*GetTypeSceneData());
			sceneData.EnableFixedPhysicsUpdate(*this);
			sceneData.EnableAfterPhysicsUpdate(*this);

			Optional<Entity::ComponentTypeSceneDataInterface*> pVehiclePhysicsSceneData = m_pVehiclePhysics->GetTypeSceneData();
			Assert(pVehiclePhysicsSceneData.IsValid());
			if (LIKELY(pVehiclePhysicsSceneData.IsValid()))
			{
				pVehiclePhysicsSceneData->GetFixedPhysicsUpdateStage()->AddSubsequentStage(*sceneData.GetFixedPhysicsUpdateStage(), sceneRegistry);
			}
		}
	}

	void VehicleController::OnHandbrake(Input::DeviceIdentifier, const bool pressed)
	{
		Assert(m_pVehiclePhysics != nullptr);
		if (LIKELY(m_pVehiclePhysics != nullptr))
		{
			m_pVehiclePhysics->SetHandbrakePosition(pressed ? 1.f : 0.f);
		}
	}

	void VehicleController::UpdateInputs(const FrameTime deltaTime)
	{
		const float steeringInterpolationSpeed = m_steerAction.GetValue().x == 0.f ? m_steeringReleaseResponsivness : m_steeringResponsivness;
		m_currentSteer =
			Math::LinearInterpolate(m_currentSteer, m_steerAction.GetValue().x, Math::Min(deltaTime * steeringInterpolationSpeed, 1.f));

		const float accelerationInterpolationSpeed = m_steerAction.GetValue().y == 0.f ? m_accelerationReleaseResponsivness
		                                                                               : m_accelerationResponsivness;

		float acceleration = m_steerAction.GetValue().y;
		if (m_accelerationAction.m_value != 0.f || m_brakeAction.m_value != 0.f)
		{
			acceleration += (m_accelerationAction.m_value - m_brakeAction.m_value) * Math::SignNonZero(m_steerAction.GetValue().y);
		}

		m_currentAccelerator =
			Math::LinearInterpolate(m_currentAccelerator, acceleration, Math::Min(deltaTime * accelerationInterpolationSpeed, 1.f));

		constexpr int AnalogInputExponent = 3;

		// Analog input takes priority
		if (m_steerActionAnalog.GetValue().x != 0.f)
		{
			m_currentSteer = Math::Abs(Math::Power(m_steerActionAnalog.GetValue().x, AnalogInputExponent));
			m_currentSteer *= Math::SignNonZero(m_steerActionAnalog.GetValue().x);
		}
	}

	void VehicleController::UpdateSteering()
	{
		const Math::Vector3f velocity = m_pVehiclePhysics->GetVelocity();
		const Math::WorldQuaternion vehicleRotation = m_pVehiclePhysics->GetWorldRotation();
		const Math::Vector3f vehicleForward = vehicleRotation.GetForwardColumn();
		const Math::Vector3f vehicleRight = vehicleRotation.GetRightColumn();
		const Math::Vector3f vehicleUp = vehicleRotation.GetUpColumn();

		float minimumSteer = -1.f;
		float maximumSteer = 1.f;
		float vehicleSlip = 0.f;

		const Math::Vector3f planeVelocity = velocity - vehicleUp * vehicleUp.Dot(velocity);
		if (planeVelocity.GetLengthSquared() > 1.f)
		{
			const Math::Vector3f normalizedVelocity = planeVelocity.GetNormalized();

			const float forwardDot = normalizedVelocity.Dot(vehicleForward);
			const float slipSign = Math::SignNonZero(normalizedVelocity.Dot(vehicleRight));
			vehicleSlip = (1.f - Math::Abs(forwardDot)) * slipSign;

			const float steerLockLimit = 0.3f + 0.7f * m_steerAssistRatio;
			const float steerReleaseLimit = 0.005f;

			if (m_steerLockDirection != 0.f)
			{
				if (m_steerLockDirection > 0.f)
				{
					if (vehicleSlip < steerReleaseLimit)
					{
						m_steerLockDirection = 0.f;
					}

					minimumSteer = -vehicleSlip;
				}
				else
				{

					if (vehicleSlip > -steerReleaseLimit)
					{
						m_steerLockDirection = 0.f;
					}

					maximumSteer = -vehicleSlip;
				}
			}
			else
			{
				if (vehicleSlip >= steerLockLimit)
				{
					m_steerLockDirection = 1.f;
				}
				else if (vehicleSlip <= -steerLockLimit)
				{
					m_steerLockDirection = -1.f;
				}
			}
		}

		m_currentSteer = Math::Clamp(m_currentSteer, minimumSteer, maximumSteer);
		float steerPosition = Math::Clamp(m_currentSteer + vehicleSlip * m_steerAssistRatio, -1.f, 1.f);

		m_pVehiclePhysics->SetSteeringWheelPosition(steerPosition);
	}

	void VehicleController::UpdateAcceleration()
	{
		const Math::Vector3f velocity = m_pVehiclePhysics->GetVelocity();
		const Math::Vector3f vehicleForward = m_pVehiclePhysics->GetWorldForwardDirection();

		const float forwardInput = m_currentAccelerator;
		float acceleratorPosition = forwardInput;
		float brakePosition = 0.f;

		// Check if input is in different direction
		if (velocity.GetLengthSquared() > 1.f)
		{
			if (m_previousDirection * forwardInput < 0.0f)
			{
				// Get vehicle velocity in local space to the body of the vehicle
				float velocityForward = velocity.Dot(vehicleForward);
				if ((forwardInput > 0.0f && velocityForward < -0.1f) || (forwardInput < 0.0f && velocityForward > 0.1f))
				{
					// Brake while we've not stopped yet
					acceleratorPosition = 0.0f;
					brakePosition = Math::Abs(forwardInput);
				}
				else
				{
					m_previousDirection = forwardInput;
				}
			}
		}

		m_pVehiclePhysics->SetAccelerationPedalPosition(acceleratorPosition);
		m_pVehiclePhysics->SetBrakePedalPosition(brakePosition);
	}

	void VehicleController::FixedPhysicsUpdate()
	{
		if (UNLIKELY(m_pVehiclePhysics == nullptr))
		{
			return;
		}

		if (UNLIKELY(!m_pVehiclePhysics->WasCreated()))
		{
			return;
		}

		Physics::Data::Scene& physicsScene = *GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		const FrameTime deltaTime = physicsScene.GetDeltaTime();
		UpdateInputs(deltaTime);
		UpdateAcceleration();
		UpdateSteering();

		m_pVehiclePhysics->SetMaximumPitchRollAngle(m_maximumPitchRollAngle);
	}

	void VehicleController::AfterPhysicsUpdate()
	{
		if (UNLIKELY(m_pVehiclePhysics == nullptr))
		{
			return;
		}

		if (UNLIKELY(!m_pVehiclePhysics->WasCreated()))
		{
			return;
		}

		if (UNLIKELY(m_pCameraControllerComponent == nullptr))
		{
			return;
		}

		const Math::WorldTransform& currentWorldTransform = m_pVehiclePhysics->GetWorldTransform();
		m_pCameraControllerComponent->SetVerticalLocation(currentWorldTransform.GetLocation().z);
	}

	[[maybe_unused]] const bool wasVehicleRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<VehicleController>>::Make());
	[[maybe_unused]] const bool wasVehicleTypeRegistered = Reflection::Registry::RegisterType<VehicleController>();
}
