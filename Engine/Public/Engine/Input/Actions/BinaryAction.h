#pragma once

#include <Common/Function/Function.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Storage/IdentifierMask.h>
#include <Engine/Input/Actions/Action.h>

namespace ngine::Input
{
	struct BinaryAction final : public Action
	{
		Function<void(DeviceIdentifier, bool), 24> OnChanged;

		[[nodiscard]] bool IsActive() const
		{
			return m_state == State::Started;
		}

		void BindInput(
			ActionMonitor& monitor,
			const DeviceType& deviceType,
			const InputIdentifier inputIdentifier,
			const EnumFlags<KeyboardModifier> requiredModifiers
		)
		{
			Action::BindInput(monitor, deviceType, inputIdentifier);

			if (requiredModifiers.AreAnySet())
			{
				m_requiredInputModifiers.Emplace(InputIdentifier(inputIdentifier), EnumFlags<KeyboardModifier>(requiredModifiers));
				if (m_pActiveInputs.IsInvalid())
				{
					m_pActiveInputs.CreateInPlace();
				}
			}
		}
		void BindInput(ActionMonitor& monitor, const DeviceType& deviceType, const InputIdentifier inputIdentifier)
		{
			Action::BindInput(monitor, deviceType, inputIdentifier);
		}
		void UnbindInput(ActionMonitor& monitor, const InputIdentifier inputIdentifier)
		{
			m_state = State::Canceled;
			Action::UnbindInput(monitor, inputIdentifier);
			if (auto it = m_requiredInputModifiers.Find(inputIdentifier); it != m_requiredInputModifiers.end())
			{
				m_requiredInputModifiers.Remove(it);
			}
		}
	protected:
		enum class State : uint8
		{
			Ended,
			Started,
			Canceled
		};

		virtual void OnKeyboardInputDown(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const KeyboardInput,
			const EnumFlags<KeyboardModifier> modifiers
		) override
		{
			const auto it = m_requiredInputModifiers.Find(inputIdentifier);
			if (it != m_requiredInputModifiers.end())
			{
				const EnumFlags<KeyboardModifier> requiredModifiers = it->second;
				if (!modifiers.AreAllSet(requiredModifiers))
				{
					return;
				}
			}

			if (m_pActiveInputs != nullptr)
			{
				m_pActiveInputs->Set(inputIdentifier);
			}

			TryChangeValue(deviceIdentifier, State::Started);
		}

		virtual void
		OnKeyboardInputUp(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
			override
		{
			if (m_pActiveInputs != nullptr && !m_pActiveInputs->IsSet(inputIdentifier))
			{
				return;
			}

			TryChangeValue(deviceIdentifier, State::Ended);
		}

		virtual void
		OnKeyboardInputCancelled(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
			override
		{
			if (m_pActiveInputs != nullptr && !m_pActiveInputs->IsSet(inputIdentifier))
			{
				return;
			}

			TryChangeValue(deviceIdentifier, State::Canceled);
		}

		virtual void OnKeyboardInputRestored(
			const DeviceIdentifier deviceIdentifier, const IdentifierMask<InputIdentifier>& inputs, const EnumFlags<KeyboardModifier> modifiers
		) override
		{
			for (const typename InputIdentifier::IndexType inputIdentifierIndex : inputs.GetSetBitsIterator())
			{
				const InputIdentifier inputIdentifier = InputIdentifier::MakeFromValidIndex(inputIdentifierIndex);
				const auto it = m_requiredInputModifiers.Find(inputIdentifier);
				if (it != m_requiredInputModifiers.end())
				{
					const EnumFlags<KeyboardModifier> requiredModifiers = it->second;
					if (!modifiers.AreAllSet(requiredModifiers))
					{
						return;
					}
				}

				if (m_pActiveInputs != nullptr)
				{
					m_pActiveInputs->Set(inputIdentifier);
				}
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

		virtual void OnAnalogInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const float newValue, [[maybe_unused]] const float delta
		) override
		{
			if (newValue > 0)
			{
				TryChangeValue(deviceIdentifier, State::Started);
			}
			else
			{
				TryChangeValue(deviceIdentifier, State::Ended);
			}
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
			[[maybe_unused]] const uint16 radius
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
				OnChanged(deviceIdentifier, m_state == State::Started);
			}
		}
	protected:
		State m_state = State::Canceled;
		UnorderedMap<InputIdentifier, EnumFlags<KeyboardModifier>, InputIdentifier::Hash> m_requiredInputModifiers;
		UniquePtr<IdentifierMask<InputIdentifier>> m_pActiveInputs;
	};
}
