#include "Input/Devices/Keyboard/Keyboard.h"
#include "Input/InputManager.h"
#include "Input/Monitor.h"

#include <Common/Math/Vector2.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Serialization/Reader.h>
#include <Common/Platform/Type.h>

#include <Renderer/Window/Window.h>

#if PLATFORM_WINDOWS
#include <Common/Platform/Windows.h>
#endif

namespace ngine::Input
{
	KeyboardDeviceType::KeyboardDeviceType(const DeviceTypeIdentifier identifier, Input::Manager& manager)
		: DeviceType(identifier)
		, m_manager(manager)
		, m_textInputIdentifier(manager.RegisterInput())
	{
		for (KeyboardInput i = KeyboardInput(); i < KeyboardInput::Count; ++i)
		{
			m_keyIdentifiers[(KeyboardInput)i] = manager.RegisterInput();
		}
	}

	Input::DeviceIdentifier
	KeyboardDeviceType::GetOrRegisterInstance(const uintptr platformIdentifier, Input::Manager& manager, Optional<Rendering::Window*>)
	{
		Optional<const Keyboard*> pKeyboard;

		manager.IterateDeviceInstances(
			[identifier = m_identifier, platformIdentifier, &pKeyboard](const DeviceInstance& instance) -> Memory::CallbackResult
			{
				if (instance.GetTypeIdentifier() == identifier)
				{
					if (static_cast<const Keyboard&>(instance).m_platformIdentifier == platformIdentifier)
					{
						pKeyboard = static_cast<const Keyboard&>(instance);
						return Memory::CallbackResult::Break;
					}
				}

				return Memory::CallbackResult::Continue;
			}
		);

		if (LIKELY(pKeyboard.IsValid()))
		{
			return pKeyboard->GetIdentifier();
		}

		Optional<Input::Monitor*> pFocusedMonitor;
		m_manager.IterateDeviceInstances(
			[mouseTypeIdentifier = m_manager.GetMouseDeviceTypeIdentifier(), &pFocusedMonitor](DeviceInstance& instance) -> Memory::CallbackResult
			{
				if (instance.GetTypeIdentifier() == mouseTypeIdentifier)
				{
					pFocusedMonitor = instance.GetActiveMonitor();
					return Memory::CallbackResult::Break;
				}

				return Memory::CallbackResult::Continue;
			}
		);

		return manager.RegisterDeviceInstance<Keyboard>(m_identifier, pFocusedMonitor, platformIdentifier);
	}

	Input::DeviceIdentifier KeyboardDeviceType::FindInstance(const uintptr platformIdentifier, Input::Manager& manager)
	{
		Optional<const Keyboard*> pKeyboard;

		manager.IterateDeviceInstances(
			[identifier = m_identifier, platformIdentifier, &pKeyboard](const DeviceInstance& instance) -> Memory::CallbackResult
			{
				if (instance.GetTypeIdentifier() == identifier)
				{
					if (static_cast<const Keyboard&>(instance).m_platformIdentifier == platformIdentifier)
					{
						pKeyboard = static_cast<const Keyboard&>(instance);
						return Memory::CallbackResult::Break;
					}
				}

				return Memory::CallbackResult::Continue;
			}
		);

		if (LIKELY(pKeyboard.IsValid()))
		{
			return pKeyboard->GetIdentifier();
		}

		return Input::DeviceIdentifier();
	}

	EnumFlags<KeyboardModifier> KeyboardDeviceType::GetActiveModifiers(Input::DeviceIdentifier deviceIdentifier) const
	{
		Keyboard& keyboard = static_cast<Keyboard&>(m_manager.GetDeviceInstance(deviceIdentifier));
		EnumFlags<KeyboardModifier> modifiers;
		modifiers |= KeyboardModifier::LeftShift * keyboard.m_keyStates.IsSet((uint8)KeyboardInput::LeftShift);
		modifiers |= KeyboardModifier::RightShift * keyboard.m_keyStates.IsSet((uint8)KeyboardInput::RightShift);
		modifiers |= KeyboardModifier::LeftControl * keyboard.m_keyStates.IsSet((uint8)KeyboardInput::LeftControl);
		modifiers |= KeyboardModifier::RightControl * keyboard.m_keyStates.IsSet((uint8)KeyboardInput::RightControl);
		modifiers |= KeyboardModifier::LeftAlt * keyboard.m_keyStates.IsSet((uint8)KeyboardInput::LeftAlt);
		modifiers |= KeyboardModifier::RightAlt * keyboard.m_keyStates.IsSet((uint8)KeyboardInput::RightAlt);
		modifiers |= KeyboardModifier::Capital * keyboard.m_keyStates.IsSet((uint8)KeyboardInput::CapsLock);
		modifiers |= KeyboardModifier::LeftCommand * keyboard.m_keyStates.IsSet((uint8)KeyboardInput::LeftCommand);
		modifiers |= KeyboardModifier::RightCommand * keyboard.m_keyStates.IsSet((uint8)KeyboardInput::RightCommand);
		return modifiers;
	}

	void KeyboardDeviceType::OnKeyDown(Input::DeviceIdentifier deviceIdentifier, Input::KeyboardInput input)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, input]()
			{
				Keyboard& keyboard = static_cast<Keyboard&>(m_manager.GetDeviceInstance(deviceIdentifier));
				if (!keyboard.m_keyStates.IsSet((UNDERLYING_TYPE(KeyboardInput))input))
				{
					keyboard.m_keyStates.Set((UNDERLYING_TYPE(KeyboardInput))input);

					if (Monitor* pMonitor = keyboard.GetActiveMonitor())
					{
						pMonitor->OnKeyboardInputDown(deviceIdentifier, m_keyIdentifiers[input], input, GetActiveModifiers(deviceIdentifier));
					}
				}
				else
				{
					if (Monitor* pMonitor = keyboard.GetActiveMonitor())
					{
						pMonitor->OnKeyboardInputRepeat(deviceIdentifier, m_keyIdentifiers[input], input, GetActiveModifiers(deviceIdentifier));
					}
				}
			}
		);
	}

	void KeyboardDeviceType::OnKeyUp(Input::DeviceIdentifier deviceIdentifier, Input::KeyboardInput input)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, input]()
			{
				Keyboard& keyboard = static_cast<Keyboard&>(m_manager.GetDeviceInstance(deviceIdentifier));
				if (keyboard.m_keyStates.IsSet((UNDERLYING_TYPE(KeyboardInput))input))
				{
					keyboard.m_keyStates.Clear((UNDERLYING_TYPE(KeyboardInput))input);
					if (Monitor* pMonitor = keyboard.GetActiveMonitor())
					{
						pMonitor->OnKeyboardInputUp(deviceIdentifier, m_keyIdentifiers[input], input, GetActiveModifiers(deviceIdentifier));
					}
				}
			}
		);
	}

	void KeyboardDeviceType::OnText(Input::DeviceIdentifier deviceIdentifier, const ConstUnicodeStringView text)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, text = UnicodeString{text}]()
			{
				Keyboard& keyboard = static_cast<Keyboard&>(m_manager.GetDeviceInstance(deviceIdentifier));
				const EnumFlags<KeyboardModifier> activeModifiers = GetActiveModifiers(deviceIdentifier);
				if (activeModifiers.AreAnySet(KeyboardModifier::LeftCommand | KeyboardModifier::RightCommand))
				{
					// Ignore system input
					return;
				}

				const bool isControlHeld = activeModifiers.AreAnySet(KeyboardModifier::LeftControl | KeyboardModifier::RightControl);
				const bool isAltHeld = activeModifiers.IsSet(KeyboardModifier::LeftAlt);
				const bool isAltGraphHeld = (isControlHeld && isAltHeld) || activeModifiers.IsSet(KeyboardModifier::RightAlt);

				if ((isControlHeld || isAltHeld) && !isAltGraphHeld)
				{
					// Ignore system input
					return;
				}

				if (Monitor* pMonitor = keyboard.GetActiveMonitor())
				{
					pMonitor->OnKeyboardInputText(deviceIdentifier, m_textInputIdentifier, text);
				}
			}
		);
	}

	void KeyboardDeviceType::SyncKeyStates([[maybe_unused]] Keyboard& keyboard)
	{
#if PLATFORM_WINDOWS
		m_manager.QueueInput(
			[this, &keyboard]()
			{
				Monitor* pMonitor = keyboard.GetActiveMonitor();
				IdentifierMask<InputIdentifier> activeIdentifiers;
				for (uint16 keyboardInput = 0; keyboardInput < (uint16)KeyboardInput::Count; ++keyboardInput)
				{
					if ((GetAsyncKeyState(keyboardInput) & (1 << 15)) != 0)
					{
						if (!keyboard.m_keyStates.IsSet(keyboardInput))
						{
							keyboard.m_keyStates.Set(keyboardInput);

							if (pMonitor != nullptr)
							{
								const Input::InputIdentifier identifier = m_keyIdentifiers[(Input::KeyboardInput)keyboardInput];
								if (identifier.IsValid())
								{
									activeIdentifiers.Set(identifier);
								}
							}
						}
					}
					else if (keyboard.m_keyStates.IsSet(keyboardInput))
					{
						OnKeyUp(keyboard.GetIdentifier(), (Input::KeyboardInput)keyboardInput);
					}
				}

				if (activeIdentifiers.GetNumberOfSetBits() > 0)
				{
					Assert(pMonitor != nullptr);
					pMonitor->OnKeyboardInputRestored(keyboard.GetIdentifier(), activeIdentifiers, GetActiveModifiers(keyboard.GetIdentifier()));
				}
			}

		);
#endif
	}

	void KeyboardDeviceType::Keyboard::OnMonitorChanged(Monitor& newMonitor, const DeviceType& deviceType, Manager& manager)
	{
		manager.QueueInput(
			[this, &newMonitor, &deviceType]()
			{
				IdentifierMask<InputIdentifier> activeIdentifiers;
				m_keyStates.IterateSetBits(
					[&activeIdentifiers, &deviceType = static_cast<const KeyboardDeviceType&>(deviceType)](const uint16 index)
					{
						const InputIdentifier inputIdentifier = deviceType.m_keyIdentifiers[(Input::KeyboardInput)index];
						if (inputIdentifier.IsValid())
						{
							activeIdentifiers.Set(inputIdentifier);
						}
						return true;
					}
				);

				if (activeIdentifiers.GetNumberOfSetBits() > 0)
				{
					newMonitor.OnKeyboardInputRestored(
						GetIdentifier(),
						activeIdentifiers,
						static_cast<const KeyboardDeviceType&>(deviceType).GetActiveModifiers(GetIdentifier())
					);
				}
			}
		);
	}

	void KeyboardDeviceType::Keyboard::RestoreInputState(
		Monitor& monitor, const DeviceType& deviceType, const InputIdentifier inputIdentifier, Manager& manager
	)
	{
		manager.QueueInput(
			[this, &monitor, &deviceType, inputIdentifier]()
			{
				IdentifierMask<InputIdentifier> activeIdentifiers;
				m_keyStates.IterateSetBits(
					[&activeIdentifiers, inputIdentifier, &deviceType = static_cast<const KeyboardDeviceType&>(deviceType)](const uint16 index)
					{
						const InputIdentifier currentKeyInputIdentifier = deviceType.m_keyIdentifiers[(Input::KeyboardInput)index];
						if (currentKeyInputIdentifier == inputIdentifier)
						{
							activeIdentifiers.Set(currentKeyInputIdentifier);
						}

						return true;
					}
				);

				if (activeIdentifiers.GetNumberOfSetBits() > 0)
				{
					monitor.OnKeyboardInputRestored(
						GetIdentifier(),
						activeIdentifiers,
						static_cast<const KeyboardDeviceType&>(deviceType).GetActiveModifiers(GetIdentifier())
					);
				}
			}
		);
	}

	void KeyboardDeviceType::RestoreInputState(Monitor& monitor, const InputIdentifier inputIdentifier) const
	{
		m_manager.QueueInput(
			[this, &monitor, inputIdentifier]()
			{
				const DeviceTypeIdentifier typeIdentifier = m_manager.GetKeyboardDeviceTypeIdentifier();
				m_manager.IterateDeviceInstances(
					[typeIdentifier, inputIdentifier, &monitor, this](DeviceInstance& deviceInstance) -> Memory::CallbackResult
					{
						if (deviceInstance.GetTypeIdentifier() == typeIdentifier)
						{
							Keyboard& keyboard = static_cast<Keyboard&>(deviceInstance);
							keyboard.RestoreInputState(monitor, *this, inputIdentifier, m_manager);
						}

						return Memory::CallbackResult::Continue;
					}
				);
			}
		);
	}

	InputIdentifier KeyboardDeviceType::DeserializeDeviceInput(const Serialization::Reader& reader) const
	{
		using KeyboardInputType = UNDERLYING_TYPE(KeyboardInput);
		Optional<KeyboardInputType> keyValue = reader.Read<KeyboardInputType>("key");

		Assert(keyValue.IsValid() && *keyValue < static_cast<KeyboardInputType>(KeyboardInput::Count), "Keyboard key is not valid");
		if (keyValue.IsValid() && *keyValue < static_cast<KeyboardInputType>(KeyboardInput::Count))
		{
			return m_keyIdentifiers[static_cast<KeyboardInput>(*keyValue)];
		}

		return {};
	}

	void KeyboardDeviceType::OnActiveWindowReceivedKeyboardFocus()
	{
		const DeviceTypeIdentifier typeIdentifier = m_manager.GetKeyboardDeviceTypeIdentifier();
		m_manager.IterateDeviceInstances(
			[typeIdentifier, this](DeviceInstance& deviceInstance) -> Memory::CallbackResult
			{
				if (deviceInstance.GetTypeIdentifier() == typeIdentifier)
				{
					Keyboard& keyboard = static_cast<Keyboard&>(deviceInstance);
					SyncKeyStates(keyboard);
				}

				return Memory::CallbackResult::Continue;
			}
		);
	}

	void KeyboardDeviceType::OnActiveWindowLostKeyboardFocus()
	{
		m_manager.QueueInput(
			[this]()
			{
				const DeviceTypeIdentifier typeIdentifier = m_manager.GetKeyboardDeviceTypeIdentifier();
				m_manager.IterateDeviceInstances(
					[typeIdentifier, this](DeviceInstance& deviceInstance) -> Memory::CallbackResult
					{
						if (deviceInstance.GetTypeIdentifier() == typeIdentifier)
						{
							Keyboard& keyboard = static_cast<Keyboard&>(deviceInstance);
							if (Monitor* pMonitor = keyboard.GetActiveMonitor())
							{
								keyboard.m_keyStates.IterateSetBits(
									[&keyboard, this, pMonitor](const uint64 bitIndex) -> bool
									{
										pMonitor->OnKeyboardInputCancelled(
											keyboard.GetIdentifier(),
											m_keyIdentifiers[(KeyboardInput)bitIndex],
											(KeyboardInput)bitIndex,
											GetActiveModifiers(keyboard.GetIdentifier())
										);
										return true;
									}
								);

								keyboard.m_keyStates.ClearAll();
								keyboard.SetActiveMonitor(nullptr, *this);
							}
						}

						return Memory::CallbackResult::Continue;
					}
				);
			}
		);
	}

	bool KeyboardDeviceType::IsPressed(const Input::DeviceIdentifier deviceIdentifier, const Input::KeyboardInput input) const
	{
		Keyboard& keyboard = static_cast<Keyboard&>(m_manager.GetDeviceInstance(deviceIdentifier));
		return keyboard.m_keyStates.IsSet((UNDERLYING_TYPE(Input::KeyboardInput))input);
	}

	KeyboardInput GetKeyboardInputFromString(const ConstStringView string)
	{
		struct CaseInsensitiveHash
		{
			using is_transparent = void;

			size operator()(ConstStringView view) const
			{
				String string{view};
				string.MakeLower();
				String::Hash stringHash;
				return stringHash(string);
			}
		};

		struct CaseInsensitiveStringView : public ConstStringView
		{
			using ConstStringView::ConstStringView;
			using ConstStringView::operator=;

			[[nodiscard]] bool operator==(const CaseInsensitiveStringView other) const
			{
				return EqualsCaseInsensitive(other);
			}
			[[nodiscard]] bool operator!=(const CaseInsensitiveStringView other) const
			{
				return !operator==(other);
			}
		};

		using LookupMapType = UnorderedMap<CaseInsensitiveStringView, KeyboardInput, CaseInsensitiveHash>;
		static const LookupMapType lookupMap{
			LookupMapType::NewPairType{"Backspace", KeyboardInput::Backspace},
			LookupMapType::NewPairType{"Tab", KeyboardInput::Tab},
			LookupMapType::NewPairType{"Enter", KeyboardInput::Enter},
			LookupMapType::NewPairType{"CapsLock", KeyboardInput::CapsLock},
			LookupMapType::NewPairType{"LeftShift", KeyboardInput::LeftShift},
			LookupMapType::NewPairType{"RightShift", KeyboardInput::RightShift},
			LookupMapType::NewPairType{"Shift", KeyboardInput::LeftShift},
			LookupMapType::NewPairType{"LeftControl", KeyboardInput::LeftControl},
			LookupMapType::NewPairType{"RightControl", KeyboardInput::RightControl},
			LookupMapType::NewPairType{"Control", KeyboardInput::LeftControl},
			LookupMapType::NewPairType{"LeftAlt", KeyboardInput::LeftAlt},
			LookupMapType::NewPairType{"RightAlt", KeyboardInput::RightAlt},
			LookupMapType::NewPairType{"Alt", KeyboardInput::LeftAlt},
			LookupMapType::NewPairType{"AltGraph", KeyboardInput::RightAlt},
			LookupMapType::NewPairType{"Escape", KeyboardInput::Escape},
			LookupMapType::NewPairType{"Space", KeyboardInput::Space},
			LookupMapType::NewPairType{"PageUp", KeyboardInput::PageUp},
			LookupMapType::NewPairType{"PageDown", KeyboardInput::PageDown},
			LookupMapType::NewPairType{"End", KeyboardInput::End},
			LookupMapType::NewPairType{"Home", KeyboardInput::Home},
			LookupMapType::NewPairType{"ArrowLeft", KeyboardInput::ArrowLeft},
			LookupMapType::NewPairType{"ArrowUp", KeyboardInput::ArrowUp},
			LookupMapType::NewPairType{"ArrowRight", KeyboardInput::ArrowRight},
			LookupMapType::NewPairType{"ArrowDown", KeyboardInput::ArrowDown},
			LookupMapType::NewPairType{"Insert", KeyboardInput::Insert},
			LookupMapType::NewPairType{"Delete", KeyboardInput::Delete},

			LookupMapType::NewPairType{"0", KeyboardInput::Zero},
			LookupMapType::NewPairType{"1", KeyboardInput::One},
			LookupMapType::NewPairType{"2", KeyboardInput::Two},
			LookupMapType::NewPairType{"3", KeyboardInput::Three},
			LookupMapType::NewPairType{"4", KeyboardInput::Four},
			LookupMapType::NewPairType{"5", KeyboardInput::Five},
			LookupMapType::NewPairType{"6", KeyboardInput::Six},
			LookupMapType::NewPairType{"7", KeyboardInput::Seven},
			LookupMapType::NewPairType{"8", KeyboardInput::Eight},
			LookupMapType::NewPairType{"9", KeyboardInput::Nine},

			LookupMapType::NewPairType{"A", KeyboardInput::A},
			LookupMapType::NewPairType{"B", KeyboardInput::B},
			LookupMapType::NewPairType{"C", KeyboardInput::C},
			LookupMapType::NewPairType{"D", KeyboardInput::D},
			LookupMapType::NewPairType{"E", KeyboardInput::E},
			LookupMapType::NewPairType{"F", KeyboardInput::F},
			LookupMapType::NewPairType{"G", KeyboardInput::G},
			LookupMapType::NewPairType{"H", KeyboardInput::H},
			LookupMapType::NewPairType{"I", KeyboardInput::I},
			LookupMapType::NewPairType{"J", KeyboardInput::J},
			LookupMapType::NewPairType{"K", KeyboardInput::K},
			LookupMapType::NewPairType{"L", KeyboardInput::L},
			LookupMapType::NewPairType{"M", KeyboardInput::M},
			LookupMapType::NewPairType{"N", KeyboardInput::N},
			LookupMapType::NewPairType{"O", KeyboardInput::O},
			LookupMapType::NewPairType{"P", KeyboardInput::P},
			LookupMapType::NewPairType{"Q", KeyboardInput::Q},
			LookupMapType::NewPairType{"R", KeyboardInput::R},
			LookupMapType::NewPairType{"S", KeyboardInput::S},
			LookupMapType::NewPairType{"T", KeyboardInput::T},
			LookupMapType::NewPairType{"U", KeyboardInput::U},
			LookupMapType::NewPairType{"V", KeyboardInput::V},
			LookupMapType::NewPairType{"W", KeyboardInput::W},
			LookupMapType::NewPairType{"X", KeyboardInput::X},
			LookupMapType::NewPairType{"Y", KeyboardInput::Y},
			LookupMapType::NewPairType{"Z", KeyboardInput::Z},

			LookupMapType::NewPairType{"/", KeyboardInput::ForwardSlash},
			LookupMapType::NewPairType{"\\", KeyboardInput::BackSlash},
			LookupMapType::NewPairType{"'", KeyboardInput::Quote},
			LookupMapType::NewPairType{"`", KeyboardInput::BackQuote},
			LookupMapType::NewPairType{"\"", KeyboardInput::DoubleQuote},
			LookupMapType::NewPairType{".", KeyboardInput::Period},
			LookupMapType::NewPairType{",", KeyboardInput::Comma},
			LookupMapType::NewPairType{"~", KeyboardInput::Tilde},
			LookupMapType::NewPairType{"-", KeyboardInput::Minus},
			LookupMapType::NewPairType{"+", KeyboardInput::Plus},
			LookupMapType::NewPairType{"|", KeyboardInput::Pipe},
			LookupMapType::NewPairType{"*", KeyboardInput::Asterisk},
			LookupMapType::NewPairType{"_", KeyboardInput::Underscore},
			LookupMapType::NewPairType{"&", KeyboardInput::Ampersand},
			LookupMapType::NewPairType{"%", KeyboardInput::Percent},
			LookupMapType::NewPairType{"$", KeyboardInput::Dollar},
			LookupMapType::NewPairType{"#", KeyboardInput::Hash},
			LookupMapType::NewPairType{"!", KeyboardInput::Exclamation},
			LookupMapType::NewPairType{"?", KeyboardInput::QuestionMark},
			LookupMapType::NewPairType{">", KeyboardInput::GreaterThan},
			LookupMapType::NewPairType{"<", KeyboardInput::LessThan},
			LookupMapType::NewPairType{"=", KeyboardInput::Equals},
			LookupMapType::NewPairType{";", KeyboardInput::Semicolon},
			LookupMapType::NewPairType{":", KeyboardInput::Colon},
			LookupMapType::NewPairType{"@", KeyboardInput::At},
			LookupMapType::NewPairType{"^", KeyboardInput::Circumflex},

			LookupMapType::NewPairType{"Numpad0", KeyboardInput::Numpad0},
			LookupMapType::NewPairType{"Numpad1", KeyboardInput::Numpad1},
			LookupMapType::NewPairType{"Numpad2", KeyboardInput::Numpad2},
			LookupMapType::NewPairType{"Numpad3", KeyboardInput::Numpad3},
			LookupMapType::NewPairType{"Numpad4", KeyboardInput::Numpad4},
			LookupMapType::NewPairType{"Numpad5", KeyboardInput::Numpad5},
			LookupMapType::NewPairType{"Numpad6", KeyboardInput::Numpad6},
			LookupMapType::NewPairType{"Numpad7", KeyboardInput::Numpad7},
			LookupMapType::NewPairType{"Numpad8", KeyboardInput::Numpad8},
			LookupMapType::NewPairType{"Numpad9", KeyboardInput::Numpad9},
			LookupMapType::NewPairType{"Multiply", KeyboardInput::Multiply},
			LookupMapType::NewPairType{"Add", KeyboardInput::Add},
			LookupMapType::NewPairType{"Subtract", KeyboardInput::Subtract},
			LookupMapType::NewPairType{"Decimal", KeyboardInput::Decimal},
			LookupMapType::NewPairType{"Divide", KeyboardInput::Divide},

			LookupMapType::NewPairType{"RightCommand", KeyboardInput::RightCommand},
			LookupMapType::NewPairType{"LeftCommand", KeyboardInput::LeftCommand},
			LookupMapType::NewPairType{"Command", KeyboardInput::LeftCommand},
			LookupMapType::NewPairType{"Meta", KeyboardInput::LeftCommand},

			LookupMapType::NewPairType{"F1", KeyboardInput::F1},
			LookupMapType::NewPairType{"F2", KeyboardInput::F2},
			LookupMapType::NewPairType{"F3", KeyboardInput::F3},
			LookupMapType::NewPairType{"F4", KeyboardInput::F4},
			LookupMapType::NewPairType{"F5", KeyboardInput::F5},
			LookupMapType::NewPairType{"F6", KeyboardInput::F6},
			LookupMapType::NewPairType{"F7", KeyboardInput::F7},
			LookupMapType::NewPairType{"F8", KeyboardInput::F8},
			LookupMapType::NewPairType{"F9", KeyboardInput::F9},
			LookupMapType::NewPairType{"F10", KeyboardInput::F10},
			LookupMapType::NewPairType{"F11", KeyboardInput::F11},
			LookupMapType::NewPairType{"F12", KeyboardInput::F12},

			LookupMapType::NewPairType{"[", KeyboardInput::OpenBracket},
			LookupMapType::NewPairType{"]", KeyboardInput::CloseBracket},
			LookupMapType::NewPairType{"{", KeyboardInput::OpenCurlyBracket},
			LookupMapType::NewPairType{"}", KeyboardInput::CloseCurlyBracket},
			LookupMapType::NewPairType{"(", KeyboardInput::OpenParantheses},
			LookupMapType::NewPairType{")", KeyboardInput::CloseParantheses}
		};

		const auto it = lookupMap.Find((CaseInsensitiveStringView)string);
		return it != lookupMap.end() ? it->second : KeyboardInput::Unknown;
	}
}
