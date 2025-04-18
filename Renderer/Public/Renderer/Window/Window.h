#pragma once

#include <Common/Math/Vector2.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Primitives/RectangleEdges.h>
#include <Common/Memory/Containers/ZeroTerminatedStringView.h>
#include <Common/EnumFlags.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Function/Function.h>
#include <Common/Function/Event.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Storage/Identifier.h>
#include <Common/IO/ForwardDeclarations/ZeroTerminatedURIView.h>
#include <Common/IO/ForwardDeclarations/URIView.h>
#include <Common/Threading/Jobs/TimerHandle.h>
#include <Common/Threading/Jobs/JobBatch.h>

#include <Engine/Asset/Identifier.h>
#include <Engine/Input/ScreenCoordinate.h>
#include <Engine/Input/WindowCoordinate.h>
#include <Engine/Input/DeviceIdentifier.h>

#include <Renderer/Window/WindowHandle.h>
#include <Renderer/Window/CursorType.h>
#include <Renderer/Window/KeyboardTypeFlags.h>
#include <Renderer/Window/DragAndDropData.h>
#include <Renderer/Window/Surface.h>
#include <Renderer/Window/OpenDocumentFlags.h>

#if USE_SDL
struct SDL_Window;
#elif PLATFORM_ANDROID
namespace ngine::Platform::Android
{
	struct GameActivityWindow;
}
#endif

namespace ngine
{
	struct Engine;

	namespace Entity
	{
		struct SceneRegistry;
	}

	namespace Input
	{
		struct Monitor;
	}

	namespace IO
	{
		struct Path;
		struct URI;
	}

	namespace Asset
	{
		struct Guid;
	}

	namespace Widgets
	{
		struct DocumentData;
	}

	namespace Threading
	{
		struct JobBatch;
	}
}

namespace ngine::Rendering
{
	struct Renderer;
	struct LogicalDevice;
	struct PhysicalDevice;
	struct IDropTarget;
	struct IDropSource;
	struct WindowThread;
	struct ScreenProperties;
	struct FileType;

	struct Window
	{
		enum class CreationFlags : uint8
		{
			//! The size is in client coordinates
			//! Indicates that the part of the area we can render to is exactly this big
			SizeIsClientArea = 1 << 0,
			//! The size is in window coordinates
			//! Indicates that the window is exactly this big, and the client area can be smaller
			SizeIsWindowArea = 1 << 1,
			Visible = 1 << 2,
			Default = SizeIsClientArea | Visible,
		};

		enum class OrientationFlags : uint8
		{
			Portrait = 1 << 0,
			PortraitUpsideDown = 1 << 1,
			PortraitAny = Portrait | PortraitUpsideDown,
			LandscapeLeft = 1 << 2,
			LandscapeRight = 1 << 3,
			LandscapeAny = LandscapeLeft | LandscapeRight,
			Any = PortraitAny | LandscapeAny,
			All = Any,
			AnyButUpsideDown = Portrait | LandscapeLeft | LandscapeRight
		};

		enum class NativeType : uint8
		{
			Invalid,
			UIKit,
			AppKit,
			Win32,
			X11,
			Wayland,
			AndroidNative,
			Web
		};

		static void Initialize(Renderer& renderer);
		static void Destroy();

		struct Initializer
		{
			Optional<LogicalDevice*> m_pLogicalDevice;
			ConstZeroTerminatedUnicodeStringView m_name;
			Math::Rectanglei m_area;
			EnumFlags<CreationFlags> m_flags = CreationFlags::Default;

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
			void* const m_pUIWindowScene;
#elif PLATFORM_ANDROID
			Platform::Android::GameActivityWindow* const m_pGameActivityWindow;
#elif PLATFORM_WEB
			ConstZeroTerminatedStringView m_canvasSelector;
#endif

			Threading::JobBatch& m_jobBatch;
		};

		Window(Initializer&& initializer);
		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;
		Window(Window&& other) = default;
		Window& operator=(Window&&) = delete;
		virtual ~Window();

		void Initialize();

		struct CreationRequestParameters
		{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
			void* m_pWindowScene;
#elif PLATFORM_ANDROID
			Platform::Android::GameActivityWindow* m_pGameActivityWindow;
#elif PLATFORM_WEB
			ConstZeroTerminatedStringView m_canvasSelector;
#endif

			Threading::JobBatch& m_jobBatch;
		};

		using OnRequestWindowCreationCallback = Function<Optional<Window*>(CreationRequestParameters&&), 24>;
		static OnRequestWindowCreationCallback& GetRequestWindowCreationCallback()
		{
			static OnRequestWindowCreationCallback callback;
			return callback;
		};
		static void RequestWindowCreation(CreationRequestParameters&& parameters);

		using OnWindowCreatedEvent = Event<void(void*, Window&), 24>;
		static OnWindowCreatedEvent& GetOnWindowCreated()
		{
			static OnWindowCreatedEvent event;
			return event;
		}

		[[nodiscard]] bool TryStartResizing();
		void TryOrQueueResize(const Math::Vector2ui newClientAreaSize);
		void TryOrQueueResizeInternal(const Math::Vector2ui newClientAreaSize);
		virtual bool CanStartResizing() const;
		void StartResizingInternal(const Math::Vector2ui newClientAreaSize);
		void DestroySurface();
		void RecreateSurface();
		virtual void OnStartResizing()
		{
		}
		virtual void OnFinishedResizing()
		{
		}
		virtual void WaitForProcessingFramesToFinish()
		{
		}

		[[nodiscard]] Math::Vector2i GetPosition() const
		{
			return m_position;
		}
		[[nodiscard]] Math::Vector2i GetClientAreaPosition() const
		{
			return m_clientAreaPosition;
		}
		[[nodiscard]] Math::Vector2ui GetClientAreaSize() const
		{
			return m_clientAreaSize;
		}
		[[nodiscard]] Math::Rectanglei GetClientArea() const
		{
			return {m_clientAreaPosition, (Math::Vector2i)m_clientAreaSize};
		}
		[[nodiscard]] Math::Rectanglei GetLocalClientArea() const
		{
			return {Math::Zero, (Math::Vector2i)m_clientAreaSize};
		}

		[[nodiscard]] ScreenCoordinate ConvertLocalToScreenCoordinates(const WindowCoordinate coordinate) const
		{
			return ScreenCoordinate{(Math::Vector2i)coordinate + GetPosition()};
		}
		[[nodiscard]] WindowCoordinate ConvertScreenToLocalCoordinates(const ScreenCoordinate coordinate) const
		{
			return WindowCoordinate{coordinate - GetPosition()};
		}

		[[nodiscard]] ZeroTerminatedUnicodeStringView GetTitle() const
		{
			return m_title;
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS LogicalDevice& GetLogicalDevice()
		{
			return *m_pLogicalDevice;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const LogicalDevice& GetLogicalDevice() const
		{
			return *m_pLogicalDevice;
		}

		[[nodiscard]] SurfaceWindowHandle GetSurfaceHandle() const;
		[[nodiscard]] NativeType GetNativeType() const;
		[[nodiscard]] void* GetOSHandle() const;

		void GiveFocus();

		void MakeVisible();
		void Hide();
		[[nodiscard]] bool IsVisible() const;
		[[nodiscard]] bool IsHidden() const
		{
			return !IsVisible();
		}

		enum class StateFlags : uint8
		{
			IsInitialized = 1 << 0,
			SwitchingToBackground = 1 << 1,
			SwitchingToForeground = 1 << 2,
			InBackground = 1 << 3,
			//! Indicates that we were notified of an incoming window resize
			IsAwaitingResize = 1 << 4,
			//! Set while we're awaiting an incoming window resize, and still awaiting the current frame to finish
			IsAwaitingResizeNewFrame = 1 << 5,
			IsResizingQueued = 1 << 6,
			IsResizing = 1 << 7,
		};

		void OnSwitchToForeground();
		void OnSwitchToBackground();
		void OnReceivedKeyboardFocus();
		void OnLostKeyboardFocus();
		[[nodiscard]] bool IsInitialized() const
		{
			return m_stateFlags.IsSet(StateFlags::IsInitialized);
		}
		[[nodiscard]] bool IsSwitchingToBackground() const
		{
			return m_stateFlags.IsSet(StateFlags::SwitchingToBackground);
		}
		[[nodiscard]] bool IsInForeground() const
		{
			return !m_stateFlags.IsSet(StateFlags::InBackground);
		}
		[[nodiscard]] bool IsInBackground() const
		{
			return m_stateFlags.IsSet(StateFlags::InBackground);
		}

		[[nodiscard]] bool HasKeyboardFocus() const;
		[[nodiscard]] bool HasMouseFocus() const;

		void HideCursor();
		bool ShowCursor();
		void SetCursorVisibility(bool isVisible);
		[[nodiscard]] bool IsCursorVisible() const;
		void SetCursor(const CursorType cursor);

		void ConstrainCursorToWindow();
		bool UnconstrainCursorFromWindow();
		void SetCursorConstrainedToWindow(bool isConstrained);
		[[nodiscard]] inline bool IsCursorConstrained() const
		{
			return m_constrainedCursorCounter > 0;
		}

		void LockCursorPosition();
		bool UnlockCursorPosition();
		void SetCursorLockPosition(bool IsLocked);
		[[nodiscard]] inline bool IsCursorLocked() const
		{
			return m_lockCursorPositionCounter > 0;
		}
		[[nodiscard]] inline Math::Vector2i GetLockedCursorPosition() const
		{
			return m_lockedCursorPosition;
		}

		void Minimize();
		void Maximize();
		void RestoreSize();
		void OnDisplayRotationChanged();
		virtual void Close();

		void SetTitle(const ConstNativeZeroTerminatedStringView title);
		void SetUri(const IO::ConstZeroTerminatedURIView uri);
		//! Gets the URI assigned to this window
		//! In a web contet it is the current uri visible in the window bar
		[[nodiscard]] IO::URI GetUri() const;
		//! Creates a URI to be used for creating shareable links
		[[nodiscard]] virtual IO::URI CreateShareableUri(IO::ConstURIView parameters) const;

		void StartDragAndDrop(ArrayView<const Widgets::DragAndDropData> draggedItems);

		[[nodiscard]] virtual bool
		OnStartDragItemsIntoWindow(const WindowCoordinate, [[maybe_unused]] const ArrayView<const Widgets::DragAndDropData> draggedItems);
		virtual void OnCancelDragItemsIntoWindow();
		[[nodiscard]] virtual bool
		OnDragItemsOverWindow(const WindowCoordinate, [[maybe_unused]] const ArrayView<const Widgets::DragAndDropData> draggedItems);
		[[nodiscard]] virtual bool
		OnDropItemsIntoWindow(const WindowCoordinate, [[maybe_unused]] const ArrayView<const Widgets::DragAndDropData> draggedItems);

		using DocumentData = Widgets::DocumentData;
		using OpenDocumentFlags = Widgets::OpenDocumentFlags;

		[[nodiscard]] virtual Threading::JobBatch
		OpenDocuments(const ArrayView<const DocumentData> documents, const EnumFlags<OpenDocumentFlags>);

		enum class CreateDocumentFlags : uint8
		{
			//! Whether to automatically open the created document
			Open = 1 << 0
		};
		virtual Asset::Identifier CreateDocument(
			const Guid assetTypeGuid,
			UnicodeString&& documentName,
			IO::Path&& documentPath,
			const EnumFlags<CreateDocumentFlags>,
			const Optional<const DocumentData*> pTemplateDocument
		);

		//! Opens an OS native dialog requesting to share the specified assets to social media or local devices
		//! triggeringButttonContentArea is used on platforms or devices where the frame of the used share button is required for the OS native
		//! share UI
		void ShareDocuments(const ArrayView<const DocumentData> documents, const Math::Rectanglei triggeringButttonContentArea);

		using SelectFileCallback = Function<void(const ArrayView<const IO::Path> files), 24>;
		using FileType = Rendering::FileType;
		enum class SelectFileFlag : uint8
		{
			Files = 1 << 0,
			Directories = 1 << 1
		};
		void SelectFiles(const EnumFlags<SelectFileFlag> flags, const ArrayView<const FileType> allowedTypes, SelectFileCallback&& callback);
		// Exports a file to a user-defined location
		//! Opens a dialog asking where the user wants to export
		void ExportFile(const IO::Path& filePath, const FileType& fileType);

		[[nodiscard]] PURE_STATICS float GetDotsPerInch() const
		{
			return m_dotsPerInch;
		}
		[[nodiscard]] PURE_STATICS float GetDotsPerInchInternal() const;
		//! Gets the pixel ratio / scale needed to convert from CSS coordinates to device pixels
		//! A value of 1 indicates a classic 96 DPI display.
		[[nodiscard]] PURE_STATICS float GetDevicePixelRatio() const
		{
			return m_devicePixelRatio;
		}
		//! Gets the physical pixel ratio / scale needed to convert from CSS coordinates to device pixels
		//! A value of 1 indicates a classic 96 DPI display.
		[[nodiscard]] PURE_STATICS float GetPhysicalDevicePixelRatio() const
		{
			return m_physicalDevicePixelRatio;
		}
		[[nodiscard]] PURE_STATICS float GetDevicePixelRatioInternal() const;
		[[nodiscard]] PURE_STATICS float GetPhysicalDevicePixelRatioInternal() const;
		[[nodiscard]] PURE_STATICS Math::RectangleEdgesf GetSafeAreaInsets() const
		{
			return m_safeAreaInsets;
		}
		[[nodiscard]] PURE_STATICS Math::RectangleEdgesf GetSafeAreaInsetsInternal() const;
		[[nodiscard]] PURE_STATICS uint16 GetMaximumScreenRefreshRate() const
		{
			return m_maximumDisplayRefreshRate;
		}
		[[nodiscard]] PURE_STATICS uint16 GetMaximumScreenRefreshRateInternal() const;
		//! Gets the properties assigned to the screen this window is currently on
		[[nodiscard]] PURE_STATICS ScreenProperties GetCurrentScreenProperties() const;

#if !PLATFORM_APPLE_VISIONOS
		[[nodiscard]] static Math::Vector2ui GetMainScreenUsableBounds();
#endif
		virtual Optional<Input::Monitor*>
		GetMouseMonitorAtCoordinates(const WindowCoordinate, [[maybe_unused]] const Optional<uint16> touchRadius)
		{
			return nullptr;
		}
		virtual Optional<Input::Monitor*> GetFocusedInputMonitor()
		{
			return nullptr;
		}
		virtual void SetInputFocusAtCoordinate(WindowCoordinate, [[maybe_unused]] const Optional<uint16> touchRadius)
		{
		}

		[[nodiscard]] virtual bool AllowScreenAutoRotation() const
		{
			return true;
		}

		void SetDisallowedScreenOrientations(const EnumFlags<OrientationFlags> disallowedOrientations);
		void ClearDisallowedScreenOrientations(const EnumFlags<OrientationFlags> disallowedOrientations);
		[[nodiscard]] EnumFlags<OrientationFlags> GetDisallowedScreenOrientations() const
		{
			return m_disallowedDeviceOrientations;
		}

		void ShowVirtualKeyboard(const EnumFlags<KeyboardTypeFlags> keyboardTypeFlags);
		void HideVirtualKeyboard();
		virtual bool HasTextInInputFocus() const
		{
			return false;
		}
		virtual void InsertTextIntoInputFocus([[maybe_unused]] const ConstUnicodeStringView text)
		{
		}
		virtual void DeleteTextInInputFocusBackwards()
		{
		}

#if PLATFORM_WINDOWS
		[[nodiscard]] long long ProcessWindowMessage(const uint32 message, const unsigned long long wParam, const long long lParam);
		void ProcessPointerFrames(const unsigned long long wParam);
#elif PLATFORM_ANDROID
		[[nodiscard]] Platform::Android::GameActivityWindow* GetAndroidGameActivityWindow() const
		{
			return m_pGameActivityWindow;
		}
#endif

#if USE_SDL
		static void ProcessSDLMessages();
#elif PLATFORM_WINDOWS
		static void ProcessWindowsMessages();

		[[nodiscard]] uint16 GetCurrentPointerCount() const
		{
			return m_pointerCount;
		}
#elif PLATFORM_ANDROID
		static void ProcessWindowMessages();
#endif

		[[nodiscard]] static bool IsExecutingOnWindowThread();
		static void ExecuteImmediatelyOnWindowThread(Function<void(), 64>&& function);
		static void QueueOnWindowThread(Function<void(), 64>&& function);
		static void QueueOnWindowThreadOrExecuteImmediately(Function<void(), 64>&& function);

		void OnMoved(const Math::Vector2i newLocation);
	protected:
		virtual void OnReceivedMouseFocus()
		{
		}
		virtual void OnLostMouseFocus()
		{
		}
		virtual void OnReceivedKeyboardFocusInternal()
		{
		}
		virtual void OnLostKeyboardFocusInternal()
		{
		}
		virtual void OnBecomeVisible()
		{
		}
		virtual void OnBecomeHidden()
		{
		}
		virtual void OnSwitchToForegroundInternal();
		virtual void OnSwitchToBackgroundInternal();

		virtual void OnDisplayPropertiesChanged()
		{
		}

		virtual bool IsHitTestDisabled() const
		{
			return false;
		}

		[[nodiscard]] virtual bool IsCaption(const Math::Vector2i location);

		friend struct Renderer;
		friend struct LogicalDevice;

		[[nodiscard]] virtual bool DidWindowSizeChange() const
		{
			return false;
		}

		[[nodiscard]] SurfaceView GetSurface() const
		{
			return m_surface;
		}
	protected:
		AtomicEnumFlags<StateFlags> m_stateFlags;

		EnumFlags<OrientationFlags> m_disallowedDeviceOrientations;
		ZeroTerminatedUnicodeStringView m_title;

		Math::Vector2i m_position;
		Math::Vector2i m_clientAreaPosition;
		Math::Vector2ui m_clientAreaSize;

#if USE_SDL
		SDL_Window* m_pSDLWindow = nullptr;
#elif PLATFORM_WINDOWS
		void* m_pWindowHandle = nullptr;
		UniquePtr<IDropTarget> m_pDropTarget;
		UniquePtr<IDropSource> m_pDropSource;
		void* m_pInteractionContext = nullptr;
		uint16 m_pointerCount = 0;
		Array<Input::DeviceIdentifier, 4> m_gamepads;
		Threading::TimerHandle m_scheduledTimerHandle;
#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		void* m_pUIWindow = nullptr;
#elif PLATFORM_APPLE_MACOS
		void* m_pNSWindow = nullptr;
#elif PLATFORM_ANDROID
		Platform::Android::GameActivityWindow* m_pGameActivityWindow;
#elif PLATFORM_WEB
		String m_canvasSelector;
#endif

		float m_dotsPerInch{96.f};
		float m_devicePixelRatio{1.f};
		float m_physicalDevicePixelRatio{1.f};
		uint16 m_maximumDisplayRefreshRate{Math::NumericLimits<uint16>::Max};
		Math::RectangleEdgesf m_safeAreaInsets = {0.f};

#if PLATFORM_APPLE
		void* CreateMetalLayer(const EnumFlags<CreationFlags> flags);
		void* m_pMetalLayer;
#endif

#if RENDERER_WEBGPU
		Optional<Rendering::LogicalDevice*> m_pLogicalDevice;
		Surface m_surface;
#else
		Surface m_surface;
		Optional<Rendering::LogicalDevice*> m_pLogicalDevice;
#endif

		uint16 m_hideCursorCounter = 0;
		uint16 m_constrainedCursorCounter = 0;
		uint16 m_lockCursorPositionCounter = 0;
		Math::Vector2i m_lockedCursorPosition;
		bool m_hasKeyboardFocus = true;

		Math::Vector2ui m_queuedNewSize;
	};

	ENUM_FLAG_OPERATORS(Window::OrientationFlags);
	ENUM_FLAG_OPERATORS(Window::StateFlags);
}
