#pragma once

#include <Common/Function/Function.h>

namespace ngine::Input
{
	struct LongPressAction final : public Action
	{
		Function<void(DeviceIdentifier, ScreenCoordinate, const uint8 fingerCount, Optional<uint16> touchRadius), 24> OnStartLongPress;
		Function<void(DeviceIdentifier, ScreenCoordinate, const uint8 fingerCount, Optional<uint16> touchRadius), 24> OnMoveLongPress;
		Function<void(DeviceIdentifier, ScreenCoordinate, const uint8 fingerCount, Optional<uint16> touchRadius), 24> OnEndLongPress;
		Function<void(DeviceIdentifier), 24> OnCancelLongPress;

		[[nodiscard]] virtual bool IsExclusive() const override
		{
			return true;
		}

		virtual void On2DSurfaceStartLongPressInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const uint16 touchRadius
		) override
		{
			OnStartLongPress(deviceIdentifier, coordinate, fingers.GetSize(), touchRadius);
		}

		virtual void On2DSurfaceLongPressMotionInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const uint16 touchRadius
		) override
		{
			OnMoveLongPress(deviceIdentifier, coordinate, fingers.GetSize(), touchRadius);
		}

		virtual void On2DSurfaceStopLongPressInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const uint16 touchRadius
		) override
		{
			OnEndLongPress(deviceIdentifier, coordinate, fingers.GetSize(), touchRadius);
		}

		virtual void On2DSurfaceCancelLongPressInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		) override
		{
			OnCancelLongPress(deviceIdentifier);
		}

		virtual void On2DSurfaceStartPressInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const uint8 numRepeats
		) override
		{
			OnStartLongPress(deviceIdentifier, coordinate, 1, Invalid);
		}

		virtual void On2DSurfaceStopPressInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const uint8 numRepeats
		) override
		{
			OnEndLongPress(deviceIdentifier, coordinate, 1, Invalid);
		}
	};
}
