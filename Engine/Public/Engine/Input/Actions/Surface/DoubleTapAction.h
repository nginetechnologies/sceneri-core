#pragma once

#include <Common/Function/Function.h>

namespace ngine::Input
{
	struct DoubleTapAction final : public Action
	{
		Function<void(DeviceIdentifier, ScreenCoordinate, const Optional<uint16> touchRadius), 24> OnDoubleTap;
		Function<void(), 24> OnCancelSingleTap = []()
		{
		};

		virtual void On2DSurfaceStopPressInput(
			const Input::DeviceIdentifier deviceIdentifier,
			const Input::InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const uint8 numRepeats
		) override
		{
			switch (numRepeats)
			{
				case 2:
					OnCancelSingleTap();
					OnDoubleTap(deviceIdentifier, coordinate, Invalid);
					break;
			}
		}

		virtual void On2DSurfaceDoubleTapInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			const uint16 radius
		) override
		{
			OnDoubleTap(deviceIdentifier, coordinate, radius);
		}
	};
}
