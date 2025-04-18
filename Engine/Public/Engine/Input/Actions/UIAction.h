#pragma once

#include <Common/Function/Function.h>
#include <Common/Memory/Containers/Vector.h>
#include <Engine/Input/Actions/Action.h>

namespace ngine::Input
{
	struct UIAction final : public Action
	{
		Function<void(DeviceIdentifier, bool), 24> OnChanged{[](DeviceIdentifier, bool)
		                                                     {
																												 }};

		[[nodiscard]] bool IsActive() const
		{
			return m_state == State::Started;
		}
	protected:
		enum class State : uint8
		{
			Ended,
			Started,
			Canceled
		};

		virtual void OnKeyboardInputDown(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier> modifiers
		) override
		{
			if (!modifiers.IsEmpty())
			{
				return;
			}

			TryChangeValue(deviceIdentifier, State::Started);
		}

		virtual void OnKeyboardInputUp(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier> modifiers
		) override
		{
			if (!modifiers.IsEmpty())
			{
				return;
			}

			TryChangeValue(deviceIdentifier, State::Ended);
		}

		virtual void OnKeyboardInputCancelled(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier> modifiers
		) override
		{
			if (!modifiers.IsEmpty())
			{
				return;
			}

			TryChangeValue(deviceIdentifier, State::Canceled);
		}

		virtual void OnKeyboardInputRestored(
			const DeviceIdentifier deviceIdentifier, const IdentifierMask<InputIdentifier>&, const EnumFlags<KeyboardModifier> modifiers
		) override
		{
			if (!modifiers.IsEmpty())
			{
				return;
			}

			TryChangeValue(deviceIdentifier, State::Started);
		}

		virtual void OnBinaryInputDown(const DeviceIdentifier deviceIdentifier, const InputIdentifier) override
		{
			TryChangeValue(deviceIdentifier, State::Started);
		}

		virtual void OnBinaryInputUp(const DeviceIdentifier deviceIdentifier, const InputIdentifier) override
		{
			TryChangeValue(deviceIdentifier, State::Ended);
		}

		virtual void OnBinaryInputCancelled(const DeviceIdentifier deviceIdentifier, const InputIdentifier) override
		{
			TryChangeValue(deviceIdentifier, State::Canceled);
		}

		virtual void On2DSurfaceStartPressInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const ScreenCoordinate, [[maybe_unused]] const uint8 numRepeats
		) override
		{
			TryChangeValue(deviceIdentifier, State::Started);
		}

		virtual void On2DSurfaceStopPressInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const ScreenCoordinate, [[maybe_unused]] const uint8 numRepeats
		) override
		{
			TryChangeValue(deviceIdentifier, State::Ended);
		}

		virtual void On2DSurfaceCancelPressInput(const DeviceIdentifier deviceIdentifier, const InputIdentifier) override
		{
			TryChangeValue(deviceIdentifier, State::Canceled);
		}

		virtual void On2DSurfaceStartTouchInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const FingerIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const Math::Ratiof pressureRatio,
			[[maybe_unused]] const uint16 touchRadius
		) override
		{
			TryChangeValue(deviceIdentifier, State::Started);
		}

		virtual void On2DSurfaceStopTouchInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const FingerIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const Math::Ratiof pressureRatio,
			[[maybe_unused]] const uint16 touchRadius
		) override
		{
			TryChangeValue(deviceIdentifier, State::Ended);
		}

		virtual void
		On2DSurfaceCancelTouchInput(const DeviceIdentifier deviceIdentifier, const InputIdentifier, const FingerIdentifier) override
		{
			TryChangeValue(deviceIdentifier, State::Canceled);
		}

		inline void TryChangeValue(const DeviceIdentifier deviceIdentifier, const State value)
		{
			if (m_state == State::Started && value == State::Canceled)
			{
				m_state = State::Canceled;
				OnChanged(deviceIdentifier, false);

				return;
			}

			if (m_state != value)
			{
				m_state = value;
				OnChanged(deviceIdentifier, (bool)m_state);
			}
		}
	protected:
		State m_state = State::Canceled;
	};
}
