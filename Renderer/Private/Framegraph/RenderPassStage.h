#pragma once

#include "Framegraph/Framegraph.h"

#include <Renderer/Renderer.h>
#include <Renderer/Stages/Pass.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Assets/Texture/TextureCache.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>

#include <Common/System/Query.h>

namespace ngine::Rendering
{
	struct RenderPassStage final : public Pass
	{
		RenderPassStage(
			LogicalDevice& logicalDevice,
			RenderOutput& renderOutput,
			Framegraph& framegraph,
			ClearValues&& clearValues,
			const Framegraph::SubpassIndex subpassCount,
			PassInfo& passInfo
		)
			: Pass(
					logicalDevice,
					renderOutput,
					framegraph,
					Forward<ClearValues>(clearValues),
					subpassCount
#if STAGE_DEPENDENCY_PROFILING
					,
					String(passInfo.m_debugName)
#endif
				)
			, m_passInfo(passInfo)
		{
			m_pSceneViewDrawer = passInfo.m_pSceneViewDrawer;
		}
	protected:
		[[nodiscard]] virtual bool ShouldRecordCommands() const override
		{
			return Pass::ShouldRecordCommands();
		}

		virtual void OnBeforeRecordCommands(const CommandEncoderView commandEncoder) override
		{
#if RENDERER_OBJECT_DEBUG_NAMES
			commandEncoder.SetDebugName(m_logicalDevice, m_passInfo.m_debugName);
#endif

			if (Pass::ShouldRecordCommands())
			{
				const FrameImageId frameImageIdentifier = m_framegraph.GetAcquireRenderOutputImageStage().GetFrameImageId();
				const QueueFamilyIndex graphicsQueueFamilyIndex = m_logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics);
				RenderPassInfo& renderPassInfo = *m_passInfo.GetRenderPassInfo();

				TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
				const AttachmentIdentifier renderOutputAttachmentIdentifier =
					textureCache.FindOrRegisterRenderTargetTemplate(Framegraph::RenderOutputRenderTargetGuid);

				// Transition attachments to the initial layout expected by the render pass
				BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();
				for (const Rendering::AttachmentDescription& attachmentDescription : renderPassInfo.m_attachmentDescriptions)
				{
					const AttachmentIndex attachmentIndex = renderPassInfo.m_attachmentDescriptions.GetIteratorIndex(&attachmentDescription);
					const ImageSubresourceRange attachmentSubresourceRange = renderPassInfo.m_attachmentSubresourceRanges[attachmentIndex];
					const AttachmentIdentifier attachmentIdentifier = m_passInfo.m_attachmentIdentifiers[attachmentIndex];

					if (attachmentIdentifier == renderOutputAttachmentIdentifier)
					{
						barrierCommandEncoder.TransitionImageLayout(
							GetSupportedPipelineStageFlags(attachmentDescription.m_initialLayout),
							GetSupportedAccessFlags(attachmentDescription.m_initialLayout),
							attachmentDescription.m_initialLayout,
							graphicsQueueFamilyIndex,
							m_renderOutput.GetCurrentColorImageView(),
							m_renderOutput.GetCurrentColorSubresourceStates(),
							attachmentSubresourceRange,
							ImageSubresourceRange{ImageAspectFlags::Color, MipRange{0, 1}, ArrayRange{0, 1}}
						);
					}
					else if (const Optional<RenderTexture*> pRenderTexture = renderPassInfo.m_textures[(uint8)frameImageIdentifier][attachmentIndex];
					         pRenderTexture.IsValid() && pRenderTexture->IsValid())
					{
						barrierCommandEncoder.TransitionImageLayout(
							GetSupportedPipelineStageFlags(attachmentDescription.m_initialLayout),
							GetSupportedAccessFlags(attachmentDescription.m_initialLayout),
							attachmentDescription.m_initialLayout,
							graphicsQueueFamilyIndex,
							*pRenderTexture,
							attachmentSubresourceRange
						);
					}
				}

				for (const RenderPassInfo::ExternalInputAttachmentState externalInputAttachmentState :
				     renderPassInfo.m_externalInputAttachmentStates)
				{
					const ImageSubresourceRange attachmentSubresourceRange = externalInputAttachmentState.m_subresourceRange;
					const SubresourceState subresourceState = externalInputAttachmentState.m_subresourceState;
					if (externalInputAttachmentState.m_attachmentIdentifier == renderOutputAttachmentIdentifier)
					{
						barrierCommandEncoder.TransitionImageLayout(
							subresourceState.m_pipelineStageFlags,
							subresourceState.m_accessFlags,
							subresourceState.m_imageLayout,
							graphicsQueueFamilyIndex,
							m_renderOutput.GetCurrentColorImageView(),
							m_renderOutput.GetCurrentColorSubresourceStates(),
							attachmentSubresourceRange,
							ImageSubresourceRange{ImageAspectFlags::Color, MipRange{0, 1}, ArrayRange{0, 1}}
						);
					}
					else if (const Optional<RenderTexture*> pRenderTexture =
					           renderPassInfo.m_textures[(uint8)frameImageIdentifier][externalInputAttachmentState.m_attachmentIndex];
					         pRenderTexture.IsValid() && pRenderTexture->IsValid())
					{
						barrierCommandEncoder.TransitionImageLayout(
							subresourceState.m_pipelineStageFlags,
							subresourceState.m_accessFlags,
							subresourceState.m_imageLayout,
							graphicsQueueFamilyIndex,
							*pRenderTexture,
							attachmentSubresourceRange
						);
					}
				}
			}

			Pass::OnBeforeRecordCommands(commandEncoder);
		}

		virtual void RecordCommands(const CommandEncoderView commandEncoder) override
		{
			if (Pass::ShouldRecordCommands())
			{
				Pass::RecordCommands(commandEncoder);
			}
		}

		virtual void OnAfterRecordCommands(const CommandEncoderView commandEncoder) override
		{
			Pass::OnAfterRecordCommands(commandEncoder);

			if (Pass::ShouldRecordCommands())
			{
				// Set the final image layouts for all render pass attachments
				const FrameImageId frameImageIdentifier = m_framegraph.GetAcquireRenderOutputImageStage().GetFrameImageId();
				const PassIndex passIndex = m_framegraph.m_passes.GetIteratorIndex(&m_passInfo);
				const QueueFamilyIndex graphicsQueueFamilyIndex = m_logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics);
				RenderPassInfo& renderPassInfo = *m_passInfo.GetRenderPassInfo();

				TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
				const AttachmentIdentifier renderOutputAttachmentIdentifier =
					textureCache.FindOrRegisterRenderTargetTemplate(Framegraph::RenderOutputRenderTargetGuid);

				for (const Rendering::AttachmentDescription& attachmentDescription : renderPassInfo.m_attachmentDescriptions)
				{
					const AttachmentIndex attachmentIndex = renderPassInfo.m_attachmentDescriptions.GetIteratorIndex(&attachmentDescription);
					const AttachmentIdentifier attachmentIdentifier = m_passInfo.m_attachmentIdentifiers[attachmentIndex];
					const ImageSubresourceRange attachmentSubresourceRange = renderPassInfo.m_attachmentSubresourceRanges[attachmentIndex];

					if (attachmentIdentifier == renderOutputAttachmentIdentifier)
					{
						SubresourceStatesBase& subresourceStates = m_renderOutput.GetCurrentColorSubresourceStates();

						subresourceStates.SetSubresourceState(
							attachmentSubresourceRange,
							SubresourceState{
								attachmentDescription.m_finalLayout,
								PassAttachmentReference{passIndex, attachmentIndex, InvalidSubpassIndex},
								GetSupportedPipelineStageFlags(attachmentDescription.m_finalLayout),
								GetSupportedAccessFlags(attachmentDescription.m_finalLayout),
								graphicsQueueFamilyIndex
							},
							ImageSubresourceRange{ImageAspectFlags::Color, MipRange{0, 1}, ArrayRange{0, 1}},
							0
						);
					}
					else if (const Optional<RenderTexture*> pRenderTexture = renderPassInfo.m_textures[(uint8)frameImageIdentifier][attachmentIndex];
					         pRenderTexture.IsValid() && pRenderTexture->IsValid())
					{
						SubresourceStatesBase& subresourceStates = pRenderTexture->GetSubresourceStates();

						subresourceStates.SetSubresourceState(
							attachmentSubresourceRange,
							SubresourceState{
								attachmentDescription.m_finalLayout,
								PassAttachmentReference{passIndex, attachmentIndex, InvalidSubpassIndex},
								GetSupportedPipelineStageFlags(attachmentDescription.m_finalLayout),
								GetSupportedAccessFlags(attachmentDescription.m_finalLayout),
								graphicsQueueFamilyIndex
							},
							pRenderTexture->GetTotalSubresourceRange(),
							0
						);
					}
				}
			}
		}

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return m_passInfo.m_debugName;
		}
#endif
	protected:
		PassInfo& m_passInfo;
	};
}
