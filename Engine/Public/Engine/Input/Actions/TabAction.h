#pragma once

#include <Common/Function/Function.h>
#include <Engine/Input/Actions/Action.h>
#include <Engine/Input/Actions/BinaryAction.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>

namespace ngine::Input
{
	struct TabAction final : public Action
	{
		enum class Mode
		{
			//! Default: Tab between widgets
			Widget,
			//! Ctrl held down, cycle windowed tabs
			Tabs
		};

		Function<void(Mode), 24> OnTabBack;
		Function<void(Mode), 24> OnTabForward;

		void BindInputs(ActionMonitor& monitor, const Input::KeyboardDeviceType& keyboard)
		{
			m_inputIdentifier = keyboard.GetInputIdentifier(Input::KeyboardInput::Tab);
			Action::BindInput(monitor, keyboard, m_inputIdentifier);
		}

		void UnbindInputs(ActionMonitor& monitor, const Input::KeyboardDeviceType&)
		{
			Action::UnbindInput(monitor, m_inputIdentifier);
		}
	protected:
		virtual void OnKeyboardInputDown(
			const DeviceIdentifier, const InputIdentifier inputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier> modifiers
		) override
		{
			if (inputIdentifier == m_inputIdentifier)
			{
				const Mode mode = modifiers.AreAnySet(KeyboardModifier::LeftControl | KeyboardModifier::RightControl) ? Mode::Tabs : Mode::Widget;
				if (modifiers.AreAnySet(KeyboardModifier::LeftShift | KeyboardModifier::RightShift))
				{
					OnTabBack(mode);
				}
				else
				{
					OnTabForward(mode);
				}
			}
		}

		virtual void OnKeyboardInputRepeat(
			const DeviceIdentifier, const InputIdentifier inputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier> modifiers
		) override
		{
			if (inputIdentifier == m_inputIdentifier)
			{
				const Mode mode = modifiers.AreAnySet(KeyboardModifier::LeftControl | KeyboardModifier::RightControl) ? Mode::Tabs : Mode::Widget;
				if (modifiers.AreAnySet(KeyboardModifier::LeftShift | KeyboardModifier::RightShift))
				{
					OnTabBack(mode);
				}
				else
				{
					OnTabForward(mode);
				}
			}
		}
	protected:
		InputIdentifier m_inputIdentifier;
	};
}
