#pragma once

#include "Framegraph/Framegraph.h"

#include <Renderer/Renderer.h>
#include <Renderer/Stages/Stage.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Assets/Texture/TextureCache.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>

#include <Common/System/Query.h>

namespace ngine::Rendering
{
	struct GenericPassStage final : public Stage
	{
		GenericPassStage(LogicalDevice& logicalDevice, RenderOutput& renderOutput, Framegraph& framegraph, PassInfo& passInfo)
			: Stage(logicalDevice, Threading::JobPriority::Draw)
			, m_renderOutput(renderOutput)
			, m_framegraph(framegraph)
			, m_passInfo(passInfo)
		{
		}

		[[nodiscard]] virtual QueueFamily GetRecordedQueueFamily() const override
		{
			return m_passInfo.GetGenericPassInfo()->m_stage.GetRecordedQueueFamily();
		}

		[[nodiscard]] virtual bool ShouldRecordCommands() const override
		{
			return true;
		}
		virtual void OnBeforeRecordCommands(const CommandEncoderView commandEncoder) override
		{
#if RENDERER_OBJECT_DEBUG_NAMES
			commandEncoder.SetDebugName(m_logicalDevice, m_passInfo.m_debugName);
#endif

			[[maybe_unused]] const bool wasSkipped = m_passInfo.GetGenericPassInfo()->m_stage.EvaluateShouldSkip();
			m_passInfo.GetGenericPassInfo()->m_stage.OnBeforeRecordCommands(commandEncoder);
		}
		virtual void RecordCommands(const CommandEncoderView commandEncoder) override
		{
#if RENDERER_OBJECT_DEBUG_NAMES
			const DebugMarker debugMarker{commandEncoder, m_logicalDevice, m_passInfo.m_debugName, "#FF0000"_color};
#endif
			const GenericPassInfo& genericPassInfo = *m_passInfo.GetGenericPassInfo();
			if (!genericPassInfo.m_stage.WasSkipped())
			{
				const FrameImageId frameImageIdentifier = m_framegraph.GetAcquireRenderOutputImageStage().GetFrameImageId();
				const QueueFamilyIndex queueFamilyIndex = m_logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics);

				TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
				const AttachmentIdentifier renderOutputAttachmentIdentifier =
					textureCache.FindOrRegisterRenderTargetTemplate(Framegraph::RenderOutputRenderTargetGuid);

				Assert(genericPassInfo.m_subpasses.GetSize() == 1, "TODO: Support multiple generic subpasses");
				{
					const GenericSubpassInfo& subpassInfo = genericPassInfo.m_subpasses[0];

					// Transition images into the required layouts for each subpass
					BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

					for (const SubresourceStates& requiredAttachmentSubresourceStates : subpassInfo.m_requiredAttachmentStates)
					{
						const AttachmentIndex localAttachmentIndex =
							subpassInfo.m_requiredAttachmentStates.GetIteratorIndex(&requiredAttachmentSubresourceStates);
						const SubpassAttachmentReference attachmentReference = subpassInfo.m_attachmentReferences[localAttachmentIndex];
						const AttachmentIdentifier attachmentIdentifier = m_passInfo.m_attachmentIdentifiers[attachmentReference.m_attachmentIndex];

						const ImageSubresourceRange fullSubresourceRange = requiredAttachmentSubresourceStates.GetSubresourceRange();
						const ImageSubresourceRange subresourceRange{
							fullSubresourceRange.m_aspectMask,
							attachmentReference.m_mipRange,
							attachmentReference.m_arrayRange
						};

						if (attachmentIdentifier == renderOutputAttachmentIdentifier)
						{
							requiredAttachmentSubresourceStates.VisitUniformSubresourceRanges(
								subresourceRange,
								[this,
							   &barrierCommandEncoder,
							   queueFamilyIndex](const SubresourceState subresourceState, const ImageSubresourceRange transitionedSubresourceRange)
								{
									barrierCommandEncoder.TransitionImageLayout(
										GetSupportedPipelineStageFlags(subresourceState.m_imageLayout),
										GetSupportedAccessFlags(subresourceState.m_imageLayout),
										subresourceState.m_imageLayout,
										queueFamilyIndex,
										m_renderOutput.GetCurrentColorImageView(),
										m_renderOutput.GetCurrentColorSubresourceStates(),
										transitionedSubresourceRange,
										ImageSubresourceRange{ImageAspectFlags::Color, MipRange{0, 1}, ArrayRange{0, 1}}
									);
								}
							);
						}
						else if (const Optional<RenderTexture*> pRenderTexture =
						           subpassInfo.m_textures[(uint8)frameImageIdentifier][localAttachmentIndex];
						         pRenderTexture.IsValid() && pRenderTexture->IsValid())
						{
							requiredAttachmentSubresourceStates.VisitUniformSubresourceRanges(
								subresourceRange,
								[&barrierCommandEncoder, queueFamilyIndex, &renderTexture = *pRenderTexture](
									const SubresourceState subresourceState,
									const ImageSubresourceRange transitionedSubresourceRange
								)
								{
									barrierCommandEncoder.TransitionImageLayout(
										GetSupportedPipelineStageFlags(subresourceState.m_imageLayout),
										GetSupportedAccessFlags(subresourceState.m_imageLayout),
										subresourceState.m_imageLayout,
										queueFamilyIndex,
										renderTexture,
										transitionedSubresourceRange
									);
								}
							);
						}
					}
				}

				genericPassInfo.m_stage.RecordCommands(commandEncoder);
			}
		}
		virtual void OnAfterRecordCommands(const CommandEncoderView commandEncoder) override
		{
			m_passInfo.GetGenericPassInfo()->m_stage.OnAfterRecordCommands(commandEncoder);
		}
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return m_passInfo.GetGenericPassInfo()->m_stage.GetPipelineStageFlags();
		}

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return m_passInfo.m_debugName;
		}
#endif
	protected:
		RenderOutput& m_renderOutput;
		Framegraph& m_framegraph;
		PassInfo& m_passInfo;
	};
}
