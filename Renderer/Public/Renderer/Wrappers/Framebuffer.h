#pragma once

#include "FramebufferView.h"

#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>
#include <Common/Math/ForwardDeclarations/Vector2.h>

#if RENDERER_METAL || RENDERER_WEBGPU
#include <Renderer/Wrappers/ImageMappingView.h>
#include <Common/Memory/Containers/InlineVector.h>
#endif

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct RenderPassView;
	struct ImageMappingView;
	struct RenderOutput;
	struct FrameImageId;

#if RENDERER_METAL || RENDERER_WEBGPU
	namespace Internal
	{
		struct FramebufferData
		{
			Optional<const RenderOutput*> m_pRenderOutput;
			InlineVector<ImageMappingView, 8, uint8> m_attachmentMappings;
		};
	}
#endif

	struct Framebuffer : public FramebufferView
	{
		Framebuffer() = default;
		Framebuffer(
			const LogicalDeviceView logicalDevice,
			const RenderPassView renderPass,
			const ArrayView<const ImageMappingView, uint8> imageAttachments,
			const Math::Vector2ui size,
			const uint32 layerCount = 1
		);
		Framebuffer(
			const LogicalDeviceView logicalDevice,
			const RenderPassView renderPass,
			const RenderOutput& renderOutput,
			const FrameImageId frameImageId,
			const ArrayView<const ImageMappingView, uint8> imageAttachments,
			const Math::Vector2ui size
		);
		Framebuffer(const Framebuffer&) = delete;
		Framebuffer& operator=(const Framebuffer&) = delete;
		Framebuffer([[maybe_unused]] Framebuffer&& other)
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			m_pFrameBuffer = other.m_pFrameBuffer;
			other.m_pFrameBuffer = 0;
#endif
		}
		Framebuffer& operator=(Framebuffer&& other);
		~Framebuffer();

		void Destroy(const LogicalDeviceView logicalDevice);
	};
}
