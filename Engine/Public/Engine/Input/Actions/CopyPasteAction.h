#pragma once

#include <Common/Function/Function.h>
#include <Common/Platform/Type.h>
#include <Engine/Input/Actions/Action.h>
#include <Engine/Input/Actions/BinaryAction.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>

namespace ngine::Input
{
	struct CopyPasteAction final : public Action
	{
		Function<void(), 24> OnCopy;
		Function<void(), 24> OnPaste;

		void BindInputs(ActionMonitor& monitor, const Input::KeyboardDeviceType& keyboard)
		{
			m_inputIdentifierC = keyboard.GetInputIdentifier(Input::KeyboardInput::C);
			Action::BindInput(monitor, keyboard, m_inputIdentifierC);

			m_inputIdentifierV = keyboard.GetInputIdentifier(Input::KeyboardInput::V);
			Action::BindInput(monitor, keyboard, m_inputIdentifierV);
		}

		void UnbindInputs(ActionMonitor& monitor, const Input::KeyboardDeviceType&)
		{
			Action::UnbindInput(monitor, m_inputIdentifierC);
			Action::UnbindInput(monitor, m_inputIdentifierV);
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
				if (inputIdentifier == m_inputIdentifierC)
				{
					OnCopy();
				}
				else if (inputIdentifier == m_inputIdentifierV)
				{
					OnPaste();
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
				if (inputIdentifier == m_inputIdentifierC)
				{
					OnCopy();
				}
				else if (inputIdentifier == m_inputIdentifierV)
				{
					OnPaste();
				}
			}
		}
	protected:
		InputIdentifier m_inputIdentifierC;
		InputIdentifier m_inputIdentifierV;
	};
}
