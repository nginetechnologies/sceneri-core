#pragma once

#include "DeviceIdentifier.h"
#include "DeviceTypeIdentifier.h"
#include "InputIdentifier.h"
#include "ScreenCoordinate.h"
#include "Devices/Touchscreen/FingerIdentifier.h"
#include "Devices/Keyboard/KeyboardModifier.h"
#include "Devices/Keyboard/KeyboardInput.h"

#include <Common/Storage/Identifier.h>
#include <Common/Storage/ForwardDeclarations/IdentifierMask.h>
#include <Common/Math/Transform.h>
#include <Common/Math/Ratio.h>
#include <Common/Math/ForwardDeclarations/Radius.h>
#include <Common/Math/Length.h>
#include <Common/Math/Angle.h>
#include <Common/Math/RotationalSpeed.h>
#include <Common/EnumFlags.h>
#include <Common/Memory/Containers/StringView.h>

namespace ngine::Input
{
	struct Monitor
	{
		//! Touch radius used if the input API does not provide it
		inline static constexpr uint16 DefaultTouchRadius = 23;

		virtual ~Monitor();

		virtual void OnKeyboardInputDown(const DeviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
		{
		}

		virtual void
		OnKeyboardInputRepeat(const DeviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
		{
		}

		virtual void OnKeyboardInputUp(const DeviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
		{
		}

		virtual void
		OnKeyboardInputCancelled(const DeviceIdentifier, const InputIdentifier, const KeyboardInput, const EnumFlags<KeyboardModifier>)
		{
		}

		virtual void OnKeyboardInputRestored(const DeviceIdentifier, const IdentifierMask<InputIdentifier>&, const EnumFlags<KeyboardModifier>)
		{
		}

		virtual void OnKeyboardInputText(const DeviceIdentifier, const InputIdentifier, [[maybe_unused]] ConstUnicodeStringView text)
		{
		}

		virtual void OnBinaryInputDown(const DeviceIdentifier, const InputIdentifier)
		{
		}

		virtual void OnBinaryInputUp(const DeviceIdentifier, const InputIdentifier)
		{
		}

		virtual void OnBinaryInputCancelled(const DeviceIdentifier, const InputIdentifier)
		{
		}

		virtual void
		OnAnalogInput(const DeviceIdentifier, const InputIdentifier, [[maybe_unused]] const float newValue, [[maybe_unused]] const float delta)
		{
		}

		virtual void On2DAnalogInput(
			const DeviceIdentifier,
			const InputIdentifier,
			[[maybe_unused]] const Math::Vector2f newValue,
			[[maybe_unused]] const Math::Vector2f delta
		)
		{
		}

		virtual void On2DSurfaceHoveringMotionInput(
			const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate, [[maybe_unused]] const Math::Vector2i deltaCoordinate
		)
		{
		}

		virtual void On2DSurfaceDraggingMotionInput(
			const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate, [[maybe_unused]] const Math::Vector2i deltaCoordinate
		)
		{
		}

		virtual void On2DSurfaceStartPressInput(
			const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate, [[maybe_unused]] const uint8 numRepeats
		)
		{
		}

		virtual void On2DSurfaceStopPressInput(
			const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate, [[maybe_unused]] const uint8 numRepeats
		)
		{
		}

		virtual void On2DSurfaceCancelPressInput(const DeviceIdentifier, const InputIdentifier)
		{
		}

		virtual void On2DSurfaceStartTouchInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const FingerIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const Math::Ratiof pressureRatio,
			[[maybe_unused]] const uint16 radius
		)
		{
		}

		virtual void On2DSurfaceTouchMotionInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const FingerIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const Math::Vector2i deltaCoordinate,
			[[maybe_unused]] const Math::Ratiof pressureRatio,
			[[maybe_unused]] const uint16 radius
		)
		{
		}

		virtual void On2DSurfaceCancelTouchInput(const DeviceIdentifier, const InputIdentifier, const FingerIdentifier)
		{
		}

		virtual void On2DSurfaceStopTouchInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const FingerIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const Math::Ratiof pressureRatio,
			[[maybe_unused]] const uint16 radius
		)
		{
		}

		virtual void On2DSurfaceStartTapInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const uint16 touchRadius
		)
		{
		}
		virtual void On2DSurfaceStopTapInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const uint16 touchRadius
		)
		{
		}
		virtual void On2DSurfaceCancelTapInput(
			const DeviceIdentifier, const InputIdentifier, [[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		)
		{
		}

		virtual void On2DSurfaceDoubleTapInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const uint16 touchRadius
		)
		{
		}

		virtual void On2DSurfaceStartLongPressInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const uint16 touchRadius
		)
		{
		}
		virtual void On2DSurfaceLongPressMotionInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const uint16 touchRadius
		)
		{
		}
		virtual void On2DSurfaceStopLongPressInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const uint16 touchRadius
		)
		{
		}
		virtual void On2DSurfaceCancelLongPressInput(
			const DeviceIdentifier, const InputIdentifier, [[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		)
		{
		}

		virtual void On2DSurfaceStartPanInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const Math::Vector2f velocity,
			[[maybe_unused]] const uint16 touchRadius
		)
		{
		}

		virtual void On2DSurfacePanMotionInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const Math::Vector2f velocity,
			[[maybe_unused]] const uint16 touchRadius
		)
		{
		}

		virtual void On2DSurfaceStopPanInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const Math::Vector2f velocity
		)
		{
		}

		virtual void On2DSurfaceCancelPanInput(
			const DeviceIdentifier, const InputIdentifier, [[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		)
		{
		}

		virtual void On2DSurfaceStartPinchInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const float scale
		)
		{
		}

		virtual void On2DSurfacePinchMotionInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const float scale
		)
		{
		}

		virtual void On2DSurfaceStopPinchInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		)
		{
		}

		virtual void On2DSurfaceCancelPinchInput(
			const DeviceIdentifier, const InputIdentifier, [[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		)
		{
		}

		virtual void On2DSurfaceStartRotateInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		)
		{
		}

		virtual void On2DSurfaceRotateMotionInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Anglef,
			const Math::RotationalSpeedf
		)
		{
		}

		virtual void On2DSurfaceStopRotateInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			[[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Anglef,
			const Math::RotationalSpeedf
		)
		{
		}

		virtual void On2DSurfaceCancelRotateInput(
			const DeviceIdentifier, const InputIdentifier, [[maybe_unused]] const ArrayView<const FingerIdentifier, uint8> fingers
		)
		{
		}

		virtual void On2DSurfaceStartScrollInput(
			const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate, [[maybe_unused]] const Math::Vector2i delta
		)
		{
		}
		virtual void On2DSurfaceScrollInput(
			const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate, [[maybe_unused]] const Math::Vector2i delta
		)
		{
		}
		virtual void On2DSurfaceEndScrollInput(
			const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate, [[maybe_unused]] const Math::Vector2f velocity
		)
		{
		}
		virtual void On2DSurfaceCancelScrollInput(const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate)
		{
		}

		virtual void OnTransformInput(const DeviceIdentifier, const InputIdentifier, [[maybe_unused]] const Math::LocalTransform transform)
		{
		}

		virtual void OnReceiveDeviceFocus(const DeviceIdentifier, const DeviceTypeIdentifier)
		{
		}

		virtual void OnLoseDeviceFocus(const DeviceIdentifier, const DeviceTypeIdentifier)
		{
		}
	};
}
