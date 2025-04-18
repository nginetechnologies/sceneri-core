#include "Stages/Stage.h"
#include "Stages/PresentStage.h"

#include <Renderer/Scene/SceneView.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <Renderer/Commands/CommandEncoder.h>
#include <Renderer/Commands/EncodedCommandBuffer.h>
#include <Renderer/Renderer.h>

#include <Engine/Engine.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Memory/AddressOf.h>

#include <Common/System/Query.h>
#include <Common/Memory/Containers/Format/StringView.h>

namespace ngine::Rendering
{
	Stage::Stage(LogicalDevice& logicalDevice, const Threading::JobPriority priority, const EnumFlags<Flags> flags)
		: StageBase(priority)
		, m_logicalDevice(logicalDevice)
		, m_flags(flags)
		, m_submitJob(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					if (!EvaluateShouldSkip() && m_flags.IsNotSet(Flags::ManagedByPass))
					{
						SubmitEncodedCommandBuffer();
						return Threading::Job::Result::AwaitExternalFinish;
					}
					else
					{
						return Threading::Job::Result::Finished;
					}
				},
				Threading::JobPriority::Submit,
				"Submitted Stage to GPU"
			))
		, m_finishedExecutionStage(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					OnCommandsExecuted();
					return Threading::Job::Result::Finished;
				},
				Threading::JobPriority::Submit,
				"Finished Stage Execution"
			))
	{
		AddSubsequentCpuStage(m_submitJob);
	}

	Stage::~Stage()
	{
		for (Semaphore& semaphore : m_drawingCompleteSemaphores)
		{
			semaphore.Destroy(m_logicalDevice);
		}
		m_drawingCompletePresentSemaphore.Destroy(m_logicalDevice);

		if (m_drawingCompletePresentFence.IsValid())
		{
			m_drawingCompletePresentFence.Destroy(m_logicalDevice);
		}

		RemoveSubsequentCpuStage(m_submitJob, Invalid, Threading::Job::RemovalFlags{});

		delete &m_submitJob;
		delete &m_finishedExecutionStage;
	}

	void Stage::AddSubsequentGpuStage(PresentStage& presentStage)
	{
#if STAGE_DEPENDENCY_PROFILING
		m_submitJob.SetDebugName(Move(String().Format("{} submit stage", GetDebugName())));
		m_finishedExecutionStage.SetDebugName(Move(String().Format("{} finished execution stage", GetDebugName())));
#endif

		Assert(m_pSubsequentGpuPresentStage.IsInvalid());

		Threading::UniqueLock lock(m_drawingCompleteSemaphoresMutex);
		if (presentStage.SupportsSemaphores())
		{
			Assert(GetPipelineStageFlags().AreAnySet());
			m_drawingCompletePresentSemaphore = Semaphore(m_logicalDevice);
#if RENDERER_OBJECT_DEBUG_NAMES
			String debugName;
			debugName.Format("{} -> {}", GetDebugName(), presentStage.GetDebugName());
			m_drawingCompletePresentSemaphore.SetDebugName(m_logicalDevice, Move(debugName));
#endif
		}
		else
		{
			m_drawingCompletePresentFence = Fence(m_logicalDevice, FenceView::Status::Unsignaled);
		}

		m_pSubsequentGpuPresentStage = &presentStage;
		presentStage.AddGpuDependency(*this);
	}

	void Stage::RemoveSubsequentGpuStage(PresentStage& presentStage)
	{
		Threading::UniqueLock lock(m_drawingCompleteSemaphoresMutex);
		Assert(m_pSubsequentGpuPresentStage == &presentStage);
		if (LIKELY(m_pSubsequentGpuPresentStage == &presentStage))
		{
			Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
			if (presentStage.SupportsSemaphores())
			{
				thread.GetRenderData().DestroySemaphore(m_logicalDevice.GetIdentifier(), Move(m_drawingCompletePresentSemaphore));
			}
			else
			{
				thread.GetRenderData().DestroyFence(m_logicalDevice.GetIdentifier(), Move(m_drawingCompletePresentFence));
			}

			m_pSubsequentGpuPresentStage = Invalid;
			presentStage.RemoveGpuDependency(*this);
		}
	}

	void Stage::AddSubsequentCpuGpuStage(PresentStage& stage)
	{
		Stage::AddSubsequentCpuStage(stage);
		Stage::AddSubsequentGpuStage(stage);
	}

	void Stage::RemoveSubsequentCpuGpuStage(
		PresentStage& stage, Threading::JobRunnerThread& thread, const Threading::StageBase::RemovalFlags flags
	)
	{
		Stage::RemoveSubsequentGpuStage(stage);
		Stage::RemoveSubsequentCpuStage(stage, thread, flags);
	}

	void StageBase::AddSubsequentGpuStage(Rendering::Stage& stage)
	{
		Assert(!stage.m_parentStages.Contains(*this));
		stage.m_parentStages.EmplaceBack(*this);
		OnAddedSubsequentGpuStage(stage);
	}

	void StageBase::RemoveSubsequentGpuStage(Rendering::Stage& stage)
	{
		Assert(stage.m_parentStages.Contains(*this));
		stage.m_parentStages.RemoveFirstOccurrence(*this);
		OnRemovedSubsequentGpuStage(stage);
	}

	void StageBase::AddSubsequentCpuGpuStage(Rendering::Stage& stage)
	{
		AddSubsequentCpuStage(stage);
		AddSubsequentGpuStage(stage);
	}

	void StageBase::RemoveSubsequentCpuGpuStage(
		Rendering::Stage& stage, const Optional<Threading::JobRunnerThread*> pThread, const Threading::StageBase::RemovalFlags flags
	)
	{
		RemoveSubsequentCpuStage(stage, pThread, flags);
		RemoveSubsequentGpuStage(stage);
	}

	void Stage::OnAddedSubsequentGpuStage(Stage& subsequentStage)
	{
		Assert(GetPipelineStageFlags().AreAnySet());
		Threading::UniqueLock lock(m_drawingCompleteSemaphoresMutex);
		[[maybe_unused]] Semaphore& semaphore = m_drawingCompleteSemaphores.EmplaceBack(m_logicalDevice);
#if RENDERER_OBJECT_DEBUG_NAMES
		String debugName;
		debugName.Format("{} -> {}", GetDebugName(), subsequentStage.GetDebugName());
		semaphore.SetDebugName(m_logicalDevice, Move(debugName));
#endif
		m_subsequentGpuStages.EmplaceBack(subsequentStage);

		m_submitJob.AddSubsequentStage(subsequentStage.GetSubmitJob());
	}

	void Stage::OnRemovedSubsequentGpuStage(Stage& subsequentStage)
	{
		Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();

		Threading::UniqueLock lock(m_drawingCompleteSemaphoresMutex);
		decltype(m_subsequentGpuStages)::iterator stageIt = m_subsequentGpuStages.Find(subsequentStage);
		Assert(stageIt != m_subsequentGpuStages.end());
		if (LIKELY(stageIt != m_subsequentGpuStages.end()))
		{
			decltype(m_drawingCompleteSemaphores)::iterator semaphoreIt = m_drawingCompleteSemaphores.begin() +
			                                                              m_subsequentGpuStages.GetIteratorIndex(stageIt);
			thread.GetRenderData().DestroySemaphore(m_logicalDevice.GetIdentifier(), Move(*semaphoreIt));

			m_drawingCompleteSemaphores.Remove(semaphoreIt);
			m_subsequentGpuStages.Remove(stageIt);
		}

		m_submitJob.RemoveSubsequentStage(subsequentStage.GetSubmitJob(), thread, Threading::StageBase::RemovalFlags{});
	}

	bool Stage::EvaluateShouldSkip()
	{
		{
			const uint8 frameIndex = System::Get<Engine>().GetCurrentFrameIndex();
			if (frameIndex != m_evaluatedSkippedFrameIndex)
			{
				m_flags &= ~(Flags::Skipped | Flags::EvaluatedSkipped);
				m_evaluatedSkippedFrameIndex = frameIndex;
			}
		}

		bool shouldSkip{false};

		for (Stage& dependency : m_dependencies)
		{
			if (dependency.EvaluateShouldSkip())
			{
				shouldSkip = true;
				break;
			}
		}

		EnumFlags<Flags> expectedFlags{m_flags.GetFlags()};
		EnumFlags<Flags> newFlags;
		do
		{
			newFlags = expectedFlags;
			if (expectedFlags.IsSet(Flags::EvaluatedSkipped))
			{
				Assert(expectedFlags.IsSet(Flags::Skipped) || !shouldSkip);
				return expectedFlags.IsSet(Flags::Skipped);
			}
			else
			{
				newFlags &= ~Flags::Skipped;
				shouldSkip |= IsDisabled() || !ShouldRecordCommands();
				newFlags |= (Flags::Skipped * shouldSkip) | Flags::EvaluatedSkipped;
			}
		} while (!m_flags.CompareExchangeWeak(expectedFlags, newFlags));

		Assert(newFlags.IsSet(Flags::Skipped) || !shouldSkip);
		return newFlags.IsSet(Flags::Skipped);
	}

	Threading::Job::Result Stage::OnExecute(Threading::JobRunnerThread& thread)
	{
		Threading::EngineJobRunnerThread& engineThreadRunner = static_cast<Threading::EngineJobRunnerThread&>(thread);
		const uint8 frameIndex = System::Get<Engine>().GetCurrentFrameIndex();

		Assert(m_flags.AreNoneSet(Flags::AwaitingSubmission | Flags::AwaitingGPUFinish));

		if (m_flags.IsSet(Flags::ManagedByPass))
		{
			return Result::Finished;
		}

		if (EvaluateShouldSkip())
		{
			if (m_finishedExecutionStage.HasSubsequentTasks())
			{
				m_finishedExecutionStage.SignalExecutionFinished(thread);
			}
			return Result::Finished;
		}

		m_pThreadRunner = &engineThreadRunner;

		engineThreadRunner.GetRenderData().OnStartFrameCpuWork(m_logicalDevice.GetIdentifier(), frameIndex);

		const QueueFamily queueFamily = GetRecordedQueueFamily();

#if QUERY_POOL_TEST
		const auto getPipelineStageFlags = [](const QueueFamily queueFamily)
		{
			switch (queueFamily)
			{
				case QueueFamily::Graphics:
					return PipelineStageFlags::VertexInput | PipelineStageFlags::VertexShader | PipelineStageFlags::GeometryShader |
					       PipelineStageFlags::FragmentShader | PipelineStageFlags::EarlyFragmentTests | PipelineStageFlags::LateFragmentTests |
					       PipelineStageFlags::ColorAttachmentOutput;
				case QueueFamily::Transfer:
					return PipelineStageFlags::Transfer;
				case QueueFamily::Compute:
					return PipelineStageFlags::ComputeShader;
				case QueueFamily::Count:
				case QueueFamily::End:
					ExpectUnreachable();
			}
		};
#endif

		{
			CommandEncoder commandEncoder;
			{
				const CommandBufferView commandBuffer =
					engineThreadRunner.GetRenderData().GetPerFrameCommandBuffer(m_logicalDevice.GetIdentifier(), queueFamily, frameIndex);
				Assert(commandBuffer.IsValid());
				commandEncoder = commandBuffer.BeginEncoding(m_logicalDevice);
			}

			RecordCommandsInternal(commandEncoder);

			{
				m_encodedCommandBuffer = commandEncoder.StopEncoding();
				Assert(m_encodedCommandBuffer.IsValid());
			}
		}

		m_waitSemaphores.Clear();
		m_signalSemaphores.Clear();
		m_waitStagesMasks.Clear();

		m_waitSemaphores.Reserve(m_parentStages.GetSize());
		m_waitStagesMasks.Reserve(m_parentStages.GetSize());

		for (const StageBase& parent : m_parentStages)
		{
			Stage::IterateUsedStages(
				parent,
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
							m_waitStagesMasks.EmplaceBack(usedStage.stage->GetPipelineStageFlags());
						}
					}
				}
			);
		}

		if (GetPipelineStageFlags().AreAnySet())
		{
			m_signalSemaphores.Reserve(m_drawingCompleteSemaphores.GetSize() + m_drawingCompletePresentSemaphore.IsValid());

			for (uint8 index = 0, count = m_drawingCompleteSemaphores.GetSize(); index < count; ++index)
			{
				if (!m_subsequentGpuStages[index]->EvaluateShouldSkip())
				{
					m_signalSemaphores.EmplaceBack(m_drawingCompleteSemaphores[index]);
				}
			}
			if (m_drawingCompletePresentSemaphore.IsValid())
			{
				m_signalSemaphores.EmplaceBack(m_drawingCompletePresentSemaphore);
			}
		}

		if (m_drawingCompletePresentFence.IsValid())
		{
			m_signalFence = m_drawingCompletePresentFence;
		}

		m_flags |= Flags::AwaitingSubmission | Flags::AwaitingGPUFinish;
		return Result::Finished;
	}

	SemaphoreView Stage::GetSubmissionFinishedSemaphore(const Threading::Job& stage) const
	{
		if (!const_cast<Stage&>(*this).EvaluateShouldSkip())
		{
			if (Optional<uint8> childIndex = m_subsequentGpuStages.FindIndex(stage))
			{
				if (static_cast<Stage&>(const_cast<Threading::Job&>(stage)).EvaluateShouldSkip())
				{
					return {};
				}

				return m_drawingCompleteSemaphores[*childIndex];
			}
			else
			{
				return m_drawingCompletePresentSemaphore;
			}
		}
		else
		{
			return {};
		}
	}

	void Stage::SubmitEncodedCommandBuffer()
	{
		Threading::EngineJobRunnerThread& engineThreadRunner = *m_pThreadRunner;
		const uint8 frameIndex = System::Get<Engine>().GetCurrentFrameIndex();

		const QueueFamily queueFamily = GetRecordedQueueFamily();

		Assert(m_flags.AreAllSet(Flags::AwaitingSubmission | Flags::AwaitingGPUFinish));

		QueueSubmissionParameters submissionParameters;
		submissionParameters.m_signalSemaphores = m_signalSemaphores.GetView();
		submissionParameters.m_waitSemaphores = m_waitSemaphores;
		submissionParameters.m_waitStagesMasks = m_waitStagesMasks;
		submissionParameters.m_fence = m_signalFence;

		submissionParameters.m_submittedCallback = [this, frameIndex, &engineThreadRunner]()
		{
			[[maybe_unused]] const bool clearedFlags = m_flags.TryClearFlags(Flags::AwaitingSubmission);
			Assert(clearedFlags);
			m_submitJob.SignalExecutionFinished(*Threading::JobRunnerThread::GetCurrent());
			engineThreadRunner.GetRenderData().OnFinishFrameCpuWork(m_logicalDevice.GetIdentifier(), frameIndex);
		};

		submissionParameters.m_finishedCallback = [this, frameIndex, &engineThreadRunner]() mutable
		{
			if (m_signalFence.IsValid())
			{
				m_signalFence.Reset(m_logicalDevice);
			}

			[[maybe_unused]] const bool clearedFlags = m_flags.TryClearFlags(Flags::AwaitingGPUFinish);
			Assert(clearedFlags);

			m_finishedExecutionStage.Queue(System::Get<Threading::JobManager>());
			engineThreadRunner.GetRenderData().OnPerFrameCommandBufferFinishedExecution(m_logicalDevice.GetIdentifier(), frameIndex);
		};

		m_logicalDevice.GetQueueSubmissionJob(queueFamily)
			.Queue(GetPriority(), ArrayView<const EncodedCommandBufferView, uint16>(m_encodedCommandBuffer), Move(submissionParameters));
	}

	[[nodiscard]] bool Stage::IsSubmissionFinishedSemaphoreUsable() const
	{
		return !const_cast<Stage&>(*this).EvaluateShouldSkip();
	}

	Threading::JobBatch Stage::AssignRenderPass(
		const RenderPassView,
		[[maybe_unused]] const Math::Rectangleui outputArea,
		[[maybe_unused]] const Math::Rectangleui fullRenderArea,
		[[maybe_unused]] const uint8 subpassIndex
	)
	{
		return {};
	}
}
