#pragma once

#include <Common/Function/Function.h>
#include <Engine/Input/Actions/Action.h>

namespace ngine::Input
{
	struct HoverAction final : public Action
	{
		Function<void(DeviceIdentifier, ScreenCoordinate, const Math::Vector2i deltaCoordinate), 24> OnHover;

		virtual void On2DSurfaceHoveringMotionInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			const Math::Vector2i deltaCoordinate
		) override
		{
			OnHover(deviceIdentifier, coordinate, deltaCoordinate);
		}
	};
}
