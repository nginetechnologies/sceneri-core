#include "RenderOutput/RenderTargetOutput.h"

#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <Common/System/Query.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Threading/JobRunnerThread.h>

#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/FrameImageId.h>
#include <Renderer/Threading/SemaphoreView.h>
#include <Renderer/Threading/FenceView.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>

namespace ngine::Rendering
{
	[[nodiscard]] inline Array<Rendering::RenderTexture, Rendering::MaximumConcurrentFrameCount>
	CreateColorTextures(Rendering::LogicalDevice& logicalDevice, const Math::Vector2ui outputResolution, const Rendering::Format format)
	{
		Array<Rendering::RenderTexture, Rendering::MaximumConcurrentFrameCount> textures;
		for (FrameIndex i = 0; i < Rendering::MaximumConcurrentFrameCount; ++i)
		{
			textures[i] = Rendering::RenderTexture(
				Rendering::RenderTexture::RenderTarget,
				logicalDevice,
				outputResolution,
				format,
				Rendering::SampleCount::One,
				Rendering::Image::Flags{},
				Rendering::UsageFlags::TransferSource | Rendering::UsageFlags::ColorAttachment | Rendering::UsageFlags::Storage,
				Rendering::MipMask::FromSizeToLargest(outputResolution),
				1
			);
		}

		return textures;
	}

	[[nodiscard]] inline Array<Rendering::ImageMapping, Rendering::MaximumConcurrentFrameCount> CreateColorTextureViews(
		const Rendering::LogicalDeviceView logicalDevice, const ArrayView<Rendering::RenderTexture, FrameIndex> colorTextures
	)
	{
		Array<Rendering::ImageMapping, Rendering::MaximumConcurrentFrameCount> views;
		for (FrameIndex i = 0; i < Rendering::MaximumConcurrentFrameCount; ++i)
		{
			views[i] = Rendering::ImageMapping(
				logicalDevice,
				colorTextures[i],
				Rendering::ImageMapping::Type::TwoDimensional,
				colorTextures[i].GetFormat(),
				Rendering::ImageAspectFlags::Color,
				colorTextures[i].GetAvailableMipRange(),
				ArrayRange{0, 1}
			);
		}

		return views;
	}

	RenderTargetOutput::RenderTargetOutput(
		LogicalDevice& logicalDevice,
		const Math::Vector2ui resolution,
		const Rendering::Format format,
		const Rendering::ImageLayout presentImageLayout
	)
		: RenderOutput(Flags::RequiresPresentFence * RENDERER_METAL)
		, m_resolution(resolution)
		, m_format(format)
		, m_presentImageLayout(presentImageLayout)
		, m_textures(CreateColorTextures(logicalDevice, resolution, format))
		, m_textureViews(CreateColorTextureViews(logicalDevice, m_textures))
	{
		for (FrameIndex i = 0; i < Rendering::MaximumConcurrentFrameCount; ++i)
		{
			m_images[i] = m_textures[i];
		}

		Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
		const CommandPoolView commandPool =
			thread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics);
		const Rendering::SingleUseCommandBuffer
			commandBuffer(logicalDevice, commandPool, thread, Rendering::QueueFamily::Graphics, Threading::JobPriority::CoreRenderStageResources);
		const CommandEncoderView commandEncoderView = commandBuffer;

		{
			BarrierCommandEncoder barrierCommandEncoder = commandEncoderView.BeginBarrier();

			for (FrameIndex i = 0; i < Rendering::MaximumConcurrentFrameCount; i++)
			{
				barrierCommandEncoder.TransitionImageLayout(
					GetSupportedPipelineStageFlags(presentImageLayout),
					GetSupportedAccessFlags(presentImageLayout),
					presentImageLayout,
					m_textures[i],
					Rendering::ImageSubresourceRange{ImageAspectFlags::Color, MipRange{0, 1}, ArrayRange{0, 1}}
				);
			};
		}
	}

	void RenderTargetOutput::Destroy(LogicalDevice& logicalDevice)
	{
		for (Rendering::ImageMapping& textureView : m_textureViews)
		{
			textureView.Destroy(logicalDevice);
		}

		for (Rendering::RenderTexture& texture : m_textures)
		{
			texture.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
		}
	}

	Optional<FrameImageId> RenderTargetOutput::AcquireNextImage(
		const LogicalDeviceView,
		[[maybe_unused]] const uint64 imageAvailabilityTimeoutNanoseconds,
		[[maybe_unused]] const SemaphoreView imageAvailableSemaphore,
		[[maybe_unused]] const FenceView fence
	)
	{
		Assert(!fence.IsValid(), "Fence is not supported for render target output!");

		const FrameIndex imageIndex = m_currentImageIndex;
		return Rendering::FrameImageId(imageIndex);
	}

	void RenderTargetOutput::PresentAcquiredImage(
		LogicalDevice& logicalDevice,
		const Rendering::FrameImageId,
		[[maybe_unused]] ArrayView<const Rendering::SemaphoreView, uint8> waitSemaphores,
		const Rendering::FenceView submissionFinishedFence,
		Rendering::PresentedCallback&& presentedCallback
	)
	{
		Assert(submissionFinishedFence.IsValid());
		if (submissionFinishedFence.IsValid())
		{
			logicalDevice.GetQueueSubmissionJob(QueueFamily::Graphics)
				.QueueAwaitFence(submissionFinishedFence, Forward<Rendering::PresentedCallback>(presentedCallback));
		}
		else if (waitSemaphores.HasElements())
		{
			Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
			const Rendering::CommandPoolView commandPool =
				thread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics);
			Rendering::UnifiedCommandBuffer commandBuffer(logicalDevice, commandPool, logicalDevice.GetCommandQueue(QueueFamily::Graphics));

			[[maybe_unused]] const CommandEncoderView commandEncoder = commandBuffer.BeginEncoding(logicalDevice);
			const EncodedCommandBufferView encodedCommandBuffer = commandBuffer.StopEncoding();

			Rendering::QueueSubmissionParameters parameters;
			parameters.m_finishedCallback = [presentedCallback = Forward<Rendering::PresentedCallback>(presentedCallback),
			                                 &logicalDevice,
			                                 commandBuffer = Move(commandBuffer),
			                                 &thread]() mutable
			{
				presentedCallback();

				thread.GetRenderData().DestroyCommandBuffer(logicalDevice.GetIdentifier(), QueueFamily::Graphics, Move(commandBuffer));
			};
			parameters.m_waitSemaphores = waitSemaphores;

			logicalDevice.GetQueueSubmissionJob(QueueFamily::Graphics)
				.Queue(Threading::JobPriority::Present, Array<EncodedCommandBufferView, 1>{encodedCommandBuffer}, Move(parameters));
		}
		else
		{
			Assert(false, "Immediate present path not supported for platform");
			presentedCallback();
		}
	}
}
