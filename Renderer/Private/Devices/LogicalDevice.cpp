#include "Devices/LogicalDevice.h"

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>
#include <Renderer/WebGPU/Includes.h>

#include "Devices/PhysicalDevice.h"
#include "Jobs/QueueSubmissionJob.h"

#include <Common/Memory/OffsetOf.h>
#include <Common/IO/Format/Path.h>
#include <Common/Memory/AddressOf.h>

#include <Renderer/Renderer.h>

#include <Common/System/Query.h>
#include <Common/IO/Log.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Engine/IO/Filesystem.h>

#if PLATFORM_EMSCRIPTEN
#include <emscripten.h>
#include <emscripten/threading.h>
#include <Common/Platform/Type.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Renderer/Window/Window.h>
#elif RENDERER_WEBGPU
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Platform/Type.h>
#include <Renderer/Window/Window.h>
#endif

#if !RENDERER_HAS_PUSH_CONSTANTS
#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#endif

namespace ngine::Rendering
{
	[[nodiscard]] CommandQueueView CreateCommandQueue(const LogicalDeviceView logicalDevice, [[maybe_unused]] const QueueFamilyIndex index)
	{
#if RENDERER_VULKAN
		VkQueue pQueue;
		vkGetDeviceQueue(logicalDevice, index, 0, &pQueue);
		return pQueue;
#elif RENDERER_METAL
		return [logicalDevice newCommandQueue];
#elif RENDERER_WEBGPU
		CommandQueueView result;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[logicalDevice, &result]()
			{
				result = wgpuDeviceGetQueue(logicalDevice);

				if (result.IsValid())
				{
#if RENDERER_WEBGPU_DAWN
					wgpuQueueAddRef(result);
#else
					wgpuQueueReference(result);
#endif
				}
			}
		);
		return result;
#endif
	}

	void LogicalDevice::WaitUntilIdle() const
	{
#if RENDERER_VULKAN
		vkDeviceWaitIdle(m_logicalDevice);
#elif RENDERER_METAL
		for (const CommandQueueView commandQueue : m_commandQueues)
		{
			commandQueue.WaitUntilIdle();
		}
#elif RENDERER_WEBGPU
		Threading::Atomic<bool> completed{false};
		Rendering::Window::QueueOnWindowThread(
			[logicalDevice = m_logicalDevice, &completed]()
			{
				wgpuQueueOnSubmittedWorkDone(
					wgpuDeviceGetQueue(logicalDevice),
					[](const WGPUQueueWorkDoneStatus, void* pUserData)
					{
						Threading::Atomic<bool>& completed = *reinterpret_cast<Threading::Atomic<bool>*>(pUserData);
						completed = true;
					},
					&completed
				);
			}
		);

		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
		while (!completed)
		{
			thread.DoRunNextJob();
		}

#else
		Assert(false, "TODO");
#endif
	}

	inline LogicalDevice::CommandQueueContainer GetCommandQueues(const PhysicalDevice& physicalDevice, const LogicalDeviceView device)
	{
		InlineVector<CommandQueueView, 4, QueueFamilyIndex> queues{
			Memory::ConstructWithSize,
			Memory::DefaultConstruct,
			physicalDevice.GetQueueFamilyCount()
		};
		for (uint8 i = 0; i < (uint8)QueueFamily::Count; ++i)
		{
			const QueueFamily family = static_cast<QueueFamily>(1 << i);
			const QueueFamilyIndex queueIndex = physicalDevice.GetQueueFamily(family);
			if (!queues[queueIndex].IsValid())
			{
				queues[queueIndex] = CreateCommandQueue(device, queueIndex);
			}
		}

		LogicalDevice::CommandQueueContainer result = {};
		for (uint8 i = 0; i < (uint8)QueueFamily::Count; ++i)
		{
			const QueueFamily family = static_cast<QueueFamily>(1 << i);
			const QueueFamilyIndex queueIndex = physicalDevice.GetQueueFamily(family);
			result[(QueueFamily)i] = queues[queueIndex];
		}

		return result;
	}

	inline LogicalDevice::QueueSubmissionJobContainer GetQueueSubmissionJobs(LogicalDevice& logicalDevice)
	{
		LogicalDevice::QueueSubmissionJobContainer container;

		for (UniquePtr<QueueSubmissionJob>& job : container)
		{
			const ArrayView<UniquePtr<QueueSubmissionJob>, uint8> otherJobs =
				container.GetSubView(0, (uint8)container.GetIteratorIndex(Memory::GetAddressOf(job)));
			const CommandQueueView commandQueueView =
				logicalDevice.m_commandQueues[(QueueFamily)container.GetIteratorIndex(Memory::GetAddressOf(job))];

			const OptionalIterator<UniquePtr<QueueSubmissionJob>> pExistingQueueJob = otherJobs.FindIf(
				[commandQueueView](const UniquePtr<QueueSubmissionJob>& pExistingJob)
				{
					return pExistingJob != nullptr && pExistingJob->GetQueue() == commandQueueView;
				}
			);
			if (!pExistingQueueJob.IsValid())
			{
				job.CreateInPlace(logicalDevice, commandQueueView);
			}
		}

		return container;
	}

	inline LogicalDevice::QueueSubmissionJobViewContainer GetQueueSubmissionJobView(LogicalDevice& logicalDevice)
	{
		LogicalDevice::QueueSubmissionJobViewContainer container;

		for (QueueSubmissionJob*& job : container)
		{
			const ArrayView<UniquePtr<QueueSubmissionJob>, uint8, QueueFamily> otherJobs =
				logicalDevice.m_queueSubmissionJobs.GetSubView(0, (uint8)container.GetIteratorIndex(Memory::GetAddressOf(job)) + 1);
			const CommandQueueView commandQueueView = logicalDevice.m_commandQueues[container.GetIteratorIndex(Memory::GetAddressOf(job))];

			const OptionalIterator<UniquePtr<QueueSubmissionJob>> pExistingQueueJob = otherJobs.FindIf(
				[commandQueueView](const UniquePtr<QueueSubmissionJob>& pExistingJob)
				{
					return pExistingJob != nullptr && pExistingJob->GetQueue() == commandQueueView;
				}
			);
			Assert(pExistingQueueJob.IsValid());
			job = &**pExistingQueueJob;
		}

		return container;
	}

	PURE_STATICS Renderer& LogicalDevice::GetRenderer()
	{
		return System::Get<Renderer>();
	}

	PURE_STATICS const Renderer& LogicalDevice::GetRenderer() const
	{
		return System::Get<Renderer>();
	}

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
	inline static constexpr size TotalPushConstantDataSize = LogicalDevice::MaximumPushConstantInstanceCount *
	                                                         LogicalDevice::MaximumPushConstantInstanceDataSize;
#endif

	LogicalDevice::LogicalDevice(
		[[maybe_unused]] const InstanceView instance,
		const PhysicalDevice& physicalDevice,
		const Optional<uint8> presentQueueIndex,
		const LogicalDeviceIdentifier identifier,
		const LogicalDeviceView device
	)
		: m_identifier(identifier)
		, m_presentQueueIndex(presentQueueIndex)
		, m_physicalDevice(physicalDevice)
		, m_logicalDevice(device)
		, m_commandQueues(GetCommandQueues(physicalDevice, m_logicalDevice))
		, m_presentCommandQueue(
				presentQueueIndex.IsValid() ? m_commandQueues[QueueFamily(*presentQueueIndex)] : GetCommandQueue(QueueFamily::Graphics)
			)
		, m_queueSubmissionJobs(GetQueueSubmissionJobs(*this))
		, m_queueSubmissionJobsView(GetQueueSubmissionJobView(*this))
		, m_deviceMemoryPool(*this, physicalDevice)
#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		, m_pushConstantsBuffer(*this, physicalDevice, m_deviceMemoryPool, TotalPushConstantDataSize)
#endif
	{
#if RENDERER_WEBGPU
		[[maybe_unused]] WGPUQueue pQueue = GetCommandQueue(QueueFamily::Graphics);
#endif

#if ENABLE_NVIDIA_GPU_CHECKPOINTS
		m_vkCmdSetCheckpointNV = vkGetDeviceProcAddr(m_logicalDevice, "vkCmdSetCheckpointNV");
		m_vkGetQueueCheckpointDataNV = vkGetDeviceProcAddr(m_logicalDevice, "vkGetQueueCheckpointDataNV");
#endif

#if ENABLE_VULKAN_DEVICE_DEBUG_UTILS
#if PLATFORM_APPLE
		m_vkCmdBeginDebugUtilsLabelEXT = (void*)vkCmdBeginDebugUtilsLabelEXT;
		m_vkCmdEndDebugUtilsLabelEXT = (void*)vkCmdEndDebugUtilsLabelEXT;
		m_vkSetDebugUtilsObjectNameEXT = (void*)vkSetDebugUtilsObjectNameEXT;
#else
		m_vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<void*>(vkGetDeviceProcAddr(m_logicalDevice, "vkCmdBeginDebugUtilsLabelEXT"));
		m_vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<void*>(vkGetDeviceProcAddr(m_logicalDevice, "vkCmdEndDebugUtilsLabelEXT"));
		m_vkSetDebugUtilsObjectNameEXT = reinterpret_cast<void*>(vkGetDeviceProcAddr(m_logicalDevice, "vkSetDebugUtilsObjectNameEXT"));
		if (m_vkCmdBeginDebugUtilsLabelEXT == nullptr)
		{
			m_vkCmdBeginDebugUtilsLabelEXT = reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCmdBeginDebugUtilsLabelEXT"));
			m_vkCmdEndDebugUtilsLabelEXT = reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkCmdEndDebugUtilsLabelEXT"));
			m_vkSetDebugUtilsObjectNameEXT = reinterpret_cast<void*>(vkGetInstanceProcAddr(instance, "vkSetDebugUtilsObjectNameEXT"));
		}
#endif
#endif

#if RENDERER_VULKAN
		m_vkCmdSetCullModeEXT = reinterpret_cast<void*>(vkGetDeviceProcAddr(m_logicalDevice, "vkCmdSetCullModeEXT"));
		m_vkGetBufferDeviceAddressKHR = reinterpret_cast<void*>(vkGetDeviceProcAddr(m_logicalDevice, "vkGetBufferDeviceAddressKHR"));
#endif

#if RENDERER_VULKAN
		if (physicalDevice.GetSupportedFeatures().IsSet(PhysicalDeviceFeatures::BufferDeviceAddress))
		{
			m_vkGetBufferDeviceAddressKHR = reinterpret_cast<void*>(vkGetDeviceProcAddr(m_logicalDevice, "vkGetBufferDeviceAddressKHR"));
		}

		if (physicalDevice.GetSupportedFeatures().IsSet(PhysicalDeviceFeatures::AccelerationStructure))
		{
			m_vkGetAccelerationStructureBuildSizesKHR =
				reinterpret_cast<void*>(vkGetDeviceProcAddr(m_logicalDevice, "vkGetAccelerationStructureBuildSizesKHR"));
			m_vkGetAccelerationStructureDeviceAddressKHR =
				reinterpret_cast<void*>(vkGetDeviceProcAddr(m_logicalDevice, "vkGetAccelerationStructureDeviceAddressKHR"));
			m_vkCreateAccelerationStructureKHR = reinterpret_cast<void*>(vkGetDeviceProcAddr(m_logicalDevice, "vkCreateAccelerationStructureKHR")
			);
			m_vkDestroyAccelerationStructureKHR =
				reinterpret_cast<void*>(vkGetDeviceProcAddr(m_logicalDevice, "vkDestroyAccelerationStructureKHR"));
			m_vkCmdBuildAccelerationStructuresKHR =
				reinterpret_cast<void*>(vkGetDeviceProcAddr(m_logicalDevice, "vkCmdBuildAccelerationStructuresKHR"));
		}
#endif
	}

	LogicalDevice::~LogicalDevice()
	{
		OnDestroyed(*this, m_identifier);

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		m_pushConstantsBuffer.UnmapFromHostMemory(*this);
		m_pushConstantsBuffer.Destroy(*this, m_deviceMemoryPool);
#endif

		for (UniquePtr<QueueSubmissionJob>& submissionJob : m_queueSubmissionJobs)
		{
			submissionJob.DestroyElement();
		}

		m_shaderCache.Destroy(m_logicalDevice);

		m_deviceMemoryPool.Destroy(m_logicalDevice);

#if RENDERER_VULKAN
		if (m_logicalDevice != nullptr)
		{
			vkDestroyDevice(m_logicalDevice, nullptr);
		}
#elif RENDERER_WEBGPU
		WGPUQueue pQueue = m_commandQueues[QueueFamily::Graphics];
		WGPUDevice pDevice = m_logicalDevice;
		if (pQueue != nullptr || pDevice != nullptr)
		{
			Rendering::Window::QueueOnWindowThread(
				[pQueue, pDevice]()
				{
					if (pQueue != nullptr)
					{
						wgpuQueueRelease(pQueue);
					}
					if (pDevice != nullptr)
					{
						wgpuDeviceRelease(pDevice);
					}
				}
			);
		}
#endif
	}

	/* static */ LogicalDeviceView LogicalDevice::CreateDevice(
		[[maybe_unused]] const PhysicalDevice& physicalDevice,
		[[maybe_unused]] const Optional<uint8> presentQueueIndex,
		[[maybe_unused]] const ArrayView<const ConstZeroTerminatedStringView, uint8> requestedExtensions
	)
	{
#if RENDERER_VULKAN
		FlatVector<VkDeviceQueueCreateInfo, (uint8)QueueFamily::Count> queueCreateInfo;

		const QueueFamilyIndex graphicsQueueIndex = physicalDevice.GetQueueFamily(QueueFamily::Graphics);
		constexpr float graphicsQueuePriority = 1.0f;
		constexpr float presentQueuePriority = 1.0f;
		constexpr float transferQueuePriority = 0.9f;
		constexpr float computeQueuePriority = 0.9f;
		queueCreateInfo.EmplaceBack(VkDeviceQueueCreateInfo{
			VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
			nullptr,
			0,
			graphicsQueueIndex,
			1,
			&graphicsQueuePriority,
		});

		const bool sharingPresentAndGraphicsQueues = presentQueueIndex.IsValid() && (presentQueueIndex.Get() == graphicsQueueIndex);
		if (!sharingPresentAndGraphicsQueues && presentQueueIndex.IsValid())
		{
			queueCreateInfo.EmplaceBack(
				VkDeviceQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, presentQueueIndex.Get(), 1, &presentQueuePriority}
			);
		}

		const QueueFamilyIndex transferQueueIndex = physicalDevice.GetQueueFamily(QueueFamily::Transfer);
		const bool sharingTransferAndGraphicsQueues = transferQueueIndex == graphicsQueueIndex;
		if (!sharingTransferAndGraphicsQueues)
		{
			queueCreateInfo.EmplaceBack(
				VkDeviceQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, transferQueueIndex, 1, &transferQueuePriority}
			);
		}

		const QueueFamilyIndex computeQueueIndex = physicalDevice.GetQueueFamily(QueueFamily::Compute);
		const bool sharingComputeAndGraphicsQueues = computeQueueIndex == graphicsQueueIndex;
		const bool sharingComputeAndTransferQueues = computeQueueIndex == transferQueueIndex;
		if ((!sharingComputeAndGraphicsQueues) & (!sharingComputeAndTransferQueues))
		{
			queueCreateInfo.EmplaceBack(
				VkDeviceQueueCreateInfo{VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO, nullptr, 0, computeQueueIndex, 1, &computeQueuePriority}
			);
		}

		// TODO: Allow specifying per device instance. Currently enabling all detected supported features.
		const EnumFlags<PhysicalDeviceFeatures> requestedDeviceFeatures = physicalDevice.GetSupportedFeatures();

		VkPhysicalDeviceFeatures2 deviceFeatures2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2, nullptr};

		deviceFeatures2.features.geometryShader = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::GeometryShader);
		deviceFeatures2.features.tessellationShader = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TesselationShader);
		deviceFeatures2.features.textureCompressionBC = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionBC);
		deviceFeatures2.features.textureCompressionASTC_LDR = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR);
		deviceFeatures2.features.depthClamp = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::DepthClamp);
		deviceFeatures2.features.depthBounds = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::DepthBounds);
		deviceFeatures2.features.depthBiasClamp = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::DepthBiasClamp);
		deviceFeatures2.features.imageCubeArray = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::CubemapArrays);
		deviceFeatures2.features.fillModeNonSolid = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::NonSolidFillMode);
		deviceFeatures2.features.shaderInt16 = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::ShaderInt16);
		deviceFeatures2.features.shaderInt64 = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::ShaderInt64);
		deviceFeatures2.features.fragmentStoresAndAtomics = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::FragmentStoresAndAtomics);
		deviceFeatures2.features.vertexPipelineStoresAndAtomics =
			requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::VertexPipelineStoresAndAtomics);
		deviceFeatures2.features.independentBlend = true;

		VkPhysicalDeviceVulkan12Features deviceFeaturesVulkan_1_2{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES, nullptr};
		if (requestedDeviceFeatures.AreAnySet(
					PhysicalDeviceFeatures::ShaderFloat16 | PhysicalDeviceFeatures::BufferDeviceAddress | PhysicalDeviceFeatures::DescriptorIndexing |
					PhysicalDeviceFeatures::SeparateDepthStencilLayout
				))
		{
			deviceFeaturesVulkan_1_2.shaderFloat16 = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::ShaderFloat16);
			deviceFeaturesVulkan_1_2.bufferDeviceAddress = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::BufferDeviceAddress);
			deviceFeaturesVulkan_1_2.descriptorIndexing = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::DescriptorIndexing);
			deviceFeaturesVulkan_1_2.descriptorBindingPartiallyBound =
				requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::PartiallyBoundDescriptorBindings);
			deviceFeaturesVulkan_1_2.runtimeDescriptorArray = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::RuntimeDescriptorArrays);
			deviceFeaturesVulkan_1_2.descriptorBindingSampledImageUpdateAfterBind =
				requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::UpdateDescriptorSampleImageAfterBind);
			deviceFeaturesVulkan_1_2.shaderSampledImageArrayNonUniformIndexing =
				requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::NonUniformImageArrayIndexing);
			deviceFeaturesVulkan_1_2.separateDepthStencilLayouts =
				requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::SeparateDepthStencilLayout);
			deviceFeatures2.pNext = &deviceFeaturesVulkan_1_2;
		}

		VkPhysicalDeviceRayTracingPipelineFeaturesKHR rayTracingPipelineFeatures{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_TRACING_PIPELINE_FEATURES_KHR,
			deviceFeatures2.pNext
		};
		if (requestedDeviceFeatures.AreAnySet(PhysicalDeviceFeatures::RayTracingPipeline))
		{
			rayTracingPipelineFeatures.rayTracingPipeline = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::RayTracingPipeline);
			deviceFeatures2.pNext = &rayTracingPipelineFeatures;
		}

		VkPhysicalDeviceAccelerationStructureFeaturesKHR accelerationStructureFeatures{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_ACCELERATION_STRUCTURE_FEATURES_KHR,
			deviceFeatures2.pNext
		};
		if (requestedDeviceFeatures.AreAnySet(
					PhysicalDeviceFeatures::AccelerationStructure | PhysicalDeviceFeatures::AccelerationStructureHostCommands
				))
		{
			accelerationStructureFeatures.accelerationStructure = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::AccelerationStructure);
			accelerationStructureFeatures.accelerationStructureHostCommands =
				requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::AccelerationStructureHostCommands);
			deviceFeatures2.pNext = &accelerationStructureFeatures;
		}

		VkPhysicalDeviceRayQueryFeaturesKHR rayQueryFeatures{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_RAY_QUERY_FEATURES_KHR, deviceFeatures2.pNext};
		if (requestedDeviceFeatures.AreAnySet(PhysicalDeviceFeatures::RayQuery))
		{
			rayQueryFeatures.rayQuery = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::RayQuery);
			deviceFeatures2.pNext = &rayQueryFeatures;
		}

		VkPhysicalDeviceExtendedDynamicStateFeaturesEXT extendedDynamicStateFeatures{
			VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_EXTENDED_DYNAMIC_STATE_FEATURES_EXT,
			deviceFeatures2.pNext
		};
		if (requestedDeviceFeatures.AreAnySet(PhysicalDeviceFeatures::ExtendedDynamicState))
		{
			extendedDynamicStateFeatures.extendedDynamicState = requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::ExtendedDynamicState);
			deviceFeatures2.pNext = &extendedDynamicStateFeatures;
		}

		const bool needsSwapchain = presentQueueIndex.IsValid();

		uint32 supportedExtensionCount;
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionCount, nullptr);
		FixedSizeVector<VkExtensionProperties, uint16>
			supportedExtensions(Memory::ConstructWithSize, Memory::Uninitialized, (uint16)supportedExtensionCount);
		vkEnumerateDeviceExtensionProperties(physicalDevice, nullptr, &supportedExtensionCount, supportedExtensions.GetData());

		InlineVector<const char*, 20> enabledExtensions;

		auto addExtension =
			[supportedExtensions = supportedExtensions.GetView(), &enabledExtensions](ConstZeroTerminatedStringView extensionName)
		{
			const bool isSupported = supportedExtensions.ContainsIf(
				[extensionName](const VkExtensionProperties& extension) -> bool
				{
					return ConstStringView(extension.extensionName, (uint32)strlen(extension.extensionName)) == extensionName;
				}
			);
			if (isSupported)
			{
				enabledExtensions.EmplaceBack(extensionName);
			}
			return isSupported;
		};

		for (const ConstZeroTerminatedStringView requestedExtension : requestedExtensions)
		{
			[[maybe_unused]] const bool wasAdded = addExtension(requestedExtension);
			Assert(wasAdded);
		}

		if (needsSwapchain)
		{
			[[maybe_unused]] const bool wasAdded = addExtension(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
			Assert(wasAdded);
		}

		if constexpr (ENABLE_NVIDIA_GPU_CHECKPOINTS)
		{
			addExtension(VK_NV_DEVICE_DIAGNOSTIC_CHECKPOINTS_EXTENSION_NAME);
		}

		// Optional: raytracing
		{
			addExtension(VK_KHR_ACCELERATION_STRUCTURE_EXTENSION_NAME);
			addExtension(VK_KHR_RAY_TRACING_PIPELINE_EXTENSION_NAME);

			// Required by VK_KHR_acceleration_structure
			addExtension(VK_KHR_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME);
			addExtension(VK_KHR_DEFERRED_HOST_OPERATIONS_EXTENSION_NAME);
			addExtension(VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME);

			addExtension(VK_KHR_RAY_QUERY_EXTENSION_NAME);

			// Required for VK_KHR_ray_tracing_pipeline
			addExtension(VK_KHR_SPIRV_1_4_EXTENSION_NAME);

			// Required by VK_KHR_spirv_1_4
			addExtension(VK_KHR_SHADER_FLOAT_CONTROLS_EXTENSION_NAME);
		}

		// Optional: extended dynamic state
		addExtension(VK_EXT_EXTENDED_DYNAMIC_STATE_EXTENSION_NAME);

		const VkDeviceCreateInfo createInfo = {
			VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
			&deviceFeatures2,
			0,
			queueCreateInfo.GetSize(),
			queueCreateInfo.GetData(),
			0,
			nullptr,
			enabledExtensions.GetSize(),
			enabledExtensions.GetData(),
			nullptr
		};

		VkDevice pDevice = nullptr;
		[[maybe_unused]] const VkResult result = vkCreateDevice(physicalDevice, &createInfo, nullptr, &pDevice);
		Assert(result == VK_SUCCESS);
		return pDevice;
#elif RENDERER_METAL
		return (id<MTLDevice>)physicalDevice;
#elif RENDERER_WEBGPU
		struct Data
		{
			WGPUDevice device;
			Threading::Atomic<bool> finished{false};
		};
		Data data{};

		/*const WGPURequiredLimits requiredLimits
		{
		  nullptr,
		  WGPULimits
		  {
		  }
		};*/

		// TODO: Allow specifying per device instance. Currently enabling all detected supported features.
		const EnumFlags<PhysicalDeviceFeatures> requestedDeviceFeatures = physicalDevice.GetSupportedFeatures();

		InlineVector<WGPUFeatureName, 1> requiredFeatures(Memory::Reserve, requestedDeviceFeatures.GetNumberOfSetFlags());

		// TODO: We currently rely on these, expose as features?
		requiredFeatures.EmplaceBack(WGPUFeatureName_Depth32FloatStencil8);
		// Temp: We have to stop relying on this, or detect support dynamically.
		// Not available on Android (at least at the moment)
		switch (Platform::GetClass())
		{
			case Platform::Class::Desktop:
				requiredFeatures.EmplaceBack(WGPUFeatureName_BGRA8UnormStorage);
				break;
			case Platform::Class::Mobile:
			case Platform::Class::Spatial:
				break;
		}
		
#if RENDERER_WEBGPU_DAWN
		requiredFeatures.EmplaceBack(WGPUFeatureName_ImplicitDeviceSynchronization);
#endif

		if (requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::DepthClamp))
		{
			requiredFeatures.EmplaceBack(WGPUFeatureName_DepthClipControl);
		}
		if (requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionBC))
		{
			requiredFeatures.EmplaceBack(WGPUFeatureName_TextureCompressionBC);
		}
		if (requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionETC2))
		{
			requiredFeatures.EmplaceBack(WGPUFeatureName_TextureCompressionASTC);
		}
		if (requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR))
		{
			requiredFeatures.EmplaceBack(WGPUFeatureName_TextureCompressionASTC);
		}
		if (requestedDeviceFeatures.IsSet(PhysicalDeviceFeatures::ShaderFloat16))
		{
			requiredFeatures.EmplaceBack(WGPUFeatureName_ShaderF16);
		}

		WGPUChainedStruct* pNext = nullptr;

#if RENDERER_WEBGPU_DAWN
		Array<const char*, 2> enabledToggles{"use_user_defined_labels_in_backend", "disable_symbol_renaming"};
		Array<const char*, 0> disabledToggles{};

		WGPUDawnTogglesDescriptor togglesDescriptor{
			WGPUChainedStruct{
				nullptr,
				WGPUSType_DawnTogglesDescriptor,
			},
			enabledToggles.GetSize(),
			enabledToggles.GetData(),
			disabledToggles.GetSize(),
			disabledToggles.GetData()
		};
		pNext = &togglesDescriptor.chain;
#endif
		
#if RENDERER_WEBGPU_DAWN
		[[maybe_unused]] const WGPUUncapturedErrorCallback uncapturedErrorCallback =
			[]([[maybe_unused]] WGPUDevice const * device, const WGPUErrorType type, const WGPUStringView message, [[maybe_unused]] void* pUserData1, [[maybe_unused]] void* pUserData2)
		{
			switch (type)
			{
				case WGPUErrorType_NoError:
					LogMessage("(WebGPU Uncaptured Message): {}", ConstStringView{ message.data, (uint32)message.length });
					break;
				case WGPUErrorType_Validation:
					LogError("(WebGPU Validation): {}", ConstStringView{ message.data, (uint32)message.length });
					BreakIfDebuggerIsAttached();
					break;
				case WGPUErrorType_OutOfMemory:
					AssertMessage(false, "(WebGPU Out of memory): {}", ConstStringView{ message.data, (uint32)message.length });
					break;
				case WGPUErrorType_Internal:
					AssertMessage(false, "(WebGPU Internal): {}", ConstStringView{ message.data, (uint32)message.length });
					break;
				case WGPUErrorType_Unknown:
					AssertMessage(false, "(WebGPU Unknown): {}", ConstStringView{ message.data, (uint32)message.length });
					break;
				case WGPUErrorType_DeviceLost:
					AssertMessage(false, "(WebGPU Device Lost): {}", ConstStringView{ message.data, (uint32)message.length });
					break;
				case WGPUErrorType_Force32:
					ExpectUnreachable();
			}
		};
#else
		[[maybe_unused]] const WGPUUncapturedErrorCallback uncapturedErrorCallback =
			[](const WGPUErrorType type, const char* message, [[maybe_unused]] void* pUserData)
		{
			switch (type)
			{
				case WGPUErrorType_NoError:
					LogMessage("(WebGPU Uncaptured Message): {}", message);
					break;
				case WGPUErrorType_Validation:
					LogError("(WebGPU Validation): {}", message);
					BreakIfDebuggerIsAttached();
					break;
				case WGPUErrorType_OutOfMemory:
					AssertMessage(false, "(WebGPU Out of memory): {}", message);
					break;
				case WGPUErrorType_Internal:
					AssertMessage(false, "(WebGPU Internal): {}", message);
					break;
				case WGPUErrorType_Unknown:
					AssertMessage(false, "(WebGPU Unknown): {}", message);
					break;
				case WGPUErrorType_DeviceLost:
					AssertMessage(false, "(WebGPU Device Lost): {}", message);
					break;
				case WGPUErrorType_Force32:
					ExpectUnreachable();
			}
		};
#endif

 #if RENDERER_WEBGPU_DAWN
		const auto deviceLostCallback = []([[maybe_unused]] WGPUDevice const * device, const WGPUDeviceLostReason reason, [[maybe_unused]] const WGPUStringView message, [[maybe_unused]] void* pUserData1, [[maybe_unused]] void* pUserData2)
		{
			switch (reason)
			{
#if RENDERER_WEBGPU_WGPU_NATIVE || RENDERER_WEBGPU_DAWN
				case WGPUDeviceLostReason_Unknown:
#else
				case WGPUDeviceLostReason_Undefined:
#endif
					AssertMessage(false, "(WebGPU Device Lost): {}", ConstStringView{ message.data, (uint32)message.length });
					break;
				case WGPUDeviceLostReason_Destroyed:
					AssertMessage(false, "(WebGPU Device Destroyed): {}", ConstStringView{ message.data, (uint32)message.length });
					BreakIfDebuggerIsAttached();
					break;
				case WGPUDeviceLostReason_InstanceDropped:
					AssertMessage(false, "(WebGPU Instance Dropped): {}", ConstStringView{ message.data, (uint32)message.length });
					BreakIfDebuggerIsAttached();
					break;
				case WGPUDeviceLostReason_FailedCreation:
					AssertMessage(false, "(WebGPU Failed Creation): {}", ConstStringView{ message.data, (uint32)message.length });
					BreakIfDebuggerIsAttached();
					break;
				case WGPUDeviceLostReason_Force32:
					ExpectUnreachable();
			}
		};
#else
 #if RENDERER_WEBGPU_DAWN
		const auto deviceLostCallback = [](const WGPUDeviceLostReason reason, [[maybe_unused]] char const * message, [[maybe_unused]] void* pUserData)
#else
		const auto deviceLostCallback = [](const WGPUDeviceLostReason reason, [[maybe_unused]] char const * message, [[maybe_unused]] void* pUserData)
#endif
		{
			switch (reason)
			{
#if RENDERER_WEBGPU_WGPU_NATIVE || RENDERER_WEBGPU_DAWN
				case WGPUDeviceLostReason_Unknown:
#else
				case WGPUDeviceLostReason_Undefined:
#endif
					AssertMessage(false, "(WebGPU Device Lost): {}", message);
					break;
				case WGPUDeviceLostReason_Destroyed:
					AssertMessage(false, "(WebGPU Device Destroyed): {}", message);
					BreakIfDebuggerIsAttached();
					break;
				case WGPUDeviceLostReason_Force32:
					ExpectUnreachable();
			}
		};
#endif

		const WGPUDeviceDescriptor deviceDescriptor
		{
			pNext,
#if RENDERER_WEBGPU_DAWN
			WGPUStringView { nullptr, 0 },
#else
			nullptr,
#endif
			requiredFeatures.GetSize(),
			requiredFeatures.GetData(),
			nullptr, // &requiredLimits
			WGPUQueueDescriptor{nullptr, "Default Queue"},
#if RENDERER_WEBGPU_DAWN
			WGPUDeviceLostCallbackInfo2
			{
				nullptr,
				WGPUCallbackMode_AllowSpontaneous,
				deviceLostCallback,
				nullptr,
				nullptr
			},
#else
			deviceLostCallback,
			nullptr,
#endif

#if RENDERER_WEBGPU_DAWN || RENDERER_WEBGPU_WGPU_NATIVE
			WGPUUncapturedErrorCallbackInfo2
			{
				nullptr,
				uncapturedErrorCallback,
				nullptr,
#if RENDERER_WEBGPU_DAWN
				nullptr
#endif
			}
#endif
		};

		Rendering::Window::QueueOnWindowThread(
			[physicalDevice = (WGPUAdapter)physicalDevice, deviceDescriptor, &data]()
			{
#if RENDERER_WEBGPU_DAWN
				const auto callback = [](
					[[maybe_unused]] const WGPURequestDeviceStatus status,
					const WGPUDevice device,
					[[maybe_unused]] const WGPUStringView message,
					void* pUserData
				)
#else
				const auto callback = [](
					[[maybe_unused]] const WGPURequestDeviceStatus status,
					const WGPUDevice device,
					[[maybe_unused]] char const * message,
					void* pUserData
				)
#endif
				{
					Assert(status == WGPURequestDeviceStatus_Success);
					Data& data = *reinterpret_cast<Data*>(pUserData);
					Assert(device != nullptr);
					if (LIKELY(device != nullptr))
					{
#if RENDERER_WEBGPU_DAWN
						wgpuDeviceAddRef(device);
#else
						wgpuDeviceReference(device);
#endif
						data.device = device;
					}

					data.finished = true;
				};

				wgpuAdapterRequestDevice(
					physicalDevice,
					&deviceDescriptor,
					callback,
					&data
				);
			}
		);

		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
		while (!data.finished)
		{
			thread.DoRunNextJob();
		}

#if RENDERER_WEBGPU_DAWN
		wgpuDeviceSetLoggingCallback(
			data.device,
			[](const WGPULoggingType type, const WGPUStringView message, [[maybe_unused]] void* pUserData)
			{
				switch (type)
				{
					case WGPULoggingType_Verbose:
						LogMessage("(WebGPU trace log): {}", ConstStringView{ message.data, (uint32)message.length });
						break;
					case WGPULoggingType_Info:
						LogMessage("(WebGPU info): {}", ConstStringView{ message.data, (uint32)message.length });
						break;
					case WGPULoggingType_Warning:
						LogWarning("(WebGPU warning): {}", ConstStringView{ message.data, (uint32)message.length });
						break;
					case WGPULoggingType_Error:
						LogError("(WebGPU error): {}", ConstStringView{ message.data, (uint32)message.length });
						BreakIfDebuggerIsAttached();
						break;
					case WGPULoggingType_Force32:
						ExpectUnreachable();
				}
			},
			nullptr
		);
#elif RENDERER_WEBGPU_WGPU_NATIVE
		wgpuSetLogCallback(
			[](const WGPULogLevel type, const char* message, [[maybe_unused]] void* pUserData)
			{
				switch (type)
				{
					case WGPULogLevel_Trace:
						LogMessage("(WebGPU trace log): {}", message);
						break;
					case WGPULogLevel_Info:
						LogMessage("(WebGPU info): {}", message);
						break;
					case WGPULogLevel_Warn:
						LogWarning("(WebGPU warning): {}", message);
						break;
					case WGPULogLevel_Error:
						LogError("(WebGPU error): {}", message);
						BreakIfDebuggerIsAttached();
						break;
					case WGPULogLevel_Debug:
						LogError("(WebGPU debug): {}", message);
						break;
					case WGPULogLevel_Off:
						break;
					case WGPULogLevel_Force32:
						ExpectUnreachable();
				}
			},
			nullptr
		);
#endif

		return LogicalDeviceView{data.device};
#else
		Assert(false, "TODO");
		return {};
#endif
	}

	PURE_LOCALS_AND_POINTERS QueueFamilyIndex LogicalDevice::GetPresentQueueIndex() const
	{
		return m_presentQueueIndex.IsValid() ? *m_presentQueueIndex : m_physicalDevice.GetQueueFamily(QueueFamily::Graphics);
	}

	void LogicalDevice::PauseQueueSubmissions()
	{
		for (UniquePtr<QueueSubmissionJob>& pQueueSubmissionJob : m_queueSubmissionJobs)
		{
			if (pQueueSubmissionJob.IsValid())
			{
				pQueueSubmissionJob->Pause();
			}
		}
	}

	void LogicalDevice::ResumeQueueSubmissions()
	{
		for (UniquePtr<QueueSubmissionJob>& pQueueSubmissionJob : m_queueSubmissionJobs)
		{
			if (pQueueSubmissionJob.IsValid())
			{
				pQueueSubmissionJob->Resume();
			}
		}
	}

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
	thread_local uint32 pushConstantNextInstanceIndex = 0;

	void LogicalDevice::ReservePushConstantsInstanceCount(const uint32 maximumPushConstantInstanceCount)
	{
		const uint32 startInstanceIndex = m_reservedPushConstantsInstanceIndex.FetchAdd(maximumPushConstantInstanceCount);
		Assert(startInstanceIndex + maximumPushConstantInstanceCount <= LogicalDevice::MaximumPushConstantInstanceCount);
		pushConstantNextInstanceIndex = startInstanceIndex;
		m_reservedPushConstantsInstanceCount += maximumPushConstantInstanceCount;
	}

	uint32 LogicalDevice::AcquirePushConstantsInstance(const ConstByteView sourceData)
	{
		Assert(RENDERER_WEBGPU, "Not supported for other graphics APIs");

		Assert(m_reservedPushConstantsInstanceCount-- > 0);
		Assert(sourceData.GetDataSize() <= MaximumPushConstantInstanceDataSize);
		const uint32 instanceIndex = pushConstantNextInstanceIndex++;
		const uint32 instanceOffset = uint32(instanceIndex * MaximumPushConstantInstanceDataSize);

		using StorageType = FixedSizeInlineVector<ByteType, MaximumPushConstantInstanceDataSize>;

		const ArrayView<const ByteType, size> sourceDataView{sourceData.GetData(), sourceData.GetDataSize()};
		const BufferView pushConstantsBuffer = m_pushConstantsBuffer;

#if RENDERER_WEBGPU
		GetQueueSubmissionJob(QueueFamily::Graphics)
			.QueueCallback(
				[commandQueue = GetCommandQueue(QueueFamily::Graphics),
		     pushConstantsBuffer,
		     instanceOffset,
		     sourceData = StorageType(sourceDataView)]()
				{
					wgpuQueueWriteBuffer(commandQueue, pushConstantsBuffer, instanceOffset, sourceData.GetData(), sourceData.GetDataSize());
				}
			);
#endif

		return instanceOffset;
	}
#endif
}
