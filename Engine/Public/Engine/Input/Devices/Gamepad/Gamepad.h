#pragma once

#include "GamepadInput.h"

#include <Common/Function/Event.h>
#include <Common/Math/Vector2.h>
#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/Array.h>
#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Math/Log2.h>

#include <Engine/Input/DeviceIdentifier.h>
#include <Engine/Input/InputIdentifier.h>
#include <Engine/Input/DeviceType.h>

namespace ngine::Rendering
{
	struct Window;
}

namespace ngine::Input
{
	struct Manager;
	struct ActionMonitor;

	class Gamepad : public DeviceInstance
	{
	public:
		using PlatformIdentifier = const uintptr;
	public:
		Gamepad(
			const DeviceIdentifier identifier,
			const DeviceTypeIdentifier typeIdentifier,
			Optional<Monitor*> pActiveMonitor,
			PlatformIdentifier platformIdentifier
		)
			: DeviceInstance(identifier, typeIdentifier, pActiveMonitor)
			, m_platformIdentifier(platformIdentifier)
		{
		}

		[[nodiscard]] bool GetButtonInput(GamepadInput::Button buttonInput) const
		{
			return m_buttons.IsSet(buttonInput);
		}

		void SetButtonInput(GamepadInput::Button buttonInput, bool value)
		{
			m_buttons.Set(buttonInput, value);
		}

		float GetAnalogInput(GamepadInput::Analog analogInput) const
		{
			return m_analogInputs[uint8(analogInput)];
		}

		void SetAnalogInput(GamepadInput::Analog analogInput, float value)
		{
			m_analogInputs[uint8(analogInput)] = value;
		}

		Math::Vector2f GetAxisInput(GamepadInput::Axis axisInput) const
		{
			return m_axisInputs[uint8(axisInput)];
		}

		void SetAxisInput(GamepadInput::Axis axisInput, Math::Vector2f value)
		{
			m_axisInputs[uint8(axisInput)] = value;
		}

		[[nodiscard]] PlatformIdentifier GetPlatformIdentifier() const
		{
			return m_platformIdentifier;
		}
	private:
		PlatformIdentifier m_platformIdentifier;
		EnumFlags<GamepadInput::Button> m_buttons;
		Array<float, (uint8)GamepadInput::Analog::Count> m_analogInputs;
		Array<Math::Vector2f, (uint8)GamepadInput::Axis::Count> m_axisInputs;
	};

	struct GamepadDeviceType final : public DeviceType
	{
		inline static constexpr Guid DeviceTypeGuid = "00B29F74-775C-46BA-96E4-BEC2CDE4387A"_guid;

		Event<void(void*, const ActionMonitor& monitor), 32> OnMonitorAssigned;
		Event<void(void*, const ActionMonitor& monitor), 32> OnMonitorAssignedEnd;

		void OnInputEnabled(const ActionMonitor& monitor);
		Event<void(void*, const ActionMonitor& monitor), 32> OnInputEnabledEvent;
		void OnInputDisabled(const ActionMonitor& monitor);
		Event<void(void*, const ActionMonitor& monitor), 32> OnInputDisabledEvent;

		Event<void(void*), 24> OnVirtualGamepadEnabled;
		Event<void(void*), 24> OnVirtualGamepadDisabled;
	public:
		GamepadDeviceType(const DeviceTypeIdentifier identifier, Manager& manager);

		[[nodiscard]] DeviceIdentifier
		GetOrRegisterInstance(Gamepad::PlatformIdentifier platformIdentifier, Manager& manager, Optional<Rendering::Window*> pSourceWindow);
		[[nodiscard]] DeviceIdentifier FindInstance(Gamepad::PlatformIdentifier platformIdentifier, Manager& manager) const;

		[[nodiscard]] InputIdentifier GetInputIdentifier(const GamepadInput::Button input) const
		{
			return m_gamepadInputIdentifiers.m_buttons[GetButtonIndex(input)];
		}

		[[nodiscard]] InputIdentifier GetInputIdentifier(const GamepadInput::Analog input) const
		{
			return m_gamepadInputIdentifiers.m_analogInputs[(uint8)input];
		}

		[[nodiscard]] InputIdentifier GetInputIdentifier(const GamepadInput::Axis input) const
		{
			return m_gamepadInputIdentifiers.m_axisInputs[(uint8)input];
		}

		void OnButtonDown(DeviceIdentifier deviceIdentifier, GamepadInput::Button button);
		void OnButtonUp(DeviceIdentifier deviceIdentifier, GamepadInput::Button button);
		void OnButtonCancel(DeviceIdentifier deviceIdentifier, GamepadInput::Button button);
		void OnAnalogInput(DeviceIdentifier deviceIdentifier, GamepadInput::Analog analogInput, const float value);
		void OnAxisInput(DeviceIdentifier deviceIdentifier, GamepadInput::Axis, const Math::Vector2f value);

		virtual void RestoreInputState(Monitor&, const InputIdentifier) const override{};

		[[nodiscard]] Optional<Gamepad*> FindGamepad(Gamepad::PlatformIdentifier platformIdentifier, Manager& manager) const;
		[[nodiscard]] Gamepad& GetGamepad(DeviceIdentifier deviceIdentifier) const;

		void EnableVirtualGamepad();
		void DisableVirtualGamepad();

		[[nodiscard]] bool IsVirtualGamepadEnabled() const
		{
			return m_isVirtualGamepadEnabled;
		}

		[[nodiscard]] virtual InputIdentifier DeserializeDeviceInput(const Serialization::Reader&) const override;
	protected:
		[[nodiscard]] uint8 GetButtonIndex(const GamepadInput::Button button) const
		{
			return (uint8)Math::Log2((uint32)button);
		}
	private:
		Manager& m_manager;
		struct
		{
			Array<InputIdentifier, (uint8)GamepadInput::Button::Count> m_buttons;
			Array<InputIdentifier, (uint8)GamepadInput::Analog::Count> m_analogInputs;
			Array<InputIdentifier, (uint8)GamepadInput::Axis::Count> m_axisInputs;
		} m_gamepadInputIdentifiers;

		bool m_isVirtualGamepadEnabled{false};
	};
}
