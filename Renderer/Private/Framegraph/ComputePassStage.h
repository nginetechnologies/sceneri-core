#pragma once

#include "Framegraph/Framegraph.h"

#include <Renderer/Renderer.h>
#include <Renderer/Stages/Stage.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Commands/ComputeCommandEncoder.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/Scene/SceneViewDrawer.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Assets/Texture/TextureCache.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>

namespace ngine::Rendering
{
	struct ComputePassStage final : public Stage
	{
		ComputePassStage(LogicalDevice& logicalDevice, RenderOutput& renderOutput, Framegraph& framegraph, PassInfo& passInfo)
			: Stage(logicalDevice, Threading::JobPriority::Draw)
			, m_renderOutput(renderOutput)
			, m_framegraph(framegraph)
			, m_passInfo(passInfo)
		{
		}

		[[nodiscard]] virtual QueueFamily GetRecordedQueueFamily() const override
		{
			return QueueFamily::Graphics;
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

			for (ComputeSubpassInfo& subpassInfo : m_passInfo.GetComputePassInfo()->m_subpasses.GetView())
			{
				for (Stage& stage : subpassInfo.m_stages)
				{
					[[maybe_unused]] const bool wasSkipped = stage.EvaluateShouldSkip();
					stage.OnBeforeRecordCommands(commandEncoder);
				}
			}
		}
		virtual void RecordCommands(const CommandEncoderView commandEncoder) override
		{
#if RENDERER_OBJECT_DEBUG_NAMES
			const DebugMarker debugMarker{commandEncoder, m_logicalDevice, m_passInfo.m_debugName, "#FF0000"_color};
#endif

			auto doComputePass = [this, commandEncoder](const ViewMatrices& viewMatrices)
			{
				const ArrayView<ComputeSubpassInfo, Framegraph::SubpassIndex> subpasses = m_passInfo.GetComputePassInfo()->m_subpasses;
				const uint32 maximumPushConstantInstanceCount = subpasses.Count(
					[](const ComputeSubpassInfo& subpassStages)
					{
						return subpassStages.m_stages.GetView().Count(
							[](const Stage& stage)
							{
								return stage.GetMaximumPushConstantInstanceCount();
							}
						);
					}
				);

				const ComputeCommandEncoder computeCommandEncoder = commandEncoder.BeginCompute(m_logicalDevice, maximumPushConstantInstanceCount);

				const FrameImageId frameImageIdentifier = m_framegraph.GetAcquireRenderOutputImageStage().GetFrameImageId();
				const QueueFamilyIndex queueFamilyIndex = m_logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics);

				TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
				const AttachmentIdentifier renderOutputAttachmentIdentifier =
					textureCache.FindOrRegisterRenderTargetTemplate(Framegraph::RenderOutputRenderTargetGuid);

#if RENDERER_OBJECT_DEBUG_NAMES
				const ComputeDebugMarker debugMarker{computeCommandEncoder, m_logicalDevice, m_passInfo.m_debugName, "#FF0000"_color};
#endif
				for (ComputeSubpassInfo& subpassInfo : subpasses)
				{
					const Framegraph::SubpassIndex subpassIndex = subpasses.GetIteratorIndex(&subpassInfo);

					// Transition images into the required layouts for each subpass
					{
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

					for (Stage& stage : subpassInfo.m_stages)
					{
						if (!stage.WasSkipped())
						{
#if RENDERER_OBJECT_DEBUG_NAMES
							ComputeDebugMarker computeDebugMarker{computeCommandEncoder, m_logicalDevice, subpassInfo.m_debugName, "#ffffff"_color};
#endif

							// TODO: do it asynchronously and concurrently
							stage.RecordComputePassCommands(computeCommandEncoder, viewMatrices, subpassIndex);
						}
					}
				}
			};

			if (m_passInfo.m_pSceneViewDrawer.IsValid())
			{
				m_passInfo.m_pSceneViewDrawer->DoComputePass(Move(doComputePass));
			}
			else
			{
				ViewMatrices viewMatrices;
				viewMatrices.Assign(
					Math::Identity,
					Math::Identity,
					Math::Zero,
					Math::Identity,
					m_renderOutput.GetOutputArea().GetSize(),
					m_renderOutput.GetOutputArea().GetSize()
				);
				doComputePass(viewMatrices);
			}
		}
		virtual void OnAfterRecordCommands(const CommandEncoderView commandEncoder) override
		{
			for (ComputeSubpassInfo& subpassInfo : m_passInfo.GetComputePassInfo()->m_subpasses.GetView())
			{
				for (Stage& stage : subpassInfo.m_stages)
				{
					stage.OnAfterRecordCommands(commandEncoder);
				}
			}
		}
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			EnumFlags<PipelineStageFlags> stageFlags;
			for (const ComputeSubpassInfo& subpassInfo : m_passInfo.GetComputePassInfo()->m_subpasses.GetView())
			{
				for (Stage& stage : subpassInfo.m_stages)
				{
					stageFlags |= stage.GetPipelineStageFlags() * (!stage.WasSkipped());
				}
			}

			if (stageFlags.AreNoneSet())
			{
				stageFlags = PipelineStageFlags::ComputeShader;
			}

			return stageFlags;
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
