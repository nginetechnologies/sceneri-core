#include "Input/Devices/Gamepad/Gamepad.h"
#include "Input/Devices/Gamepad/GamepadMapping.h"
#include "Input/InputManager.h"
#include "Input/Monitor.h"
#include "Input/Actions/ActionMonitor.h"

#include <Renderer/Window/Window.h>

#include <Common/System/Query.h>
#include <Common/Math/IsEquivalentTo.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Serialization/Reader.h>

namespace ngine::Input
{
	GamepadDeviceType::GamepadDeviceType(const DeviceTypeIdentifier identifier, Manager& manager)
		: DeviceType(identifier)
		, m_manager(manager)
	{
		for (uint8 index = 0; index < (uint8)GamepadInput::Button::Count; ++index)
		{
			m_gamepadInputIdentifiers.m_buttons[index] = manager.RegisterInput();
		}

		for (uint8 index = 0; index < (uint8)GamepadInput::Analog::Count; ++index)
		{
			m_gamepadInputIdentifiers.m_analogInputs[index] = manager.RegisterInput();
		}

		for (uint8 index = 0; index < (uint8)GamepadInput::Axis::Count; ++index)
		{
			m_gamepadInputIdentifiers.m_axisInputs[index] = manager.RegisterInput();
		}
	}

	DeviceIdentifier GamepadDeviceType::GetOrRegisterInstance(
		Gamepad::PlatformIdentifier platformIdentifier, Manager& manager, Optional<Rendering::Window*> pSourceWindow
	)
	{
		if (const DeviceIdentifier deviceIdentifier = FindInstance(platformIdentifier, manager))
		{
			return deviceIdentifier;
		}

		Optional<Monitor*> pFocusedMonitor = pSourceWindow.IsValid() ? pSourceWindow->GetFocusedInputMonitor() : Optional<Monitor*>{};
		m_manager.IterateDeviceInstances(
			[mouseTypeIdentifier = m_manager.GetMouseDeviceTypeIdentifier(),
		   touchTypeIdentifier = m_manager.GetTouchscreenDeviceTypeIdentifier(),
		   &pFocusedMonitor](DeviceInstance& instance) -> Memory::CallbackResult
			{
				if (instance.GetTypeIdentifier() == mouseTypeIdentifier)
				{
					pFocusedMonitor = instance.GetActiveMonitor();
					return Memory::CallbackResult::Break;
				}
				else if (instance.GetTypeIdentifier() == touchTypeIdentifier)
				{
					pFocusedMonitor = instance.GetActiveMonitor();
					return Memory::CallbackResult::Break;
				}

				return Memory::CallbackResult::Continue;
			}
		);

		return manager.RegisterDeviceInstance<Gamepad>(m_identifier, pFocusedMonitor, platformIdentifier);
	}

	DeviceIdentifier GamepadDeviceType::FindInstance(Gamepad::PlatformIdentifier platformIdentifier, Manager& manager) const
	{
		if (const Optional<const Gamepad*> pGamepad = FindGamepad(platformIdentifier, manager))
		{
			return pGamepad->GetIdentifier();
		}
		return DeviceIdentifier();
	}

	Optional<Gamepad*> GamepadDeviceType::FindGamepad(Gamepad::PlatformIdentifier platformIdentifier, Manager& manager) const
	{
		Optional<Gamepad*> pGamepad = Invalid;

		manager.IterateDeviceInstances(
			[identifier = m_identifier, platformIdentifier, &pGamepad](DeviceInstance& instance) -> Memory::CallbackResult
			{
				if (instance.GetTypeIdentifier() == identifier)
				{
					if (static_cast<Gamepad&>(instance).GetPlatformIdentifier() == platformIdentifier)
					{
						pGamepad = static_cast<Gamepad&>(instance);
						return Memory::CallbackResult::Break;
					}
				}
				return Memory::CallbackResult::Continue;
			}
		);

		return pGamepad;
	}

	Gamepad& GamepadDeviceType::GetGamepad(DeviceIdentifier deviceIdentifier) const
	{
		return static_cast<Gamepad&>(m_manager.GetDeviceInstance(deviceIdentifier));
	}

	void GamepadDeviceType::OnButtonDown(DeviceIdentifier deviceIdentifier, GamepadInput::Button button)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, button]()
			{
				Gamepad& gamepad = GetGamepad(deviceIdentifier);
				if (!gamepad.GetButtonInput(button))
				{
					gamepad.SetButtonInput(button, true);

					if (Monitor* pMonitor = gamepad.GetActiveMonitor())
					{
						pMonitor->OnBinaryInputDown(deviceIdentifier, GetInputIdentifier(button));
					}
				}
			}
		);
	}

	void GamepadDeviceType::OnButtonUp(DeviceIdentifier deviceIdentifier, GamepadInput::Button button)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, button]()
			{
				Gamepad& gamepad = GetGamepad(deviceIdentifier);
				if (gamepad.GetButtonInput(button))
				{
					gamepad.SetButtonInput(button, false);

					if (Monitor* pMonitor = gamepad.GetActiveMonitor())
					{
						pMonitor->OnBinaryInputUp(deviceIdentifier, GetInputIdentifier(button));
					}
				}
			}
		);
	}

	void GamepadDeviceType::OnButtonCancel(DeviceIdentifier deviceIdentifier, GamepadInput::Button button)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, button]()
			{
				Gamepad& gamepad = GetGamepad(deviceIdentifier);
				if (gamepad.GetButtonInput(button))
				{
					gamepad.SetButtonInput(button, false);

					if (Monitor* pMonitor = gamepad.GetActiveMonitor())
					{
						pMonitor->OnBinaryInputCancelled(deviceIdentifier, GetInputIdentifier(button));
					}
				}
			}
		);
	}

	void GamepadDeviceType::OnAnalogInput(DeviceIdentifier deviceIdentifier, GamepadInput::Analog analogInput, float value)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, analogInput, value]()
			{
				Gamepad& gamepad = GetGamepad(deviceIdentifier);

				const float prevValue = gamepad.GetAnalogInput(analogInput);
				if (Math::IsEquivalentTo(value, prevValue))
				{
					return;
				}

				gamepad.SetAnalogInput(analogInput, value);

				if (Monitor* pMonitor = gamepad.GetActiveMonitor())
				{
					pMonitor->OnAnalogInput(deviceIdentifier, GetInputIdentifier(analogInput), value, prevValue);
				}
			}
		);
	}

	void GamepadDeviceType::OnAxisInput(DeviceIdentifier deviceIdentifier, GamepadInput::Axis axisInput, const Math::Vector2f value)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, axisInput, value]()
			{
				Gamepad& gamepad = GetGamepad(deviceIdentifier);

				const Math::Vector2f prevValue = gamepad.GetAxisInput(axisInput);
				if (Math::IsEquivalentTo(value.x, prevValue.x) && Math::IsEquivalentTo(value.y, prevValue.y))
				{
					return;
				}

				gamepad.SetAxisInput(axisInput, value);
				if (Monitor* pMonitor = gamepad.GetActiveMonitor())
				{
					pMonitor->On2DAnalogInput(deviceIdentifier, GetInputIdentifier(axisInput), value, prevValue);
				}
			}
		);
	}

	void GamepadDeviceType::EnableVirtualGamepad()
	{
		if (!m_isVirtualGamepadEnabled)
		{
			m_isVirtualGamepadEnabled = true;
			OnVirtualGamepadEnabled();
		}
	}

	void GamepadDeviceType::DisableVirtualGamepad()
	{
		if (m_isVirtualGamepadEnabled)
		{
			m_isVirtualGamepadEnabled = false;
			OnVirtualGamepadDisabled();
		}
	}

	InputIdentifier GamepadDeviceType::DeserializeDeviceInput(const Serialization::Reader& reader) const
	{
		using GamepadButtonType = UNDERLYING_TYPE(GamepadInput::Button);
		using GamepadAxisType = UNDERLYING_TYPE(GamepadInput::Axis);
		using GamepadAnalogType = UNDERLYING_TYPE(GamepadInput::Analog);

		if (Optional<GamepadButtonType> buttonValue = reader.Read<GamepadButtonType>("button"))
		{
			return GetInputIdentifier(static_cast<GamepadInput::Button>(*buttonValue));
		}

		if (Optional<GamepadAxisType> axisValue = reader.Read<GamepadAxisType>("axis"))
		{
			return GetInputIdentifier(static_cast<GamepadInput::Axis>(*axisValue));
		}

		if (Optional<GamepadAnalogType> analogValue = reader.Read<GamepadAnalogType>("analog"))
		{
			return GetInputIdentifier(static_cast<GamepadInput::Analog>(*analogValue));
		}

		return {};
	}

	void GamepadDeviceType::OnInputEnabled(const ActionMonitor& monitor)
	{
		OnInputEnabledEvent(monitor);

		Input::Monitor* pMonitor = const_cast<Input::Monitor*>(static_cast<const Input::Monitor*>(&monitor));
		m_manager.IterateDeviceInstances(
			[this, gamepadTypeIdentifier = m_manager.GetGamepadDeviceTypeIdentifier(), pMonitor](DeviceInstance& instance
		  ) -> Memory::CallbackResult
			{
				if (instance.GetTypeIdentifier() == gamepadTypeIdentifier)
				{
					if (instance.GetActiveMonitor() != pMonitor)
					{
						instance.SetActiveMonitor(pMonitor, *this);
					}
				}
				return Memory::CallbackResult::Continue;
			}
		);
	}

	void GamepadDeviceType::OnInputDisabled(const ActionMonitor& monitor)
	{
		OnInputDisabledEvent(monitor);
	}
}
