#pragma once

#include <Common/Function/Function.h>
#include <Common/Math/Vector2.h>
#include <Engine/Input/Actions/Action.h>
#include <Engine/Input/Actions/DeleteTextType.h>
#include <Engine/Input/Actions/MoveTextCursorFlags.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>

#include <Common/Memory/Containers/String.h>

namespace ngine::Input
{
	struct TextInputAction final : public Action
	{
		Function<void(const ConstUnicodeStringView text), 24> OnInput;
		Function<void(const EnumFlags<MoveTextCursorFlags> flags), 24> OnMoveTextCursor = [](const EnumFlags<MoveTextCursorFlags>)
		{
		};
		Function<void(), 24> OnApply = []()
		{
		};
		Function<void(), 24> OnAbort = []()
		{
		};
		Function<void(DeleteTextType), 24> OnDelete = [](const DeleteTextType)
		{
		};

		void BindInputs(ActionMonitor& monitor, const Input::KeyboardDeviceType& keyboard)
		{
			Action::BindInput(monitor, keyboard, keyboard.GetTextInputIdentifier());
			Action::BindInput(monitor, keyboard, keyboard.GetInputIdentifier(KeyboardInput::ArrowLeft));
			Action::BindInput(monitor, keyboard, keyboard.GetInputIdentifier(KeyboardInput::ArrowRight));
			Action::BindInput(monitor, keyboard, keyboard.GetInputIdentifier(KeyboardInput::ArrowUp));
			Action::BindInput(monitor, keyboard, keyboard.GetInputIdentifier(KeyboardInput::ArrowDown));
			Action::BindInput(monitor, keyboard, keyboard.GetInputIdentifier(KeyboardInput::Delete));
			Action::BindInput(monitor, keyboard, keyboard.GetInputIdentifier(KeyboardInput::Enter));
			Action::BindInput(monitor, keyboard, keyboard.GetInputIdentifier(KeyboardInput::Escape));
			Action::BindInput(monitor, keyboard, keyboard.GetInputIdentifier(KeyboardInput::Backspace));
			Action::BindInput(monitor, keyboard, keyboard.GetInputIdentifier(KeyboardInput::A));
		}

		void UnbindInputs(ActionMonitor& monitor, const Input::KeyboardDeviceType& keyboard)
		{
			Action::UnbindInput(monitor, keyboard.GetTextInputIdentifier());
			Action::UnbindInput(monitor, keyboard.GetInputIdentifier(KeyboardInput::ArrowLeft));
			Action::UnbindInput(monitor, keyboard.GetInputIdentifier(KeyboardInput::ArrowRight));
			Action::UnbindInput(monitor, keyboard.GetInputIdentifier(KeyboardInput::ArrowUp));
			Action::UnbindInput(monitor, keyboard.GetInputIdentifier(KeyboardInput::ArrowDown));
			Action::UnbindInput(monitor, keyboard.GetInputIdentifier(KeyboardInput::Delete));
			Action::UnbindInput(monitor, keyboard.GetInputIdentifier(KeyboardInput::Enter));
			Action::UnbindInput(monitor, keyboard.GetInputIdentifier(KeyboardInput::Escape));
			Action::UnbindInput(monitor, keyboard.GetInputIdentifier(KeyboardInput::Backspace));
			Action::UnbindInput(monitor, keyboard.GetInputIdentifier(KeyboardInput::A));
		}
	protected:
		virtual void OnKeyboardInputText(const DeviceIdentifier, const InputIdentifier, const ConstUnicodeStringView text) override
		{
			OnInput(text);
		}

		virtual void OnKeyboardInputRepeat(
			const DeviceIdentifier, const InputIdentifier, const KeyboardInput input, const EnumFlags<KeyboardModifier> keyboardModifiers
		) override
		{
			HandleKeyboardInput(input, keyboardModifiers);
		}

		virtual void OnKeyboardInputDown(
			const DeviceIdentifier, const InputIdentifier, const KeyboardInput input, const EnumFlags<KeyboardModifier> keyboardModifiers
		) override
		{
			HandleKeyboardInput(input, keyboardModifiers);
		}

		void HandleKeyboardInput(const KeyboardInput input, const EnumFlags<KeyboardModifier> keyboardModifiers)
		{
			EnumFlags<MoveTextCursorFlags> moveTextCursorFlags;
			if (keyboardModifiers.AreAnySet(KeyboardModifier::LeftShift | KeyboardModifier::RightShift))
			{
				moveTextCursorFlags |= MoveTextCursorFlags::ApplyToSelection;
			}
			switch (input)
			{
				case KeyboardInput::A:
				{
					if (keyboardModifiers.AreAnySet(
								KeyboardModifier::LeftControl | KeyboardModifier::RightControl | KeyboardModifier::LeftCommand |
								KeyboardModifier::RightCommand
							))
					{
						OnMoveTextCursor(MoveTextCursorFlags::ApplyToSelection | MoveTextCursorFlags::All);
					}
				}
				break;
				case KeyboardInput::ArrowLeft:
				{
					if (keyboardModifiers.AreAnySet(
								KeyboardModifier::LeftControl | KeyboardModifier::RightControl | KeyboardModifier::LeftCommand |
								KeyboardModifier::RightCommand
							))
					{
						moveTextCursorFlags |= MoveTextCursorFlags::BeginningOfCurrentLine;
					}
					else if (keyboardModifiers.AreAnySet(KeyboardModifier::LeftAlt | KeyboardModifier::RightAlt))
					{
						moveTextCursorFlags |= MoveTextCursorFlags::PreviousWord;
					}
					else
					{
						moveTextCursorFlags |= MoveTextCursorFlags::Left;
					}
					OnMoveTextCursor(moveTextCursorFlags);
				}
				break;
				case KeyboardInput::ArrowRight:
				{
					if (keyboardModifiers.AreAnySet(
								KeyboardModifier::LeftControl | KeyboardModifier::RightControl | KeyboardModifier::LeftCommand |
								KeyboardModifier::RightCommand
							))
					{
						moveTextCursorFlags |= MoveTextCursorFlags::EndOfCurrentLine;
					}
					else if (keyboardModifiers.AreAnySet(KeyboardModifier::LeftAlt | KeyboardModifier::RightAlt))
					{
						moveTextCursorFlags |= MoveTextCursorFlags::NextWord;
					}
					else
					{
						moveTextCursorFlags |= MoveTextCursorFlags::Right;
					}
					OnMoveTextCursor(moveTextCursorFlags);
				}
				break;
				case KeyboardInput::ArrowUp:
				{
					if (keyboardModifiers.AreAnySet(
								KeyboardModifier::LeftControl | KeyboardModifier::RightControl | KeyboardModifier::LeftControl |
								KeyboardModifier::RightCommand
							))
					{
						moveTextCursorFlags |= MoveTextCursorFlags::BeginningOfDocument;
					}
					else if (keyboardModifiers.AreAnySet(KeyboardModifier::LeftAlt | KeyboardModifier::RightAlt))
					{
						moveTextCursorFlags |= MoveTextCursorFlags::BeginningOfCurrentParagraph;
					}
					else
					{
						moveTextCursorFlags |= MoveTextCursorFlags::Up;
					}
					OnMoveTextCursor(moveTextCursorFlags);
				}
				break;
				case KeyboardInput::ArrowDown:
				{
					if (keyboardModifiers.AreAnySet(
								KeyboardModifier::LeftControl | KeyboardModifier::RightControl | KeyboardModifier::LeftControl |
								KeyboardModifier::RightCommand
							))
					{
						moveTextCursorFlags |= MoveTextCursorFlags::EndOfDocument;
					}
					else if (keyboardModifiers.AreAnySet(KeyboardModifier::LeftAlt | KeyboardModifier::RightAlt))
					{
						moveTextCursorFlags |= MoveTextCursorFlags::EndOfCurrentParagraph;
					}
					else
					{
						moveTextCursorFlags |= MoveTextCursorFlags::Down;
					}
					OnMoveTextCursor(moveTextCursorFlags);
				}
				break;
				case KeyboardInput::Backspace:
				{
					if (keyboardModifiers.AreAnySet(
								KeyboardModifier::LeftAlt | KeyboardModifier::RightAlt | KeyboardModifier::LeftControl | KeyboardModifier::RightControl
							))
					{
						OnDelete(DeleteTextType::LeftWord);
					}
					else if (keyboardModifiers.AreAnySet(KeyboardModifier::LeftCommand | KeyboardModifier::RightCommand))
					{
						OnDelete(DeleteTextType::FullLineToLeftOfCursor);
					}
					else
					{
						OnDelete(DeleteTextType::LeftCharacter);
					}
				}
				break;
				case KeyboardInput::Delete:
				{
					if (keyboardModifiers.AreAnySet(
								KeyboardModifier::LeftAlt | KeyboardModifier::RightAlt | KeyboardModifier::LeftControl | KeyboardModifier::RightControl
							))
					{
						OnDelete(DeleteTextType::RightWord);
					}
					else if (keyboardModifiers.AreAnySet(KeyboardModifier::LeftCommand | KeyboardModifier::RightCommand))
					{
						OnDelete(DeleteTextType::FullLineToRightOfCursor);
					}
					else
					{
						OnDelete(DeleteTextType::RightCharacter);
					}
				}
				break;
				case KeyboardInput::Enter:
					OnApply();
					break;
				case KeyboardInput::Escape:
					OnAbort();
					break;
				default:
					break;
			}
		}
	protected:
		Optional<KeyboardInput> m_heldInput;
	};
}
