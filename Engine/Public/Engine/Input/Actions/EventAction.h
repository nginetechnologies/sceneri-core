#pragma once

#include <Engine/Input/Actions/Action.h>
#include <Common/Function/Function.h>

namespace ngine::Input::Actions
{
	struct Event final : public Action
	{
		Function<void(DeviceIdentifier), 24> OnStart{[](DeviceIdentifier)
		                                             {
																								 }};
		Function<void(DeviceIdentifier), 24> OnStop{[](DeviceIdentifier)
		                                            {
																								}};

		[[nodiscard]] bool IsActive() const
		{
			return m_state == State::Started;
		}

		void BindInput(ActionMonitor& monitor, const DeviceType& deviceType, const InputIdentifier inputIdentifier)
		{
			Action::BindInput(monitor, deviceType, inputIdentifier);
		}
		void UnbindInput(ActionMonitor& monitor, const InputIdentifier inputIdentifier)
		{
			Action::UnbindInput(monitor, inputIdentifier);
			m_state = State::Canceled;
		}
	protected:
		enum class State : uint8
		{
			Ended,
			Started,
			Canceled
		};

		virtual void
		OnKeyboardInputDown(const DeviceIdentifier deviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
			override
		{
			TryChangeValue(deviceIdentifier, State::Started);
		}

		virtual void
		OnKeyboardInputUp(const DeviceIdentifier deviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
			override
		{
			TryChangeValue(deviceIdentifier, State::Ended);
		}

		virtual void
		OnKeyboardInputCancelled(const DeviceIdentifier deviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
			override
		{
			TryChangeValue(deviceIdentifier, State::Canceled);
		}

		virtual void
		OnKeyboardInputRestored(const DeviceIdentifier deviceIdentifier, const IdentifierMask<InputIdentifier>&, const EnumFlags<KeyboardModifier>)
			override
		{
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

		virtual void On2DSurfaceStartLongPressInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const uint16 touchRadius
		) override
		{
			TryChangeValue(deviceIdentifier, State::Started);
		}

		virtual void On2DSurfaceStopLongPressInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const uint16 touchRadius
		) override
		{
			TryChangeValue(deviceIdentifier, State::Ended);
		}

		virtual void On2DSurfaceCancelLongPressInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		) override
		{
			TryChangeValue(deviceIdentifier, State::Canceled);
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
			[[maybe_unused]] const uint16 radius
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
			if (m_state != value)
			{
				m_state = value;
				if (value == State::Started)
				{
					OnStart(DeviceIdentifier(deviceIdentifier));
				}
				else if (value == State::Ended || (m_state == State::Started && value == State::Canceled))
				{
					OnStop(DeviceIdentifier(deviceIdentifier));
				}
			}
		}
	protected:
		State m_state = State::Canceled;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Input::Actions::Event>
	{
		inline static constexpr auto Type = Reflection::Reflect<Input::Actions::Event>(
			"{1FB85083-BB75-4373-ACCB-44E58408B620}"_guid,
			MAKE_UNICODE_LITERAL("Event Action"),
			TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization | TypeFlags::DisableDynamicInstantiation,
			Reflection::Tags{},
			Reflection::Properties{
				/// TOOD: Reflect the delegates
				/*Reflection::Property
		    {
		      MAKE_UNICODE_LITERAL("On Change"),
		      MAKE_UNICODE_LITERAL("Changed"),
		      MAKE_UNICODE_LITERAL("Changed"),
		      &DirectBinaryAction::m_worldTransform
		    }*/
			}
		);
	};
}
