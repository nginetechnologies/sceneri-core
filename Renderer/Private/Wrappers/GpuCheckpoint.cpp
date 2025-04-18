#include <Renderer/Constants.h>
#if ENABLE_GPU_CHECKPOINTS
#include <Renderer/Wrappers/GpuCheckpoint.h>

#include <Common/Platform/Unused.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/IO/Log.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Commands/CommandEncoderView.h>

#include <Renderer/Vulkan/Includes.h>

namespace ngine::Rendering
{
	void GpuCheckpointMarker::Set(
		[[maybe_unused]] const LogicalDevice& logicalDevice, [[maybe_unused]] const CommandEncoderView commandEncoder
	) const
	{
#if ENABLE_NVIDIA_GPU_CHECKPOINTS
		reinterpret_cast<PFN_vkCmdSetCheckpointNV>(logicalDevice.GetSetCheckpointNV())(commandEncoder, this);
#else
		ExpectUnreachable();
#endif
	}

	/* static */ void GpuCheckpoint::Get(
		[[maybe_unused]] const LogicalDevice& logicalDevice,
		[[maybe_unused]] const CommandQueueView queue,
		[[maybe_unused]] ArrayView<GpuCheckpoint, uint32>& checkpointsOut
	)
	{
#if ENABLE_NVIDIA_GPU_CHECKPOINTS
		static_assert(sizeof(GpuCheckpoint) == sizeof(VkCheckpointDataNV));
		static_assert(alignof(GpuCheckpoint) == alignof(VkCheckpointDataNV));

		for (GpuCheckpoint& checkpoint : checkpointsOut)
		{
			checkpoint.type = VK_STRUCTURE_TYPE_CHECKPOINT_DATA_NV;
			checkpoint.pNext = nullptr;
		}

		uint32 checkpointCount = checkpointsOut.GetSize();

		reinterpret_cast<PFN_vkGetQueueCheckpointDataNV>(logicalDevice.GetQueueCheckpointDataNV())(
			queue,
			&checkpointCount,
			reinterpret_cast<VkCheckpointDataNV*>(checkpointsOut.GetData())
		);
		checkpointsOut = ArrayView<GpuCheckpoint, uint32>{checkpointsOut.begin(), checkpointsOut.begin() + checkpointCount};
#else
		ExpectUnreachable();
#endif
	}

	/* static */ void
	GpuCheckpoint::Log(const LogicalDevice& logicalDevice, const CommandQueueView queue, ngine::Log& log, const uint32 maximumCount)
	{
		FixedSizeVector<GpuCheckpoint> checkpoints(Memory::ConstructWithSize, Memory::Uninitialized, maximumCount);
		ArrayView<GpuCheckpoint, uint32> returnedCheckpoints = checkpoints.GetView();
		Get(logicalDevice, queue, returnedCheckpoints);

		log.Message("-- Rendering checkpoints:");

		for (const GpuCheckpoint& checkpoint : returnedCheckpoints)
		{
			if (checkpoint.pMarker != nullptr)
			{
				log.Message("   Checkpoint name {}, stage {}", checkpoint.pMarker->m_name, (uint32)checkpoint.stage);
			}
			else
			{
				log.Message("   Checkpoint name [no marker], stage {}", (uint32)checkpoint.stage);
			}
		}
	}

	/* static */ void GpuCheckpoint::LogAllQueues(const LogicalDevice& logicalDevice, ngine::Log& log, const uint32 maximumCount)
	{
		log.Message("Logging graphics queue checkpoints");

		const CommandQueueView graphicsQueue = logicalDevice.GetCommandQueue(QueueFamily::Graphics);
		GpuCheckpoint::Log(logicalDevice, graphicsQueue, log);
		const CommandQueueView transferQueue = logicalDevice.GetCommandQueue(QueueFamily::Transfer);
		if (graphicsQueue != transferQueue)
		{
			log.Message("Logging transfer queue checkpoints");
			GpuCheckpoint::Log(logicalDevice, transferQueue, log, maximumCount);
		}
		const CommandQueueView computeQueue = logicalDevice.GetCommandQueue(QueueFamily::Compute);
		if (graphicsQueue != computeQueue && transferQueue != computeQueue)
		{
			log.Message("Logging compute queue checkpoints");
			GpuCheckpoint::Log(logicalDevice, computeQueue, log, maximumCount);
		}
		const CommandQueueView presentQueue = logicalDevice.GetPresentCommandQueue();
		if (graphicsQueue != presentQueue && transferQueue != presentQueue && computeQueue != presentQueue)
		{
			log.Message("Logging present queue checkpoints");
			GpuCheckpoint::Log(logicalDevice, presentQueue, log, maximumCount);
		}
	}
}
#endif
