#pragma once

#include "DialAction.h"
#include <Common/Math/Primitives/Rectangle.h>

namespace ngine::Input
{
	struct FloatingDialAction final : public DialAction
	{
		virtual void On2DSurfaceStartPanInput(
			const DeviceIdentifier deviceIdentifier,
			[[maybe_unused]] const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const Input::FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const Math::Vector2f velocity,
			const uint16 touchRadius
		) override
		{
			if (fingers.GetSize() == 1)
			{
				const Input::TouchscreenDeviceType::Touchscreen& touchscreen =
					static_cast<const Input::TouchscreenDeviceType::Touchscreen&>(m_pInputManager->GetDeviceInstance(deviceIdentifier));
				const Math::Vector2f relativeCoordinate = Math::Vector2f(coordinate) / Math::Vector2f(touchscreen.m_size);
				if (m_touchArea.HasSize())
				{
					if (!m_touchArea.Contains(relativeCoordinate))
					{
						return;
					}
				}
				m_circle.SetCenter(relativeCoordinate);
				m_startedPan = true;

				Math::Vector2f delta = Math::Vector2f(relativeCoordinate) - m_circle.GetCenter();
				delta.y = -delta.y;
				if (m_circle.GetRadius() > 0.0_units)
				{
					delta /= m_circle.GetRadius().GetUnits();
				}

				OnPan(deviceIdentifier, coordinate, delta, touchRadius);
			}
		}

		Math::Rectanglef m_touchArea;
	};
}
