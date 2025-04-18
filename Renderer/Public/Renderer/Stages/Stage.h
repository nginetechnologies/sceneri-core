#pragma once

#include <Renderer/Stages/StageBase.h>
#include <Renderer/Threading/Semaphore.h>
#include <Renderer/Threading/Fence.h>
#include <Renderer/Constants.h>
#include <Renderer/Devices/QueueFamily.h>
#include <Renderer/FrameImageId.h>
#include <Renderer/PipelineStageFlags.h>
#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/RenderCommandEncoderView.h>
#include <Renderer/Commands/ComputeCommandEncoderView.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Wrappers/ImageMappingView.h>
#include <Renderer/Commands/EncodedCommandBuffer.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Threading/Jobs/IntermediateStage.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Log2.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Function/Function.h>

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
	struct JobBatch;
	struct AsyncCallbackJobBase;
}

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct StartFrameStage;
	struct PresentStage;
	struct ViewMatrices;
	struct Pass;
	struct PassInfo;
	struct PassStage;
	struct GenericPassStage;
	struct ComputePassStage;
	struct RenderPassView;
	struct Framegraph;
	struct ImageMappingView;
	struct RenderTexture;

	struct Stage : public StageBase
	{
		inline static constexpr Guid TypeGuid = "{382C6283-BD0E-431A-BCC0-925072B9E4E2}"_guid;

		enum class Flags : uint8
		{
			Skipped = 1 << 0,
			EvaluatedSkipped = 1 << 1,
			//! Indicates that this stage is managed by a pass, and should not execute its own logic
			ManagedByPass = 1 << 2,
			Enabled = 1 << 3,
			AwaitingSubmission = 1 << 4,
			AwaitingGPUFinish = 1 << 5
		};

		Stage(LogicalDevice& logicalDevice, const Threading::JobPriority priority, const EnumFlags<Flags> flags = Flags::Enabled);
		virtual ~Stage();

		void SetManagedByRenderPass()
		{
			m_flags |= Flags::ManagedByPass;
		}
		[[nodiscard]] bool IsManagedByRenderPass() const
		{
			return m_flags.IsSet(Flags::ManagedByPass);
		}

		[[nodiscard]] virtual QueueFamily GetRecordedQueueFamily() const
		{
			return QueueFamily::Graphics;
		}
		[[nodiscard]] virtual bool ShouldRecordCommands() const
		{
			return true;
		}
		[[nodiscard]] bool EvaluateShouldSkip();

		[[nodiscard]] virtual SemaphoreView GetSubmissionFinishedSemaphore(const Threading::Job& stage) const override final;
		[[nodiscard]] virtual bool IsSubmissionFinishedSemaphoreUsable() const override final;

		[[nodiscard]] virtual FenceView GetSubmissionFinishedFence() const override final
		{
			return m_drawingCompletePresentFence;
		}
		[[nodiscard]] virtual bool IsSubmissionFinishedFenceUsable() const override final
		{
			return m_drawingCompletePresentFence.IsValid();
		}

		[[nodiscard]] bool WasSkipped() const
		{
			return m_flags.IsSet(Flags::Skipped);
		}

		void Enable()
		{
			m_flags.TrySetFlags(Flags::Enabled);
		}

		void Disable()
		{
			m_flags.TryClearFlags(Flags::Enabled);
		}

		[[nodiscard]] bool IsEnabled() const
		{
			return m_flags.IsSet(Flags::Enabled);
		}
		[[nodiscard]] bool IsDisabled() const
		{
			return !m_flags.IsSet(Flags::Enabled);
		}

		using StageBase::AddSubsequentGpuStage;
		using StageBase::RemoveSubsequentGpuStage;
		void AddSubsequentGpuStage(PresentStage& stage);
		void RemoveSubsequentGpuStage(PresentStage& stage);
		using StageBase::AddSubsequentCpuGpuStage;
		using StageBase::RemoveSubsequentCpuGpuStage;
		void AddSubsequentCpuGpuStage(PresentStage& stage);
		void RemoveSubsequentCpuGpuStage(
			PresentStage& stage,
			Threading::JobRunnerThread& thread,
			const Threading::StageBase::RemovalFlags flags = Threading::StageBase::RemovalFlags::Default
		);

		[[nodiscard]] virtual ArrayView<const ReferenceWrapper<StageBase>, uint8> GetParentStages() const override final
		{
			return m_parentStages;
		}

		[[nodiscard]] Threading::AsyncCallbackJobBase& GetSubmitJob() const
		{
			return m_submitJob;
		}
		[[nodiscard]] Threading::AsyncCallbackJobBase& GetFinishedExecutionStage()
		{
			return m_finishedExecutionStage;
		}

		virtual void OnAddedSubsequentGpuStage(Stage&) override final;
		virtual void OnRemovedSubsequentGpuStage(Stage&) override final;

#if PROFILE_BUILD
		// Ensure stages return a valid debug name
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override = 0;
#endif
		void AddDependency(Stage& stage)
		{
			m_dependencies.EmplaceBack(stage);
		}
	protected:
		friend Pass;
		friend PassInfo;
		friend PassStage;
		friend GenericPassStage;
		friend ComputePassStage;
		friend Framegraph;

		[[nodiscard]] virtual uint32 GetMaximumPushConstantInstanceCount() const
		{
			return 0;
		}

		virtual void OnBeforeRenderPassDestroyed()
		{
		}
		[[nodiscard]] virtual Threading::JobBatch AssignRenderPass(
			const RenderPassView,
			[[maybe_unused]] const Math::Rectangleui outputArea,
			[[maybe_unused]] const Math::Rectangleui fullRenderArea,
			[[maybe_unused]] const uint8 subpassIndex
		);
		virtual void OnRenderPassAttachmentsLoaded(
			[[maybe_unused]] const Math::Vector2ui resolution,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, FrameIndex> colorAttachmentMappings,
			[[maybe_unused]] const ArrayView<ImageMappingView, FrameIndex> depthAttachmentMapping,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, FrameIndex> subpassInputAttachmentMappings,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, FrameIndex> externalInputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> externalInputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, FrameIndex> colorAttachments,
			[[maybe_unused]] const uint8 subpassIndex
		)
		{
		}
		virtual void OnComputePassAttachmentsLoaded(
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, FrameIndex> outputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> outputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, FrameIndex> outputAttachments,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, FrameIndex> outputInputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> outputInputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, FrameIndex> outputInputAttachments,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, FrameIndex> inputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> inputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, FrameIndex> inputAttachments,
			[[maybe_unused]] const uint8 subpassIndex
		)
		{
		}
		virtual void OnGenericPassAttachmentsLoaded(
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, FrameIndex> outputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> outputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, FrameIndex> outputInputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> outputInputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const ImageMappingView, uint16>, FrameIndex> inputAttachmentMappings,
			[[maybe_unused]] const ArrayView<const Math::Vector2ui, uint16> inputAttachmentResolutions,
			[[maybe_unused]] const ArrayView<ArrayView<const Optional<RenderTexture*>, uint16>, FrameIndex> outputAttachments
		)
		{
		}

		virtual Result OnExecute(Threading::JobRunnerThread& thread) override final;

		virtual void OnBeforeRecordCommands(const CommandEncoderView)
		{
		}
		virtual void RecordCommands(const CommandEncoderView)
		{
		}
		virtual void RecordRenderPassCommands(
			RenderCommandEncoder&,
			const ViewMatrices&,
			[[maybe_unused]] const Math::Rectangleui renderArea,
			[[maybe_unused]] const uint8 subpassIndex
		)
		{
		}
		virtual void RecordComputePassCommands(const ComputeCommandEncoderView, const ViewMatrices&, [[maybe_unused]] const uint8 subpassIndex)
		{
		}
		virtual void OnAfterRecordCommands(const CommandEncoderView)
		{
		}
		void RecordCommandsInternal(const CommandEncoderView commandEncoder)
		{
			OnBeforeRecordCommands(commandEncoder);
			RecordCommands(commandEncoder);
			OnAfterRecordCommands(commandEncoder);
		}
		void SubmitEncodedCommandBuffer();
		virtual void OnCommandsExecuted()
		{
		}
	protected:
		LogicalDevice& m_logicalDevice;
	private:
		Threading::Mutex m_drawingCompleteSemaphoresMutex;
		Vector<Semaphore, uint8> m_drawingCompleteSemaphores;
		Semaphore m_drawingCompletePresentSemaphore;
		Fence m_drawingCompletePresentFence;

		Vector<ReferenceWrapper<Rendering::Stage>, uint8> m_subsequentGpuStages;
		Optional<Threading::Job*> m_pSubsequentGpuPresentStage;
		Vector<SemaphoreView, uint8> m_waitSemaphores;
		Vector<SemaphoreView, uint8> m_signalSemaphores;
		FenceView m_signalFence;
		Vector<EnumFlags<PipelineStageFlags>, uint8> m_waitStagesMasks;

		AtomicEnumFlags<Flags> m_flags{Flags::Enabled};
		uint8 m_evaluatedSkippedFrameIndex{Math::NumericLimits<uint8>::Max};

		EncodedCommandBuffer m_encodedCommandBuffer;
		Optional<Threading::EngineJobRunnerThread*> m_pThreadRunner{nullptr};

		friend StageBase;
		friend StartFrameStage;
		Vector<ReferenceWrapper<StageBase>, uint8> m_parentStages;
		Vector<ReferenceWrapper<Stage>> m_dependencies;

		Threading::AsyncCallbackJobBase& m_submitJob;
		Threading::AsyncCallbackJobBase& m_finishedExecutionStage;
	};

	ENUM_FLAG_OPERATORS(Stage::Flags);
}
