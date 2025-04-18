#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Wrappers/ImageMappingView.h>

#include <Renderer/Devices/LogicalDeviceView.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/FrameImageId.h>

#include <Common/Assert/Assert.h>
#include <Common/Math/Vector2.h>
#include <Common/Memory/Containers/ArrayView.h>
#include <Common/Memory/Containers/InlineVector.h>

#include <Renderer/Vulkan/Includes.h>

namespace ngine::Rendering
{
	Framebuffer::Framebuffer(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const RenderPassView renderPass,
		const ArrayView<const ImageMappingView, uint8> imageAttachments,
		[[maybe_unused]] const Math::Vector2ui outputSize,
		[[maybe_unused]] const uint32 layerCount
	)
	{

		Assert(imageAttachments.All(
			[](const ImageMappingView mapping)
			{
				return mapping.IsValid();
			}
		));

#if RENDERER_VULKAN
		const VkFramebufferCreateInfo framebufferInfo = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			nullptr,
			0,
			renderPass,
			imageAttachments.GetSize(),
			reinterpret_cast<const VkImageView*>(imageAttachments.GetData()),
			outputSize.x,
			outputSize.y,
			layerCount
		};

		[[maybe_unused]] const VkResult creationResult = vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &m_pFrameBuffer);
		Assert(creationResult == VK_SUCCESS);
#elif RENDERER_METAL || RENDERER_WEBGPU
		Assert(layerCount == 1);
		Internal::FramebufferData* __restrict pFramebufferData = new Internal::FramebufferData{};
		pFramebufferData->m_attachmentMappings.CopyEmplaceRangeBack(imageAttachments);
		m_pFrameBuffer = pFramebufferData;
#else
		UNUSED(logicalDevice);
		UNUSED(renderPass);
		UNUSED(imageAttachments);
		UNUSED(outputSize);
		UNUSED(layerCount);
#endif
	}

	Framebuffer::Framebuffer(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const RenderPassView renderPass,
		const RenderOutput& renderOutput,
		[[maybe_unused]] const FrameImageId frameImageId,
		const ArrayView<const ImageMappingView, uint8> imageAttachments,
		[[maybe_unused]] const Math::Vector2ui outputSize
	)
	{
#if RENDERER_VULKAN
		FixedCapacityInlineVector<ImageMappingView, 8, uint8> attachments{Memory::Reserve, uint8(imageAttachments.GetSize() + 1)};
		attachments.EmplaceBack(renderOutput.GetColorImageMappings()[(uint8)frameImageId]);
		attachments.CopyEmplaceRangeBack(imageAttachments);

		const VkFramebufferCreateInfo framebufferInfo = {
			VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
			nullptr,
			0,
			renderPass,
			attachments.GetSize(),
			reinterpret_cast<const VkImageView*>(attachments.GetData()),
			outputSize.x,
			outputSize.y,
			1
		};

		[[maybe_unused]] const VkResult creationResult = vkCreateFramebuffer(logicalDevice, &framebufferInfo, nullptr, &m_pFrameBuffer);
		Assert(creationResult == VK_SUCCESS);
#elif RENDERER_METAL || RENDERER_WEBGPU
		Internal::FramebufferData* __restrict pFramebufferData = new Internal::FramebufferData{&renderOutput};
		// Emplace an invalid element, will be substituted at runtime since we can't get all image mappings until after each frame image is
		// acquired.
		pFramebufferData->m_attachmentMappings.EmplaceBack();
		pFramebufferData->m_attachmentMappings.CopyEmplaceRangeBack(imageAttachments);
		m_pFrameBuffer = pFramebufferData;
#else
		Assert(false, "TODO");
		UNUSED(logicalDevice);
		UNUSED(renderPass);
		UNUSED(renderOutput);
		UNUSED(imageAttachments);
		UNUSED(outputSize);
#endif
	}

	Framebuffer& Framebuffer::operator=([[maybe_unused]] Framebuffer&& other)
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		m_pFrameBuffer = other.m_pFrameBuffer;
		other.m_pFrameBuffer = 0;
#endif
		return *this;
	}

	Framebuffer::~Framebuffer()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void Framebuffer::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroyFramebuffer(logicalDevice, m_pFrameBuffer, nullptr);
		m_pFrameBuffer = 0;
#elif RENDERER_METAL || RENDERER_WEBGPU
		delete m_pFrameBuffer;
		m_pFrameBuffer = nullptr;
#endif
	}
}
