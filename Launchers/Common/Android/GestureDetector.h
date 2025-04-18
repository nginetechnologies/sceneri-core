#pragma once

#include <Engine/Input/InputIdentifier.h>
#include <Engine/Input/DeviceIdentifier.h>
#include <Engine/Input/ScreenCoordinate.h>
#include <Engine/Input/Devices/Touchscreen/FingerIdentifier.h>

#include <Common/Memory/Containers/UnorderedSet.h>
#include <Common/Memory/Optional.h>
#include <Common/Math/Ratio.h>

namespace ngine::Input
{
	struct TouchscreenDeviceType;
}

namespace ngine::Rendering
{
	struct Window;
}

struct GameActivityMotionEvent;

namespace ngine::Platform::Android
{
	struct GestureDetector
	{
		void Initialize(Rendering::Window& window);
		void ProcessMotionEvent(const GameActivityMotionEvent& motionEvent, const int32 displayDensity);
		void SetViewControllerSettings(int32 scaledTouchSlope, int32 scaledMinimumScalingSpan);
	private:
		ScreenCoordinate GetScreenCoordinate(const GameActivityMotionEvent& motionEvent, int32 pointerIndex, int32 historyPos) const;
		uint16
		GetPointerRadius(const GameActivityMotionEvent& motionEvent, int32 pointerIndex, int32 historyPos, const int32 displayDensity) const;
		Math::Ratiof GetPointerPressure(const GameActivityMotionEvent& motionEvent, int32 pointerIndex, int32 historyPos) const;
	private:
		Optional<Input::TouchscreenDeviceType*> m_pTouchscreenDeviceType{nullptr};
		Optional<Rendering::Window*> m_pWindow;
		Input::DeviceIdentifier m_touchscreenIdentifier;
		UnorderedSet<Input::FingerIdentifier> m_activeFingerIdentifiers;
		int32 m_scaledTouchSlope{20};
		int32 m_scaledMinimumScalingSpan{400};
	};
}
