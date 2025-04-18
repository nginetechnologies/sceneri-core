#pragma once

#include <Common/Math/Clamp.h>
#include <Common/Function/Function.h>
#include <Engine/Input/Actions/Action.h>

namespace ngine::Input
{
	// TODO: Split into two, one for 0 - 1
	// another -1 - 1, taking one analog and two binary inputs
	struct AnalogAction final : public Action
	{
		Function<void(DeviceIdentifier, float), 24> OnChanged;

		virtual void
		OnKeyboardInputDown(const DeviceIdentifier deviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
			override
		{
			TryChangeValue(deviceIdentifier, 1.f);
		}

		virtual void
		OnKeyboardInputUp(const DeviceIdentifier deviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
			override
		{
			TryChangeValue(deviceIdentifier, 0.f);
		}

		virtual void
		OnKeyboardInputCancelled(const DeviceIdentifier deviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
			override
		{
			TryChangeValue(deviceIdentifier, 0.f);
		}

		virtual void
		OnKeyboardInputRestored(const DeviceIdentifier deviceIdentifier, const IdentifierMask<InputIdentifier>&, const EnumFlags<KeyboardModifier>)
			override
		{
			TryChangeValue(deviceIdentifier, 1.f);
		}

		virtual void OnBinaryInputDown(const DeviceIdentifier deviceIdentifier, const InputIdentifier) override
		{
			TryChangeValue(deviceIdentifier, 1.f);
		}

		virtual void OnBinaryInputUp(const DeviceIdentifier deviceIdentifier, const InputIdentifier) override
		{
			TryChangeValue(deviceIdentifier, 0.f);
		}

		virtual void OnBinaryInputCancelled(const DeviceIdentifier deviceIdentifier, const InputIdentifier) override
		{
			TryChangeValue(deviceIdentifier, 0.f);
		}

		virtual void OnAnalogInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const float newValue, [[maybe_unused]] const float delta
		) override
		{
			m_value = newValue;

			if (OnChanged.IsValid())
			{
				if (delta != 0.f)
				{
					OnChanged(deviceIdentifier, newValue);
				}
			}
		}

		inline void TryChangeValue(const DeviceIdentifier deviceIdentifier, const float value)
		{
			const float previousValue = Math::Clamp(m_value, 0.f, 1.f);
			m_value += -1.f + ((float)value * 2.f);

			if (OnChanged.IsValid())
			{
				const float newValue = Math::Clamp(m_value, 0.f, 1.f);
				if (newValue != previousValue)
				{
					OnChanged(deviceIdentifier, newValue);
				}
			}
		}

		float m_value = 0.f;
	};
}
