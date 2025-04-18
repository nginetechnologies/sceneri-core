#include "Input/Devices/Mouse/Mouse.h"
#include "Input/InputManager.h"
#include "Input/Monitor.h"
#include "Input/ScreenCoordinate.h"

#include "Input/Devices/Keyboard/Keyboard.h"

#include <Common/Math/Vector2.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobManager.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Serialization/Reader.h>
#include <Common/System/Query.h>

#include <Renderer/Window/Window.h>

#if PLATFORM_WINDOWS
#include <Common/Platform/Windows.h>
#endif

namespace ngine::Input
{
	inline Array<MouseDeviceType::ButtonIdentifiers, (uint8)MouseButton::Count> CreateButtonIdentifiers(Manager& manager)
	{
		Array<MouseDeviceType::ButtonIdentifiers, (uint8)MouseButton::Count> result;
		for (uint8 i = 0; i < (uint8)MouseButton::Count; ++i)
		{
			result[i] = {manager.RegisterInput(), manager.RegisterInput()};
		}
		return result;
	}

	MouseDeviceType::MouseDeviceType(const DeviceTypeIdentifier identifier, Input::Manager& manager)
		: DeviceType(identifier)
		, m_manager(manager)
		, m_moveCursorInputIdentifier(manager.RegisterInput())
		, m_motionHoverInputIdentifier(manager.RegisterInput())
		, m_motionDragInputIdentifier(manager.RegisterInput())
		, m_buttonIdentifiers(CreateButtonIdentifiers(manager))
		, m_scrollInputIdentifier(manager.RegisterInput())
	{
	}

	Input::DeviceIdentifier MouseDeviceType::GetOrRegisterInstance(
		const uintptr platformIdentifier, Input::Manager& manager, Optional<Rendering::Window*> pSourceWindow
	)
	{
		Optional<const Mouse*> pMouse;

		manager.IterateDeviceInstances(
			[identifier = m_identifier, platformIdentifier, &pMouse](const DeviceInstance& instance) -> Memory::CallbackResult
			{
				if (instance.GetTypeIdentifier() == identifier)
				{
					if (static_cast<const Mouse&>(instance).m_platformIdentifier == platformIdentifier)
					{
						pMouse = static_cast<const Mouse&>(instance);
						return Memory::CallbackResult::Break;
					}
				}

				return Memory::CallbackResult::Continue;
			}
		);

		if (LIKELY(pMouse.IsValid()))
		{
			return pMouse->GetIdentifier();
		}

		return manager.RegisterDeviceInstance<Mouse>(
			m_identifier,
			pSourceWindow.IsValid() ? pSourceWindow->GetFocusedInputMonitor() : Optional<Monitor*>{},
			platformIdentifier
		);
	}

	Input::DeviceIdentifier MouseDeviceType::FindInstance(const uintptr platformIdentifier, Input::Manager& manager)
	{
		Optional<const Mouse*> pMouse;

		manager.IterateDeviceInstances(
			[identifier = m_identifier, platformIdentifier, &pMouse](const DeviceInstance& instance) -> Memory::CallbackResult
			{
				if (instance.GetTypeIdentifier() == identifier)
				{
					if (static_cast<const Mouse&>(instance).m_platformIdentifier == platformIdentifier)
					{
						pMouse = static_cast<const Mouse&>(instance);
						return Memory::CallbackResult::Break;
					}
				}

				return Memory::CallbackResult::Continue;
			}
		);

		if (LIKELY(pMouse.IsValid()))
		{
			return pMouse->GetIdentifier();
		}

		return Input::DeviceIdentifier();
	}

	void MouseDeviceType::OnMotion(Input::DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, Rendering::Window& window)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, &window]()
			{
				Mouse& mouse = static_cast<Mouse&>(m_manager.GetDeviceInstance(deviceIdentifier));

				const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
				if (mouse.m_buttonStates.IsEmpty())
				{
					Input::Monitor* pFocusedMonitor = window.GetMouseMonitorAtCoordinates(windowCoordinate, Invalid);
					if (mouse.GetActiveMonitor() != pFocusedMonitor)
					{
						mouse.SetActiveMonitor(pFocusedMonitor, *this);
					}
				}

				if (Monitor* pMonitor = mouse.GetActiveMonitor())
				{
					const Math::Vector2i deltaCoordinates = (Math::Vector2i)screenCoordinate - mouse.m_previousCoordinates;
					mouse.m_previousCoordinates = (Math::Vector2i)screenCoordinate;

					pMonitor->On2DAnalogInput(
						deviceIdentifier,
						m_moveCursorInputIdentifier,
						static_cast<Math::Vector2f>(screenCoordinate),
						static_cast<Math::Vector2f>(deltaCoordinates)
					);

					if (mouse.m_buttonStates.IsSet(MouseButton::Left))
					{
						pMonitor->On2DSurfaceDraggingMotionInput(deviceIdentifier, m_motionDragInputIdentifier, screenCoordinate, deltaCoordinates);
					}
					else
					{
						// Cancel double click if we moved too much
						ClickState expectedState = ClickState::AwaitingSecondPress;
						mouse.m_clickState.CompareExchangeStrong(expectedState, ClickState::AwaitingPress);

						pMonitor->On2DSurfaceHoveringMotionInput(deviceIdentifier, m_motionHoverInputIdentifier, screenCoordinate, deltaCoordinates);
					}
				}
			}
		);
	}

	void MouseDeviceType::OnMotion(
		const Input::DeviceIdentifier deviceIdentifier,
		const ScreenCoordinate screenCoordinate,
		const Math::Vector2i deltaCoordinates,
		Rendering::Window& window
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, deltaCoordinates, &window]()
			{
				Mouse& mouse = static_cast<Mouse&>(m_manager.GetDeviceInstance(deviceIdentifier));

				const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
				if (mouse.m_buttonStates.IsEmpty())
				{
					Input::Monitor* pFocusedMonitor = window.GetMouseMonitorAtCoordinates(windowCoordinate, Invalid);
					if (mouse.GetActiveMonitor() != pFocusedMonitor)
					{
						mouse.SetActiveMonitor(pFocusedMonitor, *this);
					}
				}

				if (Monitor* pMonitor = mouse.GetActiveMonitor())
				{
					mouse.m_previousCoordinates = (Math::Vector2i)screenCoordinate;

					pMonitor->On2DAnalogInput(
						deviceIdentifier,
						m_moveCursorInputIdentifier,
						static_cast<Math::Vector2f>(screenCoordinate),
						static_cast<Math::Vector2f>(deltaCoordinates)
					);

					if (mouse.m_buttonStates.IsSet(MouseButton::Left))
					{
						pMonitor->On2DSurfaceDraggingMotionInput(deviceIdentifier, m_motionDragInputIdentifier, screenCoordinate, deltaCoordinates);
					}
					else
					{
						// Cancel double click if we moved too much
						ClickState expectedState = ClickState::AwaitingSecondPress;
						mouse.m_clickState.CompareExchangeStrong(expectedState, ClickState::AwaitingPress);

						pMonitor->On2DSurfaceHoveringMotionInput(deviceIdentifier, m_motionHoverInputIdentifier, screenCoordinate, deltaCoordinates);
					}
				}
			}
		);
	}

	[[nodiscard]] Time::Durationf MouseDeviceType::GetMaximumDoubleClickDelay()
	{
#if PLATFORM_WINDOWS
		return Time::Durationf::FromMilliseconds((uint64)GetDoubleClickTime());
#else
		return 500_milliseconds;
#endif
	}

	[[nodiscard]] Time::Durationf MouseDeviceType::GetLongPressDelay()
	{
		return GetMaximumDoubleClickDelay();
	}

	bool MouseDeviceType::IsWithinDoubleClickTime(const Time::Timestamp lastClickTime) const
	{
		const Time::Durationf timeSinceLastClick = (Time::Timestamp::GetCurrent() - lastClickTime).GetDuration();
		const Time::Durationf doubleClickTime = GetMaximumDoubleClickDelay();
		return timeSinceLastClick <= doubleClickTime;
	}

	inline static constexpr Time::Durationf MaximumScrollTime{200_milliseconds};

	bool MouseDeviceType::IsWithinScrollTime(const Time::Timestamp lastClickTime) const
	{
		const Time::Durationf timeSinceScroll = (Time::Timestamp::GetCurrent() - lastClickTime).GetDuration();
		const Time::Durationf lastScrollTime = MaximumScrollTime;
		return timeSinceScroll <= lastScrollTime;
	}

	void MouseDeviceType::OnPress(
		DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const MouseButton button, Rendering::Window& window
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, button, &window]()
			{
				Mouse& mouse = static_cast<Mouse&>(m_manager.GetDeviceInstance(deviceIdentifier));
				Assert(!mouse.m_buttonStates.IsSet(button));
				mouse.m_buttonStates |= button;

				const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
				{
					Input::Monitor* pFocusedMonitor = window.GetMouseMonitorAtCoordinates(windowCoordinate, Invalid);
					mouse.m_pMonitorDuringLastPress = pFocusedMonitor;
					if (mouse.GetActiveMonitor() != pFocusedMonitor)
					{
						mouse.SetActiveMonitor(pFocusedMonitor, *this);
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

				switch (button)
				{
					case MouseButton::Left:
					{
						Monitor* pMonitor = mouse.GetActiveMonitor();

						switch ((ClickState)mouse.m_clickState)
						{
							case ClickState::SentSingleClickStartPress:
							{
								if (pMonitor != nullptr)
								{
									ClickState existingState = ClickState::SentSingleClickStartPress;
									const ClickState newState = IsWithinDoubleClickTime(mouse.m_lastClickTime) ? ClickState::AwaitingSecondPress
								                                                                             : ClickState::AwaitingPress;
									[[maybe_unused]] const bool exchanged = mouse.m_clickState.CompareExchangeStrong(existingState, newState);
									Assert(exchanged);
									pMonitor->On2DSurfaceStopPressInput(
										deviceIdentifier,
										m_buttonIdentifiers[GetButtonIndex(button)].m_release,
										screenCoordinate,
										1
									);
								}
							}
								[[fallthrough]];
							case ClickState::AwaitingPress:
								if (pMonitor != nullptr)
								{
									pMonitor->On2DSurfaceStartPressInput(
										deviceIdentifier,
										m_buttonIdentifiers[GetButtonIndex(button)].m_press,
										screenCoordinate,
										1
									);
									mouse.m_clickState = ClickState::SentSingleClickStartPress;
									mouse.m_lastClickTime = Time::Timestamp::GetCurrent();
								}
								break;
							case ClickState::AwaitingSecondPress:
							{
								if (IsWithinDoubleClickTime(mouse.m_lastClickTime))
								{
									if (pMonitor != nullptr)
									{
										pMonitor->On2DSurfaceStartPressInput(
											deviceIdentifier,
											m_buttonIdentifiers[GetButtonIndex(button)].m_press,
											screenCoordinate,
											2
										);
										mouse.m_clickState = ClickState::SentDoubleClickStartPress;
									}
								}
								else
								{
									if (pMonitor != nullptr)
									{
										pMonitor->On2DSurfaceStartPressInput(
											deviceIdentifier,
											m_buttonIdentifiers[GetButtonIndex(button)].m_press,
											screenCoordinate,
											1
										);
										mouse.m_clickState = ClickState::SentSingleClickStartPress;
										mouse.m_lastClickTime = Time::Timestamp::GetCurrent();
									}
								}
							}
							break;
							case ClickState::SentDoubleClickStartPress:
								Assert(false, "Should not be reached!");
								break;
						};
					}
					break;
					default:
					{
						if (Monitor* pMonitor = mouse.GetActiveMonitor())
						{
							pMonitor
								->On2DSurfaceStartPressInput(deviceIdentifier, m_buttonIdentifiers[GetButtonIndex(button)].m_press, screenCoordinate, 1);
						}
					}
					break;
				}
			}
		);
	}

	void MouseDeviceType::OnRelease(
		DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const MouseButton button, Rendering::Window* pWindow
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, button, pWindow]()
			{
				Mouse& mouse = static_cast<Mouse&>(m_manager.GetDeviceInstance(deviceIdentifier));
				if (mouse.m_buttonStates.IsSet(button))
				{
					mouse.m_buttonStates &= ~button;

					if (Monitor* pMonitor = mouse.GetActiveMonitor())
					{
						if (pWindow != nullptr)
						{
							switch (button)
							{
								case MouseButton::Left:
								{
									ClickState state = mouse.m_clickState;
									switch (state)
									{
										case ClickState::AwaitingPress:
											Assert(false, "TODO: Should not be reached!");
											break;
										case ClickState::AwaitingSecondPress:
											Assert(false, "TODO: Should not be reached!");
											break;
										case ClickState::SentSingleClickStartPress:
										{
											const ClickState newState = IsWithinDoubleClickTime(mouse.m_lastClickTime) ? ClickState::AwaitingSecondPress
										                                                                             : ClickState::AwaitingPress;
											[[maybe_unused]] const bool exchanged = mouse.m_clickState.CompareExchangeStrong(state, newState);
											Assert(exchanged);
											mouse.m_pMonitorDuringLastPress->On2DSurfaceStopPressInput(
												deviceIdentifier,
												m_buttonIdentifiers[GetButtonIndex(button)].m_release,
												screenCoordinate,
												1
											);
										}
										break;
										case ClickState::SentDoubleClickStartPress:
										{
											[[maybe_unused]] const bool exchanged = mouse.m_clickState.CompareExchangeStrong(state, ClickState::AwaitingPress);
											Assert(exchanged);
											mouse.m_pMonitorDuringLastPress->On2DSurfaceStopPressInput(
												deviceIdentifier,
												m_buttonIdentifiers[GetButtonIndex(button)].m_release,
												screenCoordinate,
												2
											);
										}
										break;
									}
								}
								break;
								default:
								{
									mouse.m_pMonitorDuringLastPress->On2DSurfaceStopPressInput(
										deviceIdentifier,
										m_buttonIdentifiers[GetButtonIndex(button)].m_release,
										screenCoordinate,
										1
									);
								}
								break;
							}
						}
						else
						{
							pMonitor->On2DSurfaceCancelPressInput(deviceIdentifier, m_buttonIdentifiers[GetButtonIndex(button)].m_release);
						}
					}
				}
			}
		);
	}

	void MouseDeviceType::OnPressCancelled(DeviceIdentifier deviceIdentifier, const MouseButton button, Rendering::Window* pWindow)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, button, pWindow]()
			{
				Mouse& mouse = static_cast<Mouse&>(m_manager.GetDeviceInstance(deviceIdentifier));
				if (mouse.m_buttonStates.IsSet(button))
				{
					mouse.m_buttonStates &= ~button;

					if (Monitor* pMonitor = mouse.GetActiveMonitor())
					{
						if (pWindow != nullptr)
						{
							switch (button)
							{
								case MouseButton::Left:
								{
									ClickState state = mouse.m_clickState;
									switch (state)
									{
										case ClickState::AwaitingPress:
											Assert(false, "Should not be reached!");
											break;
										case ClickState::AwaitingSecondPress:
											Assert(false, "Should not be reached!");
											break;
										case ClickState::SentSingleClickStartPress:
										{
											const ClickState newState = IsWithinDoubleClickTime(mouse.m_lastClickTime) ? ClickState::AwaitingSecondPress
										                                                                             : ClickState::AwaitingPress;
											[[maybe_unused]] const bool exchanged = mouse.m_clickState.CompareExchangeStrong(state, newState);
											Assert(exchanged);
											mouse.m_pMonitorDuringLastPress
												->On2DSurfaceCancelPressInput(deviceIdentifier, m_buttonIdentifiers[GetButtonIndex(button)].m_release);
										}
										break;
										case ClickState::SentDoubleClickStartPress:
										{
											[[maybe_unused]] const bool exchanged = mouse.m_clickState.CompareExchangeStrong(state, ClickState::AwaitingPress);
											Assert(exchanged);
											mouse.m_pMonitorDuringLastPress
												->On2DSurfaceCancelPressInput(deviceIdentifier, m_buttonIdentifiers[GetButtonIndex(button)].m_release);
										}
										break;
									}
								}
								break;
								default:
								{
									mouse.m_pMonitorDuringLastPress
										->On2DSurfaceCancelPressInput(deviceIdentifier, m_buttonIdentifiers[GetButtonIndex(button)].m_release);
								}
								break;
							}
						}
						else
						{
							pMonitor->On2DSurfaceCancelPressInput(deviceIdentifier, m_buttonIdentifiers[GetButtonIndex(button)].m_release);
						}
					}
				}
			}
		);
	}

	MouseDeviceType::Mouse::~Mouse()
	{
		if (m_scheduledTimerHandle.IsValid())
		{
			System::Get<Threading::JobManager>().CancelAsyncJob(m_scheduledTimerHandle);
		}
	}

	void MouseDeviceType::OnStartScroll(
		DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const Math::Vector2i delta, Rendering::Window& window
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, delta, &window]()
			{
				OnScrollInternal(ScrollState::Scrolling, deviceIdentifier, screenCoordinate, delta, Math::Zero, window);
			}
		);
	}

	void MouseDeviceType::OnScroll(
		DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const Math::Vector2i delta, Rendering::Window& window
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, delta, &window]()
			{
				OnScrollInternal(ScrollState::Scrolling, deviceIdentifier, screenCoordinate, delta, Math::Zero, window);
			}
		);
	}

	void MouseDeviceType::OnEndScroll(
		DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, const Math::Vector2f velocity, Rendering::Window& window
	)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, velocity, &window]()
			{
				OnScrollInternal(ScrollState::End, deviceIdentifier, screenCoordinate, Math::Zero, velocity, window);
			}
		);
	}

	void MouseDeviceType::OnCancelScroll(DeviceIdentifier deviceIdentifier, ScreenCoordinate screenCoordinate, Rendering::Window& window)
	{
		m_manager.QueueInput(
			[this, deviceIdentifier, screenCoordinate, &window]()
			{
				OnScrollInternal(ScrollState::Cancel, deviceIdentifier, screenCoordinate, Math::Zero, Math::Zero, window);
			}
		);
	}

	void MouseDeviceType::OnScrollInternal(
		const ScrollState state,
		DeviceIdentifier deviceIdentifier,
		ScreenCoordinate screenCoordinate,
		const Math::Vector2i delta,
		const Math::Vector2f velocity,
		Rendering::Window& window
	)
	{
		Mouse& mouse = static_cast<Mouse&>(m_manager.GetDeviceInstance(deviceIdentifier));

		if (mouse.m_scheduledTimerHandle.IsValid())
		{
			Threading::JobManager& jobManager = System::Get<Threading::JobManager>();
			jobManager.CancelAsyncJob(mouse.m_scheduledTimerHandle);
			mouse.m_scheduledTimerHandle = {};
		}

		const WindowCoordinate windowCoordinate = window.ConvertScreenToLocalCoordinates(screenCoordinate);
		if (mouse.m_buttonStates.IsEmpty())
		{
			Input::Monitor* pFocusedMonitor = window.GetMouseMonitorAtCoordinates(windowCoordinate, Invalid);
			if (mouse.GetActiveMonitor() != pFocusedMonitor)
			{
				mouse.SetActiveMonitor(pFocusedMonitor, *this);
			}
		}

		switch (state)
		{
			case ScrollState::Scrolling:
			{
				auto onScroll = [this, &mouse, deviceIdentifier, screenCoordinate, &window]()
				{
					if (const Optional<Threading::JobManager*> pJobManager = System::Find<Threading::JobManager>())
					{
						mouse.m_scheduledTimerHandle = pJobManager->ScheduleAsync(
							MaximumScrollTime,
							[this, &mouse, deviceIdentifier, screenCoordinate, &window](Threading::JobRunnerThread&)
							{
								bool expected = true;
								if (mouse.m_isScrolling.CompareExchangeStrong(expected, false))
								{
									OnEndScroll(deviceIdentifier, screenCoordinate, Math::Vector2f(Math::Zero), window);
									mouse.m_scheduledTimerHandle = {};
								}
							},
							Threading::JobPriority::UserInterfaceAction
						);
					}
				};

				const bool wasScrolling = mouse.m_isScrolling;
				if (wasScrolling)
				{
					if (delta.x != 0 || delta.y != 0)
					{
						if (IsWithinScrollTime(mouse.m_lastScrollTime))
						{
							if (Monitor* pMonitor = mouse.GetActiveMonitor())
							{
								pMonitor->On2DSurfaceScrollInput(deviceIdentifier, m_scrollInputIdentifier, screenCoordinate, delta);
							}
							onScroll();
						}
						else
						{
							// Scroll timed out, end
							OnScrollInternal(ScrollState::End, deviceIdentifier, screenCoordinate, delta, velocity, window);
							OnScrollInternal(ScrollState::Scrolling, deviceIdentifier, screenCoordinate, delta, velocity, window);
							return;
						}
					}
					else
					{
						OnScrollInternal(ScrollState::End, deviceIdentifier, screenCoordinate, delta, velocity, window);
					}
				}
				else
				{
					if (delta.x == 0 && delta.y == 0)
					{
						return;
					}

					bool expected = false;
					if (mouse.m_isScrolling.CompareExchangeStrong(expected, true))
					{
						if (Monitor* pMonitor = mouse.GetActiveMonitor())
						{
							pMonitor->On2DSurfaceStartScrollInput(deviceIdentifier, m_scrollInputIdentifier, screenCoordinate, delta);
						}
						onScroll();
					}
				}
			}
			break;
			case ScrollState::End:
			case ScrollState::Cancel:
			{
				bool expected = true;
				if (mouse.m_isScrolling.CompareExchangeStrong(expected, false))
				{
					if (Monitor* pMonitor = mouse.GetActiveMonitor())
					{
						if (state == ScrollState::End)
						{
							pMonitor->On2DSurfaceEndScrollInput(deviceIdentifier, m_scrollInputIdentifier, screenCoordinate, velocity);
						}
						else
						{
							Assert(state == ScrollState::Cancel);
							pMonitor->On2DSurfaceCancelScrollInput(deviceIdentifier, m_scrollInputIdentifier, screenCoordinate);
						}
					}
				}
			}
			break;
		}

		mouse.m_lastScrollTime = Time::Timestamp::GetCurrent();
	}

	InputIdentifier MouseDeviceType::DeserializeDeviceInput(const Serialization::Reader& reader) const
	{
		using MouseButtonType = UNDERLYING_TYPE(MouseButton);
		Optional<MouseButtonType> buttonValue = reader.Read<MouseButtonType>("button");
		bool stateValue =
			reader.ReadWithDefaultValue<bool>("pressed", true); // We're going to assume that button pressed is the default desired action
		Optional<Motion> motion = reader.Read<Motion>("motion");

		if (buttonValue.IsValid())
		{
			return (stateValue) ? GetButtonPressInputIdentifier(static_cast<MouseButton>(1 << (*buttonValue)))
			                    : GetButtonReleaseInputIdentifier(static_cast<MouseButton>(1 << (*buttonValue)));
		}

		if (motion.IsValid())
		{
			switch (*motion)
			{
				case Motion::MoveCursor:
					return GetMoveCursorInputIdentifier();
				case Motion::Hover:
					return GetHoveringMotionInputIdentifier();
				case Motion::Drag:
					return GetDraggingMotionInputIdentifier();
				case Motion::Scroll:
					return GetScrollInputIdentifier();
			}
		}

		return {};
	}
}
