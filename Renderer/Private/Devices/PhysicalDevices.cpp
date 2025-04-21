#include "Devices/PhysicalDevices.h"
#include <Renderer/Instance.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/WebGPU/Includes.h>
#include <Renderer/Metal/Includes.h>

#include <Common/Memory/Containers/Vector.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/IO/Log.h>

#include <algorithm>

#if PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>

#include <Renderer/Window/Window.h>
#elif RENDERER_WEBGPU
#include <Common/Threading/Jobs/JobRunnerThread.h>

#include <Renderer/Window/Window.h>
#endif

namespace ngine::Rendering
{
	PhysicalDevices::PhysicalDevices(const InstanceView instance)
	{
		if (UNLIKELY_ERROR(!instance.IsValid()))
		{
			return;
		}

#if RENDERER_VULKAN
		uint32 deviceCount;
		const VkResult result = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
		Assert(result == VK_SUCCESS, "Failed to enumerate Vulkan physical devices");
		if (UNLIKELY_ERROR(result != VK_SUCCESS))
		{
			return;
		}

		FixedSizeVector<VkPhysicalDevice, uint16> devices(Memory::ConstructWithSize, Memory::Uninitialized, (uint16)deviceCount);
		vkEnumeratePhysicalDevices(instance, &deviceCount, devices.GetData());

		for (VkPhysicalDevice pDevice : devices)
		{
			VkPhysicalDeviceProperties deviceProperties;
			vkGetPhysicalDeviceProperties(pDevice, &deviceProperties);

			VkPhysicalDeviceFeatures2 deviceFeatures;
			deviceFeatures.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2;
			VkPhysicalDeviceVulkan12Features deviceVulkan12Features = {};
			deviceVulkan12Features.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
			deviceFeatures.pNext = &deviceVulkan12Features;

			PFN_vkGetPhysicalDeviceFeatures2 vkGetPhysicalDeviceFeatures2_func =
				reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>((void*)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2"));
			if (vkGetPhysicalDeviceFeatures2_func == nullptr)
			{
				// In Vulkan 1.0 might be accessible under its original extension name
				vkGetPhysicalDeviceFeatures2_func =
					reinterpret_cast<PFN_vkGetPhysicalDeviceFeatures2>((void*)vkGetInstanceProcAddr(instance, "vkGetPhysicalDeviceFeatures2KHR"));
			}
#if !PLATFORM_ANDROID
			if (vkGetPhysicalDeviceFeatures2_func == nullptr)
			{
				vkGetPhysicalDeviceFeatures2_func = vkGetPhysicalDeviceFeatures2;
			}
#endif
			vkGetPhysicalDeviceFeatures2_func(pDevice, &deviceFeatures);

			uint32 rating = (deviceProperties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) * 1000;

			// Maximum possible size of textures affects graphics quality
			rating += deviceProperties.limits.maxImageDimension2D;

			if (rating != 0)
			{
				const decltype(m_devices)::ConstPointerType upperBoundIt = std::upper_bound(
					m_devices.begin().Get(),
					m_devices.end().Get(),
					rating,
					[](const unsigned int newRating, const PhysicalDevice& __restrict device) -> bool
					{
						return newRating < device.GetRating();
					}
				);

				if (m_devices.ReachedCapacity())
				{
					if (rating > m_devices.GetLastElement().GetRating())
					{
						m_devices.Remove(m_devices.end() - 1);
					}
					else
					{
						continue;
					}
				}
				m_devices.Emplace(upperBoundIt, Memory::Uninitialized, pDevice, instance, rating);
			}
		}
#elif RENDERER_METAL

#if PLATFORM_APPLE_MACOS || PLATFORM_APPLE_MACCATALYST
		NSArray<id<MTLDevice>>* devices = MTLCopyAllDevices();
		for (id<MTLDevice> device in devices)
		{
			const bool isExternal = device.isRemovable;
			const bool isHighPower = !device.isLowPower;
			const bool isConnectedToDisplay = !device.isHeadless;

			const uint32 rating = isConnectedToDisplay + isExternal + isConnectedToDisplay + isHighPower;

			const decltype(m_devices)::ConstPointerType upperBoundIt = std::upper_bound(
				m_devices.begin().Get(),
				m_devices.end().Get(),
				rating,
				[](const unsigned int newRating, const PhysicalDevice& __restrict device) -> bool
				{
					return newRating < device.GetRating();
				}
			);

			m_devices.Emplace(upperBoundIt, Memory::Uninitialized, device, instance, rating);
		}
#else
		m_devices.EmplaceBack(MTLCreateSystemDefaultDevice(), instance, 1);
#endif

#elif RENDERER_WEBGPU
		struct Data
		{
			PhysicalDevices& physicalDevices;
			InstanceView instance;
			Threading::Atomic<bool> finished{false};
		};
		Data data{*this, instance};

		constexpr WGPUBackendType backendType = WGPUBackendType_Undefined;
		const WGPURequestAdapterOptions requestAdapterOptions
		{
			nullptr,
			nullptr,
#if RENDERER_WEBGPU_DAWN
			WGPUFeatureLevel_Core,
#endif
			WGPUPowerPreference_HighPerformance,
			backendType,
			false,
#if !RENDERER_WEBGPU_WGPU_NATIVE && !RENDERER_WEBGPU_DAWN
				false
#endif
		};
		Rendering::Window::QueueOnWindowThread(
			[requestAdapterOptions, instance, &data]()
			{
				auto callback = [](
					[[maybe_unused]] const WGPURequestAdapterStatus status,
					const WGPUAdapter adapter,
#if RENDERER_WEBGPU_DAWN
					[[maybe_unused]] WGPUStringView message,
					void* pUserData
#else
					[[maybe_unused]] char const * message,
					void* pUserData
	#endif
				)
				{
					Assert(status == WGPURequestAdapterStatus_Success);
					Data& data = *reinterpret_cast<Data*>(pUserData);
					Assert(adapter != nullptr);
					if (LIKELY(adapter != nullptr))
					{
#if RENDERER_WEBGPU_DAWN
						wgpuAdapterAddRef(adapter);
#else
						wgpuAdapterReference(adapter);
#endif
						uint32 rating = 1000;
						data.physicalDevices.m_devices.EmplaceBack(adapter, data.instance, rating);
					}

					data.finished = true;
				};


				wgpuInstanceRequestAdapter(
					instance,
					&requestAdapterOptions,
#if RENDERER_WEBGPU_DAWN2
					WGPURequestAdapterCallbackInfo
					{
						nullptr,
						WGPUCallbackMode_AllowSpontaneous,
						callback,
						&data,
						nullptr
					}
#else
					callback,
					&data
#endif
				);
			}
		);

		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
		while (!data.finished)
		{
			thread.DoRunNextJob();
		}

#endif

		if (UNLIKELY_ERROR(m_devices.IsEmpty() == 0))
		{
			LogError("Detected 0 physical vulkan devices!");
		}
	}

	Optional<PhysicalDevice*> PhysicalDevices::FindPhysicalDevice(const Rendering::PhysicalDeviceView device)
	{
		for (PhysicalDevice& physicalDevice : m_devices)
		{
			if (device == physicalDevice)
			{
				return physicalDevice;
			}
		}
		return Invalid;
	}
}
