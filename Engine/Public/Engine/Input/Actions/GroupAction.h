#pragma once

#include <Common/Function/Function.h>
#include <Common/Platform/Type.h>
#include <Engine/Input/Actions/Action.h>
#include <Engine/Input/Actions/BinaryAction.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>

namespace ngine::Input
{
	struct GroupAction final : public Action
	{
		Function<void(), 24> OnGroup;
		Function<void(), 24> OnUngroup;

		void BindInputs(ActionMonitor& monitor, const Input::KeyboardDeviceType& keyboard)
		{
			m_inputIdentifierG = keyboard.GetInputIdentifier(Input::KeyboardInput::G);
			Action::BindInput(monitor, keyboard, m_inputIdentifierG);
		}

		void UnbindInputs(ActionMonitor& monitor, const Input::KeyboardDeviceType&)
		{
			if (m_inputIdentifierG.IsValid())
			{
				Action::UnbindInput(monitor, m_inputIdentifierG);
			}
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

			if (inputIdentifier == m_inputIdentifierG && isBaseModifierActive)
			{
				if (modifiers.AreAnySet(KeyboardModifier::LeftShift | KeyboardModifier::RightShift))
				{
					OnUngroup();
				}
				else
				{
					OnGroup();
				}
			}
		}
	protected:
		InputIdentifier m_inputIdentifierG;
	};
}
