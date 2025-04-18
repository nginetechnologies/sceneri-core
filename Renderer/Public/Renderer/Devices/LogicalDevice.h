#pragma once

#include <Renderer/Constants.h>
#include "LogicalDeviceView.h"
#include "DeviceMemoryPool.h"
#include <Renderer/Devices/LogicalDeviceIdentifier.h>
#include <Renderer/Commands/CommandQueueView.h>
#include <Renderer/Devices/QueueFamily.h>

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
#include <Renderer/Buffers/StorageBuffer.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Common/Threading/AtomicInteger.h>
#endif

#include <Common/Memory/Optional.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/ForwardDeclarations/String.h>
#include <Common/Function/Event.h>

#include <Renderer/Assets/Shader/ShaderCache.h>

namespace ngine
{
	struct Engine;

	namespace Math
	{
		template<typename T>
		struct TVector2;

		using Vector2ui = TVector2<uint32>;
	}
}

namespace ngine::Rendering
{
	struct InstanceView;
	struct PhysicalDevice;
	struct Renderer;
	struct Buffer;
	struct QueueSubmissionJob;
	struct CommandEncoderView;

	struct LogicalDevice final
	{
		[[nodiscard]] PURE_STATICS Renderer& GetRenderer();
		[[nodiscard]] PURE_STATICS const Renderer& GetRenderer() const;

		LogicalDevice(
			const InstanceView instance,
			const PhysicalDevice& __restrict physicalDevice,
			const Optional<uint8> presentQueueIndex,
			const LogicalDeviceIdentifier identifier,
			const LogicalDeviceView device
		);
		LogicalDevice(const LogicalDevice&) = delete;
		LogicalDevice& operator=(const LogicalDevice&) = delete;
		LogicalDevice(LogicalDevice&& other) = delete;
		LogicalDevice& operator=(LogicalDevice&&) = delete;
		~LogicalDevice();

		[[nodiscard]] static LogicalDeviceView CreateDevice(
			const PhysicalDevice& __restrict physicalDevice,
			const Optional<uint8> presentQueueIndex,
			const ArrayView<const ConstZeroTerminatedStringView, uint8> enabledExtensions = {}
		);

		Event<void(void*, LogicalDevice&, const LogicalDeviceIdentifier), 24> OnDestroyed;

		[[nodiscard]] PURE_LOCALS_AND_POINTERS const PhysicalDevice& GetPhysicalDevice() const
		{
			return m_physicalDevice;
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS operator LogicalDeviceView() const
		{
			return m_logicalDevice;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS bool IsValid() const
		{
			return m_logicalDevice.IsValid();
		}

#if RENDERER_VULKAN
		[[nodiscard]] PURE_LOCALS_AND_POINTERS operator VkDevice() const
		{
			return m_logicalDevice;
		}
#elif RENDERER_METAL
		[[nodiscard]] PURE_LOCALS_AND_POINTERS operator id<MTLDevice>() const
		{
			return m_logicalDevice;
		}
#elif RENDERER_WEBGPU
		[[nodiscard]] PURE_LOCALS_AND_POINTERS operator WGPUDevice() const
		{
			return m_logicalDevice;
		}
#endif

		[[nodiscard]] PURE_LOCALS_AND_POINTERS LogicalDeviceIdentifier GetIdentifier() const
		{
			return m_identifier;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS QueueFamilyIndex GetPresentQueueIndex() const;

		[[nodiscard]] PURE_LOCALS_AND_POINTERS CommandQueueView GetCommandQueue(const QueueFamily family) const
		{
			return m_commandQueues[family >> 1];
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS QueueSubmissionJob& GetQueueSubmissionJob(const QueueFamily family) const
		{
			return *m_queueSubmissionJobsView[family >> 1];
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS QueueSubmissionJob& GetQueuePresentJob() const
		{
			return m_presentQueueIndex.IsValid() ? *m_queueSubmissionJobsView[(QueueFamily)*m_presentQueueIndex]
			                                     : *m_queueSubmissionJobsView[QueueFamily::Graphics >> 1];
		}
		void PauseQueueSubmissions();
		void ResumeQueueSubmissions();
		[[nodiscard]] PURE_LOCALS_AND_POINTERS CommandQueueView GetPresentCommandQueue() const
		{
			return m_presentCommandQueue;
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS ShaderCache& GetShaderCache()
		{
			return m_shaderCache;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const ShaderCache& GetShaderCache() const
		{
			return m_shaderCache;
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS DeviceMemoryPool& GetDeviceMemoryPool()
		{
			return m_deviceMemoryPool;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const DeviceMemoryPool& GetDeviceMemoryPool() const
		{
			return m_deviceMemoryPool;
		}

		void WaitUntilIdle() const;

		using CommandQueueContainer = Array<CommandQueueView, (uint8)QueueFamily::Count, QueueFamily>;

		using QueueSubmissionJobContainer = Array<UniquePtr<QueueSubmissionJob>, (uint8)QueueFamily::Count, QueueFamily>;
		using QueueSubmissionJobViewContainer = Array<QueueSubmissionJob*, (uint8)QueueFamily::Count, QueueFamily>;

#if ENABLE_NVIDIA_GPU_CHECKPOINTS
		[[nodiscard]] PURE_LOCALS_AND_POINTERS void* GetSetCheckpointNV() const
		{
			return m_vkCmdSetCheckpointNV;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS void* GetQueueCheckpointDataNV() const
		{
			return m_vkGetQueueCheckpointDataNV;
		}
#endif

#define ENABLE_VULKAN_DEVICE_DEBUG_UTILS (RENDERER_VULKAN && !PLATFORM_APPLE)
#if ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		[[nodiscard]] PURE_LOCALS_AND_POINTERS void* GetCmdBeginDebugUtilsLabelEXT() const
		{
			return m_vkCmdBeginDebugUtilsLabelEXT;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS void* GetCmdEndDebugUtilsLabelEXT() const
		{
			return m_vkCmdEndDebugUtilsLabelEXT;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS void* GetSetDebugUtilsObjectNameEXT() const
		{
			return m_vkSetDebugUtilsObjectNameEXT;
		}
#endif

#if RENDERER_VULKAN
		[[nodiscard]] PURE_LOCALS_AND_POINTERS void* GetCmdSetCullModeEXT() const
		{
			return m_vkCmdSetCullModeEXT;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS void* GetVkGetBufferDeviceAddress() const
		{
			return m_vkGetBufferDeviceAddressKHR;
		}
#endif

#if RENDERER_VULKAN
		[[nodiscard]] PURE_LOCALS_AND_POINTERS void* GetVkGetAccelerationStructureBuildSizes() const
		{
			return m_vkGetAccelerationStructureBuildSizesKHR;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS void* GetVkGetAccelerationStructureDeviceAddress() const
		{
			return m_vkGetAccelerationStructureDeviceAddressKHR;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS void* GetVkCreateAccelerationStructure() const
		{
			return m_vkCreateAccelerationStructureKHR;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS void* GetVkDestroyAccelerationStructure() const
		{
			return m_vkDestroyAccelerationStructureKHR;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS void* GetVkCmdBuildAccelerationStructuresKHR() const
		{
			return m_vkCmdBuildAccelerationStructuresKHR;
		}
#endif

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		inline static constexpr size MaximumPushConstantInstanceDataSize = 256;
		inline static constexpr size MaximumPushConstantInstanceCount = 1000000;

		[[nodiscard]] BufferView GetPushConstantsBuffer() const
		{
			return m_pushConstantsBuffer;
		}

		void ReservePushConstantsInstanceCount(const uint32 maximumPushConstantInstanceCount);
		[[nodiscard]] uint32 AcquirePushConstantsInstance(const ConstByteView data);
		void ResetPushConstants()
		{
			m_reservedPushConstantsInstanceIndex = 0;
			m_reservedPushConstantsInstanceCount = 0;
		}
#endif
	protected:
		friend Renderer;

		friend QueueSubmissionJobContainer GetQueueSubmissionJobs(LogicalDevice& logicalDevice);
		friend QueueSubmissionJobViewContainer GetQueueSubmissionJobView(LogicalDevice& logicalDevice);
	protected:
		const LogicalDeviceIdentifier m_identifier;

		Optional<uint8> m_presentQueueIndex;

		const PhysicalDevice& __restrict m_physicalDevice;
		LogicalDeviceView m_logicalDevice;

		CommandQueueContainer m_commandQueues;
		CommandQueueView m_presentCommandQueue;

		QueueSubmissionJobContainer m_queueSubmissionJobs;
		QueueSubmissionJobViewContainer m_queueSubmissionJobsView;

		friend ShaderCache;
		ShaderCache m_shaderCache;

		DeviceMemoryPool m_deviceMemoryPool;

#if ENABLE_NVIDIA_GPU_CHECKPOINTS
		void* m_vkCmdSetCheckpointNV = nullptr;
		void* m_vkGetQueueCheckpointDataNV = nullptr;
#endif

#if ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		void* m_vkCmdBeginDebugUtilsLabelEXT = nullptr;
		void* m_vkCmdEndDebugUtilsLabelEXT = nullptr;
		void* m_vkSetDebugUtilsObjectNameEXT = nullptr;
#endif

#if RENDERER_VULKAN
		void* m_vkCmdSetCullModeEXT = nullptr;
		void* m_vkGetBufferDeviceAddressKHR;

		void* m_vkGetAccelerationStructureBuildSizesKHR;
		void* m_vkGetAccelerationStructureDeviceAddressKHR;
		void* m_vkCreateAccelerationStructureKHR;
		void* m_vkDestroyAccelerationStructureKHR;
		void* m_vkCmdBuildAccelerationStructuresKHR;
#endif

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		StorageBuffer m_pushConstantsBuffer;
		Threading::Atomic<uint32> m_reservedPushConstantsInstanceIndex{0};
		Threading::Atomic<uint32> m_reservedPushConstantsInstanceCount{0};
#endif
	};
}
