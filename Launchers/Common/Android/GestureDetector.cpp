#if PLATFORM_ANDROID
#include "GestureDetector.h"
#include <Engine/Engine.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>
#include <Engine/Input/Devices/Gamepad/GamepadMapping.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>

#include <Renderer/Window/Window.h>
#include <Renderer/Devices/LogicalDevice.h>

#include <Common/IO/File.h>
#include <Common/System/Query.h>
#include <Common/CommandLine/CommandLineInitializationParameters.h>
#include <Common/IO/Log.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Math/IsEquivalentTo.h>
#include <Common/Math/Radius.h>
#include <Common/EnumFlags.h>

#include <android/sensor.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/keycodes.h>
#include <android/input.h>
#include <game-activity/native_app_glue/android_native_app_glue.h>

namespace ngine::Platform::Android
{
	void GestureDetector::Initialize(Rendering::Window& window)
	{
		m_pWindow = window;

		Input::Manager& inputManager = System::Get<Input::Manager>();
		m_pTouchscreenDeviceType = inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
		if (m_pTouchscreenDeviceType)
		{
			m_touchscreenIdentifier = m_pTouchscreenDeviceType->GetOrRegisterInstance(0, inputManager, window);
		}
	}

	void GestureDetector::ProcessMotionEvent(const GameActivityMotionEvent& motionEvent, const int32 displayDensity)
	{
		if (motionEvent.pointerCount > 0)
		{
			if ((motionEvent.source & AINPUT_SOURCE_TOUCHSCREEN) == AINPUT_SOURCE_TOUCHSCREEN)
			{
				const int actionMasked = motionEvent.action & AMOTION_EVENT_ACTION_MASK;

				const uint32 count = motionEvent.pointerCount;
				for (uint32 touchIndex = 0; touchIndex < count; ++touchIndex)
				{
					Input::TouchDescriptor touchDescriptor;
					touchDescriptor.fingerIdentifier = Input::FingerIdentifier(motionEvent.pointers[touchIndex].id);
					touchDescriptor.screenCoordinate = GetScreenCoordinate(motionEvent, touchIndex, -1);
					touchDescriptor.touchRadius = GetPointerRadius(motionEvent, touchIndex, -1, displayDensity);
					touchDescriptor.pressureRatio = GetPointerPressure(motionEvent, touchIndex, -1);

					switch (actionMasked)
					{
						case AMOTION_EVENT_ACTION_POINTER_DOWN:
						{
							auto activeFingerIt = m_activeFingerIdentifiers.Find(touchDescriptor.fingerIdentifier);
							if (activeFingerIt == m_activeFingerIdentifiers.end())
							{
								m_activeFingerIdentifiers.Emplace(Input::FingerIdentifier(touchDescriptor.fingerIdentifier));
								m_pTouchscreenDeviceType->OnStartTouch(Move(touchDescriptor), m_touchscreenIdentifier, *m_pWindow);
							}
						}
						break;
						case AMOTION_EVENT_ACTION_POINTER_UP:
						{
							auto activeFingerIt = m_activeFingerIdentifiers.Find(touchDescriptor.fingerIdentifier);
							if (activeFingerIt != m_activeFingerIdentifiers.end())
							{
								m_activeFingerIdentifiers.Remove(activeFingerIt);
								m_pTouchscreenDeviceType->OnStopTouch(Move(touchDescriptor), m_touchscreenIdentifier, *m_pWindow);
							}
						}
						break;
						case AMOTION_EVENT_ACTION_DOWN:
						{
							auto activeFingerIt = m_activeFingerIdentifiers.Find(touchDescriptor.fingerIdentifier);
							if (activeFingerIt == m_activeFingerIdentifiers.end())
							{
								m_activeFingerIdentifiers.Emplace(Input::FingerIdentifier(touchDescriptor.fingerIdentifier));
								m_pTouchscreenDeviceType->OnStartTouch(Move(touchDescriptor), m_touchscreenIdentifier, *m_pWindow);
							}
						}
						break;
						case AMOTION_EVENT_ACTION_UP:
						{
							auto activeFingerIt = m_activeFingerIdentifiers.Find(touchDescriptor.fingerIdentifier);
							if (activeFingerIt != m_activeFingerIdentifiers.end())
							{
								m_activeFingerIdentifiers.Remove(activeFingerIt);
								m_pTouchscreenDeviceType->OnStopTouch(Move(touchDescriptor), m_touchscreenIdentifier, *m_pWindow);
							}
						}
						break;
						case AMOTION_EVENT_ACTION_MOVE:
						{
							const int32 historySize = GameActivityMotionEvent_getHistorySize(&motionEvent);
							for (int32 historyPos = 0; historyPos < historySize; historyPos++)
							{
								Input::TouchDescriptor historicTouchDescriptor;
								historicTouchDescriptor.fingerIdentifier = touchDescriptor.fingerIdentifier;
								historicTouchDescriptor.screenCoordinate = GetScreenCoordinate(motionEvent, touchIndex, historyPos);
								historicTouchDescriptor.touchRadius = GetPointerRadius(motionEvent, touchIndex, historyPos, displayDensity);
								historicTouchDescriptor.pressureRatio = GetPointerPressure(motionEvent, touchIndex, historyPos);
								m_pTouchscreenDeviceType->OnMotion(Move(historicTouchDescriptor), m_touchscreenIdentifier, *m_pWindow);
							}
							m_pTouchscreenDeviceType->OnMotion(Move(touchDescriptor), m_touchscreenIdentifier, *m_pWindow);
						}
						break;
						case AMOTION_EVENT_ACTION_CANCEL:
							m_pTouchscreenDeviceType->OnCancelTouch(m_touchscreenIdentifier, touchDescriptor.fingerIdentifier, *m_pWindow);
							break;
					}
				}
			}
		}
	}

	void GestureDetector::SetViewControllerSettings(int32 scaledTouchSlope, int32 scaledMinimumScalingSpan)
	{
		m_scaledTouchSlope = scaledTouchSlope;
		m_scaledMinimumScalingSpan = scaledMinimumScalingSpan;
	}

	ScreenCoordinate
	GestureDetector::GetScreenCoordinate(const GameActivityMotionEvent& motionEvent, int32 pointerIndex, int32 historyPos) const
	{
		if (historyPos < 0)
		{
			return (ScreenCoordinate)Math::Vector2f{
				GameActivityPointerAxes_getX(&motionEvent.pointers[pointerIndex]),
				GameActivityPointerAxes_getY(&motionEvent.pointers[pointerIndex])
			};
		}
		else
		{
			return (ScreenCoordinate)Math::Vector2f{
				GameActivityMotionEvent_getHistoricalX(&motionEvent, pointerIndex, historyPos),
				GameActivityMotionEvent_getHistoricalY(&motionEvent, pointerIndex, historyPos)
			};
		}
	}

	uint16 GestureDetector::GetPointerRadius(
		const GameActivityMotionEvent& motionEvent, int32 pointerIndex, int32 historyPos, const int32 displayDensity
	) const
	{
		const int32 toolType = GameActivityPointerAxes_getToolType(&motionEvent.pointers[pointerIndex]);
		// TODO: Detect input device (touch vs tool)
		double touchRadiusMinor;
		double touchRadiusMajor;
		if (historyPos < 0)
		{
			switch (toolType)
			{
				case AMOTION_EVENT_TOOL_TYPE_FINGER:
					touchRadiusMinor = GameActivityPointerAxes_getTouchMinor(&motionEvent.pointers[pointerIndex]) * 0.5;
					touchRadiusMajor = GameActivityPointerAxes_getTouchMajor(&motionEvent.pointers[pointerIndex]) * 0.5;
					break;
				case AMOTION_EVENT_TOOL_TYPE_STYLUS:
					touchRadiusMinor = GameActivityPointerAxes_getToolMinor(&motionEvent.pointers[pointerIndex]) * 0.5;
					touchRadiusMajor = GameActivityPointerAxes_getToolMajor(&motionEvent.pointers[pointerIndex]) * 0.5;
					break;
				default:
					touchRadiusMinor = 1.0;
					touchRadiusMajor = 5.0;
					break;
			}
		}
		else
		{
			switch (toolType)
			{
				case AMOTION_EVENT_TOOL_TYPE_FINGER:
					touchRadiusMinor = GameActivityMotionEvent_getHistoricalTouchMinor(&motionEvent, pointerIndex, historyPos) * 0.5;
					touchRadiusMajor = GameActivityMotionEvent_getHistoricalTouchMajor(&motionEvent, pointerIndex, historyPos) * 0.5;
					break;
				case AMOTION_EVENT_TOOL_TYPE_STYLUS:
					touchRadiusMinor = GameActivityMotionEvent_getHistoricalToolMinor(&motionEvent, pointerIndex, historyPos) * 0.5;
					touchRadiusMajor = GameActivityMotionEvent_getHistoricalToolMajor(&motionEvent, pointerIndex, historyPos) * 0.5;
					break;
				default:
					touchRadiusMinor = 1.0;
					touchRadiusMajor = 5.0;
					break;
			}
		}
		touchRadiusMinor = Math::Sqrt(touchRadiusMajor + touchRadiusMinor);

		const float dotsPerInch = ((float)displayDensity / float(ACONFIGURATION_DENSITY_MEDIUM)) * 100.f;
		touchRadiusMinor /= (float)dotsPerInch;
		constexpr Math::Radiusd minRadius = 3_millimeters;
		constexpr Math::Radiusd maxRadius = 15_millimeters;
		touchRadiusMinor = Math::Clamp(touchRadiusMinor, minRadius.GetInches(), maxRadius.GetInches());
		touchRadiusMinor *= (float)dotsPerInch;

		return (uint16)touchRadiusMinor;
	}

	Math::Ratiof GestureDetector::GetPointerPressure(const GameActivityMotionEvent& motionEvent, int32 pointerIndex, int32 historyPos) const
	{
		if (historyPos < 0)
		{
			return Math::Ratiof(GameActivityPointerAxes_getPressure(&motionEvent.pointers[pointerIndex]));
		}
		else
		{
			return Math::Ratiof(GameActivityMotionEvent_getHistoricalPressure(&motionEvent, pointerIndex, historyPos));
		}
	}
}
#endif
