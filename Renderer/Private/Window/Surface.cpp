#include <Renderer/Window/Surface.h>

#include <Renderer/Renderer.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/WebGPU/Includes.h>
#include <Renderer/Window/Window.h>

#if USE_SDL
#include <SDL2/SDL.h>
#include <SDL2/SDL_vulkan.h>
#undef None
#undef Status
#undef DestroyAll
#undef ControlMask
#undef Bool
#undef Success
#endif

#if RENDERER_VULKAN
#if PLATFORM_WINDOWS
#include <Common/Platform/Windows.h>
#include <3rdparty/vulkan/vulkan_win32.h>
#elif PLATFORM_APPLE
#include <3rdparty/vulkan/vulkan_metal.h>
#elif PLATFORM_ANDROID
#include <3rdparty/vulkan/vulkan_android.h>
#endif
#endif

#include <Common/SourceLocation.h>
#include <Common/Math/Vector2.h>
#include <Common/System/Query.h>
#include <Common/IO/Log.h>

namespace ngine::Rendering
{
	Surface::Surface(
		[[maybe_unused]] const InstanceView instance,
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const SurfaceWindowHandle windowHandle
	)
#if RENDERER_METAL
		: SurfaceView((__bridge CAMetalLayer*)windowHandle)
#endif
	{
#if RENDERER_VULKAN

#if USE_SDL
		[[maybe_unused]] const bool success = SDL_Vulkan_CreateSurface(reinterpret_cast<SDL_Window*>(windowHandle), instance, &m_pSurface);
#elif PLATFORM_WINDOWS
		const VkWin32SurfaceCreateInfoKHR createInfo =
			{VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR, nullptr, 0, GetModuleHandle(nullptr), static_cast<HWND>(windowHandle)};
		vkCreateWin32SurfaceKHR(instance, &createInfo, nullptr, &m_pSurface);
#elif PLATFORM_APPLE
		VkMetalSurfaceCreateInfoEXT createInfo =
			{VK_STRUCTURE_TYPE_METAL_SURFACE_CREATE_INFO_EXT, nullptr, 0, (__bridge CAMetalLayer*)(windowHandle)};
		vkCreateMetalSurfaceEXT(instance, &createInfo, nullptr, &m_pSurface);
#elif PLATFORM_ANDROID
		VkAndroidSurfaceCreateInfoKHR
			createInfo{VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR, nullptr, 0, reinterpret_cast<ANativeWindow*>(windowHandle)};
		vkCreateAndroidSurfaceKHR(instance, &createInfo, nullptr, &m_pSurface);
#else
#error "Unsupported Vulkan platform"
#endif

#elif RENDERER_WEBGPU

#if PLATFORM_WEB
		WGPUSurfaceDescriptorFromCanvasHTMLSelector surfaceCreationDescriptor{};
		surfaceCreationDescriptor.chain.sType = WGPUSType_SurfaceDescriptorFromCanvasHTMLSelector;
		surfaceCreationDescriptor.selector = reinterpret_cast<const char*>(windowHandle);
#elif PLATFORM_APPLE
		WGPUSurfaceSourceMetalLayer surfaceCreationDescriptor{};
		surfaceCreationDescriptor.chain.sType = WGPUSType_SurfaceSourceMetalLayer;
		surfaceCreationDescriptor.layer = windowHandle;
#elif PLATFORM_WINDOWS
		WGPUSurfaceSourceWindowsHWND surfaceCreationDescriptor{};
		surfaceCreationDescriptor.chain.sType = WGPUSType_SurfaceSourceWindowsHWND;
		surfaceCreationDescriptor.hwnd = windowHandle;
#elif PLATFORM_ANDROID
		WGPUSurfaceSourceAndroidNativeWindow surfaceCreationDescriptor{};
		surfaceCreationDescriptor.chain.sType = WGPUSType_SurfaceSourceAndroidNativeWindow;
		surfaceCreationDescriptor.window = windowHandle;
#elif PLATFORM_LINUX
		SDL_Window* pSDLWindow = reinterpret_cast<SDL_Window*>(windowHandle);
		SDL_SysWMinfo wmInfo;
		SDL_VERSION(&wmInfo.version);
		SDL_GetWindowWMInfo(pSDLWindow, &wmInfo);

		union
		{
			WGPUSType_SurfaceSourceXlibWindow x11;
			WGPUSurfaceSourceWaylandSurface wayland;
		} surfaceCreationDescriptor;
		const char* currentVideoDriver = SDL_GetCurrentVideoDriver();
		const ConstZeroTerminatedStringView currentVideoDriverView{currentVideoDriver, (uint32)strlen(currentVideoDriver)};
#if defined(SDL_VIDEO_DRIVER_X11)
		if (currentVideoDriverView == ConstStringView{"x11"})
		{
			surfaceCreationDescriptor.x11.chain.sType = WGPUSType_SurfaceSourceXlibWindow;
			surfaceCreationDescriptor.x11.display = reinterpret_cast<void*>(wmInfo.info.x11.display);
			surfaceCreationDescriptor.x11.window = reinterpret_cast<uint64_t>(wmInfo.info.x11.window);
		}
#endif
#if defined(SDL_VIDEO_DRIVER_WAYLAND)
		if (currentVideoDriverView == ConstStringView{"wayland"})
		{
			surfaceCreationDescriptor.wayland.chain.sType = WGPUSType_SurfaceSourceWaylandSurface;
			surfaceCreationDescriptor.wayland.display = wmInfo.info.wl.display;
			surfaceCreationDescriptor.wayland.surface = wmInfo.info.wl.surface;
		}
#endif

#endif
		WGPUSurfaceDescriptor surfaceDescriptor{};
		surfaceDescriptor.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&surfaceCreationDescriptor);

		WGPUSurface pSurface;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[instance, surfaceDescriptor, &pSurface]()
			{
				pSurface = wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
#if RENDERER_WEBGPU_DAWN
				wgpuSurfaceAddRef(pSurface);
#else
				wgpuSurfaceReference(pSurface);
#endif
			}
		);
		m_pSurface = pSurface;
#endif
	}

	Surface& Surface::operator=([[maybe_unused]] Surface&& other) noexcept
	{
		Assert(IsValid(), "Destroy must be called before the surface is destroyed!");
#if RENDERER_VULKAN || RENDERER_WEBGPU
		m_pSurface = other.m_pSurface;
		other.m_pSurface = nullptr;
#endif
		return *this;
	}

	Surface::~Surface()
	{
		Assert(!IsValid(), "Destroy must be called before the surface is destroyed!");
	}

	void Surface::Destroy([[maybe_unused]] const InstanceView instance)
	{
#if RENDERER_VULKAN
		vkDestroySurfaceKHR(instance, m_pSurface, nullptr);
		m_pSurface = nullptr;
#elif RENDERER_WEBGPU
		if (m_pSurface != nullptr)
		{
			wgpuSurfaceRelease(m_pSurface);
			m_pSurface = nullptr;
		}
#endif
	}

#if RENDERER_VULKAN && PLATFORM_DESKTOP && !PLATFORM_APPLE
	/* static */ Surface
	Surface::CreateDirectToDisplay(const InstanceView instance, const PhysicalDevice& physicalDevice, const Math::Vector2ui resolution)
	{
		uint32_t displayPropertyCount;

		// Get display property
		vkGetPhysicalDeviceDisplayPropertiesKHR(physicalDevice, &displayPropertyCount, nullptr);
		FixedSizeVector<VkDisplayPropertiesKHR, uint16>
			displayProperties(Memory::ConstructWithSize, Memory::Uninitialized, (uint16)displayPropertyCount);
		vkGetPhysicalDeviceDisplayPropertiesKHR(physicalDevice, &displayPropertyCount, displayProperties.GetData());

		// Get plane property
		uint32_t planePropertyCount;
		vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physicalDevice, &planePropertyCount, nullptr);
		FixedSizeVector<VkDisplayPlanePropertiesKHR, uint16>
			planeProperties(Memory::ConstructWithSize, Memory::Uninitialized, (uint16)planePropertyCount);
		vkGetPhysicalDeviceDisplayPlanePropertiesKHR(physicalDevice, &planePropertyCount, planeProperties.GetData());

		VkDisplayKHR display = 0;
		VkDisplayModeKHR displayMode = 0;
		Vector<VkDisplayModePropertiesKHR, uint8> modeProperties;
		bool foundMode = false;

		for (decltype(displayProperties)::SizeType i = 0; i < displayPropertyCount; ++i)
		{
			display = displayProperties[i].display;
			uint32_t modeCount;
			vkGetDisplayModePropertiesKHR(physicalDevice, display, &modeCount, nullptr);
			modeProperties.Resize(static_cast<decltype(modeProperties)::SizeType>(modeCount));
			vkGetDisplayModePropertiesKHR(physicalDevice, display, &modeCount, modeProperties.GetData());

			for (decltype(modeProperties)::SizeType j = 0; j < modeCount; ++j)
			{
				const VkDisplayModePropertiesKHR* mode = &modeProperties[j];

				if (mode->parameters.visibleRegion.width == resolution.x && mode->parameters.visibleRegion.height == resolution.y)
				{
					displayMode = mode->displayMode;
					foundMode = true;
					break;
				}
			}

			if (foundMode)
			{
				break;
			}
		}

		CheckFatalError(SOURCE_LOCATION, !foundMode, System::Get<Log>(), "Failed to find a suitable display mode!");

		// Search for a best plane we can use
		decltype(planeProperties)::SizeType bestPlaneIndex = Math::NumericLimits<decltype(planeProperties)::SizeType>::Max;
		for (decltype(planeProperties)::SizeType i = 0; i < planePropertyCount; i++)
		{
			const decltype(planeProperties)::SizeType planeIndex = i;
			uint32_t displayCount;
			vkGetDisplayPlaneSupportedDisplaysKHR(physicalDevice, planeIndex, &displayCount, nullptr);
			FixedSizeVector<VkDisplayKHR, uint16> displays(Memory::ConstructWithSize, Memory::Uninitialized, (uint16)displayCount);
			vkGetDisplayPlaneSupportedDisplaysKHR(physicalDevice, planeIndex, &displayCount, displays.GetData());

			// Find a display that matches the current plane
			bestPlaneIndex = Math::NumericLimits<decltype(planeProperties)::SizeType>::Max;
			for (decltype(displays)::SizeType j = 0; j < displayCount; j++)
			{
				if (display == displays[j])
				{
					bestPlaneIndex = i;
					break;
				}
			}

			if (bestPlaneIndex != Math::NumericLimits<decltype(planeProperties)::SizeType>::Max)
			{
				break;
			}
		}

		CheckFatalError(
			SOURCE_LOCATION,
			bestPlaneIndex == Math::NumericLimits<decltype(planeProperties)::SizeType>::Max,
			System::Get<Log>(),
			"Failed to find a suitable display plane!"
		);

		VkDisplayPlaneCapabilitiesKHR planeCap;
		vkGetDisplayPlaneCapabilitiesKHR(physicalDevice, displayMode, bestPlaneIndex, &planeCap);
		VkDisplayPlaneAlphaFlagBitsKHR alphaMode = (VkDisplayPlaneAlphaFlagBitsKHR)0;

		if (planeCap.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR)
		{
			alphaMode = VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_PREMULTIPLIED_BIT_KHR;
		}
		else if (planeCap.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR)
		{
			alphaMode = VK_DISPLAY_PLANE_ALPHA_PER_PIXEL_BIT_KHR;
		}
		else if (planeCap.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR)
		{
			alphaMode = VK_DISPLAY_PLANE_ALPHA_GLOBAL_BIT_KHR;
		}
		else if (planeCap.supportedAlpha & VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR)
		{
			alphaMode = VK_DISPLAY_PLANE_ALPHA_OPAQUE_BIT_KHR;
		}

		VkDisplaySurfaceCreateInfoKHR surfaceInfo{};
		surfaceInfo.sType = VK_STRUCTURE_TYPE_DISPLAY_SURFACE_CREATE_INFO_KHR;
		surfaceInfo.pNext = nullptr;
		surfaceInfo.flags = 0;
		surfaceInfo.displayMode = displayMode;
		surfaceInfo.planeIndex = bestPlaneIndex;
		surfaceInfo.planeStackIndex = planeProperties[bestPlaneIndex].currentStackIndex;
		surfaceInfo.transform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
		surfaceInfo.globalAlpha = 1.0;
		surfaceInfo.alphaMode = alphaMode;
		surfaceInfo.imageExtent.width = resolution.x;
		surfaceInfo.imageExtent.height = resolution.y;

		VkSurfaceKHR pSurface;
		VkResult result = vkCreateDisplayPlaneSurfaceKHR(instance, &surfaceInfo, nullptr, &pSurface);
		CheckFatalError(SOURCE_LOCATION, result != VK_SUCCESS, System::Get<Log>(), "Failed to find a create a direct to display surface!");
		return Surface{pSurface};
	}
#endif

#if RENDERER_VULKAN
	Optional<const PhysicalDevice*>
	SurfaceView::GetBestPhysicalDevice(const Renderer& renderer, const Optional<uint8*> pPresentQueueIndexOut) const
#else
	/* static */ Optional<const PhysicalDevice*>
	SurfaceView::GetBestPhysicalDevice(const Renderer& renderer, const Optional<uint8*> pPresentQueueIndexOut)
#endif
	{

#if RENDERER_VULKAN
		if (IsValid())
#else
		if (true)
#endif
		{
			Optional<const PhysicalDevice*> pSelectedDevice;
			for (const PhysicalDevice& __restrict physicalDevice : renderer.GetPhysicalDevices())
			{
				if (physicalDevice.SupportsSwapchain())
				{
					uint8 presentQueueIndex = 0;
					for (QueueFamilyIndex n = physicalDevice.GetQueueFamilyCount(); presentQueueIndex < n; ++presentQueueIndex)
					{
#if RENDERER_VULKAN && !PLATFORM_APPLE && !PLATFORM_ANDROID
						VkBool32 canPresent = false;
						vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, presentQueueIndex, m_pSurface, &canPresent);

						uint32 formatCount;
						vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, m_pSurface, &formatCount, nullptr);

						/*if (formatCount != 0)
						{
						FixedSizeVector<VkSurfaceFormatKHR, uint8> formats(Memory::ConstructWithSize, formatCount);
						vkGetPhysicalDeviceSurfaceFormatsKHR(foundPhysicalDevice.m_pDevice, &surface, &formatCount, formats.GetData());
						}*/

						uint32 presentModeCount;
						vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, m_pSurface, &presentModeCount, nullptr);

						/*if (presentModeCount != 0)
						{
						FixedSizeVector<VkPresentModeKHR, uint8> presentModes(Memory::ConstructWithSize, presentModeCount);
						vkGetPhysicalDeviceSurfacePresentModesKHR(foundPhysicalDevice.m_pDevice, &surface, &presentModeCount, presentModes.GetData());
						}*/

						const bool suitableDevice = canPresent & (formatCount != 0) & (presentModeCount != 0);
#else
						const bool suitableDevice = true;
#endif
						if (suitableDevice)
						{
							if (pSelectedDevice == nullptr || pSelectedDevice->GetRating() < physicalDevice.GetRating())
							{
								pSelectedDevice = &physicalDevice;
								if (pPresentQueueIndexOut.IsValid())
								{
									*pPresentQueueIndexOut = presentQueueIndex;
								}
							}
						}
					}
				}
			}

			return pSelectedDevice;
		}

		if (renderer.GetPhysicalDevices().GetView().HasElements())
		{
			return renderer.GetPhysicalDevices().GetView()[0];
		}

		return Invalid;
	}

	[[nodiscard]] Optional<LogicalDevice*> SurfaceView::CreateLogicalDeviceForSurface(Renderer& renderer) const
	{
		uint8 presentQueueIndex;
		Optional<const PhysicalDevice*> pPhysicalDevice = GetBestPhysicalDevice(renderer, presentQueueIndex);
		if (LIKELY(pPhysicalDevice != nullptr))
		{
			return renderer.CreateLogicalDeviceFromPhysicalDevice(*pPhysicalDevice, presentQueueIndex);
		}
		return Invalid;
	}
}
