#pragma once

#include "PassInfo.h"

#include <Renderer/FrameImageId.h>
#include <Renderer/Format.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Constants.h>
#include <Renderer/PipelineStageFlags.h>
#include <Renderer/Assets/Texture/MipMask.h>

#include <Common/Storage/IdentifierMask.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/ForwardDeclarations/FixedIdentifierArrayView.h>
#include <Common/Asset/Guid.h>
#include <Common/Memory/UniqueRef.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Threading/Jobs/StageBase.h>

#if STAGE_DEPENDENCY_PROFILING
#include <Common/Memory/Containers/String.h>
#endif

namespace ngine::Threading
{
	struct Job;
	struct JobBatch;
	struct IntermediateStage;
	struct EngineJobRunnerThread;
}

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct StartFrameStage;
	struct PresentStage;
	struct Pass;
	struct RenderTargetCache;
	struct SceneViewDrawer;

	struct Framegraph
	{
		Framegraph(LogicalDevice& logicalDevice, RenderOutput& renderOutput);
		Framegraph(const Framegraph&) = delete;
		Framegraph& operator=(const Framegraph&) = delete;
		Framegraph(Framegraph&&) = delete;
		Framegraph& operator=(Framegraph&&) = delete;
		~Framegraph();

		using AttachmentDescription = FramegraphAttachmentDescription;
		using ColorAttachmentDescription = Rendering::ColorAttachmentDescription;
		using DepthAttachmentDescription = Rendering::DepthAttachmentDescription;
		using StencilAttachmentDescription = Rendering::StencilAttachmentDescription;
		using InputAttachmentDescription = Rendering::InputAttachmentDescription;
		using OutputAttachmentDescription = Rendering::OutputAttachmentDescription;
		using InputOutputAttachmentDescription = Rendering::InputOutputAttachmentDescription;

		using RenderPassDescription = Rendering::RenderPassDescription;
		using RenderSubpassDescription = Rendering::RenderSubpassDescription;
		using ExplicitRenderPassDescription = Rendering::ExplicitRenderPassDescription;
		using GenericSubpassDescription = Rendering::GenericSubpassDescription;
		using SubpassAttachmentReference = Rendering::SubpassAttachmentReference;
		using SubpassInputAttachmentReference = Rendering::SubpassInputAttachmentReference;
		using SubpassOutputAttachmentReference = Rendering::SubpassOutputAttachmentReference;
		using SubpassInputOutputAttachmentReference = Rendering::SubpassInputOutputAttachmentReference;
		using GenericPassDescription = Rendering::GenericPassDescription;
		using ComputeSubpassDescription = Rendering::ComputeSubpassDescription;
		using ComputePassDescription = Rendering::ComputePassDescription;
		using StageDescription = Rendering::StageDescription;
		using RenderPassStageDescription = Rendering::RenderPassStageDescription;
		using ExplicitRenderPassStageDescription = Rendering::ExplicitRenderPassStageDescription;
		using ComputePassStageDescription = Rendering::ComputePassStageDescription;
		using GenericStageDescription = Rendering::GenericStageDescription;

		inline static constexpr Asset::Guid RenderOutputRenderTargetGuid = "F81D546B-7C63-4B54-BB62-49ACEE00B429"_asset;

		using AttachmentIdentifier = Rendering::AttachmentIdentifier;

		using PassIndex = Rendering::PassIndex;
		inline static constexpr PassIndex InvalidPassIndex = Rendering::InvalidPassIndex;
		using SubpassIndex = Rendering::SubpassIndex;
		using StageIndex = Rendering::StageIndex;
		inline static constexpr StageIndex InvalidStageIndex = Rendering::InvalidStageIndex;
		using AttachmentIndex = Rendering::AttachmentIndex;
		inline static constexpr AttachmentIndex InvalidAttachmentIndex = Rendering::InvalidAttachmentIndex;

		using PassAttachmentReference = Rendering::PassAttachmentReference;

		using AttachmentFlags = FramegraphAttachmentFlags;

		void Compile(const ArrayView<const StageDescription, StageIndex> stages, Threading::JobBatch& jobBatch);
		void Enable();
		void Disable();
		[[nodiscard]] bool IsEnabled() const
		{
			return m_isEnabled;
		}
		void Reset();

		[[nodiscard]] bool HasPendingCompilationTasks() const
		{
			return m_pendingCompilationTasks.Load() > 0;
		}

		void OnBeforeRenderOutputResize();

		//! Gets the pass that owns / executes the specified stage
		[[nodiscard]] Optional<Stage*> GetStagePass(Stage& stage) const;

		[[nodiscard]] Threading::Job& GetStartStage()
		{
			return m_startStage;
		}
		[[nodiscard]] Threading::Job& GetFinalStage()
		{
			return m_endStage;
		}
		[[nodiscard]] Threading::Job& GetFinishFrameGpuExecutionStage() const
		{
			return m_finishFrameGpuExecutionStage;
		}

		[[nodiscard]] StartFrameStage& GetAcquireRenderOutputImageStage() const
		{
			return *m_pAcquireRenderOutputImageStage;
		}
		[[nodiscard]] PresentStage& GetPresentRenderOutputImageStage() const
		{
			return *m_pPresentRenderOutputImageStage;
		}

		void WaitForProcessingFramesToFinish(const FrameMask frameMask);

		[[nodiscard]] bool IsProcessingFrames(const FrameMask frameMask) const
		{
			return ((m_processingFrameCpuMask.Load() & frameMask) != 0) || ((m_processingFrameGpuMask.Load() & frameMask) != 0);
		}
	protected:
		void OnPendingCompilationTaskCompleted();

		void CompileColorAttachments(
			RenderPassInfo& renderPassInfo,
			const ArrayView<const ColorAttachmentDescription, AttachmentIndex> colorAttachmentDescriptions,
			const PassIndex passIndex,
			const QueueFamilyIndex queueFamilyIndex,
			IdentifierMask<AttachmentIdentifier>& processedAttachments,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos,
			const AttachmentIdentifier renderOutputAttachmentIdentifier
		);
		void CompileDepthAttachment(
			RenderPassInfo& renderPassInfo,
			const PassAttachmentReference depthAttachmentReference,
			const DepthAttachmentDescription& __restrict depthAttachmentDescription,
			const QueueFamilyIndex queueFamilyIndex,
			IdentifierMask<AttachmentIdentifier>& processedAttachments,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
		);
		void CompileStencilAttachment(
			RenderPassInfo& renderPassInfo,
			const PassAttachmentReference stencilAttachmentReference,
			const StencilAttachmentDescription& __restrict stencilAttachmentDescription,
			const Optional<DepthAttachmentDescription>& __restrict depthAttachmentDescription,
			const QueueFamilyIndex queueFamilyIndex,
			IdentifierMask<AttachmentIdentifier>& processedAttachments,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
		);
		void CompileInputAttachment(
			const PassAttachmentReference inputAttachmentReference,
			const AttachmentIdentifier attachmentIdentifier,
			const EnumFlags<PipelineStageFlags> pipelineStageFlags,
			const EnumFlags<AccessFlags> accessFlags,
			const QueueFamilyIndex queueFamilyIndex,
			const ImageSubresourceRange subresourceRange,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
		);
		void CompileOutputAttachment(
			const PassAttachmentReference outputAttachmentReference,
			const AttachmentIdentifier attachmentIdentifier,
			const EnumFlags<PipelineStageFlags> pipelineStageFlags,
			const EnumFlags<AccessFlags> accessFlags,
			const ImageLayout attachmentTargetLayout,
			const QueueFamilyIndex queueFamilyIndex,
			const ImageSubresourceRange subresourceRange,
			IdentifierMask<AttachmentIdentifier>& processedAttachments,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
		);
		void CompileInputOutputAttachment(
			const PassAttachmentReference inputOutputAttachmentReference,
			const AttachmentIdentifier attachmentIdentifier,
			const EnumFlags<PipelineStageFlags> pipelineStageFlags,
			const EnumFlags<AccessFlags> accessFlags,
			const ImageLayout attachmentTargetLayout,
			const QueueFamilyIndex queueFamilyIndex,
			const ImageSubresourceRange subresourceRange,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
		);

		void QueueLoadAttachment(
			PassInfo& __restrict passInfo,
			const AttachmentDescription& __restrict attachmentDescription,
			const AttachmentIndex localAttachmentIndex,
			const EnumFlags<ImageAspectFlags> imageAspectFlags,
			Threading::JobBatch& jobBatch,
			const AttachmentIdentifier renderOutputAttachmentIdentifier,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos,
			TextureCache& textureCache,
			RenderTargetCache& renderTargetCache
		);

		void CompileRenderPassAttachments(
			const StageDescription& stageDescription,
			const QueueFamilyIndex graphicsQueueFamilyIndex,
			const PassIndex passIndex,
			PassInfo& passInfo,
			const AttachmentIdentifier renderOutputAttachmentIdentifier,
			IdentifierMask<AttachmentIdentifier>& processedAttachments,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
		);
		void CompileExplicitRenderPassAttachments(
			const StageDescription& stageDescription,
			const QueueFamilyIndex graphicsQueueFamilyIndex,
			const PassIndex passIndex,
			PassInfo& passInfo,
			const AttachmentIdentifier renderOutputAttachmentIdentifier,
			IdentifierMask<AttachmentIdentifier>& processedAttachments,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfo
		);
		void CompileGenericPassAttachments(
			const StageDescription& stageDescription,
			const QueueFamilyIndex queueFamilyIndex,
			const PassIndex passIndex,
			PassInfo& passInfo,
			const AttachmentIdentifier renderOutputAttachmentIdentifier,
			IdentifierMask<AttachmentIdentifier>& processedAttachments,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
		);
		void CompileComputePassAttachments(
			const StageDescription& stageDescription,
			const QueueFamilyIndex computeQueueFamilyIndex,
			const PassIndex passIndex,
			PassInfo& passInfo,
			const AttachmentIdentifier renderOutputAttachmentIdentifier,
			IdentifierMask<AttachmentIdentifier>& processedAttachments,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
		);

		void TryTransitionAttachmentLayout(
			const AttachmentDescription& attachmentDescription,
			const PassAttachmentReference attachmentReference,
			const EnumFlags<AccessFlags> accessFlags,
			const ImageLayout attachmentTargetLayout,
			const QueueFamilyIndex queueFamilyIndex,
			const ImageSubresourceRange subresourceRange,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
		);

		void CreateRenderPass(
			const StageDescription& stageDescription,
			PassInfo& passInfo,
			const AttachmentIdentifier renderOutputAttachmentIdentifier,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos,
			TextureCache& textureCache,
			RenderTargetCache& renderTargetCache,
			Threading::JobBatch& jobBatch
		);
		void CreateExplicitRenderPass(
			const StageDescription& stageDescription,
			PassInfo& passInfo,
			const AttachmentIdentifier renderOutputAttachmentIdentifier,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos,
			TextureCache& textureCache,
			RenderTargetCache& renderTargetCache,
			Threading::JobBatch& jobBatch
		);
		void CreateGenericPass(
			const StageDescription& stageDescription,
			const PassIndex passIndex,
			PassInfo& passInfo,
			const AttachmentIdentifier renderOutputAttachmentIdentifier,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos,
			const QueueFamilyIndex queueFamilyIndex,
			TextureCache& textureCache,
			RenderTargetCache& renderTargetCache,
			Threading::JobBatch& jobBatch
		);
		void CreateComputePass(
			const StageDescription& stageDescription,
			const PassIndex passIndex,
			PassInfo& passInfo,
			const AttachmentIdentifier renderOutputAttachmentIdentifier,
			const FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos,
			const QueueFamilyIndex queueFamilyIndex,
			TextureCache& textureCache,
			RenderTargetCache& renderTargetCache,
			Threading::JobBatch& jobBatch
		);

		void OnStartFrame();
		void OnEndFrame(const FrameIndex frameIndex);

		void OnFinishFrameGpuExecution(const FrameIndex frameIndex);
	protected:
		friend RenderPassStage;
		friend GenericPassStage;
		friend ComputePassStage;
		friend AttachmentInfo;
		friend PassInfo;

		LogicalDevice& m_logicalDevice;
		RenderOutput& m_renderOutput;

		bool m_isEnabled{false};

		Threading::Job& m_startStage;
		Threading::Job& m_endStage;
		Threading::Job& m_finishFrameGpuExecutionStage;
		Optional<Threading::IntermediateStage*> m_pCompiledDependenciesStage;

		UniqueRef<StartFrameStage> m_pAcquireRenderOutputImageStage;
		UniqueRef<PresentStage> m_pPresentRenderOutputImageStage;

		TIdentifierArray<AttachmentInfo, AttachmentIdentifier> m_attachmentInfos;

		Vector<PassInfo, PassIndex> m_passes;
		Vector<PassIndex, StageIndex> m_stagePassIndices;
		IdentifierMask<TextureIdentifier> m_requestedRenderTargets;

		PassIndex m_firstRenderOutputPassIndex{InvalidPassIndex};
		PassIndex m_lastRenderOutputPassIndex{InvalidPassIndex};
		Vector<PassIndex> m_renderOutputDependencies;

		Threading::Atomic<uint16> m_pendingCompilationTasks{0};
		Threading::Atomic<FrameMask> m_processingFrameCpuMask{0};
		Threading::Atomic<FrameMask> m_processingFrameGpuMask{0};
		Optional<Threading::EngineJobRunnerThread*> m_pStartFrameThread;
	};
}
