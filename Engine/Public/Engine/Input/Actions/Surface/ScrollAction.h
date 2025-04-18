#pragma once

#include <Common/Function/Function.h>

namespace ngine::Input
{
	struct ScrollAction final : public Action
	{
		Function<void(DeviceIdentifier, ScreenCoordinate, const Math::Vector2i), 24> OnStartScroll =
			[](DeviceIdentifier, ScreenCoordinate, const Math::Vector2i)
		{
		};
		Function<void(DeviceIdentifier, ScreenCoordinate, const Math::Vector2i delta), 24> OnScroll =
			[](DeviceIdentifier, ScreenCoordinate, const Math::Vector2i)
		{
		};
		Function<void(DeviceIdentifier, ScreenCoordinate, const Math::Vector2f velocity), 24> OnEndScroll =
			[](DeviceIdentifier, ScreenCoordinate, const Math::Vector2f)
		{
		};
		Function<void(DeviceIdentifier, ScreenCoordinate), 24> OnCancelScroll = [](DeviceIdentifier, ScreenCoordinate)
		{
		};

		virtual void On2DSurfaceStartScrollInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const ScreenCoordinate coordinate, const Math::Vector2i delta
		) override
		{
			OnStartScroll(deviceIdentifier, coordinate, delta);
		}

		virtual void On2DSurfaceScrollInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const ScreenCoordinate coordinate, const Math::Vector2i delta
		) override
		{
			OnScroll(deviceIdentifier, coordinate, delta);
		}

		virtual void On2DSurfaceEndScrollInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier, const ScreenCoordinate coordinate, const Math::Vector2f velocity
		) override
		{
			OnEndScroll(deviceIdentifier, coordinate, velocity);
		}

		virtual void
		On2DSurfaceCancelScrollInput(const DeviceIdentifier deviceIdentifier, const InputIdentifier, const ScreenCoordinate coordinate) override
		{
			OnCancelScroll(deviceIdentifier, coordinate);
		}
	protected:
		ScreenCoordinate m_previousCoordinate;
	};
}
