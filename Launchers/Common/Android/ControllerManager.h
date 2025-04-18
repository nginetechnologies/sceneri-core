#pragma once

#include <Engine/Input/InputIdentifier.h>
#include <Engine/Input/DeviceIdentifier.h>

#include <Common/Memory/Containers/InlineVector.h>

#include <android/input.h>

namespace ngine::Rendering
{
	struct Window;
}

namespace ngine::Input
{
	struct GamepadDeviceType;
}

struct GameActivityMotionEvent;
struct GameActivityKeyEvent;

namespace ngine::Platform::Android
{
	struct ControllerManager
	{
		constexpr static int32 ACTION_DOWN = 0x00000000;
		constexpr static int32 ACTION_UP = 0x00000001;
		constexpr static int32 ACTION_MOVE = 0x00000002;

		constexpr static int32 AINPUT_SOURCE_BUTTON = 0x00000001;
		constexpr static int32 AINPUT_SOURCE_KEYBOARD = 0x00000100 | AINPUT_SOURCE_BUTTON;
		constexpr static int32 AINPUT_SOURCE_GAMEPAD = 0x00000400 | AINPUT_SOURCE_CLASS_BUTTON;

		constexpr static int32 AINPUT_SOURCE_CLASS_JOYSTICK = 0x00000010;
		constexpr static int32 AINPUT_SOURCE_JOYSTICK = 0x01000000 | AINPUT_SOURCE_CLASS_JOYSTICK;

		void Initialize(Rendering::Window& window);
		void Update();

		void AddController(int32 deviceId);
		void RemoveController(int32 deviceId);
		int32 GetControllerCount() const;

		void EnableVirtualController();
		void DisableVirtualController();

		void EnableAxisIds(uint64 activeAxisIds);
		void ProcessMotionEvent(const GameActivityMotionEvent& motionEvent);
		void ProcessKeyEvent(const GameActivityKeyEvent& keyEvent);
	protected:
		void ProcessJoystick(
			Input::GamepadDeviceType& gamepadDeviceType,
			Input::DeviceIdentifier deviceIdentifier,
			const GameActivityMotionEvent& motionEvent,
			int32 historicalPos
		);
		float GetCenteredAxis(const GameActivityMotionEvent& motionEvent, int32 axis, int32 historyPos);
	private:
		Optional<Rendering::Window*> m_pWindow;
		InlineVector<uintptr, 1> m_gamepads;
		uint64 m_activeAxisIds{0};
	};
}
