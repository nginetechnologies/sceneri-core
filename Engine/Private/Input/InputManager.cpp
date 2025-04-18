#include "Input/InputManager.h"

#include "Input/Devices/Mouse/Mouse.h"
#include "Input/Devices/Keyboard/Keyboard.h"
#include "Input/Devices/Touchscreen/Touchscreen.h"
#include "Input/Devices/Gamepad/Gamepad.h"

#include <Renderer/Renderer.h>
#include <Renderer/Window/Window.h>

#include <Engine/Engine.h>
#include <Engine/Threading/JobRunnerThread.h>

#include <Common/Memory/OffsetOf.h>
#include <Common/Function/Event.h>
#include <Common/Platform/Windows.h>
#include <Common/System/Query.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/RecurringAsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Input
{
#if PLATFORM_WINDOWS && !USE_SDL
	static LRESULT __cdecl ProcessDummyWindowMessage(const HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
	{
		switch (message)
		{
			case WM_INPUT:
			{
				const LONG_PTR userData = GetWindowLongPtrA(windowHandle, GWLP_USERDATA);
				if (userData == 0)
				{
					return DefWindowProc(windowHandle, message, wParam, lParam);
				}

				if ((GetMessageExtraInfo() & 0x82) == 0x82)
				{
					/* ignore event */
					return DefWindowProc(windowHandle, message, wParam, lParam);
				}

				Manager& inputManager = *reinterpret_cast<Manager*>(userData);
				inputManager.HandleWindowsRawInput(lParam);
			}
			break;
			case WM_TOUCH:
			{
				CloseTouchInputHandle((HTOUCHINPUT)lParam);
				return 0;
			}
		}

		return DefWindowProc(windowHandle, message, wParam, lParam);
	}
#endif

#if PLATFORM_WINDOWS && !USE_SDL
	static ATOM dummyWindowClassAtom = 0;
	static HWND dummyWindowHandle = nullptr;
#endif

	Manager::Manager()
		: m_pollForInputStage(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					bool expected = false;
					[[maybe_unused]] const bool wasExchanged = m_canProcessInput.CompareExchangeStrong(expected, true);
					Assert(wasExchanged);
					return Threading::CallbackResult::Finished;
				},
				Threading::JobPriority::UserInputPolling,
				"Poll Inputs"
			))
		, m_processInputQueueStage(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					ProcessInputQueue();
					return Threading::CallbackResult::Finished;
				},
				Threading::JobPriority::UserInputPolling,
				"Process Input Queue"
			))
		, m_polledForInputStage(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					bool expected = true;
					[[maybe_unused]] const bool wasExchanged = m_canProcessInput.CompareExchangeStrong(expected, false);
					Assert(wasExchanged);
					return Threading::CallbackResult::Finished;
				},
				Threading::JobPriority::UserInputPolling,
				"Polled Inputs"
			))
		, m_mouseDeviceType(RegisterDeviceType<Input::MouseDeviceType>(*this))
		, m_keyboardDeviceType(RegisterDeviceType<Input::KeyboardDeviceType>(*this))
		, m_touchscreenDeviceType(RegisterDeviceType<Input::TouchscreenDeviceType>(*this))
		, m_gamepadDeviceType(RegisterDeviceType<GamepadDeviceType>(*this))
	{
		Engine& engine = System::Get<Engine>();
		engine.GetStartTickStage().AddSubsequentStage(m_pollForInputStage);
		m_pollForInputStage.AddSubsequentStage(m_processInputQueueStage);
		m_processInputQueueStage.AddSubsequentStage(m_polledForInputStage);
		m_pollForInputStage.AddSubsequentStage(m_polledForInputStage);
		m_polledForInputStage.AddSubsequentStage(engine.GetEndTickStage());

		Rendering::Window::Initialize(System::Get<Rendering::Renderer>());

#if PLATFORM_WINDOWS && !USE_SDL
		Rendering::Window::QueueOnWindowThread(
			[this]()
			{
				const HINSTANCE applicationInstance = GetModuleHandle(nullptr);
				static constexpr ZeroTerminatedUnicodeStringView dummyWindowClassName = MAKE_UNICODE_LITERAL("ngineDummyWindow");

				WNDCLASSW windowClassDesc = {};
				windowClassDesc.lpfnWndProc = ProcessDummyWindowMessage;
				windowClassDesc.lpszClassName = dummyWindowClassName;
				windowClassDesc.hInstance = applicationInstance;
				dummyWindowClassAtom = RegisterClassW(&windowClassDesc);
				Assert(dummyWindowClassAtom != 0);

				constexpr unsigned long windowStyle = WS_OVERLAPPEDWINDOW;
				constexpr unsigned long windowStyleEx = 0;

				dummyWindowHandle = CreateWindowExW(
					windowStyleEx,
					dummyWindowClassName,
					L"",
					windowStyle,
					0,
					0,
					0,
					0,
					nullptr,
					nullptr,
					applicationInstance,
					nullptr
				);
				SetWindowLongPtrA(dummyWindowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

				Array<RAWINPUTDEVICE, 5> inputDevices = {// Gamepad
			                                           RAWINPUTDEVICE{0x01, 0x05, RIDEV_INPUTSINK, dummyWindowHandle},
			                                           // Mouse
			                                           RAWINPUTDEVICE{0x01, 0x02, RIDEV_INPUTSINK, dummyWindowHandle},
			                                           // Keyboard
			                                           RAWINPUTDEVICE{0x01, 0x06, 0, nullptr},
			                                           // Touch
			                                           RAWINPUTDEVICE{0x0D, 0x51, RIDEV_INPUTSINK, dummyWindowHandle},
			                                           RAWINPUTDEVICE{0x0D, 0x04, RIDEV_INPUTSINK, dummyWindowHandle}
			  };

				[[maybe_unused]] const bool registeredDevices =
					RegisterRawInputDevices(inputDevices.GetData(), inputDevices.GetSize(), sizeof(RAWINPUTDEVICE));
				Assert(registeredDevices);
			}
		);
#endif
	}

	Manager::~Manager()
	{
#if PLATFORM_WINDOWS && !USE_SDL
		Rendering::Window::QueueOnWindowThread(
			[]()
			{
				DestroyWindow(dummyWindowHandle);

				const HINSTANCE applicationInstance = GetModuleHandle(nullptr);
				[[maybe_unused]] const bool wasRemoved = UnregisterClassW(MAKEINTATOM(dummyWindowClassAtom), applicationInstance);
				Assert(wasRemoved);
				dummyWindowClassAtom = 0;
			}
		);
#endif

		Rendering::Window::Destroy();
	}

	DeviceIdentifier Manager::RegisterDeviceInstance(const DeviceTypeIdentifier typeIdentifier, Optional<Rendering::Window*> pSourceWindow)
	{
		const DeviceIdentifier identifier = m_deviceIdentifiers.AcquireIdentifier();
		Assert(identifier.IsValid());
		if (LIKELY(identifier.IsValid()))
		{
			m_deviceInstances[identifier].CreateInPlace(
				identifier,
				typeIdentifier,
				pSourceWindow.IsValid() ? pSourceWindow->GetFocusedInputMonitor() : Optional<Monitor*>{}
			);
		}
		return identifier;
	}

	DeviceTypeIdentifier Manager::GetDeviceTypeIdentifier(Guid deviceTypeGuid) const
	{
		return m_deviceTypeIdentifierLookup.Find(deviceTypeGuid);
	}

	void Manager::OnMonitorDestroyed(Monitor& monitor)
	{
		for (UniquePtr<DeviceInstance>& pDeviceInstance : m_deviceIdentifiers.GetValidElementView(m_deviceInstances.GetView()))
		{
			if (pDeviceInstance.IsValid())
			{
				if (pDeviceInstance->GetActiveMonitor() == &monitor)
				{
					pDeviceInstance->SetActiveMonitor(nullptr, *m_deviceTypes[pDeviceInstance->GetTypeIdentifier()]);
				}
			}
		}
	}

#if PLATFORM_WINDOWS && !USE_SDL
	void Manager::HandleWindowsRawInput(const long long lParam)
	{
		HRAWINPUT rawInputHandle = reinterpret_cast<HRAWINPUT>(lParam);

		unsigned int rawInputDataBufferSize;
		GetRawInputData(rawInputHandle, RID_INPUT, NULL, &rawInputDataBufferSize, sizeof(RAWINPUTHEADER));
		Assert(rawInputDataBufferSize <= Math::NumericLimits<uint16>::Max);
		m_rawInputDataBuffer.Resize((uint16)rawInputDataBufferSize);
		GetRawInputData(rawInputHandle, RID_INPUT, m_rawInputDataBuffer.GetData(), &rawInputDataBufferSize, sizeof(RAWINPUTHEADER));

		const RAWINPUT& rawInput = *reinterpret_cast<const RAWINPUT*>(m_rawInputDataBuffer.GetData());

		switch (rawInput.header.dwType)
		{
			case RIM_TYPEKEYBOARD:
			{
				auto getFocusedWindow = []() -> Rendering::Window*
				{
					const HWND windowHandle = GetFocus();
					LONG_PTR userData = GetWindowLongPtrA(windowHandle, GWLP_USERDATA);
					if (userData == 0)
					{
						return nullptr;
					}
					static constexpr ConstNativeStringView engineWindowName = L"ngineWindow";
					wchar_t buff[engineWindowName.GetSize() + 1];
					const int length = GetClassNameW(windowHandle, buff, engineWindowName.GetSize() + 1);
					if (ConstNativeStringView(buff, length) != engineWindowName)
					{
						return nullptr;
					}

					DWORD processID;
					GetWindowThreadProcessId(windowHandle, &processID);
					if (processID != ::GetCurrentProcessId())
					{
						return nullptr;
					}

					return reinterpret_cast<Rendering::Window*>(userData);
				};

				bool isKeyDown = (rawInput.data.keyboard.Flags & RI_KEY_BREAK) == 0;

				Input::KeyboardDeviceType& keyboardDeviceType = GetDeviceType<Input::KeyboardDeviceType>(GetKeyboardDeviceTypeIdentifier());
				Rendering::Window* const pFocusedWindow = getFocusedWindow();
				const Input::DeviceIdentifier keyboardIdentifier =
					keyboardDeviceType.GetOrRegisterInstance(reinterpret_cast<uintptr>(rawInput.header.hDevice), *this, pFocusedWindow);
				if (!keyboardIdentifier.IsValid())
				{
					return;
				}

				unsigned int scancode = rawInput.data.keyboard.MakeCode;
				Assert(scancode <= 0xff);

				const bool isE0 = ((rawInput.data.keyboard.Flags & RI_KEY_E0) != 0);
				scancode |= 0xE000 * isE0;
				scancode |= 0xE100 * ((rawInput.data.keyboard.Flags & RI_KEY_E1) != 0);

				const bool shouldIgnore = scancode == 0xE11D || scancode == 0xE02A || scancode == 0xE0AA || scancode == 0xE0B6 ||
				                          scancode == 0xE036;

				unsigned int virtualKey = rawInput.data.keyboard.VKey;

				switch (virtualKey)
				{
						// the standard INSERT, DELETE, HOME, END, PRIOR and NEXT keys will always have their e0 bit set, but the
						// corresponding keys on the NUMPAD will not.
					case VK_INSERT:
						if (!isE0)
							virtualKey = (uint32)Input::KeyboardInput::Numpad0;
						break;

					case VK_DELETE:
						if (!isE0)
							virtualKey = (uint32)Input::KeyboardInput::Decimal;
						break;

					case VK_HOME:
						if (!isE0)
							virtualKey = (uint32)Input::KeyboardInput::Numpad7;
						break;

					case VK_END:
						if (!isE0)
							virtualKey = (uint32)Input::KeyboardInput::Numpad1;
						break;

					case VK_PRIOR:
						if (!isE0)
							virtualKey = (uint32)Input::KeyboardInput::Numpad9;
						break;

					case VK_NEXT:
						if (!isE0)
							virtualKey = (uint32)Input::KeyboardInput::Numpad3;
						break;

						// the standard arrow keys will always have their e0 bit set, but the
						// corresponding keys on the NUMPAD will not.
					case VK_LEFT:
						if (!isE0)
							virtualKey = (uint32)Input::KeyboardInput::Numpad4;
						break;

					case VK_RIGHT:
						if (!isE0)
							virtualKey = (uint32)Input::KeyboardInput::Numpad6;
						break;

					case VK_UP:
						if (!isE0)
							virtualKey = (uint32)Input::KeyboardInput::Numpad8;
						break;

					case VK_DOWN:
						if (!isE0)
							virtualKey = (uint32)Input::KeyboardInput::Numpad2;
						break;

						// NUMPAD 5 doesn't have its e0 bit set
					case VK_CLEAR:
						if (!isE0)
							virtualKey = (uint32)Input::KeyboardInput::Numpad5;
						break;

					case VK_SHIFT:
					case VK_CONTROL:
					case VK_MENU:
						virtualKey = LOWORD(MapVirtualKeyW(scancode, MAPVK_VSC_TO_VK_EX));
						break;
				}

				if (!shouldIgnore)
				{
					const KeyboardInput input = static_cast<KeyboardInput>(virtualKey);
					if (input == KeyboardInput::CapsLock)
					{
						isKeyDown = (GetAsyncKeyState(VK_CAPITAL) & (1 << 15)) != 0;
					}
					else if (input == KeyboardInput::LeftAlt)
					{
						isKeyDown = (GetAsyncKeyState(VK_LMENU) & (1 << 15)) != 0;
					}
					else if (input == KeyboardInput::RightAlt)
					{
						isKeyDown = (GetAsyncKeyState(VK_RMENU) & (1 << 15)) != 0;
					}

					if (isKeyDown)
					{
						keyboardDeviceType.OnKeyDown(keyboardIdentifier, input);
					}
					else
					{
						keyboardDeviceType.OnKeyUp(keyboardIdentifier, input);
					}

					if (isKeyDown)
					{
						Array<wchar_t, 16> utf16Buffer;

						unsigned char keyState[256] = {};
						if (GetAsyncKeyState(VK_SHIFT) | GetAsyncKeyState(VK_LSHIFT))
						{
							keyState[VK_SHIFT] = 0x80;
							keyState[VK_LSHIFT] = 0x80;
						}

						if (GetAsyncKeyState(VK_CONTROL))
						{
							keyState[VK_CONTROL] = 0x80;
						}

						if (GetAsyncKeyState(VK_MENU))
						{
							keyState[VK_MENU] = 0x80;
						}

						if ((GetKeyState(VK_CAPITAL) & 0x0001) != 0)
						{
							keyState[VK_CAPITAL] = 0x0001;
							keyState[VK_CAPITAL] |= 0x01;
						}

						if ((GetAsyncKeyState(VK_NUMLOCK) & 0x0001) != 0)
						{
							keyState[VK_NUMLOCK] = 0x0001;
						}

						keyState[VK_CONTROL] |= keyState[VK_RMENU];
						const int keyStringLength =
							ToUnicode(virtualKey, rawInput.data.keyboard.MakeCode, keyState, utf16Buffer.GetData(), utf16Buffer.GetSize(), 0);

						if (keyStringLength > 0)
						{
							keyboardDeviceType.OnText(
								keyboardIdentifier,
								ConstUnicodeStringView(reinterpret_cast<const UnicodeCharType*>(utf16Buffer.GetData()), keyStringLength)
							);
						}
					}
				}
			}
			break;
			case RIM_TYPEMOUSE:
			{
				auto getActiveWindow = []() -> Rendering::Window*
				{
					const HWND windowHandle = GetActiveWindow();
					LONG_PTR userData = GetWindowLongPtrA(windowHandle, GWLP_USERDATA);
					if (userData == 0)
					{
						return nullptr;
					}
					static constexpr ConstNativeStringView engineWindowName = L"ngineWindow";
					wchar_t buff[engineWindowName.GetSize() + 1];
					const int length = GetClassNameW(windowHandle, buff, engineWindowName.GetSize() + 1);
					if (ConstNativeStringView(buff, length) != engineWindowName)
					{
						return nullptr;
					}

					DWORD processID;
					GetWindowThreadProcessId(windowHandle, &processID);
					if (processID != ::GetCurrentProcessId())
					{
						return nullptr;
					}

					return reinterpret_cast<Rendering::Window*>(userData);
				};

				POINT cursorPosition;
				GetCursorPos(&cursorPosition);

				auto getWindowUnderCursor = [cursorPosition]() -> Rendering::Window*
				{
					const HWND windowHandle = WindowFromPoint(cursorPosition);
					LONG_PTR userData = GetWindowLongPtrA(windowHandle, GWLP_USERDATA);
					if (userData == 0)
					{
						return nullptr;
					}

					static constexpr ConstNativeStringView engineWindowName = L"ngineWindow";
					wchar_t buff[engineWindowName.GetSize() + 1];
					const int length = GetClassNameW(windowHandle, buff, engineWindowName.GetSize() + 1);
					if (ConstNativeStringView(buff, length) != engineWindowName)
					{
						return nullptr;
					}

					DWORD processID;
					GetWindowThreadProcessId(windowHandle, &processID);
					if (processID != ::GetCurrentProcessId())
					{
						return nullptr;
					}

					return reinterpret_cast<Rendering::Window*>(userData);
				};

				Rendering::Window* pWindow = getActiveWindow();
				Rendering::Window* pWindowUnderCursor = getWindowUnderCursor();
				if (pWindow != nullptr && pWindow->IsCursorLocked() && pWindowUnderCursor == pWindow)
				{
					cursorPosition.x = pWindow->GetLockedCursorPosition().x;
					cursorPosition.y = pWindow->GetLockedCursorPosition().y;
					SetCursorPos(cursorPosition.x, cursorPosition.y);
				}

				const Optional<Input::MouseDeviceType*> pMouseDeviceType = FindDeviceType<Input::MouseDeviceType>(GetMouseDeviceTypeIdentifier());
				if (UNLIKELY(!pMouseDeviceType.IsValid()))
				{
					return;
				}
				const Input::DeviceIdentifier mouseIdentifier =
					pMouseDeviceType->GetOrRegisterInstance(reinterpret_cast<uintptr>(rawInput.header.hDevice), *this, pWindowUnderCursor);
				if (UNLIKELY(!mouseIdentifier.IsValid()))
				{
					return;
				}

				//[[maybe_unused]] const bool hasAbsoluteCoordinates = (rawInput.data.mouse.usFlags & MOUSE_MOVE_ABSOLUTE) != 0;

				bool isInClientArea = false;
				ScreenCoordinate screenCoordinate;
				const Math::Vector2i deltaCoordinates = {rawInput.data.mouse.lLastX, rawInput.data.mouse.lLastY};

				if (pWindowUnderCursor != nullptr)
				{
					screenCoordinate = ScreenCoordinate{cursorPosition.x, cursorPosition.y};
					WindowCoordinate windowCoordinate = pWindowUnderCursor->ConvertScreenToLocalCoordinates(screenCoordinate);

					isInClientArea = pWindowUnderCursor->GetLocalClientArea().Contains(windowCoordinate);

					if (isInClientArea)
					{
						auto checkSendPressEvent = [&mouseDeviceType = *pMouseDeviceType,
						                            mouseIdentifier,
						                            screenCoordinate,
						                            pWindowUnderCursor,
						                            buttonMask = rawInput.data.mouse.ulButtons](const ULONG mask, const MouseButton button)
						{
							if ((buttonMask & mask) != 0)
							{
								mouseDeviceType.OnPress(mouseIdentifier, screenCoordinate, button, *pWindowUnderCursor);
							}
						};

						checkSendPressEvent(RI_MOUSE_LEFT_BUTTON_DOWN, MouseButton::Left);
						checkSendPressEvent(RI_MOUSE_MIDDLE_BUTTON_DOWN, MouseButton::Middle);
						checkSendPressEvent(RI_MOUSE_RIGHT_BUTTON_DOWN, MouseButton::Right);
						checkSendPressEvent(RI_MOUSE_BUTTON_4_DOWN, MouseButton::Extra1);
						checkSendPressEvent(RI_MOUSE_BUTTON_5_DOWN, MouseButton::Extra2);
					}

					if ((deltaCoordinates.x != 0) | (deltaCoordinates.y != 0))
					{
						pMouseDeviceType->OnMotion(mouseIdentifier, screenCoordinate, deltaCoordinates, *pWindowUnderCursor);
					}

					Math::Vector2i wheelDelta = {
						((((short)LOWORD(rawInput.data.mouse.usButtonData)) * ((rawInput.data.mouse.ulButtons & RI_MOUSE_HWHEEL) != 0)) / WHEEL_DELTA),
						((((short)LOWORD(rawInput.data.mouse.usButtonData)) * ((rawInput.data.mouse.ulButtons & RI_MOUSE_WHEEL) != 0)) / WHEEL_DELTA)
					};

					if ((wheelDelta.x != 0) | (wheelDelta.y != 0))
					{
						unsigned int pulScrollLines = 0;
						SystemParametersInfo(SPI_GETWHEELSCROLLLINES, 0, &pulScrollLines, 0);
						if (pulScrollLines == WHEEL_PAGESCROLL) // for now interpret as single scroll
						{
							pulScrollLines = 1;
						}
						wheelDelta.x *= int(pulScrollLines);
						wheelDelta.y *= int(pulScrollLines);

						pMouseDeviceType->OnScroll(mouseIdentifier, screenCoordinate, wheelDelta, *pWindowUnderCursor);
					}
				}

				auto checkSendReleaseEvent = [&mouseDeviceType = *pMouseDeviceType,
				                              mouseIdentifier,
				                              screenCoordinate,
				                              pWindow,
				                              buttonMask = rawInput.data.mouse.ulButtons](const ULONG mask, const MouseButton button)
				{
					if ((buttonMask & mask) != 0)
					{
						mouseDeviceType.OnRelease(mouseIdentifier, screenCoordinate, button, pWindow);
					}
				};

				checkSendReleaseEvent(RI_MOUSE_LEFT_BUTTON_UP, MouseButton::Left);
				checkSendReleaseEvent(RI_MOUSE_MIDDLE_BUTTON_UP, MouseButton::Middle);
				checkSendReleaseEvent(RI_MOUSE_RIGHT_BUTTON_UP, MouseButton::Right);
				checkSendReleaseEvent(RI_MOUSE_BUTTON_4_UP, MouseButton::Extra1);
				checkSendReleaseEvent(RI_MOUSE_BUTTON_5_UP, MouseButton::Extra2);
			}
			break;
		}
	}
#endif

	void Manager::ProcessInputQueue()
	{
		// TODO: Specialize this per device type instead of super generic function pointers
		// i.e. one queue per device type, knowing exactly what function we are calling at compile time
		Assert(m_activeInputQueueIndex == 0);
		for (uint8 queueIndex = 0; queueIndex < 2; ++queueIndex)
		{
			// Ensure that input is queued on the other queue while we process
			m_activeInputQueueIndex = !queueIndex;
			Threading::UniqueLock lock(m_inputQueueMutexes[queueIndex]);
			for (QueuedInputFunction& queuedInput : m_inputQueues[queueIndex])
			{
				queuedInput();
			}
			m_inputQueues[queueIndex].Clear();
		}
	}

	bool Manager::CanProcessInput() const
	{
		return m_canProcessInput.Load() && Threading::JobRunnerThread::GetCurrent().IsValid();
	}
}
