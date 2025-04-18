#include "Input/Devices/Touchscreen/Touchscreen.h"
#include "Input/InputManager.h"
#include "Input/Monitor.h"
#include "Input/ScreenCoordinate.h"
#include "Input/Devices/Keyboard/Keyboard.h"

#include <Common/Math/Vector2.h>
#include <Common/Math/Ratio.h>
#include <Common/Math/Length.h>
#include <Common/Math/Angle.h>
#include <Common/Math/RotationalSpeed.h>

#include <Renderer/Window/Window.h>
#include <Common/Storage/IdentifierMask.h>

namespace ngine::Input
{
	TouchscreenDeviceType::TouchscreenDeviceType(const DeviceTypeIdentifier identifier, Input::Manager& manager)
		: DeviceType(identifier)
		, m_manager(manager)
		, m_startTouchInputIdentifier(manager.RegisterInput())
		, m_stopTouchInputIdentifier(manager.RegisterInput())
		, m_motionInputIdentifier(manager.RegisterInput())
		, m_tapInputIdentifier(manager.RegisterInput())
		, m_doubleTapInputIdentifier(manager.RegisterInput())
		, m_longPressInputIdentifier(manager.RegisterInput())
		, m_panInputIdentifier(manager.RegisterInput())
		, m_pinchInputIdentifier(manager.RegisterInput())
		, m_rotateInputIdentifier(manager.RegisterInput())
	{
		m_gestureRecognizer.OnTapStarted.Bind(*this, &TouchscreenDeviceType::OnStartTap);
		m_gestureRecognizer.OnTapEnded.Bind(*this, &TouchscreenDeviceType::OnStopTap);
		m_gestureRecognizer.OnCancelTap.Bind(*this, &TouchscreenDeviceType::OnCancelTap);

		m_gestureRecognizer.OnStartLongPress.Bind(*this, &TouchscreenDeviceType::OnStartLongPress);
		m_gestureRecognizer.OnStopLongPress.Bind(*this, &TouchscreenDeviceType::OnStopLongPress);
		m_gestureRecognizer.OnLongPressMotion.Bind(*this, &TouchscreenDeviceType::OnLongPressMotion);
		m_gestureRecognizer.OnCancelLongPress.Bind(*this, &TouchscreenDeviceType::OnCancelLongPress);

		m_gestureRecognizer.OnDoubleTap.Bind(*this, &TouchscreenDeviceType::OnDoubleTap);

		m_gestureRecognizer.OnStartPan.Bind(*this, &TouchscreenDeviceType::OnStartPan);
		m_gestureRecognizer.OnMotionPan.Bind(*this, &TouchscreenDeviceType::OnPanMotion);
		m_gestureRecognizer.OnStopPan.Bind(*this, &TouchscreenDeviceType::OnStopPan);
		m_gestureRecognizer.OnCancelPan.Bind(*this, &TouchscreenDeviceType::OnCancelPan);

		m_gestureRecognizer.OnStartPinch.Bind(*this, &TouchscreenDeviceType::OnStartPinch);
		m_gestureRecognizer.OnMotionPinch.Bind(*this, &TouchscreenDeviceType::OnPinchMotion);
		m_gestureRecognizer.OnStopPinch.Bind(*this, &TouchscreenDeviceType::OnStopPinch);
		m_gestureRecognizer.OnCancelPinch.Bind(*this, &TouchscreenDeviceType::OnCancelPinch);

		m_gestureRecognizer.Initialize();
	}

	Input::DeviceIdentifier TouchscreenDeviceType::GetOrRegisterInstance(
		const int64 sdlIdentifier, Input::Manager& manager, Optional<Rendering::Window*> pSourceWindow
	)
	{
		Optional<const Touchscreen*> pDevice;

		manager.IterateDeviceInstances(
			[identifier = m_identifier, sdlIdentifier, &pDevice](const DeviceInstance& instance) -> Memory::CallbackResult
			{
				if (instance.GetTypeIdentifier() == identifier)
				{
					if (static_cast<const Touchscreen&>(instance).m_sdlIdentifier == sdlIdentifier)
					{
						pDevice = static_cast<const Touchscreen&>(instance);
						return Memory::CallbackResult::Break;
					}
				}

				return Memory::CallbackResult::Continue;
			}
		);

		if (LIKELY(pDevice.IsValid()))
		{
			return pDevice->GetIdentifier();
		}

		Assert(pSourceWindow.IsValid());
		return manager.RegisterDeviceInstance<Touchscreen>(
			m_identifier,
			pSourceWindow.IsValid() ? pSourceWindow->GetFocusedInputMonitor() : Optional<Monitor*>{},
			sdlIdentifier,
			pSourceWindow.IsValid() ? (ScreenCoordinate)pSourceWindow->GetClientAreaSize() : ScreenCoordinate{Math::Zero}
		);
	}

	void TouchscreenDeviceType::OnCancelTouch(
		const DeviceIdentifier deviceIdentifier, const FingerIdentifier fingerIdentifier, Rendering::Window& window
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, fingerIdentifier, &window]()
			{
				m_gestureRecognizer.TouchCanceled(deviceIdentifier, fingerIdentifier, window);

				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));
				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingerIdentifier))
				{
					const Optional<Monitor*> pMonitorDuringLastPress = touchscreenInstance.GetMonitorDuringLastPress(fingerIdentifier);
					if (pMonitorDuringLastPress != pMonitor && pMonitorDuringLastPress != nullptr)
					{
						pMonitorDuringLastPress->On2DSurfaceCancelTouchInput(deviceIdentifier, m_stopTouchInputIdentifier, fingerIdentifier);
					}
					pMonitor->On2DSurfaceCancelTouchInput(deviceIdentifier, m_stopTouchInputIdentifier, fingerIdentifier);
				}
			}
		);
	}

	void TouchscreenDeviceType::OnStartTouch(TouchDescriptor touch, DeviceIdentifier deviceIdentifier, Rendering::Window& window)
	{
		m_manager.QueueInput(
			[this, touch, deviceIdentifier, &window]()
			{
				m_gestureRecognizer.TouchBegan(touch, deviceIdentifier, window);

				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));
				const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(touch.screenCoordinate);
				{
					Input::Monitor* pFocusedMonitor = window.GetMouseMonitorAtCoordinates(windowCoordinate, touch.touchRadius);
					touchscreenInstance.SetMonitorDuringLastPress(touch.fingerIdentifier, pFocusedMonitor);
					if (touchscreenInstance.GetActiveMonitor(touch.fingerIdentifier) != pFocusedMonitor)
					{
						touchscreenInstance.SetActiveMonitor(pFocusedMonitor, *this, touch.fingerIdentifier);
						if (pFocusedMonitor)
						{
							OnMonitorChanged(*pFocusedMonitor);
						}
					}

					m_manager.IterateDeviceInstances(
						[keyboardTypeIdentifier = m_manager.GetKeyboardDeviceTypeIdentifier(), pFocusedMonitor, this](DeviceInstance& instance
				    ) -> Memory::CallbackResult
						{
							if (instance.GetTypeIdentifier() == keyboardTypeIdentifier)
							{
								if (instance.GetActiveMonitor() != pFocusedMonitor)
								{
									instance.SetActiveMonitor(pFocusedMonitor, m_manager.GetDeviceType(keyboardTypeIdentifier));
								}
							}

							return Memory::CallbackResult::Continue;
						}
					);
				}

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(touch.fingerIdentifier))
				{
					pMonitor->On2DSurfaceStartTouchInput(
						deviceIdentifier,
						m_startTouchInputIdentifier,
						touch.fingerIdentifier,
						touch.screenCoordinate,
						touch.pressureRatio,
						touch.touchRadius
					);
				}
			}
		);
	}

	void TouchscreenDeviceType::OnStopTouch(TouchDescriptor touch, DeviceIdentifier deviceIdentifier, Rendering::Window& window)
	{
		m_manager.QueueInput(
			[this, touch, deviceIdentifier, &window]()
			{
				m_gestureRecognizer.TouchEnded(touch, deviceIdentifier, window);

				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));
				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(touch.fingerIdentifier))
				{
					const Optional<Monitor*> pMonitorDuringLastPress = touchscreenInstance.GetMonitorDuringLastPress(touch.fingerIdentifier);
					if (pMonitorDuringLastPress != pMonitor && pMonitorDuringLastPress != nullptr)
					{
						pMonitorDuringLastPress->On2DSurfaceCancelTouchInput(deviceIdentifier, m_stopTouchInputIdentifier, touch.fingerIdentifier);
						pMonitor->On2DSurfaceCancelTouchInput(deviceIdentifier, m_stopTouchInputIdentifier, touch.fingerIdentifier);
					}
					else
					{
						pMonitor->On2DSurfaceStopTouchInput(
							deviceIdentifier,
							m_stopTouchInputIdentifier,
							touch.fingerIdentifier,
							touch.screenCoordinate,
							touch.pressureRatio,
							touch.touchRadius
						);
					}
				}
			}
		);
	}

	void TouchscreenDeviceType::OnMotion(TouchDescriptor touch, DeviceIdentifier deviceIdentifier, Rendering::Window& window)
	{
		m_manager.QueueInput(
			[this, touch, deviceIdentifier, &window]()
			{
				m_gestureRecognizer.TouchMoved(touch, deviceIdentifier, window);

				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));
				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(touch.fingerIdentifier))
				{
					pMonitor->On2DSurfaceTouchMotionInput(
						deviceIdentifier,
						m_motionInputIdentifier,
						touch.fingerIdentifier,
						touch.screenCoordinate,
						touch.deltaCoordinates,
						touch.pressureRatio,
						touch.touchRadius
					);
				}
			}
		);
	}

	void TouchscreenDeviceType::OnStartTap(
		DeviceIdentifier deviceIdentifier,
		ScreenCoordinate screenCoordinate,
		const uint16 touchRadius,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		Rendering::Window& window
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, touchRadius, fingers = InlineVector<FingerIdentifier, 5>{fingers}, &window]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));
				const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
				{
					Input::Monitor* pFocusedMonitor = window.GetMouseMonitorAtCoordinates(windowCoordinate, touchRadius);
					touchscreenInstance.SetMonitorDuringLastPress(fingers[0], pFocusedMonitor);
					if (touchscreenInstance.GetActiveMonitor(fingers[0]) != pFocusedMonitor)
					{
						touchscreenInstance.SetActiveMonitor(pFocusedMonitor, *this, fingers[0]);
						if (pFocusedMonitor)
						{
							OnMonitorChanged(*pFocusedMonitor);
						}
					}

					m_manager.IterateDeviceInstances(
						[keyboardTypeIdentifier = m_manager.GetKeyboardDeviceTypeIdentifier(), pFocusedMonitor, this](DeviceInstance& instance
				    ) -> Memory::CallbackResult
						{
							if (instance.GetTypeIdentifier() == keyboardTypeIdentifier)
							{
								if (instance.GetActiveMonitor() != pFocusedMonitor)
								{
									instance.SetActiveMonitor(pFocusedMonitor, m_manager.GetDeviceType(keyboardTypeIdentifier));
								}
							}

							return Memory::CallbackResult::Continue;
						}
					);
				}

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceStartTapInput(deviceIdentifier, m_tapInputIdentifier, screenCoordinate, fingers.GetView(), touchRadius);
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnStopTap(DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const uint16 touchRadius, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, touchRadius, fingers = InlineVector<FingerIdentifier, 5>{fingers}]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));
				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceStopTapInput(deviceIdentifier, m_tapInputIdentifier, screenCoordinate, fingers.GetView(), touchRadius);
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnCancelTap(DeviceIdentifier deviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, fingers = InlineVector<FingerIdentifier, 5>{fingers}]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));
				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceCancelTapInput(deviceIdentifier, m_tapInputIdentifier, fingers.GetView());
				}
			}
		);
	}

	void TouchscreenDeviceType::OnDoubleTap(
		DeviceIdentifier deviceIdentifier,
		ScreenCoordinate screenCoordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const uint16 touchRadius,
		Rendering::Window& window
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, fingers = InlineVector<FingerIdentifier, 5>{fingers}, touchRadius, &window]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
				{
					Input::Monitor* pFocusedMonitor = window.GetMouseMonitorAtCoordinates(windowCoordinate, touchRadius);
					touchscreenInstance.SetMonitorDuringLastPress(fingers[0], pFocusedMonitor);
					if (touchscreenInstance.GetActiveMonitor(fingers[0]) != pFocusedMonitor)
					{
						touchscreenInstance.SetActiveMonitor(pFocusedMonitor, *this, fingers[0]);
						if (pFocusedMonitor)
						{
							OnMonitorChanged(*pFocusedMonitor);
						}
					}

					m_manager.IterateDeviceInstances(
						[keyboardTypeIdentifier = m_manager.GetKeyboardDeviceTypeIdentifier(),
				     gamepadTypeIdentifier = m_manager.GetGamepadDeviceTypeIdentifier(),
				     pFocusedMonitor,
				     this](DeviceInstance& instance) -> Memory::CallbackResult
						{
							if (instance.GetTypeIdentifier() == keyboardTypeIdentifier)
							{
								if (instance.GetActiveMonitor() != pFocusedMonitor)
								{
									instance.SetActiveMonitor(pFocusedMonitor, m_manager.GetDeviceType(keyboardTypeIdentifier));
								}
							}
							else if (instance.GetTypeIdentifier() == gamepadTypeIdentifier)
							{
								if (instance.GetActiveMonitor() != pFocusedMonitor)
								{
									instance.SetActiveMonitor(pFocusedMonitor, m_manager.GetDeviceType(gamepadTypeIdentifier));
								}
							}

							return Memory::CallbackResult::Continue;
						}
					);
				}

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor
						->On2DSurfaceDoubleTapInput(deviceIdentifier, m_doubleTapInputIdentifier, screenCoordinate, fingers.GetView(), touchRadius);
				}
			}
		);
	}

	void TouchscreenDeviceType::OnStartLongPress(
		DeviceIdentifier deviceIdentifier,
		ScreenCoordinate screenCoordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const uint16 touchRadius,
		Rendering::Window& window
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, fingers = InlineVector<FingerIdentifier, 5>{fingers}, touchRadius, &window]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
				{
					Input::Monitor* pFocusedMonitor = window.GetMouseMonitorAtCoordinates(windowCoordinate, touchRadius);
					touchscreenInstance.SetMonitorDuringLastPress(fingers[0], pFocusedMonitor);
					if (touchscreenInstance.GetActiveMonitor(fingers[0]) != pFocusedMonitor)
					{
						touchscreenInstance.SetActiveMonitor(pFocusedMonitor, *this, fingers[0]);
						if (pFocusedMonitor)
						{
							OnMonitorChanged(*pFocusedMonitor);
						}
					}

					m_manager.IterateDeviceInstances(
						[keyboardTypeIdentifier = m_manager.GetKeyboardDeviceTypeIdentifier(),
				     gamepadTypeIdentifier = m_manager.GetGamepadDeviceTypeIdentifier(),
				     pFocusedMonitor,
				     this](DeviceInstance& instance) -> Memory::CallbackResult
						{
							if (instance.GetTypeIdentifier() == keyboardTypeIdentifier)
							{
								if (instance.GetActiveMonitor() != pFocusedMonitor)
								{
									instance.SetActiveMonitor(pFocusedMonitor, m_manager.GetDeviceType(keyboardTypeIdentifier));
								}
							}
							else if (instance.GetTypeIdentifier() == gamepadTypeIdentifier)
							{
								if (instance.GetActiveMonitor() != pFocusedMonitor)
								{
									instance.SetActiveMonitor(pFocusedMonitor, m_manager.GetDeviceType(gamepadTypeIdentifier));
								}
							}

							return Memory::CallbackResult::Continue;
						}
					);
				}

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceStartLongPressInput(
						deviceIdentifier,
						m_longPressInputIdentifier,
						screenCoordinate,
						fingers.GetView(),
						touchRadius
					);
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnStopLongPress(DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const ArrayView<const FingerIdentifier, uint8> fingers, const uint16 touchRadius, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, fingers = InlineVector<FingerIdentifier, 5>{fingers}, touchRadius]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor
						->On2DSurfaceStopLongPressInput(deviceIdentifier, m_longPressInputIdentifier, screenCoordinate, fingers.GetView(), touchRadius);
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnLongPressMotion(DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const ArrayView<const FingerIdentifier, uint8> fingers, uint16 touchRadius, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, fingers = InlineVector<FingerIdentifier, 5>{fingers}, touchRadius]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceLongPressMotionInput(
						deviceIdentifier,
						m_longPressInputIdentifier,
						screenCoordinate,
						fingers.GetView(),
						touchRadius
					);
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnCancelLongPress(DeviceIdentifier deviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, fingers = InlineVector<FingerIdentifier, 5>{fingers}]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceCancelLongPressInput(deviceIdentifier, m_longPressInputIdentifier, fingers.GetView());
				}
			}
		);
	}

	void TouchscreenDeviceType::OnStartPan(
		DeviceIdentifier deviceIdentifier,
		ScreenCoordinate screenCoordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		const Math::Vector2f velocity,
		const uint16 touchRadius,
		Rendering::Window& window
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, fingers = InlineVector<FingerIdentifier, 5>{fingers}, velocity, touchRadius, &window]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
				{
					Input::Monitor* pFocusedMonitor = window.GetMouseMonitorAtCoordinates(windowCoordinate, touchRadius);
					touchscreenInstance.SetMonitorDuringLastPress(fingers[0], pFocusedMonitor);
					if (touchscreenInstance.GetActiveMonitor(fingers[0]) != pFocusedMonitor)
					{
						touchscreenInstance.SetActiveMonitor(pFocusedMonitor, *this, fingers[0]);
						if (pFocusedMonitor)
						{
							OnMonitorChanged(*pFocusedMonitor);
						}
					}

					m_manager.IterateDeviceInstances(
						[keyboardTypeIdentifier = m_manager.GetKeyboardDeviceTypeIdentifier(),
				     gamepadTypeIdentifier = m_manager.GetGamepadDeviceTypeIdentifier(),
				     pFocusedMonitor,
				     this](DeviceInstance& instance) -> Memory::CallbackResult
						{
							if (instance.GetTypeIdentifier() == keyboardTypeIdentifier)
							{
								if (instance.GetActiveMonitor() != pFocusedMonitor)
								{
									instance.SetActiveMonitor(pFocusedMonitor, m_manager.GetDeviceType(keyboardTypeIdentifier));
								}
							}
							else if (instance.GetTypeIdentifier() == gamepadTypeIdentifier)
							{
								if (instance.GetActiveMonitor() != pFocusedMonitor)
								{
									instance.SetActiveMonitor(pFocusedMonitor, m_manager.GetDeviceType(gamepadTypeIdentifier));
								}
							}

							return Memory::CallbackResult::Continue;
						}
					);
				}

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor
						->On2DSurfaceStartPanInput(deviceIdentifier, m_panInputIdentifier, screenCoordinate, fingers.GetView(), velocity, touchRadius);
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnStopPan(DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const Math::Vector2f velocity, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, velocity, fingers = InlineVector<FingerIdentifier, 5>{fingers}]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceStopPanInput(deviceIdentifier, m_panInputIdentifier, screenCoordinate, fingers.GetView(), velocity);
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnPanMotion(DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const Math::Vector2f velocity, const uint16 touchRadius, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, velocity, touchRadius, fingers = InlineVector<FingerIdentifier, 5>{fingers}]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor
						->On2DSurfacePanMotionInput(deviceIdentifier, m_panInputIdentifier, screenCoordinate, fingers.GetView(), velocity, touchRadius);
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnCancelPan(DeviceIdentifier deviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, fingers = InlineVector<FingerIdentifier, 5>{fingers}]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceCancelPanInput(deviceIdentifier, m_panInputIdentifier, fingers.GetView());
				}
			}
		);
	}

	void TouchscreenDeviceType::OnStartPinch(
		DeviceIdentifier deviceIdentifier,
		ScreenCoordinate screenCoordinate,
		const float scale,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		Rendering::Window& window
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, scale, fingers = InlineVector<FingerIdentifier, 5>{fingers}, &window]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
				{
					Input::Monitor* pFocusedMonitor = window.GetMouseMonitorAtCoordinates(windowCoordinate, Invalid);
					touchscreenInstance.SetMonitorDuringLastPress(fingers[0], pFocusedMonitor);
					if (touchscreenInstance.GetActiveMonitor(fingers[0]) != pFocusedMonitor)
					{
						touchscreenInstance.SetActiveMonitor(pFocusedMonitor, *this, fingers[0]);
						if (pFocusedMonitor)
						{
							OnMonitorChanged(*pFocusedMonitor);
						}
					}

					m_manager.IterateDeviceInstances(
						[keyboardTypeIdentifier = m_manager.GetKeyboardDeviceTypeIdentifier(), pFocusedMonitor, this](DeviceInstance& instance
				    ) -> Memory::CallbackResult
						{
							if (instance.GetTypeIdentifier() == keyboardTypeIdentifier)
							{
								if (instance.GetActiveMonitor() != pFocusedMonitor)
								{
									instance.SetActiveMonitor(pFocusedMonitor, m_manager.GetDeviceType(keyboardTypeIdentifier));
								}
							}

							return Memory::CallbackResult::Continue;
						}
					);
				}

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceStartPinchInput(deviceIdentifier, m_pinchInputIdentifier, screenCoordinate, fingers.GetView(), scale);
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnStopPinch(DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, fingers = InlineVector<FingerIdentifier, 5>{fingers}]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceStopPinchInput(deviceIdentifier, m_pinchInputIdentifier, screenCoordinate, fingers.GetView());
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnPinchMotion(DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const float scale, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, scale, fingers = InlineVector<FingerIdentifier, 5>{fingers}]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfacePinchMotionInput(deviceIdentifier, m_pinchInputIdentifier, screenCoordinate, fingers.GetView(), scale);
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnCancelPinch(DeviceIdentifier deviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, fingers = InlineVector<FingerIdentifier, 5>{fingers}]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceCancelPinchInput(deviceIdentifier, m_pinchInputIdentifier, fingers.GetView());
				}
			}
		);
	}

	void TouchscreenDeviceType::OnStartRotate(
		DeviceIdentifier deviceIdentifier,
		ScreenCoordinate screenCoordinate,
		const ArrayView<const FingerIdentifier, uint8> fingers,
		Rendering::Window& window
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, fingers = InlineVector<FingerIdentifier, 5>{fingers}, &window]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
				{
					Input::Monitor* pFocusedMonitor = window.GetMouseMonitorAtCoordinates(windowCoordinate, Invalid);
					touchscreenInstance.SetMonitorDuringLastPress(fingers[0], pFocusedMonitor);
					if (touchscreenInstance.GetActiveMonitor(fingers[0]) != pFocusedMonitor)
					{
						touchscreenInstance.SetActiveMonitor(pFocusedMonitor, *this, fingers[0]);
						if (pFocusedMonitor)
						{
							OnMonitorChanged(*pFocusedMonitor);
						}
					}

					m_manager.IterateDeviceInstances(
						[keyboardTypeIdentifier = m_manager.GetKeyboardDeviceTypeIdentifier(), pFocusedMonitor, this](DeviceInstance& instance
				    ) -> Memory::CallbackResult
						{
							if (instance.GetTypeIdentifier() == keyboardTypeIdentifier)
							{
								if (instance.GetActiveMonitor() != pFocusedMonitor)
								{
									instance.SetActiveMonitor(pFocusedMonitor, m_manager.GetDeviceType(keyboardTypeIdentifier));
								}
							}

							return Memory::CallbackResult::Continue;
						}
					);
				}

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceStartRotateInput(deviceIdentifier, m_rotateInputIdentifier, screenCoordinate, fingers.GetView());
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnStopRotate(DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const Math::Anglef angle, const Math::RotationalSpeedf velocity, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, angle, velocity, fingers = InlineVector<FingerIdentifier, 5>{fingers}]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor
						->On2DSurfaceStopRotateInput(deviceIdentifier, m_rotateInputIdentifier, screenCoordinate, fingers.GetView(), angle, velocity);
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnRotateMotion(DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const Math::Anglef angle, const Math::RotationalSpeedf velocity, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, angle, velocity, fingers = InlineVector<FingerIdentifier, 5>{fingers}]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor
						->On2DSurfaceRotateMotionInput(deviceIdentifier, m_rotateInputIdentifier, screenCoordinate, fingers.GetView(), angle, velocity);
				}
			}
		);
	}

	void TouchscreenDeviceType::
		OnCancelRotate(DeviceIdentifier deviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window&)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, fingers = InlineVector<FingerIdentifier, 5>{fingers}]()
			{
				Touchscreen& touchscreenInstance = static_cast<Touchscreen&>(m_manager.GetDeviceInstance(deviceIdentifier));

				if (Monitor* pMonitor = touchscreenInstance.GetActiveMonitor(fingers[0]))
				{
					pMonitor->On2DSurfaceCancelRotateInput(deviceIdentifier, m_rotateInputIdentifier, fingers.GetView());
				}
			}
		);
	}

	InputIdentifier TouchscreenDeviceType::DeserializeDeviceInput(const Serialization::Reader&) const
	{
		// TODO(Matt) Add input identifiers here
		return {};
	}
}
