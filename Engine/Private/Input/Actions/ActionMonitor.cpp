#include "Input/Actions/ActionMonitor.h"
#include "Input/Actions/Action.h"

#include "Input/Actions/Action.h"
#include "Input/Actions/EventAction.h"
#include "Input/Actions/Vector2Action.h"
#include "Input/Actions/Vector3Action.h"

#include "Input/InputManager.h"

#include <Common/Storage/IdentifierMask.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Input
{
	[[maybe_unused]] const bool wasActionRegistered = Reflection::Registry::RegisterType<Input::Action>();
	[[maybe_unused]] const bool wasActionEventRegistered = Reflection::Registry::RegisterType<Input::Actions::Event>();
	[[maybe_unused]] const bool wasVector2EventRegistered = Reflection::Registry::RegisterType<Input::Vector2Action>();
	[[maybe_unused]] const bool wasVector3EventRegistered = Reflection::Registry::RegisterType<Input::Vector3Action>();

	void Action::BindInput(ActionMonitor& monitor, const DeviceType& deviceType, const InputIdentifier inputIdentifier)
	{
		monitor.BindAction(*this, deviceType, inputIdentifier);
	}

	void Action::UnbindInput(ActionMonitor& monitor, const InputIdentifier inputIdentifier)
	{
		monitor.UnbindAction(*this, inputIdentifier);
	}

	void ActionMonitor::ClearBoundActions(Input::Manager& manager)
	{
		for (ActionContainer& boundInputs : manager.GetValidInputElementView(m_boundInputActions.GetView()))
		{
			boundInputs.Clear();
		}
	}

	void ActionMonitor::OnKeyboardInputDown(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const KeyboardInput input,
		const EnumFlags<KeyboardModifier> modifiers
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.OnKeyboardInputDown(deviceIdentifier, inputIdentifier, input, modifiers);
		}
	}

	void ActionMonitor::OnKeyboardInputRepeat(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const KeyboardInput input,
		const EnumFlags<KeyboardModifier> modifiers
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.OnKeyboardInputRepeat(deviceIdentifier, inputIdentifier, input, modifiers);
		}
	}

	void ActionMonitor::OnKeyboardInputUp(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const KeyboardInput input,
		const EnumFlags<KeyboardModifier> modifiers
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.OnKeyboardInputUp(deviceIdentifier, inputIdentifier, input, modifiers);
		}
	}

	void ActionMonitor::OnKeyboardInputCancelled(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const KeyboardInput input,
		const EnumFlags<KeyboardModifier> modifiers
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.OnKeyboardInputCancelled(deviceIdentifier, inputIdentifier, input, modifiers);
		}
	}

	void ActionMonitor::OnKeyboardInputRestored(
		const DeviceIdentifier deviceIdentifier, const IdentifierMask<InputIdentifier>& inputs, const EnumFlags<KeyboardModifier> modifiers
	)
	{
		for (const typename InputIdentifier::IndexType inputIdentifierIndex : inputs.GetSetBitsIterator())
		{
			const InputIdentifier inputIdentifier = InputIdentifier::MakeFromValidIndex(inputIdentifierIndex);
			for (Action& action : m_boundInputActions[inputIdentifier])
			{
				action.OnKeyboardInputRestored(deviceIdentifier, inputs, modifiers);
			}
		}
	}

	void ActionMonitor::OnKeyboardInputText(
		const DeviceIdentifier deviceIdentifier, const InputIdentifier textInputIdentifier, ConstUnicodeStringView text
	)
	{
		for (Action& action : m_boundInputActions[textInputIdentifier])
		{
			action.OnKeyboardInputText(deviceIdentifier, textInputIdentifier, text);
		}
	}

	void ActionMonitor::OnBinaryInputDown(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.OnBinaryInputDown(deviceIdentifier, inputIdentifier);
		}
	}

	void ActionMonitor::OnBinaryInputUp(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.OnBinaryInputUp(deviceIdentifier, inputIdentifier);
		}
	}

	void ActionMonitor::OnBinaryInputCancelled(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.OnBinaryInputCancelled(deviceIdentifier, inputIdentifier);
		}
	}

	void ActionMonitor::OnAnalogInput(
		const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const float newValue, const float delta
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.OnAnalogInput(deviceIdentifier, inputIdentifier, newValue, delta);
		}
	}

	void ActionMonitor::On2DAnalogInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const Math::Vector2f newValue,
		const Math::Vector2f delta
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DAnalogInput(deviceIdentifier, inputIdentifier, newValue, delta);
		}
	}

	void ActionMonitor::On2DSurfaceDraggingMotionInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const Math::Vector2i deltaCoordinate
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceDraggingMotionInput(deviceIdentifier, inputIdentifier, coordinate, deltaCoordinate);
		}
	}

	void ActionMonitor::On2DSurfaceHoveringMotionInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const Math::Vector2i deltaCoordinate
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceHoveringMotionInput(deviceIdentifier, inputIdentifier, coordinate, deltaCoordinate);
		}
	}

	void ActionMonitor::On2DSurfaceStartPressInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const uint8 numRepeats
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStartPressInput(deviceIdentifier, inputIdentifier, coordinate, numRepeats);
		}
	}

	void ActionMonitor::On2DSurfaceStopPressInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const uint8 numRepeats
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			if (action.IsExclusive())
			{
				action.On2DSurfaceStopPressInput(deviceIdentifier, inputIdentifier, coordinate, numRepeats);

				for (Action& otherAction : m_boundInputActions[inputIdentifier])
				{
					if (&otherAction == &action)
					{
						continue;
					}

					Assert(!otherAction.IsExclusive());
					otherAction.On2DSurfaceCancelPressInput(deviceIdentifier, inputIdentifier);
				}

				return;
			}
		}

		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStopPressInput(deviceIdentifier, inputIdentifier, coordinate, numRepeats);
		}
	}

	void ActionMonitor::On2DSurfaceCancelPressInput(const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceCancelPressInput(deviceIdentifier, inputIdentifier);
		}
	}

	void ActionMonitor::On2DSurfaceCancelTouchInput(
		const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const FingerIdentifier fingerIdentifier
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceCancelTouchInput(deviceIdentifier, inputIdentifier, fingerIdentifier);
		}
	}

	void ActionMonitor::On2DSurfaceStartTouchInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const FingerIdentifier fingerIdentifier,
		const ScreenCoordinate coordinate,
		const Math::Ratiof pressureRatio,
		const uint16 radius
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStartTouchInput(deviceIdentifier, inputIdentifier, fingerIdentifier, coordinate, pressureRatio, radius);
		}
	}

	void ActionMonitor::On2DSurfaceTouchMotionInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const FingerIdentifier fingerIdentifier,
		const ScreenCoordinate coordinate,
		const Math::Vector2i deltaCoordinate,
		const Math::Ratiof pressureRatio,
		const uint16 radius
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceTouchMotionInput(
				deviceIdentifier,
				inputIdentifier,
				fingerIdentifier,
				coordinate,
				deltaCoordinate,
				pressureRatio,
				radius
			);
		}
	}

	void ActionMonitor::On2DSurfaceStopTouchInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const FingerIdentifier fingerIdentifier,
		const ScreenCoordinate coordinate,
		const Math::Ratiof pressureRatio,
		const uint16 radius
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStopTouchInput(deviceIdentifier, inputIdentifier, fingerIdentifier, coordinate, pressureRatio, radius);
		}
	}

	void ActionMonitor::On2DSurfaceStartTapInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const uint16 radius
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStartTapInput(deviceIdentifier, inputIdentifier, coordinate, fingers, radius);
		}
	}

	void ActionMonitor::On2DSurfaceStopTapInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const uint16 radius
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStopTapInput(deviceIdentifier, inputIdentifier, coordinate, fingers, radius);
		}
	}

	void ActionMonitor::On2DSurfaceCancelTapInput(
		const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceCancelTapInput(deviceIdentifier, inputIdentifier, fingers);
		}
	}

	void ActionMonitor::On2DSurfaceDoubleTapInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const uint16 radius
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceDoubleTapInput(deviceIdentifier, inputIdentifier, coordinate, fingers, radius);
		}
	}

	void ActionMonitor::On2DSurfaceStartLongPressInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const uint16 radius
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStartLongPressInput(deviceIdentifier, inputIdentifier, coordinate, fingers, radius);
		}
	}

	void ActionMonitor::On2DSurfaceLongPressMotionInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const uint16 radius
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceLongPressMotionInput(deviceIdentifier, inputIdentifier, coordinate, fingers, radius);
		}
	}

	void ActionMonitor::On2DSurfaceStopLongPressInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const uint16 radius
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStopLongPressInput(deviceIdentifier, inputIdentifier, coordinate, fingers, radius);
		}
	}

	void ActionMonitor::On2DSurfaceCancelLongPressInput(
		const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceCancelLongPressInput(deviceIdentifier, inputIdentifier, fingers);
		}
	}

	void ActionMonitor::On2DSurfaceStartPanInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const Math::Vector2f velocity,
		const uint16 radius
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStartPanInput(deviceIdentifier, inputIdentifier, coordinate, fingers, velocity, radius);
		}
	}

	void ActionMonitor::On2DSurfacePanMotionInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const Math::Vector2f velocity,
		const uint16 touchRadius
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfacePanMotionInput(deviceIdentifier, inputIdentifier, coordinate, fingers, velocity, touchRadius);
		}
	}

	void ActionMonitor::On2DSurfaceStopPanInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const Math::Vector2f velocity
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStopPanInput(deviceIdentifier, inputIdentifier, coordinate, fingers, velocity);
		}
	}

	void ActionMonitor::On2DSurfaceCancelPanInput(
		const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceCancelPanInput(deviceIdentifier, inputIdentifier, fingers);
		}
	}

	void ActionMonitor::On2DSurfaceStartPinchInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const float scale
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStartPinchInput(deviceIdentifier, inputIdentifier, coordinate, fingers, scale);
		}
	}

	void ActionMonitor::On2DSurfacePinchMotionInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const float scale
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfacePinchMotionInput(deviceIdentifier, inputIdentifier, coordinate, fingers, scale);
		}
	}

	void ActionMonitor::On2DSurfaceStopPinchInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStopPinchInput(deviceIdentifier, inputIdentifier, coordinate, fingers);
		}
	}

	void ActionMonitor::On2DSurfaceCancelPinchInput(
		const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceCancelPinchInput(deviceIdentifier, inputIdentifier, fingers);
		}
	}

	void ActionMonitor::On2DSurfaceStartRotateInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStartRotateInput(deviceIdentifier, inputIdentifier, coordinate, fingers);
		}
	}

	void ActionMonitor::On2DSurfaceRotateMotionInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const Math::Anglef angle,
		const Math::RotationalSpeedf velocity
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceRotateMotionInput(deviceIdentifier, inputIdentifier, coordinate, fingers, angle, velocity);
		}
	}

	void ActionMonitor::On2DSurfaceStopRotateInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const Math::Anglef angle,
		const Math::RotationalSpeedf velocity
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStopRotateInput(deviceIdentifier, inputIdentifier, coordinate, fingers, angle, velocity);
		}
	}

	void ActionMonitor::On2DSurfaceCancelRotateInput(
		const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceCancelRotateInput(deviceIdentifier, inputIdentifier, fingers);
		}
	}

	void ActionMonitor::On2DSurfaceStartScrollInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const Math::Vector2i delta
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceStartScrollInput(deviceIdentifier, inputIdentifier, coordinate, delta);
		}
	}
	void ActionMonitor::On2DSurfaceScrollInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const Math::Vector2i delta
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceScrollInput(deviceIdentifier, inputIdentifier, coordinate, delta);
		}
	}
	void ActionMonitor::On2DSurfaceEndScrollInput(
		const DeviceIdentifier deviceIdentifier,
		const InputIdentifier inputIdentifier,
		const ScreenCoordinate coordinate,
		const Math::Vector2f velocity
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceEndScrollInput(deviceIdentifier, inputIdentifier, coordinate, velocity);
		}
	}
	void ActionMonitor::On2DSurfaceCancelScrollInput(
		const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const ScreenCoordinate coordinate
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.On2DSurfaceCancelScrollInput(deviceIdentifier, inputIdentifier, coordinate);
		}
	}

	void ActionMonitor::OnTransformInput(
		const DeviceIdentifier deviceIdentifier, const InputIdentifier inputIdentifier, const Math::LocalTransform transform
	)
	{
		for (Action& action : m_boundInputActions[inputIdentifier])
		{
			action.OnTransformInput(deviceIdentifier, inputIdentifier, transform);
		}
	}
}
