#pragma once

#include <Common/Function/Function.h>

namespace ngine::Input
{
	struct Vector2DeltaAction final : public Action
	{
		Function<void(DeviceIdentifier, Math::Vector2i), 24> OnChanged;
	protected:
		virtual void On2DSurfaceScrollInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const ScreenCoordinate, const Math::Vector2i delta
		) override
		{
			OnChanged(deviceIdentifier, delta);
		}

		virtual void On2DSurfaceHoveringMotionInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const ScreenCoordinate, const Math::Vector2i deltaCoordinate
		) override
		{
			OnChanged(deviceIdentifier, deltaCoordinate);
		}

		virtual void On2DSurfaceDraggingMotionInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const ScreenCoordinate, const Math::Vector2i deltaCoordinate
		) override
		{
			OnChanged(deviceIdentifier, deltaCoordinate);
		}

		virtual void On2DSurfaceStartPanInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const Math::Vector2f velocity,
			[[maybe_unused]] const uint16 radius
		) override
		{
			m_startCoordinate = coordinate;
		}

		virtual void On2DSurfacePanMotionInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			[[maybe_unused]] const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const Math::Vector2f velocity,
			[[maybe_unused]] const uint16 touchRadius
		) override
		{
			const Math::Vector2i delta = Math::Vector2i(coordinate) - Math::Vector2i(m_startCoordinate);
			OnChanged(deviceIdentifier, delta);
		}
	protected:
		ScreenCoordinate m_startCoordinate;
	};
}
