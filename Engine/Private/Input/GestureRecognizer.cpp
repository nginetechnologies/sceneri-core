#include "Input/GestureRecognizer.h"

#include <Engine/Engine.h>
#include <Engine/Input/InputManager.h>

#include <Common/Threading/Jobs/RecurringAsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Memory/CallbackResult.h>
#include <Common/System/Query.h>

namespace ngine::Input
{
	void GestureRecognizer::Initialize()
	{
		Threading::Job& stage = Threading::CreateCallback(
			[this](Threading::JobRunnerThread&)
			{
				Update();

				return Threading::CallbackResult::Finished;
			},
			Threading::JobPriority::UserInputPolling,
			"Gesture Recognizer Update"
		);
		Engine& engine = System::Get<Engine>();
		engine.ModifyFrameGraph(
			[&stage]()
			{
				Input::Manager& inputManager = System::Get<Input::Manager>();
				inputManager.GetPollForInputStage().AddSubsequentStage(stage);
				stage.AddSubsequentStage(inputManager.GetPolledForInputStage());
			}
		);
	}

	void GestureRecognizer::Update()
	{
		Threading::UniqueLock lock(m_mutex);

		for (TouchEventData& event : m_touchEventIdentifiers.GetValidElementView(m_touchEvents.GetView()))
		{
			if (!event.IsActive())
			{
				continue;
			}

			if (event.m_doubleTapState.isAwaitingDoubleTap)
			{
				Time::Durationd time = Time::Durationd::GetCurrentSystemUptime() - event.m_doubleTapState.time;
				if (time >= DoubleTapTimeout)
				{
					event.m_doubleTapState = {};
				}
			}

			switch (event.touchState)
			{
				case TouchState::AddedTap:
				case TouchState::StartedTap:
				{
					Time::Durationd time = Time::Durationd::GetCurrentSystemUptime() - event.time;
					if (time >= LongPressInterval)
					{
						if (event.activeGesture.AreNoneSet())
						{
							if (event.touchState == TouchState::StartedTap)
							{
								OnCancelTap(event.deviceIdentifier, event.fingerIdentifiers, *event.pWindow);
							}
							OnStartLongPress(
								event.deviceIdentifier,
								event.CalculateAverageScreenCoordinate(),
								event.fingerIdentifiers,
								event.CalculateAverageTouchRadius(),
								*event.pWindow
							);

							event.touchState = TouchState::StartedLongPress;
						}
					}
					else if (event.touchState == TouchState::AddedTap)
					{
						OnTapStarted(
							event.deviceIdentifier,
							event.CalculateAverageScreenCoordinate(),
							event.CalculateAverageTouchRadius(),
							event.fingerIdentifiers.GetView(),
							*event.pWindow
						);
						event.touchState = TouchState::StartedTap;
					}
					break;
				}
				case TouchState::StartedLongPress:
					break;
				case TouchState::StartedGesture:
					break;
				case TouchState::None:
					ExpectUnreachable();
			}
		}
	}

	void GestureRecognizer::TouchBegan(const TouchDescriptor& touch, DeviceIdentifier deviceIdentifier, Rendering::Window& window)
	{
		Threading::UniqueLock lock(m_mutex);

		const Time::Durationd currentTime = Time::Durationd::GetCurrentSystemUptime();

		if (m_lastTouchEventIdentifier.IsValid())
		{
			Assert(m_touchEventIdentifiers.IsIdentifierActive(m_lastTouchEventIdentifier));
			TouchEventData& lastTouchEvent = m_touchEvents[m_lastTouchEventIdentifier];
			Time::Durationd deltaTime = currentTime - lastTouchEvent.time;
			// The value is smaller so add to the existing tap
			if (deltaTime <= SingleTapTimeout)
			{
				bool isNew = lastTouchEvent.GetFinger(touch.fingerIdentifier).IsInvalid();
				if (isNew)
				{
					FingerData fingerData(touch.screenCoordinate, touch.touchRadius);
					lastTouchEvent.fingerIdentifiers.EmplaceBack(touch.fingerIdentifier);
					lastTouchEvent.fingers.EmplaceBack(Move(fingerData));
					++lastTouchEvent.internalFingerCounter;
					m_fingers.Emplace(touch.fingerIdentifier, m_lastTouchEventIdentifier);
				}
				return;
			}
			else
			{
				m_lastTouchEventIdentifier = {};
			}
		}

		const TouchEventIdentifier eventIdentifier = m_touchEventIdentifiers.AcquireIdentifier();
		TouchEventData& data = m_touchEvents[eventIdentifier];
		data.Start(window, deviceIdentifier, currentTime, TouchState::AddedTap, 1);

		FingerData fingerData(touch.screenCoordinate, touch.touchRadius);
		data.fingerIdentifiers.EmplaceBack(touch.fingerIdentifier);
		data.fingers.EmplaceBack(Move(fingerData));
		m_fingers.Emplace(touch.fingerIdentifier, eventIdentifier);

		m_lastTouchEventIdentifier = eventIdentifier;
	}

	void GestureRecognizer::TouchMoved(const TouchDescriptor& touches, DeviceIdentifier deviceIdentifier, Rendering::Window& window)
	{
		Threading::UniqueLock lock(m_mutex);

		auto it = m_fingers.Find(touches.fingerIdentifier);
		if (it == m_fingers.end())
		{
			return;
		}

		const TouchEventIdentifier touchEventIdentifier = it->second;
		TouchEventData& touchEventData = m_touchEvents[touchEventIdentifier];
		Assert(m_touchEventIdentifiers.IsIdentifierActive(touchEventIdentifier));
		touchEventData.UpdateFinger(touches.fingerIdentifier, touches);

		switch (touchEventData.touchState)
		{
			case TouchState::StartedLongPress:
			{
				OnLongPressMotion(
					deviceIdentifier,
					touchEventData.CalculateAverageScreenCoordinate(),
					touchEventData.fingerIdentifiers,
					touchEventData.CalculateAverageTouchRadius(),
					window
				);
			}
			break;
			case TouchState::AddedTap:
				OnTapStarted(
					touchEventData.deviceIdentifier,
					touchEventData.CalculateAverageScreenCoordinate(),
					touchEventData.CalculateAverageTouchRadius(),
					touchEventData.fingerIdentifiers.GetView(),
					*touchEventData.pWindow
				);
				touchEventData.touchState = TouchState::StartedTap;
				break;
			case TouchState::StartedTap:
			{
				const ScreenCoordinate latestScreenCoordinate = touchEventData.CalculateAverageScreenCoordinate();
				const ScreenCoordinate originalScreenCoordinate = touchEventData.CalculateAverageOriginalScreenCoordinate();
				const Math::Vector2i deltaCoordinate = (Math::Vector2i)latestScreenCoordinate - (Math::Vector2i)originalScreenCoordinate;
				const Math::Vector2f velocity = (Math::Vector2f)latestScreenCoordinate - (Math::Vector2f)originalScreenCoordinate;
				if (deltaCoordinate.GetLength() >= m_scaledTouchSlope)
				{
					OnStartPan(
						deviceIdentifier,
						latestScreenCoordinate,
						touchEventData.fingerIdentifiers,
						velocity,
						touchEventData.CalculateAverageTouchRadius(),
						window
					);
					touchEventData.activeGesture.Set(GestureType::Pan, true);

					if (touchEventData.activeGesture.AreAnySet())
					{
						touchEventData.touchState = TouchState::StartedGesture;
					}
				}
				if (touchEventData.fingers.GetSize() == 2u)
				{
					const float span = touchEventData.CalculateFingerSpan();
					if (touchEventData.span == 0.f)
					{
						touchEventData.span = span;
					}
					if (span >= float(m_scaledMinimumScalingSpan))
					{
						const float scale = span / touchEventData.span;
						OnStartPinch(
							deviceIdentifier,
							touchEventData.CalculateAverageScreenCoordinate(),
							scale,
							touchEventData.fingerIdentifiers,
							window
						);
						touchEventData.activeGesture.Set(GestureType::Pinch, true);
					}

					if (touchEventData.activeGesture.AreAnySet())
					{
						touchEventData.touchState = TouchState::StartedGesture;
					}
				}
			}
			break;
			case TouchState::StartedGesture:
			{
				const ScreenCoordinate latestScreenCoordinate = touchEventData.CalculateAverageScreenCoordinate();
				const ScreenCoordinate originalScreenCoordinate = touchEventData.CalculateAverageOriginalScreenCoordinate();
				const Math::Vector2i deltaCoordinate = (Math::Vector2i)latestScreenCoordinate - (Math::Vector2i)originalScreenCoordinate;
				const Math::Vector2f velocity = (Math::Vector2f)latestScreenCoordinate - (Math::Vector2f)originalScreenCoordinate;

				// Panning
				if (touchEventData.activeGesture.IsSet(GestureType::Pan))
				{
					OnMotionPan(
						deviceIdentifier,
						latestScreenCoordinate,
						velocity,
						touchEventData.CalculateAverageTouchRadius(),
						touchEventData.fingerIdentifiers,
						window
					);
				}
				else if (deltaCoordinate.GetLength() >= m_scaledTouchSlope)
				{
					OnStartPan(
						deviceIdentifier,
						latestScreenCoordinate,
						touchEventData.fingerIdentifiers,
						velocity,
						touchEventData.CalculateAverageTouchRadius(),
						window
					);
					touchEventData.activeGesture.Set(GestureType::Pan, true);
				}

				// Pinching
				if (touchEventData.activeGesture.IsSet(GestureType::Pinch))
				{
					if (touchEventData.fingers.GetSize() == 2u)
					{
						const float span = touchEventData.CalculateFingerSpan();
						const float scale = span / touchEventData.span;
						OnMotionPinch(
							deviceIdentifier,
							touchEventData.CalculateAverageScreenCoordinate(),
							scale,
							touchEventData.fingerIdentifiers,
							window
						);
					}
				}
				else if (touchEventData.fingers.GetSize() == 2u)
				{
					const float span = touchEventData.CalculateFingerSpan();
					if (touchEventData.span == 0.f)
					{
						touchEventData.span = span;
					}
					if (span >= float(m_scaledMinimumScalingSpan))
					{
						const float scale = span / touchEventData.span;
						OnStartPinch(
							deviceIdentifier,
							touchEventData.CalculateAverageScreenCoordinate(),
							scale,
							touchEventData.fingerIdentifiers,
							window
						);
						touchEventData.activeGesture.Set(GestureType::Pinch, true);
					}
				}
			}
			break;
			default:
				break;
		}
	}

	void GestureRecognizer::TouchEnded(const TouchDescriptor& touch, DeviceIdentifier, Rendering::Window&)
	{
		Threading::UniqueLock lock(m_mutex);

		auto it = m_fingers.Find(touch.fingerIdentifier);
		if (it == m_fingers.end())
		{
			return;
		}

		const TouchEventIdentifier touchEventIdentifier = it->second;
		Assert(m_touchEventIdentifiers.IsIdentifierActive(touchEventIdentifier));
		TouchEventData& touchEventData = m_touchEvents[touchEventIdentifier];
		touchEventData.UpdateFinger(touch.fingerIdentifier, touch);

		switch (touchEventData.touchState)
		{
			case TouchState::AddedTap:
			{
				OnTapStarted(
					touchEventData.deviceIdentifier,
					touchEventData.CalculateAverageScreenCoordinate(),
					touchEventData.CalculateAverageTouchRadius(),
					touchEventData.fingerIdentifiers.GetView(),
					*touchEventData.pWindow
				);
				touchEventData.touchState = TouchState::StartedTap;
			}
				[[fallthrough]];
			case TouchState::StartedTap:
			{
				WaitForLastFinger(
					touchEventIdentifier,
					touchEventData,
					touch.fingerIdentifier,
					[this, &touchEventData]()
					{
						const ScreenCoordinate screenCoordinate = touchEventData.CalculateAverageScreenCoordinate();
						const uint16 radius = touchEventData.CalculateAverageTouchRadius();

						if (touchEventData.m_doubleTapState.isAwaitingDoubleTap)
						{
							OnCancelTap(touchEventData.deviceIdentifier, touchEventData.fingerIdentifiers, *touchEventData.pWindow);
							OnDoubleTap(
								touchEventData.deviceIdentifier,
								screenCoordinate,
								touchEventData.fingerIdentifiers,
								radius,
								*touchEventData.pWindow
							);
							touchEventData.m_doubleTapState = {};
						}
						else
						{
							OnTapEnded(
								touchEventData.deviceIdentifier,
								screenCoordinate,
								radius,
								touchEventData.fingerIdentifiers,
								*touchEventData.pWindow
							);

							touchEventData.m_doubleTapState.time = Time::Durationd::GetCurrentSystemUptime();
							touchEventData.m_doubleTapState.isAwaitingDoubleTap = true;
						}
					}
				);
			}
			break;
			case TouchState::StartedLongPress:
			{
				WaitForLastFinger(
					touchEventIdentifier,
					touchEventData,
					touch.fingerIdentifier,
					[this, &touchEventData]()
					{
						const uint16 radius = touchEventData.CalculateAverageTouchRadius();

						const ScreenCoordinate screenCoordinate = touchEventData.CalculateAverageScreenCoordinate();
						OnStopLongPress(
							touchEventData.deviceIdentifier,
							screenCoordinate,
							touchEventData.fingerIdentifiers,
							radius,
							*touchEventData.pWindow
						);
					}
				);
			}
			break;
			case TouchState::StartedGesture:
			{
				WaitForLastFinger(
					touchEventIdentifier,
					touchEventData,
					touch.fingerIdentifier,
					[this, &touchEventData]()
					{
						if (touchEventData.activeGesture.IsSet(GestureType::Pan))
						{
							const ScreenCoordinate screenCoordinate = touchEventData.CalculateAverageScreenCoordinate();
							const ScreenCoordinate originalScreenCoordinate = touchEventData.CalculateAverageOriginalScreenCoordinate();
							const Math::Vector2f velocity = (Math::Vector2f)screenCoordinate - (Math::Vector2f)originalScreenCoordinate;
							OnStopPan(
								touchEventData.deviceIdentifier,
								screenCoordinate,
								velocity,
								touchEventData.fingerIdentifiers,
								*touchEventData.pWindow
							);
							touchEventData.activeGesture.Clear(GestureType::Pan);
						}

						if (touchEventData.activeGesture.IsSet(GestureType::Pinch))
						{
							OnStopPinch(
								touchEventData.deviceIdentifier,
								touchEventData.CalculateAverageScreenCoordinate(),
								touchEventData.fingerIdentifiers,
								*touchEventData.pWindow
							);
							touchEventData.activeGesture.Clear(GestureType::Pinch);
						}
					}
				);
			}
			break;
			case TouchState::None:
				ExpectUnreachable();
		}
	}

	void GestureRecognizer::TouchCanceled(DeviceIdentifier, const FingerIdentifier fingerIdentifier, Rendering::Window&)
	{
		Threading::UniqueLock lock(m_mutex);

		auto it = m_fingers.Find(fingerIdentifier);
		if (it == m_fingers.end())
		{
			return;
		}

		const TouchEventIdentifier touchEventIdentifier = it->second;
		Assert(m_touchEventIdentifiers.IsIdentifierActive(touchEventIdentifier));
		TouchEventData& touchEventData = m_touchEvents[touchEventIdentifier];

		switch (touchEventData.touchState)
		{
			case TouchState::None:
				break;
			case TouchState::AddedTap:
				break;
			case TouchState::StartedTap:
				OnCancelTap(touchEventData.deviceIdentifier, touchEventData.fingerIdentifiers, *touchEventData.pWindow);
				break;
			case TouchState::StartedLongPress:
				OnCancelLongPress(touchEventData.deviceIdentifier, touchEventData.fingerIdentifiers, *touchEventData.pWindow);
				break;
			case TouchState::StartedGesture:
				if (touchEventData.activeGesture.IsSet(GestureType::Pan))
				{
					OnCancelPan(touchEventData.deviceIdentifier, touchEventData.fingerIdentifiers, *touchEventData.pWindow);
				}
				if (touchEventData.activeGesture.IsSet(GestureType::Pinch))
				{
					OnCancelPinch(touchEventData.deviceIdentifier, touchEventData.fingerIdentifiers, *touchEventData.pWindow);
				}
				break;
		}

		WaitForLastFinger(
			touchEventIdentifier,
			touchEventData,
			fingerIdentifier,
			[]()
			{
			}
		);
	}

	float GestureRecognizer::TouchEventData::CalculateFingerSpan() const
	{
		float tempSpan{0.f};
		for (uint8 i = 0; i < fingers.GetSize(); ++i)
		{
			const uint8 nextFinger = i + 1;
			if (fingers.IsValidIndex(nextFinger))
			{
				tempSpan += (float)(fingers[i].screenCoordinate - fingers[nextFinger].screenCoordinate).GetLength();
			}
		}

		return tempSpan;
	}

	ScreenCoordinate GestureRecognizer::TouchEventData::CalculateAverageScreenCoordinate() const
	{
		ScreenCoordinate position{Math::Zero};
		for (const FingerData& finger : fingers)
		{
			position += finger.screenCoordinate;
		}

		return position / ScreenCoordinate(fingers.GetSize());
	}

	ScreenCoordinate GestureRecognizer::TouchEventData::CalculateAverageOriginalScreenCoordinate() const
	{
		ScreenCoordinate position{Math::Zero};
		for (const FingerData& finger : fingers)
		{
			position += finger.originalScreenCoordinate;
		}

		return position / ScreenCoordinate(fingers.GetSize());
	}

	uint16 GestureRecognizer::TouchEventData::CalculateAverageTouchRadius() const
	{
		float radius{0.f};
		for (const FingerData& finger : fingers)
		{
			radius += finger.radius;
		}

		radius *= 0.5f;
		radius /= float(fingers.GetSize());

		if (radius == 0.f)
		{
			radius = 23.f;
		}
		return uint16(radius);
	}

	bool GestureRecognizer::TouchEventData::RemoveFinger(FingerIdentifier identifier)
	{
		if (const OptionalIterator<FingerIdentifier> foundIdentifier = fingerIdentifiers.Find(identifier))
		{
			const uint8 index = fingerIdentifiers.GetIteratorIndex(foundIdentifier);
			fingerIdentifiers.RemoveAt(index);
			fingers.RemoveAt(index);
			return true;
		}
		return false;
	}

	bool GestureRecognizer::TouchEventData::UpdateFinger(FingerIdentifier identifier, const TouchDescriptor& touch)
	{
		if (Optional<FingerData*> pFinger = GetFinger(identifier))
		{
			pFinger->screenCoordinate = touch.screenCoordinate;
			pFinger->radius = touch.touchRadius;
			return true;
		}

		return false;
	}

	Optional<GestureRecognizer::FingerData*> GestureRecognizer::TouchEventData::GetFinger(FingerIdentifier identifier)
	{
		if (const OptionalIterator<FingerIdentifier> foundIdentifier = fingerIdentifiers.Find(identifier))
		{
			const uint8 index = fingerIdentifiers.GetIteratorIndex(foundIdentifier);
			return fingers[index];
		}

		return nullptr;
	}

}
