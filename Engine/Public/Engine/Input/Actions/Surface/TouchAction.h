#pragma once

#include <Common/Function/Function.h>
#include <Engine/Input/Actions/Action.h>

namespace ngine::Input
{
	struct TouchAction final : public Action
	{
		Function<
			void(
				DeviceIdentifier deviceIdentifier,
				FingerIdentifier fingerIdentifier,
				ScreenCoordinate screenCoordinate,
				Math::Ratiof pressureRatio,
				uint16 touchRadius
			),
			24>
			OnStartTouch;
		Function<
			void(
				DeviceIdentifier deviceIdentifier,
				FingerIdentifier fingerIdentifier,
				ScreenCoordinate screenCoordinate,
				Math::Vector2i deltaCoordinate,
				Math::Ratiof pressureRatio,
				uint16 touchRadius
			),
			24>
			OnMoveTouch;
		Function<
			void(
				DeviceIdentifier deviceIdentifier,
				FingerIdentifier fingerIdentifier,
				ScreenCoordinate screenCoordinate,
				Math::Ratiof pressureRatio,
				uint16 touchRadius
			),
			24>
			OnStopTouch;
		Function<void(DeviceIdentifier deviceIdentifier, FingerIdentifier fingerIdentifier), 24> OnCancelTouch;

		virtual void On2DSurfaceStartTouchInput(
			const DeviceIdentifier deviceIdentifier,
			[[maybe_unused]] const InputIdentifier inputIdentifier,
			const FingerIdentifier fingerIdentifier,
			const ScreenCoordinate screenCoordinate,
			const Math::Ratiof pressureRatio,
			const uint16 radius
		) override
		{
			if (OnStartTouch.IsValid())
			{
				OnStartTouch(deviceIdentifier, fingerIdentifier, screenCoordinate, pressureRatio, radius);
			}
		}

		virtual void On2DSurfaceTouchMotionInput(
			const DeviceIdentifier deviceIdentifier,
			[[maybe_unused]] const InputIdentifier inputIdentifier,
			const FingerIdentifier fingerIdentifier,
			const ScreenCoordinate screenCoordinate,
			const Math::Vector2i deltaCoordinate,
			const Math::Ratiof pressureRatio,
			const uint16 radius
		) override
		{
			if (OnMoveTouch.IsValid())
			{
				OnMoveTouch(deviceIdentifier, fingerIdentifier, screenCoordinate, deltaCoordinate, pressureRatio, radius);
			}
		}

		virtual void On2DSurfaceStopTouchInput(
			const DeviceIdentifier deviceIdentifier,
			[[maybe_unused]] const InputIdentifier inputIdentifier,
			const FingerIdentifier fingerIdentifier,
			const ScreenCoordinate screenCoordinate,
			const Math::Ratiof pressureRatio,
			const uint16 radius
		) override
		{
			if (OnStopTouch.IsValid())
			{
				OnStopTouch(deviceIdentifier, fingerIdentifier, screenCoordinate, pressureRatio, radius);
			}
		}

		virtual void On2DSurfaceCancelTouchInput(
			const DeviceIdentifier deviceIdentifier,
			[[maybe_unused]] const InputIdentifier inputIdentifier,
			const FingerIdentifier fingerIdentifier
		) override
		{
			if (OnCancelTouch.IsValid())
			{
				OnCancelTouch(deviceIdentifier, fingerIdentifier);
			}
		}

		virtual void On2DSurfaceStartPressInput(
			const Input::DeviceIdentifier deviceIdentifier,
			[[maybe_unused]] const Input::InputIdentifier inputIdentifier,
			const ScreenCoordinate screenCoordinate,
			[[maybe_unused]] const uint8 numRepeats
		) override
		{
			if (OnStartTouch.IsValid())
			{
				OnStartTouch(deviceIdentifier, Input::FingerIdentifier(0), screenCoordinate, Math::Ratiof(1.f), DefaultTouchRadius);
			}
		}

		virtual void On2DSurfaceStopPressInput(
			const Input::DeviceIdentifier deviceIdentifier,
			[[maybe_unused]] const Input::InputIdentifier inputIdentifier,
			const ScreenCoordinate screenCoordinate,
			[[maybe_unused]] const uint8 numRepeats
		) override
		{
			if (OnStopTouch.IsValid())
			{
				OnStopTouch(deviceIdentifier, Input::FingerIdentifier(0), screenCoordinate, Math::Ratiof(1.f), DefaultTouchRadius);
			}
		}

		virtual void On2DSurfaceDraggingMotionInput(
			const DeviceIdentifier deviceIdentifier,
			[[maybe_unused]] const InputIdentifier inputIdentifier,
			const ScreenCoordinate screenCoordinate,
			const Math::Vector2i deltaCoordinate
		) override
		{
			if (OnMoveTouch.IsValid())
			{
				OnMoveTouch(deviceIdentifier, Input::FingerIdentifier(0), screenCoordinate, deltaCoordinate, Math::Ratiof(1.f), DefaultTouchRadius);
			}
		}

		virtual void On2DSurfaceCancelPressInput(
			const Input::DeviceIdentifier deviceIdentifier, [[maybe_unused]] const Input::InputIdentifier inputIdentifier
		) override
		{
			if (OnCancelTouch.IsValid())
			{
				OnCancelTouch(deviceIdentifier, Input::FingerIdentifier(0));
			}
		}
	};
}
