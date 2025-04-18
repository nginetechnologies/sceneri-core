#pragma once

#include "GamepadInput.h"

#if PLATFORM_WINDOWS
#define USE_WINDOWS_CONTROLLER 1

namespace XInput
{
	enum class Gamepad
	{
		DPAD_UP = 0x0001,
		DPAD_DOWN = 0x0002,
		DPAD_LEFT = 0x0004,
		DPAD_RIGHT = 0x0008,
		START = 0x0010,
		BACK = 0x0020,
		LEFT_THUMB = 0x0040,
		RIGHT_THUMB = 0x0080,
		LEFT_SHOULDER = 0x0100,
		RIGHT_SHOULDER = 0x0200,
		A = 0x1000,
		B = 0x2000,
		X = 0x4000,
		Y = 0x8000
	};

	inline static constexpr int GAMEPAD_LEFT_THUMB_DEADZONE = 7849;
	inline static constexpr int GAMEPAD_RIGHT_THUMB_DEADZONE = 8689;
	inline static constexpr int GAMEPAD_TRIGGER_THRESHOLD = 30;
}
#elif PLATFORM_ANDROID
#define USE_ANDROID_CONTROLLER 1
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS || PLATFORM_APPLE_MACOS
#define USE_APPLE_GAME_CONTROLLER 1
#else
#define USE_APPLE_GAME_CONTROLLER 0
#endif

#include <Common/Memory/Variant.h>
#include <Common/Math/Vector2.h>

namespace ngine::Input
{
#if USE_APPLE_GAME_CONTROLLER || USE_WINDOWS_CONTROLLER || USE_ANDROID_CONTROLLER
	struct GamepadMapping
	{
#if USE_APPLE_GAME_CONTROLLER
		using SourceInputIdentifier = void*;
#elif USE_WINDOWS_CONTROLLER
		using SourceInputIdentifier = int;
#elif USE_ANDROID_CONTROLLER
		using SourceInputIdentifier = int32;
#endif

		enum class Type : uint8
		{
			Button,
			AnalogInput,
			Axis,
			DirectionalPad
		};

		GamepadMapping(SourceInputIdentifier source, GamepadInput::Button target)
			: m_source(source)
			, m_type(Type::Button)
			, m_physicalTarget(target)
		{
		}

		GamepadMapping(SourceInputIdentifier source, GamepadInput::Analog target)
			: m_source(source)
			, m_type(Type::AnalogInput)
			, m_physicalTarget(target)
		{
		}

		GamepadMapping(SourceInputIdentifier source, GamepadInput::Axis target)
			: m_source(source)
			, m_type(Type::Axis)
			, m_physicalTarget(target)
		{
		}

		GamepadMapping(SourceInputIdentifier source, Type type)
			: m_source(source)
			, m_type(type)
		{
		}

		SourceInputIdentifier m_source;
		Type m_type = Type::Button;

		Variant<GamepadInput::Button, GamepadInput::Analog, GamepadInput::Axis> m_physicalTarget;
	};
#endif

#if USE_ANDROID_CONTROLLER
	constexpr static int32 KEYCODE_BUTTON_A = 0x00000060;
	constexpr static int32 KEYCODE_BUTTON_B = 0x00000061;
	constexpr static int32 KEYCODE_BUTTON_X = 0x00000063;
	constexpr static int32 KEYCODE_BUTTON_Y = 0x00000064;
	constexpr static int32 KEYCODE_BUTTON_L1 = 0x00000066;
	constexpr static int32 KEYCODE_BUTTON_R1 = 0x00000067;
	constexpr static int32 KEYCODE_BUTTON_THUMBL = 0x0000006a;
	constexpr static int32 KEYCODE_BUTTON_THUMBR = 0x0000006b;
	constexpr static int32 KEYCODE_BUTTON_START = 0x0000006c;
	constexpr static int32 KEYCODE_BUTTON_SELECT = 0x0000006d;
	constexpr static int32 KEYCODE_DPAD_LEFT = 0x00000015;
	constexpr static int32 KEYCODE_DPAD_RIGHT = 0x00000016;
	constexpr static int32 KEYCODE_DPAD_UP = 0x00000013;
	constexpr static int32 KEYCODE_DPAD_DOWN = 0x00000014;

	inline static Array GamepadMappings = {
		GamepadMapping{KEYCODE_BUTTON_A, Input::GamepadInput::Button::A},
		GamepadMapping{KEYCODE_BUTTON_B, Input::GamepadInput::Button::B},
		GamepadMapping{KEYCODE_BUTTON_X, Input::GamepadInput::Button::X},
		GamepadMapping{KEYCODE_BUTTON_Y, Input::GamepadInput::Button::Y},
		GamepadMapping{KEYCODE_BUTTON_L1, Input::GamepadInput::Button::LeftShoulder},
		GamepadMapping{KEYCODE_BUTTON_R1, Input::GamepadInput::Button::RightShoulder},
		GamepadMapping{KEYCODE_BUTTON_THUMBL, Input::GamepadInput::Button::LeftThumbstick},
		GamepadMapping{KEYCODE_BUTTON_THUMBR, Input::GamepadInput::Button::RightThumbstick},
		GamepadMapping{KEYCODE_BUTTON_SELECT, Input::GamepadInput::Button::Menu},
		GamepadMapping{KEYCODE_BUTTON_START, Input::GamepadInput::Button::Home},
		GamepadMapping{KEYCODE_DPAD_LEFT, Input::GamepadInput::Button::DirectionPadLeft},
		GamepadMapping{KEYCODE_DPAD_RIGHT, Input::GamepadInput::Button::DirectionPadRight},
		GamepadMapping{KEYCODE_DPAD_UP, Input::GamepadInput::Button::DirectionPadUp},
		GamepadMapping{KEYCODE_DPAD_DOWN, Input::GamepadInput::Button::DirectionPadDown}
	};
#endif

#if USE_WINDOWS_CONTROLLER
	inline static Math::Vector2f GetGamepadAxisValue(int16 x, int16 y)
	{
		Math::Vector2f value{float(x), float(y)};
		float magnitude = value.GetLength();
		if (magnitude > XInput::GAMEPAD_LEFT_THUMB_DEADZONE)
		{
			magnitude = Math::Max(magnitude, 32767.f);
			magnitude -= XInput::GAMEPAD_LEFT_THUMB_DEADZONE;
			const float normalizedMagnitude = magnitude / (32767 - XInput::GAMEPAD_LEFT_THUMB_DEADZONE);
			return value.GetNormalized() * normalizedMagnitude;
		}
		return Math::Vector2f(Math::Zero);
	}

	inline static float GetGamepadAnalogValue(uint8 trigger)
	{
		float magnitude = float(trigger);
		float normalizedMagnitude = 0.f;
		if (magnitude > XInput::GAMEPAD_TRIGGER_THRESHOLD)
		{
			magnitude -= XInput::GAMEPAD_TRIGGER_THRESHOLD;
			normalizedMagnitude = magnitude / (255 - XInput::GAMEPAD_TRIGGER_THRESHOLD);
		}
		return normalizedMagnitude;
	}
#endif
} // namespace ngine::Input
