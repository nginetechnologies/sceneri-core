#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/FramebufferView.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/SubpassDescription.h>
#include <Renderer/Wrappers/SubpassDependency.h>

#include <Renderer/Devices/LogicalDeviceView.h>
#include <Renderer/Commands/CommandEncoderView.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Math/Select.h>
#include <Common/Math/Color.h>

#include <Renderer/Vulkan/Includes.h>

namespace ngine::Rendering
{
#if RENDERER_VULKAN
	static_assert((uint32)ImageLayout::ColorAttachmentOptimal == VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
	static_assert((uint32)ImageLayout::DepthStencilAttachmentOptimal == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
#endif

	RenderPass::RenderPass(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		const ArrayView<const AttachmentDescription, uint8> attachments,
		[[maybe_unused]] const ArrayView<const SubpassDescription, uint8> subpassDescriptions,
		[[maybe_unused]] const ArrayView<const SubpassDependency, uint8> subpassDependencies
	)
	{
		Assert(attachments.All(
			[](const AttachmentDescription& attachment)
			{
				return attachment.m_format != Format::Invalid;
			}
		));

#if RENDERER_VULKAN
		static_assert(sizeof(AttachmentReference) == sizeof(VkAttachmentReference));
		static_assert(alignof(AttachmentReference) == alignof(VkAttachmentReference));
		static_assert(sizeof(AttachmentDescription) == sizeof(VkAttachmentDescription));
		static_assert(alignof(AttachmentDescription) == alignof(VkAttachmentDescription));
		static_assert(sizeof(SubpassDescription) == sizeof(VkSubpassDescription));
		static_assert(alignof(SubpassDescription) == alignof(VkSubpassDescription));
		static_assert(sizeof(SubpassDependency) == sizeof(VkSubpassDependency));
		static_assert(alignof(SubpassDependency) == alignof(SubpassDependency));
#endif

#if RENDERER_VULKAN
		const VkRenderPassCreateInfo renderPassInfo = {
			VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO,
			nullptr,
			0,
			attachments.GetSize(),
			reinterpret_cast<const VkAttachmentDescription*>(attachments.GetData()),
			subpassDescriptions.GetSize(),
			reinterpret_cast<const VkSubpassDescription*>(subpassDescriptions.GetData()),
			subpassDependencies.GetSize(),
			reinterpret_cast<const VkSubpassDependency*>(subpassDependencies.GetData()),
		};

		[[maybe_unused]] const VkResult creationResult = vkCreateRenderPass(logicalDevice, &renderPassInfo, nullptr, &m_pRenderPass);
		Assert(creationResult == VK_SUCCESS);
#elif RENDERER_METAL || RENDERER_WEBGPU
		Internal::RenderPassData* __restrict pData = new Internal::RenderPassData();
		pData->m_attachmentDescriptions.CopyEmplaceRangeBack(attachments);

		Assert(subpassDescriptions.HasElements());
		pData->m_subpassColorAttachmentIndices.Reserve(subpassDescriptions.GetSize());
		pData->m_subpassInputAttachmentIndices.Reserve(subpassDescriptions.GetSize());
		pData->m_subpassDepthAttachmentIndices.Reserve(subpassDescriptions.GetSize());
		for (const SubpassDescription& __restrict subpassDescription : subpassDescriptions)
		{
			Internal::RenderPassData::SubpassAttachmentIndices& subpassInputAttachmentIndices =
				pData->m_subpassInputAttachmentIndices.EmplaceBack();
			Internal::RenderPassData::SubpassAttachmentIndices& subpassColorAttachmentIndices =
				pData->m_subpassColorAttachmentIndices.EmplaceBack();
			uint8& subpassDepthAttachmentIndex = pData->m_subpassDepthAttachmentIndices.EmplaceBack();
			subpassInputAttachmentIndices.Reserve(subpassDescription.GetInputAttachments().GetSize());
			subpassColorAttachmentIndices.Reserve(subpassDescription.GetColorAttachments().GetSize());
			for (const AttachmentReference& __restrict attachmentReference : subpassDescription.GetInputAttachments())
			{
				subpassInputAttachmentIndices.EmplaceBack((uint8)attachmentReference.m_index);
			}
			for (const AttachmentReference& __restrict attachmentReference : subpassDescription.GetColorAttachments())
			{
				subpassColorAttachmentIndices.EmplaceBack((uint8)attachmentReference.m_index);
			}
			if (const Optional<const AttachmentReference*> pDepthStencilAttachmentReference = subpassDescription.GetDepthStencilAttachment())
			{
				subpassDepthAttachmentIndex = (uint8)pDepthStencilAttachmentReference->m_index;
			}
			else
			{
				subpassDepthAttachmentIndex = Internal::RenderPassData::InvalidAttachmentIndex;
			}
		}
		m_pRenderPass = pData;
#else
		UNUSED(logicalDevice);
		UNUSED(attachments);
		UNUSED(subpassDescriptions);
		UNUSED(subpassDependencies);
#endif
	}

	RenderPass::RenderPass(
		const LogicalDeviceView logicalDevice,
		const ArrayView<const AttachmentDescription, uint8> attachments,
		const ArrayView<const AttachmentReference, uint8> colorAttachmentReferences,
		const ArrayView<const AttachmentReference, uint8> resolveAttachmentReferences,
		const Optional<const AttachmentReference*> pDepthAttachmentReference,
		const ArrayView<const SubpassDependency, uint8> subpassDependencies
	)
		: RenderPass(
				logicalDevice,
				attachments,
				Array{SubpassDescription{{}, colorAttachmentReferences, resolveAttachmentReferences, pDepthAttachmentReference.Get()}
	      }.GetDynamicView(),
				subpassDependencies
			)
	{
	}

	RenderPass& RenderPass::operator=([[maybe_unused]] RenderPass&& other)
	{
		Assert(!IsValid(), "Destroy must have been called");
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		m_pRenderPass = other.m_pRenderPass;
		other.m_pRenderPass = 0;
#endif
		return *this;
	}

	RenderPass::~RenderPass()
	{
		Assert(!IsValid(), "Destroy must have been called");
	}

	void RenderPass::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroyRenderPass(logicalDevice, m_pRenderPass, nullptr);
		m_pRenderPass = 0;
#elif RENDERER_METAL || RENDERER_WEBGPU
		delete m_pRenderPass;
		m_pRenderPass = nullptr;
#endif
	}
}
