#pragma once

#include "DeviceIdentifier.h"
#include "DeviceTypeIdentifier.h"
#include "InputIdentifier.h"
#include "ScreenCoordinate.h"
#include "Devices/Touchscreen/FingerIdentifier.h"

#include <Common/EnumFlags.h>
#include <Common/Math/Ratio.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Time/Duration.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Function/Function.h>

namespace ngine::Rendering
{
	struct Window;
}

namespace ngine::Input
{
	struct TouchDescriptor
	{
		FingerIdentifier fingerIdentifier;
		ScreenCoordinate screenCoordinate = Math::Zero;
		Math::Vector2i deltaCoordinates = Math::Zero;
		Math::Ratiof pressureRatio = 0_percent;
		uint16 touchRadius = 0u;
	};

	using TouchEventIdentifier = TIdentifier<uint32, 6>;

	class GestureRecognizer
	{
	public:
		static constexpr Time::Durationd SingleTapTimeout = 100_milliseconds;
		static constexpr Time::Durationd DoubleTapTimeout = 300_milliseconds;
		static constexpr Time::Durationd LongPressInterval = 500_milliseconds;

		// Tap
		Function<
			void(
				DeviceIdentifier deviceIdentifier,
				ScreenCoordinate coordinate,
				const uint16 touchRadius,
				const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers,
				Rendering::Window& window
			),
			24>
			OnTapStarted;
		Function<void(DeviceIdentifier deviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window& window), 24>
			OnCancelTap;
		Function<
			void(
				DeviceIdentifier deviceIdentifier,
				ScreenCoordinate coordinate,
				const uint16 touchRadius,
				const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers,
				Rendering::Window& window
			),
			24>
			OnTapEnded;

		// Long Press
		Function<
			void(
				DeviceIdentifier,
				ScreenCoordinate coordinate,
				const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers,
				const uint16 touchRadius,
				Rendering::Window& window
			),
			24>
			OnStartLongPress;
		Function<
			void(
				DeviceIdentifier,
				ScreenCoordinate coordinate,
				const ArrayView<const FingerIdentifier, uint8> fingers,
				const uint16 touchRadius,
				Rendering::Window& window
			),
			24>
			OnStopLongPress;
		Function<
			void(
				DeviceIdentifier,
				ScreenCoordinate,
				const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers,
				uint16 touchRadius,
				Rendering::Window& window
			),
			24>
			OnLongPressMotion;
		Function<void(DeviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window& window), 24>
			OnCancelLongPress;

		// Double Tap
		Function<
			void(
				DeviceIdentifier,
				ScreenCoordinate,
				const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers,
				uint16 touchRadius,
				Rendering::Window& window
			),
			24>
			OnDoubleTap;

		// Pan
		Function<
			void(
				DeviceIdentifier,
				ScreenCoordinate,
				const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers,
				Math::Vector2f velocity,
				uint16 touchRadius,
				Rendering::Window& window
			),
			24>
			OnStartPan;
		Function<
			void(
				DeviceIdentifier,
				ScreenCoordinate,
				Math::Vector2f velocity,
				const ArrayView<const FingerIdentifier, uint8> fingers,
				Rendering::Window& window
			),
			24>
			OnStopPan;
		Function<void(DeviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingers, Rendering::Window& window), 24> OnCancelPan;
		Function<
			void(
				DeviceIdentifier,
				ScreenCoordinate,
				Math::Vector2f velocity,
				uint16 touchRadius,
				const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers,
				Rendering::Window& window
			),
			24>
			OnMotionPan;

		// Pinch
		Function<
			void(
				DeviceIdentifier,
				ScreenCoordinate,
				const float scale,
				const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers,
				Rendering::Window& window
			),
			24>
			OnStartPinch;
		Function<
			void(DeviceIdentifier, ScreenCoordinate, const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers, Rendering::Window& window),
			24>
			OnStopPinch;
		Function<
			void(
				DeviceIdentifier,
				ScreenCoordinate,
				const float scale,
				const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers,
				Rendering::Window& window
			),
			24>
			OnMotionPinch;
		Function<void(DeviceIdentifier, const ArrayView<const FingerIdentifier, uint8> fingerIdentifiers, Rendering::Window& window), 24>
			OnCancelPinch;

		void Initialize();
		void TouchBegan(const TouchDescriptor&, DeviceIdentifier, Rendering::Window&);
		void TouchMoved(const TouchDescriptor&, DeviceIdentifier, Rendering::Window&);
		void TouchEnded(const TouchDescriptor&, DeviceIdentifier, Rendering::Window&);
		void TouchCanceled(DeviceIdentifier, const FingerIdentifier fingerIdentifier, Rendering::Window&);
	private:
		enum class TouchState : uint8
		{
			None,
			AddedTap,
			StartedTap,
			StartedLongPress,
			StartedGesture
		};

		enum class GestureType : uint8
		{
			Pan = 1 << 0,
			Pinch = 1 << 1
		};

		struct FingerData
		{
			FingerData(ScreenCoordinate startCoordinates, uint16 startRadius)
				: originalScreenCoordinate(startCoordinates)
				, screenCoordinate(startCoordinates)
				, radius(startRadius)
			{
			}

			FingerData(FingerData&& finger)
				: originalScreenCoordinate(finger.originalScreenCoordinate)
				, screenCoordinate(finger.screenCoordinate)
				, radius(finger.radius)
			{
			}

			FingerData(const FingerData&) = delete;
			FingerData& operator=(const FingerData&) = delete;

			const ScreenCoordinate originalScreenCoordinate{Math::Zero};
			ScreenCoordinate screenCoordinate{Math::Zero};
			uint16 radius{0};
		};

		struct TouchEventData
		{
			TouchEventData() = default;

			void Start(
				Rendering::Window& windowReference,
				const DeviceIdentifier device,
				const Time::Durationd startTime,
				const TouchState startTouchState,
				const uint8 startFingerCount
			)
			{
				Assert(deviceIdentifier.IsInvalid());
				Assert(device.IsValid());
				pWindow = windowReference;
				deviceIdentifier = device;
				time = startTime;
				touchState = startTouchState;
				internalFingerCounter = startFingerCount;
			}

			void Stop()
			{
				pWindow = {};
				Assert(deviceIdentifier.IsValid());
				deviceIdentifier = {};
				touchState = TouchState::None;
				internalFingerCounter = 0;
				activeGesture.Clear();
				fingerIdentifiers.Clear();
				fingers.Clear();
				span = 0;
				m_doubleTapState = {};
			}

			TouchEventData(TouchEventData&& other) = default;
			TouchEventData(const TouchEventData&) = delete;
			TouchEventData& operator=(const TouchEventData&) = delete;
			TouchEventData& operator=(TouchEventData&&) = default;

			[[nodiscard]] bool IsActive() const
			{
				return deviceIdentifier.IsValid();
			}

			Optional<Rendering::Window*> pWindow;
			DeviceIdentifier deviceIdentifier;
			Time::Durationd time;

			TouchState touchState = TouchState::None;
			uint8 internalFingerCounter{0};
			EnumFlags<GestureType> activeGesture;
			InlineVector<FingerIdentifier, 10, uint8, uint8> fingerIdentifiers;
			InlineVector<FingerData, 10, uint8, uint8> fingers;
			float span{0.0f};

			struct DoubleTapState
			{
				bool isAwaitingDoubleTap = false;
				Time::Durationd time = 0_seconds;
			};
			DoubleTapState m_doubleTapState;

			float CalculateFingerSpan() const;
			ScreenCoordinate CalculateAverageScreenCoordinate() const;
			ScreenCoordinate CalculateAverageOriginalScreenCoordinate() const;
			uint16 CalculateAverageTouchRadius() const;

			Optional<FingerData*> GetFinger(FingerIdentifier identifier);
			bool RemoveFinger(FingerIdentifier identifier);
			bool UpdateFinger(FingerIdentifier identifier, const TouchDescriptor& touch);
		};

		template<typename Callback>
		void WaitForLastFinger(
			const TouchEventIdentifier eventIdentifier,
			TouchEventData& touchEventData,
			const FingerIdentifier fingerIdentifier,
			Callback&& onLastFingerCallback
		)
		{
			Assert(touchEventData.internalFingerCounter > 0);
			touchEventData.internalFingerCounter--;
			auto it = m_fingers.Find(fingerIdentifier);
			Assert(it != m_fingers.end());
			m_fingers.Remove(it);
			if (touchEventData.internalFingerCounter == 0)
			{
				onLastFingerCallback();

				touchEventData.Stop();
				m_touchEventIdentifiers.ReturnIdentifier(eventIdentifier);
				m_lastTouchEventIdentifier = {};
			}
		}

		void Update();
	private:
		Threading::Mutex m_mutex;
		UnorderedMap<FingerIdentifier, TouchEventIdentifier> m_fingers;

		TSaltedIdentifierStorage<TouchEventIdentifier> m_touchEventIdentifiers;
		TIdentifierArray<TouchEventData, TouchEventIdentifier> m_touchEvents;

		TouchEventIdentifier m_lastTouchEventIdentifier;

		int32 m_scaledTouchSlope{100};
		int32 m_scaledMinimumScalingSpan{400};
	};
}
