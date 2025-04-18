#pragma once

#include <Common/Function/Function.h>
#include <Engine/Input/Actions/Action.h>

#include <Common/Math/Primitives/Circle.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>

namespace ngine::Input
{
	struct DialAction : public Action
	{
		Function<
			void(
				DeviceIdentifier deviceIdentifier,
				ScreenCoordinate coordinate,
				const Math::Vector2f distanceFromCenter,
				const Optional<uint16> touchRadius
			),
			24>
			OnPan;

		/*virtual void On2DSurfaceStartPressInput(const Input::DeviceIdentifier, const Input::InputIdentifier, const ScreenCoordinate
		coordinate,
		[[maybe_unused]] const uint8 numRepeats) override
		{
		  m_startHoldCoordinate = coordinate;
		}

		virtual void On2DSurfaceStopPressInput(const Input::DeviceIdentifier deviceIdentifier, const Input::InputIdentifier, const
		ScreenCoordinate coordinate, [[maybe_unused]] const uint8 numRepeats) override
		{
		  if (!m_startedPan)
		  {
		    return;
		  }

		        OnPan(deviceIdentifier, coordinate, Math::Zero, Invalid);
		  m_startedPan = false;
		}

		virtual void On2DSurfaceCancelPressInput(const Input::DeviceIdentifier deviceIdentifier, const Input::InputIdentifier) override
		{
		  if (m_startedPan)
		  {
		            OnPan(deviceIdentifier, Math::Zero, Math::Zero, Invalid);
		    m_startedPan = false;
		  }
		}

		virtual void On2DSurfaceDraggingMotionInput(const DeviceIdentifier deviceIdentifier, const InputIdentifier, const ScreenCoordinate
		coordinate, const Math::Vector2i deltaCoordinate) override
		{
		  if (m_startedPan)
		  {
		    OnMovePan(deviceIdentifier, coordinate, deltaCoordinate, Math::Zero, Invalid);
		  }
		  else
		  {
		    const Math::Vector2i diff = Math::Vector2i(coordinate) - Math::Vector2i(m_startHoldCoordinate);

		    constexpr int startDragEpsilon = 5;
		    if (Math::Abs(diff.x) + Math::Abs(diff.y) > startDragEpsilon)
		    {
		      m_startedPan = true;
		      OnStartPan(deviceIdentifier, coordinate, Math::Zero, Invalid);
		    }
		  }
		}*/

		[[nodiscard]] virtual bool IsExclusive() const override
		{
			return m_startedPan;
		}

		virtual void On2DSurfaceStartPanInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
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
				const bool startedPanning = m_circle.Contains(relativeCoordinate);
				m_startedPan = startedPanning;
				if (startedPanning)
				{
					Math::Vector2f delta = Math::Vector2f(relativeCoordinate) - m_circle.GetCenter();
					delta.y = -delta.y;

					if (m_circle.GetRadius() > 0.0_units)
					{
						delta /= m_circle.GetRadius().GetUnits();
					}

					OnPan(deviceIdentifier, coordinate, delta, touchRadius);
				}
			}
		}

		virtual void On2DSurfacePanMotionInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const Input::FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const Math::Vector2f velocity,
			const uint16 touchRadius
		) override
		{
			if (m_startedPan)
			{
				const Input::TouchscreenDeviceType::Touchscreen& touchscreen =
					static_cast<const Input::TouchscreenDeviceType::Touchscreen&>(m_pInputManager->GetDeviceInstance(deviceIdentifier));

				const Math::Vector2f relativeCoordinate = Math::Vector2f(coordinate) / Math::Vector2f(touchscreen.m_size);
				Math::Vector2f delta = Math::Vector2f(relativeCoordinate) - m_circle.GetCenter();
				delta.y = -delta.y;

				if (m_circle.GetRadius() > 0.0_units)
				{
					delta /= m_circle.GetRadius().GetUnits();
				}

				OnPan(deviceIdentifier, coordinate, delta, touchRadius);
			}
		}

		virtual void On2DSurfaceStopPanInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const Input::FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const Math::Vector2f velocity
		) override
		{
			if (m_startedPan)
			{
				m_startedPan = false;
				OnPan(deviceIdentifier, coordinate, Math::Zero, Invalid);
			}
		}

		virtual void On2DSurfaceCancelPanInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			[[maybe_unused]] const ArrayView<const Input::FingerIdentifier, uint8> fingers
		) override
		{
			if (m_startedPan)
			{
				m_startedPan = false;
				OnPan(deviceIdentifier, Math::Zero, Math::Zero, Invalid);
			}
		}

		Optional<Input::Manager*> m_pInputManager;

		Math::Circlef m_circle{{0.5f, 0.5f}, 0.1_units};
	protected:
		bool m_startedPan = false;
	};
}
