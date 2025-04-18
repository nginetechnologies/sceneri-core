#pragma once

#include <Renderer/Stages/StageBase.h>
#include <Renderer/Threading/Semaphore.h>
#include <Renderer/FrameImageId.h>
#include <Renderer/Constants.h>

namespace ngine::Rendering
{
	struct Stage;
	struct RenderOutput;
	struct LogicalDevice;

	struct StartFrameStage final : public StageBase
	{
		StartFrameStage(LogicalDevice& logicalDevice, RenderOutput& renderOutput);
		virtual ~StartFrameStage();

		virtual Result OnExecute(Threading::JobRunnerThread& thread) override;

		[[nodiscard]] Rendering::FrameImageId GetFrameImageId() const
		{
			return m_frameImageId;
		}

		[[nodiscard]] virtual SemaphoreView GetSubmissionFinishedSemaphore([[maybe_unused]] const Threading::Job& job) const override final;

		[[nodiscard]] virtual bool IsSubmissionFinishedSemaphoreUsable() const override final;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::TopOfPipe;
		}

		[[nodiscard]] virtual FenceView GetSubmissionFinishedFence() const override final
		{
			return {};
		}
		[[nodiscard]] virtual bool IsSubmissionFinishedFenceUsable() const override final
		{
			return false;
		}

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Start Frame Stage";
		}
#endif
	protected:
		LogicalDevice& m_logicalDevice;
		RenderOutput& m_renderOutput;
		Rendering::FrameImageId m_frameImageId;
		Array<Semaphore, Rendering::MaximumConcurrentFrameCount> m_imageAcquiringSemaphores;
	};
}
