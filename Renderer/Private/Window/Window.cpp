#include "Window/Window.h"
#include "Window/ScreenProperties.h"

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Window/DocumentData.h>
#include <Renderer/Window/FileType.h>
#include "Renderer.h"

#include "Devices/PhysicalDevice.h"

#include <Common/System/Query.h>

#include <Engine/Engine.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/Devices/Mouse/Mouse.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>
#include <Engine/Input/Devices/Touchscreen/Touchscreen.h>
#include <Engine/Input/Monitor.h>
#include <Engine/Entity/ComponentSoftReference.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Asset/AssetManager.h>

#if PLATFORM_WINDOWS
#include <Common/Platform/Windows.h>
#include <InteractionContext.h>
#include <Engine/Input/Devices/Gamepad/GamepadMapping.h>
#include <shobjidl_core.h>
#include <ShellScalingApi.h>
#include <manipulations.h>
#include <manipulations_i.c>
#include <oleidl.h>
#include <dwmapi.h>
#include <Xinput.h>
#include <shlwapi.h>
PUSH_MSVC_WARNINGS
DISABLE_MSVC_WARNINGS(5256 4265 4986)
// warning C5256: 'ABI::Windows::Storage::CreationCollisionOption': a non-defining declaration
//  of an enumeration with a fixed underlying type is only permitted as a standalone declaration
// warning C4265 : 'Microsoft::WRL::Wrappers::HandleT<Microsoft::WRL::Wrappers::HandleTraits::EventTraits>': class has virtual functions,
//  but its non - trivial destructor is not virtual; instances of this class may not be destructed correctly
#include <WRL/client.h>
#include <WRL/event.h>
#include <WRL/wrappers/corewrappers.h>
#include <windows.applicationmodel.datatransfer.h>
#pragma comment(lib, "runtimeobject.lib")
POP_MSVC_WARNINGS

#pragma comment(lib, "Dwmapi.lib")
#pragma comment(lib, "shcore.lib")
#pragma comment(lib, "Ninput.lib")
#pragma comment(lib, "XInput.lib")

static constexpr bool s_useVirtualGamepad = false;
#endif

#if USE_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#undef None
#undef Status
#undef False
#undef DestroyAll
#undef ControlMask
#undef Bool
#undef Success
#endif

#if USE_APPLE_GAME_CONTROLLER
#import <GameController/GameController.h>
#endif

#if RENDERER_VULKAN

#if PLATFORM_WINDOWS
#include <3rdparty/vulkan/vulkan_win32.h>
#endif

#endif

#if PLATFORM_APPLE
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

#if PLATFORM_APPLE_MACOS
#import <AppKit/NSWindow.h>
#import <AppKit/NSScreen.h>
#import <UniformTypeIdentifiers/UTType.h>
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <Foundation/NSKeyedArchiver.h>
#include <Renderer/Window/iOS/MetalView.h>
#include <Renderer/Window/iOS/ViewController.h>
#include <Renderer/Window/iOS/Window.h>
#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UTType.h>
#import <UniformTypeIdentifiers/UTCoreTypes.h>
#import <Foundation/NSKeyedArchiver.h>
#include <Renderer/Window/iOS/MetalView.h>
#include <Renderer/Window/iOS/ViewController.h>
#include <Renderer/Window/iOS/Window.h>

#include <sys/types.h>
#include <sys/sysctl.h>
#endif
#elif PLATFORM_ANDROID
#include <../../Launchers/Common/Android/GameActivityWindow.h>
#elif PLATFORM_EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/html5.h>
#include <emscripten/html5_webgpu.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#include <emscripten/key_codes.h>
#include <Engine/Event/EventManager.h>
#endif

#include <Common/Threading/Thread.h>
#include <Common/Threading/Jobs/RecurringAsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/IO/URI.h>
#include <Common/Network/Format/Address.h>

#include <Common/Assert/Assert.h>
#include <Common/Math/CoreNumericTypes.h>
#include <Common/Math/Ratio.h>
#include <Common/Math/RotationalSpeed.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Memory/AddressOf.h>
#include <Common/Memory/SharedPtr.h>
#include <Common/Platform/GetDeviceModel.h>
#include <Common/IO/File.h>

#if PLATFORM_WINDOWS
static_assert(::CreateWindowEx == ::CreateWindowExW);
#endif

namespace ngine::Rendering
{
#if PLATFORM_WINDOWS
	struct IDropTarget : public ::IDropTarget
	{
		IDropTarget(Rendering::Window& window)
			: m_window(window)
		{
			Window::ExecuteImmediatelyOnWindowThread(
				[&]()
				{
					RegisterDragDrop(static_cast<HWND>(window.GetSurfaceHandle()), this);
				}
			);
		}
		virtual ~IDropTarget() = default;

		virtual HRESULT QueryInterface(REFIID riid, void** ppv) override
		{
			if (IsEqualIID(riid, IID_IUnknown))
			{
				*ppv = static_cast<IUnknown*>(this);
				return S_OK;
			}
			else if (IsEqualIID(riid, IID_IDropTarget))
			{
				*ppv = static_cast<::IDropTarget*>(this);
				return S_OK;
			}
			else
			{
				*ppv = nullptr;
				return E_NOINTERFACE;
			}
		}
		virtual ULONG AddRef() override
		{
			return ++m_referenceCount;
		}
		virtual ULONG Release() override
		{
			return --m_referenceCount;
		}
		virtual HRESULT
		DragEnter(IDataObject* pDataObject, [[maybe_unused]] DWORD grfKeyState, const _POINTL point, [[maybe_unused]] DWORD* pdwEffect) override
		{
			STGMEDIUM stg;
			FORMATETC fe;
			fe.cfFormat = CF_HDROP;
			fe.dwAspect = DVASPECT_CONTENT;
			fe.ptd = nullptr;
			fe.tymed = TYMED_HGLOBAL;
			fe.lindex = -1;

			if (SUCCEEDED(pDataObject->QueryGetData(&fe)))
			{
				if (SUCCEEDED(pDataObject->GetData(&fe, &stg)))
				{
					HDROP dropHandle = (HDROP)GlobalLock(stg.hGlobal);

					const uint32 fileCount = DragQueryFileW(dropHandle, 0xFFFFFFFF, nullptr, 0);
					m_data.Clear();
					m_data.Resize(fileCount);
					Assert(fileCount > 0);

					for (uint32 fileIndex = 0; fileIndex < fileCount; fileIndex++)
					{
						IO::Path filePath;
						DragQueryFileW(dropHandle, fileIndex, filePath.GetMutableView().GetData(), IO::Path::MaximumPathLength);
						m_data[fileIndex] = IO::Path(filePath.GetView().GetData(), (IO::Path::SizeType)lstrlenW(filePath.GetView().GetData()));
					}

					GlobalUnlock(stg.hGlobal);
					ReleaseStgMedium(&stg);

					const ScreenCoordinate screenCoordinate{point.x, point.x};
					const WindowCoordinate windowCoordinate = m_window.ConvertScreenToLocalCoordinates(screenCoordinate);

					ArrayView<const Widgets::DragAndDropData> items = m_data.GetView();
					if (m_window.OnStartDragItemsIntoWindow(windowCoordinate, items))
					{
						m_startedDragging = true;
						return S_OK;
					}

					return S_OK;
				}
			}

			fe.cfFormat = (CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW);
			if (SUCCEEDED(pDataObject->QueryGetData(&fe)))
			{
				if (SUCCEEDED(pDataObject->GetData(&fe, &stg)))
				{
					const FILEGROUPDESCRIPTORW* fileDescriptor = (const FILEGROUPDESCRIPTORW*)GlobalLock(stg.hGlobal);

					m_data.Clear();
					m_data.Resize(fileDescriptor->cItems);

					for (uint32 fileIndex = 0; fileIndex < fileDescriptor->cItems; fileIndex++)
					{
						m_data[fileIndex] =
							IO::Path(fileDescriptor->fgd[fileIndex].cFileName, (IO::Path::SizeType)lstrlenW(fileDescriptor->fgd[fileIndex].cFileName));
					}

					GlobalUnlock(stg.hGlobal);
					ReleaseStgMedium(&stg);

					const ScreenCoordinate screenCoordinate{point.x, point.x};
					const WindowCoordinate windowCoordinate = m_window.ConvertScreenToLocalCoordinates(screenCoordinate);

					ArrayView<const Widgets::DragAndDropData> items = m_data.GetView();
					if (m_window.OnStartDragItemsIntoWindow(windowCoordinate, items))
					{
						m_startedDragging = true;
						return S_OK;
					}

					return S_OK;
				}
			}

			return E_INVALIDARG;
		}
		virtual HRESULT DragOver([[maybe_unused]] DWORD grfKeyState, const _POINTL point, [[maybe_unused]] DWORD* pdwEffect) override
		{
			if (m_startedDragging)
			{
				const ScreenCoordinate screenCoordinate{point.x, point.x};
				const WindowCoordinate windowCoordinate = m_window.ConvertScreenToLocalCoordinates(screenCoordinate);

				ArrayView<const Widgets::DragAndDropData> items = m_data.GetView();
				if (m_window.OnDragItemsOverWindow(windowCoordinate, items))
				{
					return S_OK;
				}
			}

			return E_INVALIDARG;
		}
		virtual HRESULT DragLeave() override
		{
			if (m_startedDragging)
			{
				m_window.OnCancelDragItemsIntoWindow();
			}

			return S_OK;
		}
		virtual HRESULT
		Drop(IDataObject* pDataObject, [[maybe_unused]] DWORD grfKeyState, const _POINTL point, [[maybe_unused]] DWORD* pdwEffect) override
		{
			STGMEDIUM stg;

			FORMATETC fe;
			fe.cfFormat = CF_HDROP;
			fe.dwAspect = DVASPECT_CONTENT;
			fe.ptd = nullptr;
			fe.tymed = TYMED_HGLOBAL;
			fe.lindex = -1;

			if (SUCCEEDED(pDataObject->GetData(&fe, &stg)))
			{
				ReleaseStgMedium(&stg);

				const ScreenCoordinate screenCoordinate{point.x, point.x};
				const WindowCoordinate windowCoordinate = m_window.ConvertScreenToLocalCoordinates(screenCoordinate);

				ArrayView<const Widgets::DragAndDropData> items = m_data.GetView();
				if (m_window.OnDropItemsIntoWindow(windowCoordinate, items))
				{
					return S_OK;
				}
				return E_INVALIDARG;
			}

			fe.cfFormat = (CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW);
			if (SUCCEEDED(pDataObject->GetData(&fe, &stg)))
			{
				ReleaseStgMedium(&stg);

				const ScreenCoordinate screenCoordinate{point.x, point.x};
				const WindowCoordinate windowCoordinate = m_window.ConvertScreenToLocalCoordinates(screenCoordinate);

				ArrayView<const Widgets::DragAndDropData> items = m_data.GetView();
				if (m_window.OnDropItemsIntoWindow(windowCoordinate, items))
				{
					return S_OK;
				}
				return E_INVALIDARG;
			}

			return E_INVALIDARG;
		}
	protected:
		Threading::Atomic<uint32> m_referenceCount = 1;
		Rendering::Window& m_window;
		bool m_startedDragging = false;
		Vector<Widgets::DragAndDropData> m_data;
	};

	struct DataObject : public IDataObject
	{
		DataObject()
		{
		}

		DataObject(const ArrayView<const Widgets::DragAndDropData> draggedItems)
		{
			Create(draggedItems);
		}

		virtual ~DataObject()
		{
		}

		void Create(const ArrayView<const Widgets::DragAndDropData> draggedItems)
		{
			m_data.Clear();
			m_data.CopyFrom(m_data.begin(), draggedItems);

			/*{
			  FORMATETC descriptorFormat;
			  descriptorFormat.cfFormat = (CLIPFORMAT)CF_HDROP;
			  descriptorFormat.tymed = TYMED_HGLOBAL;
			  descriptorFormat.lindex = -1;
			  descriptorFormat.dwAspect = DVASPECT_CONTENT;
			  descriptorFormat.ptd = nullptr;
			  m_formats.EmplaceBack(Move(descriptorFormat));
			}*/

			const bool hasFiles = m_data.GetView().Any(
				[](const Widgets::DragAndDropData& data) -> bool
				{
					return data.Is<IO::Path>();
				}
			);

			m_formats.Reserve(hasFiles + m_data.GetSize());

			if (hasFiles)
			{
				FORMATETC descriptorFormat;
				descriptorFormat.cfFormat = (CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW);
				descriptorFormat.tymed = TYMED_HGLOBAL;
				descriptorFormat.lindex = -1;
				descriptorFormat.dwAspect = DVASPECT_CONTENT;
				descriptorFormat.ptd = nullptr;
				m_formats.EmplaceBack(Move(descriptorFormat));
			}

			for (const Widgets::DragAndDropData& data : m_data)
			{
				FORMATETC contentsFormat;
				if (const Optional<const IO::Path*> path = data.Get<IO::Path>())
				{
					contentsFormat.cfFormat = (CLIPFORMAT)RegisterClipboardFormat(CFSTR_FILECONTENTS);
					contentsFormat.tymed = TYMED_HGLOBAL;
					contentsFormat.dwAspect = DVASPECT_CONTENT;
				}
				else if (const Optional<const String*> string = data.Get<String>())
				{
					contentsFormat.cfFormat = CF_TEXT;
					contentsFormat.tymed = TYMED_HGLOBAL;
					contentsFormat.dwAspect = DVASPECT_CONTENT;
				}
				else
				{
					Assert(false);
					continue;
				}

				contentsFormat.lindex = m_data.GetIteratorIndex(Memory::GetAddressOf(data));
				contentsFormat.ptd = nullptr;
				m_formats.EmplaceBack(Move(contentsFormat));
			}
		}

		virtual HRESULT QueryInterface(REFIID riid, void** ppv) override
		{
			if (IsEqualIID(riid, IID_IUnknown))
			{
				*ppv = static_cast<IUnknown*>(this);
				return S_OK;
			}
			else if (IsEqualIID(riid, IID_IDataObject))
			{
				*ppv = static_cast<::IDataObject*>(this);
				return S_OK;
			}
			else
			{
				*ppv = nullptr;
				return E_NOINTERFACE;
			}
		}
		virtual unsigned long AddRef() override
		{
			return ++m_referenceCount;
		}
		virtual unsigned long Release() override
		{
			return --m_referenceCount;
		}

		virtual HRESULT GetData(FORMATETC* pfe, STGMEDIUM* pmed) override
		{
			ZeroMemory(pmed, sizeof(*pmed));

			if (pfe->cfFormat == RegisterClipboardFormat(CFSTR_FILEDESCRIPTORW))
			{
				if (m_formats[0].cfFormat != pfe->cfFormat)
				{
					return DV_E_FORMATETC;
				}

				const uint32 pathCount = m_data.GetView().Count(
					[](const Widgets::DragAndDropData& data) -> uint32
					{
						return data.Is<IO::Path>();
					}
				);

				FixedSizeVector<char> data(Memory::ConstructWithSize, Memory::Zeroed, sizeof(uint32) + sizeof(FILEDESCRIPTORW) * pathCount);

				uint32& fileCount = *reinterpret_cast<uint32*>(data.GetData());
				fileCount = pathCount;

				FILEDESCRIPTORW* fileDescriptors = reinterpret_cast<FILEDESCRIPTORW*>(data.GetData() + sizeof(uint32));

				for (const Widgets::DragAndDropData& draggedData : m_data)
				{
					if (const Optional<const IO::Path*> path = draggedData.Get<IO::Path>())
					{
						const uint32 index = m_data.GetIteratorIndex(Memory::GetAddressOf(draggedData));
						WIN32_FILE_ATTRIBUTE_DATA wfad;
						if (!GetFileAttributesExW(path->GetZeroTerminated(), GetFileExInfoStandard, &wfad))
						{
							return E_FAIL;
						}

						fileDescriptors[index].dwFlags = FD_ATTRIBUTES | FD_CREATETIME | FD_ACCESSTIME | FD_WRITESTIME | FD_FILESIZE;
						fileDescriptors[index].dwFileAttributes = wfad.dwFileAttributes;
						fileDescriptors[index].ftCreationTime = wfad.ftCreationTime;
						fileDescriptors[index].ftLastAccessTime = wfad.ftLastAccessTime;
						fileDescriptors[index].ftLastWriteTime = wfad.ftLastWriteTime;
						fileDescriptors[index].nFileSizeHigh = wfad.nFileSizeHigh;
						fileDescriptors[index].nFileSizeLow = wfad.nFileSizeLow;

						Memory::CopyWithoutOverlap(&fileDescriptors[index].cFileName, path->GetZeroTerminated().GetData(), path->GetDataSize());
						fileDescriptors[index].cFileName[path->GetSize()] = L'\0';
					}
				}

				pmed->tymed = TYMED_HGLOBAL;
				return CreateHGlobalFromBlob(data.GetData(), data.GetSize(), GMEM_MOVEABLE, &pmed->hGlobal);
			}
			else if (pfe->cfFormat == CF_TEXT)
			{
				if ((uint32)pfe->lindex >= m_formats.GetSize() || m_formats[pfe->lindex + 1].cfFormat != pfe->cfFormat)
				{
					return DV_E_FORMATETC;
				}

				pmed->tymed = TYMED_HGLOBAL;
				String& value = *m_data[pfe->lindex].Get<String>();
				return CreateHGlobalFromBlob(value.GetData(), value.GetSize(), GMEM_MOVEABLE, &pmed->hGlobal);
			}
			else if (pfe->cfFormat == RegisterClipboardFormat(CFSTR_FILECONTENTS))
			{
				if ((uint32)pfe->lindex >= m_formats.GetSize() || m_formats[pfe->lindex + 1].cfFormat != pfe->cfFormat)
				{
					return DV_E_FORMATETC;
				}

				// file contents at index
				const IO::File file(
					*m_data[pfe->lindex].Get<IO::Path>(),
					IO::AccessModeFlags::Binary | IO::AccessModeFlags::Read,
					IO::SharingFlags::DisallowWrite
				);
				FixedSizeVector<char> fileContents(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)file.GetSize());
				if (file.ReadIntoView(fileContents.GetView()))
				{
					pmed->tymed = TYMED_HGLOBAL;
					return CreateHGlobalFromBlob(fileContents.GetData(), fileContents.GetDataSize(), GMEM_MOVEABLE, &pmed->hGlobal);
				}

				return DV_E_FORMATETC;
			}
			/*else if (pfe->cfFormat == CF_HDROP)
			{
			  struct STATICDROPFILES
			  {
			    DROPFILES df;
			    TCHAR szFile[ARRAYSIZE(TEXT("C:\\Something.txt\0"))];
			  } const c_hdrop = {
			    {
			    FIELD_OFFSET(STATICDROPFILES, szFile),
			    { 0, 0 },
			    FALSE,
			    sizeof(TCHAR) == sizeof(WCHAR)
			    },
			    TEXT("C:\\Something.txt\0"),
			  };

			  pmed->tymed = TYMED_HGLOBAL;
			  return CreateHGlobalFromBlob(&c_hdrop, sizeof(c_hdrop), GMEM_MOVEABLE, &pmed->hGlobal);
			}*/

			return DV_E_FORMATETC;
		}
		virtual HRESULT GetDataHere(FORMATETC*, STGMEDIUM*) override
		{
			return E_NOTIMPL;
		}
		virtual HRESULT QueryGetData(FORMATETC* pfe) override
		{
			return GetDataIndex(pfe).IsValid() ? S_OK : S_FALSE;
		}
		virtual HRESULT GetCanonicalFormatEtc(FORMATETC* pfeIn, FORMATETC* pfeOut) override
		{
			*pfeOut = *pfeIn;
			pfeOut->ptd = nullptr;
			return DATA_S_SAMEFORMATETC;
		}
		virtual HRESULT SetData(FORMATETC*, STGMEDIUM*, BOOL) override
		{
			return E_NOTIMPL;
		}
		virtual HRESULT EnumFormatEtc(DWORD dwDirection, LPENUMFORMATETC* ppefe) override
		{
			if (dwDirection == DATADIR_GET)
			{
				return SHCreateStdEnumFmtEtc(m_formats.GetSize(), m_formats.GetData(), ppefe);
			}
			*ppefe = nullptr;
			return E_NOTIMPL;
		}
		virtual HRESULT DAdvise(FORMATETC*, DWORD, IAdviseSink*, DWORD*) override
		{
			return OLE_E_ADVISENOTSUPPORTED;
		}
		virtual HRESULT DUnadvise(DWORD) override
		{
			return OLE_E_ADVISENOTSUPPORTED;
		}
		virtual HRESULT EnumDAdvise(LPENUMSTATDATA*) override
		{
			return OLE_E_ADVISENOTSUPPORTED;
		}
	private:
		Optional<uint32> GetDataIndex(const FORMATETC* pfe)
		{
			for (const FORMATETC& format : m_formats)
			{
				if ((pfe->cfFormat == format.cfFormat) & (pfe->tymed & format.tymed) & (pfe->dwAspect == format.dwAspect) & (pfe->lindex == format.lindex))
				{
					return m_formats.GetIteratorIndex(Memory::GetAddressOf(format));
				}
			}

			return Invalid;
		}

		HRESULT CreateHGlobalFromBlob(const void* pvData, SIZE_T cbData, UINT uFlags, HGLOBAL* phglob)
		{
			HGLOBAL hglob = GlobalAlloc(uFlags, cbData);
			if (hglob)
			{
				void* pvAlloc = GlobalLock(hglob);
				if (pvAlloc)
				{
					CopyMemory(pvAlloc, pvData, cbData);
					GlobalUnlock(hglob);
				}
				else
				{
					GlobalFree(hglob);
					hglob = nullptr;
				}
			}
			*phglob = hglob;
			return hglob ? S_OK : E_OUTOFMEMORY;
		}
	private:
		Threading::Atomic<uint32> m_referenceCount = 1;
		Vector<FORMATETC> m_formats;
		Vector<Widgets::DragAndDropData> m_data;
	};

	struct IDropSource : public ::IDropSource
	{
		IDropSource(Rendering::Window& window)
			: m_window(window)
		{
		}
		virtual ~IDropSource() = default;

		virtual HRESULT QueryInterface(REFIID riid, void** ppv) override
		{
			if (IsEqualIID(riid, IID_IUnknown))
			{
				*ppv = static_cast<IUnknown*>(this);
				return S_OK;
			}
			else if (IsEqualIID(riid, IID_IDropSource))
			{
				*ppv = static_cast<::IDropSource*>(this);
				return S_OK;
			}
			else
			{
				*ppv = nullptr;
				return E_NOINTERFACE;
			}
		}
		virtual ULONG AddRef() override
		{
			return ++m_referenceCount;
		}
		virtual ULONG Release() override
		{
			return --m_referenceCount;
		}

		virtual HRESULT QueryContinueDrag([[maybe_unused]] BOOL fEscapePressed, [[maybe_unused]] DWORD grfKeyState) override
		{
			if (fEscapePressed)
			{
				return DRAGDROP_S_CANCEL;
			}
			else if ((grfKeyState & (MK_LBUTTON | MK_RBUTTON)) == 0)
			{
				return DRAGDROP_S_DROP;
			}

			return S_OK;
		}

		virtual HRESULT GiveFeedback([[maybe_unused]] DWORD dwEffect) override
		{
			return DRAGDROP_S_USEDEFAULTCURSORS;
		}
	protected:
		Threading::Atomic<uint32> m_referenceCount = 1;
		Rendering::Window& m_window;
	};

	void __stdcall OnInteraction(void* pUserData, const INTERACTION_CONTEXT_OUTPUT* pOutput)
	{
		Window& window = *reinterpret_cast<Window*>(pUserData);

		Input::Manager& inputManager = System::Get<Input::Manager>();

		switch (pOutput->inputType)
		{
			case PT_TOUCH:
			{
				Input::TouchscreenDeviceType& touchscreenDeviceType =
					inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
				const Input::DeviceIdentifier touchscreenIdentifier = touchscreenDeviceType.GetOrRegisterInstance(0, inputManager, window);

				POINT point = {(LONG)pOutput->x, (LONG)pOutput->y};
				ScreenToClient(reinterpret_cast<HWND>(window.GetSurfaceHandle()), &point);

				const ScreenCoordinate coordinate{point.x, point.y};

				const Array fingerIdentifiers{Input::FingerIdentifier{1}};

				switch (pOutput->interactionId)
				{
					case INTERACTION_ID_TAP:
					{
						if (pOutput->interactionFlags & INTERACTION_FLAG_BEGIN)
						{
							touchscreenDeviceType
								.OnStartTap(touchscreenIdentifier, coordinate, Input::Monitor::DefaultTouchRadius, fingerIdentifiers, window);
						}

						if (pOutput->interactionFlags & INTERACTION_FLAG_CANCEL)
						{
							touchscreenDeviceType.OnCancelTap(touchscreenIdentifier, fingerIdentifiers, window);
						}

						if (pOutput->interactionFlags & INTERACTION_FLAG_END)
						{
							switch (pOutput->arguments.tap.count)
							{
								case 1:
									touchscreenDeviceType
										.OnStopTap(touchscreenIdentifier, coordinate, Input::Monitor::DefaultTouchRadius, fingerIdentifiers, window);
									break;
								case 2:
									touchscreenDeviceType
										.OnDoubleTap(touchscreenIdentifier, coordinate, fingerIdentifiers, Input::Monitor::DefaultTouchRadius, window);
									break;
							}
						}
					}
					break;
					case INTERACTION_ID_HOLD:
					{
						if (pOutput->interactionFlags & INTERACTION_FLAG_BEGIN)
						{
							touchscreenDeviceType
								.OnStartLongPress(touchscreenIdentifier, coordinate, fingerIdentifiers, Input::Monitor::DefaultTouchRadius, window);
						}

						if (pOutput->interactionFlags & INTERACTION_FLAG_CANCEL)
						{
							touchscreenDeviceType.OnCancelLongPress(touchscreenIdentifier, fingerIdentifiers, window);
						}

						if (pOutput->interactionFlags & INTERACTION_FLAG_END)
						{
							touchscreenDeviceType
								.OnStopLongPress(touchscreenIdentifier, coordinate, fingerIdentifiers, Input::Monitor::DefaultTouchRadius, window);
						}
					}
					break;
					case INTERACTION_ID_SECONDARY_TAP:
						// TODO: Expose
						break;
					case INTERACTION_ID_MANIPULATION:
					{
						const uint8 fingerCount = (uint8)window.GetCurrentPointerCount();

						static bool isPinching = false;
						static float previousScale = 1.f;
						const float scale = pOutput->arguments.manipulation.cumulative.scale;

						if (scale != previousScale && fingerCount == 2)
						{
							previousScale = scale;

							if (!isPinching)
							{
								isPinching = true;
								touchscreenDeviceType.OnStartPinch(touchscreenIdentifier, coordinate, scale, fingerIdentifiers, window);
							}
							else
							{
								touchscreenDeviceType.OnPinchMotion(touchscreenIdentifier, coordinate, scale, fingerIdentifiers, window);
							}
						}

						static bool isRotating = false;
						static float previousRotation = 0.f;
						const float rotation = pOutput->arguments.manipulation.cumulative.rotation;

						if (rotation != previousRotation && fingerCount == 2)
						{
							previousRotation = rotation;

							if (!isRotating)
							{
								isRotating = true;
								touchscreenDeviceType.OnStartRotate(touchscreenIdentifier, coordinate, fingerIdentifiers, window);
							}
							else
							{
								touchscreenDeviceType.OnRotateMotion(
									touchscreenIdentifier,
									coordinate,
									Math::Anglef::FromRadians(rotation),
									0_rads,
									fingerIdentifiers,
									window
								);
							}
						}

						static bool isPanning = false;

						const Math::Vector2f velocity = Math::Vector2f{
							pOutput->arguments.manipulation.velocity.velocityX,
							pOutput->arguments.manipulation.velocity.velocityY
						}; // *(Math::Vector2f)window.GetClientAreaSize();

						if (isPinching && fingerCount != 2)
						{
							isPinching = false;
							previousScale = 1.f;
							touchscreenDeviceType.OnCancelPinch(touchscreenIdentifier, fingerIdentifiers, window);
						}
						if (isRotating && fingerCount != 2)
						{
							isRotating = false;
							previousRotation = 0.f;
							touchscreenDeviceType.OnCancelRotate(touchscreenIdentifier, fingerIdentifiers, window);
						}

						if (pOutput->interactionFlags & INTERACTION_FLAG_CANCEL)
						{
							if (isPanning)
							{
								isPanning = false;
								touchscreenDeviceType.OnCancelPan(touchscreenIdentifier, fingerIdentifiers, window);
							}
							if (isPinching)
							{
								isPinching = false;
								previousScale = 1.f;
								touchscreenDeviceType.OnCancelPinch(touchscreenIdentifier, fingerIdentifiers, window);
							}
							if (isRotating)
							{
								isRotating = false;
								previousRotation = 0.f;
								touchscreenDeviceType.OnCancelRotate(touchscreenIdentifier, fingerIdentifiers, window);
							}
						}
						else if (pOutput->interactionFlags & INTERACTION_FLAG_END)
						{
							if (isPanning)
							{
								isPanning = false;
								touchscreenDeviceType.OnStopPan(touchscreenIdentifier, coordinate, velocity, fingerIdentifiers, window);
							}
							if (isPinching)
							{
								isPinching = false;
								previousScale = 1.f;
								touchscreenDeviceType.OnStopPinch(touchscreenIdentifier, coordinate, fingerIdentifiers, window);
							}
							if (isRotating)
							{
								isRotating = false;
								previousRotation = 0.f;
								touchscreenDeviceType
									.OnStopRotate(touchscreenIdentifier, coordinate, Math::Anglef::FromRadians(rotation), 0_rads, fingerIdentifiers, window);
							}
						}
						else if (pOutput->arguments.manipulation.delta.translationX != 0 || pOutput->arguments.manipulation.delta.translationY != 0)
						{
							if (!isPanning)
							{
								isPanning = true;
								touchscreenDeviceType
									.OnStartPan(touchscreenIdentifier, coordinate, fingerIdentifiers, velocity, Input::Monitor::DefaultTouchRadius, window);
							}
							else
							{
								touchscreenDeviceType
									.OnPanMotion(touchscreenIdentifier, coordinate, velocity, Input::Monitor::DefaultTouchRadius, fingerIdentifiers, window);
							}
						}
					}
					break;
					default:
						break;
				}
			}
		}
	}
#endif

#define USE_WINDOW_THREAD (USE_SDL || PLATFORM_WINDOWS) && !PLATFORM_APPLE
#define HAS_WINDOW_THREAD USE_WINDOW_THREAD || PLATFORM_ANDROID || PLATFORM_APPLE_MACOS

	struct WindowThreadUtilitiesBase
	{
		enum class State : uint8
		{
			AwaitingStart,
			Running,
			Stopped
		};

		virtual ~WindowThreadUtilitiesBase()
		{
			Assert(m_invokeQueues[0].m_queue.IsEmpty());
			Assert(m_invokeQueues[1].m_queue.IsEmpty());
		}

		template<typename Function>
		void Queue(Function&& callback)
		{
			Threading::UniqueLock lock(m_mutex);
			m_invokeQueues[m_currentQueue].m_queue.EmplaceBack(Forward<Function>(callback));
			// Ensure we wake the window thread
			SendSignalMessageInternal();
		}

		void OnEngineQuit()
		{
			Threading::UniqueLock lock(m_mutex);
			// Queue a dummy message to wake the thread up
			if (m_state == State::Running)
			{
				lock.Unlock();
				ExecuteImmediate(
					[]()
					{
					}
				);
				while (m_state == State::Running)
					;
			}
		}

		template<typename Function>
		void ExecuteImmediate(Function&& callback)
		{
			if (IsRunningOnThreadInternal())
			{
				callback();
				return;
			}

			switch (m_state.Load())
			{
				case State::AwaitingStart:
					callback();
					break;
				case State::Running:
				{
					Threading::UniqueLock lock(m_mutex);
					m_invokeQueues[m_currentQueue].m_queue.EmplaceBack(Forward<Function>(callback));

					// Ensure we wake the window thread
					SendSignalMessageInternal();

					m_invokeQueues[m_currentQueue].m_immediateInvokeConditionVariable.Wait(lock);
				}
				break;
				case State::Stopped:
					callback();
			}
		}

		void ProcessInvokeQueue()
		{
			{
				Threading::UniqueLock lock(m_mutex);
				m_currentQueue = !m_currentQueue;
			}
			const uint8 processingQueue = !m_currentQueue;

			for (auto& function : m_invokeQueues[processingQueue].m_queue)
			{
				function();
			}
			m_invokeQueues[processingQueue].m_queue.Clear();
			m_invokeQueues[processingQueue].m_immediateInvokeConditionVariable.NotifyAll();
		}
	protected:
		virtual void SendSignalMessageInternal() = 0;
		virtual bool IsRunningOnThreadInternal() const = 0;
	protected:
		Threading::Atomic<State> m_state{State::AwaitingStart};

		Threading::Mutex m_mutex;
		struct InvokeQueue
		{
			Vector<Function<void(), 64>> m_queue;
			Threading::ConditionVariable m_immediateInvokeConditionVariable;
		};
		uint8 m_currentQueue{0};
		InvokeQueue m_invokeQueues[2];
	};

#if USE_WINDOW_THREAD
#if USE_SDL
	static uint32 WakeWindowThreadEventId{0};
	static bool IsWindowThreadActivated{false};
#endif

	struct WindowThread final : public Threading::ThreadWithRunMember<WindowThread>, public WindowThreadUtilitiesBase
	{
		WindowThread()
		{
		}

		void Run()
		{
			{
				State expected = State::AwaitingStart;
				[[maybe_unused]] const bool wasExchanged = m_state.CompareExchangeStrong(expected, State::Running);
				Assert(wasExchanged);
			}

#if PLATFORM_WINDOWS
			OleInitialize(nullptr);
#endif

#if PLATFORM_WINDOWS
			Rendering::Window::ProcessWindowsMessages();
#elif USE_SDL
			Rendering::Window::ProcessSDLMessages();
#endif

#if PLATFORM_WINDOWS
			OleUninitialize();
#endif

			{
				Threading::UniqueLock lock(m_mutex);
				// Locked modify to ensure m_state is not set and checked until after we notify the thread
				// Otherwise there we'd have a race where the window thread could sleep in the window message processing loop
				{
					State expected = State::Running;
					[[maybe_unused]] const bool wasExchanged = m_state.CompareExchangeStrong(expected, State::Stopped);
					Assert(wasExchanged);
				}
			}

			m_invokeQueues[0].m_immediateInvokeConditionVariable.NotifyAll();
			m_invokeQueues[1].m_immediateInvokeConditionVariable.NotifyAll();
		}
	protected:
		virtual void SendSignalMessageInternal() override final
		{
#if PLATFORM_WINDOWS
			PostThreadMessage(GetThreadId().GetId(), WM_NULL, 0, 0);
#elif USE_SDL
			if (IsWindowThreadActivated)
			{
				Array<SDL_Event, 1> events;
				events[0].type = WakeWindowThreadEventId;
				SDL_PeepEvents(events.GetData(), (int)events.GetSize(), SDL_ADDEVENT, WakeWindowThreadEventId, WakeWindowThreadEventId);
			}
#endif
		}

		virtual bool IsRunningOnThreadInternal() const override final
		{
			return IsRunningOnThread();
		}
	};

	[[nodiscard]] UniquePtr<WindowThread>& GetWindowThread()
	{
		static UniquePtr<WindowThread> pWindowThread = []()
		{
			UniquePtr<WindowThread> pThread;
			pThread.CreateInPlace();
			pThread->Start(MAKE_NATIVE_LITERAL("Window Thread"));
			return pThread;
		}();
		return pWindowThread;
	}

#elif HAS_WINDOW_THREAD
	struct WindowThreadUtilities final : public WindowThreadUtilitiesBase
	{
		WindowThreadUtilities()
		{
			{
				State expected = State::AwaitingStart;
				[[maybe_unused]] const bool wasExchanged = m_state.CompareExchangeStrong(expected, State::Running);
				Assert(wasExchanged);
			}
		}

		~WindowThreadUtilities()
		{
			{
				State expected = State::Running;
				[[maybe_unused]] const bool wasExchanged = m_state.CompareExchangeStrong(expected, State::Stopped);
				Assert(wasExchanged);
			}
		}

		virtual void SendSignalMessageInternal() override final
		{
		}

		[[nodiscard]] bool IsRunningOnThread() const
		{
			return IsRunningOnThreadInternal();
		}
	protected:
		[[nodiscard]] virtual bool IsRunningOnThreadInternal() const override final
		{
			Threading::JobManager& jobManager = System::Get<Threading::JobManager>();
			return jobManager.GetJobThreads()[0]->IsExecutingOnThread();
		}
	};
	static WindowThreadUtilities s_windowThreadUtilities;
#endif

	bool Window::IsExecutingOnWindowThread()
	{
#if USE_WINDOW_THREAD
		return GetWindowThread()->IsRunningOnThread();
#elif PLATFORM_APPLE
		return [NSThread isMainThread];
#elif PLATFORM_ANDROID
		return s_windowThreadUtilities.IsRunningOnThread();
#elif PLATFORM_EMSCRIPTEN
		return emscripten_main_runtime_thread_id() == pthread_self();
#else
#error "Not implemented for platform"
#endif
	}

	void Window::ExecuteImmediatelyOnWindowThread(Function<void(), 64>&& function)
	{
#if USE_WINDOW_THREAD
		GetWindowThread()->ExecuteImmediate(Forward<decltype(function)>(function));
#elif PLATFORM_APPLE
		if ([NSThread isMainThread])
		{
			function();
		}
		else
		{
			SharedPtr<Function<void(), 64>> pFunction = SharedPtr<Function<void(), 64>>::Make(Forward<decltype(function)>(function));
			dispatch_sync(dispatch_get_main_queue(), ^{
				(*pFunction)();
			});
		}
#elif PLATFORM_ANDROID
		s_windowThreadUtilities.ExecuteImmediate(Forward<decltype(function)>(function));
#elif PLATFORM_EMSCRIPTEN
		using FunctionType = Function<void(), 64>;
		em_proxying_queue* queue = emscripten_proxy_get_system_queue();
		pthread_t target = emscripten_main_runtime_thread_id();
		if (target == pthread_self())
		{
			function();
		}
		else
		{
			[[maybe_unused]] const bool called = emscripten_proxy_sync(
																						 queue,
																						 target,
																						 [](void* pUserData)
																						 {
																							 FunctionType& function = *reinterpret_cast<FunctionType*>(pUserData);
																							 function();
																						 },
																						 &function
																					 ) == 1;
			Assert(called);
		}
#else
#error "Not implemented for platform"
#endif
	}

	void Window::QueueOnWindowThread(Function<void(), 64>&& function)
	{
#if USE_WINDOW_THREAD
		GetWindowThread()->Queue(Forward<decltype(function)>(function));
#elif PLATFORM_APPLE
		SharedPtr<Function<void(), 64>> pFunction = SharedPtr<Function<void(), 64>>::Make(Forward<decltype(function)>(function));
		dispatch_async(dispatch_get_main_queue(), ^{
			(*pFunction)();
		});
#elif PLATFORM_ANDROID
		s_windowThreadUtilities.Queue(Forward<Function<void(), 64>>(function));
#elif PLATFORM_EMSCRIPTEN
		using FunctionType = Function<void(), 64>;
		FunctionType* pFunction = new FunctionType(Forward<FunctionType>(function));

		em_proxying_queue* queue = emscripten_proxy_get_system_queue();
		pthread_t target = emscripten_main_runtime_thread_id();

		[[maybe_unused]] const bool queued = emscripten_proxy_async(
																					 queue,
																					 target,
																					 [](void* pUserData)
																					 {
																						 FunctionType& function = *reinterpret_cast<FunctionType*>(pUserData);
																						 function();
																						 delete &function;
																					 },
																					 pFunction
																				 ) == 1;
		Assert(queued);
#else
#error "Not implemented for platform"
#endif
	}

	void Window::QueueOnWindowThreadOrExecuteImmediately(Function<void(), 64>&& function)
	{
		if (IsExecutingOnWindowThread())
		{
			function();
		}
		else
		{
			QueueOnWindowThread(Forward<Function<void(), 64>>(function));
		}
	}

	/* static */ void Window::Initialize([[maybe_unused]] Renderer&)
	{
#if USE_WINDOW_THREAD
		// Make sure we start the window thread
		[[maybe_unused]] WindowThread& windowThread = *GetWindowThread();
#endif

#if USE_SDL
		ExecuteImmediatelyOnWindowThread(
			[]()
			{
				Assert(!IsWindowThreadActivated);
				IsWindowThreadActivated = true;
				SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER);
				SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);
				SDL_EventState(SDL_WINDOWEVENT, SDL_ENABLE);
				SDL_EventState(SDL_KEYDOWN, SDL_ENABLE);
				SDL_EventState(SDL_KEYUP, SDL_ENABLE);
				SDL_EventState(SDL_DROPBEGIN, SDL_ENABLE);
				SDL_EventState(SDL_DROPFILE, SDL_ENABLE);
				SDL_EventState(SDL_DROPCOMPLETE, SDL_ENABLE);
				SDL_EventState(SDL_QUIT, SDL_ENABLE);

				SDL_EventState(SDL_MOUSEMOTION, SDL_ENABLE);
				SDL_EventState(SDL_MOUSEBUTTONDOWN, SDL_ENABLE);
				SDL_EventState(SDL_MOUSEBUTTONUP, SDL_ENABLE);
				SDL_EventState(SDL_MOUSEWHEEL, SDL_ENABLE);

				SDL_EventState(SDL_FINGERDOWN, SDL_ENABLE);
				SDL_EventState(SDL_FINGERUP, SDL_ENABLE);
				SDL_EventState(SDL_FINGERMOTION, SDL_ENABLE);

				SDL_EventState(SDL_CONTROLLERAXISMOTION, SDL_ENABLE);
				SDL_EventState(SDL_CONTROLLERBUTTONDOWN, SDL_ENABLE);
				SDL_EventState(SDL_CONTROLLERBUTTONUP, SDL_ENABLE);
				SDL_EventState(SDL_CONTROLLERDEVICEADDED, SDL_ENABLE);
				SDL_EventState(SDL_CONTROLLERDEVICEREMOVED, SDL_ENABLE);

				SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
				SDL_SetHint(SDL_HINT_TOUCH_MOUSE_EVENTS, "0");
				SDL_SetHint(SDL_HINT_MOUSE_TOUCH_EVENTS, "0");
				SDL_SetHint(SDL_HINT_JOYSTICK_HIDAPI, "1");
				SDL_SetHint(SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight");

				SDL_JoystickEventState(SDL_ENABLE);

				WakeWindowThreadEventId = SDL_RegisterEvents(1);
			}
		);
#elif PLATFORM_WINDOWS
		SetProcessDpiAwareness(PROCESS_PER_MONITOR_DPI_AWARE);
#endif
	}

	static Event<void(void*), 8> CloseAllWindows;

	/* static */ void Window::Destroy()
	{
		CloseAllWindows();

#if USE_WINDOW_THREAD
		UniquePtr<WindowThread>& pWindowThread = GetWindowThread();
		pWindowThread->OnEngineQuit();
		pWindowThread.DestroyElement();
#endif

#if USE_SDL
		SDL_QuitSubSystem(SDL_INIT_VIDEO);
		SDL_Quit();
		IsWindowThreadActivated = false;
#endif
	}

#if USE_SDL
	Math::Vector2ui GetWindowClientAreaSize(SDL_Window* pWindow)
	{
		Math::Vector2i size{100};
		SDL_GetWindowSize(pWindow, &size.x, &size.y);
		return (Math::Vector2ui)size;
	}

	Math::Vector2ui GetWindowSize(SDL_Window* pWindow)
	{
		Math::Vector2i size{100};
		SDL_GetWindowSize(pWindow, &size.x, &size.y);
		Math::RectangleEdgesi edges{0};
		SDL_GetWindowBordersSize(pWindow, &edges.m_top, &edges.m_left, &edges.m_bottom, &edges.m_right);
		return (Math::Vector2ui)size + (Math::Vector2ui)edges.GetSum();
	};

	Math::Vector2i GetWindowClientAreaPosition(SDL_Window* pWindow, const Math::Vector2i position)
	{
		Math::RectangleEdgesi edges{0};
		SDL_GetWindowBordersSize(pWindow, &edges.m_top, &edges.m_left, &edges.m_bottom, &edges.m_right);
		return position + Math::Vector2i{edges.m_left, edges.m_top};
	}

	static SDL_Window* PrimaryWindow = nullptr;
	SDL_Window* CreateWindow(
		const ZeroTerminatedUnicodeStringView name,
		const Math::Vector2i position,
		const Math::Vector2ui clientAreaSize,
		Window& window,
		const EnumFlags<Window::CreationFlags> flags
	)
	{
		SDL_Window* pWindow{nullptr};

		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[=, &pWindow, &window]()
			{
				const uint32 windowFlags = SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI | SDL_WINDOW_HIDDEN | SDL_WINDOW_VULKAN;

				String nameString(name);
				pWindow = SDL_CreateWindow(nameString.GetZeroTerminated(), position.x, position.y, clientAreaSize.x, clientAreaSize.y, windowFlags);

				Math::RectangleEdgesi edges{0};
				SDL_GetWindowBordersSize(pWindow, &edges.m_top, &edges.m_left, &edges.m_bottom, &edges.m_right);

				if (flags.IsSet(Rendering::Window::CreationFlags::SizeIsWindowArea))
				{
					SDL_SetWindowSize(pWindow, clientAreaSize.x, clientAreaSize.y - edges.m_top);
				}
				SDL_SetWindowPosition(pWindow, position.x, position.y + edges.m_top);

				SDL_SetWindowData(pWindow, "Window", &window);
			}
		);
		PrimaryWindow = pWindow;
		return pWindow;
	}
#elif PLATFORM_WINDOWS
	LRESULT CALLBACK WindowProc(const HWND windowHandle, UINT message, WPARAM wParam, LPARAM lParam)
	{
		const LONG_PTR userData = GetWindowLongPtrA(windowHandle, GWLP_USERDATA);
		if (userData == 0)
		{
			return DefWindowProc(windowHandle, message, wParam, lParam);
		}

		Window& window = *reinterpret_cast<Window*>(userData);
		return window.ProcessWindowMessage(message, wParam, lParam);
	}

	void* CreateWindow(
		const ZeroTerminatedUnicodeStringView name,
		const Math::Vector2i position,
		const Math::Vector2ui clientAreaSize,
		Window& window,
		const EnumFlags<Window::CreationFlags> flags
	)
	{
		const HINSTANCE applicationInstance = GetModuleHandle(nullptr);
		static constexpr ConstNativeZeroTerminatedStringView windowClassName = MAKE_NATIVE_LITERAL("ngineWindow");

		static bool registeredWindowClass = false;
		if (!registeredWindowClass)
		{
			registeredWindowClass = true;

			WNDCLASSW windowClassDesc = {};
			windowClassDesc.lpfnWndProc = WindowProc;
			windowClassDesc.lpszClassName = windowClassName;
			windowClassDesc.hInstance = applicationInstance;
			windowClassDesc.style = CS_VREDRAW | CS_HREDRAW;
			[[maybe_unused]] const ATOM classAtom = RegisterClassW(&windowClassDesc);
			Assert(classAtom != 0);
		}

		unsigned long windowStyle = WS_POPUP | WS_THICKFRAME | WS_SYSMENU | WS_MAXIMIZEBOX | WS_MINIMIZEBOX | WS_CLIPCHILDREN;
		unsigned long windowStyleEx = WS_EX_LAYERED;

		// Calculate size of the main window based on desired client area
		RECT windowRect;
		windowRect.left = position.x;
		windowRect.top = position.y;
		if (flags.IsSet(Window::CreationFlags::SizeIsClientArea))
		{
			windowRect.right = windowRect.left + clientAreaSize.x;
			windowRect.bottom = windowRect.top + clientAreaSize.y;
			::AdjustWindowRectEx(&windowRect, windowStyle, false, windowStyleEx);
		}
		else
		{
			windowRect.right = windowRect.left + clientAreaSize.x;
			windowRect.bottom = windowRect.top + clientAreaSize.y;
		}

		const Math::Vector2ui windowSize(windowRect.right - windowRect.left, windowRect.bottom - windowRect.top);

		HWND windowHandle;

		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[&windowHandle, &window, position, windowSize, name, windowStyle, windowStyleEx, applicationInstance]()
			{
				windowHandle = CreateWindowExW(
					windowStyleEx,
					windowClassName,
					name,
					windowStyle,
					position.x,
					position.y,
					windowSize.x,
					windowSize.y,
					nullptr,
					nullptr,
					applicationInstance,
					nullptr
				);

				const MARGINS margins = {1, 1, 1, 1};
				DwmExtendFrameIntoClientArea(windowHandle, &margins);

				SetWindowPos(
					windowHandle,
					nullptr,
					0,
					0,
					0,
					0,
					SWP_FRAMECHANGED | SWP_NOACTIVATE | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOREPOSITION | SWP_NOSENDCHANGING |
						SWP_NOSIZE | SWP_NOZORDER
				);
				SetLayeredWindowAttributes(windowHandle, 0x00, 255, LWA_ALPHA);

				SetWindowLongPtrA(windowHandle, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(&window));

				DragAcceptFiles(windowHandle, TRUE);
			}
		);

		return windowHandle;
	}

	Math::Vector2ui GetWindowSize(void* pWindowHandle)
	{
		RECT rect;
		GetWindowRect(static_cast<HWND>(pWindowHandle), &rect);
		return {uint32(rect.right - rect.left), uint32(rect.bottom - rect.top)};
	}

	Math::Vector2i GetWindowClientAreaPosition(void* pWindowHandle)
	{
		RECT clientArea;
		GetClientRect(static_cast<HWND>(pWindowHandle), &clientArea);
		POINT location = {clientArea.left, clientArea.top};
		ClientToScreen(static_cast<HWND>(pWindowHandle), &location);
		return {location.x, location.y};
	}

	Math::Vector2i GetWindowAreaPosition(void* pWindowHandle)
	{
		RECT clientArea;
		GetWindowRect(static_cast<HWND>(pWindowHandle), &clientArea);
		return {clientArea.left, clientArea.top};
	}

	Math::Vector2ui GetWindowClientAreaSize(void* pWindowHandle)
	{
		RECT clientArea;
		GetClientRect(static_cast<HWND>(pWindowHandle), &clientArea);
		return (Math::Vector2ui)Math::Vector2i{clientArea.right - clientArea.left, clientArea.bottom - clientArea.top};
	}
#elif PLATFORM_APPLE_MACOS

	[[nodiscard]] Math::Vector2ui GetWindowClientAreaSize(void* pWindowHandle)
	{
		NSWindow* pWindow = (__bridge NSWindow*)pWindowHandle;

		Math::Vector2ui result{Math::Zero};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[&result, pWindow]()
			{
				CGSize size = pWindow.frame.size;
				const CGFloat scale = pWindow.backingScaleFactor;
				result = {(uint32)(size.width * scale), (uint32)(size.height * scale)};
			}
		);
		return result;
	}
	[[nodiscard]] void* CreateWindow(const Window::Initializer& initializer)
	{
		NSWindow* pWindow;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[&initializer, &pWindow]()
			{
				pWindow = [[::Window alloc] initWithContentRect:NSMakeRect(
																													initializer.m_area.GetPosition().x,
																													initializer.m_area.GetPosition().y,
																													initializer.m_area.GetSize().x,
																													initializer.m_area.GetSize().y
																												)
																							styleMask:NSWindowStyleMaskClosable | NSWindowStyleMaskResizable |
																												NSWindowStyleMaskFullSizeContentView | NSWindowStyleMaskTitled
																								backing:NSBackingStoreBuffered
																									defer:NO];
				pWindow.titlebarAppearsTransparent = true;
				pWindow.titleVisibility = NSWindowTitleHidden;
				pWindow.toolbar = nullptr;
				pWindow.toolbarStyle = NSWindowToolbarStyleUnifiedCompact;
			}
		);
		return (__bridge_retained void*)pWindow;
	}

	Math::Vector2i GetWindowClientAreaPosition(void* pWindow)
	{
		NSWindow* pNSWindow = (__bridge NSWindow*)pWindow;
		CGPoint location;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[pNSWindow, &location]()
			{
				location = pNSWindow.frame.origin;
				const CGFloat scale = pNSWindow.backingScaleFactor;
				location.x *= scale;
				location.y *= scale;
			}
		);
		return (Math::Vector2i)Math::Vector2d{location.x, location.y};
	}

#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS

#if PLATFORM_APPLE_VISIONOS
	inline static constexpr float VisionOSWindowScalingFactor = 1.5f;
	inline static constexpr float VisionOSUIScalingFactor = 1.8f;
#endif

	[[nodiscard]] Math::Vector2ui GetWindowClientAreaSize(void* pWindowHandle)
	{
		UIWindow* pWindow = (__bridge UIWindow*)pWindowHandle;

		Math::Vector2ui result{Math::Zero};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[&result, pWindow]()
			{
				UIWindowScene* __strong windowScene = pWindow.windowScene;
				CGSize size = windowScene.coordinateSpace.bounds.size;
#if PLATFORM_APPLE_IOS
				UIScreen* screen = pWindow.screen;
				if (screen == nil)
				{
					screen = windowScene.screen;
				}

				const float contentScaleFactor = (float)pWindow.screen.nativeScale;
				size.width *= contentScaleFactor;
				size.height *= contentScaleFactor;
#elif PLATFORM_APPLE_VISIONOS
				size.width *= VisionOSWindowScalingFactor;
				size.height *= VisionOSWindowScalingFactor;
#endif
				result = {(uint32)size.width, (uint32)size.height};
			}
		);
		return result;
	}

	[[nodiscard]] void* CreateWindow(const Window::Initializer& initializer)
	{
		UIWindow* pWindow;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[&initializer, &pWindow]()
			{
				UIWindowScene* pWindowScene = (__bridge UIWindowScene*)initializer.m_pUIWindowScene;
#if PLATFORM_APPLE_VISIONOS
				pWindow = pWindowScene.keyWindow;
#else
				pWindow = [[::Window alloc] initWithWindowScene:pWindowScene];
#if PLATFORM_APPLE_IOS
				UIScreen* screen = pWindow.screen;
				if (screen == nil)
				{
					screen = pWindowScene.screen;
				}

				CGRect screenBounds = [screen bounds];
				CGFloat screenScale = [screen nativeScale];
				CGSize screenSize = CGSizeMake(screenBounds.size.width * screenScale, screenBounds.size.height * screenScale);

				// Hack for Mac Catalyst: no way of finding visible screen frame.
				const CGFloat dockHeightEstimate = 130 * screenScale * PLATFORM_APPLE_MACCATALYST;

				const Math::TRectangle<double> windowArea{Math::Zero, Math::Vector2d{screenSize.width, screenSize.height - dockHeightEstimate}};

				const float contentScaleFactor = (float)screen.nativeScale;
				const Math::Rectanglef adjustedArea = (Math::Rectanglef)windowArea / Math::Vector2f{contentScaleFactor};
#endif
				pWindow.canResizeToFitContent = false;
				pWindow.autoresizingMask = UIViewAutoresizingFlexibleWidth | UIViewAutoresizingFlexibleHeight |
			                             UIViewAutoresizingFlexibleLeftMargin | UIViewAutoresizingFlexibleRightMargin |
			                             UIViewAutoresizingFlexibleTopMargin | UIViewAutoresizingFlexibleBottomMargin;
				pWindow.autoresizesSubviews = true;

				CGRect newFrame;
#if PLATFORM_APPLE_IOS
				newFrame = {{adjustedArea.GetPosition().x, adjustedArea.GetPosition().y}, {adjustedArea.GetSize().x, adjustedArea.GetSize().y}};
				pWindow.frame = newFrame;
#else
				newSize = CGRectMake(0, 0, pWindow.frame.size.width, pWindow.frame.size.height);
#endif
				[pWindow setNeedsLayout];
				[pWindow layoutIfNeeded];
				[pWindow layoutSubviews];

				if (pWindowScene.sizeRestrictions != nil)
				{
					// Hack for Mac Catalyst: No way of specifying screen size.
				  // Force bounds to get correct size, and then restore as soon as possible.
				  // Note: This vacates the OS' history of user chosen size.
					pWindowScene.sizeRestrictions.minimumSize = newFrame.size;
					pWindowScene.sizeRestrictions.maximumSize = newFrame.size;
					dispatch_after(dispatch_time(DISPATCH_TIME_NOW, 100 * NSEC_PER_MSEC), dispatch_get_main_queue(), ^{
						pWindowScene.sizeRestrictions.minimumSize = CGSizeMake(100, 100);
						pWindowScene.sizeRestrictions.maximumSize = CGSizeMake(60000, 60000);
					});
				}
#endif
			}
		);
		return (__bridge_retained void*)pWindow;
	}
#endif

#if PLATFORM_EMSCRIPTEN
	Math::Vector2ui GetWindowClientAreaSize(const ConstZeroTerminatedStringView selector)
	{
		Math::Vector2d canvasSize;
		emscripten_get_element_css_size(selector, &canvasSize.x, &canvasSize.y);

		emscripten_set_canvas_element_size(selector, (int)canvasSize.x, (int)canvasSize.y);

		canvasSize *= Math::Vector2d{emscripten_get_device_pixel_ratio()};
		return (Math::Vector2ui)canvasSize;
	}
#endif

#if USE_WINDOWS_CONTROLLER
	inline static Array GamepadMappings = {
		Input::GamepadMapping{(int)XInput::Gamepad::A, Input::GamepadInput::Button::A},
		Input::GamepadMapping{(int)XInput::Gamepad::B, Input::GamepadInput::Button::B},
		Input::GamepadMapping{(int)XInput::Gamepad::X, Input::GamepadInput::Button::X},
		Input::GamepadMapping{(int)XInput::Gamepad::Y, Input::GamepadInput::Button::Y},
		Input::GamepadMapping{(int)XInput::Gamepad::LEFT_SHOULDER, Input::GamepadInput::Button::LeftShoulder},
		Input::GamepadMapping{(int)XInput::Gamepad::RIGHT_SHOULDER, Input::GamepadInput::Button::RightShoulder},
		Input::GamepadMapping{(int)XInput::Gamepad::LEFT_THUMB, Input::GamepadInput::Button::LeftThumbstick},
		Input::GamepadMapping{(int)XInput::Gamepad::RIGHT_THUMB, Input::GamepadInput::Button::RightThumbstick},
		Input::GamepadMapping{(int)XInput::Gamepad::BACK, Input::GamepadInput::Button::Menu},
		Input::GamepadMapping{(int)XInput::Gamepad::START, Input::GamepadInput::Button::Home},
		Input::GamepadMapping{(int)XInput::Gamepad::DPAD_LEFT, Input::GamepadInput::Button::DirectionPadLeft},
		Input::GamepadMapping{(int)XInput::Gamepad::DPAD_RIGHT, Input::GamepadInput::Button::DirectionPadRight},
		Input::GamepadMapping{(int)XInput::Gamepad::DPAD_UP, Input::GamepadInput::Button::DirectionPadUp},
		Input::GamepadMapping{(int)XInput::Gamepad::DPAD_DOWN, Input::GamepadInput::Button::DirectionPadDown}
	};
#endif

	Window::Window(Initializer&& initializer)
		: m_stateFlags(StateFlags::InBackground)
		, m_title(initializer.m_name)
		, m_position(initializer.m_area.GetPosition())
		, m_clientAreaPosition(0, 0)
		, m_clientAreaSize(initializer.m_area.GetSize())
#if USE_SDL
		, m_pSDLWindow(CreateWindow(
				initializer.m_name, initializer.m_area.GetPosition(), (Math::Vector2ui)initializer.m_area.GetSize(), *this, initializer.m_flags
			))
#elif PLATFORM_WINDOWS
		, m_pWindowHandle(CreateWindow(
				initializer.m_name, initializer.m_area.GetPosition(), (Math::Vector2ui)initializer.m_area.GetSize(), *this, initializer.m_flags
			))
#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		, m_pUIWindow(CreateWindow(initializer))
#elif PLATFORM_APPLE_MACOS
		, m_pNSWindow(CreateWindow(initializer))
#elif PLATFORM_ANDROID
		, m_pGameActivityWindow(initializer.m_pGameActivityWindow)
#elif PLATFORM_WEB
		, m_canvasSelector(initializer.m_canvasSelector)
#endif
		, m_dotsPerInch(GetDotsPerInchInternal())
		, m_devicePixelRatio(GetDevicePixelRatioInternal())
		, m_physicalDevicePixelRatio(GetPhysicalDevicePixelRatioInternal())
		, m_maximumDisplayRefreshRate(GetMaximumScreenRefreshRateInternal())
		, m_safeAreaInsets(GetSafeAreaInsetsInternal())
#if PLATFORM_APPLE
		, m_pMetalLayer(CreateMetalLayer(initializer.m_flags))
#endif
#if RENDERER_VULKAN
		, m_surface(System::Get<Renderer>().GetInstance(), {}, GetSurfaceHandle())
		, m_pLogicalDevice(
				initializer.m_pLogicalDevice.IsValid() ? initializer.m_pLogicalDevice
																							 : m_surface.CreateLogicalDeviceForSurface(System::Get<Renderer>())
			)
#elif RENDERER_METAL
		, m_surface(System::Get<Renderer>().GetInstance(), {}, GetSurfaceHandle())
		, m_pLogicalDevice(
				initializer.m_pLogicalDevice.IsValid() ? initializer.m_pLogicalDevice
																							 : m_surface.CreateLogicalDeviceForSurface(System::Get<Renderer>())
			)
#elif RENDERER_WEBGPU
		, m_pLogicalDevice(
				initializer.m_pLogicalDevice.IsValid()
					? initializer.m_pLogicalDevice
					: System::Get<Renderer>().CreateLogicalDeviceFromPhysicalDevice(*Surface::GetBestPhysicalDevice(System::Get<Renderer>()))
			)
		, m_surface(System::Get<Renderer>().GetInstance(), *m_pLogicalDevice, GetSurfaceHandle())
#endif
#if USE_SDL
		, m_hasKeyboardFocus(m_pSDLWindow == SDL_GetKeyboardFocus())
#elif PLATFORM_WINDOWS
		, m_hasKeyboardFocus(m_pWindowHandle == ::GetFocus())
#endif
	{
#if USE_SDL
		m_clientAreaPosition = GetWindowClientAreaPosition(m_pSDLWindow, initializer.m_area.GetPosition());
		m_clientAreaSize = GetWindowClientAreaSize(m_pSDLWindow);
#elif PLATFORM_WINDOWS
		m_clientAreaSize = GetWindowClientAreaSize(m_pWindowHandle);
#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		m_clientAreaSize = GetWindowClientAreaSize(m_pUIWindow);
#elif PLATFORM_APPLE_MACOS
		m_clientAreaPosition = GetWindowClientAreaPosition(m_pNSWindow);
		m_clientAreaSize = GetWindowClientAreaSize(m_pNSWindow);
#elif PLATFORM_ANDROID
		m_clientAreaSize = m_pGameActivityWindow->GetClientAreaSize();
#elif PLATFORM_EMSCRIPTEN
		m_clientAreaSize = GetWindowClientAreaSize(initializer.m_canvasSelector);
#endif

		CloseAllWindows.Add(
			*this,
			[](Window& window)
			{
				window.Close();
			}
		);

#if RENDERER_VULKAN && !PLATFORM_ANDROID
		if (m_surface.IsValid())
		{
			const Rendering::PhysicalDeviceView physicalDevice = m_pLogicalDevice->GetPhysicalDevice();
			// todo: standardize these in functions
			[[maybe_unused]] uint32 presentQueueIndexOut = 0;
			VkBool32 canPresent = false;
			vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, presentQueueIndexOut, m_surface, &canPresent);

			uint32 formatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_surface, &formatCount, nullptr);

			uint32 presentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_surface, &presentModeCount, nullptr);

			[[maybe_unused]] const bool suitableDevice = canPresent & (formatCount != 0) & (presentModeCount != 0);
			Assert(suitableDevice);
		}
#endif

#if PLATFORM_WINDOWS
		m_pDropTarget.CreateInPlace(*this);
		m_pDropSource.CreateInPlace(*this);

		CreateInteractionContext(reinterpret_cast<HINTERACTIONCONTEXT*>(&m_pInteractionContext));

		SetPropertyInteractionContext(
			reinterpret_cast<HINTERACTIONCONTEXT>(m_pInteractionContext),
			INTERACTION_CONTEXT_PROPERTY_FILTER_POINTERS,
			false
		);

		const Array interactionContextConfigs = {
			INTERACTION_CONTEXT_CONFIGURATION{
				INTERACTION_ID_TAP,
				INTERACTION_CONFIGURATION_FLAG_TAP | INTERACTION_CONFIGURATION_FLAG_TAP_DOUBLE | INTERACTION_CONFIGURATION_FLAG_TAP_MULTIPLE_FINGER
			},
			INTERACTION_CONTEXT_CONFIGURATION{INTERACTION_ID_SECONDARY_TAP, INTERACTION_CONFIGURATION_FLAG_SECONDARY_TAP},
			INTERACTION_CONTEXT_CONFIGURATION{
				INTERACTION_ID_HOLD,
				INTERACTION_CONFIGURATION_FLAG_HOLD | INTERACTION_CONFIGURATION_FLAG_HOLD_MULTIPLE_FINGER
			},
			INTERACTION_CONTEXT_CONFIGURATION{
				INTERACTION_ID_MANIPULATION,
				INTERACTION_CONFIGURATION_FLAG_MANIPULATION | INTERACTION_CONFIGURATION_FLAG_MANIPULATION_TRANSLATION_X |
					INTERACTION_CONFIGURATION_FLAG_MANIPULATION_TRANSLATION_Y | INTERACTION_CONFIGURATION_FLAG_MANIPULATION_SCALING |
					INTERACTION_CONFIGURATION_FLAG_MANIPULATION_MULTIPLE_FINGER_PANNING | INTERACTION_CONFIGURATION_FLAG_MANIPULATION_ROTATION
			},
		};

		SetInteractionConfigurationInteractionContext(
			reinterpret_cast<HINTERACTIONCONTEXT>(m_pInteractionContext),
			interactionContextConfigs.GetSize(),
			interactionContextConfigs.GetData()
		);
		RegisterOutputCallbackInteractionContext(reinterpret_cast<HINTERACTIONCONTEXT>(m_pInteractionContext), OnInteraction, this);

		if constexpr (s_useVirtualGamepad)
		{
			Input::Manager& inputManager = System::Get<Input::Manager>();

			Input::GamepadDeviceType& gamepadDeviceType =
				inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());
			gamepadDeviceType.EnableVirtualGamepad();
		}

		// Init xinput and setup polling thread
		m_scheduledTimerHandle = System::Get<Threading::JobManager>().ScheduleRecurringAsync(
			2_seconds,
			[this]()
			{
				Input::Manager& inputManager = System::Get<Input::Manager>();

				Input::GamepadDeviceType& gamepadDeviceType =
					inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

				DWORD dwResult;
				uint8 controllerCount = 0;
				for (uint8 i = 0; i < XUSER_MAX_COUNT; ++i)
				{
					XINPUT_STATE state;
					ZeroMemory(&state, sizeof(XINPUT_STATE));

					dwResult = XInputGetState(i, &state);
					if (dwResult == ERROR_SUCCESS)
					{
						if (m_gamepads[i].IsInvalid())
						{
							m_gamepads[i] = gamepadDeviceType.GetOrRegisterInstance(static_cast<uintptr>(i), inputManager, *this);
						}
						++controllerCount;
					}
					else
					{
						if (m_gamepads[i].IsValid())
						{
							m_gamepads[i] = Input::DeviceIdentifier();
						}
					}
				}

				if constexpr (s_useVirtualGamepad)
				{
					if (controllerCount)
					{
						gamepadDeviceType.DisableVirtualGamepad();
					}
					else
					{
						gamepadDeviceType.EnableVirtualGamepad();
					}
				}

				return true;
			},
			Threading::JobPriority::FirstBackground
		);

		Threading::Job& stage = Threading::CreateCallback(
			[this](Threading::JobRunnerThread&)
			{
				Input::Manager& inputManager = System::Get<Input::Manager>();

				Input::GamepadDeviceType& gamepadDeviceType =
					inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

				for (uint8 i = 0; i < m_gamepads.GetSize(); ++i)
				{
					if (m_gamepads[i].IsValid())
					{
						XINPUT_STATE state;
						ZeroMemory(&state, sizeof(XINPUT_STATE));
						if (!XInputGetState(i, &state))
						{
							gamepadDeviceType.OnAxisInput(
								m_gamepads[i],
								Input::GamepadInput::Axis::LeftThumbstick,
								Input::GetGamepadAxisValue(state.Gamepad.sThumbLX, state.Gamepad.sThumbLY)
							);
							gamepadDeviceType.OnAxisInput(
								m_gamepads[i],
								Input::GamepadInput::Axis::RightThumbstick,
								Input::GetGamepadAxisValue(state.Gamepad.sThumbRX, state.Gamepad.sThumbRY)
							);

							gamepadDeviceType.OnAnalogInput(
								m_gamepads[i],
								Input::GamepadInput::Analog::LeftTrigger,
								Input::GetGamepadAnalogValue(state.Gamepad.bLeftTrigger)
							);
							gamepadDeviceType.OnAnalogInput(
								m_gamepads[i],
								Input::GamepadInput::Analog::RightTrigger,
								Input::GetGamepadAnalogValue(state.Gamepad.bRightTrigger)
							);

							for (Input::GamepadMapping mapping : GamepadMappings)
							{
								if ((state.Gamepad.wButtons & mapping.m_source) == mapping.m_source)
								{
									gamepadDeviceType.OnButtonDown(m_gamepads[i], mapping.m_physicalTarget.GetExpected<Input::GamepadInput::Button>());
								}
								else
								{
									gamepadDeviceType.OnButtonUp(m_gamepads[i], mapping.m_physicalTarget.GetExpected<Input::GamepadInput::Button>());
								}
							}
						}
					}
				}
				return Threading::CallbackResult::Finished;
			},
			Threading::JobPriority::UserInputPolling
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
#elif PLATFORM_EMSCRIPTEN
		const static auto getKeyboardInput = [](const uint32 keyCode, const DOM_KEY_LOCATION location, const ConstStringView key)
		{
			{
				const Input::KeyboardInput input = Input::GetKeyboardInputFromString(key);
				if (input != Input::KeyboardInput::Unknown)
				{
					switch (input)
					{
						case Input::KeyboardInput::LeftShift:
							return (location == DOM_KEY_LOCATION_LEFT) ? Input::KeyboardInput::LeftShift : Input::KeyboardInput::RightShift;
						case Input::KeyboardInput::LeftControl:
							return (location == DOM_KEY_LOCATION_LEFT) ? Input::KeyboardInput::LeftControl : Input::KeyboardInput::RightControl;
						case Input::KeyboardInput::LeftAlt:
							return (location == DOM_KEY_LOCATION_LEFT) ? Input::KeyboardInput::LeftAlt : Input::KeyboardInput::RightAlt;
						case Input::KeyboardInput::LeftCommand:
							return (location == DOM_KEY_LOCATION_LEFT) ? Input::KeyboardInput::LeftCommand : Input::KeyboardInput::RightCommand;
						default:
							return input;
					}
				}
			}

			switch (keyCode)
			{
				case DOM_VK_BACK_SPACE:
					return Input::KeyboardInput::Backspace;
				case DOM_VK_TAB:
					return Input::KeyboardInput::Tab;
				case DOM_VK_ENTER:
					return Input::KeyboardInput::Enter;
				case DOM_VK_SHIFT:
					return (location == DOM_KEY_LOCATION_LEFT) ? Input::KeyboardInput::LeftShift : Input::KeyboardInput::RightShift;
				case DOM_VK_CONTROL:
					return (location == DOM_KEY_LOCATION_LEFT) ? Input::KeyboardInput::LeftControl : Input::KeyboardInput::RightControl;
				case DOM_VK_ALT:
					return (location == DOM_KEY_LOCATION_LEFT) ? Input::KeyboardInput::LeftAlt : Input::KeyboardInput::RightAlt;
				case DOM_VK_META:
				case DOM_VK_WIN:
					return (location == DOM_KEY_LOCATION_LEFT) ? Input::KeyboardInput::LeftCommand : Input::KeyboardInput::RightCommand;
				case DOM_VK_CAPS_LOCK:
					return Input::KeyboardInput::CapsLock;
				case DOM_VK_ESCAPE:
					return Input::KeyboardInput::Escape;
				case DOM_VK_SPACE:
					return Input::KeyboardInput::Space;
				case DOM_VK_PAGE_UP:
					return Input::KeyboardInput::PageUp;
				case DOM_VK_PAGE_DOWN:
					return Input::KeyboardInput::PageDown;
				case DOM_VK_END:
					return Input::KeyboardInput::End;
				case DOM_VK_HOME:
					return Input::KeyboardInput::Home;
				case DOM_VK_LEFT:
					return Input::KeyboardInput::ArrowLeft;
				case DOM_VK_UP:
					return Input::KeyboardInput::ArrowUp;
				case DOM_VK_RIGHT:
					return Input::KeyboardInput::ArrowRight;
				case DOM_VK_DOWN:
					return Input::KeyboardInput::ArrowDown;
				case DOM_VK_INSERT:
					return Input::KeyboardInput::Insert;
				case DOM_VK_DELETE:
					return Input::KeyboardInput::Delete;
				case DOM_VK_0:
					return Input::KeyboardInput::Zero;
				case DOM_VK_1:
					return Input::KeyboardInput::One;
				case DOM_VK_2:
					return Input::KeyboardInput::Two;
				case DOM_VK_3:
					return Input::KeyboardInput::Three;
				case DOM_VK_4:
					return Input::KeyboardInput::Four;
				case DOM_VK_5:
					return Input::KeyboardInput::Five;
				case DOM_VK_6:
					return Input::KeyboardInput::Six;
				case DOM_VK_7:
					return Input::KeyboardInput::Seven;
				case DOM_VK_8:
					return Input::KeyboardInput::Eight;
				case DOM_VK_9:
					return Input::KeyboardInput::Nine;
				case DOM_VK_COLON:
					return Input::KeyboardInput::Colon;
				case DOM_VK_SEMICOLON:
					return Input::KeyboardInput::Semicolon;
				case DOM_VK_LESS_THAN:
					return Input::KeyboardInput::LessThan;
				case DOM_VK_EQUALS:
					return Input::KeyboardInput::Equals;
				case DOM_VK_GREATER_THAN:
					return Input::KeyboardInput::GreaterThan;
				case DOM_VK_QUESTION_MARK:
					return Input::KeyboardInput::QuestionMark;
				case DOM_VK_AT:
					return Input::KeyboardInput::At;
				case DOM_VK_A:
					return Input::KeyboardInput::A;
				case DOM_VK_B:
					return Input::KeyboardInput::B;
				case DOM_VK_C:
					return Input::KeyboardInput::C;
				case DOM_VK_D:
					return Input::KeyboardInput::D;
				case DOM_VK_E:
					return Input::KeyboardInput::E;
				case DOM_VK_F:
					return Input::KeyboardInput::F;
				case DOM_VK_G:
					return Input::KeyboardInput::G;
				case DOM_VK_H:
					return Input::KeyboardInput::H;
				case DOM_VK_I:
					return Input::KeyboardInput::I;
				case DOM_VK_J:
					return Input::KeyboardInput::J;
				case DOM_VK_K:
					return Input::KeyboardInput::K;
				case DOM_VK_L:
					return Input::KeyboardInput::L;
				case DOM_VK_M:
					return Input::KeyboardInput::M;
				case DOM_VK_N:
					return Input::KeyboardInput::N;
				case DOM_VK_O:
					return Input::KeyboardInput::O;
				case DOM_VK_P:
					return Input::KeyboardInput::P;
				case DOM_VK_Q:
					return Input::KeyboardInput::Q;
				case DOM_VK_R:
					return Input::KeyboardInput::R;
				case DOM_VK_S:
					return Input::KeyboardInput::S;
				case DOM_VK_T:
					return Input::KeyboardInput::T;
				case DOM_VK_U:
					return Input::KeyboardInput::U;
				case DOM_VK_V:
					return Input::KeyboardInput::V;
				case DOM_VK_W:
					return Input::KeyboardInput::W;
				case DOM_VK_X:
					return Input::KeyboardInput::X;
				case DOM_VK_Y:
					return Input::KeyboardInput::Y;
				case DOM_VK_Z:
					return Input::KeyboardInput::Z;
				case DOM_VK_NUMPAD0:
					return Input::KeyboardInput::Numpad0;
				case DOM_VK_NUMPAD1:
					return Input::KeyboardInput::Numpad1;
				case DOM_VK_NUMPAD2:
					return Input::KeyboardInput::Numpad2;
				case DOM_VK_NUMPAD3:
					return Input::KeyboardInput::Numpad3;
				case DOM_VK_NUMPAD4:
					return Input::KeyboardInput::Numpad4;
				case DOM_VK_NUMPAD5:
					return Input::KeyboardInput::Numpad5;
				case DOM_VK_NUMPAD6:
					return Input::KeyboardInput::Numpad6;
				case DOM_VK_NUMPAD7:
					return Input::KeyboardInput::Numpad7;
				case DOM_VK_NUMPAD8:
					return Input::KeyboardInput::Numpad8;
				case DOM_VK_NUMPAD9:
					return Input::KeyboardInput::Numpad9;
				case DOM_VK_MULTIPLY:
					return Input::KeyboardInput::Multiply;
				case DOM_VK_ADD:
					return Input::KeyboardInput::Add;
				case DOM_VK_SUBTRACT:
					return Input::KeyboardInput::Subtract;
				case DOM_VK_DECIMAL:
					return Input::KeyboardInput::Decimal;
				case DOM_VK_DIVIDE:
					return Input::KeyboardInput::Divide;
				case DOM_VK_F1:
					return Input::KeyboardInput::F1;
				case DOM_VK_F2:
					return Input::KeyboardInput::F2;
				case DOM_VK_F3:
					return Input::KeyboardInput::F3;
				case DOM_VK_F4:
					return Input::KeyboardInput::F4;
				case DOM_VK_F5:
					return Input::KeyboardInput::F5;
				case DOM_VK_F6:
					return Input::KeyboardInput::F6;
				case DOM_VK_F7:
					return Input::KeyboardInput::F7;
				case DOM_VK_F8:
					return Input::KeyboardInput::F8;
				case DOM_VK_F9:
					return Input::KeyboardInput::F9;
				case DOM_VK_F10:
					return Input::KeyboardInput::F10;
				case DOM_VK_F11:
					return Input::KeyboardInput::F11;
				case DOM_VK_F12:
					return Input::KeyboardInput::F12;
				case DOM_VK_CIRCUMFLEX:
					return Input::KeyboardInput::Circumflex;
				case DOM_VK_EXCLAMATION:
					return Input::KeyboardInput::Exclamation;
				case /*DOM_VK_DOUBLE_QUOTE*/ 0xA2:
					return Input::KeyboardInput::DoubleQuote;
				case DOM_VK_HASH:
					return Input::KeyboardInput::Hash;
				case DOM_VK_DOLLAR:
					return Input::KeyboardInput::Dollar;
				case DOM_VK_PERCENT:
					return Input::KeyboardInput::Percent;
				case DOM_VK_AMPERSAND:
					return Input::KeyboardInput::Ampersand;
				case DOM_VK_UNDERSCORE:
					return Input::KeyboardInput::Underscore;
				case DOM_VK_OPEN_PAREN:
					return Input::KeyboardInput::OpenParantheses;
				case DOM_VK_CLOSE_PAREN:
					return Input::KeyboardInput::CloseParantheses;
				case DOM_VK_ASTERISK:
					return Input::KeyboardInput::Asterisk;
				case DOM_VK_PLUS:
					return Input::KeyboardInput::Plus;
				case DOM_VK_PIPE:
					return Input::KeyboardInput::Pipe;
				case DOM_VK_HYPHEN_MINUS:
					return Input::KeyboardInput::Hyphen;
				case 0xBD:
					return Input::KeyboardInput::Minus;
				case DOM_VK_OPEN_CURLY_BRACKET:
					return Input::KeyboardInput::OpenCurlyBracket;
				case DOM_VK_CLOSE_CURLY_BRACKET:
					return Input::KeyboardInput::CloseCurlyBracket;
				case DOM_VK_TILDE:
					return Input::KeyboardInput::Tilde;
				case DOM_VK_COMMA:
					return Input::KeyboardInput::Comma;
				case DOM_VK_PERIOD:
					return Input::KeyboardInput::Period;
				case DOM_VK_SLASH:
					return Input::KeyboardInput::ForwardSlash;
				case DOM_VK_BACK_QUOTE:
					return Input::KeyboardInput::BackQuote;
				case DOM_VK_OPEN_BRACKET:
					return Input::KeyboardInput::OpenBracket;
				case DOM_VK_BACK_SLASH:
					return Input::KeyboardInput::BackSlash;
				case DOM_VK_CLOSE_BRACKET:
					return Input::KeyboardInput::CloseBracket;
				case DOM_VK_QUOTE:
					return Input::KeyboardInput::Quote;
			}
			return Input::KeyboardInput::Unknown;
		};

		[[maybe_unused]] EMSCRIPTEN_RESULT ret = emscripten_set_keydown_callback_on_thread(
			EMSCRIPTEN_EVENT_TARGET_WINDOW,
			this,
			1,
			[]([[maybe_unused]] const int eventType, const EmscriptenKeyboardEvent* e, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);

				Input::KeyboardDeviceType& keyboardDeviceType = System::Get<Input::Manager>().GetDeviceType<Input::KeyboardDeviceType>(
					System::Get<Input::Manager>().GetKeyboardDeviceTypeIdentifier()
				);
				const Input::DeviceIdentifier keyboardIdentifier =
					keyboardDeviceType.GetOrRegisterInstance(uintptr(0), System::Get<Input::Manager>(), window);
				LogMessage("keycode {}, location {}, key {}", e->keyCode, e->location, ConstStringView{e->key, (uint32)strlen(e->key)});

				const Input::KeyboardInput input = getKeyboardInput(e->keyCode, e->location, ConstStringView{e->key, (uint32)strlen(e->key)});
				keyboardDeviceType.OnKeyDown(keyboardIdentifier, input);

				switch (input)
				{
					case Input::KeyboardInput::Space:
						keyboardDeviceType.OnText(keyboardIdentifier, MAKE_UNICODE_LITERAL(" "));
						break;
					case Input::KeyboardInput::Backspace:
					case Input::KeyboardInput::Tab:
					case Input::KeyboardInput::Enter:
					case Input::KeyboardInput::CapsLock:
					case Input::KeyboardInput::LeftShift:
					case Input::KeyboardInput::RightShift:
					case Input::KeyboardInput::LeftControl:
					case Input::KeyboardInput::RightControl:
					case Input::KeyboardInput::LeftAlt:
					case Input::KeyboardInput::RightAlt:
					case Input::KeyboardInput::Escape:
					case Input::KeyboardInput::PageUp:
					case Input::KeyboardInput::PageDown:
					case Input::KeyboardInput::End:
					case Input::KeyboardInput::Home:
					case Input::KeyboardInput::ArrowLeft:
					case Input::KeyboardInput::ArrowUp:
					case Input::KeyboardInput::ArrowRight:
					case Input::KeyboardInput::ArrowDown:
					case Input::KeyboardInput::Insert:
					case Input::KeyboardInput::Delete:
					case Input::KeyboardInput::RightCommand:
					case Input::KeyboardInput::LeftCommand:
					case Input::KeyboardInput::F1:
					case Input::KeyboardInput::F2:
					case Input::KeyboardInput::F3:
					case Input::KeyboardInput::F4:
					case Input::KeyboardInput::F5:
					case Input::KeyboardInput::F6:
					case Input::KeyboardInput::F7:
					case Input::KeyboardInput::F8:
					case Input::KeyboardInput::F9:
					case Input::KeyboardInput::F10:
					case Input::KeyboardInput::F11:
					case Input::KeyboardInput::F12:
						break;
					default:
					{
						if (e->key[1] == '\0')
						{
							keyboardDeviceType.OnText(keyboardIdentifier, UnicodeString(String(&e->key[0], 1)));
						}
					}
					break;
				}

				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);
		ret = emscripten_set_keyup_callback_on_thread(
			EMSCRIPTEN_EVENT_TARGET_WINDOW,
			this,
			1,
			[]([[maybe_unused]] const int eventType, const EmscriptenKeyboardEvent* e, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);

				Input::KeyboardDeviceType& keyboardDeviceType = System::Get<Input::Manager>().GetDeviceType<Input::KeyboardDeviceType>(
					System::Get<Input::Manager>().GetKeyboardDeviceTypeIdentifier()
				);
				const Input::DeviceIdentifier keyboardIdentifier =
					keyboardDeviceType.GetOrRegisterInstance(uintptr(0), System::Get<Input::Manager>(), window);

				// TODO: Parse key instead
				LogMessage(
					"keycode {}, location {}, key {}, repeat {}",
					e->keyCode,
					e->location,
					ConstStringView{e->key, (uint32)strlen(e->key)},
					e->repeat
				);
				if (e->repeat == 0)
				{
					const Input::KeyboardInput input = getKeyboardInput(e->keyCode, e->location, ConstStringView{e->key, (uint32)strlen(e->key)});
					keyboardDeviceType.OnKeyUp(keyboardIdentifier, input);
				}
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		ret = emscripten_set_mousedown_callback_on_thread(
			initializer.m_canvasSelector,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenMouseEvent* e, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);

				Input::Manager& inputManager = System::Get<Input::Manager>();
				Input::MouseDeviceType& mouseDeviceType =
					inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
				const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(uintptr(0), inputManager, window);

				const Math::Vector2d computedCoordinate = Math::Vector2d{(double)e->clientX, (double)e->clientY} *
			                                            Math::Vector2d{window.GetDevicePixelRatio()};
				const ScreenCoordinate coordinate((Math::Vector2ui)computedCoordinate);
				window.SetInputFocusAtCoordinate(window.ConvertScreenToLocalCoordinates(coordinate), Invalid);

				const Input::MouseButton buttonMask = Input::MouseButton(1 << e->button);
				mouseDeviceType.OnPress(mouseIdentifier, coordinate, buttonMask, window);

				if (window.m_constrainedCursorCounter > 0)
				{
					EmscriptenPointerlockChangeEvent pointerLock;
					if (emscripten_get_pointerlock_status(&pointerLock) == EMSCRIPTEN_RESULT_SUCCESS && !pointerLock.isActive)
					{
						// Reconstrain cursor
						emscripten_request_pointerlock(window.m_canvasSelector.GetZeroTerminated(), 0);
					}
				}

				// TODO: Workaround: Audio can only be initialized after the first click
				static bool firstClickEvent = false;
				if (!firstClickEvent)
				{
					Events::Manager& eventManager = System::Get<Events::Manager>();
					eventManager.NotifyAll(eventManager.FindOrRegisterEvent("7bc481f1-4c81-47bb-8782-f222cbf527f6"_guid));
					firstClickEvent = true;
				}
				// ~TODO

				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);
		ret = emscripten_set_mouseup_callback_on_thread(
			initializer.m_canvasSelector,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenMouseEvent* e, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);

				Input::Manager& inputManager = System::Get<Input::Manager>();
				Input::MouseDeviceType& mouseDeviceType =
					inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
				const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(uintptr(0), inputManager, window);

				const Math::Vector2d computedCoordinate = Math::Vector2d{(double)e->clientX, (double)e->clientY} *
			                                            Math::Vector2d{window.GetDevicePixelRatio()};
				const ScreenCoordinate coordinate((Math::Vector2ui)computedCoordinate);
				window.SetInputFocusAtCoordinate(window.ConvertScreenToLocalCoordinates(coordinate), Invalid);

				const Input::MouseButton buttonMask = Input::MouseButton(1 << e->button);
				mouseDeviceType.OnRelease(mouseIdentifier, coordinate, buttonMask, &window);
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);
		ret = emscripten_set_mousemove_callback_on_thread(
			initializer.m_canvasSelector,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenMouseEvent* e, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);

				Input::Manager& inputManager = System::Get<Input::Manager>();
				Input::MouseDeviceType& mouseDeviceType =
					inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
				const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(uintptr(0), inputManager, window);

				const Math::Vector2d computedCoordinate = Math::Vector2d{(double)e->clientX, (double)e->clientY} *
			                                            Math::Vector2d{window.GetDevicePixelRatio()};
				const ScreenCoordinate coordinate((Math::Vector2ui)computedCoordinate);

				const Math::Vector2i deltaCoordinates{(int)e->movementX, (int)e->movementY};
				mouseDeviceType.OnMotion(mouseIdentifier, coordinate, deltaCoordinates, window);
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);
		ret = emscripten_set_mouseenter_callback_on_thread(
			initializer.m_canvasSelector,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenMouseEvent*, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);
				window.OnReceivedMouseFocus();
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);
		ret = emscripten_set_mouseleave_callback_on_thread(
			initializer.m_canvasSelector,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenMouseEvent*, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);
				window.OnLostMouseFocus();
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);

		ret = emscripten_set_wheel_callback_on_thread(
			initializer.m_canvasSelector,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenWheelEvent* e, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);

				Input::Manager& inputManager = System::Get<Input::Manager>();
				Input::MouseDeviceType& mouseDeviceType =
					inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
				const Input::DeviceIdentifier mouseIdentifier = mouseDeviceType.GetOrRegisterInstance(uintptr(0), inputManager, window);

				const Math::Vector2d computedCoordinate = Math::Vector2d{(double)e->mouse.clientX, (double)e->mouse.clientY} *
			                                            Math::Vector2d{window.GetDevicePixelRatio()};
				const ScreenCoordinate coordinate((Math::Vector2ui)computedCoordinate);

				mouseDeviceType.OnScroll(mouseIdentifier, coordinate, Math::Vector2i((int)e->deltaX, (int)-e->deltaY), window);
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			0
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		ret = emscripten_set_resize_callback_on_thread(
			EMSCRIPTEN_EVENT_TARGET_WINDOW,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenUiEvent*, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);

				Math::Vector2d canvasSize;
				emscripten_get_element_css_size(window.m_canvasSelector.GetZeroTerminated(), &canvasSize.x, &canvasSize.y);

				canvasSize *= Math::Vector2d{emscripten_get_device_pixel_ratio()};
				window.TryOrQueueResize((Math::Vector2ui)canvasSize);

				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		ret = emscripten_set_blur_callback_on_thread(
			EMSCRIPTEN_EVENT_TARGET_WINDOW,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenFocusEvent*, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);
				window.OnReceivedKeyboardFocus();

				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);
		ret = emscripten_set_focus_callback_on_thread(
			EMSCRIPTEN_EVENT_TARGET_WINDOW,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenFocusEvent*, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);
				window.OnLostKeyboardFocus();

				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		ret = emscripten_set_deviceorientation_callback_on_thread(
			this,
			1,
			[]([[maybe_unused]] const int eventType, const EmscriptenDeviceOrientationEvent*, void* userData) -> EM_BOOL
			{
				UNUSED(userData);
				// Window& window = *reinterpret_cast<Window*>(userData);
			  // Assert(false, "TODO");
			  // window.OnDisplayRotationChanged();
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);
		ret = emscripten_set_devicemotion_callback_on_thread(
			this,
			1,
			[]([[maybe_unused]] const int eventType, const EmscriptenDeviceMotionEvent*, void* userData) -> EM_BOOL
			{
				// TODO
				UNUSED(userData);
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		ret = emscripten_set_orientationchange_callback_on_thread(
			this,
			1,
			[]([[maybe_unused]] const int eventType, const EmscriptenOrientationChangeEvent*, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);
				Assert(false, "TODO");
				window.OnDisplayRotationChanged();
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		ret = emscripten_set_fullscreenchange_callback_on_thread(
			EMSCRIPTEN_EVENT_TARGET_DOCUMENT,
			this,
			1,
			[]([[maybe_unused]] const int eventType, const EmscriptenFullscreenChangeEvent*, void* userData) -> EM_BOOL
			{
				UNUSED(userData);
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		ret = emscripten_set_pointerlockchange_callback_on_thread(
			EMSCRIPTEN_EVENT_TARGET_DOCUMENT,
			this,
			1,
			[]([[maybe_unused]] const int eventType, const EmscriptenPointerlockChangeEvent*, void* userData) -> EM_BOOL
			{
				UNUSED(userData);
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		ret = emscripten_set_visibilitychange_callback_on_thread(
			this,
			1,
			[]([[maybe_unused]] const int eventType, const EmscriptenVisibilityChangeEvent* e, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);
				if (e->hidden)
				{
					window.OnBecomeHidden();
				}
				else
				{
					window.OnBecomeVisible();
				}

				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		// Enable virtual gamepad if a touch device is available
		Rendering::Window::QueueOnWindowThread(
			[]()
			{
				const bool hasTouchDevice = EM_ASM_INT({
																			if ('ontouchstart' in window)
																			{
																				return 1;
																			}
																			else
																			{
																				return 0;
																			}
																		}) != 0;
				if (hasTouchDevice)
				{
					Input::Manager& inputManager = System::Get<Input::Manager>();

					Input::GamepadDeviceType& gamepadDeviceType =
						inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());
					gamepadDeviceType.EnableVirtualGamepad();
				}
			}
		);

		ret = emscripten_set_touchstart_callback_on_thread(
			initializer.m_canvasSelector,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenTouchEvent* e, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);

				Input::Manager& inputManager = System::Get<Input::Manager>();
				Input::TouchscreenDeviceType& touchscreenDeviceType =
					inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());

				for (int touchIndex = 0, touchCount = e->numTouches; touchIndex < touchCount; ++touchIndex)
				{
					const EmscriptenTouchPoint& __restrict touchPoint = e->touches[touchIndex];

					const Input::DeviceIdentifier touchscreenIdentifier =
						touchscreenDeviceType.GetOrRegisterInstance(touchPoint.identifier, inputManager, window);

					const Math::Vector2d computedCoordinate = Math::Vector2d{(double)touchPoint.clientX, (double)touchPoint.clientY} *
				                                            Math::Vector2d{window.GetDevicePixelRatio()};
					const ScreenCoordinate coordinates((Math::Vector2ui)computedCoordinate);

					const Math::Ratiof pressureRatio = 100_percent;
					const Input::FingerIdentifier fingerIdentifier = (Input::FingerIdentifier)touchPoint.identifier;

					Input::TouchDescriptor touchDescriptor;
					touchDescriptor.fingerIdentifier = fingerIdentifier;
					touchDescriptor.touchRadius = Input::Monitor::DefaultTouchRadius;
					touchDescriptor.screenCoordinate = coordinates;
					touchDescriptor.pressureRatio = pressureRatio;

					touchscreenDeviceType.OnStartTouch(Move(touchDescriptor), touchscreenIdentifier, window);
				}

				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);
		ret = emscripten_set_touchend_callback_on_thread(
			initializer.m_canvasSelector,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenTouchEvent* e, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);

				Input::Manager& inputManager = System::Get<Input::Manager>();
				Input::TouchscreenDeviceType& touchscreenDeviceType =
					inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());

				for (int touchIndex = 0, touchCount = e->numTouches; touchIndex < touchCount; ++touchIndex)
				{
					const EmscriptenTouchPoint& __restrict touchPoint = e->touches[touchIndex];

					const Input::DeviceIdentifier touchscreenIdentifier =
						touchscreenDeviceType.GetOrRegisterInstance(touchPoint.identifier, inputManager, window);

					const Math::Vector2d computedCoordinate = Math::Vector2d{(double)touchPoint.clientX, (double)touchPoint.clientY} *
				                                            Math::Vector2d{window.GetDevicePixelRatio()};
					const ScreenCoordinate coordinates((Math::Vector2ui)computedCoordinate);

					const Math::Ratiof pressureRatio = 100_percent;
					const Input::FingerIdentifier fingerIdentifier = (Input::FingerIdentifier)touchPoint.identifier;

					Input::TouchDescriptor touchDescriptor;
					touchDescriptor.fingerIdentifier = fingerIdentifier;
					touchDescriptor.touchRadius = Input::Monitor::DefaultTouchRadius;
					touchDescriptor.screenCoordinate = coordinates;
					touchDescriptor.pressureRatio = pressureRatio;

					touchscreenDeviceType.OnStopTouch(Move(touchDescriptor), touchscreenIdentifier, window);
				}
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);
		ret = emscripten_set_touchmove_callback_on_thread(
			initializer.m_canvasSelector,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenTouchEvent* e, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);

				Input::Manager& inputManager = System::Get<Input::Manager>();
				Input::TouchscreenDeviceType& touchscreenDeviceType =
					inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());

				for (int touchIndex = 0, touchCount = e->numTouches; touchIndex < touchCount; ++touchIndex)
				{
					const EmscriptenTouchPoint& __restrict touchPoint = e->touches[touchIndex];

					const Input::DeviceIdentifier touchscreenIdentifier =
						touchscreenDeviceType.GetOrRegisterInstance(touchPoint.identifier, inputManager, window);

					const Math::Vector2d computedCoordinate = Math::Vector2d{(double)touchPoint.clientX, (double)touchPoint.clientY} *
				                                            Math::Vector2d{window.GetDevicePixelRatio()};
					const ScreenCoordinate coordinates((Math::Vector2ui)computedCoordinate);

					const Math::Ratiof pressureRatio = 100_percent;
					const Input::FingerIdentifier fingerIdentifier = (Input::FingerIdentifier)touchPoint.identifier;

					Input::TouchDescriptor touchDescriptor;
					touchDescriptor.fingerIdentifier = fingerIdentifier;
					touchDescriptor.touchRadius = Input::Monitor::DefaultTouchRadius;
					touchDescriptor.screenCoordinate = coordinates;
					touchDescriptor.pressureRatio = pressureRatio;

					touchscreenDeviceType.OnMotion(Move(touchDescriptor), touchscreenIdentifier, window);
				}
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);
		ret = emscripten_set_touchcancel_callback_on_thread(
			initializer.m_canvasSelector,
			this,
			0,
			[]([[maybe_unused]] const int eventType, const EmscriptenTouchEvent* e, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);

				Input::Manager& inputManager = System::Get<Input::Manager>();
				Input::TouchscreenDeviceType& touchscreenDeviceType =
					inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());

				for (int touchIndex = 0, touchCount = e->numTouches; touchIndex < touchCount; ++touchIndex)
				{
					const EmscriptenTouchPoint& __restrict touchPoint = e->touches[touchIndex];

					const Input::DeviceIdentifier touchscreenIdentifier =
						touchscreenDeviceType.GetOrRegisterInstance(touchPoint.identifier, inputManager, window);

					const Input::FingerIdentifier fingerIdentifier = (Input::FingerIdentifier)touchPoint.identifier;
					touchscreenDeviceType.OnCancelTouch(touchscreenIdentifier, fingerIdentifier, window);
				}
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		ret = emscripten_set_beforeunload_callback_on_thread(
			this,
			[]([[maybe_unused]] const int eventType, [[maybe_unused]] const void* reserved, void* userData) -> const char*
			{
				UNUSED(userData);
				return "Do you really want to leave the page?";
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		ret = emscripten_set_pointerlockchange_callback_on_thread(
			initializer.m_canvasSelector,
			this,
			true,
			[]([[maybe_unused]] int eventType, const EmscriptenPointerlockChangeEvent* pointerlockChangeEvent, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);
				UNUSED(window);
				UNUSED(pointerlockChangeEvent);
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);

		ret = emscripten_set_pointerlockerror_callback_on_thread(
			initializer.m_canvasSelector,
			this,
			true,
			[]([[maybe_unused]] int eventType, const void*, void* userData) -> EM_BOOL
			{
				Window& window = *reinterpret_cast<Window*>(userData);
				UNUSED(window);
				return EMSCRIPTEN_RESULT_SUCCESS;
			},
			EM_CALLBACK_THREAD_CONTEXT_MAIN_RUNTIME_THREAD
		);
		Assert(ret == EMSCRIPTEN_RESULT_SUCCESS);
#endif
	}

	Window::~Window()
	{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		CFRelease((CFTypeRef)m_pUIWindow);
#elif PLATFORM_APPLE_MACOS
		CFRelease((CFTypeRef)m_pNSWindow);
#endif

		if (UNLIKELY_ERROR(m_pLogicalDevice.IsInvalid()))
		{
			return;
		}

		Rendering::LogicalDevice& logicalDevice = *m_pLogicalDevice;
		logicalDevice.WaitUntilIdle();

		CloseAllWindows.Remove(this);

		m_surface.Destroy(m_pLogicalDevice->GetRenderer().GetInstance());

#if PLATFORM_WINDOWS
		DestroyInteractionContext(reinterpret_cast<HINTERACTIONCONTEXT>(m_pInteractionContext));

		if (m_scheduledTimerHandle.IsValid())
		{
			System::Get<Threading::JobManager>().CancelAsyncJob(m_scheduledTimerHandle);
		}
#endif
	}

	void Window::Initialize()
	{
		m_stateFlags |= StateFlags::IsInitialized;
		GetOnWindowCreated()(*this);

#if PLATFORM_APPLE_MACOS
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this]()
			{
				NSWindow* window = (__bridge NSWindow*)m_pNSWindow;
				MetalView* view = (MetalView*)window.contentViewController.view;
				view.engineWindow = this;
			}
		);
#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this]()
			{
				UIWindow* window = (__bridge UIWindow*)m_pUIWindow;
				MetalView* view = (MetalView*)window.rootViewController.view;
				view.engineWindow = this;
			}
		);
#endif
	}

	void Window::DestroySurface()
	{
		m_surface.Destroy(m_pLogicalDevice->GetRenderer().GetInstance());
	}

	/* static */ void Window::RequestWindowCreation(CreationRequestParameters&& parameters)
	{
		OnRequestWindowCreationCallback& callback = GetRequestWindowCreationCallback();
		if (callback.IsValid())
		{
			callback(Move(parameters));
		}
	}

	void Window::OnReceivedKeyboardFocus()
	{
		m_hasKeyboardFocus = true;

		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::KeyboardDeviceType& keyboardDeviceType =
			inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());
		keyboardDeviceType.OnActiveWindowReceivedKeyboardFocus();

		OnReceivedKeyboardFocusInternal();
	}

	void Window::OnLostKeyboardFocus()
	{
		m_hasKeyboardFocus = false;

		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::KeyboardDeviceType& keyboardDeviceType =
			inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());
		keyboardDeviceType.OnActiveWindowLostKeyboardFocus();

		OnLostKeyboardFocusInternal();
	}

	void Window::OnMoved(const Math::Vector2i newLocation)
	{
		m_position = newLocation;
#if USE_SDL
		m_clientAreaPosition = GetWindowClientAreaPosition(m_pSDLWindow, newLocation);
#elif PLATFORM_WINDOWS
		m_clientAreaPosition = GetWindowClientAreaPosition(m_pWindowHandle);
#endif

		m_dotsPerInch = GetDotsPerInchInternal();
		m_devicePixelRatio = GetDevicePixelRatioInternal();
		m_physicalDevicePixelRatio = GetPhysicalDevicePixelRatioInternal();
		m_maximumDisplayRefreshRate = GetMaximumScreenRefreshRateInternal();
		m_safeAreaInsets = GetSafeAreaInsetsInternal();

		OnDisplayPropertiesChanged();
	}

	void Window::OnDisplayRotationChanged()
	{
		m_dotsPerInch = GetDotsPerInchInternal();
		m_devicePixelRatio = GetDevicePixelRatioInternal();
		m_physicalDevicePixelRatio = GetPhysicalDevicePixelRatioInternal();
		m_maximumDisplayRefreshRate = GetMaximumScreenRefreshRateInternal();
		m_safeAreaInsets = GetSafeAreaInsetsInternal();

		OnDisplayPropertiesChanged();
	}

#if PLATFORM_APPLE
	void* Window::CreateMetalLayer(const EnumFlags<CreationFlags> flags)
	{
		void* pMetalLayerOut{nullptr};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, flags, &pMetalLayerOut]()
			{
#if PLATFORM_APPLE_MACOS
				NSWindow* window = (__bridge NSWindow*)m_pNSWindow;
				const CGRect desiredFrame = window.frame;

				NSStoryboard* storyboard = [NSStoryboard storyboardWithName:@"Main" bundle:nil];
				window.contentViewController = [storyboard instantiateControllerWithIdentifier:@"ViewController"];
				MetalView* view = (MetalView*)window.contentViewController.view;

				NSScreen* screen = window.screen;

				[window setContentSize:desiredFrame.size];
				[view setFrameSize:desiredFrame.size];
				view.wantsLayer = true;

				view.autoresizingMask = NSViewWidthSizable | NSViewHeightSizable | NSViewMinXMargin | NSViewMaxXMargin | NSViewMinYMargin |
			                          NSViewMaxYMargin;
				view.autoresizesSubviews = true;

				[view updateDrawableSize];

				CAMetalLayer* metalLayer = static_cast<CAMetalLayer*>(view.layer);
				metalLayer.contentsScale = screen.backingScaleFactor;

				pMetalLayerOut = (__bridge void*)metalLayer;
#elif PLATFORM_APPLE_VISIONOS
				UIWindow* window = (__bridge UIWindow*)m_pUIWindow;

				ViewController* viewController = [[ViewController alloc] init];
				MetalView* view = [[MetalView alloc] init];

				viewController.view = view;
				window.rootViewController = viewController;

				const Math::Rectanglef adjustedArea =
					Math::Rectanglef{(Math::Vector2f)m_position, (Math::Vector2f)GetWindowClientAreaSize(m_pUIWindow)};

				const float scale = VisionOSWindowScalingFactor;
				window.frame = CGRectMake(
					adjustedArea.GetPosition().x,
					adjustedArea.GetPosition().y,
					adjustedArea.GetSize().x / scale,
					adjustedArea.GetSize().y / scale
				);
				[view updateDrawableSize];

				CAMetalLayer* metalLayer = static_cast<CAMetalLayer*>(view.layer);
				metalLayer.contentsScale = VisionOSWindowScalingFactor;
				pMetalLayerOut = (__bridge void*)metalLayer;
#elif PLATFORM_APPLE_IOS
				UIWindow* window = (__bridge UIWindow*)m_pUIWindow;

				UIStoryboard* storyboard = [UIStoryboard storyboardWithName:@"Main" bundle:nil];
				window.rootViewController = [storyboard instantiateViewControllerWithIdentifier:@"ViewController"];
				MetalView* view = (MetalView*)window.rootViewController.view;

				UIScreen* screen = window.screen;
				if (screen == nil)
				{
					screen = window.windowScene.screen;
				}

				const float scale = (float)screen.nativeScale;
				const Math::Rectanglef adjustedArea =
					Math::Rectanglef{(Math::Vector2f)m_position, (Math::Vector2f)GetWindowClientAreaSize(m_pUIWindow)} / Math::Vector2f{scale};

				window.frame =
					CGRectMake(adjustedArea.GetPosition().x, adjustedArea.GetPosition().y, adjustedArea.GetSize().x, adjustedArea.GetSize().y);
				[view updateDrawableSize];

				CAMetalLayer* metalLayer = static_cast<CAMetalLayer*>(view.layer);
				pMetalLayerOut = (__bridge void*)metalLayer;
#else
				Assert(false, "todo");
				pMetalLayerOut = nullptr;
#endif
			}
		);
		return pMetalLayerOut;
	}
#endif

	void Window::GiveFocus()
	{
#if USE_SDL
		SDL_RaiseWindow(m_pSDLWindow);
#elif PLATFORM_WINDOWS
		SetForegroundWindow(reinterpret_cast<HWND>(m_pWindowHandle));
#endif
	}

	void Window::MakeVisible()
	{
#if USE_SDL
		SDL_ShowWindow(m_pSDLWindow);
#elif PLATFORM_WINDOWS
		ShowWindow(reinterpret_cast<HWND>(m_pWindowHandle), SW_SHOW);
#elif PLATFORM_APPLE_MACOS
		NSWindow* window = (__bridge NSWindow*)m_pNSWindow;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[window]()
			{
				[window makeKeyAndOrderFront:NSApp];
				[window makeKeyWindow];
			}
		);
		OnBecomeVisible();
#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		UIWindow* window = (__bridge UIWindow*)m_pUIWindow;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[window]()
			{
				[window makeKeyAndVisible];
			}
		);
		OnBecomeVisible();
#endif

		OnSwitchToForeground();
	}

	void Window::Hide()
	{
#if USE_SDL
		SDL_HideWindow(m_pSDLWindow);
#elif PLATFORM_WINDOWS
		ShowWindow(reinterpret_cast<HWND>(m_pWindowHandle), SW_HIDE);
#elif PLATFORM_APPLE
		OnBecomeHidden();
#endif

		// OnSwitchToBackground();
	}

	bool Window::IsVisible() const
	{
#if USE_SDL
		return (SDL_GetWindowFlags(m_pSDLWindow) & SDL_WINDOW_HIDDEN) == 0;
#elif PLATFORM_WINDOWS
		return IsWindowVisible(reinterpret_cast<HWND>(m_pWindowHandle));
#elif PLATFORM_APPLE_MACOS
		NSWindow* pWindow = (__bridge NSWindow*)m_pNSWindow;
		bool isVisible{false};
		ExecuteImmediatelyOnWindowThread(
			[pWindow, &isVisible]()
			{
				isVisible = pWindow.isVisible;
			}
		);
		return isVisible;
#elif PLATFORM_APPLE_IOS
		UIWindow* pWindow = (__bridge UIWindow*)m_pUIWindow;
		bool isVisible{false};
		ExecuteImmediatelyOnWindowThread(
			[pWindow, &isVisible]()
			{
				isVisible = !pWindow.isHidden;
			}
		);
		return isVisible;
#elif PLATFORM_APPLE_VISIONOS
		return false;
#elif PLATFORM_ANDROID || PLATFORM_EMSCRIPTEN
		return true;
#endif
	}

	void Window::OnSwitchToForeground()
	{
		m_stateFlags |= StateFlags::SwitchingToForeground;
		m_stateFlags &= ~StateFlags::InBackground;

		OnSwitchToForegroundInternal();

		if (IsVisible())
		{
			OnBecomeVisible();
		}
	}

	void Window::OnSwitchToBackground()
	{
		m_stateFlags &= ~StateFlags::SwitchingToForeground;
		m_stateFlags |= StateFlags::SwitchingToBackground;

		OnSwitchToBackgroundInternal();

		if (IsVisible())
		{
			OnBecomeHidden();
		}
	}

	void Window::OnSwitchToForegroundInternal()
	{
		if (!m_stateFlags.TryClearFlags(StateFlags::SwitchingToForeground))
		{
			Assert(false);
		}
	}

	void Window::OnSwitchToBackgroundInternal()
	{
		if (m_stateFlags.TryClearFlags(StateFlags::SwitchingToBackground))
		{
			m_stateFlags |= StateFlags::InBackground;
		}
		else
		{
			Assert(false);
		}
	}

	bool Window::HasKeyboardFocus() const
	{
		return m_hasKeyboardFocus;
	}

	bool Window::HasMouseFocus() const
	{
#if USE_SDL
		return m_pSDLWindow == SDL_GetMouseFocus();
#elif PLATFORM_WINDOWS
		POINT cursorPosition;
		GetCursorPos(&cursorPosition);
		const HWND windowHandle = WindowFromPoint(cursorPosition);
		return m_pWindowHandle == windowHandle;
#else
		return true;
#endif
	}

	void Window::HideCursor()
	{
		if (m_hideCursorCounter == 0)
		{
			SetCursorVisibility(false);
		}
		else
		{
			++m_hideCursorCounter;
		}
	}

	bool Window::ShowCursor()
	{
		if (m_hideCursorCounter > 0)
		{
			if (m_hideCursorCounter == 1)
			{
				SetCursorVisibility(true);
				return true;
			}
			--m_hideCursorCounter;
		}
		return false;
	}

	void Window::SetCursorVisibility(bool isVisible)
	{
		const bool needsUpdate = isVisible ? !!m_hideCursorCounter : !m_hideCursorCounter;
		m_hideCursorCounter = isVisible ? 0 : Math::Max(m_hideCursorCounter, uint16(1));
		if (needsUpdate)
		{
#if USE_SDL || PLATFORM_WINDOWS || PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
			Rendering::Window::QueueOnWindowThread(
				[isVisible]()
				{
#if USE_SDL
					SDL_ShowCursor(isVisible);
#elif PLATFORM_WINDOWS
					::ShowCursor(isVisible);
#elif PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
					if (isVisible)
					{
						CGDisplayShowCursor(kCGDirectMainDisplay);
					}
					else
					{
						CGDisplayHideCursor(kCGDirectMainDisplay);
					}
#endif
				}
			);
#endif
		}
	}

	bool Window::IsCursorVisible() const
	{
#if PLATFORM_WINDOWS
#if PROFILE_BUILD
		if (!HasMouseFocus())
		{
			return m_hideCursorCounter == 0;
		}
#endif
		bool isVisible = false;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[&isVisible]()
			{
				CURSORINFO ci = {sizeof(CURSORINFO)};
				isVisible = GetCursorInfo(&ci) && (ci.flags & CURSOR_SHOWING);
			}
		);
		return isVisible;
#elif USE_SDL
		bool isVisible = false;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[&isVisible]()
			{
				isVisible = SDL_ShowCursor(SDL_QUERY) == SDL_ENABLE;
			}
		);
		return isVisible;
#else
		return m_hideCursorCounter == 0;
#endif
	}

#if PLATFORM_WINDOWS
	static HCURSOR s_cursorHandle{::LoadCursor(nullptr, IDC_ARROW)};
#endif

	void Window::SetCursor([[maybe_unused]] const CursorType cursor)
	{
#if USE_SDL
		switch (cursor)
		{
			case CursorType::Arrow:
				SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_ARROW));
				break;
			case CursorType::ResizeHorizontal:
				SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZEWE));
				break;
			case CursorType::ResizeVertical:
				SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_SIZENS));
				break;
			case CursorType::NotPermitted:
				SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_NO));
				break;
			case CursorType::Hand:
				SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_HAND));
				break;
			case CursorType::TextEdit:
				SDL_SetCursor(SDL_CreateSystemCursor(SDL_SYSTEM_CURSOR_IBEAM));
				break;
		}
#elif PLATFORM_WINDOWS
		switch (cursor)
		{
			case CursorType::Arrow:
				s_cursorHandle = ::LoadCursor(nullptr, IDC_ARROW);
				::SetCursor(s_cursorHandle);
				break;
			case CursorType::ResizeHorizontal:
				s_cursorHandle = ::LoadCursor(nullptr, IDC_SIZEWE);
				::SetCursor(s_cursorHandle);
				break;
			case CursorType::ResizeVertical:
				s_cursorHandle = ::LoadCursor(nullptr, IDC_SIZENS);
				::SetCursor(s_cursorHandle);
				break;
			case CursorType::NotPermitted:
				s_cursorHandle = ::LoadCursor(nullptr, IDC_NO);
				::SetCursor(s_cursorHandle);
				break;
			case CursorType::Hand:
				s_cursorHandle = ::LoadCursor(nullptr, IDC_HAND);
				::SetCursor(s_cursorHandle);
				break;
			case CursorType::TextEdit:
				s_cursorHandle = ::LoadCursor(nullptr, IDC_IBEAM);
				::SetCursor(s_cursorHandle);
				break;
		}
#endif
	}

	void Window::ConstrainCursorToWindow()
	{
		if (m_constrainedCursorCounter == 0)
		{
			SetCursorConstrainedToWindow(true);
		}
		else
		{
			++m_constrainedCursorCounter;
		}
	}

	bool Window::UnconstrainCursorFromWindow()
	{
		if (m_constrainedCursorCounter > 0)
		{
			if (m_constrainedCursorCounter == 1)
			{
				SetCursorConstrainedToWindow(false);
				return true;
			}
			--m_constrainedCursorCounter;
		}
		return false;
	}

	void Window::SetCursorConstrainedToWindow(bool isConstrained)
	{
		const bool needsUpdate = isConstrained ? !m_constrainedCursorCounter : !!m_constrainedCursorCounter;
		m_constrainedCursorCounter = !isConstrained ? 0 : Math::Max(m_constrainedCursorCounter, uint16(1));
		if (needsUpdate)
		{
#if USE_SDL
			if (isConstrained)
			{
				SDL_SetWindowGrab(m_pSDLWindow, SDL_TRUE);
			}
			else
			{
				SDL_SetWindowGrab(m_pSDLWindow, SDL_FALSE);
			}
#elif PLATFORM_WINDOWS
			if (isConstrained)
			{
				RECT clientArea;
				GetClientRect(static_cast<HWND>(m_pWindowHandle), &clientArea);
				POINT location = {clientArea.left, clientArea.top};
				ClientToScreen(static_cast<HWND>(m_pWindowHandle), &location);
				POINT end = {clientArea.right, clientArea.bottom};
				ClientToScreen(static_cast<HWND>(m_pWindowHandle), &end);

				RECT clipArea{location.x, location.y, end.x, end.y};
				ClipCursor(&clipArea);
			}
			else
			{
				ClipCursor(nullptr);
			}
#elif PLATFORM_EMSCRIPTEN
			QueueOnWindowThread(
				[this, isConstrained]()
				{
					if (isConstrained)
					{
						emscripten_request_pointerlock(m_canvasSelector.GetZeroTerminated(), 0);
					}
					else
					{
						emscripten_exit_pointerlock();
					}
				}
			);
#endif
		}
	}

	void Window::LockCursorPosition()
	{
		if (m_lockCursorPositionCounter == 0)
		{
			SetCursorLockPosition(true);
		}
		else
		{
			++m_lockCursorPositionCounter;
		}
	}

	bool Window::UnlockCursorPosition()
	{
		if (m_lockCursorPositionCounter > 0)
		{
			if (m_lockCursorPositionCounter == 1)
			{
				SetCursorLockPosition(false);
				return true;
			}
			--m_lockCursorPositionCounter;
		}
		return false;
	}

	void Window::SetCursorLockPosition(bool isLocked)
	{
		const bool needsUpdate = isLocked ? !m_lockCursorPositionCounter : !!m_lockCursorPositionCounter;
		m_lockCursorPositionCounter = !isLocked ? 0 : Math::Max(m_lockCursorPositionCounter, uint16(1));
		if (needsUpdate)
		{
			if (isLocked)
			{
				SetCursorConstrainedToWindow(true);
				SetCursorVisibility(false);
			}
			else
			{
				SetCursorConstrainedToWindow(false);
				SetCursorVisibility(true);
			}
#if USE_SDL
			if (isLocked)
			{
				SDL_GetMouseState(&m_lockedCursorPosition.x, &m_lockedCursorPosition.y);
			}
			else
			{
				SDL_WarpMouseInWindow(m_pSDLWindow, m_lockedCursorPosition.x, m_lockedCursorPosition.y);
			}
#elif PLATFORM_WINDOWS
			if (isLocked)
			{
				POINT point = {m_lockedCursorPosition.x, m_lockedCursorPosition.y};
				GetCursorPos(&point);
				m_lockedCursorPosition.x = point.x;
				m_lockedCursorPosition.y = point.y;
			}
#elif PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
			if (isLocked)
			{
				/*NSPoint mouseLocation = [NSEvent mouseLocation];
				m_lockedCursorPosition = { mouseLocation.x, mouseLocation.y };*/
				// CGWarpMouseCursorPosition(CGPointMake(mouseLocation.x, mouseLocation.y));
				CGAssociateMouseAndMouseCursorPosition(false);
			}
			else
			{
				CGAssociateMouseAndMouseCursorPosition(true);
			}
#endif
		}
	}

	void Window::ShowVirtualKeyboard([[maybe_unused]] const EnumFlags<KeyboardTypeFlags> keyboardTypeFlags)
	{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		Rendering::Window::QueueOnWindowThread(
			[this, keyboardTypeFlags]()
			{
				UIWindow* window = (__bridge UIWindow*)m_pUIWindow;
				MetalView* view = (MetalView*)window.rootViewController.view;
				[view showOnScreenKeyboard:(uint32)keyboardTypeFlags.GetFlags()];
			}
		);
#elif PLATFORM_ANDROID
		using namespace Platform;
		EnumFlags<Android::InputTypeFlags> inputTypeFlags{Android::InputTypeFlags::TYPE_NULL};
		if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::URL))
		{
			inputTypeFlags = Android::InputTypeFlags::TYPE_CLASS_TEXT | Android::InputTypeFlags::TYPE_TEXT_VARIATION_URI;
		}
		else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::Email))
		{
			inputTypeFlags = Android::InputTypeFlags::TYPE_CLASS_TEXT | Android::InputTypeFlags::TYPE_TEXT_VARIATION_EMAIL_ADDRESS;
		}
		else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::WebSearch))
		{
			inputTypeFlags = Android::InputTypeFlags::TYPE_CLASS_TEXT;
		}
		else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::Number))
		{
			inputTypeFlags = Android::InputTypeFlags::TYPE_CLASS_NUMBER;
			if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::Signed))
			{
				inputTypeFlags |= Android::InputTypeFlags::TYPE_NUMBER_FLAG_SIGNED;
			}
			else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::Decimal))
			{
				inputTypeFlags |= Android::InputTypeFlags::TYPE_NUMBER_FLAG_DECIMAL;
			}
		}
		else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::PhoneNumber))
		{
			inputTypeFlags = Android::InputTypeFlags::TYPE_CLASS_PHONE;
		}
		else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::Name))
		{
			inputTypeFlags = Android::InputTypeFlags::TYPE_CLASS_TEXT | Android::InputTypeFlags::TYPE_TEXT_VARIATION_PERSON_NAME;
		}
		else if (keyboardTypeFlags.IsSet(Rendering::KeyboardTypeFlags::PIN))
		{
			inputTypeFlags = Android::InputTypeFlags::TYPE_CLASS_NUMBER | Android::InputTypeFlags::TYPE_NUMBER_VARIATION_PASSWORD;
		}
		else
		{
			inputTypeFlags = Android::InputTypeFlags::TYPE_CLASS_TEXT;
		}
		m_pGameActivityWindow->ShowVirtualKeyboard(inputTypeFlags);
#elif USE_SDL
		SDL_StartTextInput();
#elif PLATFORM_EMSCRIPTEN
		QueueOnWindowThreadOrExecuteImmediately(
			[]()
			{
				PUSH_CLANG_WARNINGS
				DISABLE_CLANG_WARNING("-Wdollar-in-identifier-extension")
				// clang-format off
				EM_ASM(
					{
						if ("virtualKeyboard" in navigator) {
						  navigator.virtualKeyboard.show();
						}
					}
				);
				// clang-format on
				POP_CLANG_WARNINGS
			}
		);
#endif
	}

	void Window::HideVirtualKeyboard()
	{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		Rendering::Window::QueueOnWindowThread(
			[this]()
			{
				UIWindow* window = (__bridge UIWindow*)m_pUIWindow;
				MetalView* view = (MetalView*)window.rootViewController.view;
				[view hideOnScreenKeyboard];
			}
		);
#elif PLATFORM_ANDROID
		m_pGameActivityWindow->HideVirtualKeyboard();
#elif USE_SDL
		SDL_StopTextInput();
#elif PLATFORM_EMSCRIPTEN
		QueueOnWindowThreadOrExecuteImmediately(
			[]()
			{
				PUSH_CLANG_WARNINGS
				DISABLE_CLANG_WARNING("-Wdollar-in-identifier-extension")
				// clang-format off
				EM_ASM(
					{
						if ("virtualKeyboard" in navigator) {
						  navigator.virtualKeyboard.hide();
						}
					}
				);
				// clang-format on
				POP_CLANG_WARNINGS
			}
		);
#endif
	}

	void Window::StartDragAndDrop(const ArrayView<const Widgets::DragAndDropData> draggedItems)
	{
#if PLATFORM_WINDOWS
		DataObject* pDataObject = new DataObject(draggedItems);

		DWORD effect = DROPEFFECT_COPY;
		[[maybe_unused]] const HRESULT result = DoDragDrop(pDataObject, &*m_pDropSource, effect, &effect);
#else
		UNUSED(draggedItems);
#endif
	}

	bool
	Window::OnStartDragItemsIntoWindow(const WindowCoordinate, [[maybe_unused]] const ArrayView<const Widgets::DragAndDropData> draggedItems)
	{
		return false;
	}

	void Window::OnCancelDragItemsIntoWindow()
	{
	}

	bool Window::OnDragItemsOverWindow(const WindowCoordinate, [[maybe_unused]] const ArrayView<const Widgets::DragAndDropData> draggedItems)
	{
		return false;
	}

	bool Window::OnDropItemsIntoWindow(const WindowCoordinate, [[maybe_unused]] const ArrayView<const Widgets::DragAndDropData> draggedItems)
	{
		return false;
	}

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS || PLATFORM_APPLE_MACCATALYST
}
@interface CopyAndPasteActivityItem : UIActivity
{
}
@end

@implementation CopyAndPasteActivityItem
- (UIActivityCategory)activityCategory
{
	return UIActivityCategoryShare;
}
- (UIActivityType)activityType
{
	return UIActivityTypeCopyToPasteboard;
}
- (NSString*)activityTitle
{
	return @"Copy Link";
}
- (UIImage*)activityImage
{
	return nil;
}

- (BOOL)canPerformWithActivityItems:(NSArray*)activityItems
{
	for (NSObject* activityItem : activityItems)
	{
		if (![activityItem isKindOfClass:[NSURL class]])
		{
			return false;
		}
	}
	return true;
}

NSString* copiedString = nil;

- (void)prepareWithActivityItems:(NSArray*)activityItems
{
	copiedString = [[NSString alloc] init];
	for (NSObject* activityItem : activityItems)
	{
		if ([activityItem isKindOfClass:[NSURL class]])
		{
			copiedString = [NSString stringWithFormat:@"%@%@", copiedString, [(NSURL*)activityItem absoluteString]];
		}
	}
}

- (void)performActivity
{
	UIPasteboard* pasteboard = [UIPasteboard generalPasteboard];
	if (copiedString != nil)
	{
		pasteboard.string = copiedString;
	}

	[super activityDidFinish:true];
}
@end

namespace ngine::Rendering
{
#endif

	void Window::ShareDocuments(
		[[maybe_unused]] const ArrayView<const DocumentData> documents, [[maybe_unused]] const Math::Rectanglei triggeringButttonContentArea
	)
	{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS || PLATFORM_APPLE_MACOS
		NSMutableArray* objectsToShare = [[NSMutableArray alloc] initWithCapacity:documents.GetSize()];

		for (const DocumentData& document : documents)
		{
			document.Visit(
				[&objectsToShare](const IO::Path& path)
				{
					NSString* pathString = [[NSString alloc] initWithBytes:path.GetZeroTerminated().GetData()
																													length:path.GetSize()
																												encoding:NSUTF8StringEncoding];
					NSURL* url = [NSURL fileURLWithPath:pathString isDirectory:path.IsDirectory() relativeToURL:nil];
					[objectsToShare addObject:url];
				},
				[&objectsToShare](const IO::URI& uri)
				{
					NSString* urlString = [[NSString alloc] initWithBytes:uri.GetZeroTerminated().GetData()
																												 length:uri.GetSize()
																											 encoding:NSUTF8StringEncoding];
					NSURL* url = [NSURL URLWithString:urlString];
					[objectsToShare addObject:url];
				},
				[&objectsToShare](const Asset::Guid assetGuid)
				{
					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					const IO::Path assetPath = assetManager.GetAssetPath(assetGuid);
					if (assetPath.HasElements())
					{
						NSString* pathString = [[NSString alloc] initWithBytes:assetPath.GetZeroTerminated().GetData()
																														length:assetPath.GetSize()
																													encoding:NSUTF8StringEncoding];
						NSURL* url = [NSURL fileURLWithPath:pathString isDirectory:assetPath.IsDirectory() relativeToURL:nil];
						[objectsToShare addObject:url];
					}
				},
				[&objectsToShare](const Asset::Identifier assetIdentifier)
				{
					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					const IO::Path assetPath = assetManager.GetAssetPath(assetIdentifier);
					if (assetPath.HasElements())
					{
						NSString* pathString = [[NSString alloc] initWithBytes:assetPath.GetZeroTerminated().GetData()
																														length:assetPath.GetSize()
																													encoding:NSUTF8StringEncoding];
						NSURL* url = [NSURL fileURLWithPath:pathString isDirectory:assetPath.IsDirectory() relativeToURL:nil];
						[objectsToShare addObject:url];
					}
				},
				[&objectsToShare](const Network::Address address)
				{
					String urlString;
					urlString.Format("{}://{}:{}", EngineInfo::AsciiName, address.GetIPAddress(), address.GetPort());

					NSString* addressString = [[NSString alloc] initWithBytes:urlString.GetZeroTerminated().GetData()
																														 length:urlString.GetSize()
																													 encoding:NSUTF8StringEncoding];
					NSURL* url = [NSURL URLWithString:addressString];
					[objectsToShare addObject:url];
				},
				[]()
				{
					ExpectUnreachable();
				}
			);
		}

		Rendering::Window::QueueOnWindowThread(
			[this, objectsToShare, triggeringButttonContentArea]()
			{
#if PLATFORM_APPLE_MACOS
				NSSharingServicePicker* sharingServicePicker = [[NSSharingServicePicker alloc] initWithItems:objectsToShare];

				const float scale = GetPhysicalDevicePixelRatio();

				const CGRect buttonSourceFrame{
					{(float)triggeringButttonContentArea.GetPosition().x / scale, (float)triggeringButttonContentArea.GetPosition().y / scale},
					{(float)triggeringButttonContentArea.GetSize().x / scale, (float)triggeringButttonContentArea.GetSize().y / scale}
				};

				NSWindow* pWindow = (__bridge NSWindow*)m_pNSWindow;
				NSViewController* viewController = [pWindow contentViewController];

				[sharingServicePicker showRelativeToRect:buttonSourceFrame ofView:viewController.view preferredEdge:NSRectEdgeMinX];

#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
				UIActivityViewController* activityViewController = [[UIActivityViewController alloc]
					initWithActivityItems:objectsToShare
					applicationActivities:@[ [[CopyAndPasteActivityItem alloc] init] ]];
				activityViewController.excludedActivityTypes = nil;

				UIWindow* pWindow = (__bridge UIWindow*)m_pUIWindow;
				UIViewController* viewController = [pWindow rootViewController];

				UIPopoverPresentationController* popoverPresentationController = activityViewController.popoverPresentationController;
				if (popoverPresentationController != nil)
				{
					const float scale = GetPhysicalDevicePixelRatio();

					const CGRect buttonSourceFrame{
						{(float)triggeringButttonContentArea.GetPosition().x / scale, (float)triggeringButttonContentArea.GetPosition().y / scale},
						{(float)triggeringButttonContentArea.GetSize().x / scale, (float)triggeringButttonContentArea.GetSize().y / scale}
					};

					popoverPresentationController.sourceView = viewController.view;
					popoverPresentationController.sourceRect = buttonSourceFrame;
				}

				[viewController presentViewController:activityViewController animated:YES completion:nil];
#endif
			}
		);
#elif PLATFORM_WINDOWS
		Assert(documents.GetSize() == 1, "We only support one document to share on Windows for now");

		Microsoft::WRL::ComPtr<IDataTransferManagerInterop> pDtmInterop;
		[[maybe_unused]] HRESULT result = RoGetActivationFactory(
			Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_ApplicationModel_DataTransfer_DataTransferManager).Get(),
			IID_PPV_ARGS(&pDtmInterop)
		);
		Assert(SUCCEEDED(result), "Failed to get data transfer manager");

		Microsoft::WRL::ComPtr<ABI::Windows::ApplicationModel::DataTransfer::IDataTransferManager> pDtm;
		pDtmInterop->GetForWindow(static_cast<HWND>(m_pWindowHandle), IID_PPV_ARGS(&pDtm));

		auto callback = Microsoft::WRL::Callback<ABI::Windows::Foundation::ITypedEventHandler<
			ABI::Windows::ApplicationModel::DataTransfer::DataTransferManager*,
			ABI::Windows::ApplicationModel::DataTransfer::DataRequestedEventArgs*>>(
			[document = documents[0]](auto&&, ABI::Windows::ApplicationModel::DataTransfer::IDataRequestedEventArgs* pEventArgs)
			{
				Microsoft::WRL::ComPtr<ABI::Windows::ApplicationModel::DataTransfer::IDataRequest> pRequest;
				pEventArgs->get_Request(&pRequest);

				Microsoft::WRL::ComPtr<ABI::Windows::ApplicationModel::DataTransfer::IDataPackage> pDataPackage;
				pRequest->get_Data(&pDataPackage);

				document.Visit(
					[pDataPackage](const IO::Path& path)
					{
						Microsoft::WRL::Wrappers::HString pathHString;
						pathHString.Set(NativeString(path.GetView().GetStringView()).GetData());
						pDataPackage->SetText(pathHString.Get());
					},
					[pDataPackage](const IO::URI& uri)
					{
						Microsoft::WRL::ComPtr<ABI::Windows::ApplicationModel::DataTransfer::IDataPackage2> pDataPackage2;
						pDataPackage.As(&pDataPackage2);

						Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IUriRuntimeClassFactory> pUriFactory;
						ABI::Windows::Foundation::GetActivationFactory(
							Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
							&pUriFactory
						);

						Microsoft::WRL::Wrappers::HString uriHString;
						uriHString.Set(NativeString(uri.GetView().GetStringView()).GetData());

						Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IUriRuntimeClass> pUriClass;
						pUriFactory->CreateUri(uriHString.Get(), &pUriClass);
						pDataPackage2->SetWebLink(pUriClass.Get());
					},
					[pDataPackage](const Asset::Guid assetGuid)
					{
						Asset::Manager& assetManager = System::Get<Asset::Manager>();
						const IO::Path assetPath = assetManager.GetAssetPath(assetGuid);
						if (assetPath.HasElements())
						{
							Microsoft::WRL::Wrappers::HString assetPathHString;
							assetPathHString.Set(NativeString(assetPath.GetView().GetStringView()).GetData());
							pDataPackage->SetText(assetPathHString.Get());
						}
					},
					[pDataPackage](const Asset::Identifier assetIdentifier)
					{
						Asset::Manager& assetManager = System::Get<Asset::Manager>();
						const IO::Path assetPath = assetManager.GetAssetPath(assetIdentifier);
						if (assetPath.HasElements())
						{
							Microsoft::WRL::Wrappers::HString assetPathHString;
							assetPathHString.Set(NativeString(assetPath.GetView().GetStringView()).GetData());
							pDataPackage->SetText(assetPathHString.Get());
						}
					},
					[pDataPackage](const Network::Address address)
					{
						IO::URI::StringType uriString;
						uriString.Format("{}://{}:{}", EngineInfo::AsciiName, address.GetIPAddress(), address.GetPort());

						Microsoft::WRL::ComPtr<ABI::Windows::ApplicationModel::DataTransfer::IDataPackage2> pDataPackage2;
						pDataPackage.As(&pDataPackage2);

						Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IUriRuntimeClassFactory> pUriFactory;
						ABI::Windows::Foundation::GetActivationFactory(
							Microsoft::WRL::Wrappers::HStringReference(RuntimeClass_Windows_Foundation_Uri).Get(),
							&pUriFactory
						);

						Microsoft::WRL::Wrappers::HString uriHString;
						uriHString.Set(NativeString(uriString.GetView()).GetData());

						Microsoft::WRL::ComPtr<ABI::Windows::Foundation::IUriRuntimeClass> pUriClass;
						pUriFactory->CreateUri(uriHString.Get(), &pUriClass);
						pDataPackage2->SetWebLink(pUriClass.Get());
					},
					[]()
					{
						ExpectUnreachable();
					}
				);

				Microsoft::WRL::ComPtr<ABI::Windows::ApplicationModel::DataTransfer::IDataPackagePropertySet> pProperties;
				pDataPackage->get_Properties(&pProperties);

				// Title is mandatory
				pProperties->put_Title(Microsoft::WRL::Wrappers::HStringReference(MAKE_NATIVE_LITERAL(" ")).Get());
				pProperties->put_ApplicationName(Microsoft::WRL::Wrappers::HStringReference(MAKE_NATIVE_LITERAL("Sceneri")).Get());

				return S_OK;
			}
		);

		EventRegistrationToken dataRequestedToken;
		pDtm->add_DataRequested(callback.Get(), &dataRequestedToken);
		pDtmInterop->ShowShareUIForWindow(static_cast<HWND>(m_pWindowHandle));
		// pDtm->remove_DataRequested(dataRequestedToken);
		// pDtm.Reset();
		// pDtmInterop.Reset();
#elif PLATFORM_WEB
		for (const DocumentData& document : documents)
		{
			document.Visit(
				[](const IO::Path& path)
				{
					PUSH_CLANG_WARNINGS
					DISABLE_CLANG_WARNING("-Wdollar-in-identifier-extension")
					// clang-format off
                    EM_ASM(
                        {
                            var filePath = UTF8ToString($0);
                            var createFile = (async (path) => {
                                var response = await fetch(path);
                                var data = await response.blob();
                                return new File([data], path, {
                                    type: type
                                });
                            });
                            createFile(filePath).then((file) => {
                                if(navigator.share && navigator.canShare({
                                  title: "Sceneri",
                                  text: "Share with friends!",
                                  files: [ file ]
                                })) {
                                    navigator.share({
                                      title: "Sceneri",
                                      text: "Share with friends!",
                                      files: [ file ]
                                    });
                                }
                            });
                        },
                        path.GetZeroTerminated().GetData()
                    );
					// clang-format on
					POP_CLANG_WARNINGS
				},
				[](const IO::URI& uri)
				{
					PUSH_CLANG_WARNINGS
					DISABLE_CLANG_WARNING("-Wdollar-in-identifier-extension")
					// clang-format off
                    EM_ASM(
                        {
                            var url = UTF8ToString($0);
                            if(navigator.share && navigator.canShare({
                                title: "Sceneri",
                                text: "Share with friends!",
                                url
                            })) {
                                navigator.share({
                                    title: "Sceneri",
                                    text: "Share with friends!",
                                    url
                                });
                            }
                        },
                        uri.GetZeroTerminated().GetData()
                    );
					// clang-format on
					POP_CLANG_WARNINGS
				},
				[](const Asset::Guid assetGuid)
				{
					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					const IO::Path assetPath = assetManager.GetAssetPath(assetGuid);
					if (assetPath.HasElements())
					{
						PUSH_CLANG_WARNINGS
						DISABLE_CLANG_WARNING("-Wdollar-in-identifier-extension")
						// clang-format off
                        EM_ASM(
                            {
                                var filePath = UTF8ToString($0);
                                var createFile = (async (path) => {
                                    var response = await fetch(path);
                                    var data = await response.blob();
                                    return new File([data], path, {
                                        type: type
                                    });
                                });
                                createFile(filePath).then((file) => {
                                    if(navigator.share && navigator.canShare({
                                        title: "Sceneri",
                                        text: "Share with friends!",
                                        files: [ file ]
                                    })) {
                                        navigator.share({
                                        title: "Sceneri",
                                        text: "Share with friends!",
                                        files: [ file ]
                                    });
                                    }
                                });
                            },
                            assetPath.GetZeroTerminated().GetData()
                        );
						// clang-format on
						POP_CLANG_WARNINGS
					}
				},
				[](const Asset::Identifier assetIdentifier)
				{
					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					const IO::Path assetPath = assetManager.GetAssetPath(assetIdentifier);
					if (assetPath.HasElements())
					{
						PUSH_CLANG_WARNINGS
						DISABLE_CLANG_WARNING("-Wdollar-in-identifier-extension")
						// clang-format off
                        EM_ASM(
                            {
                                var filePath = UTF8ToString($0);
                                var createFile = (async (path) => {
                                    var response = await fetch(path);
                                    var data = await response.blob();
                                    return new File([data], path, {
                                        type: type
                                    });
                                });
                                createFile(filePath).then((file) => {
                                    if(navigator.share && navigator.canShare({
                                        title: "Sceneri",
                                        text: "Share with friends!",
                                        files: [ file ]
                                    })) {
                                        navigator.share({
                                        title: "Sceneri",
                                        text: "Share with friends!",
                                        files: [ file ]
                                    });
                                    }
                                });
                            },
                            assetPath.GetZeroTerminated().GetData()
                        );
						// clang-format on
						POP_CLANG_WARNINGS
					}
				},
				[](const Network::Address address)
				{
					IO::URI::StringType uriString;
					uriString.Format("{}://{}:{}", EngineInfo::AsciiName, address.GetIPAddress(), address.GetPort());

					PUSH_CLANG_WARNINGS
					DISABLE_CLANG_WARNING("-Wdollar-in-identifier-extension")
					// clang-format off
                    EM_ASM(
                        {
                            var url = UTF8ToString($0);
                            if(navigator.share && navigator.canShare({
                                title: "Sceneri",
                                text: "Invite friends to play together!",
                                url
                            })) {
                                navigator.share({
                                    title: "Sceneri",
                                    text: "Invite friends to play together!",
                                    url
                                });
                            }
                        },
                        uriString.GetZeroTerminated().GetData()
                    );
					// clang-format on
					POP_CLANG_WARNINGS
				},
				[]()
				{
					ExpectUnreachable();
				}
			);
		}
#else
		Assert(false, "Not implemented for this platform!");
#endif
	}

#if PLATFORM_WINDOWS
	template<typename Type>
	struct COMWrapper
	{
		COMWrapper() = default;
		COMWrapper(COMWrapper&& other)
			: m_pPointer(other.m_pPointer)
		{
			other.m_pPointer = nullptr;
		}
		COMWrapper& operator=(COMWrapper&& other)
		{
			m_pPointer = other.m_pPointer;
			other.m_pPointer = nullptr;
			return *this;
		}
		COMWrapper(const COMWrapper&) = delete;
		COMWrapper& operator=(const COMWrapper&) = delete;
		~COMWrapper()
		{
			if (m_pPointer != nullptr)
			{
				m_pPointer->Release();
			}
		}

		[[nodiscard]] Type* operator->() const
		{
			return m_pPointer;
		}

		Type* m_pPointer{nullptr};
	};
#endif

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
}

@interface DocumentPickerDelegate : NSObject <UIDocumentPickerDelegate>
{
@public
	ngine::Rendering::Window::SelectFileCallback m_callback;
}
@end

@implementation DocumentPickerDelegate
- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)nsUrls
{
	using namespace ngine;
	Vector<IO::Path> paths(Memory::Reserve, (uint32)[nsUrls count]);
	for (NSURL* nsUrl : nsUrls)
	{
		if ([nsUrl isFileURL])
		{
			[[maybe_unused]] const bool isSecurityScoped = [nsUrl startAccessingSecurityScopedResource];
			const char* filePath = nsUrl.fileSystemRepresentation;

			paths.EmplaceBack(IO::Path::StringType(filePath, (IO::Path::SizeType)strlen(filePath)));

#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
			if (isSecurityScoped)
			{
				// Save in user defaults so we can access this resource in future sessions
				NSData* bookmarkData = [nsUrl bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
															 includingResourceValuesForKeys:nil
																								relativeToURL:nil
																												error:nil];
				if (bookmarkData != nil)
				{
					NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
					NSArray<NSData*>* securityBookmarks = (NSArray<NSData*>*)[userDefaults objectForKey:@"securityBookmarks"];
					if (securityBookmarks == nil)
					{
						securityBookmarks = [[NSArray<NSData*> alloc] init];
					}
					NSMutableArray<NSData*>* mutableSecurityBookmarks = [securityBookmarks mutableCopy];
					[mutableSecurityBookmarks addObject:bookmarkData];
					[userDefaults setObject:mutableSecurityBookmarks forKey:@"securityBookmarks"];
				}
			}
#endif
		}
		else
		{
			Assert(false);
		}
	}
	m_callback(paths);
}

- (void)documentPickerWasCancelled:(UIDocumentPickerViewController*)controller
{
	m_callback({});
}
@end

namespace ngine::Rendering
{
#endif

#if PLATFORM_EMSCRIPTEN

	struct ImportData
	{
		Window& window;
		Window::SelectFileCallback callback;
		IO::Path tempDirectory;
		bool failedAny{false};
		size_t completedFileCount{0};
	};

	extern "C"
	{
		EMSCRIPTEN_KEEPALIVE inline void
		ImportFile(const char* pathPtr, const ByteType* fileData, const size_t fileSize, const size_t fileCount, void* pUserData)
		{
			[[maybe_unused]] ImportData* pData = reinterpret_cast<ImportData*>(pUserData);
			const ConstStringView pathString{pathPtr, (uint32)strlen(pathPtr)};

			IO::Path targetFilePath = IO::Path::Combine(pData->tempDirectory, pathString);
			if (!IO::Path(targetFilePath.GetParentPath()).Exists())
			{
				IO::Path(targetFilePath.GetParentPath()).CreateDirectories();
			}

			{
				IO::File targetFile(targetFilePath, IO::AccessModeFlags::WriteBinary);
				Assert(targetFile.IsValid());
				if (LIKELY(targetFile.IsValid()))
				{
					const bool wasWritten = targetFile.Write(ConstByteView{fileData, fileSize}) == fileSize;
					Assert(wasWritten);
					if (UNLIKELY(!wasWritten))
					{
						pData->failedAny = true;
					}
				}
			}

			if (++pData->completedFileCount == fileCount)
			{
				const IO::PathView firstPath = targetFilePath.GetRelativeToParent(pData->tempDirectory).GetFirstPath();

				Threading::JobManager& jobManager = System::Get<Threading::JobManager>();
				jobManager.QueueCallback(
					[pData, firstPath = IO::Path(firstPath)](Threading::JobRunnerThread&)
					{
						if (!pData->failedAny)
						{
							pData->callback(Array<const IO::Path, 1>{IO::Path::Combine(pData->tempDirectory, firstPath)}.GetDynamicView());
						}
						else
						{
							pData->callback({});
						}
						delete pData;
					},
					Threading::JobPriority::UserInterfaceAction
				);
			}
		}
	}

	PUSH_CLANG_WARNINGS
	DISABLE_CLANG_WARNING("-Winvalid-pp-token")
	// Javascript function that opens a file selection dialog, imports the selected files to the filesystem and invokes the callback
	EM_JS(void, SelectAndImportLocalFiles, (const char* acceptedTypes, const bool allowDirectories, void* callbackUserData), {
		// clang-format off
		globalThis["onFileSelected"] = function(e)
		{
			const files = e.target.files;
			const fileCount = files.length;
			if (fileCount > 0)
			{
				for (const file of files)
				{
					const file_reader = new FileReader();
					file_reader.onload = (event) =>
					{
						const uint8Arr = new Uint8Array(event.target.result);
						let ptrLen = uint8Arr.length;
						const data_ptr = Module["_malloc"](ptrLen);
						const data_on_heap = new Uint8Array(Module["HEAPU8"].buffer, data_ptr, uint8Arr.length);
						data_on_heap.set(uint8Arr);

						let relativePath = file.webkitRelativePath;
						Module["ccall"](
							'ImportFile',
							'void',
							[ 'string', 'number', 'number', 'number', 'number' ],
							[ relativePath, data_on_heap.byteOffset, ptrLen, fileCount, callbackUserData ]
						);
						Module["_free"](data_ptr);
					};
					file_reader.filename = file.name;
					file_reader.mime_type = file.type;
					file_reader.readAsArrayBuffer(file);
				}
			}
		};

		var file_selector = document.createElement('input');
		file_selector.setAttribute('type', 'file');
		file_selector.setAttribute('onchange', 'globalThis["onFileSelected"](event)');
		file_selector.setAttribute('accept', UTF8ToString(acceptedTypes));
		if (allowDirectories)
		{
			file_selector.setAttribute('webkitdirectory', '');
			file_selector.setAttribute('directory', '');
			file_selector.setAttribute('multiple', '');
		}
		file_selector.click();
		// clang-format on
	});

	EM_JS(bool, JSExportDirectory, (const char* directoryName, const char* sourcePath), {
		// clang-format off
		const directoryNameString = UTF8ToString(directoryName);
		const sourcePathString = UTF8ToString(sourcePath);
		let getDirectoryHandleFromPath = async function(root, path)
		{
			const rootPrefix = "/opfs/";
			if (path.startsWith(rootPrefix))
			{
				path = path.slice(rootPrefix.length);
			}
			let parts = path.split("/");
			let currentDir = root;
			for (let i = 0; i < parts.length; i++)
			{
				const isFinalPart = i === parts.length - 1;
				if (isFinalPart)
				{
					return await currentDir.getDirectoryHandle(parts[i]);
				}
				else
				{
					currentDir = await currentDir.getDirectoryHandle(parts[i], {create : false});
				}
			}
		};
		let copyFile = async function(sourceFileHandle, destinationFileHandle)
		{
			const sourceFile = await sourceFileHandle.getFile();
			const writableStream = await destinationFileHandle.createWritable();
			await writableStream.write(await sourceFile.arrayBuffer());
			await writableStream.close();
		};
		let copyDirectoryContents = async function(sourceDirectoryHandle, destinationDirectoryHandle)
		{
			for await(const entry of sourceDirectoryHandle.values())
			{
				if (entry.kind === 'file')
				{
					const sourceFileHandle = await sourceDirectoryHandle.getFileHandle(entry.name);
					const destinationFileHandle = await destinationDirectoryHandle.getFileHandle(entry.name, {create : true});
					await copyFile(sourceFileHandle, destinationFileHandle);
				}
				else if (entry.kind === 'directory')
				{
					const sourceSubDirectoryHandle = await sourceDirectoryHandle.getDirectoryHandle(entry.name);
					const destinationSubDirectoryHandle = await destinationDirectoryHandle.getDirectoryHandle(entry.name, {create : true});
					await copyDirectoryContents(sourceSubDirectoryHandle, destinationSubDirectoryHandle);
				}
			}
		};
		if ('showDirectoryPicker' in window)
		{
			window.showDirectoryPicker().then((directoryHandle) => {
				navigator.storage.getDirectory().then((root) => {
					getDirectoryHandleFromPath(root, sourcePathString).then((sourceDirectoryHandle) => {
						directoryHandle.getDirectoryHandle(directoryNameString, {create : true}).then(targetHandle => {
							copyDirectoryContents(sourceDirectoryHandle, targetHandle).then(() => {});
						});
					});
				});
			});
			return true;
		}
		return false;
		// clang-format on
	});

	EM_JS(bool, JSExportFile, (const char* proposedNamePtr, const char* acceptedTypesPtr, const char* sourcePath), {
		// clang-format off
		const sourcePathString = UTF8ToString(sourcePath);
		let getFileHandleFromPath = async function(root, path)
		{
			const rootPrefix = "/opfs/";
			if (path.startsWith(rootPrefix))
			{
				path = path.slice(rootPrefix.length);
			}
			let parts = path.split("/");
			let currentDir = root;
			for (let i = 0; i < parts.length; i++)
			{
				const isFinalPart = i === parts.length - 1;
				if (isFinalPart)
				{
					return await currentDir.getFileHandle(parts[i]);
				}
				else
				{
					currentDir = await currentDir.getDirectoryHandle(parts[i], {create : false});
				}
			}
		};

		if ('showSaveFilePicker' in window)
		{
			const acceptedTypes = UTF8ToString(acceptedTypesPtr);
			const elements = acceptedTypes.split(',');
			const allowedFileTypes = [];
			for (let i = 0; i < elements.length; i += 3)
			{
				allowedFileTypes.push({description : elements[i], mime : elements[i + 1], extensions : elements[i + 2].split(';')});
			}

			const types =
				allowedFileTypes.map(fileType => ({description : fileType.description, accept : {[fileType.mime] : fileType.extensions}}));

			const proposedName = UTF8ToString(proposedNamePtr);
			const options = {suggestedName : proposedName, types : types};

			window.showSaveFilePicker(options).then((fileHandle) => {
				navigator.storage.getDirectory().then((root) => {
					getFileHandleFromPath(root, sourcePathString).then((sourceFileHandle) => {
						sourceFileHandle.getFile().then((file) => {
							fileHandle.createWritable().then((writableStream) => {writableStream.write(file).then(() => {
																																		writableStream.close().then(() => {});
																																	})});
						});
					});
				});
			});
			return true;
		}
		return false;
		// clang-format on
	});
	POP_CLANG_WARNINGS
#endif

	void
	Window::SelectFiles(const EnumFlags<SelectFileFlag> flags, const ArrayView<const FileType> allowedTypes, SelectFileCallback&& callback)
	{
#if PLATFORM_WINDOWS
		COMWrapper<IFileOpenDialog> fileOpenDialog;
		HRESULT hr =
			CoCreateInstance(CLSID_FileOpenDialog, NULL, CLSCTX_ALL, IID_IFileOpenDialog, reinterpret_cast<void**>(&fileOpenDialog.m_pPointer));
		if (FAILED(hr))
		{
			callback({});
			return;
		}

		struct EventHandler final : public IFileDialogEvents
		{
			EventHandler(const ArrayView<const FileType> allowedTypes)
				: m_allowedExtensions(Memory::Reserve, allowedTypes.GetSize())
			{
				for (const FileType& allowedType : allowedTypes)
				{
					m_allowedExtensions.EmplaceBack(allowedType.extension);
				}
			}
			virtual ~EventHandler() = default;

			virtual HRESULT QueryInterface(REFIID riid, void** ppv) override
			{
				static const QITAB qit[] = {
					QITABENT(EventHandler, IFileDialogEvents),
					{0},
				};
				return QISearch(this, qit, riid, ppv);
			}

			virtual ULONG AddRef() override
			{
				return InterlockedIncrement(&m_referenceCount);
			}

			virtual ULONG Release() override
			{
				long cRef = InterlockedDecrement(&m_referenceCount);
				if (!cRef)
					delete this;
				return cRef;
			}

			virtual HRESULT OnFileOk(IFileDialog* pFileDialog) override
			{
				if (m_allowedExtensions.HasElements())
				{
					PWSTR pszFilePath;
					pFileDialog->GetFileName(&pszFilePath);

					IO::PathView filePath{IO::PathView::ConstStringViewType{pszFilePath, (IO::Path::StringType::SizeType)lstrlenW(pszFilePath)}};
					if (m_allowedExtensions.Contains(filePath.GetRightMostExtension()))
					{
						CoTaskMemFree(pszFilePath);
						return S_OK;
					}
					else
					{
						CoTaskMemFree(pszFilePath);
						return S_FALSE;
					}
				}
				else
				{
					return S_OK;
				}
			}
			virtual HRESULT OnFolderChanging(IFileDialog*, IShellItem*) override
			{
				return S_OK;
			};
			virtual HRESULT OnFolderChange(IFileDialog*) override
			{
				return S_OK;
			};
			virtual HRESULT OnSelectionChange(IFileDialog*) override
			{
				return S_OK;
			};
			virtual HRESULT OnShareViolation(IFileDialog*, IShellItem*, FDE_SHAREVIOLATION_RESPONSE*) override
			{
				return S_OK;
			};
			virtual HRESULT OnTypeChange(IFileDialog*) override
			{
				return S_OK;
			}
			virtual HRESULT OnOverwrite(IFileDialog*, IShellItem*, FDE_OVERWRITE_RESPONSE*) override
			{
				return S_OK;
			};
		protected:
			long m_referenceCount{1};
			Vector<IO::Path> m_allowedExtensions;
		};

		EventHandler* pEventHandler = new EventHandler(allowedTypes);

		DWORD dwCookie;
		fileOpenDialog->Advise(pEventHandler, &dwCookie);

		DWORD dwFlags;
		hr = fileOpenDialog->GetOptions(&dwFlags);
		if (FAILED(hr))
		{
			callback({});
			return;
		}

		// In this case, get shell items only for file system items.
		hr = fileOpenDialog->SetOptions(dwFlags | FOS_FORCEFILESYSTEM | (FOS_PICKFOLDERS * flags.IsSet(SelectFileFlag::Directories)));
		if (FAILED(hr))
		{
			callback({});
			return;
		}

		InlineVector<NativeString, 1> allowedFilePatterns(Memory::Reserve, allowedTypes.GetSize() + 1);
		InlineVector<COMDLG_FILTERSPEC, 1> allowedFileTypes(Memory::Reserve, allowedTypes.GetSize() + 1);

		// Start by emplacing a filter that selects all supported assets
		{
			NativeString pattern = NativeString::Merge(MAKE_NATIVE_LITERAL("*"), allowedTypes[0].extension.GetStringView());
			for (const FileType& allowedType : (allowedTypes + 1))
			{
				pattern += NativeString::Merge(MAKE_NATIVE_LITERAL(";*"), allowedType.extension.GetStringView());
			}

			NativeString& allowedFilePattern = allowedFilePatterns.EmplaceBack(pattern);

			allowedFileTypes.EmplaceBack(COMDLG_FILTERSPEC{
				MAKE_NATIVE_LITERAL("Any supported file type"),
				allowedFilePattern.GetZeroTerminated(),
			});
		}

		for (const FileType& allowedType : allowedTypes)
		{
			NativeString pattern = NativeString::Merge(MAKE_NATIVE_LITERAL("*"), allowedType.extension.GetStringView());
			NativeString& allowedFilePattern = allowedFilePatterns.EmplaceBack(pattern);

			allowedFileTypes.EmplaceBack(COMDLG_FILTERSPEC{
				allowedType.title,
				allowedFilePattern.GetZeroTerminated(),
			});
		};

		if (flags.IsSet(SelectFileFlag::Files))
		{
			// Set the file types to display only.
			// Notice that this is a 1-based array.
			hr = fileOpenDialog->SetFileTypes(allowedFileTypes.GetSize(), allowedFileTypes.GetData());
			if (FAILED(hr))
			{
				callback({});
				return;
			}

			// Set the selected file type index to Word Docs for this example.
			hr = fileOpenDialog->SetFileTypeIndex(0);
			if (FAILED(hr))
			{
				callback({});
				return;
			}
		}

		std::thread thread(
			[this, fileOpenDialog = Move(fileOpenDialog), callback = Forward<SelectFileCallback>(callback), dwCookie]()
			{
				HRESULT hr = fileOpenDialog->Show(static_cast<HWND>(m_pWindowHandle));
				if (FAILED(hr))
				{
					callback({});
					return;
				}

				fileOpenDialog->Unadvise(dwCookie);

				COMWrapper<IShellItem> shellItem;
				hr = fileOpenDialog->GetResult(&shellItem.m_pPointer);
				if (FAILED(hr))
				{
					callback({});
					return;
				}

				PWSTR pszFilePath;
				hr = shellItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

				// Display the file name to the user.
				if (SUCCEEDED(hr))
				{
					IO::Path filePath{IO::Path::StringType{pszFilePath, (IO::Path::StringType::SizeType)lstrlenW(pszFilePath)}};
					callback(Array<IO::Path, 1>{filePath});
					CoTaskMemFree(pszFilePath);
				}
				else
				{
					callback({});
				}
			}
		);
		thread.detach();
#elif PLATFORM_APPLE_MACOS
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, flags, allowedTypes, callback = Forward<SelectFileCallback>(callback)]() mutable
			{
				NSOpenPanel* panel = [NSOpenPanel openPanel];

				if (flags.IsSet(SelectFileFlag::Directories))
				{
					[panel setCanChooseFiles:NO];
					[panel setCanChooseDirectories:YES];
				}
				else if (flags.IsSet(SelectFileFlag::Files))
				{
					[panel setCanChooseDirectories:NO];
					[panel setCanChooseFiles:YES];
				}
				[panel setTreatsFilePackagesAsDirectories:true];

				// TODO: Expose this option to all platforms as a flag
				[panel setAllowsMultipleSelection:NO];

				if (allowedTypes.HasElements())
				{
					NSMutableArray<UTType*>* allowedUTTypes = [[NSMutableArray<UTType*> alloc] initWithCapacity:allowedTypes.GetSize()];
					for (const FileType allowedFileType : allowedTypes)
					{
						NSString* extension = [[NSString alloc] initWithBytes:allowedFileType.extension.GetData() + 1
																													 length:allowedFileType.extension.GetSize() - 1
																												 encoding:NSUTF8StringEncoding];
						UTType* type;
						if (flags.IsSet(SelectFileFlag::Files))
						{
							type = [UTType typeWithTag:extension tagClass:UTTagClassFilenameExtension conformingToType:UTTypeData];
						}
						else
						{
							type = [UTType typeWithTag:extension tagClass:UTTagClassFilenameExtension conformingToType:UTTypeDirectory];
						}
						Assert(type != nil);
						if (type != nil)
						{
							[allowedUTTypes addObject:type];
						}
					}
					[panel setAllowedContentTypes:allowedUTTypes];
				}

				SharedPtr<SelectFileCallback> pCallback = SharedPtr<SelectFileCallback>::Make(Forward<SelectFileCallback>(callback));

				NSWindow* window = (__bridge NSWindow*)m_pNSWindow;
				[panel beginSheetModalForWindow:window
											completionHandler:^(NSInteger result) {
												if (result == NSModalResponseOK)
												{
													NSArray<NSURL*>* nsUrls = [panel URLs];
													Vector<IO::Path> paths(Memory::Reserve, (uint32)[nsUrls count]);
													for (NSURL* nsUrl : nsUrls)
													{
														if ([nsUrl isFileURL])
														{
															[[maybe_unused]] const bool isSecurityScoped = [nsUrl startAccessingSecurityScopedResource];
															const char* filePath = nsUrl.fileSystemRepresentation;

															paths.EmplaceBack(IO::Path::StringType(filePath, (IO::Path::SizeType)strlen(filePath)));

#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
															if (isSecurityScoped)
															{
																// Save in user defaults so we can access this resource in future sessions
																NSData* bookmarkData = [nsUrl bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
																											 includingResourceValuesForKeys:nil
																																				relativeToURL:nil
																																								error:nil];
																if (bookmarkData != nil)
																{
																	NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
																	NSArray<NSData*>* securityBookmarks = (NSArray<NSData*>*)[userDefaults objectForKey:@"securityBookmarks"];
																	if (securityBookmarks == nil)
																	{
																		securityBookmarks = [[NSArray<NSData*> alloc] init];
																	}
																	NSMutableArray<NSData*>* mutableSecurityBookmarks = [securityBookmarks mutableCopy];
																	[mutableSecurityBookmarks addObject:bookmarkData];
																	[userDefaults setObject:mutableSecurityBookmarks forKey:@"securityBookmarks"];
																}
															}
#endif
														}
														else
														{
															Assert(false);
														}
													}
													const ArrayView<const IO::Path> pathsView{paths};
													(*pCallback)(pathsView);
												}
												else
												{
													(*pCallback)({});
												}
											}];
			}
		);
#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, flags, allowedTypes, callback = Forward<SelectFileCallback>(callback)]() mutable
			{
				UIWindow* pWindow = (__bridge UIWindow*)m_pUIWindow;
				UIViewController* viewController = [pWindow rootViewController];

				UIDocumentPickerViewController* documentPicker;

				if (allowedTypes.HasElements())
				{
					NSMutableArray<UTType*>* allowedUTTypes = [[NSMutableArray<UTType*> alloc] initWithCapacity:allowedTypes.GetSize()];
					for (const FileType allowedFileType : allowedTypes)
					{
						NSString* extension = [[NSString alloc] initWithBytes:allowedFileType.extension.GetData() + 1
																													 length:allowedFileType.extension.GetSize() - 1
																												 encoding:NSUTF8StringEncoding];
						UTType* type;
						if (flags.IsSet(SelectFileFlag::Files))
						{
							type = [UTType typeWithTag:extension tagClass:UTTagClassFilenameExtension conformingToType:UTTypeData];
						}
						else
						{
							type = [UTType typeWithTag:extension tagClass:UTTagClassFilenameExtension conformingToType:UTTypeDirectory];
						}
						Assert(type != nil);
						if (type != nil)
						{
							[allowedUTTypes addObject:type];
						}
					}
					documentPicker = [[UIDocumentPickerViewController alloc] initForOpeningContentTypes:allowedUTTypes];
				}
				else
				{

					documentPicker = [[UIDocumentPickerViewController alloc] init];
				}

				// TODO: Expose this option to all platforms as a flag
				[documentPicker setAllowsMultipleSelection:NO];

				DocumentPickerDelegate* delegate = [[DocumentPickerDelegate alloc] init];
				delegate->m_callback = [callback = Forward<SelectFileCallback>(callback), delegate](const ArrayView<const IO::Path> selectedFiles)
				{
					callback(selectedFiles);
					UNUSED(delegate);
				};
				documentPicker.delegate = delegate;

				UNUSED(flags);

				[viewController presentViewController:documentPicker animated:YES completion:nil];
			}
		);
#elif PLATFORM_EMSCRIPTEN
		String allowedTypesString;
		if (allowedTypes.HasElements())
		{
			allowedTypesString = String{allowedTypes[0].extension.GetStringView()};
			for (const FileType& allowedType : (allowedTypes + 1))
			{
				allowedTypesString += allowedType.extension.GetStringView();
			}
		}

		QueueOnWindowThread(
			[this, flags, callback = Forward<SelectFileCallback>(callback), allowedTypesString = Move(allowedTypesString)]() mutable
			{
				ImportData* pData = new ImportData{
					*this,
					Move(callback),
					IO::Path::Combine(IO::Path::GetTemporaryDirectory(), Guid::Generate().ToString().GetView())
				};

				SelectAndImportLocalFiles(allowedTypesString.GetZeroTerminated(), flags.IsSet(SelectFileFlag::Directories), pData);
			}
		);
#else
		UNUSED(flags);
		UNUSED(allowedTypes);
		Assert(false, "Not implemented for platform");
		callback({});
#endif
	}

	void Window::ExportFile(const IO::Path& filePath, [[maybe_unused]] const FileType& fileType)
	{
#if PLATFORM_WINDOWS
		if (filePath.IsDirectory())
		{
			return SelectFiles(
				SelectFileFlag::Directories,
				Array<const FileType, 1>{fileType},
				[sourceFilePath = filePath](const ArrayView<const IO::Path> files)
				{
					Assert(files.GetSize() <= 1);

					if (files.GetSize() == 1)
					{
						[[maybe_unused]] const bool wasCopied = sourceFilePath.CopyDirectoryTo(files[0]);
						Assert(wasCopied);
					}
				}
			);
		}

		COMWrapper<IFileSaveDialog> fileSaveDialog;
		HRESULT hr =
			CoCreateInstance(CLSID_FileSaveDialog, NULL, CLSCTX_ALL, IID_IFileSaveDialog, reinterpret_cast<void**>(&fileSaveDialog.m_pPointer));
		if (FAILED(hr))
		{
			return;
		}

		DWORD dwFlags;
		hr = fileSaveDialog->GetOptions(&dwFlags);
		if (FAILED(hr))
		{
			return;
		}

		// In this case, get shell items only for file system items.
		hr = fileSaveDialog->SetOptions(dwFlags | FOS_FORCEFILESYSTEM);
		if (FAILED(hr))
		{
			return;
		}

		Array<NativeString, 1> allowedFilePatterns{NativeString::Merge(MAKE_NATIVE_LITERAL("*"), fileType.extension.GetStringView())};
		Array<COMDLG_FILTERSPEC, 1> allowedFileTypes{COMDLG_FILTERSPEC{
			fileType.title,
			allowedFilePatterns[0].GetZeroTerminated(),
		}};

		// Set the file types to display only.
		// Notice that this is a 1-based array.
		hr = fileSaveDialog->SetFileTypes(allowedFileTypes.GetSize(), allowedFileTypes.GetData());
		if (FAILED(hr))
		{
			return;
		}

		// Set the selected file type index to Word Docs for this example.
		hr = fileSaveDialog->SetFileTypeIndex(0);
		if (FAILED(hr))
		{
			return;
		}

		std::thread thread(
			[this, sourceFilePath = filePath, fileSaveDialog = Move(fileSaveDialog)]()
			{
				HRESULT hr = fileSaveDialog->Show(static_cast<HWND>(m_pWindowHandle));
				if (FAILED(hr))
				{
					return;
				}

				COMWrapper<IShellItem> shellItem;
				hr = fileSaveDialog->GetResult(&shellItem.m_pPointer);
				if (FAILED(hr))
				{
					return;
				}

				PWSTR pszFilePath;
				hr = shellItem->GetDisplayName(SIGDN_FILESYSPATH, &pszFilePath);

				// Display the file name to the user.
				if (SUCCEEDED(hr))
				{
					IO::Path filePath{IO::Path::StringType{pszFilePath, (IO::Path::StringType::SizeType)lstrlenW(pszFilePath)}};
					[[maybe_unused]] const bool wasCopied = sourceFilePath.CopyFileTo(filePath);
					Assert(wasCopied);

					CoTaskMemFree(pszFilePath);
				}
			}
		);
		thread.detach();
#elif PLATFORM_APPLE_MACOS
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, sourceFilePath = filePath]() mutable
			{
				NSSavePanel* panel = [NSSavePanel savePanel];

				NSMutableArray<UTType*>* allowedUTTypes = [[NSMutableArray<UTType*> alloc] initWithCapacity:1];
				{
					const IO::PathView fileExtension = sourceFilePath.GetAllExtensions();
					NSString* extension = [[NSString alloc] initWithBytes:fileExtension.GetData() + 1
																												 length:fileExtension.GetSize() - 1
																											 encoding:NSUTF8StringEncoding];
					UTType* type;
					if (sourceFilePath.IsFile())
					{
						type = [UTType typeWithTag:extension tagClass:UTTagClassFilenameExtension conformingToType:UTTypeData];
					}
					else
					{
						type = [UTType typeWithTag:extension tagClass:UTTagClassFilenameExtension conformingToType:UTTypeDirectory];
					}
					Assert(type != nil);
					if (type != nil)
					{
						[allowedUTTypes addObject:type];
					}
				}
				[panel setAllowedContentTypes:allowedUTTypes];

				NSWindow* window = (__bridge NSWindow*)m_pNSWindow;
				[panel beginSheetModalForWindow:window
											completionHandler:^(NSInteger result) {
												if (result == NSModalResponseOK)
												{
													NSURL* nsUrl = [panel URL];

													if ([nsUrl isFileURL])
													{
														[[maybe_unused]] const bool isSecurityScoped = [nsUrl startAccessingSecurityScopedResource];
														const char* pickedFilePath = nsUrl.fileSystemRepresentation;

														IO::Path path(IO::Path::StringType(pickedFilePath, (IO::Path::SizeType)strlen(pickedFilePath)));

#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
														if (isSecurityScoped)
														{
															// Save in user defaults so we can access this resource in future sessions
															NSData* bookmarkData = [nsUrl bookmarkDataWithOptions:NSURLBookmarkCreationWithSecurityScope
																										 includingResourceValuesForKeys:nil
																																			relativeToURL:nil
																																							error:nil];
															if (bookmarkData != nil)
															{
																NSUserDefaults* userDefaults = [NSUserDefaults standardUserDefaults];
																NSArray<NSData*>* securityBookmarks = (NSArray<NSData*>*)[userDefaults objectForKey:@"securityBookmarks"];
																if (securityBookmarks == nil)
																{
																	securityBookmarks = [[NSArray<NSData*> alloc] init];
																}
																NSMutableArray<NSData*>* mutableSecurityBookmarks = [securityBookmarks mutableCopy];
																[mutableSecurityBookmarks addObject:bookmarkData];
																[userDefaults setObject:mutableSecurityBookmarks forKey:@"securityBookmarks"];
															}
														}
#endif

														if (sourceFilePath.IsDirectory())
														{
															[[maybe_unused]] const bool wasCopied = sourceFilePath.CopyDirectoryTo(path);
															Assert(wasCopied);
														}
														else
														{
															[[maybe_unused]] const bool wasCopied = sourceFilePath.CopyFileTo(path);
															Assert(wasCopied);
														}
													}
													else
													{
														Assert(false);
													}
												}
											}];
			}
		);
#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, filePath]() mutable
			{
				UIWindow* pWindow = (__bridge UIWindow*)m_pUIWindow;
				UIViewController* viewController = [pWindow rootViewController];

				NSString* pathString = [NSString stringWithUTF8String:filePath.GetZeroTerminated()];
				NSURL* pathURL = [NSURL fileURLWithPath:pathString];
				UIDocumentPickerViewController* documentPicker = [[UIDocumentPickerViewController alloc] initForExportingURLs:@[ pathURL ]
																																																							 asCopy:false];

				[viewController presentViewController:documentPicker animated:YES completion:nil];
			}
		);
#elif PLATFORM_EMSCRIPTEN
		if (filePath.IsDirectory())
		{
			QueueOnWindowThread(
				[sourcePath = filePath]() mutable
				{
					JSExportDirectory(String(sourcePath.GetFileName().GetStringView()).GetZeroTerminated(), sourcePath.GetZeroTerminated());
				}
			);
		}
		else
		{
			String fileTypesString;
			fileTypesString.Format("{},{},{}", fileType.title, fileType.mimeType, fileType.extension.GetStringView());

			QueueOnWindowThread(
				[sourcePath = filePath, fileTypesString = Move(fileTypesString)]() mutable
				{
					JSExportFile(
						String(sourcePath.GetFileName().GetStringView()).GetZeroTerminated(),
						fileTypesString.GetZeroTerminated(),
						sourcePath.GetZeroTerminated()
					);
				}
			);
		}
#else
		Assert(false, "Not implemented for platform");
		UNUSED(filePath);
#endif
	}

	void Window::Minimize()
	{
#if USE_SDL
		SDL_MinimizeWindow(m_pSDLWindow);
#elif PLATFORM_WINDOWS
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this]()
			{
				::ShowWindow(static_cast<HWND>(m_pWindowHandle), SW_MINIMIZE);
			}
		);
#endif
	}

	void Window::Maximize()
	{
#if USE_SDL
		SDL_MaximizeWindow(m_pSDLWindow);
#elif PLATFORM_WINDOWS
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this]()
			{
				::ShowWindow(static_cast<HWND>(m_pWindowHandle), SW_MAXIMIZE);
			}
		);
#endif
	}

	void Window::RestoreSize()
	{
#if USE_SDL
		SDL_RestoreWindow(m_pSDLWindow);
#elif PLATFORM_WINDOWS
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this]()
			{
				::ShowWindow(static_cast<HWND>(m_pWindowHandle), SW_RESTORE);
			}
		);
#endif
	}

	bool Window::CanStartResizing() const
	{
		return true;
	}

	bool Window::TryStartResizing()
	{
		if (CanStartResizing())
		{
			const EnumFlags<StateFlags> setFlags{StateFlags::IsAwaitingResize | StateFlags::IsAwaitingResizeNewFrame};
			const EnumFlags<StateFlags> previousStateFlags = m_stateFlags.FetchOr(setFlags);
			if (previousStateFlags.AreAnySet(setFlags))
			{
				// Another resize was already in progress, abort
				m_stateFlags &= ~(~previousStateFlags & setFlags);
				return false;
			}
			System::Get<Engine>().OnBeforeStartFrame.Add(
				*this,
				[](Window& window)
				{
					[[maybe_unused]] const bool wasCleared = window.m_stateFlags.TryClearFlags(StateFlags::IsAwaitingResizeNewFrame);
					Assert(wasCleared);

					// Await size change completion before letting the new frame start
					Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
					while (window.m_stateFlags.IsSet(StateFlags::IsAwaitingResize))
					{
						thread.DoRunNextJob();
					}
				}
			);
			// Don't let the resizing complete until we're ready for the next frame
			if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
			{
				while (m_stateFlags.IsSet(StateFlags::IsAwaitingResizeNewFrame))
				{
					pThread->DoRunNextJob();
				}
			}
			else
			{
				while (m_stateFlags.IsSet(StateFlags::IsAwaitingResizeNewFrame))
					;
			}
			return true;
		}
		else
		{
			return false;
		}
	}

	void Window::TryOrQueueResize(const Math::Vector2ui newClientAreaSize)
	{
		m_queuedNewSize = newClientAreaSize;
		TryOrQueueResizeInternal(newClientAreaSize);
	}

	void Window::TryOrQueueResizeInternal(const Math::Vector2ui newClientAreaSize)
	{
		using namespace ngine;
		if (GetClientAreaSize() != newClientAreaSize)
		{
			auto tryStartResizing = [this]() -> bool
			{
				const EnumFlags<StateFlags> previousFlags = m_stateFlags.FetchOr(StateFlags::IsResizing);
				Assert(previousFlags.IsNotSet(StateFlags::IsResizingQueued) || previousFlags.IsSet(StateFlags::IsResizing));
				return previousFlags.AreNoneSet(StateFlags::IsResizingQueued | StateFlags::IsResizing);
			};

			const bool allowedResize = CanStartResizing();
			if (allowedResize && tryStartResizing())
			{
				StartResizingInternal(newClientAreaSize);
			}
			else
			{
				const EnumFlags<StateFlags> previousFlags = m_stateFlags.FetchOr(StateFlags::IsResizingQueued);
				if (previousFlags.AreNoneSet(StateFlags::IsResizingQueued | StateFlags::IsResizing))
				{
					if (allowedResize && tryStartResizing())
					{
						Assert(CanStartResizing());
						StartResizingInternal(newClientAreaSize);
					}
					else
					{
						[[maybe_unused]] const bool wasCleared = m_stateFlags.TryClearFlags(StateFlags::IsResizingQueued);
						Assert(wasCleared);

#if PLATFORM_EMSCRIPTEN
						System::Get<Engine>().OnBeforeStartFrame.Add(
							*this,
							[this, newClientAreaSize](Window&)
#else
						QueueOnWindowThread(
							[this, newClientAreaSize]()
#endif
							{
								TryOrQueueResizeInternal(newClientAreaSize);
							}
						);
					}
				}
			}
		}
		else
		{
			m_stateFlags &= ~StateFlags::IsAwaitingResize;
		}
	}

	void Window::RecreateSurface()
	{
#if RENDERER_VULKAN
		m_surface = Rendering::Surface(System::Get<Renderer>().GetInstance(), {}, GetSurfaceHandle());
#else
		Assert(false, "Surface recreation is not supported");
#endif

		using namespace ngine;
		auto tryStartResizing = [this]() -> bool
		{
			const EnumFlags<StateFlags> previousFlags = m_stateFlags.FetchOr(StateFlags::IsResizing);
			Assert(previousFlags.IsNotSet(StateFlags::IsResizingQueued) || previousFlags.IsSet(StateFlags::IsResizing));
			return previousFlags.AreNoneSet(StateFlags::IsResizingQueued | StateFlags::IsResizing);
		};

		const bool allowedResize = CanStartResizing();
		if (allowedResize && tryStartResizing())
		{
			StartResizingInternal(GetClientAreaSize());
		}
		else
		{
			const EnumFlags<StateFlags> previousFlags = m_stateFlags.FetchOr(StateFlags::IsResizingQueued);
			if (previousFlags.AreNoneSet(StateFlags::IsResizingQueued | StateFlags::IsResizing))
			{
				if (allowedResize && tryStartResizing())
				{
					Assert(CanStartResizing());
					StartResizingInternal(GetClientAreaSize());
				}
				else
				{
					[[maybe_unused]] const bool wasCleared = m_stateFlags.TryClearFlags(StateFlags::IsResizingQueued);
					Assert(wasCleared);

#if PLATFORM_EMSCRIPTEN
					System::Get<Engine>().OnBeforeStartFrame.Add(
						*this,
						[this](Window&)
#else
					QueueOnWindowThread(
						[this]()
#endif
						{
							RecreateSurface();
						}
					);
				}
			}
		}
	}

	void Window::Close()
	{
#if USE_SDL
		if (m_pSDLWindow != nullptr)
		{
			SDL_Window* pSDLWindow = m_pSDLWindow;
			m_pSDLWindow = nullptr;
			SDL_DestroyWindow(pSDLWindow);
		}
#elif PLATFORM_WINDOWS
		if (m_pWindowHandle != nullptr)
		{
			void* pWindowHandle = m_pWindowHandle;
			m_pWindowHandle = nullptr;
			Rendering::Window::ExecuteImmediatelyOnWindowThread(
				[pWindowHandle]()
				{
					DragAcceptFiles(reinterpret_cast<HWND>(pWindowHandle), FALSE);
					DestroyWindow(reinterpret_cast<HWND>(pWindowHandle));
				}
			);
		}
#endif
		delete this;
	}

	void Window::SetTitle(const ConstNativeZeroTerminatedStringView title)
	{
#if USE_SDL
		SDL_SetWindowTitle(m_pSDLWindow, title);
#elif PLATFORM_WINDOWS
		SetWindowTextW(static_cast<HWND>(m_pWindowHandle), title);
#elif PLATFORM_EMSCRIPTEN
		emscripten_set_window_title(title);
#else
		UNUSED(title);
#endif
	}

	void Window::SetUri([[maybe_unused]] const IO::ConstZeroTerminatedURIView uri)
	{
#if PLATFORM_EMSCRIPTEN
		QueueOnWindowThread(
			[uri = IO::URI(uri)]()
			{
				PUSH_CLANG_WARNINGS
				DISABLE_CLANG_WARNING("-Wdollar-in-identifier-extension")
				// clang-format off
				EM_ASM(
					{
						if (self.location && history.pushState)
						{
							var newUrl = UTF8ToString($0);
							if (self.location.href !== newUrl)
							{
								history.pushState({}, null, newUrl);
							}
						}
					},
					uri.GetZeroTerminated().GetData()
				);
				// clang-format on
				POP_CLANG_WARNINGS
			}
		);
#endif
	}

	IO::URI Window::GetUri() const
	{
#if PLATFORM_EMSCRIPTEN
		IO::URI uri;
		ExecuteImmediatelyOnWindowThread(
			[&uri]()
			{
				char* str = (char*)EM_ASM_PTR({ return stringToNewUTF8(self.location.href); });
				uri = IO::URI(IO::URI::StringType(IO::URI::StringType::ConstView{str, (IO::URI::SizeType)strlen(str)}));
				free(str);
			}
		);
		return uri;
#elif PROFILE_BUILD
		return IO::URI(MAKE_URI("https://dev.sceneri.com"));
#else
		return IO::URI(MAKE_URI("https://app.sceneri.com"));
#endif
	}

	IO::URI Window::CreateShareableUri(const IO::ConstURIView parameters) const
	{
		return IO::URI::Merge(GetUri().GetView().GetFullDomainWithProtocol().GetStringView(), MAKE_URI("?"), parameters.GetStringView());
	}

#if USE_SDL
	void Window::ProcessSDLMessages()
	{
		const static auto getKeyboardInput = [](const SDL_Keycode keyCode)
		{
			switch (keyCode)
			{
				case SDLK_BACKSPACE:
					return Input::KeyboardInput::Backspace;
				case SDLK_TAB:
					return Input::KeyboardInput::Tab;
				case SDLK_RETURN:
					return Input::KeyboardInput::Enter;
				case SDLK_LSHIFT:
					return Input::KeyboardInput::LeftShift;
				case SDLK_RSHIFT:
					return Input::KeyboardInput::RightShift;
				case SDLK_LCTRL:
					return Input::KeyboardInput::LeftControl;
				case SDLK_RCTRL:
					return Input::KeyboardInput::RightControl;
				case SDLK_LALT:
					return Input::KeyboardInput::LeftAlt;
				case SDLK_RALT:
					return Input::KeyboardInput::RightAlt;
				// case SDLK_LMETA:
				// case SDLK_LSUPER:
				case SDLK_LGUI:
					return Input::KeyboardInput::LeftCommand;
				// case SDLK_RMETA:
				// case SDLK_RSUPER:
				case SDLK_RGUI:
					return Input::KeyboardInput::RightCommand;
				case SDLK_CAPSLOCK:
					return Input::KeyboardInput::CapsLock;
				case SDLK_ESCAPE:
					return Input::KeyboardInput::Escape;
				case SDLK_SPACE:
					return Input::KeyboardInput::Space;
				case SDLK_PAGEUP:
					return Input::KeyboardInput::PageUp;
				case SDLK_PAGEDOWN:
					return Input::KeyboardInput::PageDown;
				case SDLK_END:
					return Input::KeyboardInput::End;
				case SDLK_HOME:
					return Input::KeyboardInput::Home;
				case SDLK_LEFT:
					return Input::KeyboardInput::ArrowLeft;
				case SDLK_UP:
					return Input::KeyboardInput::ArrowUp;
				case SDLK_RIGHT:
					return Input::KeyboardInput::ArrowRight;
				case SDLK_DOWN:
					return Input::KeyboardInput::ArrowDown;
				case SDLK_INSERT:
					return Input::KeyboardInput::Insert;
				case SDLK_DELETE:
					return Input::KeyboardInput::Delete;
				case SDLK_0:
					return Input::KeyboardInput::Zero;
				case SDLK_1:
					return Input::KeyboardInput::One;
				case SDLK_2:
					return Input::KeyboardInput::Two;
				case SDLK_3:
					return Input::KeyboardInput::Three;
				case SDLK_4:
					return Input::KeyboardInput::Four;
				case SDLK_5:
					return Input::KeyboardInput::Five;
				case SDLK_6:
					return Input::KeyboardInput::Six;
				case SDLK_7:
					return Input::KeyboardInput::Seven;
				case SDLK_8:
					return Input::KeyboardInput::Eight;
				case SDLK_9:
					return Input::KeyboardInput::Nine;
				case SDLK_COLON:
					return Input::KeyboardInput::Colon;
				case SDLK_SEMICOLON:
					return Input::KeyboardInput::Semicolon;
				case SDLK_LESS:
					return Input::KeyboardInput::LessThan;
				case SDLK_EQUALS:
					return Input::KeyboardInput::Equals;
				case SDLK_GREATER:
					return Input::KeyboardInput::GreaterThan;
				case SDLK_QUESTION:
					return Input::KeyboardInput::QuestionMark;
				case SDLK_AT:
					return Input::KeyboardInput::At;
				case SDLK_a:
					return Input::KeyboardInput::A;
				case SDLK_b:
					return Input::KeyboardInput::B;
				case SDLK_c:
					return Input::KeyboardInput::C;
				case SDLK_d:
					return Input::KeyboardInput::D;
				case SDLK_e:
					return Input::KeyboardInput::E;
				case SDLK_f:
					return Input::KeyboardInput::F;
				case SDLK_g:
					return Input::KeyboardInput::G;
				case SDLK_h:
					return Input::KeyboardInput::H;
				case SDLK_i:
					return Input::KeyboardInput::I;
				case SDLK_j:
					return Input::KeyboardInput::J;
				case SDLK_k:
					return Input::KeyboardInput::K;
				case SDLK_l:
					return Input::KeyboardInput::L;
				case SDLK_m:
					return Input::KeyboardInput::M;
				case SDLK_n:
					return Input::KeyboardInput::N;
				case SDLK_o:
					return Input::KeyboardInput::O;
				case SDLK_p:
					return Input::KeyboardInput::P;
				case SDLK_q:
					return Input::KeyboardInput::Q;
				case SDLK_r:
					return Input::KeyboardInput::R;
				case SDLK_s:
					return Input::KeyboardInput::S;
				case SDLK_t:
					return Input::KeyboardInput::T;
				case SDLK_u:
					return Input::KeyboardInput::U;
				case SDLK_v:
					return Input::KeyboardInput::V;
				case SDLK_w:
					return Input::KeyboardInput::W;
				case SDLK_x:
					return Input::KeyboardInput::X;
				case SDLK_y:
					return Input::KeyboardInput::Y;
				case SDLK_z:
					return Input::KeyboardInput::Z;
				case SDLK_KP_0:
					return Input::KeyboardInput::Numpad0;
				case SDLK_KP_1:
					return Input::KeyboardInput::Numpad1;
				case SDLK_KP_2:
					return Input::KeyboardInput::Numpad2;
				case SDLK_KP_3:
					return Input::KeyboardInput::Numpad3;
				case SDLK_KP_4:
					return Input::KeyboardInput::Numpad4;
				case SDLK_KP_5:
					return Input::KeyboardInput::Numpad5;
				case SDLK_KP_6:
					return Input::KeyboardInput::Numpad6;
				case SDLK_KP_7:
					return Input::KeyboardInput::Numpad7;
				case SDLK_KP_8:
					return Input::KeyboardInput::Numpad8;
				case SDLK_KP_9:
					return Input::KeyboardInput::Numpad9;
				case SDLK_KP_MULTIPLY:
					return Input::KeyboardInput::Multiply;
				case SDLK_KP_PLUS:
					return Input::KeyboardInput::Add;
				case SDLK_KP_MINUS:
					return Input::KeyboardInput::Subtract;
				case SDLK_KP_PERIOD:
					return Input::KeyboardInput::Decimal;
				case SDLK_KP_DIVIDE:
					return Input::KeyboardInput::Divide;
				case SDLK_F1:
					return Input::KeyboardInput::F1;
				case SDLK_F2:
					return Input::KeyboardInput::F2;
				case SDLK_F3:
					return Input::KeyboardInput::F3;
				case SDLK_F4:
					return Input::KeyboardInput::F4;
				case SDLK_F5:
					return Input::KeyboardInput::F5;
				case SDLK_F6:
					return Input::KeyboardInput::F6;
				case SDLK_F7:
					return Input::KeyboardInput::F7;
				case SDLK_F8:
					return Input::KeyboardInput::F8;
				case SDLK_F9:
					return Input::KeyboardInput::F9;
				case SDLK_F10:
					return Input::KeyboardInput::F10;
				case SDLK_F11:
					return Input::KeyboardInput::F11;
				case SDLK_F12:
					return Input::KeyboardInput::F12;
				case SDLK_CARET:
					return Input::KeyboardInput::Circumflex;
				case SDLK_EXCLAIM:
					return Input::KeyboardInput::Exclamation;
				case SDLK_QUOTEDBL:
					return Input::KeyboardInput::DoubleQuote;
				case SDLK_HASH:
					return Input::KeyboardInput::Hash;
				case SDLK_DOLLAR:
					return Input::KeyboardInput::Dollar;
				case SDLK_PERCENT:
					return Input::KeyboardInput::Percent;
				case SDLK_AMPERSAND:
					return Input::KeyboardInput::Ampersand;
				case SDLK_UNDERSCORE:
					return Input::KeyboardInput::Underscore;
				case SDLK_LEFTPAREN:
					return Input::KeyboardInput::OpenParantheses;
				case SDLK_RIGHTPAREN:
					return Input::KeyboardInput::CloseParantheses;
				case SDLK_ASTERISK:
					return Input::KeyboardInput::Asterisk;
				case SDLK_PLUS:
					return Input::KeyboardInput::Plus;
				case SDLK_KP_VERTICALBAR:
					return Input::KeyboardInput::Pipe;
				case SDLK_MINUS:
					return Input::KeyboardInput::Minus;
				case SDLK_KP_LEFTBRACE:
					return Input::KeyboardInput::OpenCurlyBracket;
				case SDLK_KP_RIGHTBRACE:
					return Input::KeyboardInput::CloseCurlyBracket;
				case '~':
					return Input::KeyboardInput::Tilde;
				case SDLK_COMMA:
					return Input::KeyboardInput::Comma;
				case SDLK_PERIOD:
					return Input::KeyboardInput::Period;
				case SDLK_SLASH:
					return Input::KeyboardInput::ForwardSlash;
				case SDLK_BACKQUOTE:
					return Input::KeyboardInput::BackQuote;
				case SDLK_LEFTBRACKET:
					return Input::KeyboardInput::OpenBracket;
				case SDLK_BACKSLASH:
					return Input::KeyboardInput::BackSlash;
				case SDLK_RIGHTBRACKET:
					return Input::KeyboardInput::CloseBracket;
				case SDLK_QUOTE:
					return Input::KeyboardInput::Quote;
			}
			return Input::KeyboardInput::Unknown;
		};

		static Vector<Widgets::DragAndDropData> droppedFilePaths;

		WindowThread& windowThread = *GetWindowThread();

		SDL_Event sdlEvent;
		while (!System::Get<Engine>().IsAboutToQuit())
		{
			if (!IsWindowThreadActivated)
			{
				windowThread.ProcessInvokeQueue();
			}
			else if (SDL_WaitEvent(&sdlEvent))
			{
				switch (sdlEvent.type)
				{
					case SDL_WINDOWEVENT:
					{
						SDL_Window* pWindow = SDL_GetWindowFromID(sdlEvent.window.windowID);

						if (pWindow == nullptr)
						{
							continue;
						}

						Window* pEngineWindow = reinterpret_cast<Window*>(SDL_GetWindowData(pWindow, "Window"));
						if (pEngineWindow == nullptr)
						{
							continue;
						}
						Window& window = *pEngineWindow;

						switch (sdlEvent.window.event)
						{
							case SDL_WINDOWEVENT_MOVED:
							{
								window.OnMoved(Math::Vector2i{sdlEvent.window.data1, sdlEvent.window.data2});
							}
							break;
							case SDL_WINDOWEVENT_SIZE_CHANGED:
							{
								Math::Vector2i newSize;
								SDL_GetWindowSize(pWindow, &newSize.x, &newSize.y);
								if ((window.GetClientAreaSize() != (Math::Vector2ui)newSize).AreAnySet())
								{
									window.TryOrQueueResize((Math::Vector2ui)newSize);
								}
							}
							break;
							case SDL_WINDOWEVENT_CLOSE:
							{
								window.Close();
							}
							break;
							case SDL_WINDOWEVENT_FOCUS_GAINED:
							{
								window.OnReceivedKeyboardFocus();
							}
							break;
							case SDL_WINDOWEVENT_FOCUS_LOST:
							{
								window.OnLostKeyboardFocus();
							}
							break;
							case SDL_WINDOWEVENT_ENTER:
							{
								window.OnReceivedMouseFocus();
							}
							break;
							case SDL_WINDOWEVENT_LEAVE:
							{
								window.OnLostMouseFocus();
							}
							break;
							case SDL_WINDOWEVENT_SHOWN:
							{
								window.OnBecomeVisible();
							}
							break;
							case SDL_WINDOWEVENT_HIDDEN:
							{
								window.OnBecomeHidden();
							}
							break;
							case SDL_WINDOWEVENT_MINIMIZED:
							{
								window.OnBecomeHidden();
							}
							break;
							case SDL_WINDOWEVENT_RESTORED:
							{
								window.OnBecomeVisible();
							}
							break;
						}
					}
					break;
					case SDL_MOUSEMOTION:
					{
						SDL_Window* pWindow = SDL_GetWindowFromID(sdlEvent.motion.windowID);
						if (pWindow == nullptr)
						{
							continue;
						}
						Window& window = *reinterpret_cast<Window*>(SDL_GetWindowData(pWindow, "Window"));

						Assert((sdlEvent.motion.x >= 0) | (sdlEvent.motion.y >= 0));

						Input::Manager& inputManager = System::Get<Input::Manager>();
						Input::MouseDeviceType& mouseDeviceType =
							inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
						const Input::DeviceIdentifier mouseIdentifier =
							mouseDeviceType.GetOrRegisterInstance(sdlEvent.motion.which, inputManager, window);

						if (window.m_lockCursorPositionCounter > 0)
						{
							SDL_WarpMouseInWindow(pWindow, window.m_lockedCursorPosition.x, window.m_lockedCursorPosition.y);
							sdlEvent.motion.x = window.m_lockedCursorPosition.x;
							sdlEvent.motion.y = window.m_lockedCursorPosition.y;
						}

						const ScreenCoordinate coordinate =
							window.ConvertLocalToScreenCoordinates({static_cast<int>(sdlEvent.motion.x), static_cast<int>(sdlEvent.motion.y)});
						mouseDeviceType.OnMotion(
							mouseIdentifier,
							coordinate,
							Math::Vector2i{-static_cast<int>(sdlEvent.motion.xrel), -static_cast<int>(sdlEvent.motion.yrel)},
							window
						);
					}
					break;
					case SDL_MOUSEBUTTONDOWN:
					{
						SDL_Window* pWindow = SDL_GetWindowFromID(sdlEvent.button.windowID);
						if (pWindow == nullptr)
						{
							continue;
						}
						Window& window = *reinterpret_cast<Window*>(SDL_GetWindowData(pWindow, "Window"));

						Assert((sdlEvent.button.x >= 0) | (sdlEvent.button.y >= 0));

						Input::Manager& inputManager = System::Get<Input::Manager>();
						Input::MouseDeviceType& mouseDeviceType =
							inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
						const Input::DeviceIdentifier mouseIdentifier =
							mouseDeviceType.GetOrRegisterInstance(sdlEvent.motion.which, inputManager, window);

						const ScreenCoordinate coordinate =
							window.ConvertLocalToScreenCoordinates({static_cast<int>(sdlEvent.button.x), static_cast<int>(sdlEvent.button.y)});
						window.SetInputFocusAtCoordinate(window.ConvertScreenToLocalCoordinates(coordinate), Invalid);

						const Input::MouseButton buttonMask = Input::MouseButton(1 << (sdlEvent.button.button - 1u));
						mouseDeviceType.OnPress(mouseIdentifier, coordinate, buttonMask, window);
					}
					break;
					case SDL_MOUSEBUTTONUP:
					{
						SDL_Window* pWindow = SDL_GetWindowFromID(sdlEvent.button.windowID);
						if (pWindow == nullptr)
						{
							continue;
						}
						Window& window = *reinterpret_cast<Window*>(SDL_GetWindowData(pWindow, "Window"));

						Assert((sdlEvent.button.x >= 0) | (sdlEvent.button.y >= 0));

						Input::Manager& inputManager = System::Get<Input::Manager>();
						Input::MouseDeviceType& mouseDeviceType =
							inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
						const Input::DeviceIdentifier mouseIdentifier =
							mouseDeviceType.GetOrRegisterInstance(sdlEvent.motion.which, inputManager, window);

						const Input::MouseButton buttonMask = Input::MouseButton(1 << (sdlEvent.button.button - 1u));
						mouseDeviceType.OnRelease(
							mouseIdentifier,
							window.ConvertLocalToScreenCoordinates({static_cast<int>(sdlEvent.button.x), static_cast<int>(sdlEvent.button.y)}),
							buttonMask,
							&window
						);
					}
					break;
					case SDL_MOUSEWHEEL:
					{
						SDL_Window* pWindow = SDL_GetWindowFromID(sdlEvent.wheel.windowID);
						if (pWindow == nullptr)
						{
							continue;
						}
						Window& window = *reinterpret_cast<Window*>(SDL_GetWindowData(pWindow, "Window"));

						Input::Manager& inputManager = System::Get<Input::Manager>();
						Input::MouseDeviceType& mouseDeviceType =
							inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
						const Input::DeviceIdentifier mouseIdentifier =
							mouseDeviceType.GetOrRegisterInstance(sdlEvent.motion.which, inputManager, window);

						Math::Vector2i relativeCoordinates;
						SDL_GetMouseState(&relativeCoordinates.x, &relativeCoordinates.y);

						mouseDeviceType.OnScroll(
							mouseIdentifier,
							window.ConvertLocalToScreenCoordinates({(int)relativeCoordinates.x, (int)relativeCoordinates.y}),
							{sdlEvent.wheel.x, sdlEvent.wheel.y},
							window
						);
					}
					break;
					case SDL_KEYDOWN:
					{
						SDL_Window* pWindow = SDL_GetWindowFromID(sdlEvent.button.windowID);
						if (pWindow == nullptr)
						{
							continue;
						}

						Window& window = *reinterpret_cast<Window*>(SDL_GetWindowData(pWindow, "Window"));

						Input::KeyboardDeviceType& keyboardDeviceType = System::Get<Input::Manager>().GetDeviceType<Input::KeyboardDeviceType>(
							System::Get<Input::Manager>().GetKeyboardDeviceTypeIdentifier()
						);
						const Input::DeviceIdentifier keyboardIdentifier =
							keyboardDeviceType.GetOrRegisterInstance(uintptr(0), System::Get<Input::Manager>(), window);

						if (sdlEvent.key.repeat == 0)
						{
							const Input::KeyboardInput input = getKeyboardInput(sdlEvent.key.keysym.sym);
							keyboardDeviceType.OnKeyDown(keyboardIdentifier, input);
						}
					}
					break;
					case SDL_KEYUP:
					{
						SDL_Window* pWindow = SDL_GetWindowFromID(sdlEvent.button.windowID);
						if (pWindow == nullptr)
						{
							continue;
						}

						Window& window = *reinterpret_cast<Window*>(SDL_GetWindowData(pWindow, "Window"));

						Input::KeyboardDeviceType& keyboardDeviceType = System::Get<Input::Manager>().GetDeviceType<Input::KeyboardDeviceType>(
							System::Get<Input::Manager>().GetKeyboardDeviceTypeIdentifier()
						);
						const Input::DeviceIdentifier keyboardIdentifier =
							keyboardDeviceType.GetOrRegisterInstance(uintptr(0), System::Get<Input::Manager>(), window);

						if (sdlEvent.key.repeat == 0)
						{
							const Input::KeyboardInput input = getKeyboardInput(sdlEvent.key.keysym.sym);
							keyboardDeviceType.OnKeyUp(keyboardIdentifier, input);
						}
					}
					break;
					case SDL_TEXTINPUT:
					{
						SDL_Window* pWindow = SDL_GetWindowFromID(sdlEvent.button.windowID);
						if (pWindow == nullptr)
						{
							continue;
						}

						Window& window = *reinterpret_cast<Window*>(SDL_GetWindowData(pWindow, "Window"));

						Input::KeyboardDeviceType& keyboardDeviceType = System::Get<Input::Manager>().GetDeviceType<Input::KeyboardDeviceType>(
							System::Get<Input::Manager>().GetKeyboardDeviceTypeIdentifier()
						);
						const Input::DeviceIdentifier keyboardIdentifier =
							keyboardDeviceType.GetOrRegisterInstance(uintptr(0), System::Get<Input::Manager>(), window);

						const ConstStringView text{sdlEvent.text.text, (ConstStringView::SizeType)strlen(sdlEvent.text.text)};
						keyboardDeviceType.OnText(keyboardIdentifier, UnicodeString{text});
					}
					break;
					case SDL_FINGERDOWN:
					{
						SDL_Window* pWindow = SDL_GetWindowFromID(sdlEvent.tfinger.windowID);
						if (pWindow == nullptr)
						{
							continue;
						}
						Window& window = *reinterpret_cast<Window*>(SDL_GetWindowData(pWindow, "Window"));

						Assert((sdlEvent.tfinger.x >= 0) | (sdlEvent.tfinger.y >= 0));

						Input::Manager& inputManager = System::Get<Input::Manager>();
						Input::TouchscreenDeviceType& touchscreenDeviceType =
							inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
						const Input::DeviceIdentifier touchscreenIdentifier =
							touchscreenDeviceType.GetOrRegisterInstance(sdlEvent.tfinger.touchId, inputManager, window);

						const ScreenCoordinate coordinates(
							(Math::Vector2f)window.GetClientAreaSize() * Math::Vector2f{sdlEvent.tfinger.x, sdlEvent.tfinger.y}
						);

						const Math::Ratiof pressureRatio = sdlEvent.tfinger.pressure;
						const Input::FingerIdentifier fingerIdentifier = (Input::FingerIdentifier)sdlEvent.tfinger.fingerId;

						Input::TouchDescriptor touchDescriptor;
						touchDescriptor.fingerIdentifier = fingerIdentifier;
						touchDescriptor.touchRadius = Input::Monitor::DefaultTouchRadius;
						touchDescriptor.screenCoordinate = coordinates;
						touchDescriptor.pressureRatio = pressureRatio;

						touchscreenDeviceType.OnStartTouch(Move(touchDescriptor), touchscreenIdentifier, window);
					}
					break;
					case SDL_FINGERUP:
					{
						SDL_Window* pWindow = SDL_GetWindowFromID(sdlEvent.tfinger.windowID);
						if (pWindow == nullptr)
						{
							continue;
						}
						Window& window = *reinterpret_cast<Window*>(SDL_GetWindowData(pWindow, "Window"));

						Assert((sdlEvent.tfinger.x >= 0) | (sdlEvent.tfinger.y >= 0));

						Input::Manager& inputManager = System::Get<Input::Manager>();
						Input::TouchscreenDeviceType& touchscreenDeviceType =
							inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
						const Input::DeviceIdentifier touchscreenIdentifier =
							touchscreenDeviceType.GetOrRegisterInstance(sdlEvent.tfinger.touchId, inputManager, window);

						const ScreenCoordinate coordinates(
							(Math::Vector2f)window.GetClientAreaSize() * Math::Vector2f{sdlEvent.tfinger.x, sdlEvent.tfinger.y}
						);

						const Math::Ratiof pressureRatio = sdlEvent.tfinger.pressure;
						const Input::FingerIdentifier fingerIdentifier = (Input::FingerIdentifier)sdlEvent.tfinger.fingerId;

						Input::TouchDescriptor touchDescriptor;
						touchDescriptor.fingerIdentifier = fingerIdentifier;
						touchDescriptor.touchRadius = Input::Monitor::DefaultTouchRadius;
						touchDescriptor.screenCoordinate = coordinates;
						touchDescriptor.pressureRatio = pressureRatio;

						touchscreenDeviceType.OnStopTouch(Move(touchDescriptor), touchscreenIdentifier, window);
					}
					break;
					case SDL_FINGERMOTION:
					{
						SDL_Window* pWindow = SDL_GetWindowFromID(sdlEvent.tfinger.windowID);
						if (pWindow == nullptr)
						{
							continue;
						}
						Window& window = *reinterpret_cast<Window*>(SDL_GetWindowData(pWindow, "Window"));

						Assert((sdlEvent.tfinger.x >= 0) | (sdlEvent.tfinger.y >= 0));

						Input::Manager& inputManager = System::Get<Input::Manager>();
						Input::TouchscreenDeviceType& touchscreenDeviceType =
							inputManager.GetDeviceType<Input::TouchscreenDeviceType>(inputManager.GetTouchscreenDeviceTypeIdentifier());
						const Input::DeviceIdentifier touchscreenIdentifier =
							touchscreenDeviceType.GetOrRegisterInstance(sdlEvent.tfinger.touchId, inputManager, window);

						const ScreenCoordinate coordinates(
							(Math::Vector2f)window.GetClientAreaSize() * Math::Vector2f{sdlEvent.tfinger.x, sdlEvent.tfinger.y}
						);

						const Math::Vector2i deltaCoordinates(
							(Math::Vector2f)window.GetClientAreaSize() * Math::Vector2f{sdlEvent.tfinger.dx, sdlEvent.tfinger.dy}
						);
						const Math::Ratiof pressureRatio = sdlEvent.tfinger.pressure;
						const Input::FingerIdentifier fingerIdentifier = (Input::FingerIdentifier)sdlEvent.tfinger.fingerId;

						Input::TouchDescriptor touchDescriptor;
						touchDescriptor.fingerIdentifier = fingerIdentifier;
						touchDescriptor.touchRadius = Input::Monitor::DefaultTouchRadius;
						touchDescriptor.deltaCoordinates = deltaCoordinates;
						touchDescriptor.screenCoordinate = coordinates;
						touchDescriptor.pressureRatio = pressureRatio;

						touchscreenDeviceType.OnMotion(Move(touchDescriptor), touchscreenIdentifier, window);
					}
					break;

					case SDL_CONTROLLERAXISMOTION:
					{
						Input::Manager& inputManager = System::Get<Input::Manager>();
						Input::GamepadDeviceType& gamepadDeviceType =
							inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());
						const Input::DeviceIdentifier gamepadIdentifier =
							gamepadDeviceType.GetOrRegisterInstance(sdlEvent.caxis.which, inputManager, Invalid);

						SDL_GameController* pGameController = SDL_GameControllerFromInstanceID(sdlEvent.caxis.which);

						switch (sdlEvent.caxis.axis)
						{
							case SDL_CONTROLLER_AXIS_LEFTX:
							{
								Math::Rangef sourceRange = Math::Rangef::Make(-32768.f, 65535.f);
								Math::Rangef targetRange = Math::Rangef::Make(-1.f, 2.f);
								const float axisValue = targetRange.GetRemappedValue((float)sdlEvent.caxis.value, sourceRange);
								const int16 otherAxis = SDL_GameControllerGetAxis(pGameController, SDL_CONTROLLER_AXIS_LEFTY);
								const float otherAxisValue = targetRange.GetRemappedValue((float)otherAxis, sourceRange);

								gamepadDeviceType
									.OnAxisInput(gamepadIdentifier, Input::GamepadInput::Axis::LeftThumbstick, Math::Vector2f{axisValue, -otherAxisValue});
							}
							break;
							case SDL_CONTROLLER_AXIS_LEFTY:
							{
								Math::Rangef sourceRange = Math::Rangef::Make(-32768.f, 65535.f);
								Math::Rangef targetRange = Math::Rangef::Make(-1.f, 2.f);
								const float axisValue = targetRange.GetRemappedValue((float)sdlEvent.caxis.value, sourceRange);
								const int16 otherAxis = SDL_GameControllerGetAxis(pGameController, SDL_CONTROLLER_AXIS_LEFTX);
								const float otherAxisValue = targetRange.GetRemappedValue((float)otherAxis, sourceRange);

								gamepadDeviceType
									.OnAxisInput(gamepadIdentifier, Input::GamepadInput::Axis::LeftThumbstick, Math::Vector2f{otherAxisValue, -axisValue});
							}
							break;
							case SDL_CONTROLLER_AXIS_RIGHTX:
							{
								Math::Rangef sourceRange = Math::Rangef::Make(-32768.f, 65535.f);
								Math::Rangef targetRange = Math::Rangef::Make(-1.f, 2.f);
								const float axisValue = targetRange.GetRemappedValue((float)sdlEvent.caxis.value, sourceRange);
								const int16 otherAxis = SDL_GameControllerGetAxis(pGameController, SDL_CONTROLLER_AXIS_RIGHTY);
								const float otherAxisValue = targetRange.GetRemappedValue((float)otherAxis, sourceRange);

								gamepadDeviceType
									.OnAxisInput(gamepadIdentifier, Input::GamepadInput::Axis::RightThumbstick, Math::Vector2f{axisValue, -otherAxisValue});
							}
							break;
							case SDL_CONTROLLER_AXIS_RIGHTY:
							{
								Math::Rangef sourceRange = Math::Rangef::Make(-32768.f, 65535.f);
								Math::Rangef targetRange = Math::Rangef::Make(-1.f, 2.f);
								const float axisValue = targetRange.GetRemappedValue((float)sdlEvent.caxis.value, sourceRange);
								const int16 otherAxis = SDL_GameControllerGetAxis(pGameController, SDL_CONTROLLER_AXIS_RIGHTX);
								const float otherAxisValue = targetRange.GetRemappedValue((float)otherAxis, sourceRange);

								gamepadDeviceType
									.OnAxisInput(gamepadIdentifier, Input::GamepadInput::Axis::RightThumbstick, Math::Vector2f{otherAxisValue, -axisValue});
							}
							break;
							case SDL_CONTROLLER_AXIS_TRIGGERLEFT:
							{
								Math::Rangef sourceRange = Math::Rangef::Make(0, 32767.f);
								Math::Rangef targetRange = Math::Rangef::Make(0.f, 1.f);
								const float axisValue = targetRange.GetRemappedValue((float)sdlEvent.caxis.value, sourceRange);
								gamepadDeviceType.OnAnalogInput(gamepadIdentifier, Input::GamepadInput::Analog::LeftTrigger, axisValue);
							}
							break;
							case SDL_CONTROLLER_AXIS_TRIGGERRIGHT:
							{
								Math::Rangef sourceRange = Math::Rangef::Make(0, 32767.f);
								Math::Rangef targetRange = Math::Rangef::Make(0.f, 1.f);
								const float axisValue = targetRange.GetRemappedValue((float)sdlEvent.caxis.value, sourceRange);
								gamepadDeviceType.OnAnalogInput(gamepadIdentifier, Input::GamepadInput::Analog::RightTrigger, axisValue);
							}
							break;
						}
					}
					break;
					case SDL_CONTROLLERBUTTONDOWN:
					{
						Input::Manager& inputManager = System::Get<Input::Manager>();
						Input::GamepadDeviceType& gamepadDeviceType =
							inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());
						const Input::DeviceIdentifier gamepadIdentifier =
							gamepadDeviceType.GetOrRegisterInstance(sdlEvent.cbutton.which, inputManager, Invalid);

						const Input::GamepadInput::Button button = [](SDL_GameControllerButton button)
						{
							switch (button)
							{
								case SDL_CONTROLLER_BUTTON_A:
									return Input::GamepadInput::Button::A;
								case SDL_CONTROLLER_BUTTON_B:
									return Input::GamepadInput::Button::B;
								case SDL_CONTROLLER_BUTTON_X:
									return Input::GamepadInput::Button::X;
								case SDL_CONTROLLER_BUTTON_Y:
									return Input::GamepadInput::Button::Y;
								case SDL_CONTROLLER_BUTTON_BACK:
									return Input::GamepadInput::Button::Share;
								case SDL_CONTROLLER_BUTTON_GUIDE:
									return Input::GamepadInput::Button::Home;
								case SDL_CONTROLLER_BUTTON_START:
									return Input::GamepadInput::Button::Options;
								case SDL_CONTROLLER_BUTTON_LEFTSTICK:
									return Input::GamepadInput::Button::LeftThumbstick;
								case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
									return Input::GamepadInput::Button::RightThumbstick;
								case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
									return Input::GamepadInput::Button::LeftShoulder;
								case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
									return Input::GamepadInput::Button::RightShoulder;
								case SDL_CONTROLLER_BUTTON_DPAD_UP:
									return Input::GamepadInput::Button::DirectionPadLeft;
								case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
									return Input::GamepadInput::Button::DirectionPadDown;
								case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
									return Input::GamepadInput::Button::DirectionPadLeft;
								case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
									return Input::GamepadInput::Button::DirectionPadRight;

								case SDL_CONTROLLER_BUTTON_PADDLE1:
									return Input::GamepadInput::Button::PaddleOne;
								case SDL_CONTROLLER_BUTTON_PADDLE2:
									return Input::GamepadInput::Button::PaddleTwo;
								case SDL_CONTROLLER_BUTTON_PADDLE3:
									return Input::GamepadInput::Button::PaddleThree;
								case SDL_CONTROLLER_BUTTON_PADDLE4:
									return Input::GamepadInput::Button::PaddleFour;
								case SDL_CONTROLLER_BUTTON_TOUCHPAD:
									return Input::GamepadInput::Button::Touchpad;

								case SDL_CONTROLLER_BUTTON_MISC1:
								case SDL_CONTROLLER_BUTTON_INVALID:
								case SDL_CONTROLLER_BUTTON_MAX:
									return Input::GamepadInput::Button::Invalid;
							}
							return Input::GamepadInput::Button::Invalid;
						}((SDL_GameControllerButton)sdlEvent.cbutton.button);
						if (button != Input::GamepadInput::Button::Invalid)
						{
							gamepadDeviceType.OnButtonDown(gamepadIdentifier, button);
						}
					}
					break;
					case SDL_CONTROLLERBUTTONUP:
					{
						Input::Manager& inputManager = System::Get<Input::Manager>();
						Input::GamepadDeviceType& gamepadDeviceType =
							inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());
						const Input::DeviceIdentifier gamepadIdentifier =
							gamepadDeviceType.GetOrRegisterInstance(sdlEvent.cbutton.which, inputManager, Invalid);

						const Input::GamepadInput::Button button = [](SDL_GameControllerButton button)
						{
							switch (button)
							{
								case SDL_CONTROLLER_BUTTON_A:
									return Input::GamepadInput::Button::A;
								case SDL_CONTROLLER_BUTTON_B:
									return Input::GamepadInput::Button::B;
								case SDL_CONTROLLER_BUTTON_X:
									return Input::GamepadInput::Button::X;
								case SDL_CONTROLLER_BUTTON_Y:
									return Input::GamepadInput::Button::Y;
								case SDL_CONTROLLER_BUTTON_BACK:
									return Input::GamepadInput::Button::Share;
								case SDL_CONTROLLER_BUTTON_GUIDE:
									return Input::GamepadInput::Button::Home;
								case SDL_CONTROLLER_BUTTON_START:
									return Input::GamepadInput::Button::Options;
								case SDL_CONTROLLER_BUTTON_LEFTSTICK:
									return Input::GamepadInput::Button::LeftThumbstick;
								case SDL_CONTROLLER_BUTTON_RIGHTSTICK:
									return Input::GamepadInput::Button::RightThumbstick;
								case SDL_CONTROLLER_BUTTON_LEFTSHOULDER:
									return Input::GamepadInput::Button::LeftShoulder;
								case SDL_CONTROLLER_BUTTON_RIGHTSHOULDER:
									return Input::GamepadInput::Button::RightShoulder;
								case SDL_CONTROLLER_BUTTON_DPAD_UP:
									return Input::GamepadInput::Button::DirectionPadLeft;
								case SDL_CONTROLLER_BUTTON_DPAD_DOWN:
									return Input::GamepadInput::Button::DirectionPadDown;
								case SDL_CONTROLLER_BUTTON_DPAD_LEFT:
									return Input::GamepadInput::Button::DirectionPadLeft;
								case SDL_CONTROLLER_BUTTON_DPAD_RIGHT:
									return Input::GamepadInput::Button::DirectionPadRight;

								case SDL_CONTROLLER_BUTTON_PADDLE1:
									return Input::GamepadInput::Button::PaddleOne;
								case SDL_CONTROLLER_BUTTON_PADDLE2:
									return Input::GamepadInput::Button::PaddleTwo;
								case SDL_CONTROLLER_BUTTON_PADDLE3:
									return Input::GamepadInput::Button::PaddleThree;
								case SDL_CONTROLLER_BUTTON_PADDLE4:
									return Input::GamepadInput::Button::PaddleFour;
								case SDL_CONTROLLER_BUTTON_TOUCHPAD:
									return Input::GamepadInput::Button::Touchpad;

								case SDL_CONTROLLER_BUTTON_MISC1:
								case SDL_CONTROLLER_BUTTON_INVALID:
								case SDL_CONTROLLER_BUTTON_MAX:
									return Input::GamepadInput::Button::Invalid;
							}
							return Input::GamepadInput::Button::Invalid;
						}((SDL_GameControllerButton)sdlEvent.cbutton.button);

						if (button != Input::GamepadInput::Button::Invalid)
						{
							gamepadDeviceType.OnButtonUp(gamepadIdentifier, button);
						}
					}
					break;
					case SDL_CONTROLLERDEVICEADDED:
					{
						Window& window = *reinterpret_cast<Window*>(SDL_GetWindowData(PrimaryWindow, "Window"));

						Input::Manager& inputManager = System::Get<Input::Manager>();
						Input::GamepadDeviceType& gamepadDeviceType =
							inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());
						[[maybe_unused]] const Input::DeviceIdentifier gamepadIdentifier =
							gamepadDeviceType.GetOrRegisterInstance(sdlEvent.cdevice.which, inputManager, window);

						SDL_GameControllerOpen(sdlEvent.cdevice.which);
					}
					break;
					case SDL_CONTROLLERDEVICEREMOVED:
					{
						SDL_GameController* pController = SDL_GameControllerFromInstanceID(sdlEvent.cdevice.which);
						if (pController != nullptr)
						{
							SDL_GameControllerClose(pController);
						}
					}
					break;

					case SDL_DROPBEGIN:
					{
						droppedFilePaths.Clear();
					}
					break;
					case SDL_DROPFILE:
					{
						const ConstStringView fileNameView(sdlEvent.drop.file, (IO::Path::SizeType)strlen(sdlEvent.drop.file));

						Math::Vector2i relativeCoordinates;
						SDL_GetMouseState(&relativeCoordinates.x, &relativeCoordinates.y);

						droppedFilePaths.EmplaceBack(IO::Path(IO::Path::StringType(fileNameView)));

						SDL_free(sdlEvent.drop.file);
					}
					break;
					case SDL_DROPCOMPLETE:
					{
						SDL_Window* pWindow = SDL_GetWindowFromID(sdlEvent.drop.windowID);
						Window& window = *reinterpret_cast<Window*>(SDL_GetWindowData(pWindow, "Window"));

						Math::Vector2i relativeCoordinates;
						SDL_GetMouseState(&relativeCoordinates.x, &relativeCoordinates.y);

						const ScreenCoordinate coordinate = ScreenCoordinate{(int)relativeCoordinates.x, (int)relativeCoordinates.y};

						if (window.OnStartDragItemsIntoWindow(window.ConvertScreenToLocalCoordinates(coordinate), droppedFilePaths.GetView()))
						{
							[[maybe_unused]] const bool wasDropped =
								window.OnDropItemsIntoWindow(window.ConvertScreenToLocalCoordinates(coordinate), droppedFilePaths.GetView());
						}
					}
					break;
					case SDL_QUIT:
						System::Get<Engine>().Quit();
						break;
					default:
					{
						if (sdlEvent.type == WakeWindowThreadEventId)
						{
							windowThread.ProcessInvokeQueue();
							break;
						}
					}
				}
			}
		}

		// Clear the queue one last time
		windowThread.ProcessInvokeQueue();
	}
#endif

#if PLATFORM_WINDOWS
	/* static */ void Window::ProcessWindowsMessages()
	{
		WindowThread& windowThread = *GetWindowThread();
		windowThread.ProcessInvokeQueue();

		const auto isQuitting = []()
		{
			if (const Optional<Engine*> pEngine = System::Find<Engine>())
			{
				return pEngine->IsAboutToQuit();
			}
			return false;
		};

		MSG message = {};
		BOOL returnValue;
		while (!isQuitting() && (returnValue = GetMessage(&message, 0, 0, 0)) != 0)
		{
			if (returnValue != -1)
			{
				if (message.message == WM_NULL)
				{
					windowThread.ProcessInvokeQueue();
				}
				else
				{
					TranslateMessage(&message);
					DispatchMessage(&message);
					windowThread.ProcessInvokeQueue();
				}
			}
		}

		// Clear the queue one last time
		windowThread.ProcessInvokeQueue();
	}

	void Window::ProcessPointerFrames(const unsigned long long wParam)
	{
		const uint32 pointerID = GET_POINTERID_WPARAM(wParam);

		/*uint32 entriesCount, pointerCount;
		GetPointerFrameInfoHistory(pointerID, &entriesCount, &pointerCount, nullptr);

		Array<POINTER_INFO, 128> pointerInfo;
		Assert(pointerInfo.GetSize() >= pointerCount);
		GetPointerFrameInfoHistory(pointerID, &entriesCount, &pointerCount, pointerInfo.GetData());

		ProcessPointerFramesInteractionContext(reinterpret_cast<HINTERACTIONCONTEXT>(m_pInteractionContext), entriesCount, pointerCount,
		pointerInfo.GetData());*/

		POINTER_INFO pointerInfo;
		GetPointerInfo(pointerID, &pointerInfo);
		ProcessPointerFramesInteractionContext(reinterpret_cast<HINTERACTIONCONTEXT>(m_pInteractionContext), 1, 1, &pointerInfo);
	}

	long long Window::ProcessWindowMessage(const uint32 message, const unsigned long long wParam, const long long lParam)
	{
		switch (message)
		{
			case WM_CLOSE:
				Close();
				break;
			case WM_DESTROY:
				PostQuitMessage(0);
				break;
			case WM_WINDOWPOSCHANGED:
			{
				const WINDOWPOS& windowChangeInfo = *reinterpret_cast<const WINDOWPOS*>(lParam);

				if (windowChangeInfo.flags & SWP_SHOWWINDOW)
				{
					OnBecomeVisible();
				}
				if (windowChangeInfo.flags & SWP_HIDEWINDOW)
				{
					OnBecomeHidden();
				}
				if (!(windowChangeInfo.flags & SWP_NOMOVE))
				{
					OnMoved(Math::Vector2i{windowChangeInfo.x, windowChangeInfo.y});
				}
				if ((!(windowChangeInfo.flags & SWP_NOSIZE)) | (windowChangeInfo.flags & SWP_FRAMECHANGED))
				{
					Assert(m_stateFlags.IsSet(StateFlags::IsAwaitingResize));

					RECT clientArea;
					GetClientRect(static_cast<HWND>(m_pWindowHandle), &clientArea);

					const Math::Vector2i newSize = {clientArea.right - clientArea.left, clientArea.bottom - clientArea.top};
					TryOrQueueResize((Math::Vector2ui)newSize);
				}

				return 0;
			}
			case WM_WINDOWPOSCHANGING:
			{
				WINDOWPOS& windowChangeInfo = *reinterpret_cast<WINDOWPOS*>(lParam);

				if (!(windowChangeInfo.flags & SWP_NOSIZE) | (windowChangeInfo.flags & SWP_FRAMECHANGED))
				{
					RECT windowArea;
					GetWindowRect(static_cast<HWND>(m_pWindowHandle), &windowArea);

					if ((windowChangeInfo.cx != (windowArea.right - windowArea.left)) | (windowChangeInfo.cy != (windowArea.bottom - windowArea.top)))
					{
						// Notify that the window is about to change, and block rendering
						if (!TryStartResizing())
						{
							// Don't allow resizing
							windowChangeInfo.flags &= ~SWP_FRAMECHANGED;
							windowChangeInfo.flags |= SWP_NOSIZE;
							return DefWindowProc(reinterpret_cast<HWND>(m_pWindowHandle), message, wParam, lParam);
						}
					}
				}

				return 0;
			}
			case WM_GETMINMAXINFO:
			{
				LPMINMAXINFO lpMMI = (LPMINMAXINFO)lParam;
				lpMMI->ptMinTrackSize.x = 300;
				lpMMI->ptMinTrackSize.y = 300;
				return 0;
			}
			case WM_ERASEBKGND:
				return 1;
			case WM_SETFOCUS:
				OnReceivedKeyboardFocus();
				break;
			case WM_KILLFOCUS:
				OnLostKeyboardFocus();
				break;
			case WM_SETCURSOR:
				::SetCursor(s_cursorHandle);
				return true;
			case WM_SHOWWINDOW:
			{
				if (wParam)
				{
					OnBecomeVisible();
				}
				else
				{
					OnBecomeHidden();
				}
			}
			break;
			case WM_INPUT:
			{
				if ((GetMessageExtraInfo() & 0x82) == 0x82)
				{
					/* ignore event */
					return DefWindowProcW(reinterpret_cast<HWND>(m_pWindowHandle), message, wParam, lParam);
				}

				System::Get<Input::Manager>().HandleWindowsRawInput(lParam);
				return DefWindowProc(reinterpret_cast<HWND>(m_pWindowHandle), message, wParam, lParam);
			}
			case WM_POINTERDOWN:
				m_pointerCount++;
				AddPointerInteractionContext(reinterpret_cast<HINTERACTIONCONTEXT>(m_pInteractionContext), GET_POINTERID_WPARAM(wParam));
				ProcessPointerFrames(wParam);
				break;
			case WM_POINTERUP:
				m_pointerCount--;
				RemovePointerInteractionContext(reinterpret_cast<HINTERACTIONCONTEXT>(m_pInteractionContext), GET_POINTERID_WPARAM(wParam));
				ProcessPointerFrames(wParam);
				break;
			case WM_POINTERUPDATE:
			case WM_POINTERCAPTURECHANGED:
				ProcessPointerFrames(wParam);
				break;
			case WM_NCCALCSIZE:
			{
				if (wParam == TRUE)
				{
					return 0;
				}
				break;
			}
			case WM_NCACTIVATE:
				return 1;
			case WM_NCPAINT:
				return 0;
			case WM_NCHITTEST:
			{
				const LRESULT hit = DefWindowProc(static_cast<HWND>(m_pWindowHandle), WM_NCHITTEST, wParam, lParam);
				if (hit == HTCLIENT)
				{
					const Math::Vector2i localCoordinate = Math::Vector2i{LOWORD(lParam), HIWORD(lParam)} - GetWindowAreaPosition(m_pWindowHandle);
					const Math::Vector2ui clientAreaSize = GetWindowClientAreaSize(m_pWindowHandle);
					// Assert(localCoordinate.x >= GetPosition().x && localCoordinate.y >= GetPosition().y);

					const Math::Vector2i resizingArea = {GetSystemMetrics(SM_CXSIZEFRAME), GetSystemMetrics(SM_CYSIZEFRAME)};
					const Math::Vector2i relativeToSize = (Math::Vector2i)clientAreaSize - localCoordinate;

					const bool canResizeLeft = localCoordinate.x <= resizingArea.x;
					const bool canResizeTop = localCoordinate.y <= resizingArea.y;
					const bool canResizeRight = relativeToSize.x >= 0 && relativeToSize.x <= resizingArea.x;
					const bool canResizeBottom = relativeToSize.y >= 0 && relativeToSize.y <= resizingArea.y;

					if (canResizeLeft & canResizeTop)
					{
						return HTTOPLEFT;
					}
					if (canResizeRight & canResizeTop)
					{
						return HTTOPRIGHT;
					}
					if (canResizeLeft & canResizeBottom)
					{
						return HTBOTTOMLEFT;
					}
					if (canResizeRight & canResizeBottom)
					{
						return HTBOTTOMRIGHT;
					}
					if (canResizeLeft)
					{
						return HTLEFT;
					}
					if (canResizeTop)
					{
						return HTTOP;
					}
					if (canResizeRight)
					{
						return HTRIGHT;
					}
					if (canResizeBottom)
					{
						return HTBOTTOM;
					}

					if (IsCaption(localCoordinate))
					{
						return HTCAPTION;
					}
				}

				return hit;
			}
			case WM_DWMCOMPOSITIONCHANGED:
			{
				MARGINS margins = {1, 1, 1, 1};
				::DwmExtendFrameIntoClientArea(static_cast<HWND>(m_pWindowHandle), &margins);

				return 0;
			}
			default:
				return DefWindowProc(static_cast<HWND>(m_pWindowHandle), message, wParam, lParam);
		}

		return 0;
	}
#elif PLATFORM_ANDROID
	/* static */ void Window::ProcessWindowMessages()
	{
		s_windowThreadUtilities.ProcessInvokeQueue();
	}
#endif

	bool Window::IsCaption(const Math::Vector2i location)
	{
#if PLATFORM_WINDOWS
		const int moveArea = GetSystemMetrics(SM_CYCAPTION);
		return location.y <= moveArea;
#else
		UNUSED(location);
		return false;
#endif
	}

	void Window::StartResizingInternal(const Math::Vector2ui newClientAreaSize)
	{
		Assert(m_stateFlags.IsSet(StateFlags::IsResizing));

		auto resize = [this, newClientAreaSize]()
		{
			OnStartResizing();

			m_clientAreaSize = newClientAreaSize;

#if PLATFORM_EMSCRIPTEN
			Math::Vector2d canvasSize = (Math::Vector2d)newClientAreaSize;
			canvasSize /= Math::Vector2d{emscripten_get_device_pixel_ratio()};
			emscripten_set_canvas_element_size(m_canvasSelector.GetZeroTerminated(), (int)canvasSize.x, (int)canvasSize.y);
#elif PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
			UIWindow* window = (__bridge UIWindow*)m_pUIWindow;
			MetalView* view = (MetalView*)window.rootViewController.view;
			[view onViewportResizingFinished:CGSizeMake(newClientAreaSize.x, newClientAreaSize.y)];
#elif PLATFORM_APPLE_MACOS
			NSWindow* window = (__bridge NSWindow*)m_pNSWindow;
			MetalView* view = (MetalView*)window.contentViewController.view;
			[view onViewportResizingFinished:CGSizeMake(newClientAreaSize.x, newClientAreaSize.y)];
#endif

			m_dotsPerInch = GetDotsPerInchInternal();
			m_devicePixelRatio = GetDevicePixelRatioInternal();
			m_maximumDisplayRefreshRate = GetMaximumScreenRefreshRateInternal();
			m_safeAreaInsets = GetSafeAreaInsetsInternal();

			OnFinishedResizing();

			const EnumFlags<StateFlags> previousFlags = m_stateFlags.FetchAnd(~(StateFlags::IsResizing | StateFlags::IsResizingQueued));
			Assert(previousFlags.IsSet(StateFlags::IsResizing));
			if (previousFlags.IsSet(StateFlags::IsResizingQueued))
			{
				TryOrQueueResizeInternal(m_queuedNewSize);
			}

			m_stateFlags &= ~StateFlags::IsAwaitingResize;
		};

		if (m_stateFlags.IsSet(StateFlags::IsAwaitingResize))
		{
			resize();
		}
		else
		{
			System::Get<Engine>().ModifyFrameGraph(
				[resize]()
				{
					resize();
				}
			);
		}
	}

	SurfaceWindowHandle Window::GetSurfaceHandle() const
	{
#if PLATFORM_APPLE
		return (__bridge void*)(__bridge CAMetalLayer*)(m_pMetalLayer);
#elif PLATFORM_WINDOWS
		return m_pWindowHandle;
#elif USE_SDL
		return m_pSDLWindow;
#elif PLATFORM_ANDROID
		return m_pGameActivityWindow->GetNativeWindow();
#elif PLATFORM_EMSCRIPTEN
		return (void*)m_canvasSelector.GetZeroTerminated().GetData();
#else
#error "Unknown platform"
#endif
	}

	Window::NativeType Window::GetNativeType() const
	{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		return NativeType::UIKit;
#elif PLATFORM_APPLE_MACOS
		return NativeType::AppKit;
#elif PLATFORM_WINDOWS
		return NativeType::Win32;
#elif PLATFORM_ANDROID
		return NativeType::AndroidNative;
#elif PLATFORM_WEB
		return NativeType::Web;
#elif PLATFORM_LINUX
		const char* currentVideoDriver = SDL_GetCurrentVideoDriver();
		const ConstZeroTerminatedStringView currentVideoDriverView{currentVideoDriver, (uint32)strlen(currentVideoDriver)};
#if defined(SDL_VIDEO_DRIVER_X11)
		if (currentVideoDriverView == ConstStringView{"x11"})
		{
			return NativeType::X11;
		}
#endif
#if defined(SDL_VIDEO_DRIVER_WAYLAND)
		if (currentVideoDriverView == ConstStringView{"wayland"})
		{
			return NativeType::Wayland;
		}
#endif

		return NativeType::Invalid;
#else
#error "Unknown platform"
#endif
	}

	void* Window::GetOSHandle() const
	{
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		return m_pUIWindow;
#elif PLATFORM_APPLE_MACOS
		return m_pNSWindow;
#elif PLATFORM_WINDOWS
		return m_pWindowHandle;
#elif PLATFORM_ANDROID
		return m_pGameActivityWindow->GetNativeWindow();
#elif PLATFORM_EMSCRIPTEN
		Assert(false, "TODO");
		return nullptr;
#elif USE_SDL
		SDL_SysWMinfo wmInfo;
		SDL_VERSION(&wmInfo.version);
		SDL_GetWindowWMInfo(m_pSDLWindow, &wmInfo);
#if PLATFORM_WINDOWS
		return wmInfo.info.win.window;
#elif PLATFORM_APPLE_MACOS
		return (__bridge void*)wmInfo.info.cocoa.window;
#elif PLATFORM_LINUX
		switch (GetNativeType())
		{
			case NativeType::Invalid:
				AssertMessage(false, "Unsupported Linux window manager");
				return nullptr;
			case NativeType::X11:
#if defined(SDL_VIDEO_DRIVER_X11)
				return reinterpret_cast<void*>(wmInfo.info.x11.window);
#else
				return nullptr;
#endif
			case NativeType::Wayland:
#if defined(SDL_VIDEO_DRIVER_WAYLAND)
				return wmInfo.info.wl.surface;
#else
				return nullptr;
#endif
			case NativeType::UIKit:
			case NativeType::AppKit:
			case NativeType::AndroidNative:
			case NativeType::Win32:
			case NativeType::Web:
				ExpectUnreachable();
		}
		ExpectUnreachable();
#endif
#else
#error "Unknown platform"
#endif
	}

#if !PLATFORM_APPLE_VISIONOS
	/* static */ Math::Vector2ui Window::GetMainScreenUsableBounds()
	{
#if PLATFORM_APPLE_IOS
		CGRect screenBounds = [[UIScreen mainScreen] bounds];
		CGFloat screenScale = [[UIScreen mainScreen] nativeScale];
		CGSize screenSize = CGSizeMake(screenBounds.size.width * screenScale, screenBounds.size.height * screenScale);

		const CGFloat dockHeightEstimate = 130 * screenScale * PLATFORM_APPLE_MACCATALYST;

		return {(uint32)screenSize.width, (uint32)(screenSize.height - dockHeightEstimate)};
#elif PLATFORM_APPLE_MACOS
		CGRect screenBounds = [[NSScreen mainScreen] visibleFrame];
		return {(uint32)screenBounds.size.width, (uint32)(screenBounds.size.height)};
#elif USE_SDL
		SDL_Rect mainWindowRectangle;
		SDL_GetDisplayUsableBounds(0, &mainWindowRectangle);

		return {(uint32)mainWindowRectangle.w, (uint32)mainWindowRectangle.h};
#elif PLATFORM_WINDOWS
		const POINT ptZero = {0, 0};
		const HMONITOR monitorHandle = MonitorFromPoint(ptZero, MONITOR_DEFAULTTOPRIMARY);
		MONITORINFO monitorInfo;
		monitorInfo.cbSize = sizeof(monitorInfo);
		GetMonitorInfo(monitorHandle, &monitorInfo);

		return {uint32(monitorInfo.rcWork.right - monitorInfo.rcWork.left), uint32(monitorInfo.rcWork.bottom - monitorInfo.rcWork.top)};
#elif PLATFORM_ANDROID || PLATFORM_EMSCRIPTEN
		return Math::Zero;
#else
#error "Unknown platform"
#endif
	}
#endif

#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
	[[nodiscard]] float GetiOSScreenDiagonalInInches()
	{
		switch (Platform::GetDeviceModel())
		{
			case Platform::DeviceModel::iPhone4S:
				return 3.5f;
			case Platform::DeviceModel::iPhone5:
			case Platform::DeviceModel::iPhone5C:
			case Platform::DeviceModel::iPhone5S:
			case Platform::DeviceModel::iPhoneSE:
			case Platform::DeviceModel::iPodTouch5:
			case Platform::DeviceModel::iPodTouch6:
			case Platform::DeviceModel::iPodTouch7:
				return 4.0f;
			case Platform::DeviceModel::iPhone6:
			case Platform::DeviceModel::iPhone6S:
			case Platform::DeviceModel::iPhone7:
			case Platform::DeviceModel::iPhone8:
			case Platform::DeviceModel::iPhoneSE2:
			case Platform::DeviceModel::iPhoneSE3:
				return 4.7f;
			case Platform::DeviceModel::iPhone12Mini:
			case Platform::DeviceModel::iPhone13Mini:
				return 5.4f;
			case Platform::DeviceModel::iPhone6Plus:
			case Platform::DeviceModel::iPhone6SPlus:
			case Platform::DeviceModel::iPhone7Plus:
			case Platform::DeviceModel::iPhone8Plus:
				return 5.5f;
			case Platform::DeviceModel::iPhoneX:
			case Platform::DeviceModel::iPhoneXS:
			case Platform::DeviceModel::iPhone11Pro:
				return 5.8f;
			case Platform::DeviceModel::iPhoneXR:
			case Platform::DeviceModel::iPhone11:
			case Platform::DeviceModel::iPhone12:
			case Platform::DeviceModel::iPhone12Pro:
			case Platform::DeviceModel::iPhone13:
			case Platform::DeviceModel::iPhone13Pro:
			case Platform::DeviceModel::iPhone14:
			case Platform::DeviceModel::iPhone14Pro:
			case Platform::DeviceModel::iPhone15:
			case Platform::DeviceModel::iPhone15Pro:
			case Platform::DeviceModel::iPhone16:
				return 6.1f;
			case Platform::DeviceModel::iPhone16Pro:
				return 6.3f;
			case Platform::DeviceModel::iPhoneXSMax:
			case Platform::DeviceModel::iPhone11ProMax:
				return 6.5f;
			case Platform::DeviceModel::iPhone12ProMax:
			case Platform::DeviceModel::iPhone13ProMax:
			case Platform::DeviceModel::iPhone14Plus:
			case Platform::DeviceModel::iPhone14ProMax:
			case Platform::DeviceModel::iPhone15Plus:
			case Platform::DeviceModel::iPhone15ProMax:
			case Platform::DeviceModel::iPhone16Plus:
				return 6.7f;
			case Platform::DeviceModel::iPhone16ProMax:
				return 6.9f;
			case Platform::DeviceModel::iPadMini:
			case Platform::DeviceModel::iPadMini2:
			case Platform::DeviceModel::iPadMini3:
			case Platform::DeviceModel::iPadMini4:
			case Platform::DeviceModel::iPadMini5:
				return 7.9f;
			case Platform::DeviceModel::iPadMini6:
				return 8.3f;
			case Platform::DeviceModel::iPad2:
			case Platform::DeviceModel::iPad3:
			case Platform::DeviceModel::iPad4:
			case Platform::DeviceModel::iPadAir:
			case Platform::DeviceModel::iPadAir2:
			case Platform::DeviceModel::iPadPro_9_7:
			case Platform::DeviceModel::iPad5:
			case Platform::DeviceModel::iPad6:
				return 9.7f;
			case Platform::DeviceModel::iPad7:
			case Platform::DeviceModel::iPad8:
			case Platform::DeviceModel::iPad9:
				return 10.2f;
			case Platform::DeviceModel::iPadPro_10_5:
			case Platform::DeviceModel::iPadAir3:
				return 10.5f;
			case Platform::DeviceModel::iPadAir4:
			case Platform::DeviceModel::iPadAir5:
			case Platform::DeviceModel::iPad10:
				return 10.9f;
			case Platform::DeviceModel::iPadPro_11:
			case Platform::DeviceModel::iPadPro_11_2nd:
			case Platform::DeviceModel::iPadPro_11_3rd:
			case Platform::DeviceModel::iPadPro_11_4th:
				return 11.f;
			case Platform::DeviceModel::iPadPro_12_9:
			case Platform::DeviceModel::iPadPro_12_9_2nd:
			case Platform::DeviceModel::iPadPro_12_9_3rd:
			case Platform::DeviceModel::iPadPro_12_9_4th:
			case Platform::DeviceModel::iPadPro_12_9_5th:
			case Platform::DeviceModel::iPadPro_12_9_6th:
				return 12.9f;

			case Platform::DeviceModel::MacBookAir11_2012:
			case Platform::DeviceModel::MacBookAir11_2013:
			case Platform::DeviceModel::MacBookAir11_2015:
				return 11.6f;

			case Platform::DeviceModel::MacBookAir13_2012:
			case Platform::DeviceModel::MacBookAir13_2013:
			case Platform::DeviceModel::MacBookAir13_2015:
			case Platform::DeviceModel::MacBookAir13_2018:
			case Platform::DeviceModel::MacBookAir13_2019:
			case Platform::DeviceModel::MacBookAir13_2020:
			case Platform::DeviceModel::MacBookAir13_M1_2020:
			case Platform::DeviceModel::MacBookPro13_2012:
			case Platform::DeviceModel::MacBookPro13_2013:
			case Platform::DeviceModel::MacBookPro13_2014:
			case Platform::DeviceModel::MacBookPro13_2015:
			case Platform::DeviceModel::MacBookPro13_2016:
			case Platform::DeviceModel::MacBookPro13_2017:
			case Platform::DeviceModel::MacBookPro13_2018:
			case Platform::DeviceModel::MacBookPro13_2020:
			case Platform::DeviceModel::MacBookPro13_M1_2020:
			case Platform::DeviceModel::MacBookPro13_M2:
				return 13.3f;

			case Platform::DeviceModel::MacBookAir13_M2:
				return 13.6f;

			case Platform::DeviceModel::MacBookPro15_2012:
			case Platform::DeviceModel::MacBookPro15_2013:
			case Platform::DeviceModel::MacBookPro15_2014:
			case Platform::DeviceModel::MacBookPro15_2015:
			case Platform::DeviceModel::MacBookPro15_2016:
			case Platform::DeviceModel::MacBookPro15_2017:
			case Platform::DeviceModel::MacBookPro15_2018:
				return 15.4f;

			case Platform::DeviceModel::MacBookPro14_2021:
				return 14.2f;

			case Platform::DeviceModel::MacBookPro16_2019:
			case Platform::DeviceModel::MacBookPro16_2021:
				return 16.2f;

			case Platform::DeviceModel::MacBook12_2015:
			case Platform::DeviceModel::MacBook12_2016:
			case Platform::DeviceModel::MacBook12_2017:
				return 12.f;

			case Platform::DeviceModel::MacMini_5_2012:
			case Platform::DeviceModel::MacMini_2012:
			case Platform::DeviceModel::MacMini_2014:
			case Platform::DeviceModel::MacMini_2018:
			case Platform::DeviceModel::MacMini_2020:
			case Platform::DeviceModel::MacStudio_M1Max:
			case Platform::DeviceModel::MacStudio_M1Ultra:
			case Platform::DeviceModel::MacStudio_M2Max:
			case Platform::DeviceModel::MacStudio_M2Ultra:
				return 0.f;

			case Platform::DeviceModel::iMac21_5_2012:
			case Platform::DeviceModel::iMac21_5_2013:
			case Platform::DeviceModel::iMac21_5_2014:
			case Platform::DeviceModel::iMac21_5_2015:
			case Platform::DeviceModel::iMac21_5_2017:
			case Platform::DeviceModel::iMac21_5_2019:
				return 21.5f;

			case Platform::DeviceModel::iMac27_2012:
			case Platform::DeviceModel::iMac27_2013:
			case Platform::DeviceModel::iMac27_2014:
			case Platform::DeviceModel::iMac27_2015:
			case Platform::DeviceModel::iMac27_2017:
			case Platform::DeviceModel::iMacPro27_2017:
			case Platform::DeviceModel::iMac27_2019:
			case Platform::DeviceModel::iMac24_2021:
				return 27.f;

			case Platform::DeviceModel::iMac27_2020:
				return 23.5f;

			case Platform::DeviceModel::UnknowniPad:
				Assert(false, "Unsupported iPad detected, provide screen metrics!");
				return 12.9f;

			case Platform::DeviceModel::UnknowniPhone:
				Assert(false, "Unsupported iPhone detected, provide screen metrics!");
				return 6.1f;

			case Platform::DeviceModel::UnknowniPod:
				Assert(false, "Unsupported iPod detected, provide screen metrics!");
				return 4.0;

			case Platform::DeviceModel::UnknownMacBook:
				Assert(false, "Unsupported MacBook detected, provide screen metrics!");
				return 0.f;

			case Platform::DeviceModel::UnknownMacBookAir:
				Assert(false, "Unsupported MacBook Air detected, provide screen metrics!");
				return 0.f;

			case Platform::DeviceModel::UnknownMacBookPro:
				Assert(false, "Unsupported MacBook Pro detected, provide screen metrics!");
				return 0.f;

			case Platform::DeviceModel::UnknownMac:
				Assert(false, "Unsupported Mac detected, provide screen metrics!");
				return 0.f;

			case Platform::DeviceModel::UnknownMacMini:
				Assert(false, "Unsupported Mac mini detected, provide screen metric!");
				return 0.f;

			case Platform::DeviceModel::UnknowniMac:
				Assert(false, "Unsupported iMac detected, provide screen metrics!");
				return 0.f;

			case Platform::DeviceModel::UnknownApple:
				Assert(false, "Unsupported Apple Device detected, provide screen metrics!");
				return 0.f;

			case Platform::DeviceModel::VisionPro:
			case Platform::DeviceModel::UnknownVision:
			case Platform::DeviceModel::UnknownWindows:
			case Platform::DeviceModel::UnknownAndroid:
			case Platform::DeviceModel::UnknownLinux:
			case Platform::DeviceModel::UnknownWeb:
				ExpectUnreachable();
		}

		return 0.f;
	}
#endif

	float Window::GetDotsPerInchInternal() const
	{
#if PLATFORM_APPLE_MACCATALYST
		float result{0.f};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, &result]()
			{
				UIWindow* pWindow = (__bridge UIWindow*)m_pUIWindow;
				UIScreen* screen = [pWindow screen];
				if (screen == nil)
				{
					screen = pWindow.windowScene.screen;
				}

				const uint32 displayID = (uint32)[(NSNumber*)[screen valueForKey:@"_cgsDisplayId"] unsignedIntegerValue];

				const CGSize displayPhysicalSize = CGDisplayScreenSize(displayID);
				const Math::Lengthd displayPhysicalDiagonal = Math::Lengthd::FromMillimeters(
					Math::Sqrt(displayPhysicalSize.width * displayPhysicalSize.width + displayPhysicalSize.height * displayPhysicalSize.height)
				);
				const CGFloat screenScale = [screen scale];
				const CGSize displayResolution = NSMakeSize(screen.bounds.size.width * screenScale, screen.bounds.size.height * screenScale);
				const double displayResolutionDiagonal =
					Math::Sqrt(displayResolution.width * displayResolution.width + displayResolution.height * displayResolution.height);

				const float dotsPerInch = float(displayResolutionDiagonal / displayPhysicalDiagonal.GetInches());
				result = dotsPerInch;
			}
		);
		return result;
#elif PLATFORM_APPLE_MACOS
		float result{0.f};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, &result]()
			{
				NSWindow* pWindow = (__bridge NSWindow*)m_pNSWindow;
				NSScreen* screen = [pWindow screen];

				NSDictionary* screenDictionary = [screen deviceDescription];
				NSNumber* screenID = [screenDictionary objectForKey:@"NSScreenNumber"];
				CGDirectDisplayID aID = [screenID unsignedIntValue];

				const uint32 displayID = (uint32)aID;

				const CGSize displayPhysicalSize = CGDisplayScreenSize(displayID);
				const Math::Lengthd displayPhysicalDiagonal = Math::Lengthd::FromMillimeters(
					Math::Sqrt(displayPhysicalSize.width * displayPhysicalSize.width + displayPhysicalSize.height * displayPhysicalSize.height)
				);
				const CGFloat scale = pWindow.backingScaleFactor;
				const CGSize displayResolution = NSMakeSize(screen.frame.size.width * scale, screen.frame.size.height * scale);
				const double displayResolutionDiagonal =
					Math::Sqrt(displayResolution.width * displayResolution.width + displayResolution.height * displayResolution.height);

				const float dotsPerInch = float(displayResolutionDiagonal / displayPhysicalDiagonal.GetInches());
				result = dotsPerInch;
			}
		);
		return result;
#elif PLATFORM_APPLE_VISIONOS
		return 192.f;
#elif PLATFORM_APPLE_IOS
		float result{0.f};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, &result]()
			{
				UIWindow* pWindow = (__bridge UIWindow*)m_pUIWindow;
				UIScreen* screen = [pWindow screen];
				if (screen == nil)
				{
					screen = pWindow.windowScene.screen;
				}
				const CGSize nativeScreenSize = [screen nativeBounds].size;
				const float nativeScreenDiagonal =
					Math::Sqrt(float(nativeScreenSize.width * nativeScreenSize.width + nativeScreenSize.height * nativeScreenSize.height));
				const float dotsPerInch = nativeScreenDiagonal / GetiOSScreenDiagonalInInches();
				result = dotsPerInch;
			}
		);
		return result;
#elif USE_SDL
		float diagonalDotsPerInch{96.f};
		const int displayIndex = SDL_GetWindowDisplayIndex(m_pSDLWindow);
		Assert(displayIndex >= 0, "Failed to get display index");
		[[maybe_unused]] const int result = SDL_GetDisplayDPI(displayIndex, &diagonalDotsPerInch, nullptr, nullptr);
		Assert(result == 0, "Failed to get display DPI");
		return diagonalDotsPerInch;
#elif PLATFORM_WINDOWS
		HMONITOR monitorHandle = MonitorFromWindow(static_cast<HWND>(m_pWindowHandle), MONITOR_DEFAULTTONEAREST);

		Math::TVector2<uint32> dpi;
		if (GetDpiForMonitor(monitorHandle, MDT_EFFECTIVE_DPI, &dpi.x, &dpi.y) == S_OK)
		{
			return (float)dpi.x;
		}

		const uint32 dotsPerInch = GetDpiForWindow(static_cast<HWND>(m_pWindowHandle));
		return (float)dotsPerInch;
#elif PLATFORM_ANDROID
		return m_pGameActivityWindow->GetDotsPerInch();
#elif PLATFORM_EMSCRIPTEN
		return (float)emscripten_get_device_pixel_ratio() * 96.f;
#else
#error "Unknown platform"
#endif
	}

	float Window::GetDevicePixelRatioInternal() const
	{
#if PLATFORM_APPLE_VISIONOS
		return VisionOSUIScalingFactor;
#elif PLATFORM_APPLE_IOS
		float result{0.f};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, &result]()
			{
				UIWindow* pWindow = (__bridge UIWindow*)m_pUIWindow;
				UIScreen* screen = [pWindow screen];
				if (screen == nil)
				{
					screen = pWindow.windowScene.screen;
				}
				result = (float)[screen scale];
			}
		);
		return result;
#elif PLATFORM_APPLE_MACOS
		float result{0.f};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, &result]()
			{
				NSWindow* pWindow = (__bridge NSWindow*)m_pNSWindow;
				result = (float)pWindow.backingScaleFactor;
			}
		);
		return result;
#elif PLATFORM_WINDOWS && !USE_SDL
		HMONITOR monitorHandle = MonitorFromWindow(static_cast<HWND>(m_pWindowHandle), MONITOR_DEFAULTTONEAREST);
		DEVICE_SCALE_FACTOR scaleFactor;
		if (GetScaleFactorForMonitor(monitorHandle, &scaleFactor) == S_OK)
		{
			return ((float)scaleFactor / 100.f);
		}
		return 1.f;
#elif PLATFORM_EMSCRIPTEN
		return (float)emscripten_get_device_pixel_ratio();
#elif PLATFORM_ANDROID
		return (float)Math::Round(GetDotsPerInch() / 100.f);
#elif USE_SDL
		Math::Vector2i windowSize{Math::Zero};
		Math::Vector2i windowDrawableSize{Math::Zero};
		SDL_GetWindowSize(m_pSDLWindow, &windowSize.x, &windowSize.y);
		SDL_GL_GetDrawableSize(m_pSDLWindow, &windowDrawableSize.x, &windowDrawableSize.y);
		return (float)windowDrawableSize.x / (float)windowSize.x;
#else
		return GetDotsPerInch() / 96.f;
#endif
	}

	float Window::GetPhysicalDevicePixelRatioInternal() const
	{
#if PLATFORM_APPLE_VISIONOS
		return VisionOSWindowScalingFactor;
#elif PLATFORM_APPLE_IOS
		float result{0.f};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, &result]()
			{
				UIWindow* pWindow = (__bridge UIWindow*)m_pUIWindow;
				UIScreen* screen = [pWindow screen];
				if (screen == nil)
				{
					screen = pWindow.windowScene.screen;
				}
				result = (float)[screen nativeScale];
			}
		);
		return result;
#elif PLATFORM_APPLE_MACOS
		float result{0.f};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, &result]()
			{
				NSWindow* pWindow = (__bridge NSWindow*)m_pNSWindow;
				result = (float)pWindow.backingScaleFactor;
			}
		);
		return result;
#elif PLATFORM_WINDOWS && !USE_SDL
		HMONITOR monitorHandle = MonitorFromWindow(static_cast<HWND>(m_pWindowHandle), MONITOR_DEFAULTTONEAREST);
		DEVICE_SCALE_FACTOR scaleFactor;
		if (GetScaleFactorForMonitor(monitorHandle, &scaleFactor) == S_OK)
		{
			return ((float)scaleFactor / 100.f);
		}
		return 1.f;
#elif PLATFORM_EMSCRIPTEN
		return (float)emscripten_get_device_pixel_ratio();
#elif USE_SDL
		float diagonalDotsPerInch{96.f};
		[[maybe_unused]] int result = SDL_GetDisplayDPI(SDL_GetWindowDisplayIndex(m_pSDLWindow), &diagonalDotsPerInch, nullptr, nullptr);

		SDL_DisplayMode mode;
		result = SDL_GetCurrentDisplayMode(0, &mode);
		Assert(result == 0);

		const float physicalWidthInInches = (float)mode.w / diagonalDotsPerInch;
		return static_cast<float>(mode.w) / (physicalWidthInInches * 96.0f);
#else
		return GetDotsPerInch() / 96.f;
#endif
	}

	uint16 Window::GetMaximumScreenRefreshRateInternal() const
	{
#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
		uint16 result{0};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, &result]()
			{
				UIWindow* pWindow = (__bridge UIWindow*)m_pUIWindow;
				UIScreen* screen = [pWindow screen];
				if (screen == nil)
				{
					screen = pWindow.windowScene.screen;
				}
				result = (uint16)[screen maximumFramesPerSecond];
			}
		);
		return result;
#elif PLATFORM_APPLE_MACOS
		uint16 result{0};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, &result]()
			{
				NSWindow* pWindow = (__bridge NSWindow*)m_pNSWindow;
				NSScreen* screen = [pWindow screen];
				result = (uint16)[screen maximumFramesPerSecond];
			}
		);
		return result;
#elif PLATFORM_WINDOWS && !USE_SDL
		HMONITOR monitorHandle = MonitorFromWindow(static_cast<HWND>(m_pWindowHandle), MONITOR_DEFAULTTONEAREST);

		MONITORINFOEX monitorInfo;
		monitorInfo.cbSize = sizeof(monitorInfo);
		if (GetMonitorInfo(monitorHandle, &monitorInfo))
		{
			DEVMODE devMode;
			devMode.dmSize = sizeof(devMode);
			devMode.dmDriverExtra = 0;
			if (EnumDisplaySettings(monitorInfo.szDevice, ENUM_CURRENT_SETTINGS, &devMode))
			{
				return (uint16)devMode.dmDisplayFrequency;
			}
		}
		return Math::NumericLimits<uint16>::Max;
#else
		// Assert(false, "Implement for current platform!");
		return 60; // Math::NumericLimits<uint16>::Max;
#endif
	}

	Math::RectangleEdgesf Window::GetSafeAreaInsetsInternal() const
	{
		Math::RectangleEdgesf safeArea{0, 0, 0, 0};
#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, &safeArea]()
			{
				UIWindow* pWindow = (__bridge UIWindow*)m_pUIWindow;
				UIEdgeInsets safeAreaInsets = pWindow.safeAreaInsets;
				UIScreen* screen = pWindow.screen;
				UIWindowScene* __strong windowScene = pWindow.windowScene;
				if (screen == nil)
				{
					screen = windowScene.screen;
				}

				const float contentScaleFactor = (float)screen.nativeScale;
				safeAreaInsets.left *= contentScaleFactor;
				safeAreaInsets.top *= contentScaleFactor;
				safeAreaInsets.right *= contentScaleFactor;
				safeAreaInsets.bottom *= contentScaleFactor;

				switch (windowScene.interfaceOrientation)
				{
					case UIInterfaceOrientationLandscapeLeft:
						safeArea.m_right = (float)safeAreaInsets.right;
						break;
					case UIInterfaceOrientationLandscapeRight:
						safeArea.m_left = (float)safeAreaInsets.left;
						break;
					case UIInterfaceOrientationPortrait:
					case UIInterfaceOrientationPortraitUpsideDown:
						break;
					case UIInterfaceOrientationUnknown:
						break;
				}

				safeArea.m_top = (float)safeAreaInsets.top;
				safeArea.m_bottom = (float)safeAreaInsets.bottom;
			}
		);
#elif PLATFORM_APPLE_MACOS
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[this, &safeArea]()
			{
				NSWindow* pWindow = (__bridge NSWindow*)m_pNSWindow;
				NSEdgeInsets safeAreaInsets = pWindow.contentView.safeAreaInsets;
				NSScreen* screen = pWindow.screen;

				const float contentScaleFactor = (float)screen.backingScaleFactor;
				safeAreaInsets.left *= contentScaleFactor;
				safeAreaInsets.top *= contentScaleFactor;
				safeAreaInsets.right *= contentScaleFactor;
				safeAreaInsets.bottom *= contentScaleFactor;

				safeArea.m_left = (float)safeAreaInsets.left;
				safeArea.m_right = (float)safeAreaInsets.right;
				safeArea.m_top = (float)safeAreaInsets.top;
				safeArea.m_bottom = (float)safeAreaInsets.bottom;
			}
		);
#endif
		return safeArea;
	}

	ScreenProperties Window::GetCurrentScreenProperties() const
	{
		const Math::Vector2ui clientAreaSize = GetClientAreaSize();
		return {GetDotsPerInch(), GetDevicePixelRatio(), GetSafeAreaInsets(), clientAreaSize, m_maximumDisplayRefreshRate};
	}

	void Window::SetDisallowedScreenOrientations(const EnumFlags<OrientationFlags> disallowedOrientations)
	{
		m_disallowedDeviceOrientations |= disallowedOrientations;

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
		Window::QueueOnWindowThread(
			[this]()
			{
#if !PLATFORM_APPLE_VISIONOS
				[UIViewController attemptRotationToDeviceOrientation];
#endif

				UIDevice* currentDevice = [UIDevice currentDevice];
				UIWindow* pWindow = (__bridge UIWindow*)m_pUIWindow;
				UIWindowScene* __strong windowScene = pWindow.windowScene;
				switch (windowScene.interfaceOrientation)
				{
					case UIInterfaceOrientationLandscapeLeft:
					case UIInterfaceOrientationLandscapeRight:
					{
						if (m_disallowedDeviceOrientations.AreAnySet(OrientationFlags::LandscapeAny))
						{
							[currentDevice setValue:@(UIDeviceOrientationPortrait) forKey:@"orientation"];
						}
					}
					break;
					case UIInterfaceOrientationPortrait:
					case UIInterfaceOrientationPortraitUpsideDown:
					{
						if (m_disallowedDeviceOrientations.AreAnySet(OrientationFlags::PortraitAny))
						{
							[currentDevice setValue:@(UIDeviceOrientationLandscapeLeft) forKey:@"orientation"];
						}
					}
					break;
					case UIInterfaceOrientationUnknown:
						break;
				}
			}
		);
#endif
	}

	void Window::ClearDisallowedScreenOrientations(const EnumFlags<OrientationFlags> disallowedOrientations)
	{
		m_disallowedDeviceOrientations &= ~disallowedOrientations;

#if PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS
		Window::QueueOnWindowThread(
			[]()
			{
				[UIViewController attemptRotationToDeviceOrientation];
			}
		);
#endif
	}

	Asset::Identifier Window::CreateDocument(
		[[maybe_unused]] const Guid assetTypeGuid,
		[[maybe_unused]] UnicodeString&& documentName,
		[[maybe_unused]] IO::Path&& documentPath,
		const EnumFlags<CreateDocumentFlags>,
		[[maybe_unused]] const Optional<const DocumentData*> pTemplateDocument
	)
	{
		Assert(false, "Not supported");
		return {};
	}

	Threading::JobBatch
	Window::OpenDocuments([[maybe_unused]] const ArrayView<const DocumentData> documents, const EnumFlags<OpenDocumentFlags>)
	{
		Assert(false, "Not supported");
		return {};
	}
}
