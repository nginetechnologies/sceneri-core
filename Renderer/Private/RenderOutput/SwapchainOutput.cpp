#include "RenderOutput/SwapchainOutput.h"
#include "RenderOutput/PresentMode.h"

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Window/Window.h>
#include <Renderer/Wrappers/ImageView.h>
#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Threading/SemaphoreView.h>
#include <Renderer/Threading/FenceView.h>
#include <Renderer/FrameImageId.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <Renderer.h>

#include <Engine/Threading/JobRunnerThread.h>
#include <Common/System/Query.h>
#include <Common/IO/Log.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>
#include <Metal/ConvertFormat.h>
#include <Renderer/WebGPU/Includes.h>
#include <WebGPU/ConvertFormat.h>

#if PLATFORM_APPLE
#import <QuartzCore/CAMetalLayer.h>
#endif

#if PLATFORM_EMSCRIPTEN
#include <emscripten/html5.h>
#include <emscripten/proxying.h>
#include <emscripten/threading.h>
#endif

namespace ngine::Rendering
{
	SwapchainOutput::SwapchainOutput(
		const Math::Vector2ui resolution,
		const ArrayView<const Rendering::Format, uint16> supportedFormats,
		const LogicalDevice& logicalDevice,
		const SurfaceView surface,
		const EnumFlags<UsageFlags> usageFlags,
		Optional<Window*> pWindow
	)
		: RenderOutput(
				(Flags::SupportsAcquireImageSemaphore * RENDERER_VULKAN) |
				(Flags::SupportsPresentImageSemaphore * (RENDERER_VULKAN | RENDERER_WEBGPU))
			)
		, m_resolution(resolution)
		, m_supportedFormats(supportedFormats)
		, m_format(Format::Invalid)
#if RENDERER_METAL
		, m_swapchain((CAMetalLayer*)surface)
#elif RENDERER_WEBGPU_WGPU_NATIVE || RENDERER_WEBGPU_DAWN
		, m_swapchain(surface)
#endif
		, m_pWindow(pWindow)
	{
#if RENDERER_WEBGPU_WGPU_NATIVE || RENDERER_WEBGPU_DAWN
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[swapchain = m_swapchain]()
			{
				wgpuSurfaceAddRef(swapchain);
#if RENDERER_WEBGPU_DAWN
#else
				wgpuSurfaceReference(swapchain);
#endif
			}
		);
#endif
		if (surface.IsValid())
		{
			[[maybe_unused]] const bool wasCreated = CreateSwapchain(logicalDevice, surface, usageFlags);
			Assert(wasCreated);
		}
	}

	SwapchainOutput::~SwapchainOutput() = default;

	void SwapchainOutput::Destroy([[maybe_unused]] LogicalDevice& logicalDevice)
	{
		for (ImageMapping& textureView : m_swapchainImageViews)
		{
			textureView.Destroy(logicalDevice);
		}

#if RENDERER_METAL
		for (auto& image : m_swapchainImages)
		{
			if (Internal::ImageData* pImageData = image)
			{
				pImageData->m_image = {};
				image.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
			}
		}
#elif RENDERER_WEBGPU
		for (auto& image : m_swapchainImages)
		{
			if (Internal::ImageData* pImageData = image)
			{
				delete pImageData;
				image = {};
			}
		}
#endif
		m_swapchainImages.Clear();
		m_subresourceStates.Clear();
		m_swapchainImageViews.Clear();

#if RENDERER_VULKAN
		vkDestroySwapchainKHR(logicalDevice, m_swapchain, nullptr);
		m_swapchain = {};
#elif RENDERER_WEBGPU_WGPU_NATIVE || RENDERER_WEBGPU_DAWN
		if (m_swapchain.IsValid())
		{
			wgpuSurfaceRelease(m_swapchain);
			m_swapchain = {};
		}
#elif RENDERER_WEBGPU
		if (m_swapchain.IsValid())
		{
			wgpuSwapChainRelease(m_swapchain);
			m_swapchain = {};
		}
#endif
	}

	bool
	SwapchainOutput::CreateSwapchain(const LogicalDevice& logicalDevice, const SurfaceView surface, const EnumFlags<UsageFlags> usageFlags)
	{
		Assert(surface.IsValid());
		if (UNLIKELY(!surface.IsValid()))
		{
			return false;
		}

		constexpr bool enableVerticalSync = true;

#if RENDERER_VULKAN
		FlatVector<uint32, static_cast<uint8>(QueueFamily::Count)> queueFamilyIndices;
		queueFamilyIndices.EmplaceBack(logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics));

		const bool hasSeparateTransferQueue = logicalDevice.GetCommandQueue(QueueFamily::Transfer) !=
		                                      logicalDevice.GetCommandQueue(QueueFamily::Graphics);
		const bool hasSeparatePresentQueue = logicalDevice.GetCommandQueue(QueueFamily::Graphics) != logicalDevice.GetPresentCommandQueue();

		if (hasSeparatePresentQueue)
		{
			queueFamilyIndices.EmplaceBack(logicalDevice.GetPresentQueueIndex());
		}

		if (hasSeparateTransferQueue)
		{
			queueFamilyIndices.EmplaceBack(logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Transfer));
		}

		const Rendering::PhysicalDeviceView physicalDevice = logicalDevice.GetPhysicalDevice();

		VkSurfaceCapabilitiesKHR capabilities;
		[[maybe_unused]] const VkResult capabilitiesResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
		Assert(capabilitiesResult == VK_SUCCESS);

		const Math::Vector2ui extent = Math::Clamp(
			m_resolution,
			Math::Vector2ui{capabilities.minImageExtent.width, capabilities.minImageExtent.height},
			Math::Vector2ui{capabilities.maxImageExtent.width, capabilities.maxImageExtent.height}
		);

		VkSurfaceFormatKHR surfaceFormat;
		{
			uint32 supportedDeviceFormatCount;
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &supportedDeviceFormatCount, nullptr);

			FixedSizeVector<VkSurfaceFormatKHR, uint16>
				supportedDeviceFormats(Memory::ConstructWithSize, Memory::Uninitialized, (uint16)supportedDeviceFormatCount);
			vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &supportedDeviceFormatCount, supportedDeviceFormats.GetData());

			const ArrayView<const Format, uint16> requestedFormats = m_supportedFormats;
			const OptionalIterator<VkSurfaceFormatKHR> foundSurfaceFormat = supportedDeviceFormats.FindIf(
				[requestedFormats](const VkSurfaceFormatKHR supportedFormat)
				{
					return requestedFormats.Contains(static_cast<Format>(supportedFormat.format));
				}
			);
			Assert(foundSurfaceFormat);
			if (UNLIKELY_ERROR(!foundSurfaceFormat))
			{
				return false;
			}
			surfaceFormat = *foundSurfaceFormat;
			m_format = static_cast<Format>(surfaceFormat.format);
		}

		constexpr PresentMode presentMode = enableVerticalSync ? PresentMode::FirstInFirstOut : PresentMode::Immediate;

		{
			uint32 supportedPresentModeCount;
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &supportedPresentModeCount, nullptr);

			FixedSizeVector<VkPresentModeKHR, uint16>
				supportedPresentModes(Memory::ConstructWithSize, Memory::Uninitialized, (uint16)supportedPresentModeCount);
			vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &supportedPresentModeCount, supportedPresentModes.GetData());

			const bool supportsPresentMode = supportedPresentModes.Contains(static_cast<VkPresentModeKHR>(presentMode));
			Assert(supportsPresentMode);
			if (UNLIKELY_ERROR(!supportsPresentMode))
			{
				return false;
			}
		}

		Format swapchainFormat;
		const VkSwapchainCreateInfoKHR swapChainCreateInfo = {
			VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
			nullptr,
			0,
			surface,
			Math::Clamp(
				(uint32)Rendering::MaximumConcurrentFrameCount,
				capabilities.minImageCount,
				capabilities.maxImageCount > 0 ? capabilities.maxImageCount : Rendering::MaximumConcurrentFrameCount
			),
			surfaceFormat.format,
			surfaceFormat.colorSpace,
			{extent.x, extent.y},
			1,
			static_cast<VkImageUsageFlags>(usageFlags.GetFlags()),
			VK_SHARING_MODE_EXCLUSIVE, // Math::Select(hasSeparatePresentQueue | hasSeparateTransferQueue, VK_SHARING_MODE_CONCURRENT,
		                             // VK_SHARING_MODE_EXCLUSIVE),
			queueFamilyIndices.GetSize(),
			queueFamilyIndices.GetData(),
			VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR,
			(capabilities.supportedCompositeAlpha & VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR) ? VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR
																																								 : VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR,
			static_cast<VkPresentModeKHR>(presentMode),
			VK_TRUE,
			m_swapchain
		};

		const VkResult swapchainResult = vkCreateSwapchainKHR(logicalDevice, &swapChainCreateInfo, nullptr, &m_swapchain.m_pSwapchain);
		AssertMessage(swapchainResult == VK_SUCCESS, "Failed to create swapchain!");
		if (UNLIKELY_ERROR(swapchainResult != VK_SUCCESS))
		{
			return false;
		}

		uint32 swapchainImageCount;
		vkGetSwapchainImagesKHR(logicalDevice, m_swapchain, &swapchainImageCount, nullptr);
		Assert(swapchainImageCount <= Math::NumericLimits<FrameIndex>::Max);

		const FrameIndex imageCount = (FrameIndex)swapchainImageCount;

		m_swapchainImages.Resize(imageCount);
		m_swapchainImageViews.Resize(imageCount);
		vkGetSwapchainImagesKHR(logicalDevice, m_swapchain, &swapchainImageCount, reinterpret_cast<VkImage*>(m_swapchainImages.GetData()));

		swapchainFormat = static_cast<Format>(swapChainCreateInfo.imageFormat);
#elif RENDERER_METAL
		constexpr Array supportedDeviceFormats{Format::B8G8R8A8_UNORM, Format::B8G8R8A8_SRGB, Format::R16G16B16A16_SFLOAT};
		const ArrayView<const Format, uint16> requestedFormats = m_supportedFormats;
		const OptionalIterator<const Format> foundSurfaceFormat = supportedDeviceFormats.GetView().FindIf(
			[requestedFormats](const Format supportedDeviceFormat)
			{
				return requestedFormats.Contains(supportedDeviceFormat);
			}
		);
		Assert(foundSurfaceFormat);
		if (UNLIKELY_ERROR(!foundSurfaceFormat))
		{
			return false;
		}
		const Format swapchainFormat = *foundSurfaceFormat;
		m_format = swapchainFormat;

		CAMetalLayer* metalLayer = surface;

		static constexpr uint8 imageCount = Rendering::MaximumConcurrentFrameCount;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[metalLayer, &logicalDevice, swapchainFormat, enableVerticalSync, usageFlags]()
			{
				metalLayer.device = (LogicalDeviceView)logicalDevice;
				metalLayer.pixelFormat = ConvertFormat(swapchainFormat);
				metalLayer.maximumDrawableCount = imageCount;
#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS
				UNUSED(enableVerticalSync);
#else
				metalLayer.displaySyncEnabled = enableVerticalSync;
#endif
				metalLayer.framebufferOnly =
					usageFlags.AreNoneSet(UsageFlags::TransferSource | UsageFlags::TransferDestination | UsageFlags::Sampled | UsageFlags::Storage);
				metalLayer.opaque = true;
				metalLayer.colorspace = CGColorSpaceCreateWithName(kCGColorSpaceDisplayP3);
			}
		);

		m_swapchainImages.Resize(imageCount);
		m_swapchainImageViews.Resize(imageCount);
		for (uint8 i = 0; i < imageCount; i++)
		{
			m_swapchainImages[i] = new Internal::ImageData{};
		}
#elif RENDERER_WEBGPU
		const ArrayView<const Format, uint16> requestedFormats = m_supportedFormats;

#if RENDERER_WEBGPU_DAWN || PLATFORM_EMSCRIPTEN
		InlineVector<WGPUTextureFormat, 10, uint16> supportedFormats;
		supportedFormats.EmplaceBack(WGPUTextureFormat_BGRA8Unorm);

#if PLATFORM_EMSCRIPTEN
		{
			WGPUTextureFormat preferredFormat;
			Rendering::Window::ExecuteImmediatelyOnWindowThread(
				[&logicalDevice, surface, &preferredFormat]()
				{
					preferredFormat = wgpuSurfaceGetPreferredFormat(surface, logicalDevice.GetPhysicalDevice());
				}
			);

			auto it = supportedFormats.Find(preferredFormat);
			Assert(it != supportedFormats.end());
			if (LIKELY(it != supportedFormats.end()))
			{
				supportedFormats.Remove(it);
			}
			supportedFormats.Emplace(supportedFormats.begin(), Memory::Uninitialized, WGPUTextureFormat(preferredFormat));
		}
#endif

#else
		WGPUSurfaceCapabilities surfaceCapabilities;
		wgpuSurfaceGetCapabilities(m_swapchain, logicalDevice.GetPhysicalDevice(), &surfaceCapabilities);

		InlineVector<WGPUTextureFormat, 10, uint16>
			supportedFormats(Memory::ConstructWithSize, Memory::Uninitialized, (uint16)surfaceCapabilities.formatCount);
		supportedFormats.GetView().CopyFrom(
			ArrayView<const WGPUTextureFormat, uint16>{surfaceCapabilities.formats, (uint16)surfaceCapabilities.formatCount}
		);
#endif

		const OptionalIterator<WGPUTextureFormat> foundFormat = supportedFormats.GetView().FindIf(
			[requestedFormats](const WGPUTextureFormat supportedFormat)
			{
				return requestedFormats.ContainsIf(
					[supportedFormat](const Format requestedFormat)
					{
						return supportedFormat == ConvertFormat(requestedFormat);
					}
				);
			}
		);
		Assert(foundFormat.IsValid());
		if (UNLIKELY_ERROR(!foundFormat.IsValid()))
		{
			return false;
		}
		const Rendering::Format swapchainFormat = ConvertFormat(*foundFormat);
		m_format = swapchainFormat;
		const WGPUTextureFormat format = *foundFormat;

		int swapchainUsage =
			WGPUTextureUsage_RenderAttachment *
			usageFlags.AreAnySet(UsageFlags::InputAttachment | UsageFlags::ColorAttachment | UsageFlags::DepthStencilAttachment);
		swapchainUsage |= WGPUTextureUsage_CopySrc * usageFlags.IsSet(UsageFlags::TransferSource);
		swapchainUsage |= WGPUTextureUsage_CopyDst * usageFlags.IsSet(UsageFlags::TransferDestination);
		swapchainUsage |= WGPUTextureUsage_TextureBinding * usageFlags.IsSet(UsageFlags::Sampled);
		// swapchainUsage |= WGPUTextureUsage_Transient * usageFlags.IsSet(UsageFlags::TransientAttachment);
		//  swapchainUsage |= WGPUTextureUsage_StorageBinding * usageFlags.IsSet(UsageFlags::Storage);

		const Math::Vector2ui extent{m_resolution};

		constexpr PresentMode presentMode = enableVerticalSync ? PresentMode::FirstInFirstOut : PresentMode::Immediate;

		const auto getPresentMode = [](const PresentMode presentMode)
		{
			switch (presentMode)
			{
				case PresentMode::Immediate:
					return WGPUPresentMode_Immediate;
				case PresentMode::Mailbox:
					return WGPUPresentMode_Mailbox;
				case PresentMode::FirstInFirstOut:
					return WGPUPresentMode_Fifo;
			}
			ExpectUnreachable();
		};

#if RENDERER_WEBGPU_WGPU_NATIVE || RENDERER_WEBGPU_DAWN
		const WGPUSurfaceConfiguration swapchainDescriptor{
			nullptr,
			logicalDevice,
			format,
			swapchainUsage,
			1,
			&format,
			WGPUCompositeAlphaMode_Auto,
			extent.x,
			extent.y,
			getPresentMode(presentMode)
		};
		wgpuSurfaceConfigure(m_swapchain, &swapchainDescriptor);
#else
		const WGPUSwapChainDescriptor
			swapchainDescriptor{nullptr, nullptr, (WGPUTextureUsage)swapchainUsage, format, extent.x, extent.y, getPresentMode(presentMode)};
		const LogicalDeviceView logicalDeviceView = logicalDevice;
		WGPUSwapChain pSwapchain;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[logicalDevice = logicalDeviceView, surface, swapchainDescriptor, &pSwapchain]()
			{
				pSwapchain = wgpuDeviceCreateSwapChain(logicalDevice, surface, &swapchainDescriptor);
				wgpuSwapChainReference(pSwapchain);
			}
		);
		m_swapchain = pSwapchain;
#endif

		constexpr uint8 imageCount = Rendering::MaximumConcurrentFrameCount;
		m_swapchainImages.Resize(imageCount);
		m_swapchainImageViews.Resize(imageCount);
		for (uint8 i = 0; i < imageCount; i++)
		{
			m_swapchainImages[i] = new Internal::ImageData{};
		}
#endif

		for (uint8 i = 0; i < imageCount; i++)
		{
			m_swapchainImageViews[i] = ImageMapping(
				logicalDevice,
				m_swapchainImages[i],
				Rendering::ImageMapping::Type::TwoDimensional,
				swapchainFormat,
				ImageAspectFlags::Color,
				MipRange{0, 1},
				ArrayRange{0, 1}
			);
		}

#if RENDERER_OBJECT_DEBUG_NAMES
		for (uint8 i = 0; i < imageCount; i++)
		{
			m_swapchainImages[i].SetDebugName(logicalDevice, "Swapchain image");
			m_swapchainImageViews[i].SetDebugName(logicalDevice, "Swapchain image mapping");
		}
#endif

		m_frameStates.Resize(imageCount);
		m_frameStates.GetView().InitializeAll(FrameState::Inactive);
		m_subresourceStates.Resize(imageCount);
		const ImageSubresourceRange subresourceRange{ImageAspectFlags::Color, MipRange{0, 1}, ArrayRange{0, 1}};
		for (SubresourceStatesBase& subresourceStates : m_subresourceStates)
		{
			const uint8 subresourceBucketCount = 1;
			subresourceStates.Initialize(subresourceRange, subresourceBucketCount);

			subresourceStates.SetSubresourceState(
				subresourceRange,
				SubresourceState{
					ImageLayout::Undefined,
					PassAttachmentReference{},
					PipelineStageFlags::TopOfPipe,
					AccessFlags::None,
					(QueueFamilyIndex)~0u
				},
				subresourceRange,
				0
			);
		}

		return true;
	}

	bool SwapchainOutput::RecreateSwapchain(
		LogicalDevice& logicalDevice, const SurfaceView surface, const Math::Vector2ui newResolution, const EnumFlags<UsageFlags> usageFlags
	)
	{
		m_resolution = newResolution;

		for (ImageMapping& swapchainImageView : m_swapchainImageViews)
		{
			swapchainImageView.Destroy(logicalDevice);
		}

#if RENDERER_METAL
		for (auto& image : m_swapchainImages)
		{
			if (Internal::ImageData* pImageData = image)
			{
				pImageData->m_image = {};
				image.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
			}
		}
#elif RENDERER_WEBGPU
		for (auto& image : m_swapchainImages)
		{
			if (Internal::ImageData* pImageData = image)
			{
				delete pImageData;
				image = {};
			}
		}
#endif

		[[maybe_unused]] const SwapchainView previousSwapchain = m_swapchain;

#if RENDERER_WEBGPU_WGPU_NATIVE || RENDERER_WEBGPU_DAWN
		if (previousSwapchain.IsValid())
		{
			wgpuSurfaceUnconfigure(previousSwapchain);
			m_swapchain = {};
		}
#elif RENDERER_VULKAN && PLATFORM_ANDROID
		if (previousSwapchain.IsValid())
		{
			vkDestroySwapchainKHR(logicalDevice, previousSwapchain, nullptr);
			m_swapchain = {};
		}
#endif

		const bool success = CreateSwapchain(logicalDevice, surface, usageFlags);

#if RENDERER_VULKAN && !PLATFORM_ANDROID
		vkDestroySwapchainKHR(logicalDevice, previousSwapchain, nullptr);
#elif RENDERER_WEBGPU && !RENDERER_WEBGPU_WGPU_NATIVE && !RENDERER_WEBGPU_DAWN
		if (previousSwapchain.IsValid())
		{
			Rendering::Window::QueueOnWindowThread(
				[previousSwapchain]()
				{
					wgpuSwapChainRelease(previousSwapchain);
				}
			);
		}
#endif

		return success;
	}

#if PLATFORM_EMSCRIPTEN
	int SwapchainOutput::ProcessAnimationFrame(const uint8 frameIndex, [[maybe_unused]] const double time)
	{
		{
			Threading::UniqueLock lock(m_frameStateMutex);

			Assert(m_frameStates[frameIndex] == FrameState::AwaitingAnimationFrame);
			m_frameStates[frameIndex] = FrameState::ProcessingAnimationFrame;
		}
		m_frameStateConditionVariable.NotifyAll();

		em_proxying_queue* queue = emscripten_proxy_get_system_queue();
		while (m_frameStates[frameIndex] == FrameState::ProcessingAnimationFrame)
		{
			emscripten_proxy_execute_queue(queue);
		}

		return EM_TRUE;
	}
#endif

	Optional<FrameImageId> SwapchainOutput::AcquireNextImage(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		const uint64 imageAvailabilityTimeoutNanoseconds,
		[[maybe_unused]] const SemaphoreView imageAvailableSemaphore,
		[[maybe_unused]] FenceView fence
	)
	{
		// TODO: Allow multiple frames in flight
		Threading::UniqueLock lock(m_frameStateMutex);
		for (FrameState& frameState : m_frameStates)
		{
			if (frameState != FrameState::Inactive)
			{
				return Invalid;
			}
		}

#if RENDERER_VULKAN
		uint32 imageIndex;
		const VkResult result =
			vkAcquireNextImageKHR(logicalDevice, m_swapchain, imageAvailabilityTimeoutNanoseconds, imageAvailableSemaphore, fence, &imageIndex);

		switch (result)
		{
			case VK_SUCCESS:
			case VK_SUBOPTIMAL_KHR:
			{
				const uint8 frameIndex = (uint8)imageIndex;
				m_frameIndex = frameIndex;
				m_frameStates[frameIndex] = FrameState::ProcessingAnimationFrame;
				return FrameImageId(frameIndex);
			}
			case VK_ERROR_OUT_OF_DATE_KHR:
				Assert(false, "Image acquiring with an out of date swapchain");
				return Invalid;
			default:
				return Invalid;
		}
#elif RENDERER_METAL
		Assert(!imageAvailableSemaphore.IsValid(), "Not supported");

		if (m_swapchain.m_currentDrawable != nil)
		{
			return Invalid;
		}

		const uint8 frameIndex = m_frameIndex;
		m_frameStates[frameIndex] = FrameState::AwaitingAnimationFrame;

		const bool allowTimeout = imageAvailabilityTimeoutNanoseconds < Math::NumericLimits<uint64>::Max;
		[m_swapchain.m_pSwapchain setAllowsNextDrawableTimeout:allowTimeout];

		id<CAMetalDrawable> drawable = [m_swapchain.m_pSwapchain nextDrawable];
		if (drawable == nil)
		{
			m_frameStates[frameIndex] = FrameState::Inactive;
			return Invalid;
		}

		m_swapchain.m_currentDrawable = drawable;

		m_frameIndex = frameIndex;
		m_frameStates[frameIndex] = FrameState::ProcessingAnimationFrame;

		Internal::ImageData* pImageData = m_swapchainImages[frameIndex];
		id<MTLTexture> drawableTexture = [drawable texture];
		if (pImageData->m_image != drawableTexture)
		{
			pImageData->m_image = drawableTexture;
			Internal::ImageMappingData* pImageMappingData = m_swapchainImageViews[frameIndex];
			pImageMappingData->m_textureView = [pImageData->m_image newTextureViewWithPixelFormat:ConvertFormat(m_format)];
		}

		if (fence.IsValid())
		{
			fence.Signal();
		}

		return FrameImageId{frameIndex};
#elif RENDERER_WEBGPU
		const uint8 frameIndex = m_frameIndex;

		m_frameStates[frameIndex] = FrameState::AwaitingAnimationFrame;
#if PLATFORM_EMSCRIPTEN
		m_frameStateConditionVariable.NotifyAll();

		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[this, frameIndex]()
			{
				struct FrameData
				{
					SwapchainOutput& output;
					uint8 frameIndex;
				};
				FrameData* pFrameData = new FrameData{*this, frameIndex};

				emscripten_request_animation_frame(
					[]([[maybe_unused]] const double time, [[maybe_unused]] void* pUserData) -> EM_BOOL
					{
						FrameData* pFrameData = reinterpret_cast<FrameData*>(pUserData);

						const EM_BOOL result = pFrameData->output.ProcessAnimationFrame(pFrameData->frameIndex, time);
						delete pFrameData;
						return result;
					},
					pFrameData
				);
			}
		);
		while (m_frameStates[frameIndex] == FrameState::AwaitingAnimationFrame)
		{
			m_frameStateConditionVariable.Wait(lock);
		}
#endif

#if RENDERER_WEBGPU_WGPU_NATIVE || RENDERER_WEBGPU_DAWN
		WGPUSurfaceTexture surfaceTexture;
		wgpuSurfaceGetCurrentTexture(m_swapchain, &surfaceTexture);

		if (surfaceTexture.status != WGPUSurfaceGetCurrentTextureStatus_Success || surfaceTexture.texture == nullptr)
		{
			m_frameStates[frameIndex] = FrameState::Inactive;
			return Invalid;
		}

		m_frameIndex = frameIndex;
		m_frameStates[frameIndex] = FrameState::ProcessingAnimationFrame;

		const WGPUTextureViewDescriptor textureViewDescriptor
		{
			nullptr,
#if RENDERER_WEBGPU_DAWN
				WGPUStringView{nullptr, 0},
#else
				nullptr,
#endif
				ConvertFormat(m_format), WGPUTextureViewDimension_2D, 0, 1, 0, 1, WGPUTextureAspect_All
		};

		WGPUTextureView pTextureView = wgpuTextureCreateView(surfaceTexture.texture, &textureViewDescriptor);
		if (pTextureView != nullptr)
		{
#if RENDERER_WEBGPU_DAWN
			wgpuTextureViewAddRef(pTextureView);
#else
			wgpuTextureViewReference(pTextureView);
#endif
		}

#else
		WGPUTextureView pTextureView;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[&pTextureView, this]()
			{
				pTextureView = wgpuSwapChainGetCurrentTextureView(m_swapchain);
				if (pTextureView != nullptr)
				{
#if RENDERER_WEBGPU_DAWN
					wgpuTextureViewAddRef(pTextureView);
#else
					wgpuTextureViewReference(pTextureView);
#endif
				}
			}
		);
#endif

		if (pTextureView == nullptr)
		{
			m_frameStates[frameIndex] = FrameState::Inactive;
			return Invalid;
		}

		Internal::ImageMappingData* pImageMappingData = m_swapchainImageViews[frameIndex];
		pImageMappingData->m_textureView = pTextureView;

		UNUSED(logicalDevice);
		UNUSED(imageAvailabilityTimeoutNanoseconds);
		UNUSED(imageAvailableSemaphore);

		if (fence.IsValid())
		{
			fence.Signal();
		}

		return FrameImageId(frameIndex);
#else
		Assert(false, "TODO");
		UNUSED(logicalDevice);
		UNUSED(imageAvailabilityTimeoutNanoseconds);
		UNUSED(imageAvailableSemaphore);
		UNUSED(fence);
		return Invalid;
#endif
	}

	void SwapchainOutput::PresentAcquiredImage(
		LogicalDevice& logicalDevice,
		const Rendering::FrameImageId imageIndex,
		const ArrayView<const Rendering::SemaphoreView, uint8> waitSemaphores,
		const Rendering::FenceView fence,
		Rendering::PresentedCallback&& presentedCallback
	)
	{
		{
			Threading::UniqueLock lock(m_frameStateMutex);
			Assert(m_frameStates[(uint8)imageIndex] == FrameState::ProcessingAnimationFrame);
			m_frameStates[(uint8)imageIndex] = FrameState::FinishedCpuSubmits;
		}
#if RENDERER_WEBGPU && PLATFORM_EMSCRIPTEN
		m_frameStateConditionVariable.NotifyAll();
#endif

		presentedCallback = [this, imageIndex, presentedCallback = Move(presentedCallback)]()
		{
			presentedCallback();

#if RENDERER_METAL
			Internal::ImageData* pImageData = m_swapchainImages[(uint8)imageIndex];
			pImageData->m_image = {};
			m_swapchain.m_currentDrawable = nullptr;
#elif RENDERER_WEBGPU
			Internal::ImageMappingData* pImageMappingData = m_swapchainImageViews[(uint8)imageIndex];
			Rendering::Window::ExecuteImmediatelyOnWindowThread(
				[pImageMappingData]()
				{
					wgpuTextureViewRelease(pImageMappingData->m_textureView);
					pImageMappingData->m_textureView = {};
				}
			);
#endif

#if RENDERER_WEBGPU || RENDERER_METAL
			const uint8 imageCount = m_swapchainImages.GetSize();
			const uint8 frameIndex = (uint8)((m_frameIndex + 1) % imageCount);
			m_frameIndex = frameIndex;
#endif

			{
				Threading::UniqueLock lock(m_frameStateMutex);
				Assert(m_frameStates[(uint8)imageIndex] == FrameState::FinishedCpuSubmits);
				m_frameStates[(uint8)imageIndex] = FrameState::Inactive;
			}
#if RENDERER_WEBGPU && PLATFORM_EMSCRIPTEN
			m_frameStateConditionVariable.NotifyAll();
#endif
		};

		if (waitSemaphores.HasElements())
		{
			Assert(SupportsPresentImageSemaphore());
			logicalDevice.GetQueuePresentJob()
				.QueuePresent(Threading::JobPriority::Present, *this, imageIndex, waitSemaphores, Forward<PresentedCallback>(presentedCallback));
		}
		else if (fence.IsValid())
		{
			Assert(fence.IsValid());
			logicalDevice.GetQueuePresentJob().QueueAwaitFence(
				fence,
				[this, &logicalDevice, imageIndex, waitSemaphores, fence, presentedCallback = Forward<PresentedCallback>(presentedCallback)](
				) mutable
				{
					fence.Reset(logicalDevice);

					logicalDevice.GetQueuePresentJob()
						.QueuePresent(Threading::JobPriority::Present, *this, imageIndex, waitSemaphores, Move(presentedCallback));
				}
			);
		}
		else if constexpr (RENDERER_METAL)
		{
			logicalDevice.GetQueuePresentJob()
				.QueuePresent(Threading::JobPriority::Present, *this, imageIndex, waitSemaphores, Move(presentedCallback));
		}
		else
		{
			Assert(false, "Immediate present path not supported for platform");
			presentedCallback();
		}
	}
}
