#include "Stages/PresentStage.h"

#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Stages/Stage.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <Renderer/Framegraph/SubresourceStates.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/Commands/UnifiedCommandBuffer.h>

#include <Engine/Threading/JobManager.h>
#include <Engine/Threading/JobRunnerThread.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/System/Query.h>

#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>

namespace ngine::Rendering
{
	PresentStage::PresentStage(LogicalDevice& logicalDevice, RenderOutput& renderOutput, StartFrameStage& startFrameStage)
		: Job(Threading::JobPriority::Present)
		, m_logicalDevice(logicalDevice)
		, m_renderOutput(renderOutput)
		, m_startFrameStage(startFrameStage)
	{
	}

	PresentStage::~PresentStage()
	{
	}

	void PresentStage::AddGpuDependency(Rendering::Stage& stage)
	{
		Assert(!m_parentStages.Contains(stage));
		m_parentStages.EmplaceBack(stage);
		stage.GetSubmitJob().AddSubsequentStage(*this);
	}

	void PresentStage::RemoveGpuDependency(Rendering::Stage& stage)
	{
		[[maybe_unused]] const bool wasRemoved = m_parentStages.RemoveFirstOccurrence(stage);
		Assert(wasRemoved);
		stage.GetSubmitJob().RemoveSubsequentStage(*this, Invalid, Threading::Job::RemovalFlags{});
	}

	bool PresentStage::SupportsSemaphores() const
	{
		return m_renderOutput.SupportsPresentImageSemaphore();
	}

	Threading::Job::Result PresentStage::OnExecute(Threading::JobRunnerThread&)
	{
		return Job::Result::AwaitExternalFinish;
	}

	void PresentStage::OnAwaitExternalFinish(Threading::JobRunnerThread&)
	{
		const FrameImageId frameImageId = m_startFrameStage.GetFrameImageId();

		m_waitSemaphores.Clear();

		if (m_parentStages.HasElements())
		{
			if (m_renderOutput.SupportsPresentImageSemaphore())
			{
				for (const Stage& stage : m_parentStages)
				{
					Stage::IterateUsedStages(
						stage,
						*this,
						[this](Stage::UsedStage usedStage)
						{
							if (usedStage.stage->IsSubmissionFinishedSemaphoreUsable())
							{
								if (const SemaphoreView waitSemaphore = usedStage.stage->GetSubmissionFinishedSemaphore(usedStage.nextStage);
							      waitSemaphore.IsValid())
								{
									Assert(!m_waitSemaphores.Contains(waitSemaphore));
									m_waitSemaphores.EmplaceBack(waitSemaphore);
								}
							}
						}
					);
				}
			}
		}

		FenceView submissionFinishedFence;
		if (m_renderOutput.RequiresPresentFence())
		{
			const Stage& lastStage = m_parentStages.GetLastElement();
			submissionFinishedFence = lastStage.IsSubmissionFinishedFenceUsable() ? lastStage.GetSubmissionFinishedFence() : FenceView();

			if (!submissionFinishedFence.IsValid())
			{
				for (const Stage& stage : m_parentStages)
				{
					Stage::IterateUsedStages(
						stage,
						*this,
						[&submissionFinishedFence](Stage::UsedStage usedStage)
						{
							if (!submissionFinishedFence.IsValid())
							{
								submissionFinishedFence = usedStage.stage->GetSubmissionFinishedFence();
							}
						}
					);
				}
			}
		}

		auto present = [this, frameImageId, submissionFinishedFence]()
		{
			m_renderOutput.PresentAcquiredImage(
				m_logicalDevice,
				frameImageId,
				m_waitSemaphores.GetView(),
				submissionFinishedFence,
				[this, submissionFinishedFence]()
				{
					if (submissionFinishedFence.IsValid())
					{
						submissionFinishedFence.Reset(m_logicalDevice);
					}

					if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
					{
						SignalExecutionFinished(*Threading::JobRunnerThread::GetCurrent());
					}
					else
					{
						Threading::JobManager& jobManager = System::Get<Threading::JobManager>();
						jobManager.QueueCallback(
							[this](Threading::JobRunnerThread& thread)
							{
								SignalExecutionFinished(thread);
							},
							Threading::JobPriority::Present
						);
					}
				}
			);
		};

		// Ensure the output is in the present layout
		SubresourceStatesBase& renderOutputSubresourceStates = m_renderOutput.GetCurrentColorSubresourceStates();
		const ImageSubresourceRange renderOutputSubresourceRange{ImageAspectFlags::Color, MipRange{0, 1}, ArrayRange{0, 1}};
		const SubresourceState renderOutputSubresourceState =
			*renderOutputSubresourceStates.GetUniformSubresourceState(renderOutputSubresourceRange, renderOutputSubresourceRange, 0);
		const ImageLayout presentImageLayout = m_renderOutput.GetPresentColorImageLayout();
		if (renderOutputSubresourceState.m_imageLayout != presentImageLayout)
		{
			Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
			UnifiedCommandBuffer commandBuffer(
				m_logicalDevice,
				thread.GetRenderData().GetCommandPool(m_logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics),
				m_logicalDevice.GetCommandQueue(Rendering::QueueFamily::Graphics)
			);
			CommandEncoderView commandEncoder = commandBuffer.BeginEncoding(m_logicalDevice);

			{
				BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

				barrierCommandEncoder.TransitionImageLayout(
					GetSupportedPipelineStageFlags(presentImageLayout),
					GetSupportedAccessFlags(presentImageLayout),
					presentImageLayout,
					m_renderOutput.GetCurrentColorImageView(),
					renderOutputSubresourceStates,
					renderOutputSubresourceRange,
					renderOutputSubresourceRange
				);
			}

			const EncodedCommandBufferView encodedCommandBuffer = commandBuffer.StopEncoding();

			QueueSubmissionParameters parameters;
			parameters.m_finishedCallback =
				[present = Move(present), &logicalDevice = m_logicalDevice, &thread, commandBuffer = Move(commandBuffer)]() mutable
			{
				present();

				thread.QueueExclusiveCallbackFromAnyThread(
					Threading::JobPriority::DeallocateResourcesMin,
					[commandBuffer = Move(commandBuffer), &logicalDevice](Threading::JobRunnerThread& thread) mutable
					{
						Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
						commandBuffer.Destroy(
							logicalDevice,
							engineThread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics)
						);
					}
				);
			};

			m_logicalDevice.GetQueueSubmissionJob(QueueFamily::Graphics)
				.Queue(
					Threading::JobPriority::CreateRenderTargetSubmission,
					ArrayView<const EncodedCommandBufferView, uint16>(encodedCommandBuffer),
					Move(parameters)
				);
		}
		else
		{
			// Image was already in the expected layout, proceed immediately
			present();
		}
	}
}
