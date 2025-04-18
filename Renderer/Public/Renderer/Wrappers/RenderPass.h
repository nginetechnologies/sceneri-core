#pragma once

#include "RenderPassView.h"

#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>
#include <Common/Memory/ForwardDeclarations/Optional.h>

#if RENDERER_METAL || RENDERER_WEBGPU
#include <Common/Memory/Containers/InlineVector.h>
#include <Renderer/Format.h>
#include <Renderer/Wrappers/ImageMappingView.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/SubpassDescription.h>
#endif

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct AttachmentDescription;
	struct AttachmentReference;
	struct SubpassDescription;
	struct SubpassDependency;

#if RENDERER_METAL || RENDERER_WEBGPU
	namespace Internal
	{
		struct RenderPassData
		{
			using SubpassAttachmentIndices = InlineVector<uint8, 4>;
			inline static constexpr uint8 InvalidAttachmentIndex = Math::NumericLimits<uint8>::Max;

			InlineVector<AttachmentDescription, 8, uint8> m_attachmentDescriptions;
			InlineVector<SubpassAttachmentIndices, 8, uint8> m_subpassInputAttachmentIndices;
			InlineVector<SubpassAttachmentIndices, 8, uint8> m_subpassColorAttachmentIndices;
			InlineVector<uint8, 8, uint8> m_subpassDepthAttachmentIndices;
		};
	}
#endif

	struct RenderPass : public RenderPassView
	{
		RenderPass() = default;
		RenderPass(
			const LogicalDeviceView logicalDevice,
			const ArrayView<const AttachmentDescription, uint8> attachments,
			const ArrayView<const SubpassDescription, uint8> subpassDescriptions,
			const ArrayView<const SubpassDependency, uint8> subpassDependencies
		);
		RenderPass(
			const LogicalDeviceView logicalDevice,
			const ArrayView<const AttachmentDescription, uint8> attachments,
			const ArrayView<const AttachmentReference, uint8> colorAttachmentReferences,
			const ArrayView<const AttachmentReference, uint8> resolveAttachmentReferences,
			const Optional<const AttachmentReference*> pDepthAttachmentReference,
			const ArrayView<const SubpassDependency, uint8> subpassDependencies
		);
		RenderPass(const RenderPass&) = delete;
		RenderPass& operator=(const RenderPass&) = delete;
		RenderPass([[maybe_unused]] RenderPass&& other)
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			m_pRenderPass = other.m_pRenderPass;
			other.m_pRenderPass = 0;
#endif
		}
		RenderPass& operator=(RenderPass&& other);
		~RenderPass();

		void Destroy(const LogicalDeviceView logicalDevice);
	};
}
