#pragma once

#include <Common/Function/Function.h>
#include <Common/Platform/Type.h>

#include <Engine/Input/Actions/Action.h>
#include <Engine/Input/Actions/BinaryAction.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>

namespace ngine::Input
{
	struct UndoAction final : public Action
	{
		Function<void(), 24> OnUndo;
		Function<void(), 24> OnRedo;
		Function<void(), 24> OnRepeatUndo = []()
		{
		};
		Function<void(), 24> OnRepeatRedo = []()
		{
		};

		void BindInputs(ActionMonitor& monitor, const Input::KeyboardDeviceType& keyboard)
		{
			m_inputIdentifierZ = keyboard.GetInputIdentifier(Input::KeyboardInput::Z);
			Action::BindInput(monitor, keyboard, m_inputIdentifierZ);

			m_inputIdentifierY = keyboard.GetInputIdentifier(Input::KeyboardInput::Y);
			Action::BindInput(monitor, keyboard, m_inputIdentifierY);
		}

		void UnbindInputs(ActionMonitor& monitor, const Input::KeyboardDeviceType&)
		{
			Action::UnbindInput(monitor, m_inputIdentifierZ);
			Action::UnbindInput(monitor, m_inputIdentifierY);
		}
	protected:
		[[nodiscard]] static EnumFlags<KeyboardModifier> GetRequiredModifiers()
		{
			switch (Platform::GetEffectiveType())
			{
				case Platform::Type::Windows:
				case Platform::Type::Android:
				case Platform::Type::Linux:
					return KeyboardModifier::LeftControl | KeyboardModifier::RightControl;
				case Platform::Type::macOS:
				case Platform::Type::macCatalyst:
				case Platform::Type::iOS:
				case Platform::Type::visionOS:
					return KeyboardModifier::LeftCommand | KeyboardModifier::RightCommand;
				case Platform::Type::Web:
					Assert(false, "Failed to get effective platform, returning all required modifiers");
					// Allow any variation, means GetEffectiveType is wrong
					return KeyboardModifier::LeftControl | KeyboardModifier::RightControl | KeyboardModifier::LeftCommand |
					       KeyboardModifier::RightCommand;
				case Platform::Type::Apple:
				case Platform::Type::All:
					ExpectUnreachable();
			}
			ExpectUnreachable();
		}

		virtual void OnKeyboardInputDown(
			const DeviceIdentifier, const InputIdentifier inputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier> modifiers
		) override
		{
			const bool isBaseModifierActive = modifiers.AreAnySet(GetRequiredModifiers());

			if (isBaseModifierActive)
			{
				if (inputIdentifier == m_inputIdentifierZ)
				{
					if (modifiers.AreAnySet(KeyboardModifier::LeftShift | KeyboardModifier::RightShift))
					{
						OnRedo();
					}
					else if (isBaseModifierActive)
					{
						OnUndo();
					}
				}
				else if (inputIdentifier == m_inputIdentifierY)
				{
					OnRedo();
				}
			}
		}

		virtual void OnKeyboardInputRepeat(
			const DeviceIdentifier, const InputIdentifier inputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier> modifiers
		) override
		{
			const bool isBaseModifierActive = modifiers.AreAnySet(GetRequiredModifiers());

			if (isBaseModifierActive)
			{
				if (inputIdentifier == m_inputIdentifierZ)
				{
					if (modifiers.AreAnySet(KeyboardModifier::LeftShift | KeyboardModifier::RightShift))
					{
						OnRedo();
					}
					else if (isBaseModifierActive)
					{
						OnUndo();
					}
				}
				else if (inputIdentifier == m_inputIdentifierY)
				{
					OnRedo();
				}
			}
		}
	protected:
		InputIdentifier m_inputIdentifierZ;
		InputIdentifier m_inputIdentifierY;
	};
}
