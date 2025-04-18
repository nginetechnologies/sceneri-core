#pragma once

#include "AttachmentInfo.h"

#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Devices/LogicalDeviceIdentifier.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Storage/Identifier.h>
#include <Common/Threading/Jobs/StageBase.h>

namespace ngine::Rendering
{
	struct RenderPassStage;
	struct GenericPassStage;
	struct ComputePassStage;
	struct TextureCache;
	struct RenderOutput;
	struct RenderTexture;

	struct SubpassInfo
	{
		InlineVector<Vector<ImageMappingView, AttachmentIndex>, Rendering::MaximumConcurrentFrameCount> m_imageMappingViews;
		InlineVector<Vector<ImageMapping, AttachmentIndex>, Rendering::MaximumConcurrentFrameCount> m_imageMappings;
		InlineVector<Vector<Optional<RenderTexture*>, AttachmentIndex>, Rendering::MaximumConcurrentFrameCount> m_textures;
	};

	struct RenderSubpassInfo : public SubpassInfo
	{
		InlineVector<ReferenceWrapper<Stage>, 1> m_stages;
		InlineVector<AttachmentReference, 8> m_attachmentReferences;
		ArrayView<const AttachmentReference, AttachmentIndex> m_colorAttachments;
		ArrayView<const AttachmentReference, AttachmentIndex> m_externalInputAttachments;
		ArrayView<const AttachmentReference, AttachmentIndex> m_subpassInputAttachments;
		ArrayView<const AttachmentReference, AttachmentIndex> m_resolveAttachments;
		Optional<const AttachmentReference*> m_pDepthAttachment;
	};

	struct RenderPassInfo
	{
		Math::Rectangleui m_drawableArea;
		UniquePtr<RenderPassStage> m_pPass;
		Vector<RenderSubpassInfo, SubpassIndex> m_subpasses;
		InlineVector<SubpassDependency, 8> m_subpassDependencies;
		Vector<Rendering::AttachmentDescription, AttachmentIndex> m_attachmentDescriptions;
		Vector<Rendering::ImageMappingType, AttachmentIndex> m_attachmentMappingTypes;
		Vector<ImageSubresourceRange, AttachmentIndex> m_attachmentSubresourceRanges;

		struct ExternalInputAttachmentState
		{
			AttachmentIdentifier m_attachmentIdentifier;
			AttachmentIndex m_attachmentIndex;
			ImageSubresourceRange m_subresourceRange;
			SubresourceState m_subresourceState;
		};
		Vector<ExternalInputAttachmentState, AttachmentIndex> m_externalInputAttachmentStates;

		InlineVector<Vector<ImageMappingView, AttachmentIndex>, Rendering::MaximumConcurrentFrameCount> m_imageMappingViews;
		InlineVector<Vector<ImageMapping, AttachmentIndex>, Rendering::MaximumConcurrentFrameCount> m_imageMappings;
		InlineVector<Vector<Optional<RenderTexture*>, AttachmentIndex>, Rendering::MaximumConcurrentFrameCount> m_textures;
		Vector<Math::Vector2ui, AttachmentIndex> m_imageResolutions;
	};

	struct GenericSubpassInfo : public SubpassInfo
	{
		InlineVector<SubpassAttachmentReference, 8, AttachmentIndex> m_attachmentReferences;
		ArrayView<const SubpassAttachmentReference, AttachmentIndex> m_outputAttachments;
		ArrayView<const SubpassAttachmentReference, AttachmentIndex> m_inputOutputAttachments;
		ArrayView<const SubpassAttachmentReference, AttachmentIndex> m_inputAttachments;

		//! The state that each attachment should be to start the subpass
		InlineVector<SubresourceStates, 1, AttachmentIndex> m_requiredAttachmentStates;

		Vector<Math::Vector2ui, AttachmentIndex> m_imageResolutions;
	};

	struct GenericPassInfo
	{
		Stage& m_stage;
		UniquePtr<GenericPassStage> m_pPass;

		InlineVector<GenericSubpassInfo, 1, SubpassIndex> m_subpasses;
	};

	struct ComputeSubpassInfo : public SubpassInfo
	{
#if STAGE_DEPENDENCY_PROFILING
		String m_debugName;
#endif

		InlineVector<SubpassAttachmentReference, 8, AttachmentIndex> m_attachmentReferences;
		ArrayView<const SubpassAttachmentReference, AttachmentIndex> m_outputAttachments;
		ArrayView<const SubpassAttachmentReference, AttachmentIndex> m_inputOutputAttachments;
		ArrayView<const SubpassAttachmentReference, AttachmentIndex> m_inputAttachments;

		Vector<Math::Vector2ui, AttachmentIndex> m_imageResolutions;

		//! The state that each attachment should be to start the subpass
		InlineVector<SubresourceStates, 1, AttachmentIndex> m_requiredAttachmentStates;

		InlineVector<ReferenceWrapper<Stage>, 20, SubpassIndex> m_stages;
	};

	struct ComputePassInfo
	{
		ComputePassInfo(Stage& stage);

		UniquePtr<ComputePassStage> m_pPass;
		Stage& m_stage;
		Vector<ComputeSubpassInfo, SubpassIndex> m_subpasses;
	};

	struct PassInfo
	{
		PassInfo(const StageType type, const Optional<Stage*> pStage, const Optional<SceneViewDrawer*> pSceneViewDrawer);
		PassInfo(const PassInfo&) = delete;
		PassInfo& operator=(const PassInfo&) = delete;
		PassInfo(PassInfo&&) = delete;
		PassInfo& operator=(PassInfo&&) = delete;
		~PassInfo();

		[[nodiscard]] bool OnAttachmentLoaded(
			LogicalDevice& logicalDevice,
			RenderTexture& texture,
			const AttachmentIndex localAttachmentIndex,
			const EnumFlags<ImageAspectFlags> imageAspectFlags,
			const Math::Vector2ui resolution,
			Framegraph& framegraph
		);
		void OnPendingCompilationTaskCompleted(LogicalDevice& logicalDevice, Framegraph& framegraph);

		[[nodiscard]] Optional<RenderPassInfo*> GetRenderPassInfo()
		{
			return Optional<RenderPassInfo*>{&m_renderPassInfo, m_type == StageType::RenderPass || m_type == StageType::ExplicitRenderPass};
		}
		[[nodiscard]] Optional<const RenderPassInfo*> GetRenderPassInfo() const
		{
			return Optional<const RenderPassInfo*>{&m_renderPassInfo, m_type == StageType::RenderPass || m_type == StageType::ExplicitRenderPass};
		}

		[[nodiscard]] Optional<GenericPassInfo*> GetGenericPassInfo()
		{
			return Optional<GenericPassInfo*>{&m_genericPassInfo, m_type == StageType::Generic};
		}
		[[nodiscard]] Optional<const GenericPassInfo*> GetGenericPassInfo() const
		{
			return Optional<const GenericPassInfo*>{&m_genericPassInfo, m_type == StageType::Generic};
		}

		[[nodiscard]] Optional<ComputePassInfo*> GetComputePassInfo()
		{
			return Optional<ComputePassInfo*>{&m_computePassInfo, m_type == StageType::Compute};
		}
		[[nodiscard]] Optional<const ComputePassInfo*> GetComputePassInfo() const
		{
			return Optional<const ComputePassInfo*>{&m_computePassInfo, m_type == StageType::Compute};
		}

		[[nodiscard]] Optional<Stage*> GetStage() const;
		[[nodiscard]] bool ContainsStage(const Stage&) const;

#if STAGE_DEPENDENCY_PROFILING
		String m_debugName;
#endif

		Vector<PassIndex> m_cpuDependencies;
		Vector<PassIndex> m_gpuDependencies;

		StageType m_type;

		// Starting with one task, to be completed when initial compilation completes
		Threading::Atomic<uint16> m_pendingCompilationTasks{1};

		Optional<SceneViewDrawer*> m_pSceneViewDrawer;

		Vector<AttachmentIdentifier, AttachmentIndex> m_attachmentIdentifiers;
	protected:
		union
		{
			RenderPassInfo m_renderPassInfo;
			GenericPassInfo m_genericPassInfo;
			ComputePassInfo m_computePassInfo;
		};
	};
}
