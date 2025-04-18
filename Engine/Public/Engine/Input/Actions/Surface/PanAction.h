#pragma once

#include <Common/Function/Function.h>
#include <Engine/Input/Actions/Action.h>

namespace ngine::Input
{
	struct PanAction final : public Action
	{
		Function<
			void(
				DeviceIdentifier deviceIdentifier,
				ScreenCoordinate coordinate,
				const uint8 fingerCount,
				const Math::Vector2f velocity,
				const Optional<uint16> touchRadius
			),
			24>
			OnStartPan = [](DeviceIdentifier, ScreenCoordinate, const uint8, const Math::Vector2f, const Optional<uint16>)
		{
		};
		Function<
			void(
				DeviceIdentifier deviceIdentifier,
				ScreenCoordinate,
				const Math::Vector2i deltaCoordinate,
				const uint8 fingerCount,
				const Math::Vector2f velocity,
				const Optional<uint16> touchRadius
			),
			24>
			OnMovePan = [](DeviceIdentifier, ScreenCoordinate, const Math::Vector2i, const uint8, const Math::Vector2f, const Optional<uint16>)
		{
		};
		Function<void(DeviceIdentifier deviceIdentifier, ScreenCoordinate coordinate, const Math::Vector2f velocity), 24> OnEndPan;
		Function<void(DeviceIdentifier), 24> OnCancelPan = [](DeviceIdentifier)
		{
		};

		virtual void On2DSurfaceStartPressInput(
			const Input::DeviceIdentifier,
			const Input::InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const uint8 numRepeats
		) override
		{
			m_startHoldCoordinate = coordinate;
		}

		virtual void On2DSurfaceStopPressInput(
			const Input::DeviceIdentifier deviceIdentifier,
			const Input::InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const uint8 numRepeats
		) override
		{
			if (m_startedPan)
			{
				m_startedPan = false;
				OnEndPan(deviceIdentifier, coordinate, Math::Zero);
			}
		}

		virtual void On2DSurfaceCancelPressInput(const Input::DeviceIdentifier deviceIdentifier, const Input::InputIdentifier) override
		{
			if (m_startedPan)
			{
				m_startedPan = false;
				OnCancelPan(deviceIdentifier);
			}
		}

		virtual void On2DSurfaceDraggingMotionInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			const Math::Vector2i deltaCoordinate
		) override
		{
			if (m_minimumFingerCount == 1)
			{
				if (m_startedPan)
				{
					OnMovePan(deviceIdentifier, coordinate, deltaCoordinate, 1, Math::Zero, Invalid);
				}
				else
				{
					const Math::Vector2i delta = Math::Vector2i(coordinate) - Math::Vector2i(m_startHoldCoordinate);

					constexpr int startDragEpsilon = 5;
					if (Math::Abs(delta.x) + Math::Abs(delta.y) > startDragEpsilon)
					{
						m_startedPan = true;
						OnStartPan(deviceIdentifier, coordinate, 1, (Math::Vector2f)delta, Invalid);
					}
				}
			}
		}

		[[nodiscard]] virtual bool IsExclusive() const override
		{
			return m_startedPan;
		}

		virtual void On2DSurfaceStartPanInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Vector2f velocity,
			const uint16 radius
		) override
		{
			if (fingers.GetSize() >= m_minimumFingerCount && fingers.GetSize() <= m_maximumFingerCount)
			{
				m_startedPan = true;
				m_startHoldCoordinate = coordinate;
				OnStartPan(deviceIdentifier, coordinate, fingers.GetSize(), velocity, radius);
			}
		}

		virtual void On2DSurfacePanMotionInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Vector2f velocity,
			const uint16 touchRadius
		) override
		{
			if (m_startedPan)
			{
				if (fingers.GetSize() >= m_minimumFingerCount && fingers.GetSize() <= m_maximumFingerCount)
				{
					const Math::Vector2i delta = Math::Vector2i(coordinate) - Math::Vector2i(m_startHoldCoordinate);

					OnMovePan(deviceIdentifier, coordinate, delta, fingers.GetSize(), velocity, touchRadius);
				}
				else
				{
					m_startedPan = false;
					OnCancelPan(deviceIdentifier);
				}
			}
			else if (fingers.GetSize() >= m_minimumFingerCount && fingers.GetSize() <= m_maximumFingerCount)
			{
				m_startedPan = true;
				m_startHoldCoordinate = coordinate;
				OnStartPan(deviceIdentifier, coordinate, fingers.GetSize(), velocity, touchRadius);
			}
		}

		virtual void On2DSurfaceStopPanInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate coordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Vector2f velocity
		) override
		{
			if (m_startedPan)
			{
				OnEndPan(deviceIdentifier, coordinate, velocity);
			}
		}

		virtual void On2DSurfaceCancelPanInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		) override
		{
			if (m_startedPan)
			{
				OnCancelPan(deviceIdentifier);
			}
		}

		void SetRequiredFingerCount(const uint8 minimumFingerCount, const uint8 maximumFingerCount)
		{
			m_minimumFingerCount = minimumFingerCount;
			m_maximumFingerCount = maximumFingerCount;
		}
	protected:
		bool m_startedPan = false;
		uint8 m_minimumFingerCount = 1;
		uint8 m_maximumFingerCount = 1;
		ScreenCoordinate m_startHoldCoordinate;
	};
}
