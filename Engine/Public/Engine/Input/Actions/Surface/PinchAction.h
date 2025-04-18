#pragma once

#include <Common/Function/Function.h>

namespace ngine::Input
{
	struct PinchAction final : public Action
	{
		Function<void(DeviceIdentifier, ScreenCoordinate, const float scale), 24> OnStartPinch =
			[](DeviceIdentifier, ScreenCoordinate, [[maybe_unused]] const float scale)
		{
		};
		Function<void(DeviceIdentifier, ScreenCoordinate, const Math::Vector2i deltaCoordinate, [[maybe_unused]] const float scale), 24>
			OnMovePinch =
				[](DeviceIdentifier, ScreenCoordinate, [[maybe_unused]] const Math::Vector2i deltaCoordinate, [[maybe_unused]] const float scale)
		{
		};
		Function<void(DeviceIdentifier, ScreenCoordinate), 24> OnEndPinch = [](DeviceIdentifier, ScreenCoordinate)
		{
		};
		Function<void(DeviceIdentifier), 24> OnCancelPinch = [](DeviceIdentifier)
		{
		};

		virtual void On2DSurfaceStartPinchInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			const float scale
		) override
		{
			OnStartPinch(deviceIdentifier, coordinate, scale);
			m_previousCoordinate = coordinate;
		}

		virtual void On2DSurfacePinchMotionInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			const float scale
		) override
		{
			const Math::Vector2i deltaCoordinate = (Math::Vector2i)coordinate - (Math::Vector2i)m_previousCoordinate;
			m_previousCoordinate = coordinate;

			OnMovePinch(deviceIdentifier, coordinate, deltaCoordinate, scale);
		}

		virtual void On2DSurfaceStopPinchInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		) override
		{
			OnEndPinch(deviceIdentifier, coordinate);
		}

		virtual void On2DSurfaceCancelPinchInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		) override
		{
			OnCancelPinch(deviceIdentifier);
		}
	protected:
		ScreenCoordinate m_previousCoordinate;
	};
}
