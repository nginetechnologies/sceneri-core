#include <Renderer/Commands/CommandBuffer.h>
#include <Renderer/Commands/CommandEncoder.h>
#include <Renderer/Commands/EncodedCommandBuffer.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Commands/ParallelRenderCommandEncoder.h>
#include <Renderer/Commands/EncodedParallelCommandBuffer.h>
#include <Renderer/Commands/ComputeCommandEncoder.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/Commands/AccelerationStructureCommandEncoder.h>
#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Commands/CommandPoolView.h>
#include <Renderer/Commands/ClearValue.h>

#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Buffers/DataToBufferBatch.h>

#include <Renderer/Descriptors/DescriptorSetView.h>
#include <Metal/DescriptorSetData.h>

#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Wrappers/Image.h>
#include <Renderer/Wrappers/ImageMemoryBarrier.h>
#include <Renderer/Wrappers/BufferMemoryBarrier.h>
#include <Renderer/Wrappers/PipelineLayoutView.h>
#include <Renderer/Wrappers/GraphicsPipelineView.h>
#include <Renderer/Wrappers/ComputePipelineView.h>
#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Window/Window.h>
#include <Renderer/RenderOutput/RenderOutput.h>

#include <Renderer/Pipelines/GraphicsPipeline.h>
#include <Renderer/Pipelines/ComputePipeline.h>

#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Index.h>
#include <Renderer/FormatInfo.h>
#include <Framegraph/SubresourceStates.inl>

#include "Devices/LogicalDevice.h"
#include "Jobs/QueueSubmissionJob.h"
#include "Renderer/Renderer.h"

#include <Common/Math/Vector3.h>
#include <Common/Memory/Containers/Array.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>
#include <Renderer/WebGPU/Includes.h>
#include "WebGPU/ConvertFormat.h"
#include "WebGPU/ConvertImageAspectFlags.h"

namespace ngine::Rendering
{
	CommandBuffer::CommandBuffer(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const CommandPoolView commandPool,
		[[maybe_unused]] const CommandQueueView commandQueue
	)
	{
#if RENDERER_HAS_COMMAND_POOL
		commandPool.AllocateCommandBuffers(logicalDevice, ArrayView<CommandBuffer, uint16>(this));
#elif RENDERER_METAL
		m_pCommandBuffer = [(id<MTLCommandQueue>)commandQueue commandBuffer];
#elif RENDERER_WEBGPU
		m_isValid = true;
#endif
	}

	CommandBuffer& CommandBuffer::operator=([[maybe_unused]] CommandBuffer&& other) noexcept
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN || RENDERER_METAL
		m_pCommandBuffer = other.m_pCommandBuffer;
		other.m_pCommandBuffer = nullptr;
#elif RENDERER_WEBGPU
		m_isValid = other.m_isValid;
		other.m_isValid = false;
#endif
		return *this;
	}

	CommandBuffer::~CommandBuffer()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void CommandBuffer::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice, [[maybe_unused]] const CommandPoolView commandPool)
	{
#if RENDERER_HAS_COMMAND_POOL
		commandPool.FreeCommandBuffers(logicalDevice, ArrayView<CommandBuffer, uint16>(this));
#elif RENDERER_METAL
		m_pCommandBuffer = nullptr;
#elif RENDERER_WEBGPU
		m_isValid = false;
#endif
	}

	CommandEncoder
	CommandBufferView::BeginEncoding([[maybe_unused]] const LogicalDeviceView logicalDevice, [[maybe_unused]] const Flags flags) const
	{
#if RENDERER_VULKAN
		const VkCommandBufferBeginInfo beginInfo =
			{VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO, nullptr, static_cast<VkCommandBufferUsageFlags>(flags), nullptr};

		const VkResult result = vkBeginCommandBuffer(m_pCommandBuffer, &beginInfo);
		Assert(result == VK_SUCCESS);
		if (LIKELY(result == VK_SUCCESS))
		{
			return CommandEncoder{m_pCommandBuffer};
		}
		else
		{
			return {};
		}
#elif RENDERER_METAL
		return CommandEncoder{m_pCommandBuffer};
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		WGPUCommandEncoder* pEncoder = new WGPUCommandEncoder{nullptr};
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[pEncoder, logicalDevice]()
			{
				WGPUCommandEncoder pWGPUCommandEncoder = wgpuDeviceCreateCommandEncoder(logicalDevice, nullptr);
#if RENDERER_WEBGPU_DAWN
				wgpuCommandEncoderAddRef(pWGPUCommandEncoder);
#else
				wgpuCommandEncoderReference(pWGPUCommandEncoder);
#endif
				*pEncoder = pWGPUCommandEncoder;
			}
		);
		return CommandEncoder{pEncoder};
#else
		WGPUCommandEncoder pEncoder = wgpuDeviceCreateCommandEncoder(logicalDevice, nullptr);
#if RENDERER_WEBGPU_DAWN
		wgpuCommandEncoderAddRef(pEncoder);
#else
		wgpuCommandEncoderReference(pEncoder);
#endif
		return CommandEncoder{pEncoder};
#endif
#else
		UNUSED(flags);
		Assert(false, "TODO");
		return CommandEncoder{};
#endif
	}

	CommandEncoder::~CommandEncoder()
	{
#if RENDERER_WEBGPU
		if (m_pCommandEncoder != nullptr)
		{
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pCommandEncoder = m_pCommandEncoder]()
				{
					wgpuCommandEncoderRelease(*pCommandEncoder);
					delete pCommandEncoder;
				}
			);
#else
			wgpuCommandEncoderRelease(m_pCommandEncoder);
#endif
		}
#endif
	}

	CommandEncoder& CommandEncoder::operator=(CommandEncoder&& other) noexcept
	{
#if RENDERER_VULKAN || RENDERER_METAL
		m_pCommandEncoder = other.m_pCommandEncoder;
		other.m_pCommandEncoder = nullptr;
#elif RENDERER_WEBGPU
		if (m_pCommandEncoder != nullptr)
		{
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pCommandEncoder = m_pCommandEncoder]()
				{
					WGPUCommandEncoder pWGPUCommandEncoder = *pCommandEncoder;
					if (pWGPUCommandEncoder != nullptr)
					{
						wgpuCommandEncoderRelease(pWGPUCommandEncoder);
					}
					delete pCommandEncoder;
				}
			);
#else
			wgpuCommandEncoderRelease(m_pCommandEncoder);
#endif
			m_pCommandEncoder = nullptr;
		}
		m_pCommandEncoder = other.m_pCommandEncoder;
		other.m_pCommandEncoder = nullptr;
#endif
		return *this;
	}

	EncodedCommandBuffer CommandEncoder::StopEncoding()
	{
#if RENDERER_VULKAN
		if (m_pCommandEncoder != nullptr)
		{
			const VkResult result = vkEndCommandBuffer(m_pCommandEncoder);
			Assert(result == VK_SUCCESS);
			if (LIKELY(result == VK_SUCCESS))
			{
				EncodedCommandBuffer encodedCommandBuffer{m_pCommandEncoder};
				m_pCommandEncoder = nullptr;
				return Move(encodedCommandBuffer);
			}
			else
			{
				m_pCommandEncoder = nullptr;
			}
		}
#elif RENDERER_METAL
		if (m_pCommandEncoder != nullptr)
		{
			id<MTLCommandBuffer> commandBuffer = m_pCommandEncoder;
			m_pCommandEncoder = nullptr;
			return EncodedCommandBuffer{commandBuffer};
		}
#elif RENDERER_WEBGPU
		if (m_pCommandEncoder != nullptr)
		{
#if WEBGPU_SINGLE_THREADED
			WGPUCommandBuffer* pCommandBuffer = new WGPUCommandBuffer{nullptr};
			Rendering::Window::ExecuteImmediatelyOnWindowThread(
				[pCommandEncoder = m_pCommandEncoder, pCommandBuffer]()
				{
					WGPUCommandEncoder pWGPUCommandEncoder = *pCommandEncoder;
					if (pWGPUCommandEncoder != nullptr)
					{
						WGPUCommandBuffer pWGPUCommandBuffer = wgpuCommandEncoderFinish(pWGPUCommandEncoder, nullptr);
						Assert(pWGPUCommandBuffer != nullptr);
						if (LIKELY(pWGPUCommandBuffer != nullptr))
						{
#if RENDERER_WEBGPU_DAWN
							wgpuCommandBufferAddRef(pWGPUCommandBuffer);
#else
							wgpuCommandBufferReference(pWGPUCommandBuffer);
#endif
							*pCommandBuffer = pWGPUCommandBuffer;
						}
						wgpuCommandEncoderRelease(pWGPUCommandEncoder);
					}
					delete pCommandEncoder;
				}
			);
			m_pCommandEncoder = nullptr;
			return EncodedCommandBuffer{pCommandBuffer};
#else
			EncodedCommandBuffer encodedCommandBuffer;
			WGPUCommandBuffer pCommandBuffer = wgpuCommandEncoderFinish(m_pCommandEncoder, nullptr);
			Assert(pCommandBuffer != nullptr);
			if (LIKELY(pCommandBuffer != nullptr))
			{
				encodedCommandBuffer = EncodedCommandBuffer{pCommandBuffer};
#if RENDERER_WEBGPU_DAWN
				wgpuCommandBufferAddRef(pCommandBuffer);
#else
				wgpuCommandBufferReference(pCommandBuffer);
#endif
				wgpuCommandEncoderRelease(m_pCommandEncoder);
			}
			m_pCommandEncoder = nullptr;
			return Move(encodedCommandBuffer);
#endif
		}

#else
		Assert(false, "TODO");
#endif
		return EncodedCommandBuffer{};
	}

	void CommandEncoderView::SetDebugName([[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name) const
	{
#if RENDERER_VULKAN
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_COMMAND_BUFFER,
			reinterpret_cast<uint64_t>(m_pCommandEncoder),
			name
		};

#if PLATFORM_APPLE
		vkSetDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT =
			reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(logicalDevice.GetSetDebugUtilsObjectNameEXT());
		if (setDebugUtilsObjectNameEXT != nullptr)
		{
			setDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder setLabel:[NSString stringWithUTF8String:name]];
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, name]()
			{
#if RENDERER_WEBGPU_DAWN
				wgpuCommandEncoderSetLabel(*pCommandEncoder, WGPUStringView{name, name.GetSize()});
#else
				wgpuCommandEncoderSetLabel(*pCommandEncoder, name);
#endif
			}
		);
#else
		wgpuCommandEncoderSetLabel(m_pCommandEncoder, name);
#endif
#else
		UNUSED(logicalDevice);
		UNUSED(name);
#endif
	}

	void CommandEncoderView::BeginDebugMarker(
		[[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView label, [[maybe_unused]] const Math::Color color
	) const
	{
#if RENDERER_VULKAN
		VkDebugUtilsLabelEXT labelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, label, {color.r, color.g, color.b, color.a}};

#if PLATFORM_APPLE
		vkCmdBeginDebugUtilsLabelEXT(m_pCommandEncoder, &labelInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkCmdBeginDebugUtilsLabelEXT beginDebugUtilsLabelEXT =
			reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(logicalDevice.GetCmdBeginDebugUtilsLabelEXT());
		if (beginDebugUtilsLabelEXT != nullptr)
		{
			beginDebugUtilsLabelEXT(m_pCommandEncoder, &labelInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder pushDebugGroup:(NSString* _Nonnull)[NSString stringWithUTF8String:label]];

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, label]()
			{
				WGPUCommandEncoder pWGPUCommandEncoder = *pCommandEncoder;
#if RENDERER_WEBGPU_DAWN
				wgpuCommandEncoderPushDebugGroup(pWGPUCommandEncoder, WGPUStringView{label, label.GetSize()});
				wgpuCommandEncoderInsertDebugMarker(pWGPUCommandEncoder, WGPUStringView{label, label.GetSize()});
#else
				wgpuCommandEncoderPushDebugGroup(pWGPUCommandEncoder, label);
				wgpuCommandEncoderInsertDebugMarker(pWGPUCommandEncoder, label);
#endif
			}
		);
#else
		wgpuCommandEncoderPushDebugGroup(m_pCommandEncoder, label);
		wgpuCommandEncoderInsertDebugMarker(m_pCommandEncoder, label);
#endif
#else
		UNUSED(logicalDevice);
		UNUSED(label);
#endif
	}

	void CommandEncoderView::EndDebugMarker([[maybe_unused]] const LogicalDevice& logicalDevice) const
	{
#if RENDERER_VULKAN && PROFILE_BUILD

#if PLATFORM_APPLE
		vkCmdEndDebugUtilsLabelEXT(m_pCommandEncoder);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkCmdEndDebugUtilsLabelEXT endDebugUtilsLabelEXT =
			reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(logicalDevice.GetCmdEndDebugUtilsLabelEXT());
		if (endDebugUtilsLabelEXT != nullptr)
		{
			endDebugUtilsLabelEXT(m_pCommandEncoder);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder popDebugGroup];

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder]()
			{
				wgpuCommandEncoderPopDebugGroup(*pCommandEncoder);
			}
		);
#else
		wgpuCommandEncoderPopDebugGroup(m_pCommandEncoder);
#endif
#endif
	}

	void CommandEncoderView::RecordPipelineBarrier(
		const EnumFlags<PipelineStageFlags> sourceStage,
		const EnumFlags<PipelineStageFlags> targetStage,
		const ArrayView<const ImageMemoryBarrier> imageBarriers,
		const ArrayView<const BufferMemoryBarrier> bufferBarriers
	) const
	{
		Assert(imageBarriers.All(
			[](const ImageMemoryBarrier barrier)
			{
				return barrier.m_image.IsValid();
			}
		));
		Assert(bufferBarriers.All(
			[](const BufferMemoryBarrier barrier)
			{
				return barrier.m_buffer.IsValid();
			}
		));

#if RENDERER_VULKAN && !PLATFORM_APPLE
		static_assert(sizeof(ImageMemoryBarrier) == sizeof(VkImageMemoryBarrier));
		static_assert(alignof(ImageMemoryBarrier) == alignof(VkImageMemoryBarrier));
		static_assert(sizeof(BufferMemoryBarrier) == sizeof(VkBufferMemoryBarrier));
		static_assert(alignof(BufferMemoryBarrier) == alignof(VkBufferMemoryBarrier));

		vkCmdPipelineBarrier(
			m_pCommandEncoder,
			static_cast<VkPipelineStageFlags>(sourceStage.GetUnderlyingValue()),
			static_cast<VkPipelineStageFlags>(targetStage.GetUnderlyingValue()),
			0,
			0,
			nullptr,
			bufferBarriers.GetSize(),
			reinterpret_cast<const VkBufferMemoryBarrier*>(bufferBarriers.GetData()),
			imageBarriers.GetSize(),
			reinterpret_cast<const VkImageMemoryBarrier*>(imageBarriers.GetData())
		);
#elif RENDERER_METAL
		// TODO: synchronizeResource and synchronizeTexture on blit encoders if needed
		UNUSED(sourceStage);
		UNUSED(targetStage);
		UNUSED(imageBarriers);
		UNUSED(bufferBarriers);
#elif RENDERER_WEBGPU
		// No barriers
		UNUSED(sourceStage);
		UNUSED(targetStage);
		UNUSED(imageBarriers);
		UNUSED(bufferBarriers);
#else
		UNUSED(sourceStage);
		UNUSED(targetStage);
		UNUSED(imageBarriers);
		UNUSED(bufferBarriers);
#endif
	}

#if RENDERER_METAL
	[[nodiscard]] MTLLoadAction ConvertLoadType(const AttachmentLoadType loadType)
	{
		switch (loadType)
		{
			case AttachmentLoadType::LoadExisting:
				return MTLLoadActionLoad;
			case AttachmentLoadType::Clear:
				return MTLLoadActionClear;
			case AttachmentLoadType::Undefined:
				return MTLLoadActionDontCare;
		}
		ExpectUnreachable();
	}
	[[nodiscard]] MTLStoreAction ConvertStoreType(const AttachmentStoreType loadType)
	{
		switch (loadType)
		{
			case AttachmentStoreType::Store:
				return MTLStoreActionStore;
			case AttachmentStoreType::Undefined:
				return MTLStoreActionDontCare;
		}
		ExpectUnreachable();
	}
#elif RENDERER_WEBGPU
	[[nodiscard]] WGPULoadOp ConvertLoadType(const AttachmentLoadType loadType, const WGPULoadOp undefinedOperation = WGPULoadOp_Undefined)
	{
		switch (loadType)
		{
			case AttachmentLoadType::LoadExisting:
				return WGPULoadOp_Load;
			case AttachmentLoadType::Clear:
				return WGPULoadOp_Clear;
			case AttachmentLoadType::Undefined:
				return undefinedOperation;
		}
		ExpectUnreachable();
	}
	[[nodiscard]] WGPUStoreOp
	ConvertStoreType(const AttachmentStoreType loadType, const WGPUStoreOp undefinedOperation = WGPUStoreOp_Undefined)
	{
		switch (loadType)
		{
			case AttachmentStoreType::Store:
				return WGPUStoreOp_Store;
			case AttachmentStoreType::Undefined:
				return undefinedOperation;
		}
		ExpectUnreachable();
	}
#endif

#if RENDERER_METAL || RENDERER_WEBGPU
	[[nodiscard]] AttachmentStoreType GetNextSubpassAttachmentStoreType(
		AttachmentStoreType attachmentStoreType,
		const uint8 currentSubpassIndex,
		const uint8 subpassCount,
		const uint8 attachmentIndex,
		const ImageAspectFlags aspectFlags,
		const Internal::RenderPassData& __restrict renderPassData
	)
	{
		if (attachmentStoreType == AttachmentStoreType::Undefined)
		{
			// Check if this attachment is read in any following subpasses
			for (uint8 otherSubpassIndex = currentSubpassIndex + 1; otherSubpassIndex < subpassCount; ++otherSubpassIndex)
			{
				const ArrayView<const uint8> otherSubpassInputAttachmentIndices = renderPassData.m_subpassInputAttachmentIndices[otherSubpassIndex];
				const bool isSampleable = (aspectFlags == ImageAspectFlags::Color) | (aspectFlags == ImageAspectFlags::Depth);
				const bool isSampledInSubsequentPass = otherSubpassInputAttachmentIndices.Contains(attachmentIndex) & isSampleable;
				const bool isDepthOrStencilAttachmentInSubsequentPass = renderPassData.m_subpassDepthAttachmentIndices[otherSubpassIndex] ==
				                                                        attachmentIndex;
				if (isSampledInSubsequentPass | isDepthOrStencilAttachmentInSubsequentPass)
				{
					attachmentStoreType = AttachmentStoreType::Store;
				}
			}
		}
		return attachmentStoreType;
	}
#endif

	RenderCommandEncoder CommandEncoderView::BeginRenderPass(
		[[maybe_unused]] LogicalDevice& logicalDevice,
		const RenderPassView renderPass,
		const FramebufferView framebuffer,
		[[maybe_unused]] const Math::Rectangleui extent,
		const ArrayView<const ClearValue, uint8> clearValues,
		[[maybe_unused]] const uint32 maximumPushConstantInstanceCount
	) const
	{
		Assert(framebuffer.IsValid());

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		if (maximumPushConstantInstanceCount > 0)
		{
			logicalDevice.ReservePushConstantsInstanceCount(maximumPushConstantInstanceCount);
		}
#endif

#if RENDERER_VULKAN
		static_assert(sizeof(VkClearValue) == sizeof(ClearValue));
		static_assert(alignof(VkClearValue) == alignof(ClearValue));

		const VkRenderPassBeginInfo renderPassInfo = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			nullptr,
			renderPass,
			framebuffer,
			VkRect2D{
				VkOffset2D{static_cast<int32>(extent.GetPosition().x), static_cast<int32>(extent.GetPosition().y)},
				VkExtent2D{extent.GetSize().x, extent.GetSize().y}
			},
			clearValues.GetSize(),
			reinterpret_cast<const VkClearValue*>(clearValues.GetData())
		};

		vkCmdBeginRenderPass(m_pCommandEncoder, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
		return RenderCommandEncoder{m_pCommandEncoder};
#elif RENDERER_METAL
		MTLRenderPassDescriptor* renderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];

		const Internal::RenderPassData* __restrict pRenderPassData{renderPass};
		const Internal::FramebufferData* __restrict pFramebufferData{framebuffer};
		const ArrayView<const ImageMappingView, uint8> attachmentMappings = pFramebufferData->m_attachmentMappings;

		const uint8 subpassIndex = 0;
		const uint8 subpassCount = pRenderPassData->m_subpassColorAttachmentIndices.GetSize();
		const ArrayView<const uint8> firstSubpassColorAttachmentIndices = pRenderPassData->m_subpassColorAttachmentIndices[subpassIndex];
		for (const uint8 attachmentIndex : firstSubpassColorAttachmentIndices)
		{
			const AttachmentDescription& __restrict attachmentDescription = pRenderPassData->m_attachmentDescriptions[attachmentIndex];
			const EnumFlags<FormatFlags> attachmentFormatFlags = GetFormatInfo(attachmentDescription.m_format).m_flags;

			const ImageMappingView imageMapping = attachmentMappings[attachmentIndex];
			id<MTLTexture> metalTexture = imageMapping;
			Assert(metalTexture != nil);

			const Math::Color clearValue = clearValues[attachmentIndex];

			MTLRenderPassColorAttachmentDescriptor* colorAttachmentDescriptor = [[MTLRenderPassColorAttachmentDescriptor alloc] init];

			colorAttachmentDescriptor.texture = metalTexture;

			colorAttachmentDescriptor.loadAction = ConvertLoadType(attachmentDescription.m_loadType);
			colorAttachmentDescriptor.storeAction = ConvertStoreType(GetNextSubpassAttachmentStoreType(
				attachmentDescription.m_storeType,
				subpassIndex,
				subpassCount,
				attachmentIndex,
				ImageAspectFlags::Color,
				*pRenderPassData
			));

			colorAttachmentDescriptor.clearColor = MTLClearColorMake(clearValue.r, clearValue.g, clearValue.b, clearValue.a);

			renderPassDescriptor.colorAttachments[attachmentIndex] = colorAttachmentDescriptor;
		}

		const uint8 depthAttachmentIndex = pRenderPassData->m_subpassDepthAttachmentIndices[subpassIndex];
		if (depthAttachmentIndex != Internal::RenderPassData::InvalidAttachmentIndex)
		{
			const AttachmentDescription& __restrict attachmentDescription = pRenderPassData->m_attachmentDescriptions[depthAttachmentIndex];
			const EnumFlags<FormatFlags> attachmentFormatFlags = GetFormatInfo(attachmentDescription.m_format).m_flags;

			const ImageMappingView imageMapping = attachmentMappings[depthAttachmentIndex];
			id<MTLTexture> metalTexture = imageMapping;

			const DepthStencilValue clearValue = clearValues[depthAttachmentIndex];
			if (attachmentFormatFlags.IsSet(FormatFlags::Depth))
			{
				MTLRenderPassDepthAttachmentDescriptor* depthAttachmentDescriptor = [[MTLRenderPassDepthAttachmentDescriptor alloc] init];
				depthAttachmentDescriptor.texture = metalTexture;
				depthAttachmentDescriptor.loadAction = ConvertLoadType(attachmentDescription.m_loadType);
				depthAttachmentDescriptor.storeAction = ConvertStoreType(GetNextSubpassAttachmentStoreType(
					attachmentDescription.m_storeType,
					subpassIndex,
					subpassCount,
					depthAttachmentIndex,
					ImageAspectFlags::Depth,
					*pRenderPassData
				));
				depthAttachmentDescriptor.clearDepth = clearValue.m_depth.m_value;
				renderPassDescriptor.depthAttachment = depthAttachmentDescriptor;
			}
			if (attachmentFormatFlags.IsSet(FormatFlags::Stencil))
			{
				MTLRenderPassStencilAttachmentDescriptor* stencilAttachmentDescriptor = [[MTLRenderPassStencilAttachmentDescriptor alloc] init];
				stencilAttachmentDescriptor.texture = metalTexture;
				stencilAttachmentDescriptor.loadAction = ConvertLoadType(attachmentDescription.m_stencilLoadType);
				stencilAttachmentDescriptor.storeAction = ConvertStoreType(GetNextSubpassAttachmentStoreType(
					attachmentDescription.m_stencilStoreType,
					subpassIndex,
					subpassCount,
					depthAttachmentIndex,
					ImageAspectFlags::Stencil,
					*pRenderPassData
				));
				stencilAttachmentDescriptor.clearStencil = clearValue.m_stencil.m_value;
				renderPassDescriptor.stencilAttachment = stencilAttachmentDescriptor;
			}
		}

		return RenderCommandEncoder{
			[m_pCommandEncoder renderCommandEncoderWithDescriptor:renderPassDescriptor],
			*this,
			renderPass,
			framebuffer,
			0
		};
#elif RENDERER_WEBGPU
		const Internal::RenderPassData* __restrict pRenderPassData{renderPass};
		const Internal::FramebufferData* __restrict pFramebufferData{framebuffer};
		const ArrayView<const ImageMappingView, uint8> attachmentMappings = pFramebufferData->m_attachmentMappings;

		const uint8 subpassIndex = 0;
		const uint8 subpassCount = pRenderPassData->m_subpassColorAttachmentIndices.GetSize();
		Assert(subpassIndex < subpassCount);
		const ArrayView<const uint8> firstSubpassColorAttachmentIndices = pRenderPassData->m_subpassColorAttachmentIndices[subpassIndex];

		FixedCapacityInlineVector<WGPURenderPassColorAttachment, 8>
			colorAttachments(Memory::ConstructWithSize, Memory::DefaultConstruct, firstSubpassColorAttachmentIndices.GetSize());
		WGPURenderPassDepthStencilAttachment depthStencilAttachment{nullptr};

		uint8 localAttachmentIndex = 0;
		for (const uint8 attachmentIndex : firstSubpassColorAttachmentIndices)
		{
			const AttachmentDescription& __restrict attachmentDescription = pRenderPassData->m_attachmentDescriptions[attachmentIndex];
			const EnumFlags<FormatFlags> attachmentFormatFlags = GetFormatInfo(attachmentDescription.m_format).m_flags;

			const ImageMappingView imageMapping = attachmentMappings[attachmentIndex];

			const Math::Color clearValue = clearValues[attachmentIndex];
			const AttachmentStoreType attachmentStoreType = GetNextSubpassAttachmentStoreType(
				attachmentDescription.m_storeType,
				subpassIndex,
				subpassCount,
				attachmentIndex,
				ImageAspectFlags::Color,
				*pRenderPassData
			);

			colorAttachments[localAttachmentIndex] = WGPURenderPassColorAttachment{
				nullptr,
				imageMapping,
				WGPU_DEPTH_SLICE_UNDEFINED,
				nullptr, // resolve target mapping
				ConvertLoadType(attachmentDescription.m_loadType, WGPULoadOp_Clear),
				ConvertStoreType(attachmentStoreType, WGPUStoreOp_Discard),
				WGPUColor{clearValue.r, clearValue.g, clearValue.b, clearValue.a}
			};
			localAttachmentIndex++;
		}

		const uint8 depthAttachmentIndex = pRenderPassData->m_subpassDepthAttachmentIndices[subpassIndex];
		if (depthAttachmentIndex != Internal::RenderPassData::InvalidAttachmentIndex)
		{
			const AttachmentDescription& __restrict attachmentDescription = pRenderPassData->m_attachmentDescriptions[depthAttachmentIndex];
			const EnumFlags<FormatFlags> attachmentFormatFlags = GetFormatInfo(attachmentDescription.m_format).m_flags;

			const ImageMappingView imageMapping = attachmentMappings[depthAttachmentIndex];

			const DepthStencilValue clearValue = clearValues[depthAttachmentIndex];

			bool isDepthReadOnly = (attachmentDescription.m_initialLayout == ImageLayout::DepthStencilReadOnlyOptimal) |
			                       (attachmentDescription.m_initialLayout == ImageLayout::DepthReadOnlyOptimal) |
			                       (attachmentDescription.m_initialLayout == ImageLayout::DepthReadOnlyStencilAttachmentOptimal);
			bool isStencilReadOnly = (attachmentDescription.m_initialLayout == ImageLayout::DepthStencilReadOnlyOptimal) |
			                         (attachmentDescription.m_initialLayout == ImageLayout::DepthAttachmentStencilReadOnlyOptimal) |
			                         (attachmentDescription.m_initialLayout == ImageLayout::StencilReadOnlyOptimal);

			// Read only state must match both depth & stencil for combined depth stencils
			const bool isCombinedDepthStencil = attachmentFormatFlags.AreAllSet(FormatFlags::DepthStencil);
			if (isCombinedDepthStencil)
			{
				isDepthReadOnly = isStencilReadOnly = isDepthReadOnly && isStencilReadOnly;
			}

			const AttachmentStoreType depthStoreType = GetNextSubpassAttachmentStoreType(
				attachmentDescription.m_storeType,
				subpassIndex,
				subpassCount,
				depthAttachmentIndex,
				ImageAspectFlags::Depth,
				*pRenderPassData
			);
			const AttachmentStoreType stencilStoreType = GetNextSubpassAttachmentStoreType(
				attachmentDescription.m_stencilStoreType,
				subpassIndex,
				subpassCount,
				depthAttachmentIndex,
				ImageAspectFlags::Stencil,
				*pRenderPassData
			);

			depthStencilAttachment = WGPURenderPassDepthStencilAttachment{
				imageMapping,
				ConvertLoadType(attachmentDescription.m_loadType),
				ConvertStoreType(depthStoreType),
				clearValue.m_depth.m_value,
				isDepthReadOnly,
				ConvertLoadType(attachmentDescription.m_stencilLoadType),
				ConvertStoreType(stencilStoreType),
				clearValue.m_stencil.m_value,
				isStencilReadOnly
			};

			if (attachmentFormatFlags.IsSet(FormatFlags::Depth) && !isDepthReadOnly)
			{
				if (depthStencilAttachment.depthLoadOp == WGPULoadOp_Undefined)
				{
					depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
				}
				if (depthStencilAttachment.depthStoreOp == WGPUStoreOp_Undefined)
				{
					depthStencilAttachment.depthStoreOp = WGPUStoreOp_Discard;
				}
			}
			if (attachmentFormatFlags.IsSet(FormatFlags::Stencil) && !isStencilReadOnly)
			{
				if (depthStencilAttachment.stencilLoadOp == WGPULoadOp_Undefined)
				{
					depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
				}
				if (depthStencilAttachment.stencilStoreOp == WGPUStoreOp_Undefined)
				{
					depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Discard;
				}
			}
		}

#if WEBGPU_SINGLE_THREADED
		WGPURenderPassEncoder* pRenderPassEncoder = new WGPURenderPassEncoder{nullptr};
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, extent, colorAttachments = Move(colorAttachments), depthStencilAttachment, pRenderPassEncoder]()
			{
				const WGPURenderPassDescriptor renderPassDescriptor
				{
					nullptr,
#if RENDERER_WEBGPU_DAWN
						WGPUStringView{nullptr, 0},
#else
						nullptr,
#endif
						colorAttachments.GetSize(), colorAttachments.GetData(),
						depthStencilAttachment.view != nullptr ? &depthStencilAttachment : nullptr, nullptr, nullptr
				};
				WGPURenderPassEncoder pWGPURenderPassEncoder = wgpuCommandEncoderBeginRenderPass(*pCommandEncoder, &renderPassDescriptor);
#if RENDERER_WEBGPU_DAWN
				wgpuRenderPassEncoderAddRef(pWGPURenderPassEncoder);
#else
				wgpuRenderPassEncoderReference(pWGPURenderPassEncoder);
#endif

				wgpuRenderPassEncoderSetViewport(
					pWGPURenderPassEncoder,
					(float)extent.GetPosition().x,
					(float)extent.GetPosition().y,
					(float)extent.GetSize().x,
					(float)extent.GetSize().y,
					0.f,
					1.f
				);
				*pRenderPassEncoder = pWGPURenderPassEncoder;
			}
		);
		return RenderCommandEncoder{pRenderPassEncoder, *this, renderPass, framebuffer, 0};
#else
		const WGPURenderPassDescriptor renderPassDescriptor{
			nullptr,
			nullptr,
			colorAttachments.GetSize(),
			colorAttachments.GetData(),
			depthStencilAttachment.view != nullptr ? &depthStencilAttachment : nullptr,
			nullptr,
			nullptr
		};
		WGPURenderPassEncoder pRenderPassCommandEncoder = wgpuCommandEncoderBeginRenderPass(m_pCommandEncoder, &renderPassDescriptor);
#if RENDERER_WEBGPU_DAWN
		wgpuRenderPassEncoderAddRef(pRenderPassCommandEncoder);
#else
		wgpuRenderPassEncoderReference(pRenderPassCommandEncoder);
#endif

		RenderCommandEncoder renderCommandEncoder{pRenderPassCommandEncoder, *this, renderPass, framebuffer, 0};
		renderCommandEncoder.SetViewport(extent);
		return renderCommandEncoder;
#endif
#else
		Assert(false, "TODO");
		UNUSED(commandEncoder);
		UNUSED(frameBuffer);
		UNUSED(extent);
		UNUSED(clearValues);
#endif
	}

	RenderCommandEncoder::~RenderCommandEncoder()
	{
#if RENDERER_VULKAN
		if (m_pCommandEncoder != 0)
		{
			vkCmdEndRenderPass(m_pCommandEncoder);
		}
#elif RENDERER_METAL
		if (m_pCommandEncoder != nil)
		{
			[m_pCommandEncoder endEncoding];
			m_pCommandEncoder = nil;
		}
#elif RENDERER_WEBGPU
		if (m_pCommandEncoder != nullptr)
		{
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pCommandEncoder = m_pCommandEncoder]()
				{
					WGPURenderPassEncoder pRenderPassEncoder = *pCommandEncoder;
					if (pRenderPassEncoder != nullptr)
					{
						wgpuRenderPassEncoderEnd(pRenderPassEncoder);
						wgpuRenderPassEncoderRelease(pRenderPassEncoder);
					}
					delete pCommandEncoder;
				}
			);
#else
			wgpuRenderPassEncoderEnd(m_pCommandEncoder);
			wgpuRenderPassEncoderRelease(m_pCommandEncoder);
#endif
		}
#endif
	}

	RenderCommandEncoder& RenderCommandEncoder::operator=(RenderCommandEncoder&& other) noexcept
	{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
		Assert(m_pCommandEncoder == 0);
		m_pCommandEncoder = other.m_pCommandEncoder;
		other.m_pCommandEncoder = 0;
#endif

#if RENDERER_METAL || RENDERER_WEBGPU
		m_commandEncoder = other.m_commandEncoder;
		m_renderPass = other.m_renderPass;
		m_framebuffer = other.m_framebuffer;
		m_currentSubpassIndex = other.m_currentSubpassIndex;
#endif
		return *this;
	}

	void RenderCommandEncoder::End()
	{
#if RENDERER_VULKAN
		vkCmdEndRenderPass(m_pCommandEncoder);
		m_pCommandEncoder = 0;
#elif RENDERER_METAL
		[m_pCommandEncoder endEncoding];
		m_pCommandEncoder = nil;
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder]()
			{
				WGPURenderPassEncoder pRenderPassEncoder = *pCommandEncoder;
				if (pRenderPassEncoder != nullptr)
				{
					wgpuRenderPassEncoderEnd(pRenderPassEncoder);
					wgpuRenderPassEncoderRelease(pRenderPassEncoder);
				}
				delete pCommandEncoder;
			}
		);
#else
		wgpuRenderPassEncoderEnd(m_pCommandEncoder);
		wgpuRenderPassEncoderRelease(m_pCommandEncoder);
#endif
		m_pCommandEncoder = nullptr;
#endif
	}

	void RenderCommandEncoderView::SetViewport(const Math::Rectangleui extent) const
	{
#if RENDERER_VULKAN
		VkViewport viewport =
			{(float)extent.GetPosition().x, (float)extent.GetPosition().y, (float)extent.GetSize().x, (float)extent.GetSize().y, 0.f, 1.f};
		vkCmdSetViewport(m_pCommandEncoder, 0, 1, &viewport);
#elif RENDERER_METAL
		[m_pCommandEncoder setViewport:MTLViewport{
																		 (float)extent.GetPosition().x,
																		 (float)extent.GetPosition().y,
																		 (float)extent.GetSize().x,
																		 (float)extent.GetSize().y,
																		 0.f,
																		 1.f
																	 }];
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, extent]()
			{
				wgpuRenderPassEncoderSetViewport(
					*pCommandEncoder,
					(float)extent.GetPosition().x,
					(float)extent.GetPosition().y,
					(float)extent.GetSize().x,
					(float)extent.GetSize().y,
					0.f,
					1.f
				);
			}
		);
#else
		wgpuRenderPassEncoderSetViewport(
			m_pCommandEncoder,
			(float)extent.GetPosition().x,
			(float)extent.GetPosition().y,
			(float)extent.GetSize().x,
			(float)extent.GetSize().y,
			0.f,
			1.f
		);
#endif

#else
		Assert(false, "TODO");
		UNUSED(extent);
#endif
	}

	void RenderCommandEncoderView::SetDebugName(
		[[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name
	) const
	{
#if RENDERER_VULKAN && PROFILE_BUILD
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_COMMAND_BUFFER,
			reinterpret_cast<uint64_t>(m_pCommandEncoder),
			name
		};

#if PLATFORM_APPLE
		vkSetDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT =
			reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(logicalDevice.GetSetDebugUtilsObjectNameEXT());
		if (setDebugUtilsObjectNameEXT != nullptr)
		{
			setDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder setLabel:[NSString stringWithUTF8String:name]];

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, name]()
			{
#if RENDERER_WEBGPU_DAWN
				wgpuRenderPassEncoderSetLabel(*pCommandEncoder, WGPUStringView{name, name.GetSize()});
#else
				wgpuRenderPassEncoderSetLabel(*pCommandEncoder, name);
#endif
			}
		);
#else
		wgpuRenderPassEncoderSetLabel(m_pCommandEncoder, name);
#endif
#else
		UNUSED(logicalDevice);
		UNUSED(name);
#endif
	}

	void RenderCommandEncoderView::BeginDebugMarker(
		[[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView label, [[maybe_unused]] const Math::Color color
	) const
	{
#if RENDERER_VULKAN && PROFILE_BUILD
		VkDebugUtilsLabelEXT labelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, label, {color.r, color.g, color.b, color.a}};

#if PLATFORM_APPLE
		vkCmdBeginDebugUtilsLabelEXT(m_pCommandEncoder, &labelInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkCmdBeginDebugUtilsLabelEXT beginDebugUtilsLabelEXT =
			reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(logicalDevice.GetCmdBeginDebugUtilsLabelEXT());
		if (beginDebugUtilsLabelEXT != nullptr)
		{
			beginDebugUtilsLabelEXT(m_pCommandEncoder, &labelInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder pushDebugGroup:(NSString* _Nonnull)[NSString stringWithUTF8String:label]];

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, label]()
			{
				WGPURenderPassEncoder pRenderPassEncoder = *pCommandEncoder;
#if RENDERER_WEBGPU_DAWN
				wgpuRenderPassEncoderPushDebugGroup(pRenderPassEncoder, WGPUStringView{label, label.GetSize()});
				wgpuRenderPassEncoderInsertDebugMarker(pRenderPassEncoder, WGPUStringView{label, label.GetSize()});
#else
				wgpuRenderPassEncoderPushDebugGroup(pRenderPassEncoder, label);
				wgpuRenderPassEncoderInsertDebugMarker(pRenderPassEncoder, label);
#endif
			}
		);
#else
		wgpuRenderPassEncoderPushDebugGroup(m_pCommandEncoder, label);
		wgpuRenderPassEncoderInsertDebugMarker(m_pCommandEncoder, label);
#endif
#else
		UNUSED(logicalDevice);
		UNUSED(label);
#endif
	}

	void RenderCommandEncoderView::EndDebugMarker([[maybe_unused]] const LogicalDevice& logicalDevice) const
	{

#if RENDERER_VULKAN && PROFILE_BUILD

#if PLATFORM_APPLE
		vkCmdEndDebugUtilsLabelEXT(m_pCommandEncoder);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkCmdEndDebugUtilsLabelEXT endDebugUtilsLabelEXT =
			reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(logicalDevice.GetCmdEndDebugUtilsLabelEXT());
		if (endDebugUtilsLabelEXT != nullptr)
		{
			endDebugUtilsLabelEXT(m_pCommandEncoder);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder popDebugGroup];

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder]()
			{
				wgpuRenderPassEncoderPopDebugGroup(*pCommandEncoder);
			}
		);
#else
		wgpuRenderPassEncoderPopDebugGroup(m_pCommandEncoder);
#endif
#endif
	}

	void RenderCommandEncoderView::BindVertexBuffers(
		const ArrayView<const BufferView> buffers, const ArrayView<const uint64> offsets, [[maybe_unused]] const ArrayView<const uint64> sizes
	) const
	{
		Assert(buffers.All(
			[](const BufferView buffer)
			{
				return buffer.IsValid();
			}
		));
		Assert(buffers.GetSize() == offsets.GetSize());
		uint32 firstBindingIndex = 0;
#if RENDERER_VULKAN
		vkCmdBindVertexBuffers(
			m_pCommandEncoder,
			firstBindingIndex,
			buffers.GetSize(),
			reinterpret_cast<const VkBuffer*>(buffers.GetData()),
			offsets.GetData()
		);
#elif RENDERER_METAL
		for (const BufferView& buffer : buffers)
		{
			const uint32 index = buffers.GetIteratorIndex(&buffer);
			[m_pCommandEncoder setVertexBuffer:buffer offset:offsets[index] atIndex:firstBindingIndex + index];
		}
#elif RENDERER_WEBGPU
		for (const BufferView& buffer : buffers)
		{
			const uint32 index = buffers.GetIteratorIndex(&buffer);
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pCommandEncoder = m_pCommandEncoder, index = firstBindingIndex + index, buffer, offset = offsets[index], size = sizes[index]]()
				{
					wgpuRenderPassEncoderSetVertexBuffer(*pCommandEncoder, index, buffer, offset, size);
				}
			);
#else
			wgpuRenderPassEncoderSetVertexBuffer(m_pCommandEncoder, firstBindingIndex + index, buffer, offsets[index], sizes[index]);
#endif
		}
#else
		UNUSED(firstBindingIndex);
		UNUSED(buffers);
		UNUSED(offsets);
		UNUSED(sizes);
		Assert(false, "TODO");
#endif
	}

	void RenderCommandEncoderView::BindDescriptorSets(
		[[maybe_unused]] const PipelineLayoutView pipelineLayout, const ArrayView<const DescriptorSetView, uint8> sets, uint32 firstSetIndex
	) const
	{
		Assert(sets.All(
			[](const DescriptorSetView descriptorSet)
			{
				return descriptorSet.IsValid();
			}
		));

#if RENDERER_VULKAN
		vkCmdBindDescriptorSets(
			m_pCommandEncoder,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			firstSetIndex,
			sets.GetSize(),
			reinterpret_cast<const VkDescriptorSet*>(sets.GetData()),
			0,
			nullptr
		);
#elif RENDERER_METAL
		const Internal::PipelineLayoutData* __restrict pPipelineLayoutData = pipelineLayout;

		Array<uint8, (uint8)ShaderStage::Count> descriptorIndices{Memory::Zeroed};
		for (const DescriptorSetLayoutView skippedDescriptorSetLayout :
		     pPipelineLayoutData->m_descriptorSetLayouts.GetSubView(0u, (uint8)firstSetIndex))
		{
			const Internal::DescriptorSetLayoutData* __restrict pDescriptorSetLayoutData = skippedDescriptorSetLayout;

			for (const ShaderStage shaderStage : pDescriptorSetLayoutData->m_stages)
			{
				const uint8 shaderStageIndex = GetShaderStageIndex(shaderStage);
				if (pDescriptorSetLayoutData->m_argumentBuffers)
				{
					descriptorIndices[shaderStageIndex]++;
				}
				else
				{
					for (const Internal::DescriptorSetLayoutData::Binding& __restrict descriptorSetBinding : pDescriptorSetLayoutData->m_bindings)
					{
						if (descriptorSetBinding.m_stages.IsSet(shaderStage))
						{
							descriptorIndices[shaderStageIndex]++;
						}
					}
				}
			}
		}

		for (const DescriptorSetView& descriptorSet : sets)
		{
			const Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
			Assert(pDescriptorSetData != nullptr);
			if (UNLIKELY_ERROR(pDescriptorSetData == nullptr))
			{
				continue;
			}

			const Internal::DescriptorSetLayoutData* __restrict pDescriptorSetLayoutData = pDescriptorSetData->m_layout;

			for (const ShaderStage shaderStage : pDescriptorSetLayoutData->m_stages)
			{
				const uint8 shaderStageIndex = GetShaderStageIndex(shaderStage);

				switch (shaderStage)
				{
					case ShaderStage::Vertex:
					{
						const uint8 descriptorOffset = pPipelineLayoutData->m_vertexBufferCount;
						if (pDescriptorSetLayoutData->m_argumentBuffers)
						{
							[m_pCommandEncoder setVertexBuffer:pDescriptorSetData->m_argumentBuffer
																					offset:0
																				 atIndex:descriptorOffset + descriptorIndices[shaderStageIndex]++];
						}
						else
						{
							for (const Internal::DescriptorSetLayoutData::Binding& __restrict descriptorSetBinding : pDescriptorSetLayoutData->m_bindings)
							{
								if (descriptorSetBinding.m_stages.IsSet(shaderStage))
								{
									switch (descriptorSetBinding.m_type)
									{
										case DescriptorType::Sampler:
										{
											[m_pCommandEncoder
												setVertexSamplerState:(id<MTLSamplerState>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																			atIndex:descriptorOffset + descriptorIndices[shaderStageIndex]++];
										}
										break;
										case DescriptorType::SampledImage:
										case DescriptorType::InputAttachment:
										case DescriptorType::UniformTexelBuffer:
										case DescriptorType::StorageImage:
										case DescriptorType::StorageTexelBuffer:
										{
											[m_pCommandEncoder setVertexTexture:(id<MTLTexture>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																									atIndex:descriptorOffset + descriptorIndices[shaderStageIndex]++];
										}
										break;
										case DescriptorType::UniformBuffer:
										case DescriptorType::UniformBufferDynamic:
										case DescriptorType::StorageBuffer:
										case DescriptorType::StorageBufferDynamic:
										{
											[m_pCommandEncoder setVertexBuffer:(id<MTLBuffer>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																									offset:0
																								 atIndex:descriptorOffset + descriptorIndices[shaderStageIndex]++];
										}
										break;
										case DescriptorType::AccelerationStructure:
										{
											[m_pCommandEncoder setVertexAccelerationStructure:(id<MTLAccelerationStructure>)
																																					pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																													atBufferIndex:descriptorOffset + descriptorIndices[shaderStageIndex]++];
										}
										break;
										case DescriptorType::CombinedImageSampler:
										{
											[m_pCommandEncoder setVertexTexture:(id<MTLTexture>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																									atIndex:descriptorOffset + descriptorIndices[shaderStageIndex]++];
											[m_pCommandEncoder
												setVertexSamplerState:(id<MTLSamplerState>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex + 1]
																			atIndex:descriptorOffset + descriptorIndices[shaderStageIndex]++];
										}
										break;
									}
								}
							}
						}
					}
					break;
					case ShaderStage::Fragment:
					{
						if (pDescriptorSetLayoutData->m_argumentBuffers)
						{
							[m_pCommandEncoder setFragmentBuffer:pDescriptorSetData->m_argumentBuffer
																						offset:0
																					 atIndex:descriptorIndices[shaderStageIndex]++];
						}
						else
						{
							for (const Internal::DescriptorSetLayoutData::Binding& __restrict descriptorSetBinding : pDescriptorSetLayoutData->m_bindings)
							{
								if (descriptorSetBinding.m_stages.IsSet(shaderStage))
								{
									switch (descriptorSetBinding.m_type)
									{
										case DescriptorType::Sampler:
										{
											[m_pCommandEncoder
												setFragmentSamplerState:(id<MTLSamplerState>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																				atIndex:descriptorIndices[shaderStageIndex]++];
										}
										break;
										case DescriptorType::SampledImage:
										case DescriptorType::InputAttachment:
										case DescriptorType::UniformTexelBuffer:
										case DescriptorType::StorageImage:
										case DescriptorType::StorageTexelBuffer:
										{
											[m_pCommandEncoder setFragmentTexture:(id<MTLTexture>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																										atIndex:descriptorIndices[shaderStageIndex]++];
										}
										break;
										case DescriptorType::UniformBuffer:
										case DescriptorType::UniformBufferDynamic:
										case DescriptorType::StorageBuffer:
										case DescriptorType::StorageBufferDynamic:
										{
											[m_pCommandEncoder setFragmentBuffer:(id<MTLBuffer>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																										offset:0
																									 atIndex:descriptorIndices[shaderStageIndex]++];
										}
										break;
										case DescriptorType::AccelerationStructure:
										{
											[m_pCommandEncoder setFragmentAccelerationStructure:(id<MTLAccelerationStructure>)
																																						pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																														atBufferIndex:descriptorIndices[shaderStageIndex]++];
										}
										break;
										case DescriptorType::CombinedImageSampler:
										{
											[m_pCommandEncoder setFragmentTexture:(id<MTLTexture>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																										atIndex:descriptorIndices[shaderStageIndex]++];
											[m_pCommandEncoder
												setFragmentSamplerState:(id<MTLSamplerState>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex + 1]
																				atIndex:descriptorIndices[shaderStageIndex]++];
										}
										break;
									}
								}
							}
						}
					}
					break;
					case ShaderStage::Compute:
						break;

					case ShaderStage::TessellationControl:
					case ShaderStage::TessellationEvaluation:
						break;

					case Rendering::ShaderStage::RayGeneration:
					case Rendering::ShaderStage::RayAnyHit:
					case Rendering::ShaderStage::RayClosestHit:
					case Rendering::ShaderStage::RayIntersection:
					case Rendering::ShaderStage::RayMiss:
					case Rendering::ShaderStage::RayCallable:
						break;

					case ShaderStage::Geometry:
						break;

					case ShaderStage::Invalid:
					case ShaderStage::Count:
					case ShaderStage::All:
						ExpectUnreachable();
				}
			}

			for (const Internal::DescriptorSetData::ResourceBatch& __restrict resourceBatch : pDescriptorSetData->m_resourceBatches)
			{
				const uint8 resourceBatchIndex = pDescriptorSetData->m_resourceBatches.GetIteratorIndex(&resourceBatch);
				const Internal::DescriptorSetLayoutData::ResourceBatch& __restrict batch =
					pDescriptorSetLayoutData->m_resourceBatches[resourceBatchIndex];

				[m_pCommandEncoder useResources:resourceBatch.m_resources.GetData()
																	count:resourceBatch.m_resources.GetSize()
																	usage:batch.m_usage
																 stages:batch.m_stages];
			}
		}
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, sets = InlineVector<DescriptorSetView, 2>(sets), firstSetIndex]()
			{
				WGPURenderPassEncoder pWGPURenderPassEncoder = *pCommandEncoder;
				if (pWGPURenderPassEncoder != nullptr)
				{
					for (const DescriptorSetView& descriptorSet : sets)
					{
						const uint32 index = firstSetIndex + sets.GetIteratorIndex(&descriptorSet);
						Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
						Assert(pDescriptorSetData->m_bindGroup != nullptr);
						wgpuRenderPassEncoderSetBindGroup(pWGPURenderPassEncoder, index, pDescriptorSetData->m_bindGroup, 0, nullptr);
					}
				}
			}
		);
#else
		for (const DescriptorSetView& descriptorSet : sets)
		{
			const uint32 index = firstSetIndex + sets.GetIteratorIndex(&descriptorSet);
			Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
			Assert(pDescriptorSetData->m_bindGroup != nullptr);
			wgpuRenderPassEncoderSetBindGroup(m_pCommandEncoder, index, pDescriptorSetData->m_bindGroup, 0, nullptr);
		}
#endif
#else
		Assert(false, "TODO");
		UNUSED(pipelineLayout);
		UNUSED(sets);
		UNUSED(firstSetIndex);
#endif
	}

	void RenderCommandEncoderView::BindDynamicDescriptorSets(
		[[maybe_unused]] const PipelineLayoutView pipelineLayout,
		const ArrayView<const DescriptorSetView, uint8> sets,
		const ArrayView<const uint32, uint8> dynamicOffsets,
		const uint32 firstSetIndex
	) const
	{
		Assert(sets.All(
			[](const DescriptorSetView descriptorSet)
			{
				return descriptorSet.IsValid();
			}
		));

		Assert(dynamicOffsets.GetSize() == sets.GetSize());
#if RENDERER_VULKAN
		vkCmdBindDescriptorSets(
			m_pCommandEncoder,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			firstSetIndex,
			sets.GetSize(),
			reinterpret_cast<const VkDescriptorSet*>(sets.GetData()),
			dynamicOffsets.GetSize(),
			dynamicOffsets.GetData()
		);
#elif RENDERER_METAL
		Assert(false, "Not supported for Metal yet");
		UNUSED(sets);
		UNUSED(dynamicOffsets);
		UNUSED(firstSetIndex);
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder,
		   sets = InlineVector<DescriptorSetView, 2>(sets),
		   firstSetIndex,
		   dynamicOffsets = InlineVector<uint32, 2>(dynamicOffsets)]()
			{
				WGPURenderPassEncoder pWGPURenderPassEncoder = *pCommandEncoder;
				if (pWGPURenderPassEncoder != nullptr)
				{
					for (const DescriptorSetView& descriptorSet : sets)
					{
						const uint32 index = firstSetIndex + sets.GetIteratorIndex(&descriptorSet);
						Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
						Assert(pDescriptorSetData->m_bindGroup != nullptr);
						wgpuRenderPassEncoderSetBindGroup(
							pWGPURenderPassEncoder,
							index,
							pDescriptorSetData->m_bindGroup,
							dynamicOffsets.GetSize(),
							dynamicOffsets.GetData()
						);
					}
				}
			}
		);
#else
		for (const DescriptorSetView& descriptorSet : sets)
		{
			const uint32 index = firstSetIndex + sets.GetIteratorIndex(&descriptorSet);
			Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
			Assert(pDescriptorSetData->m_bindGroup != nullptr);
			wgpuRenderPassEncoderSetBindGroup(
				m_pCommandEncoder,
				index,
				pDescriptorSetData->m_bindGroup,
				dynamicOffsets.GetSize(),
				dynamicOffsets.GetData()
			);
		}
#endif
#else
		Assert(false, "TODO");
		UNUSED(pipelineLayout);
		UNUSED(sets);
		UNUSED(firstSetIndex);
#endif
	}

	void RenderCommandEncoderView::BindPipeline(const GraphicsPipeline& pipeline) const
	{
		Assert(pipeline.IsValid());

#if RENDERER_VULKAN
		vkCmdBindPipeline(m_pCommandEncoder, VK_PIPELINE_BIND_POINT_GRAPHICS, (GraphicsPipelineView)pipeline);
#elif RENDERER_METAL
		[m_pCommandEncoder setRenderPipelineState:(GraphicsPipelineView)pipeline];
		if (pipeline.m_depthStencilState != nil)
		{
			[m_pCommandEncoder setDepthStencilState:pipeline.m_depthStencilState];

			[m_pCommandEncoder setCullMode:static_cast<MTLCullMode>(pipeline.m_cullMode.GetUnderlyingValue())];
			[m_pCommandEncoder
				setFrontFacingWinding:pipeline.m_windingOrder == WindingOrder::CounterClockwise ? MTLWindingCounterClockwise : MTLWindingClockwise];
			[m_pCommandEncoder
				setTriangleFillMode:pipeline.m_polygonMode == PolygonMode::Fill ? MTLTriangleFillModeFill : MTLTriangleFillModeLines];
			[m_pCommandEncoder setDepthBias:(float)pipeline.m_depthBiasConstantFactor
													 slopeScale:pipeline.m_depthBiasSlopeFactor
																clamp:pipeline.m_depthBiasClamp];

			if (pipeline.m_depthClamp)
			{
				[m_pCommandEncoder setDepthClipMode:MTLDepthClipModeClamp];
			}
			else
			{
				[m_pCommandEncoder setDepthClipMode:MTLDepthClipModeClip];
			}

			[m_pCommandEncoder setStencilFrontReferenceValue:pipeline.m_stencilFrontReference backReferenceValue:pipeline.m_stencilBackReference];
		}
#elif RENDERER_WEBGPU

#if WEBGPU_SINGLE_THREADED
		GraphicsPipelineView pipelineView = pipeline;
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder,
		   pipelineView,
		   stencilTest = pipeline.m_stencilTest,
		   stencilReference = pipeline.m_stencilReference]()
			{
				WGPURenderPassEncoder pRenderPassEncoder = *pCommandEncoder;
				wgpuRenderPassEncoderSetPipeline(pRenderPassEncoder, pipelineView);

				if (stencilTest)
				{
					wgpuRenderPassEncoderSetStencilReference(pRenderPassEncoder, stencilReference);
				}
			}
		);
#else
		wgpuRenderPassEncoderSetPipeline(m_pCommandEncoder, (GraphicsPipelineView)pipeline);

		if (pipeline.m_stencilTest)
		{
			wgpuRenderPassEncoderSetStencilReference(m_pCommandEncoder, pipeline.m_stencilReference);
		}
#endif

#else
		Assert(false, "TODO");
		UNUSED(pipeline);
#endif
	}

	void RenderCommandEncoderView::DrawIndexed(
		const BufferView buffer,
		const uint64 offset,
		const uint64 size,
		const uint32 indexCount,
		const uint32 instanceCount,
		const uint32 firstIndex,
		const int32 vertexOffset,
		const uint32 firstInstance
	) const
	{
		Assert(buffer.IsValid());

#if RENDERER_VULKAN
		// TODO: Investigate vkCmdBindVertexBuffers2EXT
		// It allows strided vertex buffer binding

		UNUSED(size);
		constexpr VkIndexType indexType = Math::Select(TypeTraits::IsSame<Index, uint32>, VK_INDEX_TYPE_UINT32, VK_INDEX_TYPE_UINT16);
		vkCmdBindIndexBuffer(m_pCommandEncoder, buffer, offset, indexType);

		vkCmdDrawIndexed(m_pCommandEncoder, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
#elif RENDERER_METAL
		constexpr MTLIndexType indexType = Math::Select(TypeTraits::IsSame<Index, uint32>, MTLIndexTypeUInt32, MTLIndexTypeUInt16);

		Assert(firstIndex == 0);
		UNUSED(firstIndex);
		UNUSED(size);

		// TODO: Read primitive from pipeline
		[m_pCommandEncoder drawIndexedPrimitives:MTLPrimitiveTypeTriangle
																	indexCount:indexCount
																	 indexType:indexType
																 indexBuffer:buffer
													 indexBufferOffset:offset
															 instanceCount:instanceCount
																	baseVertex:vertexOffset
																baseInstance:firstInstance];
#elif RENDERER_WEBGPU

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, buffer, offset, size, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance]()
			{
				WGPURenderPassEncoder pRenderPassEncoder = *pCommandEncoder;
				constexpr WGPUIndexFormat indexFormat =
					Math::Select(TypeTraits::IsSame<Index, uint32>, WGPUIndexFormat_Uint32, WGPUIndexFormat_Uint16);
				wgpuRenderPassEncoderSetIndexBuffer(pRenderPassEncoder, buffer, indexFormat, offset, size);

				wgpuRenderPassEncoderDrawIndexed(pRenderPassEncoder, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
			}
		);
#else
		constexpr WGPUIndexFormat indexFormat = Math::Select(TypeTraits::IsSame<Index, uint32>, WGPUIndexFormat_Uint32, WGPUIndexFormat_Uint16);
		wgpuRenderPassEncoderSetIndexBuffer(m_pCommandEncoder, buffer, indexFormat, offset, size);

		wgpuRenderPassEncoderDrawIndexed(m_pCommandEncoder, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
#endif

#else
		UNUSED(indexCount);
		UNUSED(instanceCount);
		UNUSED(firstIndex);
		UNUSED(vertexOffset);
		UNUSED(firstInstance);
		Assert(false, "TODO");
#endif
	}

	void RenderCommandEncoderView::Draw(
		const uint32 vertexCount, const uint32 instanceCount, const uint32 firstVertex, const uint32 firstInstance
	) const
	{
#if RENDERER_VULKAN
		vkCmdDraw(m_pCommandEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
#elif RENDERER_METAL
		// TODO: Read primitive from pipeline
		[m_pCommandEncoder drawPrimitives:MTLPrimitiveTypeTriangle
													vertexStart:firstVertex
													vertexCount:vertexCount
												instanceCount:instanceCount
												 baseInstance:firstInstance];
#elif RENDERER_WEBGPU

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, vertexCount, instanceCount, firstVertex, firstInstance]()
			{
				wgpuRenderPassEncoderDraw(*pCommandEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
			}
		);
#else
		wgpuRenderPassEncoderDraw(m_pCommandEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
#endif

#else
		UNUSED(vertexCount);
		UNUSED(instanceCount);
		UNUSED(firstVertex);
		UNUSED(firstInstance);
		Assert(false, "TODO");
#endif
	}

	void RenderCommandEncoderView::DrawIndexedIndirect(
		const BufferView buffer, uint64 offset, const uint32 drawCount, const uint32 offsetStride
	) const
	{
		Assert(buffer.IsValid());

#if RENDERER_VULKAN
		vkCmdDrawIndexedIndirect(m_pCommandEncoder, buffer, offset, drawCount, offsetStride);
#elif RENDERER_WEBGPU

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, drawCount, buffer, offset, offsetStride]() mutable
			{
				WGPURenderPassEncoder pRenderPassEncoder = *pCommandEncoder;
				for (uint32 drawIndex = 0; drawIndex < drawCount; ++drawIndex)
				{
					wgpuRenderPassEncoderDrawIndexedIndirect(pRenderPassEncoder, buffer, offset);
					offset += offsetStride;
				}
			}
		);
#else
		for (uint32 drawIndex = 0; drawIndex < drawCount; ++drawIndex)
		{
			wgpuRenderPassEncoderDrawIndexedIndirect(m_pCommandEncoder, buffer, offset);
			offset += offsetStride;
		}
#endif

#else
		UNUSED(buffer);
		UNUSED(offset);
		UNUSED(drawCount);
		UNUSED(offsetStride);
		Assert(false, "TODO");
#endif
	}

	void
	RenderCommandEncoderView::DrawIndirect(const BufferView buffer, uint64 offset, const uint32 drawCount, const uint32 offsetStride) const
	{
		Assert(buffer.IsValid());

#if RENDERER_VULKAN
		vkCmdDrawIndirect(m_pCommandEncoder, buffer, offset, drawCount, offsetStride);
#elif RENDERER_WEBGPU

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, drawCount, buffer, offset, offsetStride]() mutable
			{
				WGPURenderPassEncoder pRenderPassEncoder = *pCommandEncoder;
				for (uint32 drawIndex = 0; drawIndex < drawCount; ++drawIndex)
				{
					wgpuRenderPassEncoderDrawIndirect(pRenderPassEncoder, buffer, offset);
					offset += offsetStride;
				}
			}
		);
#else
		for (uint32 drawIndex = 0; drawIndex < drawCount; ++drawIndex)
		{
			wgpuRenderPassEncoderDrawIndirect(m_pCommandEncoder, buffer, offset);
			offset += offsetStride;
		}
#endif

#else
		UNUSED(buffer);
		UNUSED(offset);
		UNUSED(drawCount);
		UNUSED(offsetStride);
		Assert(false, "TODO");
#endif
	}

	void RenderCommandEncoderView::SetDepthBias(
		const float depthBiasConstantFactor, const float depthBiasClamp, const float depthBiasSlopeFactor
	) const
	{
#if RENDERER_VULKAN
		vkCmdSetDepthBias(m_pCommandEncoder, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
#elif RENDERER_METAL
		[m_pCommandEncoder setDepthBias:depthBiasConstantFactor slopeScale:depthBiasSlopeFactor clamp:depthBiasClamp];
#elif RENDERER_WEBGPU
		Assert(false, "Unsupported!");
		UNUSED(depthBiasConstantFactor);
		UNUSED(depthBiasClamp);
		UNUSED(depthBiasSlopeFactor);
#else
		UNUSED(depthBiasConstantFactor);
		UNUSED(depthBiasClamp);
		UNUSED(depthBiasSlopeFactor);
		Assert(false, "TODO");
#endif
	}

	void RenderCommandEncoderView::SetCullMode([[maybe_unused]] const LogicalDevice& logicalDevice, const EnumFlags<CullMode> cullMode) const
	{
#if RENDERER_VULKAN
		PFN_vkCmdSetCullModeEXT cmdSetCullModeEXT = reinterpret_cast<PFN_vkCmdSetCullModeEXT>(logicalDevice.GetCmdSetCullModeEXT());
		Assert(cmdSetCullModeEXT != nullptr);
		if (LIKELY(cmdSetCullModeEXT != nullptr))
		{
			cmdSetCullModeEXT(m_pCommandEncoder, static_cast<VkCullModeFlags>(cullMode.GetFlags()));
		}
#elif RENDERER_METAL
		[m_pCommandEncoder setCullMode:static_cast<MTLCullMode>(cullMode.GetUnderlyingValue())];
#elif RENDERER_WEBGPU
		// Assert(false, "Unsupported!");
		UNUSED(cullMode);
#else
		UNUSED(cullMode);
		Assert(false, "TODO");
#endif
	}

	void RenderCommandEncoderView::ExecuteCommands(const ArrayView<const EncodedParallelCommandBufferView> otherCommandBuffers) const
	{
		Assert(otherCommandBuffers.All(
			[](const EncodedParallelCommandBufferView commandBuffer)
			{
				return commandBuffer.IsValid();
			}
		));

#if RENDERER_VULKAN
		vkCmdExecuteCommands(
			m_pCommandEncoder,
			otherCommandBuffers.GetSize(),
			reinterpret_cast<const VkCommandBuffer*>(otherCommandBuffers.GetData())
		);
#elif RENDERER_WEBGPU

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder,
		   otherCommandBuffers = InlineVector<EncodedParallelCommandBufferView, 2>{otherCommandBuffers}]() mutable
			{
				InlineVector<WGPURenderBundle, 2> renderBundles(Memory::Reserve, otherCommandBuffers.GetSize());
				for (const EncodedParallelCommandBufferView encodedParallelCommandBuffer : otherCommandBuffers)
				{
					renderBundles.EmplaceBack(encodedParallelCommandBuffer);
				}
				wgpuRenderPassEncoderExecuteBundles(*pCommandEncoder, renderBundles.GetSize(), renderBundles.GetData());
			}
		);
#else
		wgpuRenderPassEncoderExecuteBundles(
			m_pCommandEncoder,
			otherCommandBuffers.GetSize(),
			reinterpret_cast<WGPURenderBundle const *>(otherCommandBuffers.GetData())
		);
#endif

#else
		UNUSED(otherCommandBuffers);
		Assert(false, "TODO");
#endif
	}

#if RENDERER_METAL || RENDERER_WEBGPU
	[[nodiscard]] AttachmentLoadType GetSubpassAttachmentLoadType(
		AttachmentLoadType attachmentLoadType,
		const uint8 currentSubpassIndex,
		const uint8 attachmentIndex,
		const Internal::RenderPassData& __restrict renderPassData
	)
	{
		switch (attachmentLoadType)
		{
			case AttachmentLoadType::LoadExisting:
			case AttachmentLoadType::Undefined:
				return AttachmentLoadType::LoadExisting;
			case AttachmentLoadType::Clear:
			{
				for (uint8 otherSubpassIndex = 0; otherSubpassIndex < currentSubpassIndex; ++otherSubpassIndex)
				{
					const bool isInOtherSubpass = renderPassData.m_subpassColorAttachmentIndices[otherSubpassIndex].Contains(attachmentIndex);
					if (isInOtherSubpass || renderPassData.m_subpassDepthAttachmentIndices[otherSubpassIndex] == attachmentIndex)
					{
						return AttachmentLoadType::LoadExisting;
					}
				}
				return AttachmentLoadType::Clear;
			}
		}
		ExpectUnreachable();
	}
#endif

	void RenderCommandEncoder::StartNextSubpass([[maybe_unused]] const ArrayView<const ClearValue, uint8> clearValues)
	{
#if RENDERER_VULKAN
		vkCmdNextSubpass(m_pCommandEncoder, VK_SUBPASS_CONTENTS_INLINE);
#elif RENDERER_METAL
		[m_pCommandEncoder endEncoding];
		m_pCommandEncoder = nil;

		const Internal::RenderPassData* __restrict pRenderPassData{m_renderPass};
		const Internal::FramebufferData* __restrict pFramebufferData{m_framebuffer};
		const ArrayView<const ImageMappingView, uint8> attachmentMappings = pFramebufferData->m_attachmentMappings;

		MTLRenderPassDescriptor* renderPassDescriptor = [[MTLRenderPassDescriptor alloc] init];

		const uint8 subpassIndex = ++m_currentSubpassIndex;
		const uint8 subpassCount = pRenderPassData->m_subpassColorAttachmentIndices.GetSize();
		const ArrayView<const uint8> subpassColorAttachmentIndices = pRenderPassData->m_subpassColorAttachmentIndices[subpassIndex];
		uint8 localAttachmentIndex{0};

		for (const uint8 attachmentIndex : subpassColorAttachmentIndices)
		{
			const AttachmentDescription& __restrict attachmentDescription = pRenderPassData->m_attachmentDescriptions[attachmentIndex];
			const EnumFlags<FormatFlags> attachmentFormatFlags = GetFormatInfo(attachmentDescription.m_format).m_flags;

			const ImageMappingView imageMapping = attachmentMappings[attachmentIndex];
			id<MTLTexture> metalTexture = imageMapping;

			const Math::Color clearValue = clearValues[attachmentIndex];

			MTLRenderPassColorAttachmentDescriptor* colorAttachmentDescriptor = [[MTLRenderPassColorAttachmentDescriptor alloc] init];

			colorAttachmentDescriptor.texture = metalTexture;

			colorAttachmentDescriptor.loadAction =
				ConvertLoadType(GetSubpassAttachmentLoadType(attachmentDescription.m_loadType, subpassIndex, attachmentIndex, *pRenderPassData));
			colorAttachmentDescriptor.storeAction = ConvertStoreType(GetNextSubpassAttachmentStoreType(
				attachmentDescription.m_storeType,
				subpassIndex,
				subpassCount,
				attachmentIndex,
				ImageAspectFlags::Color,
				*pRenderPassData
			));

			colorAttachmentDescriptor.clearColor = MTLClearColorMake(clearValue.r, clearValue.g, clearValue.b, clearValue.a);

			renderPassDescriptor.colorAttachments[localAttachmentIndex] = colorAttachmentDescriptor;

			localAttachmentIndex++;
		}

		const uint8 depthAttachmentIndex = pRenderPassData->m_subpassDepthAttachmentIndices[subpassIndex];
		if (depthAttachmentIndex != Internal::RenderPassData::InvalidAttachmentIndex)
		{
			const AttachmentDescription& __restrict attachmentDescription = pRenderPassData->m_attachmentDescriptions[depthAttachmentIndex];
			const EnumFlags<FormatFlags> attachmentFormatFlags = GetFormatInfo(attachmentDescription.m_format).m_flags;

			const ImageMappingView imageMapping = attachmentMappings[depthAttachmentIndex];
			id<MTLTexture> metalTexture = imageMapping;

			const DepthStencilValue clearValue = clearValues[depthAttachmentIndex];
			if (attachmentFormatFlags.IsSet(FormatFlags::Depth))
			{
				MTLRenderPassDepthAttachmentDescriptor* depthAttachmentDescriptor = [[MTLRenderPassDepthAttachmentDescriptor alloc] init];
				depthAttachmentDescriptor.texture = metalTexture;
				depthAttachmentDescriptor.loadAction = ConvertLoadType(
					GetSubpassAttachmentLoadType(attachmentDescription.m_loadType, subpassIndex, depthAttachmentIndex, *pRenderPassData)
				);
				depthAttachmentDescriptor.storeAction = ConvertStoreType(GetNextSubpassAttachmentStoreType(
					attachmentDescription.m_storeType,
					subpassIndex,
					subpassCount,
					depthAttachmentIndex,
					ImageAspectFlags::Depth,
					*pRenderPassData
				));
				depthAttachmentDescriptor.clearDepth = clearValue.m_depth.m_value;
				renderPassDescriptor.depthAttachment = depthAttachmentDescriptor;
			}
			if (attachmentFormatFlags.IsSet(FormatFlags::Stencil))
			{
				MTLRenderPassStencilAttachmentDescriptor* stencilAttachmentDescriptor = [[MTLRenderPassStencilAttachmentDescriptor alloc] init];
				stencilAttachmentDescriptor.texture = metalTexture;
				stencilAttachmentDescriptor.loadAction = ConvertLoadType(
					GetSubpassAttachmentLoadType(attachmentDescription.m_stencilLoadType, subpassIndex, depthAttachmentIndex, *pRenderPassData)
				);
				stencilAttachmentDescriptor.storeAction = ConvertStoreType(GetNextSubpassAttachmentStoreType(
					attachmentDescription.m_stencilStoreType,
					subpassIndex,
					subpassCount,
					depthAttachmentIndex,
					ImageAspectFlags::Stencil,
					*pRenderPassData
				));
				stencilAttachmentDescriptor.clearStencil = clearValue.m_stencil.m_value;
				renderPassDescriptor.stencilAttachment = stencilAttachmentDescriptor;
			}
		}

		m_pCommandEncoder = [(id<MTLCommandBuffer>)m_commandEncoder renderCommandEncoderWithDescriptor:renderPassDescriptor];
#elif RENDERER_WEBGPU

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder]() mutable
			{
				WGPURenderPassEncoder pRenderPassEncoder = *pCommandEncoder;
				wgpuRenderPassEncoderEnd(pRenderPassEncoder);
				wgpuRenderPassEncoderRelease(pRenderPassEncoder);
			}
		);
#else
		wgpuRenderPassEncoderEnd(m_pCommandEncoder);
		wgpuRenderPassEncoderRelease(m_pCommandEncoder);
		m_pCommandEncoder = nullptr;
#endif

		const Internal::RenderPassData* __restrict pRenderPassData{m_renderPass};
		const Internal::FramebufferData* __restrict pFramebufferData{m_framebuffer};
		const ArrayView<const ImageMappingView, uint8> attachmentMappings = pFramebufferData->m_attachmentMappings;

		const uint8 subpassIndex = ++m_currentSubpassIndex;
		const uint8 subpassCount = pRenderPassData->m_subpassColorAttachmentIndices.GetSize();
		Assert(subpassIndex < subpassCount);
		const ArrayView<const uint8> subpassColorAttachmentIndices = pRenderPassData->m_subpassColorAttachmentIndices[subpassIndex];

		FixedCapacityInlineVector<WGPURenderPassColorAttachment, 8>
			colorAttachments(Memory::ConstructWithSize, Memory::DefaultConstruct, subpassColorAttachmentIndices.GetSize());
		WGPURenderPassDepthStencilAttachment depthStencilAttachment{nullptr};

		uint8 localAttachmentIndex{0};

		for (const uint8 attachmentIndex : subpassColorAttachmentIndices)
		{
			const AttachmentDescription& __restrict attachmentDescription = pRenderPassData->m_attachmentDescriptions[attachmentIndex];
			const EnumFlags<FormatFlags> attachmentFormatFlags = GetFormatInfo(attachmentDescription.m_format).m_flags;

			const ImageMappingView imageMapping = attachmentMappings[attachmentIndex];

			const Math::Color clearValue = clearValues[attachmentIndex];
			const AttachmentLoadType attachmentLoadType =
				GetSubpassAttachmentLoadType(attachmentDescription.m_loadType, subpassIndex, attachmentIndex, *pRenderPassData);
			const AttachmentStoreType attachmentStoreType = GetNextSubpassAttachmentStoreType(
				attachmentDescription.m_storeType,
				subpassIndex,
				subpassCount,
				attachmentIndex,
				ImageAspectFlags::Color,
				*pRenderPassData
			);

			colorAttachments[localAttachmentIndex] = WGPURenderPassColorAttachment{
				nullptr,
				imageMapping,
				WGPU_DEPTH_SLICE_UNDEFINED,
				nullptr, // resolve target mapping
				ConvertLoadType(attachmentLoadType, WGPULoadOp_Clear),
				ConvertStoreType(attachmentStoreType, WGPUStoreOp_Discard),
				WGPUColor{clearValue.r, clearValue.g, clearValue.b, clearValue.a}
			};

			localAttachmentIndex++;
		}

		const uint8 depthAttachmentIndex = pRenderPassData->m_subpassDepthAttachmentIndices[subpassIndex];
		if (depthAttachmentIndex != Internal::RenderPassData::InvalidAttachmentIndex)
		{
			const AttachmentDescription& __restrict attachmentDescription = pRenderPassData->m_attachmentDescriptions[depthAttachmentIndex];
			const EnumFlags<FormatFlags> attachmentFormatFlags = GetFormatInfo(attachmentDescription.m_format).m_flags;

			const ImageMappingView imageMapping = attachmentMappings[depthAttachmentIndex];

			const DepthStencilValue clearValue = clearValues[depthAttachmentIndex];

			const bool isDepthReadOnly = (attachmentDescription.m_initialLayout == ImageLayout::DepthStencilReadOnlyOptimal) |
			                             (attachmentDescription.m_initialLayout == ImageLayout::DepthReadOnlyOptimal) |
			                             (attachmentDescription.m_initialLayout == ImageLayout::DepthReadOnlyStencilAttachmentOptimal);
			const bool isStencilReadOnly = (attachmentDescription.m_initialLayout == ImageLayout::DepthStencilReadOnlyOptimal) |
			                               (attachmentDescription.m_initialLayout == ImageLayout::DepthAttachmentStencilReadOnlyOptimal) |
			                               (attachmentDescription.m_initialLayout == ImageLayout::StencilReadOnlyOptimal);

			const AttachmentLoadType depthLoadType =
				GetSubpassAttachmentLoadType(attachmentDescription.m_loadType, subpassIndex, depthAttachmentIndex, *pRenderPassData);
			const AttachmentLoadType stencilLoadType =
				GetSubpassAttachmentLoadType(attachmentDescription.m_stencilLoadType, subpassIndex, depthAttachmentIndex, *pRenderPassData);

			const AttachmentStoreType depthStoreType = GetNextSubpassAttachmentStoreType(
				attachmentDescription.m_storeType,
				subpassIndex,
				subpassCount,
				depthAttachmentIndex,
				ImageAspectFlags::Depth,
				*pRenderPassData
			);
			const AttachmentStoreType stencilStoreType = GetNextSubpassAttachmentStoreType(
				attachmentDescription.m_stencilStoreType,
				subpassIndex,
				subpassCount,
				depthAttachmentIndex,
				ImageAspectFlags::Stencil,
				*pRenderPassData
			);

			depthStencilAttachment = WGPURenderPassDepthStencilAttachment{
				imageMapping,
				ConvertLoadType(depthLoadType),
				ConvertStoreType(depthStoreType),
				clearValue.m_depth.m_value,
				isDepthReadOnly,
				ConvertLoadType(stencilLoadType),
				ConvertStoreType(stencilStoreType),
				clearValue.m_stencil.m_value,
				isStencilReadOnly
			};

			if (attachmentFormatFlags.IsSet(FormatFlags::Depth) && !isDepthReadOnly)
			{
				if (depthStencilAttachment.depthLoadOp == WGPULoadOp_Undefined)
				{
					depthStencilAttachment.depthLoadOp = WGPULoadOp_Clear;
				}
				if (depthStencilAttachment.depthStoreOp == WGPUStoreOp_Undefined)
				{
					depthStencilAttachment.depthStoreOp = WGPUStoreOp_Discard;
				}
			}
			if (attachmentFormatFlags.IsSet(FormatFlags::Stencil) && !isStencilReadOnly)
			{
				if (depthStencilAttachment.stencilLoadOp == WGPULoadOp_Undefined)
				{
					depthStencilAttachment.stencilLoadOp = WGPULoadOp_Clear;
				}
				if (depthStencilAttachment.stencilStoreOp == WGPUStoreOp_Undefined)
				{
					depthStencilAttachment.stencilStoreOp = WGPUStoreOp_Discard;
				}
			}
		}

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pRenderPassEncoder = m_pCommandEncoder,
		   commandEncoder = m_commandEncoder,
		   colorAttachments = Move(colorAttachments),
		   depthStencilAttachment]() mutable
			{
				const WGPURenderPassDescriptor renderPassDescriptor
				{
					nullptr,
#if RENDERER_WEBGPU_DAWN
						WGPUStringView{nullptr, 0},
#else
						nullptr,
#endif
						colorAttachments.GetSize(), colorAttachments.GetData(),
						depthStencilAttachment.view != nullptr ? &depthStencilAttachment : nullptr, nullptr, nullptr
				};

				WGPURenderPassEncoder pWGPURenderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDescriptor);
#if RENDERER_WEBGPU_DAWN
				wgpuRenderPassEncoderAddRef(pWGPURenderPassEncoder);
#else
				wgpuRenderPassEncoderReference(pWGPURenderPassEncoder);
#endif
				*pRenderPassEncoder = pWGPURenderPassEncoder;
			}
		);
#else
		const WGPURenderPassDescriptor renderPassDescriptor{
			nullptr,
			nullptr,
			colorAttachments.GetSize(),
			colorAttachments.GetData(),
			depthStencilAttachment.view != nullptr ? &depthStencilAttachment : nullptr,
			nullptr,
			nullptr
		};
		WGPURenderPassEncoder pCommandEncoder = wgpuCommandEncoderBeginRenderPass(m_commandEncoder, &renderPassDescriptor);
#if RENDERER_WEBGPU_DAWN
		wgpuRenderPassEncoderAddRef(pCommandEncoder);
#else
		wgpuRenderPassEncoderReference(pCommandEncoder);
#endif
		m_pCommandEncoder = pCommandEncoder;
#endif

#else
		Assert(false, "TODO");
#endif
	}

	ParallelRenderCommandEncoder CommandEncoderView::BeginParallelRenderPass(
		LogicalDevice& logicalDevice,
		const RenderPassView renderPass,
		[[maybe_unused]] const FramebufferView framebuffer,
		const Math::Rectangleui extent,
		const ArrayView<const ClearValue, uint8> clearValues,
		[[maybe_unused]] const uint32 maximumPushConstantInstanceCount
	) const
	{
		Assert(framebuffer.IsValid());

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		if (maximumPushConstantInstanceCount > 0)
		{
			logicalDevice.ReservePushConstantsInstanceCount(maximumPushConstantInstanceCount);
		}
#endif

#if RENDERER_VULKAN
		UNUSED(logicalDevice);

		static_assert(sizeof(VkClearValue) == sizeof(ClearValue));
		static_assert(alignof(VkClearValue) == alignof(ClearValue));

		const VkRenderPassBeginInfo renderPassInfo = {
			VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
			nullptr,
			renderPass,
			framebuffer,
			VkRect2D{
				VkOffset2D{static_cast<int32>(extent.GetPosition().x), static_cast<int32>(extent.GetPosition().y)},
				VkExtent2D{extent.GetSize().x, extent.GetSize().y}
			},
			clearValues.GetSize(),
			reinterpret_cast<const VkClearValue*>(clearValues.GetData())
		};

		vkCmdBeginRenderPass(m_pCommandEncoder, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
		return ParallelRenderCommandEncoder{m_pCommandEncoder};
#elif RENDERER_WEBGPU
		const Internal::RenderPassData* __restrict pRenderPassData{renderPass};

		FixedCapacityInlineVector<WGPUTextureFormat, 8> colorTextureFormats(
			Memory::Reserve,
			pRenderPassData->m_attachmentDescriptions.GetSize()
		);
		WGPUTextureFormat depthStencilTextureFormat{WGPUTextureFormat_Undefined};

		const uint32 sampleCount{(uint32)pRenderPassData->m_attachmentDescriptions[0].m_sampleCount};
		Assert(pRenderPassData->m_attachmentDescriptions.GetView().All(
			[sampleCount](const AttachmentDescription& __restrict attachmentDescription)
			{
				return (uint32)attachmentDescription.m_sampleCount == sampleCount;
			}
		));

		bool isDepthReadOnly{true};
		bool isStencilReadOnly{true};

		for (const AttachmentDescription& __restrict attachmentDescription : pRenderPassData->m_attachmentDescriptions)
		{
			const EnumFlags<FormatFlags> attachmentFormatFlags = GetFormatInfo(attachmentDescription.m_format).m_flags;

			if (attachmentFormatFlags.AreNoneSet(FormatFlags::DepthStencil))
			{
				colorTextureFormats.EmplaceBack(ConvertFormat(attachmentDescription.m_format));
			}
			else
			{
				depthStencilTextureFormat = ConvertFormat(attachmentDescription.m_format);
				isDepthReadOnly = (attachmentDescription.m_initialLayout == ImageLayout::DepthStencilReadOnlyOptimal) |
				                  (attachmentDescription.m_initialLayout == ImageLayout::DepthReadOnlyOptimal) |
				                  (attachmentDescription.m_initialLayout == ImageLayout::DepthReadOnlyStencilAttachmentOptimal);
				isStencilReadOnly = (attachmentDescription.m_initialLayout == ImageLayout::DepthStencilReadOnlyOptimal) |
				                    (attachmentDescription.m_initialLayout == ImageLayout::DepthAttachmentStencilReadOnlyOptimal) |
				                    (attachmentDescription.m_initialLayout == ImageLayout::StencilReadOnlyOptimal);
			}
		}

		const WGPURenderBundleEncoderDescriptor descriptor
		{
			nullptr,
#if RENDERER_WEBGPU_DAWN
				WGPUStringView{nullptr, 0},
#else
				nullptr,
#endif
				colorTextureFormats.GetSize(), colorTextureFormats.GetData(), depthStencilTextureFormat, sampleCount, isDepthReadOnly,
				isStencilReadOnly
		};

		UNUSED(extent);
		UNUSED(clearValues);
#if WEBGPU_SINGLE_THREADED
		WGPURenderBundleEncoder* pParallelRenderPassCommandEncoder = new WGPURenderBundleEncoder{nullptr};
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pParallelRenderPassCommandEncoder, &logicalDevice, descriptor]() mutable
			{
				WGPURenderBundleEncoder pBundleEncoder = wgpuDeviceCreateRenderBundleEncoder(logicalDevice, &descriptor);
#if RENDERER_WEBGPU_DAWN
				wgpuRenderBundleEncoderAddRef(pBundleEncoder);
#else
				wgpuRenderBundleEncoderReference(pBundleEncoder);
#endif
				*pParallelRenderPassCommandEncoder = pBundleEncoder;
			}
		);
		return ParallelRenderCommandEncoder{pParallelRenderPassCommandEncoder};
#else
		WGPURenderBundleEncoder pParallelRenderPassCommandEncoder = wgpuDeviceCreateRenderBundleEncoder(logicalDevice, &descriptor);
#if RENDERER_WEBGPU_DAWN
		wgpuRenderBundleEncoderAddRef(pParallelRenderPassCommandEncoder);
#else
		wgpuRenderBundleEncoderReference(pParallelRenderPassCommandEncoder);
#endif
		return ParallelRenderCommandEncoder{pParallelRenderPassCommandEncoder};
#endif
#else
		Assert(false, "TODO");
		UNUSED(logicalDevice);
		UNUSED(renderPass);
		UNUSED(framebuffer);
		UNUSED(extent);
		UNUSED(clearValues);
		return {};
#endif
	}

	ParallelRenderCommandEncoder::~ParallelRenderCommandEncoder()
	{
#if RENDERER_VULKAN
		vkCmdEndRenderPass(m_pCommandEncoder);
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder]() mutable
			{
				WGPURenderBundleEncoder pWGPURenderBundleEncoder = *pCommandEncoder;
				if (pWGPURenderBundleEncoder != nullptr)
				{
					wgpuRenderBundleEncoderRelease(pWGPURenderBundleEncoder);
				}
				delete pCommandEncoder;
			}
		);
#else
		wgpuRenderBundleEncoderRelease(m_pCommandEncoder);
#endif
#endif
	}

	ParallelRenderCommandEncoder& ParallelRenderCommandEncoder::operator=(ParallelRenderCommandEncoder&& other) noexcept
	{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
		Assert(m_pCommandEncoder == 0);
		m_pCommandEncoder = other.m_pCommandEncoder;
		other.m_pCommandEncoder = 0;
#endif
		return *this;
	}

	EncodedParallelCommandBuffer ParallelRenderCommandEncoder::StopEncoding()
	{
#if RENDERER_VULKAN
		if (m_pCommandEncoder != nullptr)
		{
			const VkResult result = vkEndCommandBuffer(m_pCommandEncoder);
			Assert(result == VK_SUCCESS);
			if (LIKELY(result == VK_SUCCESS))
			{
				EncodedParallelCommandBuffer encodedCommandBuffer{m_pCommandEncoder};
				m_pCommandEncoder = nullptr;
				return Move(encodedCommandBuffer);
			}
			else
			{
				m_pCommandEncoder = nullptr;
			}
		}
#elif RENDERER_WEBGPU
		if (m_pCommandEncoder != nullptr)
		{

			const WGPURenderBundleDescriptor descriptor{};
#if WEBGPU_SINGLE_THREADED
			WGPURenderBundle* pEncodedRenderBundle = new WGPURenderBundle{nullptr};
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pCommandEncoder = m_pCommandEncoder, descriptor, pEncodedRenderBundle]() mutable
				{
					WGPURenderBundleEncoder pRenderBundleEncoder = *pCommandEncoder;
					if (pRenderBundleEncoder != nullptr)
					{
						WGPURenderBundle pWGPURenderBundle = wgpuRenderBundleEncoderFinish(pRenderBundleEncoder, &descriptor);
						Assert(pWGPURenderBundle != nullptr);
						if (LIKELY(pWGPURenderBundle != nullptr))
						{
#if RENDERER_WEBGPU_DAWN
							wgpuRenderBundleAddRef(pWGPURenderBundle);
#else
							wgpuRenderBundleReference(pWGPURenderBundle);
#endif
							*pEncodedRenderBundle = pWGPURenderBundle;
						}
						wgpuRenderBundleEncoderRelease(pRenderBundleEncoder);
					}
					delete pCommandEncoder;
				}
			);
			m_pCommandEncoder = nullptr;
			return EncodedParallelCommandBuffer{pEncodedRenderBundle};
#else
			WGPURenderBundle pEncodedCommandBuffer = wgpuRenderBundleEncoderFinish(m_pCommandEncoder, &descriptor);
			Assert(pEncodedCommandBuffer != nullptr);
			if (LIKELY(pEncodedCommandBuffer != nullptr))
			{
				EncodedParallelCommandBuffer encodedCommandBuffer{pEncodedCommandBuffer};
#if RENDERER_WEBGPU_DAWN
				wgpuRenderBundleAddRef(pEncodedCommandBuffer);
#else
				wgpuRenderBundleReference(pEncodedCommandBuffer);
#endif
				wgpuRenderBundleEncoderRelease(m_pCommandEncoder);
				m_pCommandEncoder = nullptr;
				return Move(encodedCommandBuffer);
			}
			else
			{
				m_pCommandEncoder = nullptr;
			}
#endif
		}
#else
		Assert(false, "TODO");
#endif
		return EncodedParallelCommandBuffer{};
	}

	void ParallelRenderCommandEncoderView::SetDebugName(
		[[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name
	) const
	{
#if RENDERER_VULKAN && PROFILE_BUILD
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_COMMAND_BUFFER,
			reinterpret_cast<uint64_t>(m_pCommandEncoder),
			name
		};

#if PLATFORM_APPLE
		vkSetDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT =
			reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(logicalDevice.GetSetDebugUtilsObjectNameEXT());
		if (setDebugUtilsObjectNameEXT != nullptr)
		{
			setDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder setLabel:[NSString stringWithUTF8String:name]];

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, name]()
			{
#if RENDERER_WEBGPU_DAWN
				wgpuRenderBundleEncoderSetLabel(*pCommandEncoder, WGPUStringView{name, name.GetSize()});
#else
				wgpuRenderBundleEncoderSetLabel(*pCommandEncoder, name);
#endif
			}
		);
#else
		wgpuRenderBundleEncoderSetLabel(m_pCommandEncoder, name);
#endif
#else
		UNUSED(logicalDevice);
		UNUSED(name);
#endif
	}

	void ParallelRenderCommandEncoderView::BeginDebugMarker(
		[[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView label, [[maybe_unused]] const Math::Color color
	) const
	{
#if RENDERER_VULKAN && PROFILE_BUILD
		VkDebugUtilsLabelEXT labelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, label, {color.r, color.g, color.b, color.a}};

#if PLATFORM_APPLE
		vkCmdBeginDebugUtilsLabelEXT(m_pCommandEncoder, &labelInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkCmdBeginDebugUtilsLabelEXT beginDebugUtilsLabelEXT =
			reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(logicalDevice.GetCmdBeginDebugUtilsLabelEXT());
		if (beginDebugUtilsLabelEXT != nullptr)
		{
			beginDebugUtilsLabelEXT(m_pCommandEncoder, &labelInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder pushDebugGroup:(NSString* _Nonnull)[NSString stringWithUTF8String:label]];

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, label]()
			{
				WGPURenderBundleEncoder pWGPURenderBundleEncoder = *pCommandEncoder;
#if RENDERER_WEBGPU_DAWN
				wgpuRenderBundleEncoderPushDebugGroup(pWGPURenderBundleEncoder, WGPUStringView{label, label.GetSize()});
				wgpuRenderBundleEncoderInsertDebugMarker(pWGPURenderBundleEncoder, WGPUStringView{label, label.GetSize()});
#else
				wgpuRenderBundleEncoderPushDebugGroup(pWGPURenderBundleEncoder, label);
				wgpuRenderBundleEncoderInsertDebugMarker(pWGPURenderBundleEncoder, label);
#endif
			}
		);
#else
		wgpuRenderBundleEncoderPushDebugGroup(m_pCommandEncoder, label);
		wgpuRenderBundleEncoderInsertDebugMarker(m_pCommandEncoder, label);
#endif
#else
		UNUSED(logicalDevice);
		UNUSED(label);
#endif
	}

	void ParallelRenderCommandEncoderView::EndDebugMarker([[maybe_unused]] const LogicalDevice& logicalDevice) const
	{

#if RENDERER_VULKAN && PROFILE_BUILD

#if PLATFORM_APPLE
		vkCmdEndDebugUtilsLabelEXT(m_pCommandEncoder);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkCmdEndDebugUtilsLabelEXT endDebugUtilsLabelEXT =
			reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(logicalDevice.GetCmdEndDebugUtilsLabelEXT());
		if (endDebugUtilsLabelEXT != nullptr)
		{
			endDebugUtilsLabelEXT(m_pCommandEncoder);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder popDebugGroup];

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder]()
			{
				wgpuRenderBundleEncoderPopDebugGroup(*pCommandEncoder);
			}
		);
#else
		wgpuRenderBundleEncoderPopDebugGroup(m_pCommandEncoder);
#endif
#endif
	}

	void ParallelRenderCommandEncoderView::BindVertexBuffers(
		const ArrayView<const BufferView> buffers, const ArrayView<const uint64> offsets, const ArrayView<const uint64> sizes
	) const
	{
		Assert(buffers.GetSize() == offsets.GetSize());
		const uint32 firstBindingIndex = 0;
#if RENDERER_VULKAN
		UNUSED(sizes);
		vkCmdBindVertexBuffers(
			m_pCommandEncoder,
			firstBindingIndex,
			buffers.GetSize(),
			reinterpret_cast<const VkBuffer*>(buffers.GetData()),
			offsets.GetData()
		);
#elif RENDERER_WEBGPU
		for (const BufferView& buffer : buffers)
		{
			const uint32 index = buffers.GetIteratorIndex(&buffer);
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pCommandEncoder = m_pCommandEncoder, index = firstBindingIndex + index, buffer, offset = offsets[index], size = sizes[index]](
				) mutable
				{
					wgpuRenderBundleEncoderSetVertexBuffer(*pCommandEncoder, index, buffer, offset, size);
				}
			);
#else
			wgpuRenderBundleEncoderSetVertexBuffer(m_pCommandEncoder, firstBindingIndex + index, buffer, offsets[index], sizes[index]);
#endif
		}
#else
		UNUSED(firstBindingIndex);
		UNUSED(buffers);
		UNUSED(offsets);
		UNUSED(sizes);
		Assert(false, "TODO");
#endif
	}

	void ParallelRenderCommandEncoderView::BindDescriptorSets(
		const PipelineLayoutView pipelineLayout, const ArrayView<const DescriptorSetView, uint8> sets, const uint32 firstSetIndex
	) const
	{
		Assert(sets.All(
			[](const DescriptorSetView descriptorSet)
			{
				return descriptorSet.IsValid();
			}
		));

#if RENDERER_VULKAN
		vkCmdBindDescriptorSets(
			m_pCommandEncoder,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			firstSetIndex,
			sets.GetSize(),
			reinterpret_cast<const VkDescriptorSet*>(sets.GetData()),
			0,
			nullptr
		);
#elif RENDERER_WEBGPU
		UNUSED(pipelineLayout);

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, sets = InlineVector<DescriptorSetView, 2>(sets), firstSetIndex]()
			{
				WGPURenderBundleEncoder pWGPURenderPassEncoder = *pCommandEncoder;
				if (pWGPURenderPassEncoder != nullptr)
				{
					for (const DescriptorSetView& descriptorSet : sets)
					{
						const uint32 index = firstSetIndex + sets.GetIteratorIndex(&descriptorSet);
						Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
						Assert(pDescriptorSetData->m_bindGroup != nullptr);
						wgpuRenderBundleEncoderSetBindGroup(pWGPURenderPassEncoder, index, pDescriptorSetData->m_bindGroup, 0, nullptr);
					}
				}
			}
		);
#else
		for (const DescriptorSetView& descriptorSet : sets)
		{
			const uint32 index = firstSetIndex + sets.GetIteratorIndex(&descriptorSet);
			Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
			Assert(pDescriptorSetData->m_bindGroup != nullptr);
			wgpuRenderBundleEncoderSetBindGroup(m_pCommandEncoder, index, pDescriptorSetData->m_bindGroup, 0, nullptr);
		}
#endif
#else
		Assert(false, "TODO");
		UNUSED(pipelineLayout);
		UNUSED(sets);
		UNUSED(firstSetIndex);
#endif
	}

	void ParallelRenderCommandEncoderView::BindDynamicDescriptorSets(
		[[maybe_unused]] const PipelineLayoutView pipelineLayout,
		const ArrayView<const DescriptorSetView, uint8> sets,
		const ArrayView<const uint32, uint8> dynamicOffsets,
		const uint32 firstSetIndex
	) const
	{
		Assert(sets.All(
			[](const DescriptorSetView descriptorSet)
			{
				return descriptorSet.IsValid();
			}
		));

		Assert(dynamicOffsets.GetSize() == sets.GetSize());
#if RENDERER_VULKAN
		vkCmdBindDescriptorSets(
			m_pCommandEncoder,
			VK_PIPELINE_BIND_POINT_GRAPHICS,
			pipelineLayout,
			firstSetIndex,
			sets.GetSize(),
			reinterpret_cast<const VkDescriptorSet*>(sets.GetData()),
			dynamicOffsets.GetSize(),
			dynamicOffsets.GetData()
		);
#elif RENDERER_METAL
		Assert(false, "Not supported for Metal yet");
		UNUSED(sets);
		UNUSED(dynamicOffsets);
		UNUSED(firstSetIndex);
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder,
		   sets = InlineVector<DescriptorSetView, 2>(sets),
		   firstSetIndex,
		   dynamicOffsets = InlineVector<uint32, 2>(dynamicOffsets)]()
			{
				WGPURenderBundleEncoder pWGPURenderPassEncoder = *pCommandEncoder;
				if (pWGPURenderPassEncoder != nullptr)
				{
					for (const DescriptorSetView& descriptorSet : sets)
					{
						const uint32 index = firstSetIndex + sets.GetIteratorIndex(&descriptorSet);
						Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
						Assert(pDescriptorSetData->m_bindGroup != nullptr);
						wgpuRenderBundleEncoderSetBindGroup(pWGPURenderPassEncoder, index, pDescriptorSetData->m_bindGroup, 0, nullptr);
					}
				}
			}
		);
#else
		for (const DescriptorSetView& descriptorSet : sets)
		{
			const uint32 index = firstSetIndex + sets.GetIteratorIndex(&descriptorSet);
			Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
			Assert(pDescriptorSetData->m_bindGroup != nullptr);
			wgpuRenderBundleEncoderSetBindGroup(
				m_pCommandEncoder,
				index,
				pDescriptorSetData->m_bindGroup,
				dynamicOffsets.GetSize(),
				dynamicOffsets.GetData()
			);
		}
#endif
#else
		Assert(false, "TODO");
		UNUSED(pipelineLayout);
		UNUSED(sets);
		UNUSED(firstSetIndex);
#endif
	}

	void ParallelRenderCommandEncoderView::BindPipeline(const GraphicsPipeline& pipeline) const
	{
		Assert(pipeline.IsValid());

#if RENDERER_VULKAN
		vkCmdBindPipeline(m_pCommandEncoder, VK_PIPELINE_BIND_POINT_GRAPHICS, (GraphicsPipelineView)pipeline);
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		GraphicsPipelineView pipelineView = pipeline;
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, pipelineView]() mutable
			{
				wgpuRenderBundleEncoderSetPipeline(*pCommandEncoder, pipelineView);
			}
		);
#else
		wgpuRenderBundleEncoderSetPipeline(m_pCommandEncoder, (GraphicsPipelineView)pipeline);
#endif
#else
		Assert(false, "TODO");
		UNUSED(pipeline);
#endif
	}

	void ParallelRenderCommandEncoderView::DrawIndexed(
		const BufferView buffer,
		const uint64 offset,
		const uint64 size,
		const uint32 indexCount,
		const uint32 instanceCount,
		const uint32 firstIndex,
		const int32 vertexOffset,
		const uint32 firstInstance
	) const
	{
#if RENDERER_VULKAN
		UNUSED(size);
		constexpr VkIndexType indexType = Math::Select(TypeTraits::IsSame<Index, uint32>, VK_INDEX_TYPE_UINT32, VK_INDEX_TYPE_UINT16);
		vkCmdBindIndexBuffer(m_pCommandEncoder, buffer, offset, indexType);

		vkCmdDrawIndexed(m_pCommandEncoder, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
#elif RENDERER_WEBGPU

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, buffer, offset, size, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance](
			) mutable
			{
				WGPURenderBundleEncoder pWGPURenderBundleEncoder = *pCommandEncoder;
				constexpr WGPUIndexFormat indexFormat =
					Math::Select(TypeTraits::IsSame<Index, uint32>, WGPUIndexFormat_Uint32, WGPUIndexFormat_Uint16);
				wgpuRenderBundleEncoderSetIndexBuffer(pWGPURenderBundleEncoder, buffer, indexFormat, offset, size);

				wgpuRenderBundleEncoderDrawIndexed(pWGPURenderBundleEncoder, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
			}
		);
#else
		constexpr WGPUIndexFormat indexFormat = Math::Select(TypeTraits::IsSame<Index, uint32>, WGPUIndexFormat_Uint32, WGPUIndexFormat_Uint16);
		wgpuRenderBundleEncoderSetIndexBuffer(m_pCommandEncoder, buffer, indexFormat, offset, size);

		wgpuRenderBundleEncoderDrawIndexed(m_pCommandEncoder, indexCount, instanceCount, firstIndex, vertexOffset, firstInstance);
#endif

#else
		UNUSED(buffer);
		UNUSED(offset);
		UNUSED(size);
		UNUSED(indexCount);
		UNUSED(instanceCount);
		UNUSED(firstIndex);
		UNUSED(vertexOffset);
		UNUSED(firstInstance);
		Assert(false, "TODO");
#endif
	}

	void ParallelRenderCommandEncoderView::Draw(
		const uint32 vertexCount, const uint32 instanceCount, const uint32 firstVertex, const uint32 firstInstance
	) const
	{
#if RENDERER_VULKAN
		vkCmdDraw(m_pCommandEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, vertexCount, instanceCount, firstVertex, firstInstance]() mutable
			{
				wgpuRenderBundleEncoderDraw(*pCommandEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
			}
		);
#else
		wgpuRenderBundleEncoderDraw(m_pCommandEncoder, vertexCount, instanceCount, firstVertex, firstInstance);
#endif

#else
		UNUSED(vertexCount);
		UNUSED(instanceCount);
		UNUSED(firstVertex);
		UNUSED(firstInstance);
		Assert(false, "TODO");
#endif
	}

	void ParallelRenderCommandEncoderView::DrawIndexedIndirect(
		const BufferView buffer, uint64 offset, const uint32 drawCount, const uint32 offsetStride
	) const
	{
#if RENDERER_VULKAN
		vkCmdDrawIndexedIndirect(m_pCommandEncoder, buffer, offset, drawCount, offsetStride);
#elif RENDERER_WEBGPU

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, drawCount, buffer, offset, offsetStride]() mutable
			{
				WGPURenderBundleEncoder pWGPURenderBundleEncoder = *pCommandEncoder;
				for (uint32 drawIndex = 0; drawIndex < drawCount; ++drawIndex)
				{
					wgpuRenderBundleEncoderDrawIndexedIndirect(pWGPURenderBundleEncoder, buffer, offset);
					offset += offsetStride;
				}
			}
		);
#else
		for (uint32 drawIndex = 0; drawIndex < drawCount; ++drawIndex)
		{
			wgpuRenderBundleEncoderDrawIndexedIndirect(m_pCommandEncoder, buffer, offset);
			offset += offsetStride;
		}
#endif

#else
		UNUSED(buffer);
		UNUSED(offset);
		UNUSED(drawCount);
		UNUSED(offsetStride);
		Assert(false, "TODO");
#endif
	}

	void ParallelRenderCommandEncoderView::DrawIndirect(
		const BufferView buffer, uint64 offset, const uint32 drawCount, const uint32 offsetStride
	) const
	{
#if RENDERER_VULKAN
		vkCmdDrawIndirect(m_pCommandEncoder, buffer, offset, drawCount, offsetStride);
#elif RENDERER_WEBGPU

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, drawCount, buffer, offset, offsetStride]() mutable
			{
				WGPURenderBundleEncoder pWGPURenderBundleEncoder = *pCommandEncoder;
				for (uint32 drawIndex = 0; drawIndex < drawCount; ++drawIndex)
				{
					wgpuRenderBundleEncoderDrawIndirect(pWGPURenderBundleEncoder, buffer, offset);
					offset += offsetStride;
				}
			}
		);
#else
		for (uint32 drawIndex = 0; drawIndex < drawCount; ++drawIndex)
		{
			wgpuRenderBundleEncoderDrawIndirect(m_pCommandEncoder, buffer, offset);
			offset += offsetStride;
		}
#endif

#else
		UNUSED(buffer);
		UNUSED(offset);
		UNUSED(drawCount);
		UNUSED(offsetStride);
		Assert(false, "TODO");
#endif
	}

	void ParallelRenderCommandEncoderView::SetDepthBias(
		const float depthBiasConstantFactor, const float depthBiasClamp, const float depthBiasSlopeFactor
	) const
	{
#if RENDERER_VULKAN
		vkCmdSetDepthBias(m_pCommandEncoder, depthBiasConstantFactor, depthBiasClamp, depthBiasSlopeFactor);
#elif RENDERER_WEBGPU
		Assert(false, "Unsupported!");
		UNUSED(depthBiasConstantFactor);
		UNUSED(depthBiasClamp);
		UNUSED(depthBiasSlopeFactor);
#else
		UNUSED(depthBiasConstantFactor);
		UNUSED(depthBiasClamp);
		UNUSED(depthBiasSlopeFactor);
		Assert(false, "TODO");
#endif
	}

	void ParallelRenderCommandEncoderView::StartNextSubpass() const
	{
#if RENDERER_VULKAN
		vkCmdNextSubpass(m_pCommandEncoder, VK_SUBPASS_CONTENTS_INLINE);
#else
		Assert(false, "TODO");
#endif
	}

	ComputeCommandEncoder CommandEncoderView::BeginCompute(
		[[maybe_unused]] LogicalDevice& logicalDevice, [[maybe_unused]] const uint32 maximumPushConstantInstanceCount
	) const
	{
#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		if (maximumPushConstantInstanceCount > 0)
		{
			logicalDevice.ReservePushConstantsInstanceCount(maximumPushConstantInstanceCount);
		}
#endif

#if RENDERER_VULKAN
		return ComputeCommandEncoder{m_pCommandEncoder};
#elif RENDERER_METAL
		MTLComputePassDescriptor* computePassDescriptor = [[MTLComputePassDescriptor alloc] init];
		computePassDescriptor.dispatchType = MTLDispatchTypeConcurrent;
		return ComputeCommandEncoder{[m_pCommandEncoder computeCommandEncoderWithDescriptor:computePassDescriptor]};
#elif RENDERER_WEBGPU

		WGPUComputePassDescriptor computePassDescriptor
		{
			nullptr,
#if RENDERER_WEBGPU_DAWN
				WGPUStringView{nullptr, 0},
#else
				nullptr,
#endif
				nullptr
		};
#if WEBGPU_SINGLE_THREADED
		WGPUComputePassEncoder* pComputePassEncoder = new WGPUComputePassEncoder{nullptr};
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, computePassDescriptor, pComputePassEncoder]() mutable
			{
				WGPUComputePassEncoder pWGPUComputePassEncoder = wgpuCommandEncoderBeginComputePass(*pCommandEncoder, &computePassDescriptor);
#if RENDERER_WEBGPU_DAWN
				wgpuComputePassEncoderAddRef(pWGPUComputePassEncoder);
#else
				wgpuComputePassEncoderReference(pWGPUComputePassEncoder);
#endif
				*pComputePassEncoder = pWGPUComputePassEncoder;
			}
		);
		return ComputeCommandEncoder{pComputePassEncoder};
#else
		WGPUComputePassEncoder pComputeEncoder = wgpuCommandEncoderBeginComputePass(m_pCommandEncoder, &computePassDescriptor);
#if RENDERER_WEBGPU_DAWN
		wgpuComputePassEncoderAddRef(pComputeEncoder);
#else
		wgpuComputePassEncoderReference(pComputeEncoder);
#endif
		return ComputeCommandEncoder{pComputeEncoder};
#endif

#else
		Assert(false, "TODO");
		return {};
#endif
	}

	ComputeCommandEncoder::~ComputeCommandEncoder()
	{
#if RENDERER_METAL
		if (m_pCommandEncoder != nullptr)
		{
			[m_pCommandEncoder endEncoding];
		}
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder]()
			{
				WGPUComputePassEncoder pWGPUComputePassEncoder = *pCommandEncoder;
				if (pWGPUComputePassEncoder != nullptr)
				{
					wgpuComputePassEncoderEnd(pWGPUComputePassEncoder);
					wgpuComputePassEncoderRelease(pWGPUComputePassEncoder);
				}
				delete pCommandEncoder;
			}
		);
#else
		wgpuComputePassEncoderEnd(m_pCommandEncoder);
		wgpuComputePassEncoderRelease(m_pCommandEncoder);
#endif
#endif
	}

	ComputeCommandEncoder& ComputeCommandEncoder::operator=(ComputeCommandEncoder&& other) noexcept
	{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
		Assert(m_pCommandEncoder == 0);
		m_pCommandEncoder = other.m_pCommandEncoder;
		other.m_pCommandEncoder = 0;
#endif
		return *this;
	}

	void ComputeCommandEncoder::End()
	{
#if RENDERER_VULKAN
		m_pCommandEncoder = 0;
#elif RENDERER_METAL
		[m_pCommandEncoder endEncoding];
		m_pCommandEncoder = nullptr;
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder]()
			{
				WGPUComputePassEncoder pWGPUComputePassEncoder = *pCommandEncoder;
				if (pWGPUComputePassEncoder != nullptr)
				{
					wgpuComputePassEncoderEnd(pWGPUComputePassEncoder);
					wgpuComputePassEncoderRelease(pWGPUComputePassEncoder);
				}
				delete pCommandEncoder;
			}
		);
#else
		wgpuComputePassEncoderEnd(m_pCommandEncoder);
		wgpuComputePassEncoderRelease(m_pCommandEncoder);
#endif
		m_pCommandEncoder = nullptr;
#endif
	}

	void ComputeCommandEncoderView::BindDescriptorSets(
		[[maybe_unused]] const PipelineLayoutView pipelineLayout,
		const ArrayView<const DescriptorSetView, uint8> sets,
		const uint32 firstSetIndex
	) const
	{
		Assert(sets.All(
			[](const DescriptorSetView descriptorSet)
			{
				return descriptorSet.IsValid();
			}
		));

#if RENDERER_VULKAN
		vkCmdBindDescriptorSets(
			m_pCommandEncoder,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			pipelineLayout,
			firstSetIndex,
			sets.GetSize(),
			reinterpret_cast<const VkDescriptorSet*>(sets.GetData()),
			0,
			nullptr
		);
#elif RENDERER_METAL
		const Internal::PipelineLayoutData* __restrict pPipelineLayoutData = pipelineLayout;

		uint8 descriptorIndices{0};
		for (const DescriptorSetLayoutView skippedDescriptorSetLayout :
		     pPipelineLayoutData->m_descriptorSetLayouts.GetSubView(0u, (uint8)firstSetIndex))
		{
			const Internal::DescriptorSetLayoutData* __restrict pDescriptorSetLayoutData = skippedDescriptorSetLayout;
			descriptorIndices += pDescriptorSetLayoutData->m_stages.IsSet(ShaderStage::Compute);
		}

		for (const DescriptorSetView& descriptorSet : sets)
		{
			const Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
			const Internal::DescriptorSetLayoutData* __restrict pDescriptorSetLayoutData = pDescriptorSetData->m_layout;

			if (pDescriptorSetLayoutData->m_argumentBuffers)
			{
				[m_pCommandEncoder setBuffer:pDescriptorSetData->m_argumentBuffer offset:0 atIndex:descriptorIndices++];
			}
			else
			{
				for (const Internal::DescriptorSetLayoutData::Binding& __restrict descriptorSetBinding : pDescriptorSetLayoutData->m_bindings)
				{
					if (descriptorSetBinding.m_stages.IsSet(ShaderStage::Compute))
					{
						switch (descriptorSetBinding.m_type)
						{
							case DescriptorType::Sampler:
							{
								[m_pCommandEncoder setSamplerState:(id<MTLSamplerState>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																					 atIndex:descriptorIndices++];
							}
							break;
							case DescriptorType::SampledImage:
							case DescriptorType::InputAttachment:
							case DescriptorType::UniformTexelBuffer:
							case DescriptorType::StorageImage:
							case DescriptorType::StorageTexelBuffer:
							{
								[m_pCommandEncoder setTexture:(id<MTLTexture>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																			atIndex:descriptorIndices++];
							}
							break;
							case DescriptorType::UniformBuffer:
							case DescriptorType::UniformBufferDynamic:
							case DescriptorType::StorageBuffer:
							case DescriptorType::StorageBufferDynamic:
							{
								[m_pCommandEncoder setBuffer:(id<MTLBuffer>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																			offset:0
																		 atIndex:descriptorIndices++];
							}
							break;
							case DescriptorType::AccelerationStructure:
							{
								[m_pCommandEncoder
									setAccelerationStructure:(id<MTLAccelerationStructure>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
														 atBufferIndex:descriptorIndices++];
							}
							break;
							case DescriptorType::CombinedImageSampler:
							{
								[m_pCommandEncoder setTexture:(id<MTLTexture>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex]
																			atIndex:descriptorIndices++];
								[m_pCommandEncoder setSamplerState:(id<MTLSamplerState>)pDescriptorSetData->m_objects[descriptorSetBinding.m_baseIndex + 1]
																					 atIndex:descriptorIndices++];
							}
							break;
						}
					}
				}
			}

			for (const Internal::DescriptorSetData::ResourceBatch& __restrict resourceBatch : pDescriptorSetData->m_resourceBatches)
			{
				const uint8 resourceBatchIndex = pDescriptorSetData->m_resourceBatches.GetIteratorIndex(&resourceBatch);
				const Internal::DescriptorSetLayoutData::ResourceBatch& __restrict batch =
					pDescriptorSetLayoutData->m_resourceBatches[resourceBatchIndex];

				[m_pCommandEncoder useResources:resourceBatch.m_resources.GetData() count:resourceBatch.m_resources.GetSize() usage:batch.m_usage];
			}
		}
#elif RENDERER_WEBGPU
		UNUSED(pipelineLayout);
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, sets = InlineVector<DescriptorSetView, 2>(sets), firstSetIndex]()
			{
				WGPUComputePassEncoder pWGPURenderPassEncoder = *pCommandEncoder;
				if (pWGPURenderPassEncoder != nullptr)
				{
					for (const DescriptorSetView& descriptorSet : sets)
					{
						const uint32 index = firstSetIndex + sets.GetIteratorIndex(&descriptorSet);
						Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
						Assert(pDescriptorSetData->m_bindGroup != nullptr);
						wgpuComputePassEncoderSetBindGroup(pWGPURenderPassEncoder, index, pDescriptorSetData->m_bindGroup, 0, nullptr);
					}
				}
			}
		);
#else
		for (const DescriptorSetView& descriptorSet : sets)
		{
			const uint32 index = firstSetIndex + sets.GetIteratorIndex(&descriptorSet);
			Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
			Assert(pDescriptorSetData->m_bindGroup != nullptr);
			wgpuComputePassEncoderSetBindGroup(m_pCommandEncoder, index, pDescriptorSetData->m_bindGroup, 0, nullptr);
		}
#endif
#else
		Assert(false, "TODO");
		UNUSED(pipelineLayout);
		UNUSED(sets);
		UNUSED(firstSetIndex);
#endif
	}

	void ComputeCommandEncoderView::BindDynamicDescriptorSets(
		[[maybe_unused]] const PipelineLayoutView pipelineLayout,
		const ArrayView<const DescriptorSetView, uint8> sets,
		const ArrayView<const uint32, uint8> dynamicOffsets,
		const uint32 firstSetIndex
	) const
	{
		Assert(sets.All(
			[](const DescriptorSetView descriptorSet)
			{
				return descriptorSet.IsValid();
			}
		));

		Assert(dynamicOffsets.GetSize() == sets.GetSize());
#if RENDERER_VULKAN
		vkCmdBindDescriptorSets(
			m_pCommandEncoder,
			VK_PIPELINE_BIND_POINT_COMPUTE,
			pipelineLayout,
			firstSetIndex,
			sets.GetSize(),
			reinterpret_cast<const VkDescriptorSet*>(sets.GetData()),
			dynamicOffsets.GetSize(),
			dynamicOffsets.GetData()
		);
#elif RENDERER_METAL
		Assert(false, "Not supported for Metal yet");
		UNUSED(sets);
		UNUSED(dynamicOffsets);
		UNUSED(firstSetIndex);
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder,
		   sets = InlineVector<DescriptorSetView, 2>(sets),
		   firstSetIndex,
		   dynamicOffsets = InlineVector<uint32, 2>(dynamicOffsets)]()
			{
				WGPUComputePassEncoder pWGPURenderPassEncoder = *pCommandEncoder;
				if (pWGPURenderPassEncoder != nullptr)
				{
					for (const DescriptorSetView& descriptorSet : sets)
					{
						const uint32 index = firstSetIndex + sets.GetIteratorIndex(&descriptorSet);
						Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
						Assert(pDescriptorSetData->m_bindGroup != nullptr);
						wgpuComputePassEncoderSetBindGroup(
							pWGPURenderPassEncoder,
							index,
							pDescriptorSetData->m_bindGroup,
							dynamicOffsets.GetSize(),
							dynamicOffsets.GetData()
						);
					}
				}
			}
		);
#else
		for (const DescriptorSetView& descriptorSet : sets)
		{
			const uint32 index = firstSetIndex + sets.GetIteratorIndex(&descriptorSet);
			Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
			Assert(pDescriptorSetData->m_bindGroup != nullptr);
			wgpuComputePassEncoderSetBindGroup(
				m_pCommandEncoder,
				index,
				pDescriptorSetData->m_bindGroup,
				dynamicOffsets.GetSize(),
				dynamicOffsets.GetData()
			);
		}
#endif
#else
		Assert(false, "TODO");
		UNUSED(pipelineLayout);
		UNUSED(sets);
		UNUSED(firstSetIndex);
#endif
	}

	void ComputeCommandEncoderView::BindPipeline(const ComputePipeline& pipeline) const
	{
		Assert(pipeline.IsValid());

#if RENDERER_VULKAN
		vkCmdBindPipeline(m_pCommandEncoder, VK_PIPELINE_BIND_POINT_COMPUTE, (ComputePipelineView)pipeline);
#elif RENDERER_METAL
		[m_pCommandEncoder setComputePipelineState:(ComputePipelineView)pipeline];
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		ComputePipelineView pipelineView = pipeline;
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, pipelineView]()
			{
				wgpuComputePassEncoderSetPipeline(*pCommandEncoder, pipelineView);
			}
		);
#else
		wgpuComputePassEncoderSetPipeline(m_pCommandEncoder, (ComputePipelineView)pipeline);
#endif

#else
		Assert(false, "TODO");
		UNUSED(pipeline);
#endif
	}

	void ComputeCommandEncoderView::Dispatch(const Math::Vector3ui groupCount, [[maybe_unused]] const Math::Vector3ui groupSize) const
	{
#if RENDERER_VULKAN
		vkCmdDispatch(m_pCommandEncoder, groupCount.x, groupCount.y, groupCount.z);
#elif RENDERER_METAL
		[m_pCommandEncoder dispatchThreadgroups:MTLSizeMake(groupCount.x, groupCount.y, groupCount.z)
											threadsPerThreadgroup:MTLSizeMake(groupSize.x, groupSize.y, groupSize.z)];
#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, groupCount]()
			{
				wgpuComputePassEncoderDispatchWorkgroups(*pCommandEncoder, groupCount.x, groupCount.y, groupCount.z);
			}
		);
#else
		wgpuComputePassEncoderDispatchWorkgroups(m_pCommandEncoder, groupCount.x, groupCount.y, groupCount.z);
#endif

#else
		UNUSED(groupCount);
		Assert(false, "TODO");
#endif
	}

	void ComputeCommandEncoderView::SetDebugName(
		[[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name
	) const
	{
#if RENDERER_VULKAN && PROFILE_BUILD
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_COMMAND_BUFFER,
			reinterpret_cast<uint64_t>(m_pCommandEncoder),
			name
		};

#if PLATFORM_APPLE
		vkSetDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT =
			reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(logicalDevice.GetSetDebugUtilsObjectNameEXT());
		if (setDebugUtilsObjectNameEXT != nullptr)
		{
			setDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder setLabel:[NSString stringWithUTF8String:name]];

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, name]()
			{
#if RENDERER_WEBGPU_DAWN
				wgpuComputePassEncoderSetLabel(*pCommandEncoder, WGPUStringView{name, name.GetSize()});
#else
				wgpuComputePassEncoderSetLabel(*pCommandEncoder, name);
#endif
			}
		);
#else
		wgpuComputePassEncoderSetLabel(m_pCommandEncoder, name);
#endif
#else
		UNUSED(logicalDevice);
		UNUSED(name);
#endif
	}

	void ComputeCommandEncoderView::BeginDebugMarker(
		[[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView label, [[maybe_unused]] const Math::Color color
	) const
	{
#if RENDERER_VULKAN && PROFILE_BUILD
		VkDebugUtilsLabelEXT labelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, label, {color.r, color.g, color.b, color.a}};

#if PLATFORM_APPLE
		vkCmdBeginDebugUtilsLabelEXT(m_pCommandEncoder, &labelInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkCmdBeginDebugUtilsLabelEXT beginDebugUtilsLabelEXT =
			reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(logicalDevice.GetCmdBeginDebugUtilsLabelEXT());
		if (beginDebugUtilsLabelEXT != nullptr)
		{
			beginDebugUtilsLabelEXT(m_pCommandEncoder, &labelInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder pushDebugGroup:(NSString* _Nonnull)[NSString stringWithUTF8String:label]];

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder, label]()
			{
				WGPUComputePassEncoder pWGPUComputePassEncoder = *pCommandEncoder;
#if RENDERER_WEBGPU_DAWN
				wgpuComputePassEncoderPushDebugGroup(pWGPUComputePassEncoder, WGPUStringView{label, label.GetSize()});
				wgpuComputePassEncoderInsertDebugMarker(pWGPUComputePassEncoder, WGPUStringView{label, label.GetSize()});
#else
				wgpuComputePassEncoderPushDebugGroup(pWGPUComputePassEncoder, label);
				wgpuComputePassEncoderInsertDebugMarker(pWGPUComputePassEncoder, label);
#endif
			}
		);
#else
		wgpuComputePassEncoderPushDebugGroup(m_pCommandEncoder, label);
		wgpuComputePassEncoderInsertDebugMarker(m_pCommandEncoder, label);
#endif
#else
		UNUSED(logicalDevice);
		UNUSED(label);
#endif
	}

	void ComputeCommandEncoderView::EndDebugMarker([[maybe_unused]] const LogicalDevice& logicalDevice) const
	{
#if RENDERER_VULKAN && PROFILE_BUILD

#if PLATFORM_APPLE
		vkCmdEndDebugUtilsLabelEXT(m_pCommandEncoder);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkCmdEndDebugUtilsLabelEXT endDebugUtilsLabelEXT =
			reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(logicalDevice.GetCmdEndDebugUtilsLabelEXT());
		if (endDebugUtilsLabelEXT != nullptr)
		{
			endDebugUtilsLabelEXT(m_pCommandEncoder);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder popDebugGroup];

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pCommandEncoder = m_pCommandEncoder]()
			{
				wgpuComputePassEncoderPopDebugGroup(*pCommandEncoder);
			}
		);
#else
		wgpuComputePassEncoderPopDebugGroup(m_pCommandEncoder);
#endif
#endif
	}

	BlitCommandEncoder CommandEncoderView::BeginBlit() const
	{
#if RENDERER_VULKAN
		return BlitCommandEncoder{m_pCommandEncoder};
#elif RENDERER_METAL
		MTLBlitPassDescriptor* blitPassDescriptor = [[MTLBlitPassDescriptor alloc] init];
		id<MTLBlitCommandEncoder> blitEncoder = [m_pCommandEncoder blitCommandEncoderWithDescriptor:blitPassDescriptor];
		return BlitCommandEncoder{blitEncoder};
#elif RENDERER_WEBGPU
		return BlitCommandEncoder{m_pCommandEncoder};
#else
		Assert(false, "TODO");
		return {};
#endif
	}

	BlitCommandEncoder::~BlitCommandEncoder()
	{
#if RENDERER_METAL
		if (m_pCommandEncoder != nullptr)
		{
			[m_pCommandEncoder endEncoding];
		}
#endif
	}

	BlitCommandEncoder& BlitCommandEncoder::operator=(BlitCommandEncoder&& other) noexcept
	{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
		Assert(m_pCommandEncoder == 0);
		m_pCommandEncoder = other.m_pCommandEncoder;
		other.m_pCommandEncoder = 0;
#endif
		return *this;
	}

	void BlitCommandEncoder::End()
	{
#if RENDERER_VULKAN || RENDERER_WEBGPU
		m_pCommandEncoder = 0;
#elif RENDERER_METAL
		[m_pCommandEncoder endEncoding];
		m_pCommandEncoder = 0;
#endif
	}

	void
	BlitCommandEncoderView::SetDebugName([[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name) const
	{
#if RENDERER_VULKAN
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_COMMAND_BUFFER,
			reinterpret_cast<uint64_t>(m_pCommandEncoder),
			name
		};

#if PLATFORM_APPLE
		vkSetDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT =
			reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(logicalDevice.GetSetDebugUtilsObjectNameEXT());
		if (setDebugUtilsObjectNameEXT != nullptr)
		{
			setDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder setLabel:[NSString stringWithUTF8String:name]];

#else
		UNUSED(logicalDevice);
		UNUSED(name);
#endif
	}

	void BlitCommandEncoderView::BeginDebugMarker(
		[[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView label, [[maybe_unused]] const Math::Color color
	) const
	{
#if RENDERER_VULKAN && PROFILE_BUILD
		VkDebugUtilsLabelEXT labelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, label, {color.r, color.g, color.b, color.a}};

#if PLATFORM_APPLE
		vkCmdBeginDebugUtilsLabelEXT(m_pCommandEncoder, &labelInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkCmdBeginDebugUtilsLabelEXT beginDebugUtilsLabelEXT =
			reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(logicalDevice.GetCmdBeginDebugUtilsLabelEXT());
		if (beginDebugUtilsLabelEXT != nullptr)
		{
			beginDebugUtilsLabelEXT(m_pCommandEncoder, &labelInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder pushDebugGroup:(NSString* _Nonnull)[NSString stringWithUTF8String:label]];

#else
		UNUSED(logicalDevice);
		UNUSED(label);
#endif
	}

	void BlitCommandEncoderView::EndDebugMarker([[maybe_unused]] const LogicalDevice& logicalDevice) const
	{
#if RENDERER_VULKAN && PROFILE_BUILD

#if PLATFORM_APPLE
		vkCmdEndDebugUtilsLabelEXT(m_pCommandEncoder);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkCmdEndDebugUtilsLabelEXT endDebugUtilsLabelEXT =
			reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(logicalDevice.GetCmdEndDebugUtilsLabelEXT());
		if (endDebugUtilsLabelEXT != nullptr)
		{
			endDebugUtilsLabelEXT(m_pCommandEncoder);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder popDebugGroup];

#endif
	}

	void BlitCommandEncoderView::RecordCopyBufferToBuffer(
		const BufferView sourceBuffer, const BufferView targetBuffer, const ArrayView<const BufferCopy, uint16> regions
	) const
	{
		Assert(sourceBuffer.IsValid());
		Assert(targetBuffer.IsValid());
		Assert(
			sourceBuffer != targetBuffer || regions.All(
																				[](const BufferCopy& copy)
																				{
																					return !Math::Range<uint64>::Make(copy.m_sourceOffset, copy.m_size)
			                                              .Overlaps(Math::Range<uint64>::Make(copy.m_destinationOffset, copy.m_size));
																				}
																			),
			"Copies must not overlap!"
		);

#if RENDERER_VULKAN
		static_assert(sizeof(BufferCopy) == sizeof(VkBufferCopy));
		static_assert(alignof(BufferCopy) == alignof(VkBufferCopy));
		vkCmdCopyBuffer(
			m_pCommandEncoder,
			sourceBuffer,
			targetBuffer,
			regions.GetSize(),
			reinterpret_cast<const VkBufferCopy*>(regions.GetData())
		);

#elif RENDERER_WEBGPU
		Assert(sourceBuffer != targetBuffer, "WebGPU does not support copying regions within the same buffer");
		static auto copyBufferToBuffer = [](
																			 const BlitCommandEncoderView commandEncoder,
																			 const BufferView sourceBuffer,
																			 const BufferView targetBuffer,
																			 const BufferCopy& __restrict region
																		 )
		{
			wgpuCommandEncoderCopyBufferToBuffer(
				commandEncoder,
				sourceBuffer,
				region.m_sourceOffset,
				targetBuffer,
				region.m_destinationOffset,
				region.m_size
			);
		};

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[commandEncoder = *this, sourceBuffer, targetBuffer, regions = InlineVector<BufferCopy, 1>{regions}]()
			{
				for (const BufferCopy& __restrict region : regions)
				{
					copyBufferToBuffer(commandEncoder, sourceBuffer, targetBuffer, region);
				}
			}
		);
#else
		for (const BufferCopy& __restrict region : regions)
		{
			copyBufferToBuffer(*this, sourceBuffer, targetBuffer, region);
		}
#endif
#elif RENDERER_METAL
		for (const BufferCopy& __restrict region : regions)
		{
			[m_pCommandEncoder copyFromBuffer:sourceBuffer
													 sourceOffset:region.m_sourceOffset
															 toBuffer:targetBuffer
											destinationOffset:region.m_destinationOffset
																	 size:region.m_size];
		}
#else
		Assert(false, "TODO");
		UNUSED(targetBuffer);
		UNUSED(sourceBuffer);
		UNUSED(regions);
#endif
	}

	void BlitCommandEncoderView::RecordCopyDataToBuffer(
		LogicalDevice& logicalDevice,
		[[maybe_unused]] const QueueFamily queueFamily,
		const ArrayView<const DataToBufferBatch, uint16> batches,
		[[maybe_unused]] Optional<StagingBuffer>& stagingBufferOut
	) const
	{
		Assert(batches.All(
			[](const DataToBufferBatch& batch)
			{
				return batch.targetBuffer.IsValid();
			}
		));

#if RENDERER_METAL || RENDERER_VULKAN
		const size totalSize = batches.Count(
			[](const DataToBufferBatch& __restrict batch)
			{
				return batch.regions.Count(
					[](const DataToBuffer& __restrict region)
					{
						return region.source.GetDataSize();
					}
				);
			}
		);

		if (stagingBufferOut.IsInvalid())
		{
			stagingBufferOut = StagingBuffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				totalSize,
				StagingBuffer::Flags::TransferSource
			);
		}

		stagingBufferOut->MapToHostMemory(
			logicalDevice,
			Math::Range<size>::Make(0, totalSize),
			Rendering::Buffer::MapMemoryFlags::Write,
			[batches](const Rendering::Buffer::MapMemoryStatus status, ByteView data, [[maybe_unused]] const bool executedAsynchronously)
			{
				Assert(status == Rendering::Buffer::MapMemoryStatus::Success);
				if (LIKELY(status == Rendering::Buffer::MapMemoryStatus::Success))
				{
					for (const DataToBufferBatch& __restrict batch : batches)
					{
						for (const DataToBuffer& __restrict region : batch.regions)
						{
							data.CopyFrom(region.source);
							data += region.source.GetDataSize();
						}
					}
				}
			}
		);

		const uint16 totalCopies = batches.Count(
			[](const DataToBufferBatch& __restrict batch)
			{
				return batch.regions.GetSize();
			}
		);

		InlineVector<BufferCopy, 6> copies;
		copies.Reserve(totalCopies);
		size stagingBufferOffset{0};
		for (const DataToBufferBatch& __restrict batch : batches)
		{
			for (const DataToBuffer& __restrict region : batch.regions)
			{
				copies.EmplaceBack(BufferCopy{stagingBufferOffset, region.targetOffset, region.source.GetDataSize()});
				stagingBufferOffset += region.source.GetDataSize();
			}

			RecordCopyBufferToBuffer(*stagingBufferOut, batch.targetBuffer, copies.GetView());

			copies.Clear();
		}
#elif RENDERER_WEBGPU
		const CommandQueueView commandQueue = logicalDevice.GetCommandQueue(queueFamily);
#if WEBGPU_SINGLE_THREADED
		InlineVector<DataToBufferBatch, 1> copiedBatches(Memory::Reserve, batches.GetSize());

		const uint16 totalRegionCount = batches.Count(
			[](const DataToBufferBatch& batch)
			{
				return batch.regions.GetSize();
			}
		);

		Vector<DataToBuffer> copiedRegions(Memory::Reserve, totalRegionCount);
		for (const DataToBufferBatch& batch : batches)
		{
			copiedBatches.EmplaceBack(DataToBufferBatch{batch.targetBuffer, copiedRegions.CopyEmplaceRangeBack(batch.regions)});
		}

		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[commandQueue, batches = Move(copiedBatches), regions = Move(copiedRegions)]()
			{
				for (const DataToBufferBatch& __restrict batch : batches)
				{
					for (const DataToBuffer& __restrict region : batch.regions)
					{
						wgpuQueueWriteBuffer(
							commandQueue,
							batch.targetBuffer,
							region.targetOffset,
							region.source.GetData(),
							region.source.GetDataSize()
						);
					}
				}
			}
		);
#else
		for (const DataToBufferBatch& __restrict batch : batches)
		{
			for (const DataToBuffer& __restrict region : batch.regions)
			{
				wgpuQueueWriteBuffer(commandQueue, batch.targetBuffer, region.targetOffset, region.source.GetData(), region.source.GetDataSize());
			}
		}
#endif
#else
		static_assert(false, "Not implemented for renderer!");
#endif
	}

	void BlitCommandEncoderView::RecordCopyBufferToImage(
		const BufferView sourceBuffer,
		const RenderTexture& targetImage,
		[[maybe_unused]] const ImageLayout targetImageLayout,
		const ArrayView<const BufferImageCopy, uint16> regions
	) const
	{
		Assert(sourceBuffer.IsValid());
		Assert(targetImage.IsValid());

		Assert(targetImageLayout == ImageLayout::TransferDestinationOptimal, "Invalid target image layout for copy from buffer");
		if constexpr (ENABLE_ASSERTS)
		{
			const SubresourceStatesBase& subresourceStates = targetImage.GetSubresourceStates();
			const ImageSubresourceRange fullSubresourceRange{targetImage.GetTotalSubresourceRange()};
			for ([[maybe_unused]] const BufferImageCopy& region : regions)
			{
				[[maybe_unused]] const SubresourceState subresourceState = *subresourceStates.GetUniformSubresourceState(
					ImageSubresourceRange{
						region.m_imageSubresource.aspectMask,
						MipRange{region.m_imageSubresource.mipLevel, 1},
						region.m_imageSubresource.arrayLayerRanges
					},
					fullSubresourceRange,
					0
				);

				Assert(subresourceState.m_imageLayout == targetImageLayout, "Image was not in layout for copy from buffer");
			}
		}

#if RENDERER_VULKAN
		static_assert(sizeof(BufferImageCopy) == sizeof(VkBufferImageCopy));
		static_assert(alignof(BufferImageCopy) == alignof(VkBufferImageCopy));
		static_assert(sizeof(ImageSubresourceRange) == sizeof(VkImageSubresourceRange));
		static_assert(alignof(ImageSubresourceRange) == alignof(VkImageSubresourceRange));

		vkCmdCopyBufferToImage(
			m_pCommandEncoder,
			sourceBuffer,
			targetImage,
			static_cast<VkImageLayout>(targetImageLayout),
			regions.GetSize(),
			reinterpret_cast<const VkBufferImageCopy*>(regions.GetData())
		);

#elif RENDERER_WEBGPU
		static auto copyBufferToImage = [](
																			const BlitCommandEncoderView commandEncoder,
																			const BufferView sourceBuffer,
																			const ImageView targetImage,
																			const BufferImageCopy& __restrict region
																		)
		{
			const WGPUImageCopyBuffer source
			{
#if !RENDERER_WEBGPU_DAWN
				nullptr,
#endif
					WGPUTextureDataLayout{nullptr, region.m_bufferOffset, region.m_bufferBytesPerDimension.x, region.m_blockCountPerDimension.y},
					sourceBuffer,
			};
			const WGPUImageCopyTexture target
			{
#if !RENDERER_WEBGPU_DAWN
				nullptr,
#endif
					targetImage, region.m_imageSubresource.mipLevel,
					WGPUOrigin3D{
						(uint32)region.m_imageOffset.x,
						(uint32)region.m_imageOffset.y,
						(uint32)region.m_imageOffset.z + region.m_imageSubresource.arrayLayerRanges.GetIndex()
					},
					ConvertImageAspectFlags(region.m_imageSubresource.aspectMask),
			};
			const WGPUExtent3D extent{
				region.m_imageExtent.x,
				region.m_imageExtent.y,
				Math::Max(region.m_imageExtent.z, region.m_imageSubresource.arrayLayerRanges.GetCount())
			};

			wgpuCommandEncoderCopyBufferToTexture(commandEncoder, &source, &target, &extent);
		};

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[commandEncoder = *this, sourceBuffer, targetImage = (ImageView)targetImage, regions = InlineVector<BufferImageCopy, 1>{regions}]()
			{
				for (const BufferImageCopy& __restrict region : regions)
				{
					copyBufferToImage(commandEncoder, sourceBuffer, targetImage, region);
				}
			}
		);
#else
		for (const BufferImageCopy& __restrict region : regions)
		{
			copyBufferToImage(*this, sourceBuffer, targetImage, region);
		}
#endif
#elif RENDERER_METAL
		for (const BufferImageCopy& __restrict region : regions)
		{
			for (const uint32 targetLayerIndex : region.m_imageSubresource.arrayLayerRanges)
			{
				[m_pCommandEncoder copyFromBuffer:sourceBuffer
														 sourceOffset:region.m_bufferOffset
												sourceBytesPerRow:region.m_bufferBytesPerDimension.x
											sourceBytesPerImage:region.m_bufferBytesPerDimension.x * region.m_bufferBytesPerDimension.y
															 sourceSize:MTLSizeMake(region.m_imageExtent.x, region.m_imageExtent.y, region.m_imageExtent.z)
																toTexture:targetImage
												 destinationSlice:targetLayerIndex
												 destinationLevel:region.m_imageSubresource.mipLevel
												destinationOrigin:MTLOriginMake(region.m_imageOffset.x, region.m_imageOffset.y, region.m_imageOffset.z)];
			}
		}
#else
		UNUSED(sourceBuffer);
		UNUSED(targetImage);
		UNUSED(regions);
		Assert(false, "TODO");
#endif
	}

	void BlitCommandEncoderView::RecordCopyImageToBuffer(
		const RenderTexture& sourceImage,
		[[maybe_unused]] const ImageLayout sourceImageLayout,
		const BufferView targetBuffer,
		const ArrayView<const BufferImageCopy, uint16> regions
	) const
	{
		Assert(sourceImage.IsValid());
		Assert(targetBuffer.IsValid());

		Assert(sourceImageLayout == ImageLayout::TransferSourceOptimal, "Invalid source image layout for copy to buffer");
		if constexpr (ENABLE_ASSERTS)
		{
			const SubresourceStatesBase& subresourceStates = sourceImage.GetSubresourceStates();
			const ImageSubresourceRange fullSubresourceRange{sourceImage.GetTotalSubresourceRange()};
			for ([[maybe_unused]] const BufferImageCopy& region : regions)
			{
				[[maybe_unused]] const SubresourceState subresourceState = *subresourceStates.GetUniformSubresourceState(
					ImageSubresourceRange{
						region.m_imageSubresource.aspectMask,
						MipRange{region.m_imageSubresource.mipLevel, 1},
						region.m_imageSubresource.arrayLayerRanges
					},
					fullSubresourceRange,
					0
				);

				Assert(subresourceState.m_imageLayout == sourceImageLayout, "Image was not in layout for copy to buffer");
			}
		}

#if RENDERER_VULKAN
		vkCmdCopyImageToBuffer(
			m_pCommandEncoder,
			sourceImage,
			static_cast<VkImageLayout>(sourceImageLayout),
			targetBuffer,
			regions.GetSize(),
			reinterpret_cast<const VkBufferImageCopy*>(regions.GetData())
		);

#elif RENDERER_METAL
		for (const BufferImageCopy& __restrict region : regions)
		{
			for (const uint32 sourceLayerIndex : region.m_imageSubresource.arrayLayerRanges)
			{
				[m_pCommandEncoder copyFromTexture:sourceImage
															 sourceSlice:sourceLayerIndex
															 sourceLevel:region.m_imageSubresource.mipLevel
															sourceOrigin:MTLOriginMake(region.m_imageOffset.x, region.m_imageOffset.y, region.m_imageOffset.z)
																sourceSize:MTLSizeMake(region.m_imageExtent.x, region.m_imageExtent.y, region.m_imageExtent.z)
																	toBuffer:targetBuffer
												 destinationOffset:region.m_bufferOffset
										destinationBytesPerRow:region.m_bufferBytesPerDimension.x
									destinationBytesPerImage:region.m_bufferBytesPerDimension.x * region.m_bufferBytesPerDimension.y];
			}
		}

#elif RENDERER_WEBGPU
		static auto copyTextureToBuffer = [](
																				const BlitCommandEncoderView commandEncoder,
																				const ImageView sourceImage,
																				const BufferView targetBuffer,
																				const BufferImageCopy& __restrict region
																			)
		{
			const WGPUImageCopyTexture source
			{
#if !RENDERER_WEBGPU_DAWN
				nullptr,
#endif
					sourceImage, region.m_imageSubresource.mipLevel,
					WGPUOrigin3D{
						(uint32)region.m_imageOffset.x,
						(uint32)region.m_imageOffset.y,
						(uint32)region.m_imageOffset.z + region.m_imageSubresource.arrayLayerRanges.GetIndex()
					},
					ConvertImageAspectFlags(region.m_imageSubresource.aspectMask),
			};
			const WGPUImageCopyBuffer target
			{
#if !RENDERER_WEBGPU_DAWN
				nullptr,
#endif
					WGPUTextureDataLayout{
						nullptr,
						region.m_bufferOffset,
						region.m_bufferBytesPerDimension.x,
						region.m_bufferBytesPerDimension.x * region.m_blockCountPerDimension.y
					},
					targetBuffer,
			};
			const WGPUExtent3D extent{
				region.m_imageExtent.x,
				region.m_imageExtent.y,
				Math::Max(region.m_imageExtent.z, region.m_imageSubresource.arrayLayerRanges.GetCount())
			};
			wgpuCommandEncoderCopyTextureToBuffer(commandEncoder, &source, &target, &extent);
		};

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[commandEncoder = *this, sourceImage = (ImageView)sourceImage, targetBuffer, regions = InlineVector<BufferImageCopy, 1>{regions}]()
			{
				for (const BufferImageCopy& __restrict region : regions)
				{
					copyTextureToBuffer(commandEncoder, sourceImage, targetBuffer, region);
				}
			}
		);
#else
		for (const BufferImageCopy& __restrict region : regions)
		{
			copyTextureToBuffer(*this, sourceImage, targetBuffer, region);
		}
#endif

#else
		UNUSED(targetBuffer);
		UNUSED(sourceImage);
		UNUSED(regions);
		Assert(false, "TODO");
#endif
	}

	void BlitCommandEncoderView::RecordCopyImageToImage(
		const RenderTexture& sourceImage,
		[[maybe_unused]] const ImageLayout sourceImageLayout,
		const RenderTexture& targetImage,
		[[maybe_unused]] const ImageLayout targetImageLayout,
		const ArrayView<const ImageCopy, uint16> regions
	) const
	{
		Assert(sourceImage.IsValid());
		Assert(targetImage.IsValid());

		Assert(sourceImageLayout == ImageLayout::TransferSourceOptimal, "Invalid source image layout for copy");
		Assert(targetImageLayout == ImageLayout::TransferDestinationOptimal, "Invalid target image layout for copy");
		if constexpr (ENABLE_ASSERTS)
		{
			{
				const SubresourceStatesBase& sourceSubresourceStates = sourceImage.GetSubresourceStates();
				const ImageSubresourceRange sourceFullSubresourceRange{sourceImage.GetTotalSubresourceRange()};
				for ([[maybe_unused]] const ImageCopy& region : regions)
				{
					[[maybe_unused]] const SubresourceState sourceSubresourceState = *sourceSubresourceStates.GetUniformSubresourceState(
						ImageSubresourceRange{
							region.m_sourceLayers.aspectMask,
							MipRange{region.m_sourceLayers.mipLevel, 1},
							region.m_sourceLayers.arrayLayerRanges
						},
						sourceFullSubresourceRange,
						0
					);
					Assert(sourceSubresourceState.m_imageLayout == sourceImageLayout, "Source image was not in layout for copy");
				}
			}

			const SubresourceStatesBase& targetSubresourceStates = targetImage.GetSubresourceStates();
			const ImageSubresourceRange targetFullSubresourceRange{targetImage.GetTotalSubresourceRange()};
			for ([[maybe_unused]] const ImageCopy& region : regions)
			{
				[[maybe_unused]] const SubresourceState targetSubresourceState = *targetSubresourceStates.GetUniformSubresourceState(
					ImageSubresourceRange{
						region.m_targetLayers.aspectMask,
						MipRange{region.m_targetLayers.mipLevel, 1},
						region.m_targetLayers.arrayLayerRanges
					},
					targetFullSubresourceRange,
					0
				);
				Assert(targetSubresourceState.m_imageLayout == targetImageLayout, "Target image was not in layout for copy");
			}
		}

#if RENDERER_VULKAN
		static_assert(sizeof(ImageCopy) == sizeof(VkImageCopy));
		static_assert(alignof(ImageCopy) == alignof(VkImageCopy));

		vkCmdCopyImage(
			m_pCommandEncoder,
			sourceImage,
			static_cast<VkImageLayout>(sourceImageLayout),
			targetImage,
			static_cast<VkImageLayout>(targetImageLayout),
			regions.GetSize(),
			reinterpret_cast<const VkImageCopy*>(regions.GetData())
		);

#elif RENDERER_METAL
		for (const ImageCopy& __restrict region : regions)
		{
			Assert(region.m_sourceLayers.arrayLayerRanges.GetCount() == region.m_targetLayers.arrayLayerRanges.GetCount());
			// TODO: Expose levelCount in our API so we can make use of Metal here
			[m_pCommandEncoder copyFromTexture:sourceImage
														 sourceSlice:region.m_sourceLayers.arrayLayerRanges.GetIndex()
														 sourceLevel:region.m_sourceLayers.mipLevel
															 toTexture:targetImage
												destinationSlice:region.m_targetLayers.arrayLayerRanges.GetIndex()
												destinationLevel:region.m_targetLayers.mipLevel
															sliceCount:region.m_sourceLayers.arrayLayerRanges.GetCount()
															levelCount:1];
		}

#elif RENDERER_WEBGPU
		static auto copyImageToImage = [](
																		 const BlitCommandEncoderView commandEncoder,
																		 const ImageView sourceImage,
																		 const ImageView targetImage,
																		 const ImageCopy& __restrict region
																	 )
		{
			const WGPUImageCopyTexture source
			{
#if !RENDERER_WEBGPU_DAWN
				nullptr,
#endif
					sourceImage, region.m_sourceLayers.mipLevel,
					WGPUOrigin3D{
						(uint32)region.m_sourceOffset.x,
						(uint32)region.m_sourceOffset.y,
						(uint32)region.m_sourceOffset.z + region.m_sourceLayers.arrayLayerRanges.GetIndex()
					},
					ConvertImageAspectFlags(region.m_sourceLayers.aspectMask),
			};
			const WGPUImageCopyTexture target
			{
#if !RENDERER_WEBGPU_DAWN
				nullptr,
#endif
					targetImage, region.m_targetLayers.mipLevel,
					WGPUOrigin3D{
						(uint32)region.m_targetOffset.x,
						(uint32)region.m_targetOffset.y,
						(uint32)region.m_targetOffset.z + region.m_targetLayers.arrayLayerRanges.GetIndex()
					},
					ConvertImageAspectFlags(region.m_targetLayers.aspectMask),
			};
			const WGPUExtent3D extent{
				region.m_extent.x,
				region.m_extent.y,
				Math::Max(
					region.m_extent.z,
					Math::Min(region.m_sourceLayers.arrayLayerRanges.GetCount(), region.m_targetLayers.arrayLayerRanges.GetCount())
				)
			};

			wgpuCommandEncoderCopyTextureToTexture(commandEncoder, &source, &target, &extent);
		};

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[commandEncoder = *this,
		   sourceImage = (ImageView)sourceImage,
		   targetImage = (ImageView)targetImage,
		   regions = InlineVector<ImageCopy, 1>{regions}]()
			{
				for (const ImageCopy& __restrict region : regions)
				{
					copyImageToImage(commandEncoder, sourceImage, targetImage, region);
				}
			}
		);
#else
		for (const ImageCopy& __restrict region : regions)
		{
			copyImageToImage(*this, sourceImage, targetImage, region);
		}
#endif
#else
		UNUSED(targetImage);
		UNUSED(sourceImage);
		UNUSED(regions);
		Assert(false, "TODO");
#endif
	}

	void BlitCommandEncoderView::RecordBlitImage(
		const RenderTexture& targetImage,
		const ImageLayout targetImageLayout,
		const RenderTexture& sourceImage,
		const ImageLayout sourceImageLayout,
		const ArrayView<const ImageBlit, uint16> regions
	) const
	{
		Assert(targetImage.IsValid());
		Assert(sourceImage.IsValid());

		Assert(sourceImageLayout == ImageLayout::TransferSourceOptimal, "Invalid source image layout for copy");
		Assert(targetImageLayout == ImageLayout::TransferDestinationOptimal, "Invalid target image layout for copy");
		if constexpr (ENABLE_ASSERTS)
		{
			{
				const SubresourceStatesBase& sourceSubresourceStates = sourceImage.GetSubresourceStates();
				const ImageSubresourceRange sourceFullSubresourceRange{sourceImage.GetTotalSubresourceRange()};
				for ([[maybe_unused]] const ImageBlit& region : regions)
				{
					[[maybe_unused]] const SubresourceState sourceSubresourceState = *sourceSubresourceStates.GetUniformSubresourceState(
						ImageSubresourceRange{
							region.m_sourceLayers.aspectMask,
							MipRange{region.m_sourceLayers.mipLevel, 1},
							region.m_sourceLayers.arrayLayerRanges
						},
						sourceFullSubresourceRange,
						0
					);
					Assert(sourceSubresourceState.m_imageLayout == sourceImageLayout, "Source image was not in layout for copy");
				}
			}

			const SubresourceStatesBase& targetSubresourceStates = targetImage.GetSubresourceStates();
			const ImageSubresourceRange targetFullSubresourceRange{targetImage.GetTotalSubresourceRange()};
			for ([[maybe_unused]] const ImageBlit& region : regions)
			{
				[[maybe_unused]] const SubresourceState targetSubresourceState = *targetSubresourceStates.GetUniformSubresourceState(
					ImageSubresourceRange{
						region.m_targetLayers.aspectMask,
						MipRange{region.m_targetLayers.mipLevel, 1},
						region.m_targetLayers.arrayLayerRanges
					},
					targetFullSubresourceRange,
					0
				);
				Assert(targetSubresourceState.m_imageLayout == targetImageLayout, "Target image was not in layout for copy");
			}
		}

#if RENDERER_VULKAN
		static_assert(sizeof(ImageBlit) == sizeof(VkImageBlit));
		static_assert(alignof(ImageBlit) == alignof(VkImageBlit));

		vkCmdBlitImage(
			m_pCommandEncoder,
			sourceImage,
			static_cast<VkImageLayout>(sourceImageLayout),
			targetImage,
			static_cast<VkImageLayout>(targetImageLayout),
			regions.GetSize(),
			reinterpret_cast<const VkImageBlit*>(regions.GetData()),
			VK_FILTER_LINEAR
		);

#elif RENDERER_METAL
		UNUSED(targetImageLayout);
		UNUSED(sourceImageLayout);

		for (const ImageBlit& region : regions)
		{
			// Calculate source and destination origin and size based on ImageBlit struct
			MTLOrigin sourceOrigin = {(uint32)region.m_sourceOffset.x, (uint32)region.m_sourceOffset.y, (uint32)region.m_sourceOffset.z};
			const Math::Vector3i sourceSize3 = region.m_sourceExtent - region.m_sourceOffset;
			MTLSize sourceSize = {(uint32)sourceSize3.x, (uint32)sourceSize3.y, (uint32)sourceSize3.z};

			MTLOrigin destOrigin = {(uint32)region.m_targetOffset.x, (uint32)region.m_targetOffset.y, (uint32)region.m_targetOffset.z};

			[m_pCommandEncoder copyFromTexture:sourceImage
														 sourceSlice:region.m_sourceLayers.arrayLayerRanges.GetIndex()
														 sourceLevel:region.m_sourceLayers.mipLevel
														sourceOrigin:sourceOrigin
															sourceSize:sourceSize
															 toTexture:targetImage
												destinationSlice:region.m_targetLayers.arrayLayerRanges.GetIndex()
												destinationLevel:region.m_targetLayers.mipLevel
											 destinationOrigin:destOrigin];
		}

#else
		UNUSED(targetImage);
		UNUSED(targetImageLayout);
		UNUSED(sourceImage);
		UNUSED(sourceImageLayout);
		UNUSED(regions);
		Assert(false, "TODO");
#endif
	}

	BarrierCommandEncoder CommandEncoderView::BeginBarrier() const
	{
		return BarrierCommandEncoder{*this};
	}

	BarrierCommandEncoder::~BarrierCommandEncoder()
	{
		if (IsValid())
		{
			End();
		}
	}

	BarrierCommandEncoder& BarrierCommandEncoder::operator=(BarrierCommandEncoder&& other) noexcept
	{
		Assert(!m_commandEncoder.IsValid());
		m_commandEncoder = other.m_commandEncoder;
		other.m_commandEncoder = {};
		return *this;
	}

	void BarrierCommandEncoder::Emplace(Barrier&& barrier)
	{
		Assert(barrier.m_sourceStages.AreAnySet());
		Assert(barrier.m_targetStages.AreAnySet());

		for (Barrier& __restrict existingBarrier : m_barriers)
		{
			if (existingBarrier.m_sourceStages == barrier.m_sourceStages && existingBarrier.m_targetStages == barrier.m_targetStages)
			{
				Assert(
					!existingBarrier.m_imageMemoryBarriers.ContainsIf(
						[otherBarriers = barrier.m_imageMemoryBarriers.GetView()](const ImageMemoryBarrier& existingBarrier)
						{
							for (const ImageMemoryBarrier& otherBarrier : otherBarriers)
							{
								if (otherBarrier.m_image == existingBarrier.m_image && otherBarrier.m_subresourceRange.Contains(existingBarrier.m_subresourceRange))
								{
									return true;
								}
							}
							return false;
						}
					),
					"Barriers must not duplicate images!"
				);

				existingBarrier.m_imageMemoryBarriers.MoveEmplaceRangeBack(barrier.m_imageMemoryBarriers.GetView());
				return;
			}
		}

		m_barriers.EmplaceBack(Forward<Barrier>(barrier));
	}

	void BarrierCommandEncoder::TransitionImageLayout(
		const EnumFlags<PipelineStageFlags> targetPipelineStageFlags,
		const EnumFlags<AccessFlags> targetAccessMask,
		const ImageLayout newLayout,
		const ImageView image,
		SubresourceStatesBase& imageSubresourceStates,
		const ImageSubresourceRange subresourceRange,
		const ImageSubresourceRange fullSubresourceRange
	)
	{
		Assert(newLayout != ImageLayout::Undefined, "Must specify target layout");
		Assert(image.IsValid(), "Texture must be valid to transition layout!");

		Assert(
			GetSupportedPipelineStageFlags(targetAccessMask).AreAllSet(targetPipelineStageFlags),
			"Transitioned image target stages do not match target access mask"
		);
		Assert(
			GetSupportedAccessFlags(targetPipelineStageFlags).AreAllSet(targetAccessMask),
			"Transitioned image target access flags do not match target stages"
		);

		Assert(GetSupportedAccessFlags(newLayout).AreAllSet(targetAccessMask), "Specified target access mask should match new layout");
		Assert(
			GetSupportedPipelineStageFlags(newLayout).AreAllSet(targetPipelineStageFlags),
			"Specified pipeline stage flags should match new layout"
		);

		const SubresourceState newSubresourceState{
			newLayout,
			PassAttachmentReference{},
			targetPipelineStageFlags,
			targetAccessMask,
			(QueueFamilyIndex)~QueueFamilyIndex(0u)
		};
		imageSubresourceStates.VisitUniformSubresourceRanges(
			subresourceRange,
			[this, newLayout, image, &imageSubresourceStates, fullSubresourceRange, newSubresourceState](
				const SubresourceState previousSubresourceState,
				const ImageSubresourceRange transitionedSubresourceRange
			)
			{
				if (previousSubresourceState.m_imageLayout != newLayout)
				{
					imageSubresourceStates.SetSubresourceState(transitionedSubresourceRange, newSubresourceState, fullSubresourceRange, 0);
					Assert(
						imageSubresourceStates.GetUniformSubresourceState(transitionedSubresourceRange, fullSubresourceRange, 0)->m_imageLayout ==
						newSubresourceState.m_imageLayout
					);

					Emplace(Barrier{
						previousSubresourceState.m_pipelineStageFlags,
						newSubresourceState.m_pipelineStageFlags,
						ImageMemoryBarriers{ImageMemoryBarrier{
							previousSubresourceState.m_accessFlags,
							newSubresourceState.m_accessFlags,
							previousSubresourceState.m_imageLayout,
							newSubresourceState.m_imageLayout,
							image,
							transitionedSubresourceRange
						}}
					});
				}
			},
			fullSubresourceRange,
			0
		);

		if constexpr (ENABLE_ASSERTS)
		{
			for (const ImageAspectFlags aspectFlag : subresourceRange.m_aspectMask)
			{
				for (const ArrayRange::UnitType arrayLayerIndex : subresourceRange.m_arrayRange)
				{
					for (const MipRange::UnitType mipLevelIndex : subresourceRange.m_mipRange)
					{
						const SubresourceState subresourceState =
							imageSubresourceStates.GetSubresourceState(aspectFlag, mipLevelIndex, arrayLayerIndex, fullSubresourceRange, 0);
						Assert(subresourceState.m_imageLayout == newSubresourceState.m_imageLayout);
					}
				}
			}
		}
	}

	void BarrierCommandEncoder::TransitionImageLayout(
		const EnumFlags<PipelineStageFlags> targetPipelineStageFlags,
		const EnumFlags<AccessFlags> targetAccessMask,
		const ImageLayout newLayout,
		RenderTexture& texture,
		const ImageSubresourceRange subresourceRange
	)
	{
		TransitionImageLayout(
			targetPipelineStageFlags,
			targetAccessMask,
			newLayout,
			texture,
			texture.GetSubresourceStates(),
			subresourceRange,
			texture.GetTotalSubresourceRange()
		);
	}

	void BarrierCommandEncoder::TransitionImageLayout(
		const EnumFlags<PipelineStageFlags> targetPipelineStageFlags,
		const EnumFlags<AccessFlags> targetAccessMask,
		const ImageLayout newLayout,
		const QueueFamilyIndex targetQueueFamilyIndex,
		const ImageView image,
		SubresourceStatesBase& imageSubresourceStates,
		const ImageSubresourceRange subresourceRange,
		const ImageSubresourceRange fullSubresourceRange
	)
	{
		Assert(newLayout != ImageLayout::Undefined, "Must specify target layout");
		Assert(image.IsValid(), "Texture must be valid to transition layout!");

		Assert(
			GetSupportedPipelineStageFlags(targetAccessMask).AreAllSet(targetPipelineStageFlags),
			"Transitioned image target stages do not match target access mask"
		);
		Assert(
			GetSupportedAccessFlags(targetPipelineStageFlags).AreAllSet(targetAccessMask),
			"Transitioned image target access flags do not match target stages"
		);

		Assert(GetSupportedAccessFlags(newLayout).AreAllSet(targetAccessMask), "Specified target access mask should match new layout");
		Assert(
			GetSupportedPipelineStageFlags(newLayout).AreAllSet(targetPipelineStageFlags),
			"Specified pipeline stage flags should match new layout"
		);
		const SubresourceState
			newSubresourceState{newLayout, PassAttachmentReference{}, targetPipelineStageFlags, targetAccessMask, targetQueueFamilyIndex};
		imageSubresourceStates.VisitUniformSubresourceRanges(
			subresourceRange,
			[this, newLayout, image, &imageSubresourceStates, fullSubresourceRange, newSubresourceState](
				const SubresourceState previousSubresourceState,
				const ImageSubresourceRange transitionedSubresourceRange
			)
			{
				if (previousSubresourceState.m_imageLayout != newLayout)
				{
					imageSubresourceStates.SetSubresourceState(transitionedSubresourceRange, newSubresourceState, fullSubresourceRange, 0);
					Assert(
						imageSubresourceStates.GetUniformSubresourceState(transitionedSubresourceRange, fullSubresourceRange, 0)->m_imageLayout ==
						newSubresourceState.m_imageLayout
					);

					Emplace(Barrier{
						previousSubresourceState.m_pipelineStageFlags,
						newSubresourceState.m_pipelineStageFlags,
						ImageMemoryBarriers{ImageMemoryBarrier{
							previousSubresourceState.m_accessFlags,
							newSubresourceState.m_accessFlags,
							previousSubresourceState.m_imageLayout,
							newSubresourceState.m_imageLayout,
							image,
							transitionedSubresourceRange
						}}
					});
				}
			},
			fullSubresourceRange,
			0
		);

		if constexpr (ENABLE_ASSERTS)
		{
			for (const ImageAspectFlags aspectFlag : subresourceRange.m_aspectMask)
			{
				for (const ArrayRange::UnitType arrayLayerIndex : subresourceRange.m_arrayRange)
				{
					for (const MipRange::UnitType mipLevelIndex : subresourceRange.m_mipRange)
					{
						const SubresourceState subresourceState =
							imageSubresourceStates.GetSubresourceState(aspectFlag, mipLevelIndex, arrayLayerIndex, fullSubresourceRange, 0);
						Assert(subresourceState.m_imageLayout == newSubresourceState.m_imageLayout);
					}
				}
			}
		}
	}

	void BarrierCommandEncoder::TransitionImageLayout(
		const EnumFlags<PipelineStageFlags> targetPipelineStageFlags,
		const EnumFlags<AccessFlags> targetAccessMask,
		const ImageLayout newLayout,
		const QueueFamilyIndex targetQueueFamilyIndex,
		RenderTexture& texture,
		const ImageSubresourceRange subresourceRange
	)
	{
		TransitionImageLayout(
			targetPipelineStageFlags,
			targetAccessMask,
			newLayout,
			targetQueueFamilyIndex,
			texture,
			texture.GetSubresourceStates(),
			subresourceRange,
			texture.GetTotalSubresourceRange()
		);
	}

	void BarrierCommandEncoder::End()
	{
		Assert(m_commandEncoder.IsValid());
		if (m_barriers.HasElements())
		{
			for (const Barrier& barrier : m_barriers)
			{
				m_commandEncoder.RecordPipelineBarrier(barrier.m_sourceStages, barrier.m_targetStages, barrier.m_imageMemoryBarriers, {});
			}
		}
		m_commandEncoder = {};
	}

	AccelerationStructureCommandEncoder CommandEncoderView::BeginAccelerationStructure() const
	{
#if RENDERER_VULKAN
		return AccelerationStructureCommandEncoder{m_pCommandEncoder};
#elif RENDERER_METAL
		id<MTLAccelerationStructureCommandEncoder> accelerationStructureEncoder = [m_pCommandEncoder accelerationStructureCommandEncoder];
		return AccelerationStructureCommandEncoder{accelerationStructureEncoder};
#elif RENDERER_WEBGPU
		return AccelerationStructureCommandEncoder{m_pCommandEncoder};
#else
		Assert(false, "TODO");
		return {};
#endif
	}

	AccelerationStructureCommandEncoder::~AccelerationStructureCommandEncoder()
	{
#if RENDERER_METAL
		if (m_pCommandEncoder != nullptr)
		{
			[m_pCommandEncoder endEncoding];
		}
#endif
	}

	AccelerationStructureCommandEncoder& AccelerationStructureCommandEncoder::operator=(AccelerationStructureCommandEncoder&& other) noexcept
	{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
		Assert(m_pCommandEncoder == 0);
		m_pCommandEncoder = other.m_pCommandEncoder;
		other.m_pCommandEncoder = 0;
#endif
		return *this;
	}

	void AccelerationStructureCommandEncoder::End()
	{
#if RENDERER_VULKAN || RENDERER_WEBGPU
		m_pCommandEncoder = 0;
#elif RENDERER_METAL
		[m_pCommandEncoder endEncoding];
		m_pCommandEncoder = 0;
#endif
	}

	void AccelerationStructureCommandEncoderView::SetDebugName(
		[[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name
	) const
	{
#if RENDERER_VULKAN
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_COMMAND_BUFFER,
			reinterpret_cast<uint64_t>(m_pCommandEncoder),
			name
		};

#if PLATFORM_APPLE
		vkSetDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT =
			reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(logicalDevice.GetSetDebugUtilsObjectNameEXT());
		if (setDebugUtilsObjectNameEXT != nullptr)
		{
			setDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder setLabel:[NSString stringWithUTF8String:name]];

#else
		UNUSED(logicalDevice);
		UNUSED(name);
#endif
	}

	void AccelerationStructureCommandEncoderView::BeginDebugMarker(
		[[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView label, [[maybe_unused]] const Math::Color color
	) const
	{
#if RENDERER_VULKAN && PROFILE_BUILD
		VkDebugUtilsLabelEXT labelInfo = {VK_STRUCTURE_TYPE_DEBUG_UTILS_LABEL_EXT, nullptr, label, {color.r, color.g, color.b, color.a}};

#if PLATFORM_APPLE
		vkCmdBeginDebugUtilsLabelEXT(m_pCommandEncoder, &labelInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkCmdBeginDebugUtilsLabelEXT beginDebugUtilsLabelEXT =
			reinterpret_cast<PFN_vkCmdBeginDebugUtilsLabelEXT>(logicalDevice.GetCmdBeginDebugUtilsLabelEXT());
		if (beginDebugUtilsLabelEXT != nullptr)
		{
			beginDebugUtilsLabelEXT(m_pCommandEncoder, &labelInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder pushDebugGroup:(NSString* _Nonnull)[NSString stringWithUTF8String:label]];

#else
		UNUSED(logicalDevice);
		UNUSED(label);
#endif
	}

	void AccelerationStructureCommandEncoderView::EndDebugMarker([[maybe_unused]] const LogicalDevice& logicalDevice) const
	{
#if RENDERER_VULKAN && PROFILE_BUILD

#if PLATFORM_APPLE
		vkCmdEndDebugUtilsLabelEXT(m_pCommandEncoder);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkCmdEndDebugUtilsLabelEXT endDebugUtilsLabelEXT =
			reinterpret_cast<PFN_vkCmdEndDebugUtilsLabelEXT>(logicalDevice.GetCmdEndDebugUtilsLabelEXT());
		if (endDebugUtilsLabelEXT != nullptr)
		{
			endDebugUtilsLabelEXT(m_pCommandEncoder);
		}
#endif

#elif RENDERER_METAL
		[m_pCommandEncoder popDebugGroup];

#endif
	}

	SingleUseCommandBuffer::SingleUseCommandBuffer(
		const Rendering::LogicalDevice& logicalDevice,
		const Rendering::CommandPoolView commandPool,
		Threading::JobRunnerThread& commandBufferCreationThread,
		const Rendering::QueueFamily queueFamily,
		const Threading::JobPriority priority
	)
		: m_pLogicalDevice(&logicalDevice)
		, m_commandPool(commandPool)
		, m_pCommandBufferCreationThread(&commandBufferCreationThread)
		, m_unifiedCommandBuffer(logicalDevice, commandPool, logicalDevice.GetCommandQueue(queueFamily))
		, m_queueFamily(queueFamily)
		, m_priority(priority)
	{
		if (m_unifiedCommandBuffer.IsValid())
		{
			m_unifiedCommandBuffer.BeginEncoding(logicalDevice, CommandBuffer::Flags::OneTimeSubmit);
		}
	}

	SingleUseCommandBuffer::~SingleUseCommandBuffer()
	{
		if (m_unifiedCommandBuffer.IsEncoding())
		{
			m_unifiedCommandBuffer.StopEncoding();

			Rendering::QueueSubmissionParameters parameters;
			const EncodedCommandBufferView encodedCommandBuffer = m_unifiedCommandBuffer;

			const LogicalDeviceView logicalDevice = *m_pLogicalDevice;
			parameters.m_finishedCallback = [logicalDevice,
			                                 onFinished = Move(OnFinished),
			                                 unifiedCommandBuffer = Move(m_unifiedCommandBuffer),
			                                 commandPool = m_commandPool,
			                                 &commandBufferCreationThread = *m_pCommandBufferCreationThread]() mutable
			{
				onFinished();

				commandBufferCreationThread.QueueExclusiveCallbackFromAnyThread(
					Threading::JobPriority::DeallocateResourcesMin,
					[logicalDevice, unifiedCommandBuffer = Move(unifiedCommandBuffer), commandPool](Threading::JobRunnerThread&) mutable
					{
						unifiedCommandBuffer.Destroy(logicalDevice, commandPool);
					}
				);
			};

			m_pLogicalDevice->GetQueueSubmissionJob(m_queueFamily)
				.Queue(m_priority, ArrayView<const Rendering::EncodedCommandBufferView, uint16>(encodedCommandBuffer), Move(parameters));
		}
	}

	EncodedCommandBuffer& EncodedCommandBuffer::operator=([[maybe_unused]] EncodedCommandBuffer&& other) noexcept
	{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		m_pCommandBuffer = other.m_pCommandBuffer;
		other.m_pCommandBuffer = nullptr;
#endif
		return *this;
	}

	EncodedCommandBuffer::~EncodedCommandBuffer()
	{
#if RENDERER_WEBGPU
		if (m_pCommandBuffer != nullptr)
		{
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pCommandBuffer = m_pCommandBuffer]()
				{
					wgpuCommandBufferRelease(*pCommandBuffer);
					delete pCommandBuffer;
				}
			);
#else
			wgpuCommandBufferRelease(m_pCommandBuffer);
#endif
		}
#endif
	}

	EncodedParallelCommandBuffer& EncodedParallelCommandBuffer::operator=([[maybe_unused]] EncodedParallelCommandBuffer&& other) noexcept
	{
		// Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		m_pCommandBuffer = other.m_pCommandBuffer;
		other.m_pCommandBuffer = nullptr;
#endif
		return *this;
	}

	EncodedParallelCommandBuffer::~EncodedParallelCommandBuffer()
	{
#if RENDERER_WEBGPU
		if (m_pCommandBuffer != nullptr)
		{
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pCommandBuffer = m_pCommandBuffer]()
				{
					WGPURenderBundle pRenderBundle = *pCommandBuffer;
					if (pRenderBundle != nullptr)
					{
						wgpuRenderBundleRelease(pRenderBundle);
					}
					delete pCommandBuffer;
				}
			);
#else
			wgpuRenderBundleRelease(m_pCommandBuffer);
#endif
		}
#endif
	}

	UnifiedCommandBuffer::UnifiedCommandBuffer(
		const LogicalDeviceView logicalDevice, const CommandPoolView commandPool, const CommandQueueView commandQueue
	)
		: CommandBuffer(logicalDevice, commandPool, commandQueue)
	{
	}

	CommandEncoderView UnifiedCommandBuffer::BeginEncoding(const Rendering::LogicalDeviceView logicalDevice, const CommandBuffer::Flags flags)
	{
		Assert(!m_commandEncoder.IsValid());
		Assert(!m_encodedCommandBuffer.IsValid());
		return m_commandEncoder = CommandBuffer::BeginEncoding(logicalDevice, flags);
	}

	void UnifiedCommandBuffer::Destroy(const LogicalDeviceView logicalDevice, const CommandPoolView commandPool)
	{
		CommandBuffer::Destroy(logicalDevice, commandPool);
	}
}
