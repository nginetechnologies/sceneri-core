#if PLATFORM_ANDROID
#include "ControllerManager.h"
#include <Engine/Engine.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>
#include <Engine/Input/Devices/Gamepad/GamepadMapping.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>

#include <Renderer/Window/Window.h>
#include <Renderer/Devices/LogicalDevice.h>

#include <Common/IO/File.h>
#include <Common/System/Query.h>
#include <Common/CommandLine/CommandLineInitializationParameters.h>
#include <Common/IO/Log.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Math/IsEquivalentTo.h>
#include <Common/Math/Radius.h>
#include <Common/EnumFlags.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/keycodes.h>
#include <android/input.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

namespace ngine::Platform::Android
{
	void ControllerManager::Initialize(Rendering::Window& window)
	{
		m_pWindow = window;
		EnableVirtualController();
	}

	void ControllerManager::Update()
	{
	}

	void ControllerManager::AddController(int32 deviceId)
	{
		m_gamepads.EmplaceBackUnique(uintptr(deviceId));
	}

	void ControllerManager::RemoveController(int32 deviceId)
	{
		m_gamepads.RemoveAllOccurrences(uintptr(deviceId));

		// TODO: Remove device instance?
	}

	int32 ControllerManager::GetControllerCount() const
	{
		return int32(m_gamepads.GetSize());
	}

	void ControllerManager::EnableVirtualController()
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		gamepadDeviceType.EnableVirtualGamepad();
	}

	void ControllerManager::DisableVirtualController()
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		gamepadDeviceType.DisableVirtualGamepad();
	}

	void ControllerManager::EnableAxisIds(uint64 activeAxisIds)
	{
		uint64 newAxisIds = activeAxisIds ^ m_activeAxisIds;
		if (newAxisIds != 0)
		{
			m_activeAxisIds = activeAxisIds;

			int32 currentAxisId = 0;
			while (newAxisIds != 0)
			{
				if ((newAxisIds & 1) != 0)
				{
					GameActivityPointerAxes_enableAxis(currentAxisId);
				}
				++currentAxisId;
				newAxisIds >>= 1;
			}
		}
	}

	void ControllerManager::ProcessMotionEvent(const GameActivityMotionEvent& motionEvent)
	{
		if ((motionEvent.source & AINPUT_SOURCE_JOYSTICK) == AINPUT_SOURCE_JOYSTICK && motionEvent.action == ACTION_MOVE)
		{
			Input::Manager& inputManager = System::Get<Input::Manager>();
			Input::GamepadDeviceType& gamepadDeviceType =
				inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

			if (const Input::DeviceIdentifier deviceIdentifier = gamepadDeviceType.GetOrRegisterInstance(uintptr(motionEvent.deviceId), inputManager, m_pWindow))
			{
				const int32 historySize = GameActivityMotionEvent_getHistorySize(&motionEvent);
				for (int32 i = 0; i < historySize; i++)
				{
					ProcessJoystick(gamepadDeviceType, deviceIdentifier, motionEvent, i);
				}
				ProcessJoystick(gamepadDeviceType, deviceIdentifier, motionEvent, -1);
			}
		}
	}

	void ControllerManager::ProcessKeyEvent(const GameActivityKeyEvent& keyEvent)
	{
		if ((keyEvent.source & AINPUT_SOURCE_GAMEPAD) == AINPUT_SOURCE_GAMEPAD)
		{
			Input::Manager& inputManager = System::Get<Input::Manager>();
			Input::GamepadDeviceType& gamepadDeviceType =
				inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

			if (const Input::DeviceIdentifier deviceIdentifier = gamepadDeviceType.GetOrRegisterInstance(uintptr(keyEvent.deviceId), inputManager, m_pWindow))
			{
				for (Input::GamepadMapping mapping : Input::GamepadMappings)
				{
					if (keyEvent.keyCode == mapping.m_source)
					{
						switch (keyEvent.action)
						{
							case ACTION_DOWN:
								gamepadDeviceType.OnButtonDown(deviceIdentifier, mapping.m_physicalTarget.GetExpected<Input::GamepadInput::Button>());
								break;
							case ACTION_UP:
								gamepadDeviceType.OnButtonUp(deviceIdentifier, mapping.m_physicalTarget.GetExpected<Input::GamepadInput::Button>());
								break;
							default:
								LogWarning("Unexpected action {} for keycode {}", keyEvent.action, keyEvent.keyCode);
						}
						break;
					}
				}
			}
		}
		else if ((keyEvent.source & AINPUT_SOURCE_KEYBOARD) == AINPUT_SOURCE_KEYBOARD)
		{
			Input::Manager& inputManager = System::Get<Input::Manager>();
			Input::KeyboardDeviceType& keyboardDeviceType =
				inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());

			if (const Input::DeviceIdentifier deviceIdentifier = keyboardDeviceType.GetOrRegisterInstance(uintptr(keyEvent.deviceId), inputManager, m_pWindow))
			{
				const static auto getKeyboardInput = [](const int32 keyCode, const int32 modifiers)
				{
					switch (keyCode)
					{
						case AKEYCODE_BACK:
							return Input::KeyboardInput::Backspace;
						case AKEYCODE_TAB:
							return Input::KeyboardInput::Tab;
						case AKEYCODE_ENTER:
							return Input::KeyboardInput::Enter;
						case AKEYCODE_SHIFT_LEFT:
							return Input::KeyboardInput::LeftShift;
						case AKEYCODE_SHIFT_RIGHT:
							return Input::KeyboardInput::RightShift;
						case AKEYCODE_CTRL_LEFT:
							return Input::KeyboardInput::LeftControl;
						case AKEYCODE_CTRL_RIGHT:
							return Input::KeyboardInput::RightControl;
						case AKEYCODE_ALT_LEFT:
							return Input::KeyboardInput::LeftAlt;
						case AKEYCODE_ALT_RIGHT:
							return Input::KeyboardInput::RightAlt;
						case AKEYCODE_META_LEFT:
							return Input::KeyboardInput::LeftCommand;
						case AKEYCODE_META_RIGHT:
							return Input::KeyboardInput::RightCommand;
						case AKEYCODE_CAPS_LOCK:
							return Input::KeyboardInput::CapsLock;
						case AKEYCODE_ESCAPE:
							return Input::KeyboardInput::Escape;
						case AKEYCODE_SPACE:
							return Input::KeyboardInput::Space;
						case AKEYCODE_PAGE_UP:
							return Input::KeyboardInput::PageUp;
						case AKEYCODE_PAGE_DOWN:
							return Input::KeyboardInput::PageDown;
						case AKEYCODE_MOVE_END:
							return Input::KeyboardInput::End;
						case AKEYCODE_HOME:
							return Input::KeyboardInput::Home;
						case AKEYCODE_DPAD_LEFT:
							return Input::KeyboardInput::ArrowLeft;
						case AKEYCODE_DPAD_UP:
							return Input::KeyboardInput::ArrowUp;
						case AKEYCODE_DPAD_RIGHT:
							return Input::KeyboardInput::ArrowRight;
						case AKEYCODE_DPAD_DOWN:
							return Input::KeyboardInput::ArrowDown;
						case AKEYCODE_INSERT:
							return Input::KeyboardInput::Insert;
						case AKEYCODE_DEL:
							return Input::KeyboardInput::Delete;
						case AKEYCODE_0:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::CloseParantheses;
							}
							else
							{
								return Input::KeyboardInput::Zero;
							}
						case AKEYCODE_1:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::Exclamation;
							}
							else
							{
								return Input::KeyboardInput::One;
							}
						case AKEYCODE_2:
							return Input::KeyboardInput::Two;
						case AKEYCODE_3:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::Hash;
							}
							else
							{
								return Input::KeyboardInput::Three;
							}
						case AKEYCODE_4:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::Dollar;
							}
							else
							{
								return Input::KeyboardInput::Four;
							}
						case AKEYCODE_5:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::Percent;
							}
							else
							{
								return Input::KeyboardInput::Five;
							}
						case AKEYCODE_6:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::Circumflex;
							}
							else
							{
								return Input::KeyboardInput::Six;
							}
						case AKEYCODE_7:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::Ampersand;
							}
							else
							{
								return Input::KeyboardInput::Seven;
							}
						case AKEYCODE_8:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::Asterisk;
							}
							else
							{
								return Input::KeyboardInput::Eight;
							}
						case AKEYCODE_9:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::OpenParantheses;
							}
							else
							{
								return Input::KeyboardInput::Nine;
							}
						case AKEYCODE_SEMICOLON:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::Colon;
							}
							else
							{
								return Input::KeyboardInput::Semicolon;
							}
						case AKEYCODE_COMMA:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::LessThan;
							}
							else
							{
								return Input::KeyboardInput::Comma;
							}
						case AKEYCODE_PERIOD:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::GreaterThan;
							}
							else
							{
								return Input::KeyboardInput::Period;
							}
						case AKEYCODE_SLASH:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::QuestionMark;
							}
							else
							{
								return Input::KeyboardInput::ForwardSlash;
							}
						case AKEYCODE_EQUALS:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::Plus;
							}
							else
							{
								return Input::KeyboardInput::Equals;
							}
						case AKEYCODE_AT:
							return Input::KeyboardInput::At;
						case AKEYCODE_A:
							return Input::KeyboardInput::A;
						case AKEYCODE_B:
							return Input::KeyboardInput::B;
						case AKEYCODE_C:
							return Input::KeyboardInput::C;
						case AKEYCODE_D:
							return Input::KeyboardInput::D;
						case AKEYCODE_E:
							return Input::KeyboardInput::E;
						case AKEYCODE_F:
							return Input::KeyboardInput::F;
						case AKEYCODE_G:
							return Input::KeyboardInput::G;
						case AKEYCODE_H:
							return Input::KeyboardInput::H;
						case AKEYCODE_I:
							return Input::KeyboardInput::I;
						case AKEYCODE_J:
							return Input::KeyboardInput::J;
						case AKEYCODE_K:
							return Input::KeyboardInput::K;
						case AKEYCODE_L:
							return Input::KeyboardInput::L;
						case AKEYCODE_M:
							return Input::KeyboardInput::M;
						case AKEYCODE_N:
							return Input::KeyboardInput::N;
						case AKEYCODE_O:
							return Input::KeyboardInput::O;
						case AKEYCODE_P:
							return Input::KeyboardInput::P;
						case AKEYCODE_Q:
							return Input::KeyboardInput::Q;
						case AKEYCODE_R:
							return Input::KeyboardInput::R;
						case AKEYCODE_S:
							return Input::KeyboardInput::S;
						case AKEYCODE_T:
							return Input::KeyboardInput::T;
						case AKEYCODE_U:
							return Input::KeyboardInput::U;
						case AKEYCODE_V:
							return Input::KeyboardInput::V;
						case AKEYCODE_W:
							return Input::KeyboardInput::W;
						case AKEYCODE_X:
							return Input::KeyboardInput::X;
						case AKEYCODE_Y:
							return Input::KeyboardInput::Y;
						case AKEYCODE_Z:
							return Input::KeyboardInput::Z;
						case AKEYCODE_NUMPAD_0:
							return Input::KeyboardInput::Numpad0;
						case AKEYCODE_NUMPAD_1:
							return Input::KeyboardInput::Numpad1;
						case AKEYCODE_NUMPAD_2:
							return Input::KeyboardInput::Numpad2;
						case AKEYCODE_NUMPAD_3:
							return Input::KeyboardInput::Numpad3;
						case AKEYCODE_NUMPAD_4:
							return Input::KeyboardInput::Numpad4;
						case AKEYCODE_NUMPAD_5:
							return Input::KeyboardInput::Numpad5;
						case AKEYCODE_NUMPAD_6:
							return Input::KeyboardInput::Numpad6;
						case AKEYCODE_NUMPAD_7:
							return Input::KeyboardInput::Numpad7;
						case AKEYCODE_NUMPAD_8:
							return Input::KeyboardInput::Numpad8;
						case AKEYCODE_NUMPAD_9:
							return Input::KeyboardInput::Numpad9;
						case AKEYCODE_NUMPAD_MULTIPLY:
							return Input::KeyboardInput::Multiply;
						case AKEYCODE_NUMPAD_ADD:
							return Input::KeyboardInput::Add;
						case AKEYCODE_NUMPAD_SUBTRACT:
							return Input::KeyboardInput::Subtract;
						case AKEYCODE_NUMPAD_DOT:
							return Input::KeyboardInput::Decimal;
						case AKEYCODE_NUMPAD_DIVIDE:
							return Input::KeyboardInput::Divide;
						case AKEYCODE_F1:
							return Input::KeyboardInput::F1;
						case AKEYCODE_F2:
							return Input::KeyboardInput::F2;
						case AKEYCODE_F3:
							return Input::KeyboardInput::F3;
						case AKEYCODE_F4:
							return Input::KeyboardInput::F4;
						case AKEYCODE_F5:
							return Input::KeyboardInput::F5;
						case AKEYCODE_F6:
							return Input::KeyboardInput::F6;
						case AKEYCODE_F7:
							return Input::KeyboardInput::F7;
						case AKEYCODE_F8:
							return Input::KeyboardInput::F8;
						case AKEYCODE_F9:
							return Input::KeyboardInput::F9;
						case AKEYCODE_F10:
							return Input::KeyboardInput::F10;
						case AKEYCODE_F11:
							return Input::KeyboardInput::F11;
						case AKEYCODE_F12:
							return Input::KeyboardInput::F12;
						case AKEYCODE_PLUS:
							return Input::KeyboardInput::Plus;
						case AKEYCODE_MINUS:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::Underscore;
							}
							else
							{
								return Input::KeyboardInput::Hyphen;
							}
						case AKEYCODE_GRAVE:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::Tilde;
							}
							else
							{
								return Input::KeyboardInput::BackQuote;
							}
						case AKEYCODE_LEFT_BRACKET:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::OpenCurlyBracket;
							}
							else
							{
								return Input::KeyboardInput::OpenBracket;
							}
						case AKEYCODE_BACKSLASH:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::Pipe;
							}
							else
							{
								return Input::KeyboardInput::BackSlash;
							}
						case AKEYCODE_RIGHT_BRACKET:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::CloseCurlyBracket;
							}
							else
							{
								return Input::KeyboardInput::CloseBracket;
							}
						case AKEYCODE_APOSTROPHE:
							if ((modifiers & AMETA_SHIFT_ON) != 0)
							{
								return Input::KeyboardInput::DoubleQuote;
							}
							else
							{
								return Input::KeyboardInput::Quote;
							}
					}
					return Input::KeyboardInput::Unknown;
				};

				const Input::KeyboardInput input = getKeyboardInput(keyEvent.keyCode, keyEvent.modifiers);

				switch (keyEvent.action)
				{
					case ACTION_DOWN:
						keyboardDeviceType.OnKeyDown(deviceIdentifier, input);
						break;
					case ACTION_UP:
						keyboardDeviceType.OnKeyUp(deviceIdentifier, input);
						break;
					default:
						LogWarning("Unexpected action {} for keycode {}", keyEvent.action, keyEvent.keyCode);
				}
			}
		}
	}

	void ControllerManager::ProcessJoystick(
		Input::GamepadDeviceType& gamepadDeviceType,
		Input::DeviceIdentifier deviceIdentifier,
		const GameActivityMotionEvent& motionEvent,
		int32 historicalPos
	)
	{
		Math::Vector2f joystickLeft(Math::Zero);
		if (m_activeAxisIds & (1 << AMOTION_EVENT_AXIS_X))
		{
			joystickLeft.x = GetCenteredAxis(motionEvent, AMOTION_EVENT_AXIS_X, historicalPos);
		}
		if (m_activeAxisIds & (1 << AMOTION_EVENT_AXIS_Y))
		{
			joystickLeft.y = GetCenteredAxis(motionEvent, AMOTION_EVENT_AXIS_Y, historicalPos);
		}
		gamepadDeviceType.OnAxisInput(deviceIdentifier, Input::GamepadInput::Axis::LeftThumbstick, joystickLeft);

		Math::Vector2f joystickRight(Math::Zero);
		if (m_activeAxisIds & (1 << AMOTION_EVENT_AXIS_Z))
		{
			joystickRight.x = GetCenteredAxis(motionEvent, AMOTION_EVENT_AXIS_Z, historicalPos);
		}
		if (m_activeAxisIds & (1 << AMOTION_EVENT_AXIS_RZ))
		{
			joystickRight.y = GetCenteredAxis(motionEvent, AMOTION_EVENT_AXIS_RZ, historicalPos);
		}
		gamepadDeviceType.OnAxisInput(deviceIdentifier, Input::GamepadInput::Axis::RightThumbstick, joystickRight);

		if (m_activeAxisIds & (1 << AMOTION_EVENT_AXIS_HAT_X))
		{
			const float dPadX = GetCenteredAxis(motionEvent, AMOTION_EVENT_AXIS_HAT_X, historicalPos);
			if (Math::IsEquivalentTo(dPadX, -1.0f))
			{
				gamepadDeviceType.OnButtonDown(deviceIdentifier, Input::GamepadInput::Button::DirectionPadLeft);
			}
			else if (Math::IsEquivalentTo(dPadX, 1.0f))
			{
				gamepadDeviceType.OnButtonDown(deviceIdentifier, Input::GamepadInput::Button::DirectionPadRight);
			}
			else
			{
				gamepadDeviceType.OnButtonUp(deviceIdentifier, Input::GamepadInput::Button::DirectionPadLeft);
				gamepadDeviceType.OnButtonUp(deviceIdentifier, Input::GamepadInput::Button::DirectionPadRight);
			}
		}
		if (m_activeAxisIds & (1 << AMOTION_EVENT_AXIS_HAT_Y))
		{
			const float dPadY = GetCenteredAxis(motionEvent, AMOTION_EVENT_AXIS_HAT_Y, historicalPos);
			if (Math::IsEquivalentTo(dPadY, -1.0f))
			{
				gamepadDeviceType.OnButtonDown(deviceIdentifier, Input::GamepadInput::Button::DirectionPadUp);
			}
			else if (Math::IsEquivalentTo(dPadY, 1.0f))
			{
				gamepadDeviceType.OnButtonDown(deviceIdentifier, Input::GamepadInput::Button::DirectionPadDown);
			}
			else
			{
				gamepadDeviceType.OnButtonUp(deviceIdentifier, Input::GamepadInput::Button::DirectionPadUp);
				gamepadDeviceType.OnButtonUp(deviceIdentifier, Input::GamepadInput::Button::DirectionPadDown);
			}
		}
		if (m_activeAxisIds & (1 << AMOTION_EVENT_AXIS_BRAKE))
		{
			const float brakeValue = GetCenteredAxis(motionEvent, AMOTION_EVENT_AXIS_BRAKE, historicalPos);
			gamepadDeviceType.OnAnalogInput(deviceIdentifier, Input::GamepadInput::Analog::LeftTrigger, brakeValue);
		}
		if (m_activeAxisIds & (1 << AMOTION_EVENT_AXIS_GAS))
		{
			const float gasValue = GetCenteredAxis(motionEvent, AMOTION_EVENT_AXIS_GAS, historicalPos);
			gamepadDeviceType.OnAnalogInput(deviceIdentifier, Input::GamepadInput::Analog::RightTrigger, gasValue);
		}
	}

	float ControllerManager::GetCenteredAxis(const GameActivityMotionEvent& motionEvent, int32 axis, int32 historyPos)
	{
		// TODO: Get the real value from hardware motion range (getMotionRange.getFlat)
		constexpr float flat = 0.05f;
		const float value = historyPos < 0 ? GameActivityPointerAxes_getAxisValue(motionEvent.pointers, axis)
		                                   : GameActivityMotionEvent_getHistoricalAxisValue(&motionEvent, axis, 0, historyPos);

		if (Math::Abs(value) > flat)
		{
			return value;
		}
		return 0.f;
	}
}
#endif
