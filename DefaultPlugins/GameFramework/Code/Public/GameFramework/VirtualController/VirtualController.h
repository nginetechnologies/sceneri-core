#pragma once

#include <Engine/Input/Devices/Gamepad/GamepadInput.h>

#include <Common/Asset/Guid.h>
#include <Common/Math/CoreNumericTypes.h>
#include <Common/Guid.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Optional.h>
#include <Engine/Input/Actions/Surface/TouchAction.h>

namespace ngine::Input
{
	struct ActionMonitor;
	struct GamepadDeviceType;
}
namespace ngine::Widgets
{
	struct Widget;
}

namespace ngine::GameFramework
{
	struct VirtualController
	{
		static constexpr Asset::Guid GamepadBaseWidgetAssetGuid = "638119c4-b28a-4642-9392-74f141a0570a"_asset;
		static constexpr Guid LeftModuleSlotTagGuid = "52870dfe-e23a-49f4-a005-1e50d6f1d55b"_guid;
		static constexpr Guid RightModuleSlotTagGuid = "38b95ebd-8063-4829-b2f8-0128ba448591"_guid;

		enum class GamepadModulePosition : uint8
		{
			Left,
			Right,
			RightExtension,
			Unassigned
		};

		enum class GamepadModuleType : uint16
		{
			Joystick,          // joystick
			LeftRight,         // two arrows (left right)
			Arrows,            // four arrows (left up down right)
			Button,            // one button
			ButtonAB,          // two buttons (a b)
			ButtonABXY,        // four buttons (a b x y)
			Analog,            // analog buttons (left / right)
			ExtensionButtonAB, // two buttons (a b) extending existing module
			ExtensionButtonA,  // one button (a) extending existing module
			Unassigned
		};

		struct GamepadModuleMapping
		{
			GamepadModulePosition position{GamepadModulePosition::Unassigned};
			GamepadModuleType type{GamepadModuleType::Unassigned};
			Asset::Guid assetGuid;
		};

		static constexpr Array s_GamepadModuleMappings = {
			GamepadModuleMapping{GamepadModulePosition::Left, GamepadModuleType::Joystick, "cf732505-b1d1-44bd-8cf8-b82bfa86a252"_asset},
			GamepadModuleMapping{GamepadModulePosition::Left, GamepadModuleType::LeftRight, "43fe3b35-57af-49f7-9aee-b93182accf7a"_asset},
			GamepadModuleMapping{GamepadModulePosition::Left, GamepadModuleType::Arrows, "63905376-bceb-45db-acec-c2642dec8447"_asset},
			GamepadModuleMapping{GamepadModulePosition::Right, GamepadModuleType::Joystick, "cf732505-b1d1-44bd-8cf8-b82bfa86a252"_asset},
			GamepadModuleMapping{GamepadModulePosition::Right, GamepadModuleType::Button, "a0965d73-c9ed-4bc1-9109-d96689f67aec"_asset},
			GamepadModuleMapping{GamepadModulePosition::Right, GamepadModuleType::ButtonAB, "c2aacbb4-e0f4-44a2-a274-8c3feffda6dc"_asset},
			GamepadModuleMapping{GamepadModulePosition::Right, GamepadModuleType::ButtonABXY, "4ce903fd-5586-4f16-8e87-fc0814bdf340"_asset},
			GamepadModuleMapping{GamepadModulePosition::Right, GamepadModuleType::Analog, "8d4b596d-faa9-4ed8-8a6d-9d5526ec4afb"_asset},
			GamepadModuleMapping{
				GamepadModulePosition::RightExtension, GamepadModuleType::ExtensionButtonAB, "86b7c3b2-b2c5-4ed4-84a4-84b9318426db"_asset
			},
			GamepadModuleMapping{
				GamepadModulePosition::RightExtension, GamepadModuleType::ExtensionButtonA, "d9d09289-4106-4111-94d6-8032f49bcfbd"_asset
			}
		};
	public:
		void Initialize(Widgets::Widget& parentWidget);
		void Destroy();

		void Configure(const Input::ActionMonitor& actionMonitor);

		void Show();
		void Hide();

		bool IsValid() const;
	protected:
		void OnStartTouch(
			Input::DeviceIdentifier deviceIdentifier,
			Input::FingerIdentifier fingerIdentifier,
			ScreenCoordinate screenCoordinate,
			Math::Ratiof pressureRatio,
			uint16 touchRadius
		);
		void OnMoveTouch(
			Input::DeviceIdentifier deviceIdentifier,
			Input::FingerIdentifier fingerIdentifier,
			ScreenCoordinate screenCoordinate,
			Math::Vector2i deltaCoordinate,
			Math::Ratiof pressureRatio,
			uint16 touchRadius
		);
		void OnStopTouch(
			Input::DeviceIdentifier deviceIdentifier,
			Input::FingerIdentifier fingerIdentifier,
			ScreenCoordinate screenCoordinate,
			Math::Ratiof pressureRatio,
			uint16 touchRadius
		);
		void OnCancelTouch(Input::DeviceIdentifier deviceIdentifier, Input::FingerIdentifier fingerIdentifier);
	private:
		void OnButtonDown(Input::GamepadInput::Button button);
		void OnButtonUp(Input::GamepadInput::Button button);
		void OnButtonCancel(Input::GamepadInput::Button button);

		void OnAnalogInput(Input::GamepadInput::Analog analogInput, float value);

		void OnAxisInput(Input::GamepadInput::Axis axis, Widgets::Widget& axisWidget, ScreenCoordinate coordinate);
		void ResetInputAxis(Input::GamepadInput::Axis axis);

		void LoadWidgets(Widgets::Widget& parentWidget);
		void SetupController(Widgets::Widget& parentWidget);

		void EnableModule(GamepadModulePosition position, GamepadModuleType type);
		void DisableModules();

		void HookTouchInput(Input::ActionMonitor& actionMonitor);
		void UnhookTouchInput();
		void UpdateTouchInputHook(Input::Monitor& newMonitor);

		void ConnectEventCallbacks();
		void DisconnectEventCallbacks();

		void SetupModuleMapping();
		void SetActiveMonitor(Input::GamepadDeviceType& gamepadDeviceType);
	private:
		Input::DeviceIdentifier m_deviceIdentifier;
		bool m_shouldBeVisible{false};
		Optional<const Input::ActionMonitor*> m_pTouchActionMonitor{nullptr};
		Optional<const Input::ActionMonitor*> m_pGamepadActionMonitor{nullptr};
		Optional<Widgets::Widget*> m_pGamepad{nullptr};
		struct GamepadModuleInstance
		{
			Optional<Widgets::Widget*> pWidget{nullptr};
			uint16 mappingIndex;
		};
		Vector<GamepadModuleInstance> m_moduleInstances;
		UnorderedMap<uintptr, Input::GamepadInput::Button> m_buttonInstances;
		UnorderedMap<uintptr, Input::GamepadInput::Analog> m_analogInstances;
		UnorderedMap<uintptr, Input::GamepadInput::Axis> m_axisInstances;
		UnorderedMap<Input::FingerIdentifier, uintptr> m_activeWidgets;

		Input::TouchAction m_touchAction;
	};
}
