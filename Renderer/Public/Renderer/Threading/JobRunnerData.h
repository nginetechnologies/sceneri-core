#pragma once

#include <Renderer/Commands/CommandPool.h>
#include <Renderer/Commands/CommandBuffer.h>

#include <Renderer/Descriptors/DescriptorPool.h>

#include <Renderer/Constants.h>
#include <Renderer/Devices/LogicalDeviceIdentifier.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/AtomicEnumFlags.h>

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
}

namespace ngine::Rendering
{
	struct DescriptorSetLayout;
	struct DescriptorSet;
	struct ImageMapping;
	struct Image;
	struct Buffer;
	struct Semaphore;
	struct Fence;
	struct PrimitiveAccelerationStructure;
	struct InstanceAccelerationStructure;

	struct JobRunnerData
	{
		[[nodiscard]] PURE_LOCALS_AND_POINTERS Threading::EngineJobRunnerThread& GetRunnerThread();
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const Threading::EngineJobRunnerThread& GetRunnerThread() const;

		JobRunnerData();
		JobRunnerData(const JobRunnerData&) = delete;
		JobRunnerData& operator=(const JobRunnerData&) = delete;
		JobRunnerData(JobRunnerData&&) = delete;
		JobRunnerData& operator=(JobRunnerData&&) = delete;
		~JobRunnerData();

		void Destroy();

		//! Called when an external party starts CPU work on a frame. Can be called multiple times per frame.
		void OnStartFrameCpuWork(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex);
		//! Called when an external party has finished processing a frame on CPU, should be called once for every time that OnStartFrameCpuWork
		//! was called
		void OnFinishFrameCpuWork(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex);

		void OnStartFrameGpuWork(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex);
		void OnFinishFrameGpuWork(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex);

		//! Requests a command buffer that will be valid for the extent of this frame.
		//! OnPerFrameCommandBufferFinishedExecution *must* be called when the command buffer execution completes.
		[[nodiscard]] CommandBufferView
		GetPerFrameCommandBuffer(const LogicalDeviceIdentifier logicalDeviceIdentifier, const QueueFamily queueFamily, const uint8 frameIndex);
		void OnPerFrameCommandBufferFinishedExecution(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex);

		void AwaitFrameFinish(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex);
		void AwaitFrameFinish(const uint8 frameIndex);
		void AwaitAnyFramesFinish(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameMask);
		void AwaitAnyFramesFinish(const uint8 frameMask);
		[[nodiscard]] bool IsProcessingFrame(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex) const;
		[[nodiscard]] bool IsProcessingFrame(const uint8 frameIndex) const;
		[[nodiscard]] bool IsProcessingAnyFrames(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameMask) const;
		[[nodiscard]] bool IsProcessingAnyFrames(const uint8 frameMask) const;

		[[nodiscard]] CommandPoolView GetCommandPool(const LogicalDeviceIdentifier logicalDeviceIdentifier, const QueueFamily queueFamily) const
		{
			return m_logicalDeviceData[logicalDeviceIdentifier]->m_commandPoolViews[(uint8)queueFamily >> 1];
		}
		[[nodiscard]] DescriptorPoolView GetDescriptorPool(const LogicalDeviceIdentifier logicalDeviceIdentifier) const
		{
			return m_logicalDeviceData[logicalDeviceIdentifier]->m_descriptorPool;
		}

		void DestroyDescriptorSet(const LogicalDeviceIdentifier logicalDeviceIdentifier, DescriptorSet&& descriptorSet);
		void DestroyDescriptorSets(const LogicalDeviceIdentifier logicalDeviceIdentifier, ArrayView<DescriptorSet> descriptorSets);
		void DestroyDescriptorSet(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, DescriptorSet&& descriptorSet);
		void DestroyDescriptorSets(
			const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, ArrayView<DescriptorSet> descriptorSets
		);

		void DestroyDescriptorSetLayout(const LogicalDeviceIdentifier logicalDeviceIdentifier, DescriptorSetLayout&& descriptorSetLayout);
		void
		DestroyDescriptorSetLayouts(const LogicalDeviceIdentifier logicalDeviceIdentifier, ArrayView<DescriptorSetLayout> descriptorSetLayouts);
		void DestroyDescriptorSetLayout(
			const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, DescriptorSetLayout&& descriptorSetLayout
		);
		void DestroyDescriptorSetLayouts(
			const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, ArrayView<DescriptorSetLayout> descriptorSetLayouts
		);

		void DestroyImageMapping(const LogicalDeviceIdentifier logicalDeviceIdentifier, ImageMapping&& imageMapping);
		void DestroyImageMappings(const LogicalDeviceIdentifier logicalDeviceIdentifier, ArrayView<ImageMapping> imageMappings);
		void DestroyImageMapping(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, ImageMapping&& imageMapping);
		void DestroyImageMappings(
			const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, ArrayView<ImageMapping> imageMappings
		);

		void DestroyImage(const LogicalDeviceIdentifier logicalDeviceIdentifier, Image&& image);
		void DestroyImages(const LogicalDeviceIdentifier logicalDeviceIdentifier, ArrayView<Image> images);
		void DestroyImage(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, Image&& image);
		void DestroyImages(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, ArrayView<Image> images);

		void DestroyBuffer(const LogicalDeviceIdentifier logicalDeviceIdentifier, Buffer&& buffer);
		void DestroyBuffers(const LogicalDeviceIdentifier logicalDeviceIdentifier, ArrayView<Buffer> buffers);
		void DestroyBuffer(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, Buffer&& buffer);
		void DestroyBuffers(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, ArrayView<Buffer> buffers);

		void DestroyCommandBuffer(
			const LogicalDeviceIdentifier logicalDeviceIdentifier, const QueueFamily queueFamily, CommandBuffer&& commandBuffer
		);
		void DestroyCommandBuffers(
			const LogicalDeviceIdentifier logicalDeviceIdentifier, const QueueFamily queueFamily, ArrayView<CommandBuffer> commandBuffers
		);
		void DestroyCommandBuffer(
			const LogicalDeviceIdentifier logicalDeviceIdentifier,
			const uint8 frameIndex,
			const QueueFamily queueFamily,
			CommandBuffer&& commandBuffer
		);
		void DestroyCommandBuffers(
			const LogicalDeviceIdentifier logicalDeviceIdentifier,
			const uint8 frameIndex,
			const QueueFamily queueFamily,
			ArrayView<CommandBuffer> commandBuffer
		);

		void DestroySemaphore(const LogicalDeviceIdentifier logicalDeviceIdentifier, Semaphore&& semaphore);
		void DestroySemaphore(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, Semaphore&& semaphore);

		void DestroyFence(const LogicalDeviceIdentifier logicalDeviceIdentifier, Fence&& fence);
		void DestroyFence(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, Fence&& fence);

		void DestroyAccelerationStructure(
			const LogicalDeviceIdentifier logicalDeviceIdentifier, PrimitiveAccelerationStructure&& accelerationStructure
		);
		void DestroyAccelerationStructure(
			const LogicalDeviceIdentifier logicalDeviceIdentifier, InstanceAccelerationStructure&& accelerationStructure
		);
		void DestroyAccelerationStructure(
			const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, PrimitiveAccelerationStructure&& accelerationStructure
		);
		void DestroyAccelerationStructure(
			const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, InstanceAccelerationStructure&& accelerationStructure
		);

		enum class FrameStateFlags : uint8
		{
			//! Set when the first work starts on CPU, and cleared when the last finishes
			AwaitingCpuFinish = 1 << 0,
			//! Set when the first work starts on GPU, and cleared when the last finishes
			AwaitingGpuFinish = 1 << 1,
			//! Set when the first work starts on CPU, and cleared when both CPU and GPU are finished
			AwaitingFrameFinish = 1 << 2,
		};

		struct LogicalDeviceData;
		struct PerFrameQueue
		{
			enum class Type : uint8
			{
				First,
				DescriptorSet = First,
				DescriptorSetLayout,
				ImageMapping,
				Image,
				Buffer,
				CommandBuffer,
				Semaphore,
				Fence,
				AccelerationStructure,
				Last = AccelerationStructure,
				End,
				Count = End
			};

			void Reset(LogicalDeviceData& logicalDeviceData, LogicalDevice& logicalDevice, const Type type);
			[[nodiscard]] bool HasElements(const Type type) const;

			Vector<DescriptorSet> m_queuedDescriptorReleases;
			Vector<DescriptorSetLayout> m_queuedDescriptorLayoutReleases;
			Vector<ImageMapping> m_queuedImageMappingReleases;
			Vector<Image> m_queuedImageReleases;
			Vector<Buffer> m_queuedBufferReleases;
			Array<Vector<CommandBuffer>, (uint8)QueueFamily::Count> m_queuedCommandBufferReleases;
			Vector<Semaphore> m_queuedSemaphoreReleases;
			Vector<Fence> m_queuedFenceReleases;
			Vector<PrimitiveAccelerationStructure> m_queuedPrimitiveAccelerationStructureReleases;
			Vector<InstanceAccelerationStructure> m_queuedInstanceAccelerationStructureReleases;
		};
	protected:
		void QueueFinishFrameWorkInternal(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex);
		void OnFinishFrameWorkInternal(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex);
		void ResetFrameResources(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex);
	protected:
		struct PerFramePoolData
		{
			CommandPool m_commandPool;
#define RENDERER_SUPPORTS_REUSABLE_COMMAND_BUFFERS RENDERER_VULKAN
#if RENDERER_SUPPORTS_REUSABLE_COMMAND_BUFFERS
			Vector<CommandBuffer> m_availableCommandBuffers;
			Vector<CommandBuffer> m_usedCommandBuffers;
#endif
		};

		struct PerFrameData
		{
			Array<PerFramePoolData, (uint8)QueueFamily::Count, QueueFamily> m_commandPoolStorage;
			Array<PerFramePoolData*, (uint8)QueueFamily::Count, QueueFamily> m_commandPools = {};

			struct State
			{
				State()
					: m_value(0)
				{
				}

				union
				{
					struct
					{
						uint16 m_ongoingGpuWorkCounter;
						uint16 m_ongoingCpuWorkCounter;
						EnumFlags<FrameStateFlags> m_stateFlags;
					} m_data;
					uint64 m_value;
				};
			};

			struct AtomicState
			{
				AtomicState()
					: m_value(0)
				{
				}

				void StartCpuWork();
				[[nodiscard]] bool FinishCpuWork();
				void StartGpuWork();
				[[nodiscard]] bool FinishGpuWork();

				[[nodiscard]] bool IsProcessingFrame() const;
				[[nodiscard]] bool IsProcessingFrameOrAwaitingReset() const;
				void FinishFrame();

				union
				{
					struct
					{
						Threading::Atomic<uint16> m_ongoingGpuWorkCounter;
						Threading::Atomic<uint16> m_ongoingCpuWorkCounter;
						AtomicEnumFlags<FrameStateFlags> m_stateFlags;
					} m_data;
					Threading::Atomic<uint64> m_value;
				};
			};
			static_assert(sizeof(AtomicState) == sizeof(uint64));
			AtomicState m_state;

			Array<Threading::SharedMutex, (uint8)PerFrameQueue::Type::Count> m_queueMutexes;
			PerFrameQueue m_queue;
		};
	public:
		struct LogicalDeviceData
		{
			Array<PerFrameData, Rendering::MaximumConcurrentFrameCount> m_perFrameData;

			Array<CommandPool, (uint8)QueueFamily::Count> m_commandPools;
			Array<CommandPoolView, (uint8)QueueFamily::Count> m_commandPoolViews;

			DescriptorPool m_descriptorPool;
		};
	protected:
		TIdentifierArray<UniquePtr<LogicalDeviceData>, LogicalDeviceIdentifier> m_logicalDeviceData;
	};

	ENUM_FLAG_OPERATORS(JobRunnerData::FrameStateFlags);
	ENUM_FLAG_OPERATORS(JobRunnerData::PerFrameQueue::Type);
}
