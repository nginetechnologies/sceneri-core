#pragma once

#include <Engine/Input/Monitor.h>
#include <Engine/Input/DeviceType.h>

#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Storage/IdentifierArray.h>

namespace ngine::Widgets::Document
{
	struct Scene3D;
}

namespace ngine::Input
{
	struct Manager;
	struct Action;

	struct ActionMonitor : public Monitor
	{
		using ActionContainer = Vector<ReferenceWrapper<Action>>;
		using BoundInputActions = TIdentifierArray<ActionContainer, InputIdentifier>;
		virtual ~ActionMonitor() = default;

		void ClearBoundActions(Manager& manager);
		[[nodiscard]] BoundInputActions::ConstRestrictedView GetBoundActions() const
		{
			return m_boundInputActions.GetView();
		}
	protected:
		// Monitor
		virtual void OnKeyboardInputDown(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const KeyboardInput input,
			const EnumFlags<KeyboardModifier> modifiers
		) override;
		virtual void OnKeyboardInputRepeat(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const KeyboardInput input,
			const EnumFlags<KeyboardModifier> modifiers
		) override;
		virtual void OnKeyboardInputUp(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const KeyboardInput input,
			const EnumFlags<KeyboardModifier> modifiers
		) override;
		virtual void OnKeyboardInputCancelled(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const KeyboardInput input,
			const EnumFlags<KeyboardModifier> modifiers
		) override;
		virtual void OnKeyboardInputRestored(
			const DeviceIdentifier deviceIdentifier, const IdentifierMask<InputIdentifier>&, const EnumFlags<KeyboardModifier> modifiers
		) override;
		virtual void OnKeyboardInputText(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier textInputIdentifier, ConstUnicodeStringView text
		) override;
		virtual void OnBinaryInputDown(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier) override;
		virtual void OnBinaryInputUp(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier) override;
		virtual void OnBinaryInputCancelled(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier) override;
		virtual void OnAnalogInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const float newValue, const float delta
		) override;
		virtual void On2DAnalogInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const Math::Vector2f newValue,
			const Math::Vector2f delta
		) override;
		virtual void On2DSurfaceDraggingMotionInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const Math::Vector2i deltaCoordinate
		) override;
		virtual void On2DSurfaceHoveringMotionInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const Math::Vector2i deltaCoordinate
		) override;
		virtual void On2DSurfaceStartPressInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const uint8 numRepeats
		) override;
		virtual void On2DSurfaceStopPressInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const uint8 numRepeats
		) override;
		virtual void On2DSurfaceCancelPressInput(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifie) override;
		virtual void On2DSurfaceStartTouchInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const FingerIdentifier fingerIdentifier,
			const ScreenCoordinate coordinate,
			const Math::Ratiof pressureRatio,
			const uint16 radius
		) override;
		virtual void On2DSurfaceTouchMotionInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const FingerIdentifier fingerIdentifier,
			const ScreenCoordinate coordinate,
			const Math::Vector2i deltaCoordinate,
			const Math::Ratiof pressureRatio,
			const uint16 radius
		) override;
		virtual void On2DSurfaceStopTouchInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const FingerIdentifier fingerIdentifier,
			const ScreenCoordinate coordinate,
			const Math::Ratiof pressureRatio,
			const uint16 radius
		) override;
		virtual void On2DSurfaceCancelTouchInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const FingerIdentifier fingerIdentifier
		) override;
		virtual void On2DSurfaceStartTapInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const uint16 radius
		) override;
		virtual void On2DSurfaceStopTapInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const uint16 radius
		) override;
		virtual void On2DSurfaceCancelTapInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers
		) override;
		virtual void On2DSurfaceDoubleTapInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const uint16 radius
		) override;
		virtual void On2DSurfaceStartLongPressInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const uint16 radius
		) override;
		virtual void On2DSurfaceLongPressMotionInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const uint16 touchRadius
		) override;
		virtual void On2DSurfaceStopLongPressInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			[[maybe_unused]] const uint16 touchRadius
		) override;
		virtual void On2DSurfaceCancelLongPressInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers
		) override;
		virtual void On2DSurfaceStartPanInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Vector2f velocity,
			const uint16 radius
		) override;
		virtual void On2DSurfacePanMotionInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Vector2f velocity,
			const uint16 touchRadius
		) override;
		virtual void On2DSurfaceStopPanInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Vector2f velocity
		) override;
		virtual void On2DSurfaceCancelPanInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers
		) override;
		virtual void On2DSurfaceStartPinchInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const float scale
		) override;
		virtual void On2DSurfacePinchMotionInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const float scale
		) override;
		virtual void On2DSurfaceStopPinchInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers
		) override;
		virtual void On2DSurfaceCancelPinchInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers
		) override;
		virtual void On2DSurfaceStartRotateInput(
			const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate, const ArrayView<const FingerIdentifier, uint8> fingers
		) override;
		virtual void On2DSurfaceRotateMotionInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Anglef,
			const Math::RotationalSpeedf
		) override;
		virtual void On2DSurfaceStopRotateInput(
			const DeviceIdentifier,
			const InputIdentifier,
			const ScreenCoordinate,
			const ArrayView<const FingerIdentifier, uint8> fingers,
			const Math::Anglef,
			const Math::RotationalSpeedf
		) override;
		virtual void On2DSurfaceCancelRotateInput(
			const DeviceIdentifier, const InputIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers
		) override;
		virtual void
		On2DSurfaceStartScrollInput(const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate, const Math::Vector2i delta) override;
		virtual void On2DSurfaceScrollInput(
			const DeviceIdentifier deviceIdentifier,
			const InputIdentifier inputIdentifier,
			const ScreenCoordinate coordinate,
			const Math::Vector2i delta
		) override;
		virtual void On2DSurfaceEndScrollInput(
			const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate, [[maybe_unused]] const Math::Vector2f velocity
		) override;
		virtual void On2DSurfaceCancelScrollInput(const DeviceIdentifier, const InputIdentifier, const ScreenCoordinate) override;
		virtual void OnTransformInput(
			const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const Math::LocalTransform transform
		) override;
		// ~Monitor
	private:
		void BindAction(Action& action, const DeviceType& deviceType, const InputIdentifier input)
		{
			Assert(!m_boundInputActions[input].Contains(action));
			Assert(input.IsValid());
			m_boundInputActions[input].EmplaceBack(action);

			deviceType.RestoreInputState(*this, input);
		}

		bool UnbindAction(Action& action, const InputIdentifier input)
		{
			return m_boundInputActions[input].RemoveFirstOccurrence(action);
		}

		friend Widgets::Document::Scene3D;
		friend Action;
	protected:
		BoundInputActions m_boundInputActions{Memory::Zeroed};
	};
}
