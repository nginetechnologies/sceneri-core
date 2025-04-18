#pragma once

#include <Common/Function/Function.h>
#include <Engine/Input/Actions/Action.h>

namespace ngine::Input
{
	struct TapAction final : public Action
	{
		Function<void(DeviceIdentifier, ScreenCoordinate, const uint8 touchCount, Optional<uint16> touchRadius), 24> OnStartTap;
		Function<void(DeviceIdentifier, ScreenCoordinate, const uint8 touchCount, Optional<uint16> touchRadius), 24> OnTap;
		Function<void(DeviceIdentifier), 24> OnCancelTap;

		virtual void On2DSurfaceStartPressInput(
			const Input::DeviceIdentifier deviceIdentifier,
			const Input::InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const uint8 numRepeats
		) override
		{
			switch (numRepeats)
			{
				case 1:
					if (OnStartTap.IsValid())
					{
						OnStartTap(deviceIdentifier, coordinate, 1, Invalid);
					}
					break;
			}
		}
		virtual void On2DSurfaceStopPressInput(
			const Input::DeviceIdentifier deviceIdentifier,
			const Input::InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const uint8 numRepeats
		) override
		{
			switch (numRepeats)
			{
				case 1:
					OnTap(deviceIdentifier, coordinate, 1, Invalid);
					break;
			}
		}
		virtual void On2DSurfaceCancelPressInput(const Input::DeviceIdentifier deviceIdentifier, const Input::InputIdentifier) override
		{
			if (OnCancelTap.IsValid())
			{
				OnCancelTap(deviceIdentifier);
			}
		}

		virtual void On2DSurfaceStartTapInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const uint16 radius
		) override
		{
			if (fingers.GetSize() >= m_minimumTouchCount && fingers.GetSize() <= m_maximumTouchCount)
			{
				m_startedTap = true;
				if (OnStartTap.IsValid())
				{
					OnStartTap(deviceIdentifier, coordinate, fingers.GetSize(), radius);
				}
			}
		}
		virtual void On2DSurfaceStopTapInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const uint16 radius
		) override
		{
			if (m_startedTap)
			{
				m_startedTap = false;
				OnTap(deviceIdentifier, coordinate, fingers.GetSize(), radius);
			}
		}
		virtual void On2DSurfaceCancelTapInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		) override
		{
			if (m_startedTap)
			{
				m_startedTap = false;
				if (OnCancelTap.IsValid())
				{
					OnCancelTap(deviceIdentifier);
				}
			}
		}

		void SetMinimumTouchCount(const uint8 count)
		{
			m_minimumTouchCount = count;
		}
		[[nodiscard]] uint8 GetMinimumTouchCount() const
		{
			return m_minimumTouchCount;
		}
		void SetMaximumTouchCount(const uint8 count)
		{
			m_maximumTouchCount = count;
		}
		[[nodiscard]] uint8 GetMaximumTouchCount() const
		{
			return m_maximumTouchCount;
		}
	protected:
		uint8 m_minimumTouchCount = 1;
		uint8 m_maximumTouchCount = 1;
		bool m_startedTap = false;
	};
}
