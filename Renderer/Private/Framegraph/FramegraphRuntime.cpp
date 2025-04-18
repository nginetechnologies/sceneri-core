#include "Framegraph/Framegraph.h"
#include "GenericPassStage.h"
#include "RenderPassStage.h"
#include "ComputePassStage.h"

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Stages/Stage.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Stages/PresentStage.h>
#include <Renderer/Assets/Texture/RenderTexture.h>

#include <Engine/Engine.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Threading/JobRunnerThread.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Rendering
{
	Framegraph::Framegraph(LogicalDevice& logicalDevice, RenderOutput& renderOutput)
		: m_logicalDevice(logicalDevice)
		, m_renderOutput(renderOutput)
		, m_startStage(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					OnStartFrame();
					return Threading::CallbackResult::Finished;
				},
				Threading::JobPriority::Submit,
				"Framegraph Start Stage"
			))
		, m_endStage(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					// TODO: Multiple frames in flight
					const FrameMask processingFramesMask = m_processingFrameCpuMask.Load();
					Assert(Memory::GetNumberOfSetBits(processingFramesMask) == 1);
					const FrameIndex frameIndex = Math::Log2(processingFramesMask);
					OnEndFrame(frameIndex);
					return Threading::CallbackResult::Finished;
				},
				Threading::JobPriority::Submit,
				"Framegraph End Stage"
			))
		, m_finishFrameGpuExecutionStage(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					// TODO: Multiple frames in flight
					const FrameMask processingFramesMask = m_processingFrameGpuMask.Load();
					Assert(Memory::GetNumberOfSetBits(processingFramesMask) == 1);
					const FrameIndex frameIndex = Math::Log2(processingFramesMask);
					OnFinishFrameGpuExecution(frameIndex);
					return Threading::CallbackResult::Finished;
				},
				Threading::JobPriority::Submit,
				"Framegraph Finish Frame GPU Execution"
			))
		, m_pAcquireRenderOutputImageStage(Memory::ConstructInPlace, logicalDevice, renderOutput)
		, m_pPresentRenderOutputImageStage(Memory::ConstructInPlace, logicalDevice, renderOutput, *m_pAcquireRenderOutputImageStage)
	{
		m_startStage.AddSubsequentStage(m_finishFrameGpuExecutionStage);
		m_startStage.AddSubsequentStage(m_endStage);
	}

	Framegraph::~Framegraph()
	{
		Reset();
	}

	void Framegraph::Reset()
	{
		Assert(!IsProcessingFrames(AllFramesMask));
		if (Optional<Threading::JobRunnerThread*> pCurrentThread = Threading::JobRunnerThread::GetCurrent())
		{
			while (m_pendingCompilationTasks.Load() != 0)
			{
				pCurrentThread->DoRunNextJob();
			}
		}
		else
		{
			while (m_pendingCompilationTasks.Load() != 0)
				;
		}

		LogicalDevice& logicalDevice = m_logicalDevice;
		for (PassInfo& __restrict passInfo : m_passes)
		{
			switch (passInfo.m_type)
			{
				case StageType::RenderPass:
				case StageType::ExplicitRenderPass:
				{
					RenderPassInfo& __restrict renderPassInfo = *passInfo.GetRenderPassInfo();
					renderPassInfo.m_pPass->OnBeforeRenderPassDestroyed();

					for (const ArrayView<ImageMapping, AttachmentIndex> attachmentMappings : renderPassInfo.m_imageMappings)
					{
						for (ImageMapping& attachmentMapping : attachmentMappings)
						{
							attachmentMapping.Destroy(logicalDevice);
						}
					}

					for (RenderSubpassInfo& subpassInfo : renderPassInfo.m_subpasses)
					{
						for (const ArrayView<ImageMapping, AttachmentIndex> attachmentMappings : subpassInfo.m_imageMappings)
						{
							for (ImageMapping& attachmentMapping : attachmentMappings)
							{
								attachmentMapping.Destroy(logicalDevice);
							}
						}

						for (Stage& subpassStage : subpassInfo.m_stages)
						{
							subpassStage.OnBeforeRenderPassDestroyed();
						}
					}
				}
				break;
				case StageType::Generic:
				{
					GenericPassInfo& __restrict genericPassInfo = *passInfo.GetGenericPassInfo();

					genericPassInfo.m_pPass->OnBeforeRenderPassDestroyed();
					genericPassInfo.m_stage.OnBeforeRenderPassDestroyed();

					for (GenericSubpassInfo& genericSubpassInfo : genericPassInfo.m_subpasses)
					{
						for (const ArrayView<ImageMapping, AttachmentIndex> attachmentMappings : genericSubpassInfo.m_imageMappings)
						{
							for (ImageMapping& attachmentMapping : attachmentMappings)
							{
								attachmentMapping.Destroy(logicalDevice);
							}
						}
					}
				}
				break;
				case StageType::Compute:
				{
					ComputePassInfo& __restrict computePassInfo = *passInfo.GetComputePassInfo();

					computePassInfo.m_pPass->OnBeforeRenderPassDestroyed();
					computePassInfo.m_stage.OnBeforeRenderPassDestroyed();

					for (ComputeSubpassInfo& subpassInfo : computePassInfo.m_subpasses)
					{
						for (const ArrayView<ImageMapping, AttachmentIndex> attachmentMappings : subpassInfo.m_imageMappings)
						{
							for (ImageMapping& attachmentMapping : attachmentMappings)
							{
								attachmentMapping.Destroy(logicalDevice);
							}
						}
					}
				}
				break;
			}
		}

		TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
		for (const TextureIdentifier::IndexType textureIdentifierIndex : m_requestedRenderTargets.GetSetBitsIterator())
		{
			const TextureIdentifier textureIdentifier = TextureIdentifier::MakeFromValidIndex(textureIdentifierIndex);
			textureCache.RemoveRenderTextureListener(logicalDevice.GetIdentifier(), textureIdentifier, this);
		}

		m_passes.Clear();
		m_stagePassIndices.Clear();
		m_requestedRenderTargets.ClearAll();
		m_firstRenderOutputPassIndex = InvalidPassIndex;
		m_lastRenderOutputPassIndex = InvalidPassIndex;
		m_renderOutputDependencies.Clear();
	}

	void Framegraph::Enable()
	{
		Assert(!IsProcessingFrames(Rendering::AllFramesMask));

		if (m_firstRenderOutputPassIndex != InvalidPassIndex)
		{
			m_startStage.AddSubsequentStage(*m_pAcquireRenderOutputImageStage);
			m_pAcquireRenderOutputImageStage->AddSubsequentGpuStage(*m_passes[m_firstRenderOutputPassIndex].GetStage());
		}

		for (const PassInfo& __restrict passInfo : m_passes)
		{
			m_startStage.AddSubsequentStage(*passInfo.GetStage());

			for (const PassIndex passDependencyIndex : passInfo.m_cpuDependencies)
			{
				const PassInfo& __restrict passDependency = m_passes[passDependencyIndex];
				if (!passDependency.GetStage()->IsDirectlyFollowedBy(*passInfo.GetStage()))
				{
					passDependency.GetStage()->AddSubsequentCpuStage(*passInfo.GetStage());
				}
			}
			for (const PassIndex passDependencyIndex : passInfo.m_gpuDependencies)
			{
				const PassInfo& __restrict passDependency = m_passes[passDependencyIndex];
				passDependency.GetStage()->AddSubsequentGpuStage(*passInfo.GetStage());

				if (!passDependency.GetStage()->IsDirectlyFollowedBy(*passInfo.GetStage()))
				{
					passDependency.GetStage()->AddSubsequentCpuStage(*passInfo.GetStage());
				}
			}

			if (const Optional<const RenderPassInfo*> pRenderPassInfo = passInfo.GetRenderPassInfo())
			{
				for (const RenderSubpassInfo& __restrict subpassInfo : pRenderPassInfo->m_subpasses)
				{
					for (Stage& subpassStage : subpassInfo.m_stages)
					{
						if (!passInfo.GetStage()->IsDirectlyFollowedBy(subpassStage))
						{
							passInfo.GetStage()->AddSubsequentCpuStage(subpassStage);
						}
						if (!passInfo.GetStage()->GetSubmitJob().IsDirectlyFollowedBy(subpassStage.GetSubmitJob()))
						{
							passInfo.GetStage()->GetSubmitJob().AddSubsequentStage(subpassStage.GetSubmitJob());
						}
						if (!passInfo.GetStage()->GetFinishedExecutionStage().IsDirectlyFollowedBy(subpassStage.GetFinishedExecutionStage()))
						{
							passInfo.GetStage()->GetFinishedExecutionStage().AddSubsequentStage(subpassStage.GetFinishedExecutionStage());
						}
						if (!subpassStage.GetFinishedExecutionStage().IsDirectlyFollowedBy(m_finishFrameGpuExecutionStage))
						{
							subpassStage.GetFinishedExecutionStage().AddSubsequentStage(m_finishFrameGpuExecutionStage);
						}
						if (!subpassStage.GetSubmitJob().IsDirectlyFollowedBy(m_endStage))
						{
							subpassStage.GetSubmitJob().AddSubsequentStage(m_endStage);
						}
					}
				}
			}
			else if (const Optional<const GenericPassInfo*> pGenericPassInfo = passInfo.GetGenericPassInfo())
			{
				if (!passInfo.GetStage()->IsDirectlyFollowedBy(pGenericPassInfo->m_stage))
				{
					passInfo.GetStage()->AddSubsequentCpuStage(pGenericPassInfo->m_stage);
				}
				if (!passInfo.GetStage()->GetSubmitJob().IsDirectlyFollowedBy(pGenericPassInfo->m_stage.GetSubmitJob()))
				{
					passInfo.GetStage()->GetSubmitJob().AddSubsequentStage(pGenericPassInfo->m_stage.GetSubmitJob());
				}
				if (!passInfo.GetStage()->GetFinishedExecutionStage().IsDirectlyFollowedBy(pGenericPassInfo->m_stage.GetFinishedExecutionStage()))
				{
					passInfo.GetStage()->GetFinishedExecutionStage().AddSubsequentStage(pGenericPassInfo->m_stage.GetFinishedExecutionStage());
				}
				if (!pGenericPassInfo->m_stage.GetFinishedExecutionStage().IsDirectlyFollowedBy(m_finishFrameGpuExecutionStage))
				{
					pGenericPassInfo->m_stage.GetFinishedExecutionStage().AddSubsequentStage(m_finishFrameGpuExecutionStage);
				}
				if (!pGenericPassInfo->m_stage.GetSubmitJob().IsDirectlyFollowedBy(m_endStage))
				{
					pGenericPassInfo->m_stage.GetSubmitJob().AddSubsequentStage(m_endStage);
				}
			}

			if (!passInfo.GetStage()->GetFinishedExecutionStage().IsDirectlyFollowedBy(m_finishFrameGpuExecutionStage))
			{
				passInfo.GetStage()->GetFinishedExecutionStage().AddSubsequentStage(m_finishFrameGpuExecutionStage);
			}
			if (!passInfo.GetStage()->GetSubmitJob().IsDirectlyFollowedBy(m_endStage))
			{
				passInfo.GetStage()->GetSubmitJob().AddSubsequentStage(m_endStage);
			}
		}

		for (const PassIndex renderOutputDependencyIndex : m_renderOutputDependencies)
		{
			const PassInfo& __restrict renderOutputDependency = m_passes[renderOutputDependencyIndex];
			m_pAcquireRenderOutputImageStage->AddSubsequentCpuStage(*renderOutputDependency.GetStage());
		}

		if (m_lastRenderOutputPassIndex != InvalidPassIndex)
		{
			m_passes[m_lastRenderOutputPassIndex].GetStage()->AddSubsequentGpuStage(*m_pPresentRenderOutputImageStage);
			m_pPresentRenderOutputImageStage->AddSubsequentStage(m_finishFrameGpuExecutionStage);
		}

		m_isEnabled = true;
	}

	void Framegraph::Disable()
	{
		Assert(!IsProcessingFrames(Rendering::AllFramesMask));
		m_isEnabled = false;

		if (m_firstRenderOutputPassIndex != InvalidPassIndex)
		{
			m_startStage.RemoveSubsequentStage(*m_pAcquireRenderOutputImageStage, Invalid, Threading::Job::RemovalFlags{});
			m_pAcquireRenderOutputImageStage->RemoveSubsequentGpuStage(*m_passes[m_firstRenderOutputPassIndex].GetStage());
		}

		for (const PassInfo& __restrict passInfo : m_passes)
		{
			m_startStage.RemoveSubsequentStage(*passInfo.GetStage(), Invalid, Threading::Job::RemovalFlags{});

			for (const PassIndex passDependencyIndex : passInfo.m_cpuDependencies)
			{
				const PassInfo& __restrict passDependency = m_passes[passDependencyIndex];
				if (passDependency.GetStage()->IsDirectlyFollowedBy(*passInfo.GetStage()))
				{
					passDependency.GetStage()->RemoveSubsequentCpuStage(*passInfo.GetStage(), Invalid, Threading::Job::RemovalFlags{});
				}
			}
			for (const PassIndex passDependencyIndex : passInfo.m_gpuDependencies)
			{
				const PassInfo& __restrict passDependency = m_passes[passDependencyIndex];
				passDependency.GetStage()->RemoveSubsequentGpuStage(*passInfo.GetStage());

				if (passDependency.GetStage()->IsDirectlyFollowedBy(*passInfo.GetStage()))
				{
					passDependency.GetStage()->RemoveSubsequentCpuStage(*passInfo.GetStage(), Invalid, Threading::Job::RemovalFlags{});
				}
			}

			if (const Optional<const RenderPassInfo*> pRenderPassInfo = passInfo.GetRenderPassInfo())
			{
				for (const RenderSubpassInfo& __restrict subpassInfo : pRenderPassInfo->m_subpasses)
				{
					for (Stage& subpassStage : subpassInfo.m_stages)
					{
						if (passInfo.GetStage()->IsDirectlyFollowedBy(subpassStage))
						{
							passInfo.GetStage()->RemoveSubsequentCpuStage(subpassStage, Invalid, Threading::Job::RemovalFlags{});
						}
						if (passInfo.GetStage()->GetSubmitJob().IsDirectlyFollowedBy(subpassStage.GetSubmitJob()))
						{
							passInfo.GetStage()
								->GetSubmitJob()
								.RemoveSubsequentStage(subpassStage.GetSubmitJob(), Invalid, Threading::Job::RemovalFlags{});
						}
						if (passInfo.GetStage()->GetFinishedExecutionStage().IsDirectlyFollowedBy(subpassStage.GetFinishedExecutionStage()))
						{
							passInfo.GetStage()
								->GetFinishedExecutionStage()
								.RemoveSubsequentStage(subpassStage.GetFinishedExecutionStage(), Invalid, Threading::Job::RemovalFlags{});
						}
						if (subpassStage.GetFinishedExecutionStage().IsDirectlyFollowedBy(m_finishFrameGpuExecutionStage))
						{
							subpassStage.GetFinishedExecutionStage()
								.RemoveSubsequentStage(m_finishFrameGpuExecutionStage, Invalid, Threading::Job::RemovalFlags{});
						}
						if (subpassStage.GetSubmitJob().IsDirectlyFollowedBy(m_endStage))
						{
							subpassStage.GetSubmitJob().RemoveSubsequentStage(m_endStage, Invalid, Threading::Job::RemovalFlags{});
						}
					}
				}
			}
			else if (const Optional<const GenericPassInfo*> pGenericPassInfo = passInfo.GetGenericPassInfo())
			{
				if (passInfo.GetStage()->IsDirectlyFollowedBy(pGenericPassInfo->m_stage))
				{
					passInfo.GetStage()->RemoveSubsequentCpuStage(pGenericPassInfo->m_stage, Invalid, Threading::Job::RemovalFlags{});
				}
				if (passInfo.GetStage()->GetSubmitJob().IsDirectlyFollowedBy(pGenericPassInfo->m_stage.GetSubmitJob()))
				{
					passInfo.GetStage()
						->GetSubmitJob()
						.RemoveSubsequentStage(pGenericPassInfo->m_stage.GetSubmitJob(), Invalid, Threading::Job::RemovalFlags{});
				}
				if (passInfo.GetStage()->GetFinishedExecutionStage().IsDirectlyFollowedBy(pGenericPassInfo->m_stage.GetFinishedExecutionStage()))
				{
					passInfo.GetStage()
						->GetFinishedExecutionStage()
						.RemoveSubsequentStage(pGenericPassInfo->m_stage.GetFinishedExecutionStage(), Invalid, Threading::Job::RemovalFlags{});
				}
				if (pGenericPassInfo->m_stage.GetFinishedExecutionStage().IsDirectlyFollowedBy(m_finishFrameGpuExecutionStage))
				{
					pGenericPassInfo->m_stage.GetFinishedExecutionStage()
						.RemoveSubsequentStage(m_finishFrameGpuExecutionStage, Invalid, Threading::Job::RemovalFlags{});
				}
				if (pGenericPassInfo->m_stage.GetSubmitJob().IsDirectlyFollowedBy(m_endStage))
				{
					pGenericPassInfo->m_stage.GetSubmitJob().RemoveSubsequentStage(m_endStage, Invalid, Threading::Job::RemovalFlags{});
				}
			}

			if (passInfo.GetStage()->GetFinishedExecutionStage().IsDirectlyFollowedBy(m_finishFrameGpuExecutionStage))
			{
				passInfo.GetStage()
					->GetFinishedExecutionStage()
					.RemoveSubsequentStage(m_finishFrameGpuExecutionStage, Invalid, Threading::Job::RemovalFlags{});
			}
			if (passInfo.GetStage()->GetSubmitJob().IsDirectlyFollowedBy(m_endStage))
			{
				passInfo.GetStage()->GetSubmitJob().RemoveSubsequentStage(m_endStage, Invalid, Threading::Job::RemovalFlags{});
			}
		}

		for (const PassIndex renderOutputDependencyIndex : m_renderOutputDependencies)
		{
			const PassInfo& __restrict renderOutputDependency = m_passes[renderOutputDependencyIndex];
			m_pAcquireRenderOutputImageStage
				->RemoveSubsequentCpuStage(*renderOutputDependency.GetStage(), Invalid, Threading::Job::RemovalFlags{});
		}

		if (m_lastRenderOutputPassIndex != InvalidPassIndex)
		{
			m_passes[m_lastRenderOutputPassIndex].GetStage()->RemoveSubsequentGpuStage(*m_pPresentRenderOutputImageStage);
			m_pPresentRenderOutputImageStage->RemoveSubsequentStage(m_finishFrameGpuExecutionStage, Invalid, Threading::Job::RemovalFlags{});
		}
	}

	Optional<Stage*> Framegraph::GetStagePass(Stage& stage) const
	{
		for (const PassInfo& __restrict passInfo : m_passes)
		{
			if (passInfo.ContainsStage(stage))
			{
				return passInfo.GetStage();
			}
		}
		return Invalid;
	}

	void Framegraph::OnBeforeRenderOutputResize()
	{
		if (Optional<Threading::JobRunnerThread*> pCurrentThread = Threading::JobRunnerThread::GetCurrent())
		{
			while (m_pendingCompilationTasks.Load() != 0)
			{
				pCurrentThread->DoRunNextJob();
			}
		}
		else
		{
			while (m_pendingCompilationTasks.Load() != 0)
				;
		}

		for (PassInfo& __restrict passInfo : m_passes)
		{
			if (const Optional<RenderPassInfo*> pRenderPassInfo = passInfo.GetRenderPassInfo())
			{
				pRenderPassInfo->m_pPass->OnBeforeRenderPassDestroyed();

				for (RenderSubpassInfo& __restrict subpassInfo : pRenderPassInfo->m_subpasses)
				{
					for (Stage& subpassStage : subpassInfo.m_stages)
					{
						subpassStage.OnBeforeRenderPassDestroyed();
					}
				}
			}
			else if (const Optional<GenericPassInfo*> pGenericPassInfo = passInfo.GetGenericPassInfo())
			{
				pGenericPassInfo->m_pPass->OnBeforeRenderPassDestroyed();
				pGenericPassInfo->m_stage.OnBeforeRenderPassDestroyed();
			}
			else if (const Optional<ComputePassInfo*> pComputePassInfo = *passInfo.GetComputePassInfo())
			{
				pComputePassInfo->m_pPass->OnBeforeRenderPassDestroyed();
				pComputePassInfo->m_stage.OnBeforeRenderPassDestroyed();
			}
		}

		TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
		for (const TextureIdentifier::IndexType textureIdentifierIndex : m_requestedRenderTargets.GetSetBitsIterator())
		{
			const TextureIdentifier textureIdentifier = TextureIdentifier::MakeFromValidIndex(textureIdentifierIndex);
			textureCache.DestroyRenderTarget(m_logicalDevice, textureIdentifier);
		}

		const AttachmentIdentifier renderOutputAttachmentIdentifier =
			textureCache.FindRenderTargetTemplateIdentifier(RenderOutputRenderTargetGuid);
		RenderTargetCache& renderTargetCache = m_renderOutput.GetRenderTargetCache();
		const TextureIdentifier renderOutputTextureIdentifier =
			renderTargetCache.FindRenderTargetFromTemplateIdentifier(renderOutputAttachmentIdentifier);
		if (renderOutputTextureIdentifier.IsValid())
		{
			textureCache.DestroyRenderTarget(m_logicalDevice, renderOutputTextureIdentifier);
		}
	}

	void Framegraph::WaitForProcessingFramesToFinish(const FrameMask frameMask)
	{
		if (Optional<Threading::JobRunnerThread*> pCurrentThread = Threading::JobRunnerThread::GetCurrent())
		{
			while ((m_processingFrameCpuMask.Load() & frameMask) != 0 || (m_processingFrameGpuMask.Load() & frameMask) != 0)
			{
				pCurrentThread->DoRunNextJob();
			}
		}
		else
		{
			while ((m_processingFrameCpuMask.Load() & frameMask) != 0 || (m_processingFrameGpuMask.Load() & frameMask) != 0)
				;
		}
	}

	void Framegraph::OnStartFrame()
	{
		Engine& engine = System::Get<Engine>();
		const FrameIndex frameIndex = engine.GetCurrentFrameIndex();

		WaitForProcessingFramesToFinish(Rendering::AllFramesMask);

		const FrameMask frameMask = FrameMask(1u << frameIndex);
		{
			const FrameMask previousFrameMask = m_processingFrameCpuMask.FetchOr(frameMask);
			[[maybe_unused]] const bool canStartFrame = (previousFrameMask & frameMask) == 0;
			Assert(canStartFrame);
			Assert(previousFrameMask == 0, "TODO: Multiple frames in flight");
		}
		{
			const FrameMask previousFrameMask = m_processingFrameGpuMask.FetchOr(frameMask);
			[[maybe_unused]] const bool canStartFrame = (previousFrameMask & frameMask) == 0;
			Assert(canStartFrame);
			Assert(previousFrameMask == 0, "TODO: Multiple frames in flight");
		}

		m_pStartFrameThread = *Threading::EngineJobRunnerThread::GetCurrent();

		while (m_pendingCompilationTasks.Load() > 0)
		{
			m_pStartFrameThread->DoRunNextJob();
		}

		m_pStartFrameThread->GetRenderData().OnStartFrameCpuWork(m_logicalDevice.GetIdentifier(), frameIndex);
		m_pStartFrameThread->GetRenderData().OnStartFrameCpuWork(m_logicalDevice.GetIdentifier(), frameIndex);
		m_pStartFrameThread->GetRenderData().OnStartFrameGpuWork(m_logicalDevice.GetIdentifier(), frameIndex);
	}

	void Framegraph::OnEndFrame(const FrameIndex frameIndex)
	{
		const FrameMask frameMask = FrameMask(1u << frameIndex);
		const FrameMask previousFrameMask = m_processingFrameCpuMask.FetchAnd((FrameMask)~frameMask);
		[[maybe_unused]] const bool finishedFrame = (previousFrameMask & frameMask) == frameMask;
		Assert(finishedFrame);

		m_pStartFrameThread->GetRenderData().OnFinishFrameCpuWork(m_logicalDevice.GetIdentifier(), frameIndex);
	}

	void Framegraph::OnFinishFrameGpuExecution(const FrameIndex frameIndex)
	{
		const FrameMask frameMask = FrameMask(1u << frameIndex);
		const FrameMask previousFrameMask = m_processingFrameGpuMask.FetchAnd((FrameMask)~frameMask);
		[[maybe_unused]] const bool finishedFrame = (previousFrameMask & frameMask) == frameMask;
		Assert(finishedFrame);

		m_pStartFrameThread->GetRenderData().OnFinishFrameCpuWork(m_logicalDevice.GetIdentifier(), frameIndex);
		m_pStartFrameThread->GetRenderData().OnFinishFrameGpuWork(m_logicalDevice.GetIdentifier(), frameIndex);
	}
}
