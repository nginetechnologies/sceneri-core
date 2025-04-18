#pragma once

#include <Renderer/Constants.h>

#if ENABLE_GPU_CHECKPOINTS
#include <Renderer/PipelineStageFlags.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Platform/DllExport.h>

namespace ngine
{
	struct Log;
}

namespace ngine::Rendering
{
	struct CommandQueueView;
	struct CommandEncoderView;
	struct LogicalDevice;

	struct GpuCheckpointMarker
	{
		GpuCheckpointMarker() = default;
		GpuCheckpointMarker(const ConstStringView name)
			: m_name(name)
		{
		}

		void Set(const LogicalDevice& logicalDevice, const CommandEncoderView commandEncoder) const;

		ConstStringView m_name;
	};

	struct GpuCheckpoint
	{
	private:
		uint32 type;
		void* pNext;
	public:
		PipelineStageFlags stage;
		GpuCheckpointMarker* pMarker;

		RENDERER_EXPORT_API static void
		Get(const LogicalDevice& logicalDevice, const CommandQueueView queue, ArrayView<GpuCheckpoint, uint32>& checkpointsOut);
		RENDERER_EXPORT_API static void
		Log(const LogicalDevice& logicalDevice, const CommandQueueView queue, ngine::Log& log, const uint32 maximumCount = 10);
		RENDERER_EXPORT_API static void LogAllQueues(const LogicalDevice& logicalDevice, ngine::Log& log, const uint32 maximumCount = 10);
	};
}
#endif
