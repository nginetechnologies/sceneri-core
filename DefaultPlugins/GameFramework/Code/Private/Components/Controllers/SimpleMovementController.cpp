#include "Components/Controllers/SimpleMovementController.h"
#include "Tags.h"

#include <Engine/Entity/SpringComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Data/Tags.h>
#include <Engine/Entity/Component3D.inl>
#include <Animation/Components/SkeletonComponent.h>
#include <Animation/Animation.h>
#include <Animation/SamplingCache.h>
#include <Animation/Plugin.h>
#include <Animation/AnimationCache.h>

#include <Engine/Entity/InputComponent.h>
#include <Engine/Input/ActionMapBinding.inl>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>
#include <Engine/Input/InputManager.h>

#include <PhysicsCore/Components/Data/SceneComponent.h>

#include "Engine/Entity/ComponentType.h"
#include <Common/Reflection/Registry.inl>
#include <Common/Math/LinearInterpolate.h>
#include <Common/Math/Mod.h>
#include <Common/Math/Serialization/Range.h>

namespace ngine::GameFramework
{
	SimpleMovementController::SimpleMovementController(const SimpleMovementController& templateComponent, const Cloner& cloner)
		: m_owner(cloner.GetParent())
		, m_flags(templateComponent.m_flags)
		, m_speed(templateComponent.m_speed)
		, m_movementLimitX(templateComponent.m_movementLimitX)
		, m_movementLimitY(templateComponent.m_movementLimitY)
		, m_movementLimitZ(templateComponent.m_movementLimitZ)
	{
		RegisterForUpdate();
	}

	SimpleMovementController::SimpleMovementController(const Deserializer& deserializer)
		: m_owner(deserializer.GetParent())
	{
		RegisterForUpdate();
	}

	SimpleMovementController::SimpleMovementController(Initializer&& initializer)
		: m_owner(initializer.GetParent())
	{
		RegisterForUpdate();
	}

	SimpleMovementController::~SimpleMovementController()
	{
		Entity::ComponentTypeSceneData<SimpleMovementController>& sceneData =
			*m_owner.GetSceneRegistry().FindComponentTypeData<SimpleMovementController>();
		sceneData.DisableFixedPhysicsUpdate(*this);
	}

	void SimpleMovementController::OnCreated()
	{
		Optional<Entity::InputComponent*> pInputComponent = m_owner.FindDataComponentOfType<Entity::InputComponent>();
		if (pInputComponent.IsValid())
		{
			pInputComponent->OnAssignedActionMonitor.Add(*this, &SimpleMovementController::OnInputAssigned);
			pInputComponent->OnInputEnabled.Add(*this, &SimpleMovementController::OnInputAssigned);
			pInputComponent->OnInputDisabled.Add(*this, &SimpleMovementController::OnInputDisabled);
		}
	}

	void SimpleMovementController::OnDestroying(Entity::Component3D& owner)
	{
		Optional<Entity::InputComponent*> pInputComponent = owner.FindDataComponentOfType<Entity::InputComponent>();
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
	}

	void SimpleMovementController::OnInputDisabled(Input::ActionMonitor& actionMonitor)
	{
		// Unbind input
		Input::Manager& inputManager = System::Get<Input::Manager>();
		const Input::KeyboardDeviceType& keyboardDeviceType =
			inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());

		m_moveAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::W));
		m_moveAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::S));
		m_moveAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::A));
		m_moveAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::D));
		m_moveAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::Q));
		m_moveAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::E));
	}

	void SimpleMovementController::OnInputAssigned(Input::ActionMonitor& actionMonitor)
	{
		const Input::Manager& inputManager = System::Get<Input::Manager>();
		const Input::KeyboardDeviceType& keyboardDeviceType =
			inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());

		m_moveAction
			.BindAxisInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::W), Math::Forward);
		m_moveAction
			.BindAxisInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::S), Math::Backward);
		m_moveAction
			.BindAxisInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::A), Math::Left);
		m_moveAction
			.BindAxisInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::D), Math::Right);
		m_moveAction
			.BindAxisInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::Q), Math::Down);
		m_moveAction.BindAxisInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::E), Math::Up);
	}

	void SimpleMovementController::RegisterForUpdate()
	{
		Entity::ComponentTypeSceneData<SimpleMovementController>& sceneData =
			*m_owner.GetSceneRegistry().FindComponentTypeData<SimpleMovementController>();
		sceneData.EnableFixedPhysicsUpdate(*this);
	}

	void SimpleMovementController::FixedPhysicsUpdate()
	{
		Physics::Data::Scene& physicsScene = *m_owner.GetRootSceneComponent().FindDataComponentOfType<Physics::Data::Scene>();
		const FrameTime deltaTime = physicsScene.GetDeltaTime();
		m_moveAction.Update(deltaTime, m_momentum);
		if (m_moveAction.GetValue().IsZero())
		{
			return;
		}

		Math::Vector3f movementToApply = m_moveAction.GetValue() * m_speed * deltaTime;
		const Math::Vector3f newAccumulatedMovement = m_accumulatedMovement + movementToApply;

		const Math::Vector3f limitedMovement{
			m_movementLimitX.GetClampedValue(newAccumulatedMovement.x),
			m_movementLimitY.GetClampedValue(newAccumulatedMovement.y),
			m_movementLimitZ.GetClampedValue(newAccumulatedMovement.z)
		};
		const Math::Vector3f delta = newAccumulatedMovement - limitedMovement;
		const Math::Vector3f condition{
			m_flags.IsSet(Flags::MovementLimitedXAxis) ? 1.f : 0.f,
			m_flags.IsSet(Flags::MovementLimitedYAxis) ? 1.f : 0.f,
			m_flags.IsSet(Flags::MovementLimitedZAxis) ? 1.f : 0.f
		};
		movementToApply -= Math::Select(!condition.IsZero(), delta, Math::Zero);

		m_accumulatedMovement += movementToApply;
		m_owner.SetWorldLocation(m_owner.GetWorldLocation() + movementToApply);
	}

	[[maybe_unused]] const bool wasSimpleMovementRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SimpleMovementController>>::Make());
	[[maybe_unused]] const bool wasSimpleMovementTypeRegistered = Reflection::Registry::RegisterType<SimpleMovementController>();
}
