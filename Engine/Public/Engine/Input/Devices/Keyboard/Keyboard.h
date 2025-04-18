#pragma once

#include "KeyboardInput.h"
#include "KeyboardModifier.h"

#include <Common/Function/Event.h>
#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Bitset.h>
#include <Common/EnumFlags.h>
#include <Common/Memory/Containers/ForwardDeclarations/StringView.h>

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

	struct KeyboardDeviceType final : public DeviceType
	{
		inline static constexpr Guid DeviceTypeGuid = "D757AEE0-C9BB-48F8-845F-3E13C5D45846"_guid;

		KeyboardDeviceType(const DeviceTypeIdentifier identifier, Input::Manager& manager);

		[[nodiscard]] DeviceIdentifier
		GetOrRegisterInstance(const uintptr platformIdentifier, Manager& manager, Optional<Rendering::Window*> pSourceWindow);
		[[nodiscard]] DeviceIdentifier FindInstance(const uintptr platformIdentifier, Manager& manager);

		void OnKeyDown(Input::DeviceIdentifier deviceIdentifier, Input::KeyboardInput);
		void OnKeyUp(Input::DeviceIdentifier deviceIdentifier, Input::KeyboardInput);
		void OnText(Input::DeviceIdentifier deviceIdentifier, const ConstUnicodeStringView);
		void OnActiveWindowReceivedKeyboardFocus();
		void OnActiveWindowLostKeyboardFocus();

		[[nodiscard]] InputIdentifier GetTextInputIdentifier() const
		{
			return m_textInputIdentifier;
		}

		[[nodiscard]] InputIdentifier GetInputIdentifier(const KeyboardInput input) const
		{
			return m_keyIdentifiers[input];
		}

		[[nodiscard]] EnumFlags<KeyboardModifier> GetActiveModifiers(Input::DeviceIdentifier deviceIdentifier) const;

		virtual void RestoreInputState(Monitor& monitor, const InputIdentifier inputIdentifier) const override;

		[[nodiscard]] virtual InputIdentifier DeserializeDeviceInput(const Serialization::Reader&) const override;

		[[nodiscard]] bool IsPressed(const Input::DeviceIdentifier deviceIdentifier, const Input::KeyboardInput input) const;
	protected:
		struct Keyboard final : public DeviceInstance
		{
			Keyboard(
				const DeviceIdentifier identifier,
				const DeviceTypeIdentifier typeIdentifier,
				Monitor* pActiveMonitor,
				const uintptr platformIdentifier
			)
				: DeviceInstance(identifier, typeIdentifier, pActiveMonitor)
				, m_platformIdentifier(platformIdentifier)
			{
			}

			virtual void OnMonitorChanged(Monitor& newMonitor, const DeviceType& deviceType, Manager&) override;
			virtual void
			RestoreInputState(Monitor& monitor, const DeviceType& deviceType, const InputIdentifier inputIdentifier, Manager&) override;

			uintptr m_platformIdentifier;
			Bitset<(UNDERLYING_TYPE(KeyboardInput))KeyboardInput::Count> m_keyStates;
		};

		void SyncKeyStates(Keyboard& keyboard);

		Manager& m_manager;
		InputIdentifier m_textInputIdentifier;
		Array<InputIdentifier, (UNDERLYING_TYPE(KeyboardInput))KeyboardInput::Count, KeyboardInput> m_keyIdentifiers;
	};
}
