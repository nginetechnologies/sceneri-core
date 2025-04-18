#include "Framegraph/Framegraph.h"
#include "Framegraph/SubresourceStates.inl"
#include "GenericPassStage.h"
#include "RenderPassStage.h"
#include "ComputePassStage.h"

#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/FormatInfo.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Commands/SingleUseCommandBuffer.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <Renderer/Wrappers/SubpassDescription.h>

#include <Engine/Threading/JobManager.h>
#include <Engine/Threading/JobRunnerThread.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Rendering
{
	void AttachmentInfo::OnUsed(const PassAttachmentReference attachmentReference)
	{
		const PassIndex previousPassIndex = m_previousPassAttachmentReference.passIndex;
		m_previousPassAttachmentReference = attachmentReference;

		if (attachmentReference.passIndex != previousPassIndex)
		{
			m_isFinalLayoutLocked = false;
		}
	}

	void AttachmentInfo::TransitionLayout(
		const ArrayView<PassInfo, PassIndex> passes,
		const SubresourceState previousSubresourceState,
		const SubresourceState newSubresourceState,
		const ImageSubresourceRange subresourceRange
	)
	{
		// Change the final layout of the previous pass to transition into our starting layout
		if (previousSubresourceState.m_attachmentReference.passIndex != InvalidPassIndex)
		{
			PassInfo& __restrict previousPassInfo = passes[previousSubresourceState.m_attachmentReference.passIndex];
			if (const Optional<RenderPassInfo*> pPreviousRenderPassInfo = previousPassInfo.GetRenderPassInfo();
			    pPreviousRenderPassInfo.IsValid() &&
			    previousSubresourceState.m_attachmentReference.attachmentIndex < pPreviousRenderPassInfo->m_attachmentDescriptions.GetSize())
			{
				Rendering::AttachmentDescription& lastAttachmentDescription =
					pPreviousRenderPassInfo->m_attachmentDescriptions[previousSubresourceState.m_attachmentReference.attachmentIndex];
				if (!IsFinalLayoutLocked() && lastAttachmentDescription.m_finalLayout != newSubresourceState.m_imageLayout)
				{
					// Transition to our requested layout when the previous render pass ends
					lastAttachmentDescription.m_finalLayout = newSubresourceState.m_imageLayout;

					[[maybe_unused]] const ImageSubresourceRange lastAttachmentSubresourceRange =
						pPreviousRenderPassInfo->m_attachmentSubresourceRanges[previousSubresourceState.m_attachmentReference.attachmentIndex];
					Assert(lastAttachmentSubresourceRange.Contains(subresourceRange));
					LockFinalLayout();
				}
			}
		}

		SetSubresourceState(subresourceRange, newSubresourceState);
		OnUsed(newSubresourceState.m_attachmentReference);
	}

	SubresourceState AttachmentInfo::RequestInitialRenderPassLayout(
		const ArrayView<PassInfo, PassIndex> passes,
		const PassAttachmentReference attachmentReference,
		const EnumFlags<PipelineStageFlags> newPipelineStageFlags,
		const EnumFlags<AccessFlags> newAccessFlags,
		const ImageLayout preferredInitialImageLayout,
		const QueueFamilyIndex newQueueFamilyIndex,
		const ImageSubresourceRange subresourceRange
	)
	{
		const SubresourceState requestedSubresourceState{
			preferredInitialImageLayout,
			attachmentReference,
			newPipelineStageFlags,
			newAccessFlags,
			newQueueFamilyIndex
		};

		const SubresourceState previousSubresourceState = *GetUniformSubresourceState(subresourceRange);
		TransitionLayout(passes, previousSubresourceState, requestedSubresourceState, m_subresourceRange);
		return requestedSubresourceState;
	}

	void AttachmentInfo::RequestOrTransitionLayout(
		const ArrayView<PassInfo, PassIndex> passes,
		const PassAttachmentReference attachmentReference,
		const EnumFlags<PipelineStageFlags> newPipelineStageFlags,
		const EnumFlags<AccessFlags> newAccessFlags,
		const ImageLayout requestedImageLayout,
		const QueueFamilyIndex newQueueFamilyIndex,
		const ImageSubresourceRange subresourceRange
	)
	{
		Assert(requestedImageLayout != ImageLayout::Undefined);
		Assert(GetSupportedAccessFlags(newPipelineStageFlags).AreAllSet(newAccessFlags));

		VisitUniformSubresourceRanges(
			subresourceRange,
			[this, passes, attachmentReference, newPipelineStageFlags, newAccessFlags, requestedImageLayout, newQueueFamilyIndex](
				const SubresourceState previousSubresourceState,
				const ImageSubresourceRange transitionedSubresourceRange
			)
			{
				const SubresourceState
					newSubresourceState{requestedImageLayout, attachmentReference, newPipelineStageFlags, newAccessFlags, newQueueFamilyIndex};
				if (previousSubresourceState.m_imageLayout == requestedImageLayout)
				{
					// Layout was already what we need, do nothing
					SetSubresourceState(transitionedSubresourceRange, newSubresourceState);
					OnUsed(attachmentReference);
				}
				else if (previousSubresourceState.WasUsed())
				{
					TransitionLayout(passes, previousSubresourceState, newSubresourceState, transitionedSubresourceRange);
				}
				else
				{
					OnUsed(attachmentReference);
					SetSubresourceState(transitionedSubresourceRange, newSubresourceState);
				}
			}
		);
	}

	[[nodiscard]] ImageLayout
	GetInitialDepthStencilImageLayout(const AttachmentLoadType attachmentLoadType, const AttachmentStoreType attachmentStoreType)
	{
		switch (attachmentStoreType)
		{
			case AttachmentStoreType::Store:
				return ImageLayout::DepthStencilAttachmentOptimal;
			case AttachmentStoreType::Undefined:
			{
				switch (attachmentLoadType)
				{
					case AttachmentLoadType::Clear:
						return ImageLayout::DepthStencilAttachmentOptimal;
					case AttachmentLoadType::Undefined:
					case AttachmentLoadType::LoadExisting:
						// Default to read-only. Can be changed by a subsequent stage if it needs to load what we write.
						return ImageLayout::DepthAttachmentStencilReadOnlyOptimal;
				}
				ExpectUnreachable();
			}
		}
		ExpectUnreachable();
	}
	[[nodiscard]] PURE_STATICS AttachmentLoadType GetAttachmentLoadType(
		const Framegraph::AttachmentDescription& attachmentDescription,
		const Framegraph::PassAttachmentReference previousPassAttachmentReference
	)
	{
		if (attachmentDescription.m_flags.IsSet(Framegraph::AttachmentFlags::Clear))
		{
			return AttachmentLoadType::Clear;
		}
		else if (attachmentDescription.m_flags.IsSet(Framegraph::AttachmentFlags::CanRead))
		{
			if (previousPassAttachmentReference.passIndex != Framegraph::InvalidPassIndex)
			{
				return AttachmentLoadType::LoadExisting;
			}
		}
		else if (attachmentDescription.m_flags.IsSet(Framegraph::AttachmentFlags::MustRead))
		{
			Assert(previousPassAttachmentReference.passIndex != Framegraph::InvalidPassIndex, "Attachment expecting read without prior pass");
			return AttachmentLoadType::LoadExisting;
		}

		return AttachmentLoadType::Undefined;
	}
	[[nodiscard]] PURE_STATICS AttachmentStoreType
	GetInitialAttachmentStoreType(const Framegraph::AttachmentDescription& attachmentDescription)
	{
		if (attachmentDescription.m_flags.IsSet(Framegraph::AttachmentFlags::MustStore) || attachmentDescription.m_flags.AreAllSet(Framegraph::AttachmentFlags::Clear | Framegraph::AttachmentFlags::CanStore))
		{
			return AttachmentStoreType::Store;
		}
		return AttachmentStoreType::Undefined;
	}
	[[nodiscard]] EnumFlags<AccessFlags> GetAttachmentAccessFlags(
		const AttachmentLoadType attachmentLoadType,
		const AttachmentStoreType attachmentStoreType,
		const AccessFlags readFlag,
		const AccessFlags writeFlag
	)
	{
		EnumFlags<AccessFlags> accessFlags;
		switch (attachmentLoadType)
		{
			case AttachmentLoadType::Clear:
				break;
			case AttachmentLoadType::Undefined:
				break;
			case AttachmentLoadType::LoadExisting:
				accessFlags |= readFlag;
				break;
		}
		switch (attachmentStoreType)
		{
			case AttachmentStoreType::Store:
				accessFlags |= writeFlag;
				break;
			case AttachmentStoreType::Undefined:
				break;
		}

		return accessFlags;
	}

	[[nodiscard]] bool ReadsAnyAttachments(
		const ArrayView<const Framegraph::AttachmentIndex, Framegraph::AttachmentIndex> subpassAttachmentIndices,
		const ArrayView<const Rendering::AttachmentDescription, Framegraph::AttachmentIndex> attachmentDescriptions
	)
	{
		return subpassAttachmentIndices.Any(
			[attachmentDescriptions](const Framegraph::AttachmentIndex attachmentIndex)
			{
				return attachmentDescriptions[attachmentIndex].m_loadType == AttachmentLoadType::LoadExisting;
			}
		);
	}
	[[nodiscard]] bool WritesAnyAttachments(
		const ArrayView<const Framegraph::AttachmentIndex, Framegraph::AttachmentIndex> subpassAttachmentIndices,
		const ArrayView<const Rendering::AttachmentDescription, Framegraph::AttachmentIndex> attachmentDescriptions
	)
	{
		return subpassAttachmentIndices.Any(
			[attachmentDescriptions](const Framegraph::AttachmentIndex attachmentIndex)
			{
				return attachmentDescriptions[attachmentIndex].m_storeType == AttachmentStoreType::Store;
			}
		);
	}
	[[nodiscard]] bool ReadsAttachment(
		const Framegraph::AttachmentIndex subpassAttachmentIndex,
		const ArrayView<const Rendering::AttachmentDescription, Framegraph::AttachmentIndex> attachmentDescriptions
	)
	{
		return attachmentDescriptions[subpassAttachmentIndex].m_loadType == AttachmentLoadType::LoadExisting;
	}
	[[nodiscard]] bool WritesAttachment(
		const Framegraph::AttachmentIndex subpassAttachmentIndex,
		const ArrayView<const Rendering::AttachmentDescription, Framegraph::AttachmentIndex> attachmentDescriptions
	)
	{
		return attachmentDescriptions[subpassAttachmentIndex].m_storeType == AttachmentStoreType::Store;
	}
	[[nodiscard]] bool ReadsStencilAttachment(
		const Framegraph::AttachmentIndex subpassAttachmentIndex,
		const ArrayView<const Rendering::AttachmentDescription, Framegraph::AttachmentIndex> attachmentDescriptions
	)
	{
		return attachmentDescriptions[subpassAttachmentIndex].m_stencilLoadType == AttachmentLoadType::LoadExisting;
	}
	[[nodiscard]] bool WritesStencilAttachment(
		const Framegraph::AttachmentIndex subpassAttachmentIndex,
		const ArrayView<const Rendering::AttachmentDescription, Framegraph::AttachmentIndex> attachmentDescriptions
	)
	{
		return attachmentDescriptions[subpassAttachmentIndex].m_stencilStoreType == AttachmentStoreType::Store;
	}

	[[nodiscard]] Rendering::SubpassDependency GetNextSubpassDependency(
		const RenderSubpassDescription& __restrict subpassDescription,
		const ArrayView<const RenderSubpassDescription, Framegraph::SubpassIndex> subsequentSubpassDescriptions,
		const ArrayView<const Rendering::AttachmentDescription, Framegraph::AttachmentIndex> attachmentDescriptions,
		const Framegraph::SubpassIndex newSubpassIndex,
		uint32& previousSubpassIndex,
		EnumFlags<PipelineStageFlags>& previousPipelineStageFlags,
		EnumFlags<AccessFlags>& previousAccessFlags
	)
	{
		const bool readsColorInput = subpassDescription.m_subpassInputAttachmentIndices.Any(
			[attachmentDescriptions](const Framegraph::AttachmentIndex attachmentIndex)
			{
				return attachmentDescriptions[attachmentIndex].m_initialLayout == ImageLayout::ColorAttachmentOptimal;
			}
		);
		const bool readsDepthInput = subpassDescription.m_subpassInputAttachmentIndices.Any(
			[attachmentDescriptions](const Framegraph::AttachmentIndex attachmentIndex)
			{
				switch (attachmentDescriptions[attachmentIndex].m_initialLayout)
				{
					case ImageLayout::DepthStencilAttachmentOptimal:
					case ImageLayout::DepthStencilReadOnlyOptimal:
					case ImageLayout::DepthAttachmentOptimal:
					case ImageLayout::DepthReadOnlyStencilAttachmentOptimal:
					case ImageLayout::DepthAttachmentStencilReadOnlyOptimal:
					case ImageLayout::DepthReadOnlyOptimal:
					case ImageLayout::StencilAttachmentOptimal:
					case ImageLayout::StencilReadOnlyOptimal:
					case ImageLayout::ShaderReadOnlyOptimal:
					case ImageLayout::General:
						return true;
					case ImageLayout::ColorAttachmentOptimal:
						return false;
					case ImageLayout::Undefined:
					case ImageLayout::TransferSourceOptimal:
					case ImageLayout::TransferDestinationOptimal:
					case ImageLayout::Preinitialized:
					case ImageLayout::PresentSource:
						ExpectUnreachable();
				}
				ExpectUnreachable();
			}
		);
		const bool writesColorOutput = subsequentSubpassDescriptions.Any(
			[colorAttachmentIndices =
		     subpassDescription.m_colorAttachmentIndices](const RenderSubpassDescription& __restrict subsequentSubpassDescription)
			{
				return subsequentSubpassDescription.m_subpassInputAttachmentIndices.ContainsAny(colorAttachmentIndices);
			}
		);
		const bool writesDepthOutput = subpassDescription.m_depthAttachmentIndex != Framegraph::InvalidAttachmentIndex &&
		                               subsequentSubpassDescriptions.Any(
																		 [depthAttachmentIndex = subpassDescription.m_depthAttachmentIndex](
																			 const RenderSubpassDescription& __restrict subsequentSubpassDescription
																		 )
																		 {
																			 return subsequentSubpassDescription.m_subpassInputAttachmentIndices.Contains(depthAttachmentIndex);
																		 }
																	 );
		const bool writesStencilOutput = subpassDescription.m_stencilAttachmentIndex != Framegraph::InvalidAttachmentIndex &&
		                                 subsequentSubpassDescriptions.Any(
																			 [stencilAttachmentIndex = subpassDescription.m_stencilAttachmentIndex](
																				 const RenderSubpassDescription& __restrict subsequentSubpassDescription
																			 )
																			 {
																				 return subsequentSubpassDescription.m_subpassInputAttachmentIndices.Contains(stencilAttachmentIndex
			                                   );
																			 }
																		 );

		const bool readsColor = ReadsAnyAttachments(subpassDescription.m_colorAttachmentIndices, attachmentDescriptions) || readsColorInput;
		const bool writesColor = WritesAnyAttachments(subpassDescription.m_colorAttachmentIndices, attachmentDescriptions) || writesColorOutput;
		const bool readsDepth = ((subpassDescription.m_depthAttachmentIndex != Framegraph::InvalidAttachmentIndex) &&
		                         ReadsAttachment(subpassDescription.m_depthAttachmentIndex, attachmentDescriptions)) ||
		                        readsDepthInput;
		const bool writesDepth = ((subpassDescription.m_depthAttachmentIndex != Framegraph::InvalidAttachmentIndex) &&
		                          WritesAttachment(subpassDescription.m_depthAttachmentIndex, attachmentDescriptions)) ||
		                         writesDepthOutput;
		const bool readsStencil = ((subpassDescription.m_stencilAttachmentIndex != Framegraph::InvalidAttachmentIndex) &&
		                           ReadsStencilAttachment(subpassDescription.m_stencilAttachmentIndex, attachmentDescriptions)) ||
		                          readsDepthInput;
		const bool writesStencil = ((subpassDescription.m_stencilAttachmentIndex != Framegraph::InvalidAttachmentIndex) &&
		                            WritesStencilAttachment(subpassDescription.m_stencilAttachmentIndex, attachmentDescriptions)) ||
		                           writesStencilOutput;
		const bool readsDepthStencil = readsDepth | readsStencil;
		const bool writesDepthStencil = writesDepth | writesStencil;

		const EnumFlags<PipelineStageFlags> newPipelineStageFlags = (Rendering::PipelineStageFlags::ColorAttachmentOutput *
		                                                             (readsColor | writesColor)) |
		                                                            (Rendering::PipelineStageFlags::EarlyFragmentTests * readsDepthStencil) |
		                                                            (Rendering::PipelineStageFlags::LateFragmentTests * writesDepthStencil);
		const EnumFlags<AccessFlags> newAccessFlags = (Rendering::AccessFlags::ColorAttachmentRead * readsColor) |
		                                              (Rendering::AccessFlags::ColorAttachmentWrite * writesColor) |
		                                              (Rendering::AccessFlags::DepthStencilRead * readsDepthStencil) |
		                                              (Rendering::AccessFlags::DepthStencilWrite * writesDepthStencil);

		const Rendering::SubpassDependency subpassDependency{
			previousSubpassIndex,
			newSubpassIndex,
			previousPipelineStageFlags,
			newPipelineStageFlags,
			previousAccessFlags,
			newAccessFlags,
			Rendering::DependencyFlags::ByRegion
		};

		previousSubpassIndex = newSubpassIndex;
		previousPipelineStageFlags = newPipelineStageFlags;
		previousAccessFlags = newAccessFlags;

		return subpassDependency;
	}

	void Framegraph::CompileColorAttachments(
		RenderPassInfo& renderPassInfo,
		const ArrayView<const ColorAttachmentDescription, AttachmentIndex> colorAttachmentDescriptions,
		const PassIndex passIndex,
		const QueueFamilyIndex queueFamilyIndex,
		IdentifierMask<AttachmentIdentifier>& processedAttachments,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos,
		const AttachmentIdentifier renderOutputAttachmentIdentifier
	)
	{
		for (const ColorAttachmentDescription& __restrict colorAttachmentDescription : colorAttachmentDescriptions)
		{
			processedAttachments.Set(colorAttachmentDescription.m_identifier);

			const AttachmentIndex colorAttachmentIndex = colorAttachmentDescriptions.GetIteratorIndex(&colorAttachmentDescription);
			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[colorAttachmentDescription.m_identifier];

			const bool isRenderOutputAttachment = colorAttachmentDescription.m_identifier == renderOutputAttachmentIdentifier;

			const Format attachmentFormat = isRenderOutputAttachment ? m_renderOutput.GetColorFormat()
			                                                         : Rendering::Format::Invalid; // Populated later

			Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(colorAttachmentDescription.m_subresourceRange);
			const AttachmentLoadType attachmentLoadType =
				GetAttachmentLoadType(colorAttachmentDescription, previousSubresourceState->m_attachmentReference);
			const AttachmentStoreType initialAttachmentStoreType = GetInitialAttachmentStoreType(colorAttachmentDescription);

			const EnumFlags<AccessFlags> accessFlags = GetAttachmentAccessFlags(
				attachmentLoadType,
				initialAttachmentStoreType,
				AccessFlags::ColorAttachmentRead,
				AccessFlags::ColorAttachmentWrite
			);
			Assert(GetSupportedAccessFlags(PipelineStageFlags::ColorAttachmentOutput).AreAllSet(accessFlags));

			const ImageLayout attachmentTargetLayout = ImageLayout::ColorAttachmentOptimal;
			previousSubresourceState = attachmentInfo.RequestInitialRenderPassLayout(
				m_passes,
				PassAttachmentReference{passIndex, colorAttachmentIndex},
				PipelineStageFlags::ColorAttachmentOutput,
				accessFlags,
				attachmentTargetLayout,
				queueFamilyIndex,
				colorAttachmentDescription.m_subresourceRange
			);

			renderPassInfo.m_attachmentDescriptions[colorAttachmentIndex] = Rendering::AttachmentDescription{
				attachmentFormat,
				SampleCount::One,
				attachmentLoadType,
				initialAttachmentStoreType, // Note: Can be changed if a future pass ends up depending on this attachment
				AttachmentLoadType::Undefined,
				AttachmentStoreType::Undefined,
				previousSubresourceState->m_imageLayout,
				attachmentTargetLayout // Note: can be changed if a future pass ends up depending on this attachment
			};

			renderPassInfo.m_attachmentMappingTypes[colorAttachmentIndex] = colorAttachmentDescription.m_mappingType;

			renderPassInfo.m_attachmentSubresourceRanges[colorAttachmentIndex] = colorAttachmentDescription.m_subresourceRange;

			Assert(previousSubresourceState.IsValid());
			if (previousSubresourceState->WasUsed())
			{
				if (attachmentLoadType == AttachmentLoadType::LoadExisting)
				{
					PassInfo& __restrict previousPassInfo = m_passes[previousSubresourceState->m_attachmentReference.passIndex];
					if (const Optional<RenderPassInfo*> pPreviousRenderPassInfo = previousPassInfo.GetRenderPassInfo();
					    pPreviousRenderPassInfo.IsValid() &&
					    previousSubresourceState->m_attachmentReference.attachmentIndex < pPreviousRenderPassInfo->m_attachmentDescriptions.GetSize())
					{
						Rendering::AttachmentDescription& __restrict previousPassAttachmentDescription =
							pPreviousRenderPassInfo->m_attachmentDescriptions[previousSubresourceState->m_attachmentReference.attachmentIndex];
						previousPassAttachmentDescription.m_storeType = AttachmentStoreType::Store;

						[[maybe_unused]] const ImageSubresourceRange lastAttachmentSubresourceRange =
							pPreviousRenderPassInfo->m_attachmentSubresourceRanges[previousSubresourceState->m_attachmentReference.attachmentIndex];
						Assert(lastAttachmentSubresourceRange.Contains(colorAttachmentDescription.m_subresourceRange));
					}
				}
			}
		}
	}

	void Framegraph::CompileDepthAttachment(
		RenderPassInfo& renderPassInfo,
		const PassAttachmentReference depthAttachmentReference,
		const DepthAttachmentDescription& __restrict depthAttachmentDescription,
		const QueueFamilyIndex queueFamilyIndex,
		IdentifierMask<AttachmentIdentifier>& processedAttachments,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
	)
	{
		processedAttachments.Set(depthAttachmentDescription.m_identifier);

		AttachmentInfo& __restrict attachmentInfo = attachmentInfos[depthAttachmentDescription.m_identifier];

		const Format attachmentFormat = Rendering::Format::Invalid; // Populated later
		// TODO: Depth only format if possible? Could also have a dynamic attachment which switches depth formats based on need in framegraph.

		Optional<SubresourceState> previousSubresourceState =
			attachmentInfo.GetUniformSubresourceState(depthAttachmentDescription.m_subresourceRange);
		const AttachmentLoadType attachmentLoadType =
			GetAttachmentLoadType(depthAttachmentDescription, previousSubresourceState->m_attachmentReference);
		const AttachmentStoreType initialAttachmentStoreType = GetInitialAttachmentStoreType(depthAttachmentDescription);
		const EnumFlags<AccessFlags> accessFlags = GetAttachmentAccessFlags(
			attachmentLoadType,
			initialAttachmentStoreType,
			AccessFlags::DepthStencilRead,
			AccessFlags::DepthStencilWrite
		);
		const EnumFlags<PipelineStageFlags> pipelineStageFlags =
			(PipelineStageFlags::EarlyFragmentTests * accessFlags.IsSet(AccessFlags::DepthStencilRead)) |
			(PipelineStageFlags::LateFragmentTests * accessFlags.IsSet(AccessFlags::DepthStencilWrite));
		Assert(GetSupportedAccessFlags(pipelineStageFlags).AreAllSet(accessFlags));

		const ImageLayout attachmentTargetLayout = GetInitialDepthStencilImageLayout(attachmentLoadType, initialAttachmentStoreType);
		previousSubresourceState = attachmentInfo.RequestInitialRenderPassLayout(
			m_passes,
			depthAttachmentReference,
			pipelineStageFlags,
			accessFlags,
			attachmentTargetLayout,
			queueFamilyIndex,
			depthAttachmentDescription.m_subresourceRange
		);

		renderPassInfo.m_attachmentDescriptions[depthAttachmentReference.attachmentIndex] = Rendering::AttachmentDescription{
			attachmentFormat,
			SampleCount::One,
			attachmentLoadType,
			initialAttachmentStoreType, // Note: Can be changed if a future pass ends up depending on this stage's attachment
			AttachmentLoadType::Undefined,
			AttachmentStoreType::Undefined,
			previousSubresourceState
				->m_imageLayout,     // Note: Can be changed if a future pass ends up changing the store type (defaults to read only)
			attachmentTargetLayout // Note: Can be changed if a future pass ends up depending on this attachment
		};

		renderPassInfo.m_attachmentMappingTypes[depthAttachmentReference.attachmentIndex] = depthAttachmentDescription.m_mappingType;

		renderPassInfo.m_attachmentSubresourceRanges[depthAttachmentReference.attachmentIndex] = depthAttachmentDescription.m_subresourceRange;

		Assert(previousSubresourceState.IsValid());
		if (previousSubresourceState->WasUsed())
		{
			// Update the previous stage to ensure it stores the image and transitions to our ideal layout
			if (attachmentLoadType == AttachmentLoadType::LoadExisting)
			{
				PassInfo& __restrict previousPassInfo = m_passes[previousSubresourceState->m_attachmentReference.passIndex];
				if (const Optional<RenderPassInfo*> pPreviousRenderPassInfo = previousPassInfo.GetRenderPassInfo())
				{
					Rendering::AttachmentDescription& __restrict previousPassAttachmentDescription =
						pPreviousRenderPassInfo->m_attachmentDescriptions[previousSubresourceState->m_attachmentReference.attachmentIndex];
					previousPassAttachmentDescription.m_storeType = AttachmentStoreType::Store;
					if (previousSubresourceState->m_imageLayout != ImageLayout::DepthStencilAttachmentOptimal)
					{
						if (!attachmentInfo.IsFinalLayoutLocked())
						{
							previousPassAttachmentDescription.m_finalLayout = ImageLayout::DepthStencilAttachmentOptimal;

							[[maybe_unused]] const ImageSubresourceRange lastAttachmentSubresourceRange =
								pPreviousRenderPassInfo->m_attachmentSubresourceRanges[previousSubresourceState->m_attachmentReference.attachmentIndex];
							Assert(lastAttachmentSubresourceRange.Contains(depthAttachmentDescription.m_subresourceRange));
						}
					}
				}
			}
		}
	}

	void Framegraph::CompileStencilAttachment(
		RenderPassInfo& renderPassInfo,
		const PassAttachmentReference stencilAttachmentReference,
		const StencilAttachmentDescription& __restrict stencilAttachmentDescription,
		const Optional<DepthAttachmentDescription>& __restrict depthAttachmentDescription,
		const QueueFamilyIndex queueFamilyIndex,
		IdentifierMask<AttachmentIdentifier>& processedAttachments,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
	)
	{
		processedAttachments.Set(stencilAttachmentDescription.m_identifier);

		AttachmentInfo& __restrict attachmentInfo = attachmentInfos[stencilAttachmentDescription.m_identifier];

		const Format attachmentFormat = Rendering::Format::Invalid; // Populated later
		// TODO: Stencil only format if possible? Could also have a dynamic attachment which switches depth formats based on need in
		// framegraph.

		Optional<SubresourceState> previousSubresourceState =
			attachmentInfo.GetUniformSubresourceState(stencilAttachmentDescription.m_subresourceRange);
		const AttachmentLoadType attachmentLoadType =
			GetAttachmentLoadType(stencilAttachmentDescription, previousSubresourceState->m_attachmentReference);
		const AttachmentStoreType initialAttachmentStoreType = GetInitialAttachmentStoreType(stencilAttachmentDescription);
		const EnumFlags<AccessFlags> accessFlags = GetAttachmentAccessFlags(
			attachmentLoadType,
			initialAttachmentStoreType,
			AccessFlags::DepthStencilRead,
			AccessFlags::DepthStencilWrite
		);
		const EnumFlags<PipelineStageFlags> pipelineStageFlags =
			(PipelineStageFlags::EarlyFragmentTests * accessFlags.IsSet(AccessFlags::DepthStencilRead)) |
			(PipelineStageFlags::LateFragmentTests * accessFlags.IsSet(AccessFlags::DepthStencilWrite));
		Assert(GetSupportedAccessFlags(pipelineStageFlags).AreAllSet(accessFlags));

		const ImageLayout attachmentTargetLayout = GetInitialDepthStencilImageLayout(attachmentLoadType, initialAttachmentStoreType);
		previousSubresourceState = attachmentInfo.RequestInitialRenderPassLayout(
			m_passes,
			stencilAttachmentReference,
			pipelineStageFlags,
			accessFlags,
			attachmentTargetLayout,
			queueFamilyIndex,
			stencilAttachmentDescription.m_subresourceRange
		);

		Rendering::AttachmentDescription& __restrict attachmentDescription =
			renderPassInfo.m_attachmentDescriptions[stencilAttachmentReference.attachmentIndex];
		if (depthAttachmentDescription.IsValid() && depthAttachmentDescription->m_identifier == stencilAttachmentDescription.m_identifier)
		{
			Assert(attachmentDescription.m_format == attachmentFormat, "Unified depth and stencil must have the same format");
			Assert(
				renderPassInfo.m_attachmentMappingTypes[stencilAttachmentReference.attachmentIndex] == stencilAttachmentDescription.m_mappingType,
				"Unified depth and stencil must have the same mapping type"
			);
			attachmentDescription.m_stencilLoadType = attachmentLoadType;
			attachmentDescription.m_stencilStoreType =
				initialAttachmentStoreType; // Note: Can be changed if a future pass ends up depending on this stage's attachment

			attachmentDescription.m_initialLayout = previousSubresourceState->m_imageLayout;
			attachmentDescription.m_finalLayout = attachmentTargetLayout;
		}
		else
		{
			attachmentDescription = Rendering::AttachmentDescription{
				attachmentFormat,
				SampleCount::One,
				AttachmentLoadType::Undefined,
				AttachmentStoreType::Undefined,
				attachmentLoadType,
				initialAttachmentStoreType, // Note: Can be changed if a future pass ends up depending on this stage's attachment
				previousSubresourceState
					->m_imageLayout,     // Note: Can be changed if a future pass ends up changing the store type (defaults to read only)
				attachmentTargetLayout // Note: Can be changed if a future pass ends up depending on this attachment
			};

			renderPassInfo.m_attachmentMappingTypes[stencilAttachmentReference.attachmentIndex] = stencilAttachmentDescription.m_mappingType;
		}

		Assert(previousSubresourceState.IsValid());
		if (previousSubresourceState->WasUsed())
		{
			// Update the previous stage to ensure it stores the image and transitions to our ideal layout
			if (attachmentLoadType == AttachmentLoadType::LoadExisting)
			{
				PassInfo& __restrict previousPassInfo = m_passes[previousSubresourceState->m_attachmentReference.passIndex];
				if (const Optional<RenderPassInfo*> pPreviousRenderPassInfo = previousPassInfo.GetRenderPassInfo())
				{
					Rendering::AttachmentDescription& __restrict previousPassAttachmentDescription =
						pPreviousRenderPassInfo->m_attachmentDescriptions[previousSubresourceState->m_attachmentReference.attachmentIndex];
					previousPassAttachmentDescription.m_stencilStoreType = AttachmentStoreType::Store;
					Assert(previousSubresourceState->m_imageLayout == ImageLayout::DepthStencilAttachmentOptimal);
					previousPassAttachmentDescription.m_finalLayout = ImageLayout::DepthStencilAttachmentOptimal;

					[[maybe_unused]] const ImageSubresourceRange lastAttachmentSubresourceRange =
						pPreviousRenderPassInfo->m_attachmentSubresourceRanges[previousSubresourceState->m_attachmentReference.attachmentIndex];
					Assert(lastAttachmentSubresourceRange.Contains(stencilAttachmentDescription.m_subresourceRange));
				}
				else
				{
					Assert(previousSubresourceState->m_imageLayout == ImageLayout::DepthStencilAttachmentOptimal);
				}
			}
		}
	}

	void Framegraph::CompileInputAttachment(
		const PassAttachmentReference inputAttachmentReference,
		const AttachmentIdentifier attachmentIdentifier,
		const EnumFlags<PipelineStageFlags> pipelineStageFlags,
		const EnumFlags<AccessFlags> accessFlags,
		const QueueFamilyIndex queueFamilyIndex,
		const ImageSubresourceRange subresourceRange,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
	)
	{
		Assert(GetSupportedAccessFlags(pipelineStageFlags).AreAllSet(accessFlags));

		AttachmentInfo& __restrict attachmentInfo = attachmentInfos[attachmentIdentifier];

		const Optional<SubresourceState> previousSubresourceState = attachmentInfo.GetUniformSubresourceState(subresourceRange);
		Assert(
			previousSubresourceState.IsValid() && previousSubresourceState->WasUsed(),
			"Input attachment must be processed by a prior stage!"
		);

		// Ensure that the previous stage stores the attachment
		{
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			PassInfo& __restrict previousPassInfo = m_passes[previousPassAttachmentReference.passIndex];
			if (const Optional<RenderPassInfo*> pPreviousRenderPassInfo = previousPassInfo.GetRenderPassInfo();
			    pPreviousRenderPassInfo.IsValid() &&
			    previousPassAttachmentReference.attachmentIndex < pPreviousRenderPassInfo->m_attachmentDescriptions.GetSize())
			{
				Rendering::AttachmentDescription& __restrict previousPassAttachmentDescription =
					pPreviousRenderPassInfo->m_attachmentDescriptions[previousPassAttachmentReference.attachmentIndex];
				previousPassAttachmentDescription.m_storeType = AttachmentStoreType::Store;

				[[maybe_unused]] const ImageSubresourceRange lastAttachmentSubresourceRange =
					pPreviousRenderPassInfo->m_attachmentSubresourceRanges[previousPassAttachmentReference.attachmentIndex];
				Assert(lastAttachmentSubresourceRange.Contains(subresourceRange));
			}
		}

		ImageLayout attachmentTargetLayout;
		if (subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil))
		{
			attachmentTargetLayout = ImageLayout::DepthStencilReadOnlyOptimal;
		}
		else
		{
			attachmentTargetLayout = ImageLayout::ShaderReadOnlyOptimal;
		}
		attachmentInfo.RequestOrTransitionLayout(
			m_passes,
			inputAttachmentReference,
			pipelineStageFlags,
			accessFlags,
			attachmentTargetLayout,
			queueFamilyIndex,
			subresourceRange
		);
	}

	void Framegraph::CompileOutputAttachment(
		const PassAttachmentReference outputAttachmentReference,
		const AttachmentIdentifier attachmentIdentifier,
		const EnumFlags<PipelineStageFlags> pipelineStageFlags,
		const EnumFlags<AccessFlags> accessFlags,
		const ImageLayout attachmentTargetLayout,
		const QueueFamilyIndex queueFamilyIndex,
		const ImageSubresourceRange subresourceRange,
		IdentifierMask<AttachmentIdentifier>& processedAttachments,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
	)
	{
		Assert(GetSupportedAccessFlags(pipelineStageFlags).AreAllSet(accessFlags));
		processedAttachments.Set(attachmentIdentifier);

		AttachmentInfo& __restrict attachmentInfo = attachmentInfos[attachmentIdentifier];

		attachmentInfo.RequestOrTransitionLayout(
			m_passes,
			outputAttachmentReference,
			pipelineStageFlags,
			accessFlags,
			attachmentTargetLayout,
			queueFamilyIndex,
			subresourceRange
		);
	}

	void Framegraph::CompileInputOutputAttachment(
		const PassAttachmentReference inputOutputAttachmentReference,
		const AttachmentIdentifier attachmentIdentifier,
		const EnumFlags<PipelineStageFlags> pipelineStageFlags,
		const EnumFlags<AccessFlags> accessFlags,
		const ImageLayout attachmentTargetLayout,
		const QueueFamilyIndex queueFamilyIndex,
		const ImageSubresourceRange subresourceRange,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
	)
	{
		Assert(GetSupportedAccessFlags(pipelineStageFlags).AreAllSet(accessFlags));
		AttachmentInfo& __restrict attachmentInfo = attachmentInfos[attachmentIdentifier];

		const Optional<SubresourceState> previousSubresourceState = attachmentInfo.GetUniformSubresourceState(subresourceRange);
		if (previousSubresourceState.IsValid() && previousSubresourceState->WasUsed())
		{
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;

			// Update the previous stage to ensure it stores the image and transitions to our ideal layout
			{
				PassInfo& __restrict previousPassInfo = m_passes[previousPassAttachmentReference.passIndex];
				if (const Optional<RenderPassInfo*> pPreviousRenderPassInfo = previousPassInfo.GetRenderPassInfo())
				{
					Rendering::AttachmentDescription& __restrict previousPassAttachmentDescription =
						pPreviousRenderPassInfo->m_attachmentDescriptions[previousPassAttachmentReference.attachmentIndex];
					previousPassAttachmentDescription.m_storeType = AttachmentStoreType::Store;
				}
			}
		}

		attachmentInfo.RequestOrTransitionLayout(
			m_passes,
			inputOutputAttachmentReference,
			pipelineStageFlags,
			accessFlags,
			attachmentTargetLayout,
			queueFamilyIndex,
			subresourceRange
		);
	}

	void Framegraph::QueueLoadAttachment(
		PassInfo& __restrict passInfo,
		const AttachmentDescription& __restrict attachmentDescription,
		const AttachmentIndex localAttachmentIndex,
		EnumFlags<ImageAspectFlags> imageAspectFlags,
		Threading::JobBatch& jobBatch,
		const AttachmentIdentifier renderOutputAttachmentIdentifier,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos,
		TextureCache& textureCache,
		RenderTargetCache& renderTargetCache
	)
	{
		if (attachmentDescription.m_identifier == renderOutputAttachmentIdentifier)
		{
			const FrameIndex imageCount = m_renderOutput.GetImageCount();
			switch (passInfo.m_type)
			{
				case StageType::RenderPass:
				case StageType::ExplicitRenderPass:
				{
					RenderPassInfo& __restrict renderPassInfo = *passInfo.GetRenderPassInfo();
					for (FrameIndex i = 0; i < imageCount; ++i)
					{
						renderPassInfo.m_imageMappingViews[i][localAttachmentIndex] = m_renderOutput.GetColorImageMappings()[i];
					}
					renderPassInfo.m_imageResolutions[localAttachmentIndex] = m_renderOutput.GetOutputArea().GetSize();

					passInfo.OnPendingCompilationTaskCompleted(m_logicalDevice, *this);
				}
				break;
				case StageType::Generic:
				{
					GenericPassInfo& __restrict genericPassInfo = *passInfo.GetGenericPassInfo();
					for (GenericSubpassInfo& __restrict genericSubpassInfo : genericPassInfo.m_subpasses)
					{
						for (const SubpassAttachmentReference& __restrict subpassAttachmentReference : genericSubpassInfo.m_attachmentReferences)
						{
							if (subpassAttachmentReference.m_attachmentIndex == localAttachmentIndex)
							{
								const AttachmentIndex attachmentIndex =
									genericSubpassInfo.m_attachmentReferences.GetIteratorIndex(&subpassAttachmentReference);
								for (FrameIndex i = 0; i < imageCount; ++i)
								{
									genericSubpassInfo.m_imageMappingViews[i][attachmentIndex] = m_renderOutput.GetColorImageMappings()[i];
								}
								genericSubpassInfo.m_imageResolutions[attachmentIndex] = m_renderOutput.GetOutputArea().GetSize();
							}
						}
					}

					passInfo.OnPendingCompilationTaskCompleted(m_logicalDevice, *this);
				}
				break;
				case StageType::Compute:
				{
					ComputePassInfo& __restrict computePassInfo = *passInfo.GetComputePassInfo();
					for (ComputeSubpassInfo& __restrict subpassInfo : computePassInfo.m_subpasses)
					{
						for (const SubpassAttachmentReference& __restrict subpassAttachmentReference : subpassInfo.m_attachmentReferences)
						{
							if (subpassAttachmentReference.m_attachmentIndex == localAttachmentIndex)
							{
								const AttachmentIndex attachmentIndex = subpassInfo.m_attachmentReferences.GetIteratorIndex(&subpassAttachmentReference);
								for (FrameIndex i = 0; i < imageCount; ++i)
								{
									subpassInfo.m_imageMappingViews[i][attachmentIndex] = m_renderOutput.GetColorImageMappings()[i];
								}
								subpassInfo.m_imageResolutions[attachmentIndex] = m_renderOutput.GetOutputArea().GetSize();
							}
						}
					}

					passInfo.OnPendingCompilationTaskCompleted(m_logicalDevice, *this);
				}
				break;
			}
		}
		else
		{
			const TextureIdentifier textureIdentifier =
				renderTargetCache.FindOrRegisterRenderTargetFromTemplateIdentifier(textureCache, attachmentDescription.m_identifier);
			m_requestedRenderTargets.Set(textureIdentifier);

			Threading::Job* pJob = textureCache.GetOrLoadRenderTarget(
				m_logicalDevice,
				textureIdentifier,
				attachmentDescription.m_identifier,
				SampleCount::One,
				attachmentDescription.m_size,
				MipMask::FromRange(attachmentDescription.m_subresourceRange.m_mipRange),
				attachmentDescription.m_subresourceRange.m_arrayRange,
				TextureCache::TextureLoadListenerData{
					*this,
					[this,
			     &passInfo,
			     localAttachmentIndex,
			     imageAspectFlags,
			     attachmentInfos,
			     resolution = attachmentDescription.m_size,
			     attachmentIdentifier = attachmentDescription.m_identifier,
			     arrayRange = attachmentDescription.m_subresourceRange.m_arrayRange](
						Framegraph& framegraph,
						LogicalDevice& logicalDevice,
						const Rendering::TextureIdentifier,
						RenderTexture& texture,
						const MipMask mipMask,
						[[maybe_unused]] const EnumFlags<LoadedTextureFlags> flags
					) mutable -> EventCallbackResult
					{
						if constexpr (RENDERER_WEBGPU)
						{
							switch (passInfo.m_type)
							{
								case StageType::RenderPass:
								case StageType::ExplicitRenderPass:
									// Render attachments must always carry over the stencil flag if the texture format is capable of it
									imageAspectFlags |= ImageAspectFlags::Stencil * GetFormatInfo(texture.GetFormat()).m_flags.IsSet(FormatFlags::Stencil);
									break;
								case StageType::Generic:
								case StageType::Compute:
									break;
							}
						}

						Assert(!flags.IsSet(LoadedTextureFlags::IsDummy));

						AttachmentInfo& __restrict attachmentInfo = attachmentInfos[attachmentIdentifier];

						const ImageSubresourceRange subresourceRange{imageAspectFlags, MipRange{0, mipMask.GetSize()}, arrayRange};
						const Optional<SubresourceState> uniformSubresourceState = attachmentInfo.GetUniformSubresourceState(subresourceRange);
						Assert(uniformSubresourceState.IsValid());

						if (texture.IsValid() && uniformSubresourceState->m_imageLayout != ImageLayout::Undefined)
						{
							Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
							UnifiedCommandBuffer commandBuffer(
								m_logicalDevice,
								thread.GetRenderData().GetCommandPool(m_logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics),
								m_logicalDevice.GetCommandQueue(Rendering::QueueFamily::Graphics)
							);
							CommandEncoderView commandEncoder = commandBuffer.BeginEncoding(m_logicalDevice);

							{
								BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

								barrierCommandEncoder.TransitionImageLayout(
									GetSupportedPipelineStageFlags(uniformSubresourceState->m_imageLayout),
									GetSupportedAccessFlags(uniformSubresourceState->m_imageLayout),
									uniformSubresourceState->m_imageLayout,
									texture,
									subresourceRange
								);
							}

							const EncodedCommandBufferView encodedCommandBuffer = commandBuffer.StopEncoding();

							QueueSubmissionParameters parameters;
							parameters.m_finishedCallback = [&passInfo,
					                                     localAttachmentIndex,
					                                     imageAspectFlags,
					                                     resolution,
					                                     &framegraph,
					                                     &logicalDevice,
					                                     &texture,
					                                     commandBuffer = Move(commandBuffer),
					                                     &thread]() mutable
							{
								if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
								{
									[[maybe_unused]] const bool wasFirstLoad =
										passInfo.OnAttachmentLoaded(logicalDevice, texture, localAttachmentIndex, imageAspectFlags, resolution, framegraph);
								}
								else
								{
									Threading::JobManager& jobManager = System::Get<Threading::JobManager>();
									jobManager.QueueCallback(
										[&passInfo,
							       localAttachmentIndex,
							       imageAspectFlags,
							       resolution,
							       &framegraph,
							       &logicalDevice,
							       &texture](Threading::JobRunnerThread&)
										{
											[[maybe_unused]] const bool wasFirstLoad =
												passInfo.OnAttachmentLoaded(logicalDevice, texture, localAttachmentIndex, imageAspectFlags, resolution, framegraph);
										},
										Threading::JobPriority::CoreRenderStageResources
									);
								}

								thread.QueueExclusiveCallbackFromAnyThread(
									Threading::JobPriority::DeallocateResourcesMin,
									[commandBuffer = Move(commandBuffer), &logicalDevice](Threading::JobRunnerThread& thread) mutable
									{
										Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
										commandBuffer.Destroy(
											logicalDevice,
											engineThread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics)
										);
									}
								);
							};

							m_logicalDevice.GetQueueSubmissionJob(QueueFamily::Graphics)
								.Queue(
									Threading::JobPriority::CreateRenderTargetSubmission,
									ArrayView<const EncodedCommandBufferView, uint16>(encodedCommandBuffer),
									Move(parameters)
								);
						}
						else
						{
							[[maybe_unused]] const bool wasFirstLoad =
								passInfo.OnAttachmentLoaded(logicalDevice, texture, localAttachmentIndex, imageAspectFlags, resolution, framegraph);
						}

						return EventCallbackResult::Remove;
					}
				}
			);
			if (pJob != nullptr)
			{
				jobBatch.QueueAfterStartStage(*pJob);
			}
		}
	}

	void Framegraph::CompileRenderPassAttachments(
		const StageDescription& __restrict stageDescription,
		const QueueFamilyIndex graphicsQueueFamilyIndex,
		const PassIndex passIndex,
		PassInfo& __restrict passInfo,
		const AttachmentIdentifier renderOutputAttachmentIdentifier,
		IdentifierMask<AttachmentIdentifier>& processedAttachments,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
	)
	{
		const RenderPassDescription& __restrict passDescription = stageDescription.m_renderPassDescription;
		RenderPassInfo& renderPassInfo = *passInfo.GetRenderPassInfo();
		RenderSubpassInfo& __restrict subpassInfo = renderPassInfo.m_subpasses.EmplaceBack();
		if (stageDescription.m_pStage.IsValid())
		{
			subpassInfo.m_stages.EmplaceBack(*stageDescription.m_pStage);
		}

		renderPassInfo.m_drawableArea = passDescription.m_renderArea;

		const AttachmentIndex totalRenderAttachmentCount = passDescription.m_colorAttachments.GetSize() +
		                                                   (passDescription.m_depthAttachment.IsValid() |
		                                                    passDescription.m_stencilAttachment.IsValid());
		const AttachmentIndex totalAttachmentCount = totalRenderAttachmentCount + passDescription.m_inputAttachments.GetSize();
		passInfo.m_pendingCompilationTasks += totalAttachmentCount;
		m_pendingCompilationTasks += totalAttachmentCount;

		{

			const AttachmentIndex totalSubpassAttachmentCount = passDescription.m_colorAttachments.GetSize() +
			                                                    (passDescription.m_depthAttachment.IsValid() |
			                                                     passDescription.m_stencilAttachment.IsValid()) +
			                                                    passDescription.m_inputAttachments.GetSize();

			const FrameIndex imageCount = m_renderOutput.GetImageCount();
			renderPassInfo.m_imageMappingViews.Resize(imageCount);
			renderPassInfo.m_imageMappings.Resize(imageCount);
			renderPassInfo.m_textures.Resize(imageCount);

			subpassInfo.m_textures.Resize(imageCount);
			subpassInfo.m_imageMappings.Resize(imageCount);
			subpassInfo.m_imageMappingViews.Resize(imageCount);

			for (FrameIndex i = 0; i < imageCount; ++i)
			{
				renderPassInfo.m_imageMappingViews[i].Resize(totalAttachmentCount);
				renderPassInfo.m_imageMappings[i].Resize(totalAttachmentCount);
				renderPassInfo.m_textures[i].Resize(totalAttachmentCount);

				subpassInfo.m_textures[i].Resize(passDescription.m_colorAttachments.GetSize());
				subpassInfo.m_imageMappings[i].Resize(totalSubpassAttachmentCount);
				subpassInfo.m_imageMappingViews[i].Resize(totalSubpassAttachmentCount);
			}
		}
		renderPassInfo.m_imageResolutions.Resize(totalAttachmentCount);

		renderPassInfo.m_attachmentMappingTypes.Resize(totalAttachmentCount);
		renderPassInfo.m_attachmentDescriptions.Resize(totalRenderAttachmentCount);
		renderPassInfo.m_attachmentSubresourceRanges.Resize(totalRenderAttachmentCount);
		passInfo.m_attachmentIdentifiers.Resize(totalAttachmentCount);

		// Add pass dependencies
		AttachmentIndex nextAttachmentIndex{0};
		for (const ColorAttachmentDescription& __restrict colorAttachmentDescription : passDescription.m_colorAttachments)
		{
			const AttachmentIndex attachmentIndex = nextAttachmentIndex++;

			passInfo.m_attachmentIdentifiers[attachmentIndex] = colorAttachmentDescription.m_identifier;

			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[colorAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(colorAttachmentDescription.m_subresourceRange);

			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(colorAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			if (previousPassAttachmentReference.passIndex != InvalidPassIndex && !passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex))
			{
				passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);
			}

			if (colorAttachmentDescription.m_identifier == renderOutputAttachmentIdentifier)
			{
				if (!m_renderOutputDependencies.Contains(passIndex))
				{
					m_renderOutputDependencies.EmplaceBack(passIndex);
				}
			}
		}
		if (passDescription.m_depthAttachment.IsValid())
		{
			const DepthAttachmentDescription& __restrict depthAttachmentDescription = *passDescription.m_depthAttachment;

			const AttachmentIndex attachmentIndex = nextAttachmentIndex++;

			passInfo.m_attachmentIdentifiers[attachmentIndex] = depthAttachmentDescription.m_identifier;

			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[depthAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(depthAttachmentDescription.m_subresourceRange);

			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(depthAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			if (previousPassAttachmentReference.passIndex != InvalidPassIndex && !passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex))
			{
				passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);
			}
		}
		if (passDescription.m_stencilAttachment.IsValid())
		{
			const StencilAttachmentDescription& __restrict stencilAttachmentDescription = *passDescription.m_stencilAttachment;

			const bool isUniqueStencilAttachment = passDescription.m_depthAttachment.IsValid() &&
			                                       stencilAttachmentDescription.m_identifier != passDescription.m_depthAttachment->m_identifier;
			const AttachmentIndex attachmentIndex = isUniqueStencilAttachment ? nextAttachmentIndex++ : nextAttachmentIndex;

			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[stencilAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(stencilAttachmentDescription.m_subresourceRange);

			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(stencilAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			if (previousPassAttachmentReference.passIndex != InvalidPassIndex && !passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex))
			{
				passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);
			}

			if (isUniqueStencilAttachment)
			{
				passInfo.m_attachmentIdentifiers[attachmentIndex] = stencilAttachmentDescription.m_identifier;
			}
		}

		for (const InputAttachmentDescription& __restrict inputAttachmentDescription : passDescription.m_inputAttachments)
		{
			const AttachmentIndex attachmentIndex = nextAttachmentIndex++;

			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[inputAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(inputAttachmentDescription.m_subresourceRange);

			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(inputAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			Assert(
				previousPassAttachmentReference.passIndex != InvalidPassIndex,
				"Can't specify input attachment as the first usage of an attachment!"
			);
			passInfo.m_gpuDependencies.EmplaceBackUnique(PassIndex{previousPassAttachmentReference.passIndex});

			if (inputAttachmentDescription.m_identifier == renderOutputAttachmentIdentifier)
			{
				if (!m_renderOutputDependencies.Contains(passIndex))
				{
					m_renderOutputDependencies.EmplaceBack(passIndex);
				}
			}

			passInfo.m_attachmentIdentifiers[attachmentIndex] = inputAttachmentDescription.m_identifier;

			ImageLayout attachmentTargetLayout;
			if (inputAttachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil))
			{
				attachmentTargetLayout = ImageLayout::DepthStencilReadOnlyOptimal;
			}
			else
			{
				attachmentTargetLayout = ImageLayout::ShaderReadOnlyOptimal;
			}

			renderPassInfo.m_externalInputAttachmentStates.EmplaceBack(RenderPassInfo::ExternalInputAttachmentState{
				inputAttachmentDescription.m_identifier,
				attachmentIndex,
				inputAttachmentDescription.m_subresourceRange,
				SubresourceState{
					attachmentTargetLayout,
					PassAttachmentReference{passIndex, attachmentIndex},
					PipelineStageFlags::VertexShader | PipelineStageFlags::GeometryShader | PipelineStageFlags::FragmentShader,
					AccessFlags::ShaderRead,
					graphicsQueueFamilyIndex
				}
			});
		}

		// Resolve image layouts
		CompileColorAttachments(
			renderPassInfo,
			passDescription.m_colorAttachments,
			passIndex,
			graphicsQueueFamilyIndex,
			processedAttachments,
			attachmentInfos,
			renderOutputAttachmentIdentifier
		);

		AttachmentIndex baseAttachmentIndex = passDescription.m_colorAttachments.GetSize();
		if (passDescription.m_depthAttachment.IsValid())
		{
			const AttachmentIndex depthAttachmentIndex = baseAttachmentIndex++;
			CompileDepthAttachment(
				renderPassInfo,
				PassAttachmentReference{passIndex, depthAttachmentIndex},
				*passDescription.m_depthAttachment,
				graphicsQueueFamilyIndex,
				processedAttachments,
				attachmentInfos
			);
		}
		if (passDescription.m_stencilAttachment.IsValid())
		{
			const AttachmentIndex stencilAttachmentIndex = (passDescription.m_depthAttachment.IsInvalid() ||
			                                                passDescription.m_depthAttachment->m_identifier !=
			                                                  passDescription.m_stencilAttachment->m_identifier)
			                                                 ? baseAttachmentIndex++
			                                                 : (baseAttachmentIndex - 1);
			CompileStencilAttachment(
				renderPassInfo,
				PassAttachmentReference{passIndex, stencilAttachmentIndex},
				*passDescription.m_stencilAttachment,
				passDescription.m_depthAttachment,
				graphicsQueueFamilyIndex,
				processedAttachments,
				attachmentInfos
			);
		}

		// Resolve the image layouts of input attachments
		for (const InputAttachmentDescription& __restrict inputAttachmentDescription : passDescription.m_inputAttachments)
		{
			const AttachmentIndex inputAttachmentIndex = baseAttachmentIndex++;
			renderPassInfo.m_attachmentMappingTypes[inputAttachmentIndex] = inputAttachmentDescription.m_mappingType;

			// TODO: Be explicit about where these inputs are used
			CompileInputAttachment(
				PassAttachmentReference{passIndex, inputAttachmentIndex},
				inputAttachmentDescription.m_identifier,
				PipelineStageFlags::VertexShader | PipelineStageFlags::GeometryShader | PipelineStageFlags::FragmentShader,
				AccessFlags::ShaderRead,
				graphicsQueueFamilyIndex,
				inputAttachmentDescription.m_subresourceRange,
				attachmentInfos
			);
		}
	}

	void Framegraph::CompileExplicitRenderPassAttachments(
		const StageDescription& __restrict stageDescription,
		const QueueFamilyIndex graphicsQueueFamilyIndex,
		const PassIndex passIndex,
		PassInfo& __restrict passInfo,
		const AttachmentIdentifier renderOutputAttachmentIdentifier,
		IdentifierMask<AttachmentIdentifier>& processedAttachments,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
	)
	{
		const ExplicitRenderPassDescription& __restrict explicitPassDescription = stageDescription.m_explicitRenderPassDescription;
		RenderPassInfo& renderPassInfo = *passInfo.GetRenderPassInfo();

		renderPassInfo.m_subpasses.Reserve(explicitPassDescription.m_subpassDescriptions.GetSize());
		for (const RenderSubpassDescription& __restrict subpassDescription : explicitPassDescription.m_subpassDescriptions)
		{
			RenderSubpassInfo& __restrict subpassInfo = renderPassInfo.m_subpasses.EmplaceBack();
			if (subpassDescription.m_stages.HasElements())
			{
				subpassInfo.m_stages.CopyEmplaceRangeBack(subpassDescription.m_stages);
			}
			else
			{
				subpassInfo.m_stages.EmplaceBack(*stageDescription.m_pStage);
			}
		}

		renderPassInfo.m_drawableArea = explicitPassDescription.m_renderArea;

		const AttachmentIndex totalRenderAttachmentCount = explicitPassDescription.m_colorAttachments.GetSize() +
		                                                   (explicitPassDescription.m_depthAttachment.IsValid() |
		                                                    explicitPassDescription.m_stencilAttachment.IsValid());
		const AttachmentIndex totalAttachmentCount = totalRenderAttachmentCount + explicitPassDescription.m_externalInputAttachments.GetSize();
		passInfo.m_pendingCompilationTasks += totalAttachmentCount;
		m_pendingCompilationTasks += totalAttachmentCount;

		const FrameIndex imageCount = m_renderOutput.GetImageCount();
		renderPassInfo.m_imageMappingViews.Resize(imageCount);
		renderPassInfo.m_imageMappings.Resize(imageCount);
		renderPassInfo.m_textures.Resize(imageCount);

		for (FrameIndex i = 0; i < imageCount; ++i)
		{
			renderPassInfo.m_imageMappingViews[i].Resize(totalAttachmentCount);
			renderPassInfo.m_imageMappings[i].Resize(totalAttachmentCount);
			renderPassInfo.m_textures[i].Resize(totalAttachmentCount);
		}

		for (const RenderSubpassDescription& __restrict subpassDescription : explicitPassDescription.m_subpassDescriptions)
		{
			const SubpassIndex subpassIndex = explicitPassDescription.m_subpassDescriptions.GetIteratorIndex(&subpassDescription);
			RenderSubpassInfo& subpassInfo = renderPassInfo.m_subpasses[subpassIndex];

			const AttachmentIndex totalSubpassAttachmentCount = subpassDescription.m_colorAttachmentIndices.GetSize() +
			                                                    ((subpassDescription.m_depthAttachmentIndex != InvalidAttachmentIndex) |
			                                                     (subpassDescription.m_stencilAttachmentIndex != InvalidAttachmentIndex)) +
			                                                    subpassDescription.m_subpassInputAttachmentIndices.GetSize() +
			                                                    subpassDescription.m_externalInputAttachmentIndices.GetSize();

			subpassInfo.m_textures.Resize(imageCount);
			subpassInfo.m_imageMappings.Resize(imageCount);
			subpassInfo.m_imageMappingViews.Resize(imageCount);
			;
			for (FrameIndex i = 0; i < imageCount; ++i)
			{
				subpassInfo.m_textures[i].Resize(subpassDescription.m_colorAttachmentIndices.GetSize());
				subpassInfo.m_imageMappings[i].Resize(totalSubpassAttachmentCount);
				subpassInfo.m_imageMappingViews[i].Resize(totalSubpassAttachmentCount);
			}
		}
		renderPassInfo.m_imageResolutions.Resize(totalAttachmentCount);

		renderPassInfo.m_attachmentMappingTypes.Resize(totalAttachmentCount);
		renderPassInfo.m_attachmentDescriptions.Resize(totalRenderAttachmentCount);
		renderPassInfo.m_attachmentSubresourceRanges.Resize(totalRenderAttachmentCount);
		passInfo.m_attachmentIdentifiers.Resize(totalAttachmentCount);

		// Add pass dependencies
		AttachmentIndex nextAttachmentIndex{0};
		for (const ColorAttachmentDescription& __restrict colorAttachmentDescription : explicitPassDescription.m_colorAttachments)
		{
			const AttachmentIndex attachmentIndex = nextAttachmentIndex++;
			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[colorAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(colorAttachmentDescription.m_subresourceRange);
			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(colorAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			if (previousPassAttachmentReference.passIndex != InvalidPassIndex && !passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex))
			{
				passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);
			}

			passInfo.m_attachmentIdentifiers[attachmentIndex] = colorAttachmentDescription.m_identifier;

			if (colorAttachmentDescription.m_identifier == renderOutputAttachmentIdentifier)
			{
				if (!m_renderOutputDependencies.Contains(passIndex))
				{
					m_renderOutputDependencies.EmplaceBack(passIndex);
				}
			}
		}
		if (explicitPassDescription.m_depthAttachment.IsValid())
		{
			const AttachmentIndex attachmentIndex = nextAttachmentIndex++;
			const DepthAttachmentDescription& __restrict depthAttachmentDescription = *explicitPassDescription.m_depthAttachment;
			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[depthAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(depthAttachmentDescription.m_subresourceRange);

			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(depthAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			if (previousPassAttachmentReference.passIndex != InvalidPassIndex && !passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex))
			{
				passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);
			}

			passInfo.m_attachmentIdentifiers[attachmentIndex] = depthAttachmentDescription.m_identifier;
		}
		if (explicitPassDescription.m_stencilAttachment.IsValid())
		{
			const bool isUniqueStencilAttachment = explicitPassDescription.m_depthAttachment.IsValid() &&
			                                       explicitPassDescription.m_stencilAttachment->m_identifier !=
			                                         explicitPassDescription.m_depthAttachment->m_identifier;
			const AttachmentIndex attachmentIndex = isUniqueStencilAttachment ? nextAttachmentIndex++ : nextAttachmentIndex;
			const StencilAttachmentDescription& __restrict stencilAttachmentDescription = *explicitPassDescription.m_stencilAttachment;
			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[stencilAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(stencilAttachmentDescription.m_subresourceRange);

			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(stencilAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			if (previousPassAttachmentReference.passIndex != InvalidPassIndex && !passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex))
			{
				passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);
			}

			if (isUniqueStencilAttachment)
			{
				passInfo.m_attachmentIdentifiers[attachmentIndex] = stencilAttachmentDescription.m_identifier;
			}
		}

		for (const InputAttachmentDescription& __restrict inputAttachmentDescription : explicitPassDescription.m_externalInputAttachments)
		{
			const AttachmentIndex attachmentIndex = nextAttachmentIndex++;
			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[inputAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(inputAttachmentDescription.m_subresourceRange);

			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(inputAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			Assert(
				previousPassAttachmentReference.passIndex != InvalidPassIndex,
				"Can't specify input attachment as the first usage of an attachment!"
			);
			Assert(!passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex));
			passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);

			if (inputAttachmentDescription.m_identifier == renderOutputAttachmentIdentifier)
			{
				if (!m_renderOutputDependencies.Contains(passIndex))
				{
					m_renderOutputDependencies.EmplaceBack(passIndex);
				}
			}

			passInfo.m_attachmentIdentifiers[attachmentIndex] = inputAttachmentDescription.m_identifier;

			ImageLayout attachmentTargetLayout;
			if (inputAttachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil))
			{
				attachmentTargetLayout = ImageLayout::DepthStencilReadOnlyOptimal;
			}
			else
			{
				attachmentTargetLayout = ImageLayout::ShaderReadOnlyOptimal;
			}

			renderPassInfo.m_externalInputAttachmentStates.EmplaceBack(RenderPassInfo::ExternalInputAttachmentState{
				inputAttachmentDescription.m_identifier,
				attachmentIndex,
				inputAttachmentDescription.m_subresourceRange,
				SubresourceState{
					attachmentTargetLayout,
					PassAttachmentReference{passIndex, attachmentIndex},
					PipelineStageFlags::VertexShader | PipelineStageFlags::GeometryShader | PipelineStageFlags::FragmentShader,
					AccessFlags::ShaderRead,
					graphicsQueueFamilyIndex
				}
			});
		}

		// Resolve image layouts
		CompileColorAttachments(
			renderPassInfo,
			explicitPassDescription.m_colorAttachments,
			passIndex,
			graphicsQueueFamilyIndex,
			processedAttachments,
			attachmentInfos,
			renderOutputAttachmentIdentifier
		);

		AttachmentIndex baseAttachmentIndex = explicitPassDescription.m_colorAttachments.GetSize();
		if (explicitPassDescription.m_depthAttachment.IsValid())
		{
			const AttachmentIndex depthAttachmentIndex = baseAttachmentIndex++;
			CompileDepthAttachment(
				renderPassInfo,
				PassAttachmentReference{passIndex, depthAttachmentIndex},
				*explicitPassDescription.m_depthAttachment,
				graphicsQueueFamilyIndex,
				processedAttachments,
				attachmentInfos
			);
		}

		if (explicitPassDescription.m_stencilAttachment.IsValid())
		{
			const AttachmentIndex stencilAttachmentIndex = (explicitPassDescription.m_depthAttachment.IsInvalid() ||
			                                                explicitPassDescription.m_depthAttachment->m_identifier !=
			                                                  explicitPassDescription.m_stencilAttachment->m_identifier)
			                                                 ? baseAttachmentIndex++
			                                                 : (baseAttachmentIndex - 1);
			CompileStencilAttachment(
				renderPassInfo,
				PassAttachmentReference{passIndex, stencilAttachmentIndex},
				*explicitPassDescription.m_stencilAttachment,
				explicitPassDescription.m_depthAttachment,
				graphicsQueueFamilyIndex,
				processedAttachments,
				attachmentInfos
			);
		}

		// Resolve the image layouts of input attachments
		for (const InputAttachmentDescription& __restrict inputAttachmentDescription : explicitPassDescription.m_externalInputAttachments)
		{
			const AttachmentIndex inputAttachmentIndex = baseAttachmentIndex++;
			renderPassInfo.m_attachmentMappingTypes[inputAttachmentIndex] = inputAttachmentDescription.m_mappingType;

			// TODO: Be explicit about where these inputs are used
			CompileInputAttachment(
				PassAttachmentReference{passIndex, inputAttachmentIndex},
				inputAttachmentDescription.m_identifier,
				PipelineStageFlags::VertexShader | PipelineStageFlags::GeometryShader | PipelineStageFlags::FragmentShader,
				AccessFlags::ShaderRead,
				graphicsQueueFamilyIndex,
				inputAttachmentDescription.m_subresourceRange,
				attachmentInfos
			);
		}
	}

	void Framegraph::CompileGenericPassAttachments(
		const StageDescription& __restrict stageDescription,
		const QueueFamilyIndex queueFamilyIndex,
		const PassIndex passIndex,
		PassInfo& __restrict passInfo,
		const AttachmentIdentifier renderOutputAttachmentIdentifier,
		IdentifierMask<AttachmentIdentifier>& processedAttachments,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
	)
	{
		const GenericPassDescription& __restrict genericPassDescription = stageDescription.m_genericPassDescription;
		GenericPassInfo& genericPassInfo = *passInfo.GetGenericPassInfo();

		const AttachmentIndex totalAttachmentCount = genericPassDescription.m_outputAttachments.GetSize() +
		                                             genericPassDescription.m_inputOutputAttachments.GetSize() +
		                                             genericPassDescription.m_inputAttachments.GetSize();
		passInfo.m_pendingCompilationTasks += totalAttachmentCount;
		m_pendingCompilationTasks += totalAttachmentCount;

		passInfo.m_attachmentIdentifiers.Resize(totalAttachmentCount);

		genericPassInfo.m_subpasses.EmplaceBack(); // TODO: Support multiple
		GenericSubpassInfo& genericSubpassInfo = genericPassInfo.m_subpasses[0];

		const AttachmentIndex totalSubpassAttachmentCount = genericPassDescription.m_subpass.m_outputAttachments.GetSize() +
		                                                    genericPassDescription.m_subpass.m_inputOutputAttachments.GetSize() +
		                                                    genericPassDescription.m_subpass.m_inputAttachments.GetSize();

		const FrameIndex imageCount = m_renderOutput.GetImageCount();
		genericSubpassInfo.m_textures.Resize(imageCount);
		genericSubpassInfo.m_imageMappings.Resize(imageCount);
		genericSubpassInfo.m_imageMappingViews.Resize(imageCount);

		for (FrameIndex i = 0; i < imageCount; ++i)
		{
			genericSubpassInfo.m_imageMappingViews[i].Resize(totalSubpassAttachmentCount);
			genericSubpassInfo.m_imageMappings[i].Resize(totalSubpassAttachmentCount);
			genericSubpassInfo.m_textures[i].Resize(totalSubpassAttachmentCount);
		}
		genericSubpassInfo.m_imageResolutions.Resize(totalSubpassAttachmentCount);

		AttachmentIndex nextAttachmentIndex{0};
		for (const OutputAttachmentDescription& __restrict outputAttachmentDescription : genericPassDescription.m_outputAttachments)
		{
			const AttachmentIndex attachmentIndex = nextAttachmentIndex++;

			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[outputAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(outputAttachmentDescription.m_subresourceRange);

			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(outputAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			if (previousPassAttachmentReference.passIndex != InvalidPassIndex && !passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex))
			{
				passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);
			}

			if (outputAttachmentDescription.m_identifier == renderOutputAttachmentIdentifier)
			{
				if (!m_renderOutputDependencies.Contains(passIndex))
				{
					m_renderOutputDependencies.EmplaceBack(passIndex);
				}
			}

			passInfo.m_attachmentIdentifiers[attachmentIndex] = outputAttachmentDescription.m_identifier;
		}

		for (const InputOutputAttachmentDescription& __restrict inputOutputAttachmentDescription :
		     genericPassDescription.m_inputOutputAttachments)
		{
			const AttachmentIndex attachmentIndex = nextAttachmentIndex++;

			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[inputOutputAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(inputOutputAttachmentDescription.m_subresourceRange);
			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(inputOutputAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			Assert(
				previousPassAttachmentReference.passIndex != InvalidPassIndex,
				"Can't specify input output attachment as the first usage of an attachment!"
			);
			Assert(!passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex));
			passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);

			if (inputOutputAttachmentDescription.m_identifier == renderOutputAttachmentIdentifier)
			{
				if (!m_renderOutputDependencies.Contains(passIndex))
				{
					m_renderOutputDependencies.EmplaceBack(passIndex);
				}
			}

			passInfo.m_attachmentIdentifiers[attachmentIndex] = inputOutputAttachmentDescription.m_identifier;
		}

		for (const InputAttachmentDescription& __restrict inputAttachmentDescription : genericPassDescription.m_inputAttachments)
		{
			const AttachmentIndex attachmentIndex = nextAttachmentIndex++;

			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[inputAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(inputAttachmentDescription.m_subresourceRange);

			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(inputAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			Assert(
				previousPassAttachmentReference.passIndex != InvalidPassIndex,
				"Can't specify input attachment as the first usage of an attachment!"
			);
			if (!passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex))
			{
				passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);
			}

			if (inputAttachmentDescription.m_identifier == renderOutputAttachmentIdentifier)
			{
				if (!m_renderOutputDependencies.Contains(passIndex))
				{
					m_renderOutputDependencies.EmplaceBack(passIndex);
				}
			}

			passInfo.m_attachmentIdentifiers[attachmentIndex] = inputAttachmentDescription.m_identifier;
		}

		// Resolve attachment layouts
		for (const OutputAttachmentDescription& __restrict outputAttachmentDescription : genericPassDescription.m_outputAttachments)
		{
			const AttachmentIndex attachmentIndex = genericPassDescription.m_outputAttachments.GetIteratorIndex(&outputAttachmentDescription);
			ImageLayout attachmentTargetLayout;
			if (outputAttachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil))
			{
				attachmentTargetLayout = ImageLayout::DepthStencilAttachmentOptimal;
			}
			else
			{
				attachmentTargetLayout = ImageLayout::ColorAttachmentOptimal;
			}
			const EnumFlags<AccessFlags> accessFlags =
				outputAttachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil)
					? AccessFlags::DepthStencilWrite
					: AccessFlags::ColorAttachmentWrite;
			const EnumFlags<PipelineStageFlags> pipelineStageFlags =
				outputAttachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil)
					? PipelineStageFlags::LateFragmentTests
					: PipelineStageFlags::ColorAttachmentOutput;
			CompileOutputAttachment(
				PassAttachmentReference{passIndex, attachmentIndex},
				outputAttachmentDescription.m_identifier,
				pipelineStageFlags,
				accessFlags,
				attachmentTargetLayout,
				queueFamilyIndex,
				outputAttachmentDescription.m_subresourceRange,
				processedAttachments,
				attachmentInfos
			);
		}
		for (const InputOutputAttachmentDescription& __restrict inputOutputAttachmentDescription :
		     genericPassDescription.m_inputOutputAttachments)
		{
			const AttachmentIndex attachmentIndex =
				genericPassDescription.m_inputOutputAttachments.GetIteratorIndex(&inputOutputAttachmentDescription);
			CompileInputOutputAttachment(
				PassAttachmentReference{passIndex, attachmentIndex},
				inputOutputAttachmentDescription.m_identifier,
				PipelineStageFlags::ComputeShader,
				AccessFlags::ShaderReadWrite,
				ImageLayout::General,
				queueFamilyIndex,
				inputOutputAttachmentDescription.m_subresourceRange,
				attachmentInfos
			);
		}
		for (const InputAttachmentDescription& __restrict inputAttachmentDescription : genericPassDescription.m_inputAttachments)
		{
			const AttachmentIndex attachmentIndex = genericPassDescription.m_inputAttachments.GetIteratorIndex(&inputAttachmentDescription);
			CompileInputAttachment(
				PassAttachmentReference{passIndex, attachmentIndex},
				inputAttachmentDescription.m_identifier,
				PipelineStageFlags::ComputeShader,
				AccessFlags::ShaderRead,
				queueFamilyIndex,
				inputAttachmentDescription.m_subresourceRange,
				attachmentInfos
			);
		}
	}

	[[nodiscard]] const Framegraph::AttachmentDescription&
	GetAttachmentDescription(const ComputePassDescription& __restrict computePassDescription, Framegraph::AttachmentIndex attachmentIndex)
	{
		if (attachmentIndex < computePassDescription.m_outputAttachments.GetSize())
		{
			return computePassDescription.m_outputAttachments[attachmentIndex];
		}
		attachmentIndex -= computePassDescription.m_outputAttachments.GetSize();
		if (attachmentIndex < computePassDescription.m_inputOutputAttachments.GetSize())
		{
			return computePassDescription.m_inputOutputAttachments[attachmentIndex];
		}
		attachmentIndex -= computePassDescription.m_inputOutputAttachments.GetSize();
		return computePassDescription.m_inputAttachments[attachmentIndex];
	}

	void Framegraph::TryTransitionAttachmentLayout(
		const AttachmentDescription& __restrict attachmentDescription,
		const PassAttachmentReference attachmentReference,
		const EnumFlags<AccessFlags> accessFlags,
		const ImageLayout attachmentTargetLayout,
		const QueueFamilyIndex queueFamilyIndex,
		const ImageSubresourceRange subresourceRange,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
	)
	{
		Assert(GetSupportedAccessFlags(PipelineStageFlags::ComputeShader).AreAllSet(accessFlags));
		AttachmentInfo& __restrict attachmentInfo = attachmentInfos[attachmentDescription.m_identifier];

		attachmentInfo.OnUsed(attachmentReference);
		attachmentInfo.SetSubresourceState(
			subresourceRange,
			SubresourceState{attachmentTargetLayout, attachmentReference, PipelineStageFlags::ComputeShader, accessFlags, queueFamilyIndex}
		);
	};

	void Framegraph::CompileComputePassAttachments(
		const StageDescription& __restrict stageDescription,
		const QueueFamilyIndex computeQueueFamilyIndex,
		const PassIndex passIndex,
		PassInfo& __restrict passInfo,
		const AttachmentIdentifier renderOutputAttachmentIdentifier,
		IdentifierMask<AttachmentIdentifier>& processedAttachments,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos
	)
	{
		const ComputePassDescription& __restrict computePassDescription = stageDescription.m_computePassDescription;
		ComputePassInfo& __restrict computePassInfo = *passInfo.GetComputePassInfo();

		computePassInfo.m_subpasses.Reserve(computePassDescription.m_subpassDescriptions.GetSize());
		for ([[maybe_unused]] const ComputeSubpassDescription& __restrict subpassDescription : computePassDescription.m_subpassDescriptions)
		{
			[[maybe_unused]] ComputeSubpassInfo& subpass = computePassInfo.m_subpasses.EmplaceBack();
#if STAGE_DEPENDENCY_PROFILING
			subpass.m_debugName = String(subpassDescription.m_name);
#endif
		}

		const AttachmentIndex totalAttachmentCount = computePassDescription.m_outputAttachments.GetSize() +
		                                             computePassDescription.m_inputOutputAttachments.GetSize() +
		                                             computePassDescription.m_inputAttachments.GetSize();
		passInfo.m_pendingCompilationTasks += totalAttachmentCount;
		m_pendingCompilationTasks += totalAttachmentCount;

		passInfo.m_attachmentIdentifiers.Resize(totalAttachmentCount);

		const FrameIndex imageCount = m_renderOutput.GetImageCount();

		for (const ComputeSubpassDescription& __restrict subpassDescription : computePassDescription.m_subpassDescriptions)
		{
			const SubpassIndex subpassIndex = computePassDescription.m_subpassDescriptions.GetIteratorIndex(&subpassDescription);
			const AttachmentIndex totalSubpassAttachmentCount = subpassDescription.m_outputAttachments.GetSize() +
			                                                    subpassDescription.m_inputOutputAttachments.GetSize() +
			                                                    subpassDescription.m_inputAttachments.GetSize();
			ComputeSubpassInfo& __restrict subpassInfo = computePassInfo.m_subpasses[subpassIndex];

			subpassInfo.m_textures.Resize(imageCount);
			subpassInfo.m_imageMappings.Resize(imageCount);
			subpassInfo.m_imageMappingViews.Resize(imageCount);
			for (FrameIndex i = 0; i < imageCount; ++i)
			{
				subpassInfo.m_imageMappingViews[i].Resize(totalSubpassAttachmentCount);
				subpassInfo.m_imageMappings[i].Resize(totalSubpassAttachmentCount);
				subpassInfo.m_textures[i].Resize(totalSubpassAttachmentCount);
			}
			subpassInfo.m_imageResolutions.Resize(totalSubpassAttachmentCount);
		}

		AttachmentIndex nextAttachmentIndex{0};
		for (const OutputAttachmentDescription& __restrict outputAttachmentDescription : computePassDescription.m_outputAttachments)
		{
			const AttachmentIndex attachmentIndex = nextAttachmentIndex++;
			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[outputAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(outputAttachmentDescription.m_subresourceRange);

			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(outputAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			if (previousPassAttachmentReference.passIndex != InvalidPassIndex && !passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex))
			{
				passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);
			}

			if (outputAttachmentDescription.m_identifier == renderOutputAttachmentIdentifier)
			{
				if (!m_renderOutputDependencies.Contains(passIndex))
				{
					m_renderOutputDependencies.EmplaceBack(passIndex);
				}
			}

			passInfo.m_attachmentIdentifiers[attachmentIndex] = outputAttachmentDescription.m_identifier;
		}

		for (const InputOutputAttachmentDescription& __restrict inputOutputAttachmentDescription :
		     computePassDescription.m_inputOutputAttachments)
		{
			const AttachmentIndex attachmentIndex = nextAttachmentIndex++;
			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[inputOutputAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(inputOutputAttachmentDescription.m_subresourceRange);
			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(inputOutputAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			Assert(
				previousPassAttachmentReference.passIndex != InvalidPassIndex,
				"Can't specify input output attachment as the first usage of an attachment!"
			);
			Assert(!passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex));
			passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);

			if (inputOutputAttachmentDescription.m_identifier == renderOutputAttachmentIdentifier)
			{
				if (!m_renderOutputDependencies.Contains(passIndex))
				{
					m_renderOutputDependencies.EmplaceBack(passIndex);
				}
			}

			passInfo.m_attachmentIdentifiers[attachmentIndex] = inputOutputAttachmentDescription.m_identifier;
		}

		for (const InputAttachmentDescription& __restrict inputAttachmentDescription : computePassDescription.m_inputAttachments)
		{
			const AttachmentIndex attachmentIndex = nextAttachmentIndex++;
			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[inputAttachmentDescription.m_identifier];
			attachmentInfo.RegisterUsedSubresourceRange(inputAttachmentDescription.m_subresourceRange);
			const Optional<SubresourceState> previousSubresourceState =
				attachmentInfo.GetUniformSubresourceState(inputAttachmentDescription.m_subresourceRange);
			const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;
			Assert(
				previousPassAttachmentReference.passIndex != InvalidPassIndex,
				"Can't specify input attachment as the first usage of an attachment!"
			);
			if (!passInfo.m_gpuDependencies.Contains(previousPassAttachmentReference.passIndex))
			{
				passInfo.m_gpuDependencies.EmplaceBack(previousPassAttachmentReference.passIndex);
			}

			if (inputAttachmentDescription.m_identifier == renderOutputAttachmentIdentifier)
			{
				if (!m_renderOutputDependencies.Contains(passIndex))
				{
					m_renderOutputDependencies.EmplaceBack(passIndex);
				}
			}

			passInfo.m_attachmentIdentifiers[attachmentIndex] = inputAttachmentDescription.m_identifier;
		}

		// Resolve attachment layouts
		for (const OutputAttachmentDescription& __restrict outputAttachmentDescription : computePassDescription.m_outputAttachments)
		{
			const AttachmentIndex outputAttachmentIndex = computePassDescription.m_outputAttachments.GetIteratorIndex(&outputAttachmentDescription
			);
			CompileOutputAttachment(
				PassAttachmentReference{passIndex, outputAttachmentIndex},
				outputAttachmentDescription.m_identifier,
				PipelineStageFlags::ComputeShader,
				AccessFlags::ShaderWrite,
				ImageLayout::General,
				computeQueueFamilyIndex,
				outputAttachmentDescription.m_subresourceRange,
				processedAttachments,
				attachmentInfos
			);
		}
		AttachmentIndex baseAttachmentIndex = computePassDescription.m_outputAttachments.GetSize();
		for (const InputOutputAttachmentDescription& __restrict inputOutputAttachmentDescription :
		     computePassDescription.m_inputOutputAttachments)
		{
			const AttachmentIndex inputOutputAttachmentIndex =
				computePassDescription.m_inputOutputAttachments.GetIteratorIndex(&inputOutputAttachmentDescription);
			CompileInputOutputAttachment(
				PassAttachmentReference{passIndex, AttachmentIndex(baseAttachmentIndex + inputOutputAttachmentIndex)},
				inputOutputAttachmentDescription.m_identifier,
				PipelineStageFlags::ComputeShader,
				AccessFlags::ShaderReadWrite,
				ImageLayout::General,
				computeQueueFamilyIndex,
				inputOutputAttachmentDescription.m_subresourceRange,
				attachmentInfos
			);
		}
		baseAttachmentIndex += computePassDescription.m_inputOutputAttachments.GetSize();
		for (const InputAttachmentDescription& __restrict inputAttachmentDescription : computePassDescription.m_inputAttachments)
		{
			const AttachmentIndex inputAttachmentIndex = computePassDescription.m_inputAttachments.GetIteratorIndex(&inputAttachmentDescription);
			CompileInputAttachment(
				PassAttachmentReference{passIndex, AttachmentIndex(baseAttachmentIndex + inputAttachmentIndex)},
				inputAttachmentDescription.m_identifier,
				PipelineStageFlags::ComputeShader,
				AccessFlags::ShaderRead,
				computeQueueFamilyIndex,
				inputAttachmentDescription.m_subresourceRange,
				attachmentInfos
			);
		}

		// Create transitions between subpasses
		for (const ComputeSubpassDescription& __restrict subpassDescription : computePassDescription.m_subpassDescriptions.GetSubViewFrom(1))
		{
			const SubpassIndex subpassIndex = computePassDescription.m_subpassDescriptions.GetIteratorIndex(&subpassDescription);

			for (const SubpassAttachmentReference attachmentReference : subpassDescription.m_outputAttachments)
			{
				const AttachmentDescription& __restrict attachmentDescription =
					GetAttachmentDescription(computePassDescription, attachmentReference.m_attachmentIndex);
				Assert(
					attachmentReference.m_mipRange.GetIndex() >= attachmentDescription.m_subresourceRange.m_mipRange.GetIndex() &&
					attachmentReference.m_mipRange.GetEnd() <= attachmentDescription.m_subresourceRange.m_mipRange.GetEnd()
				);

				constexpr ImageLayout attachmentTargetLayout = ImageLayout::General;
				const ImageSubresourceRange imageSubresourceRange{
					attachmentDescription.m_subresourceRange.m_aspectMask,
					attachmentReference.m_mipRange,
					attachmentDescription.m_subresourceRange.m_arrayRange
				};
				TryTransitionAttachmentLayout(
					attachmentDescription,
					PassAttachmentReference{passIndex, attachmentReference.m_attachmentIndex, subpassIndex},
					AccessFlags::ShaderWrite,
					attachmentTargetLayout,
					computeQueueFamilyIndex,
					imageSubresourceRange,
					attachmentInfos
				);
			}

			for (const SubpassAttachmentReference attachmentReference : subpassDescription.m_inputOutputAttachments)
			{
				const AttachmentDescription& __restrict attachmentDescription =
					GetAttachmentDescription(computePassDescription, attachmentReference.m_attachmentIndex);
				Assert(
					attachmentReference.m_mipRange.GetIndex() >= attachmentDescription.m_subresourceRange.m_mipRange.GetIndex() &&
					attachmentReference.m_mipRange.GetEnd() <= attachmentDescription.m_subresourceRange.m_mipRange.GetEnd()
				);

				constexpr ImageLayout attachmentTargetLayout = ImageLayout::General;
				const ImageSubresourceRange imageSubresourceRange{
					attachmentDescription.m_subresourceRange.m_aspectMask,
					attachmentReference.m_mipRange,
					attachmentDescription.m_subresourceRange.m_arrayRange
				};
				TryTransitionAttachmentLayout(
					attachmentDescription,
					PassAttachmentReference{passIndex, attachmentReference.m_attachmentIndex, subpassIndex},
					AccessFlags::ShaderReadWrite,
					attachmentTargetLayout,
					computeQueueFamilyIndex,
					imageSubresourceRange,
					attachmentInfos
				);
			}

			for (const SubpassAttachmentReference attachmentReference : subpassDescription.m_inputAttachments)
			{
				const AttachmentDescription& __restrict attachmentDescription =
					GetAttachmentDescription(computePassDescription, attachmentReference.m_attachmentIndex);
				Assert(
					attachmentReference.m_mipRange.GetIndex() >= attachmentDescription.m_subresourceRange.m_mipRange.GetIndex() &&
					attachmentReference.m_mipRange.GetEnd() <= attachmentDescription.m_subresourceRange.m_mipRange.GetEnd()
				);

				ImageLayout attachmentTargetLayout;
				if (attachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil))
				{
					attachmentTargetLayout = ImageLayout::DepthStencilReadOnlyOptimal;
				}
				else
				{
					attachmentTargetLayout = ImageLayout::ShaderReadOnlyOptimal;
				}
				const ImageSubresourceRange imageSubresourceRange{
					attachmentDescription.m_subresourceRange.m_aspectMask,
					attachmentReference.m_mipRange,
					attachmentDescription.m_subresourceRange.m_arrayRange
				};
				TryTransitionAttachmentLayout(
					attachmentDescription,
					PassAttachmentReference{passIndex, attachmentReference.m_attachmentIndex, subpassIndex},
					AccessFlags::ShaderRead,
					attachmentTargetLayout,
					computeQueueFamilyIndex,
					imageSubresourceRange,
					attachmentInfos
				);
			}
		}
	}

	void Framegraph::CreateRenderPass(
		const StageDescription& __restrict stageDescription,
		PassInfo& __restrict passInfo,
		const AttachmentIdentifier renderOutputAttachmentIdentifier,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos,
		TextureCache& textureCache,
		RenderTargetCache& renderTargetCache,
		Threading::JobBatch& jobBatch
	)
	{
		const RenderPassDescription& __restrict passDescription = stageDescription.m_renderPassDescription;
		RenderPassInfo& renderPassInfo = *passInfo.GetRenderPassInfo();

		// Populate clear values
		Pass::ClearValues clearValues;
		clearValues.Resize(
			passDescription.m_colorAttachments.GetSize() +
			(passDescription.m_depthAttachment.IsValid() | passDescription.m_stencilAttachment.IsValid())
		);

		for (const ColorAttachmentDescription& __restrict colorAttachmentDescription : passDescription.m_colorAttachments)
		{
			const AttachmentIndex colorAttachmentIndex = passDescription.m_colorAttachments.GetIteratorIndex(&colorAttachmentDescription);
			if (colorAttachmentDescription.m_clearValue.IsValid())
			{
				clearValues[colorAttachmentIndex] = *colorAttachmentDescription.m_clearValue;
			}
		}
		if (passDescription.m_depthAttachment.IsValid())
		{
			const DepthAttachmentDescription& __restrict depthAttachmentDescription = *passDescription.m_depthAttachment;
			const AttachmentIndex depthStencilAttachmentIndex = passDescription.m_colorAttachments.GetSize();
			if (depthAttachmentDescription.m_clearValue.IsValid())
			{
				clearValues[depthStencilAttachmentIndex] = *depthAttachmentDescription.m_clearValue;
			}
		}
		if (passDescription.m_stencilAttachment.IsValid())
		{
			const StencilAttachmentDescription& __restrict stencilAttachmentDescription = *passDescription.m_stencilAttachment;
			const AttachmentIndex depthStencilAttachmentIndex = passDescription.m_colorAttachments.GetSize();
			if (stencilAttachmentDescription.m_clearValue.IsValid())
			{
				clearValues[depthStencilAttachmentIndex] = *stencilAttachmentDescription.m_clearValue;
			}
		}

		constexpr SubpassIndex subpassCount = 1; // todo: merging
		FlatVector<RenderSubpassDescription, subpassCount, SubpassIndex>
			subpassDescriptions(Memory::ConstructWithSize, Memory::DefaultConstruct, subpassCount);

		InlineVector<AttachmentIndex, 8> subpassAttachmentIndices;

		{
			RenderSubpassInfo& __restrict subpassInfo = renderPassInfo.m_subpasses.GetLastElement(); // todo: merging
			RenderSubpassDescription& __restrict subpassDescription = subpassDescriptions.GetLastElement();

			const AttachmentIndex attachmentCount = passDescription.m_colorAttachments.GetSize() + passDescription.m_depthAttachment.IsValid() +
			                                        passDescription.m_inputAttachments.GetSize();
			subpassInfo.m_attachmentReferences.Reserve(attachmentCount);
			subpassAttachmentIndices.Reserve(attachmentCount);

			for (const ColorAttachmentDescription& __restrict colorAttachmentDescription : passDescription.m_colorAttachments)
			{
				const AttachmentIndex colorAttachmentIndex = passDescription.m_colorAttachments.GetIteratorIndex(&colorAttachmentDescription);
				const ImageLayout requiredAttachmentLayout = ImageLayout::ColorAttachmentOptimal;
				subpassInfo.m_attachmentReferences.EmplaceBack(AttachmentReference{colorAttachmentIndex, requiredAttachmentLayout});

				subpassAttachmentIndices.EmplaceBack(colorAttachmentIndex);
			}
			subpassInfo.m_colorAttachments = {subpassInfo.m_attachmentReferences.GetSubView(0, passDescription.m_colorAttachments.GetSize())};
			subpassDescription.m_colorAttachmentIndices = {subpassAttachmentIndices.GetSubView(0, passDescription.m_colorAttachments.GetSize())};

			if (passDescription.m_depthAttachment.IsValid())
			{
				const AttachmentIndex depthAttachmentIndex = passDescription.m_colorAttachments.GetSize();
				const Rendering::AttachmentDescription& __restrict depthAttachmentDescription =
					renderPassInfo.m_attachmentDescriptions[depthAttachmentIndex];

				subpassInfo.m_pDepthAttachment = subpassInfo.m_attachmentReferences.EmplaceBack(
					AttachmentReference{depthAttachmentIndex, depthAttachmentDescription.m_initialLayout}
				);

				subpassDescription.m_depthAttachmentIndex = depthAttachmentIndex;

				if (passDescription.m_stencilAttachment.IsValid())
				{
					Assert(passDescription.m_depthAttachment->m_identifier == passDescription.m_stencilAttachment->m_identifier);
					subpassDescription.m_stencilAttachmentIndex = depthAttachmentIndex;
				}
			}
			else if (passDescription.m_stencilAttachment.IsValid())
			{
				const AttachmentIndex stencilAttachmentIndex = passDescription.m_colorAttachments.GetSize();
				const Rendering::AttachmentDescription& __restrict stencilAttachmentDescription =
					renderPassInfo.m_attachmentDescriptions[stencilAttachmentIndex];

				subpassInfo.m_pDepthAttachment = subpassInfo.m_attachmentReferences.EmplaceBack(
					AttachmentReference{stencilAttachmentIndex, stencilAttachmentDescription.m_initialLayout}
				);

				subpassDescription.m_stencilAttachmentIndex = stencilAttachmentIndex;
			}

			for (const InputAttachmentDescription& __restrict inputAttachmentDescription : passDescription.m_inputAttachments)
			{
				const AttachmentIndex inputAttachmentIndex = passDescription.m_colorAttachments.GetSize() +
				                                             passDescription.m_depthAttachment.IsValid() +
				                                             passDescription.m_inputAttachments.GetIteratorIndex(&inputAttachmentDescription);

				const ImageLayout requiredAttachmentLayout = ImageLayout::ShaderReadOnlyOptimal;
				subpassInfo.m_attachmentReferences.EmplaceBack(AttachmentReference{inputAttachmentIndex, requiredAttachmentLayout});

				subpassAttachmentIndices.EmplaceBack(inputAttachmentIndex);
			}
			subpassInfo.m_externalInputAttachments = {subpassInfo.m_attachmentReferences.GetSubView(
				passDescription.m_colorAttachments.GetSize() + passDescription.m_depthAttachment.IsValid(),
				passDescription.m_inputAttachments.GetSize()
			)};
			subpassDescription.m_externalInputAttachmentIndices = {subpassAttachmentIndices.GetSubView(
				passDescription.m_colorAttachments.GetSize() + passDescription.m_depthAttachment.IsValid(),
				passDescription.m_inputAttachments.GetSize()
			)};

			// TODO: Merged pass input attachments
			/*for (const InputAttachmentDescription& __restrict inputAttachmentDescription : stageDescription.m_inputAttachments)
			{
			  const AttachmentIndex inputAttachmentIndex = stageDescription.m_colorAttachments.GetSize() +
			stageDescription.m_depthAttachment.IsValid() +
			stageDescription.m_inputAttachments.GetIteratorIndex(&inputAttachmentDescription);

			  subpassInfo.m_attachmentReferences.EmplaceBack(AttachmentReference
			  {
			    inputAttachmentIndex,
			    inputAttachmentDescription.m_requestedLayout
			  });
			}
			subpassInfo.m_inputAttachments = { subpassInfo.m_attachmentReferences.GetSubView(stageDescription.m_colorAttachments.GetSize() +
			stageDescription.m_depthAttachment.IsValid(), stageDescription.m_inputAttachments.GetSize())};*/
			subpassInfo.m_subpassInputAttachments = {};

			subpassInfo.m_resolveAttachments = {};
		}

		renderPassInfo.m_subpassDependencies.Reserve(subpassCount + 1);

		uint32 previousSubpassIndex{ExternalSubpass};
		EnumFlags<PipelineStageFlags> previousPipelineStageFlags{PipelineStageFlags::BottomOfPipe};
		EnumFlags<AccessFlags> previousAccessFlags{AccessFlags::MemoryRead};

		for (SubpassIndex subpassIndex = 0; subpassIndex < subpassCount; ++subpassIndex)
		{
			renderPassInfo.m_subpassDependencies.EmplaceBack(GetNextSubpassDependency(
				subpassDescriptions[subpassIndex],
				subpassDescriptions.GetView().GetSubViewFrom(subpassIndex + 1),
				renderPassInfo.m_attachmentDescriptions,
				subpassIndex,
				previousSubpassIndex,
				previousPipelineStageFlags,
				previousAccessFlags
			));
		}

		renderPassInfo.m_subpassDependencies.EmplaceBack(Rendering::SubpassDependency{
			previousSubpassIndex,
			Rendering::ExternalSubpass,
			previousPipelineStageFlags,
			Rendering::PipelineStageFlags::BottomOfPipe,
			previousAccessFlags,
			Rendering::AccessFlags::MemoryRead,
			Rendering::DependencyFlags::ByRegion
		});

		renderPassInfo.m_pPass.CreateInPlace(m_logicalDevice, m_renderOutput, *this, Move(clearValues), subpassCount, passInfo);
		RenderPassStage& __restrict passStage = *renderPassInfo.m_pPass;

		// TODO: Pass merging
		const SubpassIndex subpassIndex = 0;
		if (stageDescription.m_pStage.IsValid())
		{
			passStage.AddStage(*stageDescription.m_pStage, subpassIndex);
		}

		// Load attachments
		for (const ColorAttachmentDescription& __restrict colorAttachmentDescription : passDescription.m_colorAttachments)
		{
			const AttachmentIndex colorAttachmentIndex = passDescription.m_colorAttachments.GetIteratorIndex(&colorAttachmentDescription);
			QueueLoadAttachment(
				passInfo,
				colorAttachmentDescription,
				colorAttachmentIndex,
				ImageAspectFlags::Color,
				jobBatch,
				renderOutputAttachmentIdentifier,
				attachmentInfos,
				textureCache,
				renderTargetCache
			);
		}

		AttachmentIndex baseAttachmentIndex = passDescription.m_colorAttachments.GetSize();
		if (passDescription.m_depthAttachment.IsValid())
		{
			const DepthAttachmentDescription& __restrict depthAttachmentDescription = *passDescription.m_depthAttachment;
			const AttachmentIndex depthStencilAttachmentIndex = baseAttachmentIndex++;
			QueueLoadAttachment(
				passInfo,
				depthAttachmentDescription,
				depthStencilAttachmentIndex,
				ImageAspectFlags::Depth | (ImageAspectFlags::Stencil * passDescription.m_stencilAttachment.IsValid()),
				jobBatch,
				renderOutputAttachmentIdentifier,
				attachmentInfos,
				textureCache,
				renderTargetCache
			);
		}
		if(passDescription.m_stencilAttachment.IsValid() && (passDescription.m_depthAttachment.IsInvalid() || passDescription.m_depthAttachment->m_identifier != passDescription.m_stencilAttachment->m_identifier))
		{
			const StencilAttachmentDescription& __restrict stencilAttachmentDescription = *passDescription.m_stencilAttachment;
			const AttachmentIndex depthStencilAttachmentIndex = baseAttachmentIndex++;
			QueueLoadAttachment(
				passInfo,
				stencilAttachmentDescription,
				depthStencilAttachmentIndex,
				ImageAspectFlags::Stencil,
				jobBatch,
				renderOutputAttachmentIdentifier,
				attachmentInfos,
				textureCache,
				renderTargetCache
			);
		}

		for (const InputAttachmentDescription& __restrict inputAttachmentDescription : passDescription.m_inputAttachments)
		{
			const AttachmentIndex localInputAttachmentIndex = passDescription.m_inputAttachments.GetIteratorIndex(&inputAttachmentDescription);
			const AttachmentIndex inputAttachmentIndex = baseAttachmentIndex + localInputAttachmentIndex;
			QueueLoadAttachment(
				passInfo,
				inputAttachmentDescription,
				inputAttachmentIndex,
				inputAttachmentDescription.m_subresourceRange.m_aspectMask,
				jobBatch,
				renderOutputAttachmentIdentifier,
				attachmentInfos,
				textureCache,
				renderTargetCache
			);
		}
	}

	void Framegraph::CreateExplicitRenderPass(
		const StageDescription& __restrict stageDescription,
		PassInfo& __restrict passInfo,
		const AttachmentIdentifier renderOutputAttachmentIdentifier,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos,
		TextureCache& textureCache,
		RenderTargetCache& renderTargetCache,
		Threading::JobBatch& jobBatch
	)
	{
		const ExplicitRenderPassDescription& __restrict explicitPassDescription = stageDescription.m_explicitRenderPassDescription;
		RenderPassInfo& renderPassInfo = *passInfo.GetRenderPassInfo();

		// Populate clear values
		Pass::ClearValues clearValues;
		clearValues.Resize(
			explicitPassDescription.m_colorAttachments.GetSize() +
			(explicitPassDescription.m_depthAttachment.IsValid() | explicitPassDescription.m_stencilAttachment.IsValid())
		);

		for (const ColorAttachmentDescription& __restrict colorAttachmentDescription : explicitPassDescription.m_colorAttachments)
		{
			const AttachmentIndex colorAttachmentIndex = explicitPassDescription.m_colorAttachments.GetIteratorIndex(&colorAttachmentDescription);
			if (colorAttachmentDescription.m_clearValue.IsValid())
			{
				clearValues[colorAttachmentIndex] = *colorAttachmentDescription.m_clearValue;
			}
		}
		if (explicitPassDescription.m_depthAttachment.IsValid())
		{
			const DepthAttachmentDescription& __restrict depthAttachmentDescription = *explicitPassDescription.m_depthAttachment;
			const AttachmentIndex depthStencilAttachmentIndex = explicitPassDescription.m_colorAttachments.GetSize();
			if (depthAttachmentDescription.m_clearValue.IsValid())
			{
				clearValues[depthStencilAttachmentIndex] = *depthAttachmentDescription.m_clearValue;
			}
		}
		if (explicitPassDescription.m_stencilAttachment.IsValid())
		{
			const StencilAttachmentDescription& __restrict stencilAttachmentDescription = *explicitPassDescription.m_stencilAttachment;
			const AttachmentIndex depthStencilAttachmentIndex = explicitPassDescription.m_colorAttachments.GetSize();
			if (stencilAttachmentDescription.m_clearValue.IsValid())
			{
				clearValues[depthStencilAttachmentIndex] = *stencilAttachmentDescription.m_clearValue;
			}
		}

		for (const RenderSubpassDescription& __restrict subpassDescription : explicitPassDescription.m_subpassDescriptions)
		{
			const SubpassIndex subpassIndex = explicitPassDescription.m_subpassDescriptions.GetIteratorIndex(&subpassDescription);

			RenderSubpassInfo& __restrict subpassInfo = renderPassInfo.m_subpasses[subpassIndex];
			const bool hasDepthStencilAttachment = subpassDescription.m_depthAttachmentIndex != InvalidAttachmentIndex ||
			                                       subpassDescription.m_stencilAttachmentIndex != InvalidAttachmentIndex;
			const AttachmentIndex attachmentCount = subpassDescription.m_colorAttachmentIndices.GetSize() + hasDepthStencilAttachment +
			                                        subpassDescription.m_externalInputAttachmentIndices.GetSize() +
			                                        subpassDescription.m_subpassInputAttachmentIndices.GetSize();
			subpassInfo.m_attachmentReferences.Reserve(attachmentCount);

			for (const AttachmentIndex colorAttachmentIndex : subpassDescription.m_colorAttachmentIndices)
			{
				const ImageLayout requiredAttachmentLayout = ImageLayout::ColorAttachmentOptimal;
				subpassInfo.m_attachmentReferences.EmplaceBack(AttachmentReference{colorAttachmentIndex, requiredAttachmentLayout});
			}
			subpassInfo.m_colorAttachments = {
				subpassInfo.m_attachmentReferences.GetSubView(0, subpassDescription.m_colorAttachmentIndices.GetSize())
			};

			AttachmentIndex baseAttachmentIndex = explicitPassDescription.m_colorAttachments.GetSize();
			if (hasDepthStencilAttachment)
			{
				const AttachmentIndex depthAttachmentIndex = baseAttachmentIndex++;
				const Rendering::AttachmentDescription& __restrict depthAttachmentDescription =
					renderPassInfo.m_attachmentDescriptions[depthAttachmentIndex];

				subpassInfo.m_pDepthAttachment = subpassInfo.m_attachmentReferences.EmplaceBack(
					AttachmentReference{depthAttachmentIndex, depthAttachmentDescription.m_initialLayout}
				);
			}

			for (const AttachmentIndex inputAttachmentIndex : subpassDescription.m_subpassInputAttachmentIndices)
			{
				const bool isColorAttachment = inputAttachmentIndex < explicitPassDescription.m_colorAttachments.GetSize();
				const ImageLayout subpassInputImageLayout = isColorAttachment ? ImageLayout::ShaderReadOnlyOptimal
				                                                              : ImageLayout::DepthStencilReadOnlyOptimal;

				subpassInfo.m_attachmentReferences.EmplaceBack(AttachmentReference{inputAttachmentIndex, subpassInputImageLayout});
			}
			subpassInfo.m_subpassInputAttachments = {subpassInfo.m_attachmentReferences.GetSubView(
				subpassDescription.m_colorAttachmentIndices.GetSize() + hasDepthStencilAttachment,
				subpassDescription.m_subpassInputAttachmentIndices.GetSize()
			)};

			for (const AttachmentIndex inputAttachmentIndex : subpassDescription.m_externalInputAttachmentIndices)
			{
				const ImageLayout requiredImageLayout = ImageLayout::ShaderReadOnlyOptimal;
				subpassInfo.m_attachmentReferences.EmplaceBack(AttachmentReference{inputAttachmentIndex, requiredImageLayout});
			}
			subpassInfo.m_externalInputAttachments = {subpassInfo.m_attachmentReferences.GetSubView(
				subpassDescription.m_colorAttachmentIndices.GetSize() + hasDepthStencilAttachment +
					subpassDescription.m_subpassInputAttachmentIndices.GetSize(),
				subpassDescription.m_externalInputAttachmentIndices.GetSize()
			)};

			subpassInfo.m_resolveAttachments = {};
		}

		// Create subpass dependencies
		const SubpassIndex subpassCount = explicitPassDescription.m_subpassDescriptions.GetSize();
		renderPassInfo.m_subpassDependencies.Reserve(subpassCount + 1);

		uint32 previousSubpassIndex{ExternalSubpass};
		EnumFlags<PipelineStageFlags> previousPipelineStageFlags{PipelineStageFlags::BottomOfPipe};
		EnumFlags<AccessFlags> previousAccessFlags{AccessFlags::MemoryRead};

		for (SubpassIndex subpassIndex = 0; subpassIndex < subpassCount; ++subpassIndex)
		{
			renderPassInfo.m_subpassDependencies.EmplaceBack(GetNextSubpassDependency(
				explicitPassDescription.m_subpassDescriptions[subpassIndex],
				explicitPassDescription.m_subpassDescriptions.GetSubViewFrom(subpassIndex + 1),
				renderPassInfo.m_attachmentDescriptions,
				subpassIndex,
				previousSubpassIndex,
				previousPipelineStageFlags,
				previousAccessFlags
			));
		}

		renderPassInfo.m_subpassDependencies.EmplaceBack(Rendering::SubpassDependency{
			previousSubpassIndex,
			Rendering::ExternalSubpass,
			previousPipelineStageFlags,
			Rendering::PipelineStageFlags::BottomOfPipe,
			previousAccessFlags,
			Rendering::AccessFlags::MemoryRead,
			Rendering::DependencyFlags::ByRegion
		});

		renderPassInfo.m_pPass.CreateInPlace(m_logicalDevice, m_renderOutput, *this, Move(clearValues), subpassCount, passInfo);
		RenderPassStage& __restrict passStage = *renderPassInfo.m_pPass;
		for (const RenderSubpassDescription& __restrict subpassDescription : explicitPassDescription.m_subpassDescriptions)
		{
			const SubpassIndex subpassIndex = explicitPassDescription.m_subpassDescriptions.GetIteratorIndex(&subpassDescription);
			if (subpassDescription.m_stages.HasElements())
			{
				for (Stage& subpassStage : subpassDescription.m_stages)
				{
					passStage.AddStage(subpassStage, subpassIndex);
				}
			}
			else
			{
				passStage.AddStage(*stageDescription.m_pStage, subpassIndex);
			}
		}

		// Load attachments
		for (const ColorAttachmentDescription& __restrict colorAttachmentDescription : explicitPassDescription.m_colorAttachments)
		{
			const AttachmentIndex colorAttachmentIndex = explicitPassDescription.m_colorAttachments.GetIteratorIndex(&colorAttachmentDescription);
			QueueLoadAttachment(
				passInfo,
				colorAttachmentDescription,
				colorAttachmentIndex,
				ImageAspectFlags::Color,
				jobBatch,
				renderOutputAttachmentIdentifier,
				attachmentInfos,
				textureCache,
				renderTargetCache
			);
		}

		AttachmentIndex baseAttachmentIndex = explicitPassDescription.m_colorAttachments.GetSize();
		if (explicitPassDescription.m_depthAttachment.IsValid())
		{
			const DepthAttachmentDescription& __restrict depthAttachmentDescription = *explicitPassDescription.m_depthAttachment;
			const AttachmentIndex depthStencilAttachmentIndex = baseAttachmentIndex++;
			QueueLoadAttachment(
				passInfo,
				depthAttachmentDescription,
				depthStencilAttachmentIndex,
				ImageAspectFlags::Depth | (ImageAspectFlags::Stencil * explicitPassDescription.m_stencilAttachment.IsValid()),
				jobBatch,
				renderOutputAttachmentIdentifier,
				attachmentInfos,
				textureCache,
				renderTargetCache
			);
		}
		if(explicitPassDescription.m_stencilAttachment.IsValid() && (explicitPassDescription.m_depthAttachment.IsInvalid() || explicitPassDescription.m_depthAttachment->m_identifier != explicitPassDescription.m_stencilAttachment->m_identifier))
		{
			const StencilAttachmentDescription& __restrict stencilAttachmentDescription = *explicitPassDescription.m_stencilAttachment;
			const AttachmentIndex depthStencilAttachmentIndex = baseAttachmentIndex++;
			QueueLoadAttachment(
				passInfo,
				stencilAttachmentDescription,
				depthStencilAttachmentIndex,
				ImageAspectFlags::Stencil,
				jobBatch,
				renderOutputAttachmentIdentifier,
				attachmentInfos,
				textureCache,
				renderTargetCache
			);
		}
		for (const InputAttachmentDescription& __restrict inputAttachmentDescription : explicitPassDescription.m_externalInputAttachments)
		{
			const AttachmentIndex localInputAttachmentIndex =
				explicitPassDescription.m_externalInputAttachments.GetIteratorIndex(&inputAttachmentDescription);
			const AttachmentIndex inputAttachmentIndex = baseAttachmentIndex + localInputAttachmentIndex;
			QueueLoadAttachment(
				passInfo,
				inputAttachmentDescription,
				inputAttachmentIndex,
				inputAttachmentDescription.m_subresourceRange.m_aspectMask,
				jobBatch,
				renderOutputAttachmentIdentifier,
				attachmentInfos,
				textureCache,
				renderTargetCache
			);
		}
	}

	void Framegraph::CreateGenericPass(
		const StageDescription& __restrict stageDescription,
		const PassIndex passIndex,
		PassInfo& __restrict passInfo,
		const AttachmentIdentifier renderOutputAttachmentIdentifier,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos,
		const QueueFamilyIndex queueFamilyIndex,
		TextureCache& textureCache,
		RenderTargetCache& renderTargetCache,
		Threading::JobBatch& jobBatch
	)
	{
		GenericPassInfo& genericPassInfo = *passInfo.GetGenericPassInfo();

		const GenericPassDescription& __restrict genericPassDescription = stageDescription.m_genericPassDescription;

		// for(const GenericSubpassDescription& __restrict subpassDescription : genericPassDescription.m_subpassDescriptions)
		{
			const GenericSubpassDescription& __restrict subpassDescription = genericPassDescription.m_subpass;

			const SubpassIndex subpassIndex = 0; // genericPassDescription.m_subpassDescriptions.GetIteratorIndex(&subpassDescription);
			GenericSubpassInfo& __restrict subpassInfo = genericPassInfo.m_subpasses[subpassIndex];

			const AttachmentIndex attachmentCount = subpassDescription.m_outputAttachments.GetSize() +
			                                        subpassDescription.m_inputOutputAttachments.GetSize() +
			                                        subpassDescription.m_inputAttachments.GetSize();
			subpassInfo.m_attachmentReferences.Reserve(attachmentCount);
			subpassInfo.m_requiredAttachmentStates.Resize(attachmentCount);

			for (const SubpassAttachmentReference& outputAttachmentReference : subpassDescription.m_outputAttachments)
			{
				const AttachmentIndex subpassAttachmentIndex = subpassDescription.m_outputAttachments.GetIteratorIndex(&outputAttachmentReference);
				subpassInfo.m_attachmentReferences.EmplaceBack(outputAttachmentReference);

				const OutputAttachmentDescription& outputAttachmentDescription =
					genericPassDescription.m_outputAttachments[outputAttachmentReference.m_attachmentIndex];

				ImageLayout requiredAttachmentLayout;
				if (outputAttachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil))
				{
					requiredAttachmentLayout = ImageLayout::DepthStencilAttachmentOptimal;
				}
				else
				{
					requiredAttachmentLayout = ImageLayout::ColorAttachmentOptimal;
				}

				// Indicate the required state of the subresources in the attachment
				SubresourceStates& attachmentSubresourceStates = subpassInfo.m_requiredAttachmentStates[subpassAttachmentIndex];
				attachmentSubresourceStates.RegisterUsedSubresourceRange(outputAttachmentDescription.m_subresourceRange);
				attachmentSubresourceStates.SetSubresourceState(
					outputAttachmentDescription.m_subresourceRange,
					SubresourceState{
						requiredAttachmentLayout,
						PassAttachmentReference{passIndex, outputAttachmentReference.m_attachmentIndex, subpassIndex},
						GetSupportedPipelineStageFlags(requiredAttachmentLayout),
						GetSupportedAccessFlags(requiredAttachmentLayout),
						queueFamilyIndex
					}
				);
			}
			subpassInfo.m_outputAttachments = {subpassInfo.m_attachmentReferences.GetSubView(0, subpassDescription.m_outputAttachments.GetSize())
			};

			AttachmentIndex subpassAttachmentIndexOffset{subpassDescription.m_outputAttachments.GetSize()};
			AttachmentIndex passAttachmentIndexOffset{genericPassDescription.m_outputAttachments.GetSize()};
			for (const SubpassAttachmentReference& inputOutputAttachmentReference : subpassDescription.m_inputOutputAttachments)
			{
				const AttachmentIndex subpassAttachmentIndex =
					subpassAttachmentIndexOffset + subpassDescription.m_inputOutputAttachments.GetIteratorIndex(&inputOutputAttachmentReference);
				subpassInfo.m_attachmentReferences.EmplaceBack(inputOutputAttachmentReference);

				Assert(inputOutputAttachmentReference.m_attachmentIndex >= passAttachmentIndexOffset);
				const InputOutputAttachmentDescription& inputOutputAttachmentDescription =
					genericPassDescription.m_inputOutputAttachments[inputOutputAttachmentReference.m_attachmentIndex - passAttachmentIndexOffset];

				const ImageLayout requiredAttachmentLayout = ImageLayout::General;
				EnumFlags<PipelineStageFlags> pipelineStageFlags =
					inputOutputAttachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil)
						? (PipelineStageFlags::EarlyFragmentTests | PipelineStageFlags::LateFragmentTests | PipelineStageFlags::ComputeShader |
				       PipelineStageFlags::FragmentShader)
						: (PipelineStageFlags::FragmentShader | PipelineStageFlags::ColorAttachmentOutput | PipelineStageFlags::ComputeShader);
				pipelineStageFlags &= GetSupportedPipelineStageFlags(requiredAttachmentLayout);

				EnumFlags<AccessFlags> accessFlags =
					inputOutputAttachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil)
						? (AccessFlags::DepthStencilReadWrite | AccessFlags::InputAttachmentRead | AccessFlags::ShaderReadWrite)
						: (AccessFlags::ColorAttachmentReadWrite | AccessFlags::InputAttachmentRead | AccessFlags::ShaderReadWrite);
				accessFlags &= GetSupportedAccessFlags(requiredAttachmentLayout);

				// Indicate the required state of the subresources in the attachment
				SubresourceStates& attachmentSubresourceStates = subpassInfo.m_requiredAttachmentStates[subpassAttachmentIndex];
				attachmentSubresourceStates.RegisterUsedSubresourceRange(inputOutputAttachmentDescription.m_subresourceRange);
				attachmentSubresourceStates.SetSubresourceState(
					inputOutputAttachmentDescription.m_subresourceRange,
					SubresourceState{
						requiredAttachmentLayout,
						PassAttachmentReference{passIndex, inputOutputAttachmentReference.m_attachmentIndex, subpassIndex},
						pipelineStageFlags,
						accessFlags,
						queueFamilyIndex
					}
				);
			}
			subpassInfo.m_inputOutputAttachments = {subpassInfo.m_attachmentReferences.GetSubView(
				subpassDescription.m_outputAttachments.GetSize(),
				subpassDescription.m_inputOutputAttachments.GetSize()
			)};

			subpassAttachmentIndexOffset += subpassDescription.m_inputOutputAttachments.GetSize();
			passAttachmentIndexOffset += genericPassDescription.m_inputOutputAttachments.GetSize();
			for (const SubpassAttachmentReference& inputAttachmentReference : subpassDescription.m_inputAttachments)
			{
				const AttachmentIndex subpassAttachmentIndex = subpassAttachmentIndexOffset +
				                                               subpassDescription.m_inputAttachments.GetIteratorIndex(&inputAttachmentReference);
				subpassInfo.m_attachmentReferences.EmplaceBack(inputAttachmentReference);

				Assert(inputAttachmentReference.m_attachmentIndex >= passAttachmentIndexOffset);
				const InputAttachmentDescription& inputAttachmentDescription =
					genericPassDescription.m_inputAttachments[inputAttachmentReference.m_attachmentIndex - passAttachmentIndexOffset];

				ImageLayout requiredAttachmentLayout;
				if (inputAttachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil))
				{
					requiredAttachmentLayout = ImageLayout::DepthStencilReadOnlyOptimal;
				}
				else
				{
					requiredAttachmentLayout = ImageLayout::ShaderReadOnlyOptimal;
				}

				EnumFlags<PipelineStageFlags> pipelineStageFlags =
					inputAttachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil)
						? (PipelineStageFlags::EarlyFragmentTests | PipelineStageFlags::ComputeShader | PipelineStageFlags::FragmentShader)
						: (PipelineStageFlags::FragmentShader | PipelineStageFlags::ComputeShader);
				pipelineStageFlags &= GetSupportedPipelineStageFlags(requiredAttachmentLayout);

				EnumFlags<AccessFlags> accessFlags =
					inputAttachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil)
						? (AccessFlags::DepthStencilRead | AccessFlags::InputAttachmentRead | AccessFlags::ShaderRead)
						: (AccessFlags::ColorAttachmentRead | AccessFlags::InputAttachmentRead | AccessFlags::ShaderRead);
				accessFlags &= GetSupportedAccessFlags(requiredAttachmentLayout);

				// Indicate the required state of the subresources in the attachment
				SubresourceStates& attachmentSubresourceStates = subpassInfo.m_requiredAttachmentStates[subpassAttachmentIndex];
				attachmentSubresourceStates.RegisterUsedSubresourceRange(inputAttachmentDescription.m_subresourceRange);
				attachmentSubresourceStates.SetSubresourceState(
					inputAttachmentDescription.m_subresourceRange,
					SubresourceState{
						requiredAttachmentLayout,
						PassAttachmentReference{passIndex, inputAttachmentReference.m_attachmentIndex, subpassIndex},
						pipelineStageFlags,
						accessFlags,
						queueFamilyIndex
					}
				);
			}
			subpassInfo.m_inputAttachments = {subpassInfo.m_attachmentReferences.GetSubView(
				subpassDescription.m_outputAttachments.GetSize() + subpassDescription.m_inputOutputAttachments.GetSize(),
				subpassDescription.m_inputAttachments.GetSize()
			)};
		}

		// Queue attachments for load
		{
			for (const OutputAttachmentDescription& __restrict outputAttachmentDescription : genericPassDescription.m_outputAttachments)
			{
				const AttachmentIndex localOutputAttachmentIndex =
					genericPassDescription.m_outputAttachments.GetIteratorIndex(&outputAttachmentDescription);
				QueueLoadAttachment(
					passInfo,
					outputAttachmentDescription,
					localOutputAttachmentIndex,
					outputAttachmentDescription.m_subresourceRange.m_aspectMask,
					jobBatch,
					renderOutputAttachmentIdentifier,
					attachmentInfos,
					textureCache,
					renderTargetCache
				);
			}

			AttachmentIndex baseAttachmentIndex = genericPassDescription.m_outputAttachments.GetSize();
			for (const InputOutputAttachmentDescription& __restrict inputOutputAttachmentDescription :
			     genericPassDescription.m_inputOutputAttachments)
			{
				const AttachmentIndex localInputOutputAttachmentIndex =
					genericPassDescription.m_inputOutputAttachments.GetIteratorIndex(&inputOutputAttachmentDescription);
				const AttachmentIndex inputOutputAttachmentIndex = baseAttachmentIndex + localInputOutputAttachmentIndex;
				QueueLoadAttachment(
					passInfo,
					inputOutputAttachmentDescription,
					inputOutputAttachmentIndex,
					inputOutputAttachmentDescription.m_subresourceRange.m_aspectMask,
					jobBatch,
					renderOutputAttachmentIdentifier,
					attachmentInfos,
					textureCache,
					renderTargetCache
				);
			}
			baseAttachmentIndex += genericPassDescription.m_inputOutputAttachments.GetSize();

			for (const InputAttachmentDescription& __restrict inputAttachmentDescription : genericPassDescription.m_inputAttachments)
			{
				const AttachmentIndex localInputAttachmentIndex =
					genericPassDescription.m_inputAttachments.GetIteratorIndex(&inputAttachmentDescription);
				const AttachmentIndex inputAttachmentIndex = baseAttachmentIndex + localInputAttachmentIndex;
				QueueLoadAttachment(
					passInfo,
					inputAttachmentDescription,
					inputAttachmentIndex,
					inputAttachmentDescription.m_subresourceRange.m_aspectMask,
					jobBatch,
					renderOutputAttachmentIdentifier,
					attachmentInfos,
					textureCache,
					renderTargetCache
				);
			}
		}

		genericPassInfo.m_pPass.CreateInPlace(m_logicalDevice, m_renderOutput, *this, passInfo);
		stageDescription.m_pStage->SetManagedByRenderPass();
	}

	void Framegraph::CreateComputePass(
		const StageDescription& __restrict stageDescription,
		const PassIndex passIndex,
		PassInfo& __restrict passInfo,
		const AttachmentIdentifier renderOutputAttachmentIdentifier,
		FixedIdentifierArrayView<AttachmentInfo, AttachmentIdentifier> attachmentInfos,
		const QueueFamilyIndex queueFamilyIndex,
		TextureCache& textureCache,
		RenderTargetCache& renderTargetCache,
		Threading::JobBatch& jobBatch
	)
	{
		ComputePassInfo& computePassInfo = *passInfo.GetComputePassInfo();

		const ComputePassDescription& __restrict computePassDescription = stageDescription.m_computePassDescription;

		for (const ComputeSubpassDescription& __restrict subpassDescription : computePassDescription.m_subpassDescriptions)
		{
			const SubpassIndex subpassIndex = computePassDescription.m_subpassDescriptions.GetIteratorIndex(&subpassDescription);

			ComputeSubpassInfo& __restrict subpassInfo = computePassInfo.m_subpasses[subpassIndex];

			const AttachmentIndex attachmentCount = subpassDescription.m_outputAttachments.GetSize() +
			                                        subpassDescription.m_inputOutputAttachments.GetSize() +
			                                        subpassDescription.m_inputAttachments.GetSize();
			subpassInfo.m_attachmentReferences.Reserve(attachmentCount);
			subpassInfo.m_requiredAttachmentStates.Resize(attachmentCount);

			AttachmentIndex nextLocalAttachmentIndex{0};
			for (const SubpassAttachmentReference outputAttachmentReference : subpassDescription.m_outputAttachments)
			{
				const AttachmentIndex localAttachmentIndex = nextLocalAttachmentIndex++;
				subpassInfo.m_attachmentReferences.EmplaceBack(outputAttachmentReference);

				const AttachmentDescription& __restrict outputAttachmentDescription =
					computePassDescription.GetAttachmentDescription(outputAttachmentReference.m_attachmentIndex);

				// Indicate the required state of the subresources in the attachment
				SubresourceStates& attachmentSubresourceStates = subpassInfo.m_requiredAttachmentStates[localAttachmentIndex];
				attachmentSubresourceStates.RegisterUsedSubresourceRange(outputAttachmentDescription.m_subresourceRange);
				attachmentSubresourceStates.SetSubresourceState(
					outputAttachmentDescription.m_subresourceRange,
					SubresourceState{
						ImageLayout::General,
						PassAttachmentReference{passIndex, outputAttachmentReference.m_attachmentIndex, subpassIndex},
						PipelineStageFlags::ComputeShader,
						AccessFlags::ShaderWrite,
						queueFamilyIndex
					}
				);
			}
			subpassInfo.m_outputAttachments = {subpassInfo.m_attachmentReferences.GetSubView(0, subpassDescription.m_outputAttachments.GetSize())
			};

			for (const SubpassAttachmentReference inputOutputAttachmentReference : subpassDescription.m_inputOutputAttachments)
			{
				const AttachmentIndex localAttachmentIndex = nextLocalAttachmentIndex++;
				subpassInfo.m_attachmentReferences.EmplaceBack(inputOutputAttachmentReference);

				const AttachmentDescription& __restrict inputOutputAttachmentDescription =
					computePassDescription.GetAttachmentDescription(inputOutputAttachmentReference.m_attachmentIndex);

				// Indicate the required state of the subresources in the attachment
				SubresourceStates& attachmentSubresourceStates = subpassInfo.m_requiredAttachmentStates[localAttachmentIndex];
				attachmentSubresourceStates.RegisterUsedSubresourceRange(inputOutputAttachmentDescription.m_subresourceRange);
				attachmentSubresourceStates.SetSubresourceState(
					inputOutputAttachmentDescription.m_subresourceRange,
					SubresourceState{
						ImageLayout::General,
						PassAttachmentReference{passIndex, inputOutputAttachmentReference.m_attachmentIndex, subpassIndex},
						PipelineStageFlags::ComputeShader,
						AccessFlags::ShaderReadWrite,
						queueFamilyIndex
					}
				);
			}
			subpassInfo.m_inputOutputAttachments = {subpassInfo.m_attachmentReferences.GetSubView(
				subpassDescription.m_outputAttachments.GetSize(),
				subpassDescription.m_inputOutputAttachments.GetSize()
			)};

			for (const SubpassAttachmentReference inputAttachmentReference : subpassDescription.m_inputAttachments)
			{
				const AttachmentIndex localAttachmentIndex = nextLocalAttachmentIndex++;
				subpassInfo.m_attachmentReferences.EmplaceBack(inputAttachmentReference);

				const AttachmentDescription& __restrict inputAttachmentDescription =
					computePassDescription.GetAttachmentDescription(inputAttachmentReference.m_attachmentIndex);
				ImageLayout attachmentTargetLayout;
				if (inputAttachmentDescription.m_subresourceRange.m_aspectMask.AreAnySet(ImageAspectFlags::DepthStencil))
				{
					attachmentTargetLayout = ImageLayout::DepthStencilReadOnlyOptimal;
				}
				else
				{
					attachmentTargetLayout = ImageLayout::ShaderReadOnlyOptimal;
				}

				// Indicate the required state of the subresources in the attachment
				SubresourceStates& attachmentSubresourceStates = subpassInfo.m_requiredAttachmentStates[localAttachmentIndex];
				attachmentSubresourceStates.RegisterUsedSubresourceRange(inputAttachmentDescription.m_subresourceRange);
				attachmentSubresourceStates.SetSubresourceState(
					inputAttachmentDescription.m_subresourceRange,
					SubresourceState{
						attachmentTargetLayout,
						PassAttachmentReference{passIndex, inputAttachmentReference.m_attachmentIndex, subpassIndex},
						PipelineStageFlags::ComputeShader,
						AccessFlags::ShaderRead,
						queueFamilyIndex
					}
				);
			}
			subpassInfo.m_inputAttachments = {subpassInfo.m_attachmentReferences.GetSubView(
				subpassDescription.m_outputAttachments.GetSize() + subpassDescription.m_inputOutputAttachments.GetSize(),
				subpassDescription.m_inputAttachments.GetSize()
			)};
		}

		for (const OutputAttachmentDescription& __restrict outputAttachmentDescription : computePassDescription.m_outputAttachments)
		{
			const AttachmentIndex localOutputAttachmentIndex =
				computePassDescription.m_outputAttachments.GetIteratorIndex(&outputAttachmentDescription);
			QueueLoadAttachment(
				passInfo,
				outputAttachmentDescription,
				localOutputAttachmentIndex,
				outputAttachmentDescription.m_subresourceRange.m_aspectMask,
				jobBatch,
				renderOutputAttachmentIdentifier,
				attachmentInfos,
				textureCache,
				renderTargetCache
			);
		}
		AttachmentIndex baseAttachmentIndex = computePassDescription.m_outputAttachments.GetSize();
		for (const InputOutputAttachmentDescription& __restrict inputOutputAttachmentDescription :
		     computePassDescription.m_inputOutputAttachments)
		{
			const AttachmentIndex localInputOutputAttachmentIndex =
				computePassDescription.m_inputOutputAttachments.GetIteratorIndex(&inputOutputAttachmentDescription);
			const AttachmentIndex inputOutputAttachmentIndex = baseAttachmentIndex + localInputOutputAttachmentIndex;
			QueueLoadAttachment(
				passInfo,
				inputOutputAttachmentDescription,
				inputOutputAttachmentIndex,
				inputOutputAttachmentDescription.m_subresourceRange.m_aspectMask,
				jobBatch,
				renderOutputAttachmentIdentifier,
				attachmentInfos,
				textureCache,
				renderTargetCache
			);
		}
		baseAttachmentIndex += computePassDescription.m_inputOutputAttachments.GetSize();
		;
		for (const InputAttachmentDescription& __restrict inputAttachmentDescription : computePassDescription.m_inputAttachments)
		{
			const AttachmentIndex localInputAttachmentIndex =
				computePassDescription.m_inputAttachments.GetIteratorIndex(&inputAttachmentDescription);
			const AttachmentIndex inputAttachmentIndex = baseAttachmentIndex + localInputAttachmentIndex;
			QueueLoadAttachment(
				passInfo,
				inputAttachmentDescription,
				inputAttachmentIndex,
				inputAttachmentDescription.m_subresourceRange.m_aspectMask,
				jobBatch,
				renderOutputAttachmentIdentifier,
				attachmentInfos,
				textureCache,
				renderTargetCache
			);
		}

		computePassInfo.m_pPass.CreateInPlace(m_logicalDevice, m_renderOutput, *this, passInfo);
		for (const ComputeSubpassDescription& __restrict subpassDescription : computePassDescription.m_subpassDescriptions)
		{
			const SubpassIndex subpassIndex = computePassDescription.m_subpassDescriptions.GetIteratorIndex(&subpassDescription);

			computePassInfo.m_subpasses[subpassIndex].m_stages.EmplaceBack(*stageDescription.m_pStage);
			stageDescription.m_pStage->SetManagedByRenderPass();
		}
	}

	void Framegraph::Compile(const ArrayView<const StageDescription, StageIndex> stageDescriptions, Threading::JobBatch& jobBatch)
	{
		{
			[[maybe_unused]] const uint16 previousCompilationTaskCount = m_pendingCompilationTasks.FetchAdd(1);
			Assert(previousCompilationTaskCount == 0);
		}

		Assert(m_passes.IsEmpty());
		// Reserve for the worst case scenario where we have one pass per stage
		m_passes.Reserve(stageDescriptions.GetSize());
		m_stagePassIndices.Resize(stageDescriptions.GetSize(), Memory::Uninitialized);

		m_attachmentInfos.GetView().DestroyElements();
		m_attachmentInfos.GetView().DefaultConstruct();
		TIdentifierArray<AttachmentInfo, AttachmentIdentifier>& attachmentInfos = m_attachmentInfos;

		IdentifierMask<AttachmentIdentifier> processedAttachments;

		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		TextureCache& textureCache = renderer.GetTextureCache();
		RenderTargetCache& renderTargetCache = m_renderOutput.GetRenderTargetCache();

		const AttachmentIdentifier renderOutputAttachmentIdentifier =
			textureCache.FindOrRegisterRenderTargetTemplate(RenderOutputRenderTargetGuid);

		AttachmentInfo& renderOutputAttachmentInfo = attachmentInfos[renderOutputAttachmentIdentifier];
		const ImageSubresourceRange renderOutputSubresourceRange{ImageAspectFlags::Color, MipRange(0, 1), ArrayRange(0, 1)};
		renderOutputAttachmentInfo.RegisterUsedSubresourceRange(renderOutputSubresourceRange);

		const QueueFamilyIndex graphicsQueueFamilyIndex = m_logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics);
		// Currently executing all compute on the graphics queue.
		const QueueFamilyIndex computeQueueFamilyIndex =
			graphicsQueueFamilyIndex; // m_logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Compute);

		for (const StageDescription& __restrict stageDescription : stageDescriptions)
		{
			const StageIndex stageIndex = stageDescriptions.GetIteratorIndex(&stageDescription);

			// TODO: Merge passes
			const PassIndex passIndex = m_passes.GetNextAvailableIndex();
			PassInfo& __restrict passInfo =
				m_passes.EmplaceBack(stageDescription.m_type, stageDescription.m_pStage, stageDescription.m_pSceneViewDrawer);
			m_pendingCompilationTasks++;

			m_stagePassIndices[stageIndex] = passIndex;

#if STAGE_DEPENDENCY_PROFILING
			passInfo.m_debugName = stageDescription.m_name;
#endif

			if (stageDescription.m_previousStageIndex != Framegraph::InvalidStageIndex)
			{
				passInfo.m_cpuDependencies.EmplaceBack(stageDescription.m_previousStageIndex);
				passInfo.m_gpuDependencies.EmplaceBack(stageDescription.m_previousStageIndex);
			}

			switch (stageDescription.m_type)
			{
				case StageType::RenderPass:
				{
					CompileRenderPassAttachments(
						stageDescription,
						graphicsQueueFamilyIndex,
						passIndex,
						passInfo,
						renderOutputAttachmentIdentifier,
						processedAttachments,
						attachmentInfos
					);
				}
				break;
				case StageType::ExplicitRenderPass:
				{
					CompileExplicitRenderPassAttachments(
						stageDescription,
						graphicsQueueFamilyIndex,
						passIndex,
						passInfo,
						renderOutputAttachmentIdentifier,
						processedAttachments,
						attachmentInfos
					);
				}
				break;
				case StageType::Generic:
				{
					CompileGenericPassAttachments(
						stageDescription,
						graphicsQueueFamilyIndex,
						passIndex,
						passInfo,
						renderOutputAttachmentIdentifier,
						processedAttachments,
						attachmentInfos
					);
				}
				break;
				case StageType::Compute:
				{
					CompileComputePassAttachments(
						stageDescription,
						computeQueueFamilyIndex,
						passIndex,
						passInfo,
						renderOutputAttachmentIdentifier,
						processedAttachments,
						attachmentInfos
					);
				}
				break;
			}
		}

		// Ensure that the stage before present always stores the result
		{
			const Optional<SubresourceState> previousSubresourceState =
				renderOutputAttachmentInfo.GetUniformSubresourceState(renderOutputSubresourceRange);
			if (previousSubresourceState.IsValid() && previousSubresourceState->WasUsed())
			{
				const PassAttachmentReference previousPassAttachmentReference = previousSubresourceState->m_attachmentReference;

				PassInfo& __restrict previousPassInfo = m_passes[previousPassAttachmentReference.passIndex];
				if (const Optional<RenderPassInfo*> pPreviousRenderPassInfo = previousPassInfo.GetRenderPassInfo())
				{
					Rendering::AttachmentDescription& __restrict previousPassAttachmentDescription =
						pPreviousRenderPassInfo->m_attachmentDescriptions[previousPassAttachmentReference.attachmentIndex];
					previousPassAttachmentDescription.m_storeType = AttachmentStoreType::Store;

					[[maybe_unused]] const ImageSubresourceRange previousPassSubresourceRange =
						pPreviousRenderPassInfo->m_attachmentSubresourceRanges[previousPassAttachmentReference.attachmentIndex];
					Assert(previousPassSubresourceRange.Contains(renderOutputSubresourceRange));
				}
			}
		}

		for (const StageDescription& __restrict stageDescription : stageDescriptions)
		{
			const StageIndex stageIndex = stageDescriptions.GetIteratorIndex(&stageDescription);

			// TODO: Merge passes
			const PassIndex passIndex = m_stagePassIndices[stageIndex];
			PassInfo& __restrict passInfo = m_passes[passIndex];

			switch (stageDescription.m_type)
			{
				case StageType::RenderPass:
				{
					CreateRenderPass(
						stageDescription,
						passInfo,
						renderOutputAttachmentIdentifier,
						attachmentInfos,
						textureCache,
						renderTargetCache,
						jobBatch
					);
				}
				break;
				case StageType::ExplicitRenderPass:
				{
					CreateExplicitRenderPass(
						stageDescription,
						passInfo,
						renderOutputAttachmentIdentifier,
						attachmentInfos,
						textureCache,
						renderTargetCache,
						jobBatch
					);
				}
				break;
				case StageType::Generic:
				{
					CreateGenericPass(
						stageDescription,
						passIndex,
						passInfo,
						renderOutputAttachmentIdentifier,
						attachmentInfos,
						graphicsQueueFamilyIndex,
						textureCache,
						renderTargetCache,
						jobBatch
					);
				}
				break;
				case StageType::Compute:
				{
					CreateComputePass(
						stageDescription,
						passIndex,
						passInfo,
						renderOutputAttachmentIdentifier,
						attachmentInfos,
						computeQueueFamilyIndex,
						textureCache,
						renderTargetCache,
						jobBatch
					);
				}
				break;
			}
		}

		{
			const Optional<SubresourceState> previousSubresourceState =
				renderOutputAttachmentInfo.GetUniformSubresourceState(renderOutputSubresourceRange);
			const Optional<SubresourceState> initialSubresourceState =
				renderOutputAttachmentInfo.GetUniformSubresourceState(renderOutputSubresourceRange, SubresourceStates::Bucket::Initial);
			m_firstRenderOutputPassIndex = initialSubresourceState->m_attachmentReference.passIndex;
			m_lastRenderOutputPassIndex = previousSubresourceState->m_attachmentReference.passIndex;
		}

		{
			const ImageLayout presentImageLayout = m_renderOutput.GetPresentColorImageLayout();
			if (renderOutputAttachmentInfo.GetUniformSubresourceState(renderOutputSubresourceRange)->m_imageLayout != presentImageLayout)
			{
				renderOutputAttachmentInfo.RequestOrTransitionLayout(
					m_passes,
					PassAttachmentReference{},
					GetSupportedPipelineStageFlags(presentImageLayout),
					GetSupportedAccessFlags(presentImageLayout),
					presentImageLayout,
					m_logicalDevice.GetPresentQueueIndex(),
					renderOutputAttachmentInfo.GetSubresourceRange()
				);
				renderOutputAttachmentInfo.LockFinalLayout();
			}
		}

		// Transition attachments back to their initial layouts
		for (const AttachmentIdentifier::IndexType attachmentIdentifierIndex : processedAttachments.GetSetBitsIterator())
		{
			const AttachmentIdentifier attachmentIdentifier = AttachmentIdentifier::MakeFromValidIndex(attachmentIdentifierIndex);
			AttachmentInfo& __restrict attachmentInfo = attachmentInfos[attachmentIdentifier];
			const Optional<SubresourceState> initialSubresourceState =
				attachmentInfo.GetUniformSubresourceState(attachmentInfo.GetSubresourceRange(), SubresourceStates::Bucket::Initial);
			if (initialSubresourceState.IsValid() && initialSubresourceState->m_imageLayout != ImageLayout::Undefined)
			{
				attachmentInfo.RequestOrTransitionLayout(
					m_passes,
					PassAttachmentReference{},
					initialSubresourceState->m_pipelineStageFlags,
					initialSubresourceState->m_accessFlags,
					initialSubresourceState->m_imageLayout,
					initialSubresourceState->m_queueFamilyIndex,
					attachmentInfo.GetSubresourceRange()
				);
			}
		}

		// Indicate compilation complete
		for (PassInfo& __restrict passInfo : m_passes)
		{
			passInfo.OnPendingCompilationTaskCompleted(m_logicalDevice, *this);
		}

		if (jobBatch.IsInvalid())
		{
			jobBatch = Threading::JobBatch::IntermediateStage;
		}
		m_pCompiledDependenciesStage = Threading::CreateIntermediateStage();
		m_pCompiledDependenciesStage->AddSubsequentStage(jobBatch.GetFinishedStage());

		OnPendingCompilationTaskCompleted();
	}

	void Framegraph::OnPendingCompilationTaskCompleted()
	{
		[[maybe_unused]] const uint16 previousCompilationTaskCount = m_pendingCompilationTasks.FetchSubtract(1);
		Assert(previousCompilationTaskCount > 0);
		if (previousCompilationTaskCount == 1)
		{
			m_pCompiledDependenciesStage->SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
			m_pCompiledDependenciesStage = {};
		}
	}

	bool PassInfo::OnAttachmentLoaded(
		LogicalDevice& logicalDevice,
		RenderTexture& texture,
		const AttachmentIndex localAttachmentIndex,
		const EnumFlags<ImageAspectFlags> imageAspectFlags,
		const Math::Vector2ui resolution,
		Framegraph& framegraph
	)
	{
		const Rendering::Format textureFormat = texture.GetFormat();
		const FrameIndex imageCount = framegraph.m_renderOutput.GetImageCount();

		switch (m_type)
		{
			case StageType::RenderPass:
			case StageType::ExplicitRenderPass:
			{
				if (m_renderPassInfo.m_imageMappings[0][localAttachmentIndex].IsValid())
				{
					// Attachment already loaded
					return false;
				}

				ImageMapping imageMapping(
					logicalDevice,
					texture,
					m_renderPassInfo.m_attachmentMappingTypes[localAttachmentIndex],
					textureFormat,
					imageAspectFlags,
					texture.GetAvailableMipRange(),
					ArrayRange{0, texture.GetTotalArrayCount()}
				);

#if RENDERER_OBJECT_DEBUG_NAMES
				imageMapping.SetDebugName(logicalDevice, texture.GetDebugName());
#endif

				if (localAttachmentIndex < m_renderPassInfo.m_attachmentDescriptions.GetSize())
				{
					Rendering::AttachmentDescription& attachmentDescription = m_renderPassInfo.m_attachmentDescriptions[localAttachmentIndex];
					attachmentDescription.m_format = textureFormat;

					// Indicate use of depth & stencil flags if the format is capable of it
					const EnumFlags<FormatFlags> textureFormatFlags = GetFormatInfo(textureFormat).m_flags;
					ImageSubresourceRange& imageSubresourceRange = m_renderPassInfo.m_attachmentSubresourceRanges[localAttachmentIndex];
					imageSubresourceRange.m_aspectMask |= ImageAspectFlags::Depth * textureFormatFlags.IsSet(FormatFlags::Depth);
					imageSubresourceRange.m_aspectMask |= ImageAspectFlags::Stencil * textureFormatFlags.IsSet(FormatFlags::Stencil);
				}

				ImageMapping& imageMappingRef = m_renderPassInfo.m_imageMappings[0][localAttachmentIndex] = Move(imageMapping);
				for (FrameIndex i = 0; i < imageCount; ++i)
				{
					m_renderPassInfo.m_textures[i][localAttachmentIndex] = texture;
					m_renderPassInfo.m_imageMappingViews[i][localAttachmentIndex] = imageMappingRef;
				}
				m_renderPassInfo.m_imageResolutions[localAttachmentIndex] = resolution;

				for (RenderSubpassInfo& __restrict subpassInfo : m_renderPassInfo.m_subpasses)
				{
					const OptionalIterator<AttachmentReference> pAttachmentReference = subpassInfo.m_attachmentReferences.FindIf(
						[localAttachmentIndex](const AttachmentReference& attachmentReference)
						{
							return attachmentReference.m_index == localAttachmentIndex;
						}
					);
					if (pAttachmentReference.IsValid())
					{
						const AttachmentIndex subpassAttachmentIndex = (AttachmentIndex
						)subpassInfo.m_attachmentReferences.GetIteratorIndex(pAttachmentReference);

						for (FrameIndex i = 0; i < imageCount; ++i)
						{
							subpassInfo.m_imageMappingViews[i][subpassAttachmentIndex] = m_renderPassInfo.m_imageMappingViews[i][localAttachmentIndex];
						}

						const bool requiresSeparateImageMapping = RENDERER_WEBGPU &&
						                                          pAttachmentReference->m_layout == ImageLayout::DepthStencilReadOnlyOptimal;
						if (requiresSeparateImageMapping)
						{
							for (FrameIndex i = 0; i < imageCount; ++i)
							{
								subpassInfo.m_imageMappingViews[i][subpassAttachmentIndex] = subpassInfo.m_imageMappings[i][subpassAttachmentIndex] =
									ImageMapping(
										logicalDevice,
										texture,
										m_renderPassInfo.m_attachmentMappingTypes[localAttachmentIndex],
										textureFormat,
										imageAspectFlags & (~ImageAspectFlags::Stencil),
										texture.GetAvailableMipRange(),
										ArrayRange{0, texture.GetTotalArrayCount()}
									);
							}
						}

						if (subpassAttachmentIndex < subpassInfo.m_textures[0].GetSize())
						{
							for (FrameIndex i = 0; i < imageCount; ++i)
							{
								subpassInfo.m_textures[i][subpassAttachmentIndex] = m_renderPassInfo.m_textures[i][localAttachmentIndex];
							}
						}
					}
				}
			}
			break;
			case StageType::Generic:
			{
				for (GenericSubpassInfo& genericSubpassInfo : m_genericPassInfo.m_subpasses)
				{
					for (const SubpassAttachmentReference& __restrict subpassAttachmentReference : genericSubpassInfo.m_attachmentReferences)
					{
						if (subpassAttachmentReference.m_attachmentIndex == localAttachmentIndex)
						{
							const AttachmentIndex attachmentIndex = genericSubpassInfo.m_attachmentReferences.GetIteratorIndex(&subpassAttachmentReference
							);
							if (genericSubpassInfo.m_imageMappings[0][attachmentIndex].IsValid())
							{
								// Attachment already loaded
								return false;
							}

							ImageMapping imageMapping(
								logicalDevice,
								texture,
								subpassAttachmentReference.m_mappingType,
								textureFormat,
								imageAspectFlags,
								subpassAttachmentReference.m_mipRange,
								subpassAttachmentReference.m_arrayRange
							);

#if RENDERER_OBJECT_DEBUG_NAMES
							imageMapping.SetDebugName(logicalDevice, texture.GetDebugName());
#endif

							ImageMapping& imageMappingRef = genericSubpassInfo.m_imageMappings[0][attachmentIndex] = Move(imageMapping);
							for (FrameIndex i = 0; i < imageCount; ++i)
							{
								genericSubpassInfo.m_textures[i][attachmentIndex] = texture;
								genericSubpassInfo.m_imageMappingViews[i][attachmentIndex] = imageMappingRef;
							}
							genericSubpassInfo.m_imageResolutions[attachmentIndex] = resolution;
						}
					}
				}
			}
			break;
			case StageType::Compute:
			{
				for (ComputeSubpassInfo& __restrict subpassInfo : m_computePassInfo.m_subpasses)
				{
					for (const SubpassAttachmentReference& __restrict subpassAttachmentReference : subpassInfo.m_attachmentReferences)
					{
						if (subpassAttachmentReference.m_attachmentIndex == localAttachmentIndex)
						{
							const AttachmentIndex attachmentIndex = subpassInfo.m_attachmentReferences.GetIteratorIndex(&subpassAttachmentReference);
							if (subpassInfo.m_imageMappings[0][attachmentIndex].IsValid())
							{
								// Attachment already loaded
								return false;
							}

							ImageMapping imageMapping(
								logicalDevice,
								texture,
								subpassAttachmentReference.m_mappingType,
								textureFormat,
								imageAspectFlags,
								subpassAttachmentReference.m_mipRange,
								subpassAttachmentReference.m_arrayRange
							);

#if RENDERER_OBJECT_DEBUG_NAMES
							imageMapping.SetDebugName(logicalDevice, texture.GetDebugName());
#endif

							ImageMapping& imageMappingRef = subpassInfo.m_imageMappings[0][attachmentIndex] = Move(imageMapping);
							for (FrameIndex i = 0; i < imageCount; ++i)
							{
								subpassInfo.m_imageMappingViews[i][attachmentIndex] = imageMappingRef;
							}

							for (FrameIndex i = 0; i < imageCount; ++i)
							{
								subpassInfo.m_textures[i][attachmentIndex] = texture;
							}
							subpassInfo.m_imageResolutions[attachmentIndex] = resolution;
						}
					}
				}
			}
			break;
		}

		OnPendingCompilationTaskCompleted(logicalDevice, framegraph);
		return true;
	}

	void PassInfo::OnPendingCompilationTaskCompleted(LogicalDevice& logicalDevice, Framegraph& framegraph)
	{
		const uint16 previousCompilationTaskCount = m_pendingCompilationTasks.FetchSubtract(1);
		Assert(previousCompilationTaskCount > 0);
		if (previousCompilationTaskCount == 1)
		{
			const FrameIndex imageCount = framegraph.m_renderOutput.GetImageCount();
			switch (m_type)
			{
				case StageType::RenderPass:
				case StageType::ExplicitRenderPass:
				{
					Assert(m_renderPassInfo.m_imageMappingViews.GetView().All(
						[](const ArrayView<const ImageMappingView, AttachmentIndex> imageMappings)
						{
							return imageMappings.All(
								[](const ImageMappingView imageMapping)
								{
									return imageMapping.IsValid();
								}
							);
						}
					));

					FixedCapacityInlineVector<Rendering::SubpassDescription, 8, SubpassIndex> subpassDescriptions(
						Memory::Reserve,
						m_renderPassInfo.m_subpasses.GetSize()
					);
					for (const RenderSubpassInfo& __restrict subpassInfo : m_renderPassInfo.m_subpasses)
					{
						subpassDescriptions.EmplaceBack(Rendering::SubpassDescription{
							subpassInfo.m_subpassInputAttachments,
							subpassInfo.m_colorAttachments,
							subpassInfo.m_resolveAttachments,
							subpassInfo.m_pDepthAttachment
						});
					}

					const Math::Rectangleui outputArea = m_renderPassInfo.m_drawableArea;
					const Math::Rectangleui renderArea = m_renderPassInfo.m_drawableArea;
					Threading::JobBatch jobBatch = m_renderPassInfo.m_pPass->Initialize(
						logicalDevice,
						m_renderPassInfo.m_attachmentDescriptions,
						subpassDescriptions.GetView(),
						m_renderPassInfo.m_subpassDependencies.GetView(),
						outputArea,
						renderArea
					);

					InlineVector<ArrayView<const ImageMappingView, uint16>, MaximumConcurrentFrameCount> passAttachmentMappings{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					const AttachmentIndex passAttachmentCount = m_renderPassInfo.m_attachmentDescriptions.GetSize();
					for (FrameIndex i = 0; i < imageCount; ++i)
					{
						passAttachmentMappings[i] = m_renderPassInfo.m_imageMappingViews[i].GetSubView(0u, passAttachmentCount);
					}
					m_renderPassInfo.m_pPass
						->OnPassAttachmentsLoaded(logicalDevice, passAttachmentMappings.GetView(), m_renderPassInfo.m_drawableArea.GetSize());

					InlineVector<ArrayView<const ImageMappingView, uint16>, MaximumConcurrentFrameCount> subpassColorAttachmentMappings{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					InlineVector<ImageMappingView, MaximumConcurrentFrameCount> subpassDepthAttachmentMappings{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					InlineVector<ArrayView<const ImageMappingView, uint16>, MaximumConcurrentFrameCount> subpassInputAttachmentMappings{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					InlineVector<ArrayView<const ImageMappingView, uint16>, MaximumConcurrentFrameCount> subpassExternalInputAttachmentMappings{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					InlineVector<ArrayView<const Optional<RenderTexture*>, uint16>, MaximumConcurrentFrameCount> subpassColorAttachments{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};

					constexpr uint8 maximumSubpassExternalAttachmentCount = 12;
					FlatVector<Math::Vector2ui, maximumSubpassExternalAttachmentCount> subpassExternalAttachmentResolutions;

					for (const RenderSubpassInfo& __restrict subpassInfo : m_renderPassInfo.m_subpasses)
					{
						const SubpassIndex subpassIndex = m_renderPassInfo.m_subpasses.GetIteratorIndex(&subpassInfo);
						for (FrameIndex i = 0; i < imageCount; ++i)
						{
							ArrayView<const ImageMappingView, AttachmentIndex> remainingImageMappingViews = subpassInfo.m_imageMappingViews[i];

							Assert(remainingImageMappingViews.GetSize() >= subpassInfo.m_colorAttachments.GetSize());
							subpassColorAttachmentMappings[i] = remainingImageMappingViews.GetSubView(0, subpassInfo.m_colorAttachments.GetSize());
							remainingImageMappingViews += subpassInfo.m_colorAttachments.GetSize();
							subpassColorAttachments[i] = subpassInfo.m_textures[i].GetView().GetSubView(0, subpassInfo.m_colorAttachments.GetSize());
							if (subpassInfo.m_pDepthAttachment.IsValid())
							{
								Assert(remainingImageMappingViews.GetSize() >= 1);
								subpassDepthAttachmentMappings[i] = remainingImageMappingViews[0];
								remainingImageMappingViews++;
							}
							Assert(remainingImageMappingViews.GetSize() >= subpassInfo.m_subpassInputAttachments.GetSize());
							subpassInputAttachmentMappings[i] = remainingImageMappingViews.GetSubView(0, subpassInfo.m_subpassInputAttachments.GetSize());
							remainingImageMappingViews += subpassInfo.m_subpassInputAttachments.GetSize();

							Assert(remainingImageMappingViews.GetSize() == subpassInfo.m_externalInputAttachments.GetSize());
							subpassExternalInputAttachmentMappings[i] =
								remainingImageMappingViews.GetSubView(0, subpassInfo.m_externalInputAttachments.GetSize());
						}

						for (const AttachmentReference externalInputAttachment : subpassInfo.m_externalInputAttachments)
						{
							subpassExternalAttachmentResolutions.EmplaceBack(
								m_renderPassInfo.m_imageResolutions[(AttachmentIndex)externalInputAttachment.m_index]
							);
						}
						const ArrayView<const Math::Vector2ui, uint16> subpassExternalInputAttachmentResolutions =
							subpassExternalAttachmentResolutions.GetView().GetSubViewUpTo(subpassInfo.m_externalInputAttachments.GetSize());

						for (Stage& subpassStage : subpassInfo.m_stages)
						{
							subpassStage.OnRenderPassAttachmentsLoaded(
								renderArea.GetSize(),
								subpassColorAttachmentMappings.GetView(),
								subpassDepthAttachmentMappings.GetView(),
								subpassInputAttachmentMappings.GetView(),
								subpassExternalInputAttachmentMappings.GetView(),
								subpassExternalInputAttachmentResolutions,
								subpassColorAttachments.GetView(),
								subpassIndex
							);
						}

						subpassExternalAttachmentResolutions.Clear();
					}

					if (jobBatch.IsValid())
					{
						// Ensure resizing can't trigger while compiling pass pipelines etc.
						jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
							[&framegraph](Threading::JobRunnerThread&)
							{
								framegraph.OnPendingCompilationTaskCompleted();
							},
							Threading::JobPriority::LoadGraphicsPipeline
						));
						Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
					}
					else
					{
						framegraph.OnPendingCompilationTaskCompleted();
					}
				}
				break;
				case StageType::Generic:
				{
					constexpr uint8 maximumSubpassAttachmentCount = 32;
					FlatVector<ImageMappingView, maximumSubpassAttachmentCount> subpassAttachmentMappingViews;
					InlineVector<Math::Vector2ui, maximumSubpassAttachmentCount / MaximumConcurrentFrameCount> subpassAttachmentResolutions;

					InlineVector<ArrayView<const ImageMappingView, uint16>, MaximumConcurrentFrameCount> subpassOutputAttachmentMappings{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					InlineVector<ArrayView<const ImageMappingView, uint16>, MaximumConcurrentFrameCount> subpassOutputInputAttachmentMappings{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					InlineVector<ArrayView<const ImageMappingView, uint16>, MaximumConcurrentFrameCount> subpassInputAttachmentMappings{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					InlineVector<ArrayView<const Optional<RenderTexture*>, uint16>, MaximumConcurrentFrameCount> subpassOutputAttachments{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};

					for (const GenericSubpassInfo& __restrict subpassInfo : m_genericPassInfo.m_subpasses)
					{
						Assert(subpassInfo.m_imageMappingViews.GetView().All(
							[](const ArrayView<const ImageMappingView, AttachmentIndex> imageMappings)
							{
								return imageMappings.All(
									[](const ImageMappingView imageMapping)
									{
										return imageMapping.IsValid();
									}
								);
							}
						));

						for (FrameIndex i = 0; i < imageCount; ++i)
						{
							AttachmentIndex subpassAttachmentIndex{0};
							for ([[maybe_unused]] const SubpassAttachmentReference subpassAttachmentReference : subpassInfo.m_outputAttachments)
							{
								subpassAttachmentMappingViews.EmplaceBack(subpassInfo.m_imageMappingViews[i][subpassAttachmentIndex++]);
							}
							subpassOutputAttachmentMappings[i] = subpassAttachmentMappingViews.GetView().GetSubViewFrom(
								subpassAttachmentMappingViews.GetSize() - subpassInfo.m_outputAttachments.GetSize()
							);
							for ([[maybe_unused]] const SubpassAttachmentReference subpassAttachmentReference : subpassInfo.m_inputOutputAttachments)
							{
								subpassAttachmentMappingViews.EmplaceBack(subpassInfo.m_imageMappingViews[i][subpassAttachmentIndex++]);
							}
							subpassOutputInputAttachmentMappings[i] = subpassAttachmentMappingViews.GetView().GetSubViewFrom(
								subpassAttachmentMappingViews.GetSize() - subpassInfo.m_inputOutputAttachments.GetSize()
							);
							for ([[maybe_unused]] const SubpassAttachmentReference subpassAttachmentReference : subpassInfo.m_inputAttachments)
							{
								subpassAttachmentMappingViews.EmplaceBack(subpassInfo.m_imageMappingViews[i][subpassAttachmentIndex++]);
							}
							subpassInputAttachmentMappings[i] = subpassAttachmentMappingViews.GetView().GetSubViewFrom(
								subpassAttachmentMappingViews.GetSize() - subpassInfo.m_inputAttachments.GetSize()
							);
						}

						AttachmentIndex subpassAttachmentIndex{0};
						for ([[maybe_unused]] const SubpassAttachmentReference subpassAttachmentReference : subpassInfo.m_outputAttachments)
						{
							subpassAttachmentResolutions.EmplaceBack(subpassInfo.m_imageResolutions[subpassAttachmentIndex++]);
						}
						const ArrayView<const Math::Vector2ui, uint16> subpassOutputAttachmentResolutions =
							subpassAttachmentResolutions.GetView().GetSubViewFrom(
								subpassAttachmentResolutions.GetSize() - subpassInfo.m_outputAttachments.GetSize()
							);

						for ([[maybe_unused]] const SubpassAttachmentReference subpassAttachmentReference : subpassInfo.m_inputOutputAttachments)
						{
							subpassAttachmentResolutions.EmplaceBack(subpassInfo.m_imageResolutions[subpassAttachmentIndex++]);
						}
						const ArrayView<const Math::Vector2ui, uint16> subpassOutputInputAttachmentResolutions =
							subpassAttachmentResolutions.GetView().GetSubViewFrom(
								subpassAttachmentResolutions.GetSize() - subpassInfo.m_inputOutputAttachments.GetSize()
							);

						for ([[maybe_unused]] const SubpassAttachmentReference subpassAttachmentReference : subpassInfo.m_inputAttachments)
						{
							subpassAttachmentResolutions.EmplaceBack(subpassInfo.m_imageResolutions[subpassAttachmentIndex++]);
						}
						const ArrayView<const Math::Vector2ui, uint16> subpassInputAttachmentResolutions =
							subpassAttachmentResolutions.GetView().GetSubViewFrom(
								subpassAttachmentResolutions.GetSize() - subpassInfo.m_inputAttachments.GetSize()
							);

						m_genericPassInfo.m_stage.OnGenericPassAttachmentsLoaded(
							subpassOutputAttachmentMappings.GetView(),
							subpassOutputAttachmentResolutions,
							subpassOutputInputAttachmentMappings.GetView(),
							subpassOutputInputAttachmentResolutions,
							subpassInputAttachmentMappings.GetView(),
							subpassInputAttachmentResolutions,
							subpassOutputAttachments.GetView()
						);
						subpassAttachmentMappingViews.Clear();
						subpassAttachmentResolutions.Clear();
					}

					framegraph.OnPendingCompilationTaskCompleted();
				}
				break;
				case StageType::Compute:
				{
					constexpr uint8 maximumSubpassAttachmentCount = 32;
					InlineVector<ImageMappingView, maximumSubpassAttachmentCount> subpassAttachmentMappingViews;
					InlineVector<Optional<RenderTexture*>, maximumSubpassAttachmentCount> subpassAttachmentViews;
					InlineVector<Math::Vector2ui, maximumSubpassAttachmentCount / MaximumConcurrentFrameCount> subpassAttachmentResolutions;

					InlineVector<ArrayView<const ImageMappingView, uint16>, MaximumConcurrentFrameCount> subpassOutputAttachmentMappings{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					InlineVector<ArrayView<const Optional<RenderTexture*>, uint16>, MaximumConcurrentFrameCount> subpassOutputAttachments{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					InlineVector<ArrayView<const ImageMappingView, uint16>, MaximumConcurrentFrameCount> subpassOutputInputAttachmentMappings{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					InlineVector<ArrayView<const Optional<RenderTexture*>, uint16>, MaximumConcurrentFrameCount> subpassOutputInputAttachments{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					InlineVector<ArrayView<const ImageMappingView, uint16>, MaximumConcurrentFrameCount> subpassInputAttachmentMappings{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};
					InlineVector<ArrayView<const Optional<RenderTexture*>, uint16>, MaximumConcurrentFrameCount> subpassInputAttachments{
						Memory::ConstructWithSize,
						Memory::DefaultConstruct,
						imageCount
					};

					for (const ComputeSubpassInfo& __restrict subpassInfo : m_computePassInfo.m_subpasses)
					{
						Assert(subpassInfo.m_imageMappingViews.GetView().All(
							[](const ArrayView<const ImageMappingView, AttachmentIndex> imageMappings)
							{
								return imageMappings.All(
									[](const ImageMappingView imageMapping)
									{
										return imageMapping.IsValid();
									}
								);
							}
						));

						const SubpassIndex subpassIndex = m_computePassInfo.m_subpasses.GetIteratorIndex(&subpassInfo);
						for (FrameIndex i = 0; i < imageCount; ++i)
						{
							AttachmentIndex subpassAttachmentIndex{0};
							for ([[maybe_unused]] const SubpassAttachmentReference subpassAttachmentReference : subpassInfo.m_outputAttachments)
							{
								subpassAttachmentMappingViews.EmplaceBack(subpassInfo.m_imageMappingViews[i][subpassAttachmentIndex]);
								subpassAttachmentViews.EmplaceBack(subpassInfo.m_textures[i][subpassAttachmentIndex]);
								subpassAttachmentIndex++;
							}
							subpassOutputAttachmentMappings[i] = subpassAttachmentMappingViews.GetView().GetSubViewFrom(
								subpassAttachmentMappingViews.GetSize() - subpassInfo.m_outputAttachments.GetSize()
							);
							subpassOutputAttachments[i] = subpassAttachmentViews.GetView().GetSubViewFrom(
								subpassAttachmentViews.GetSize() - subpassInfo.m_outputAttachments.GetSize()
							);

							for ([[maybe_unused]] const SubpassAttachmentReference subpassAttachmentReference : subpassInfo.m_inputOutputAttachments)
							{
								subpassAttachmentMappingViews.EmplaceBack(subpassInfo.m_imageMappingViews[i][subpassAttachmentIndex]);
								subpassAttachmentViews.EmplaceBack(subpassInfo.m_textures[i][subpassAttachmentIndex]);
								subpassAttachmentIndex++;
							}
							subpassOutputInputAttachmentMappings[i] = subpassAttachmentMappingViews.GetView().GetSubViewFrom(
								subpassAttachmentMappingViews.GetSize() - subpassInfo.m_inputOutputAttachments.GetSize()
							);
							subpassOutputInputAttachments[i] = subpassAttachmentViews.GetView().GetSubViewFrom(
								subpassAttachmentViews.GetSize() - subpassInfo.m_inputOutputAttachments.GetSize()
							);

							for ([[maybe_unused]] const SubpassAttachmentReference subpassAttachmentReference : subpassInfo.m_inputAttachments)
							{
								subpassAttachmentMappingViews.EmplaceBack(subpassInfo.m_imageMappingViews[i][subpassAttachmentIndex]);
								subpassAttachmentViews.EmplaceBack(subpassInfo.m_textures[i][subpassAttachmentIndex]);
								subpassAttachmentIndex++;
							}
							subpassInputAttachmentMappings[i] = subpassAttachmentMappingViews.GetView().GetSubViewFrom(
								subpassAttachmentMappingViews.GetSize() - subpassInfo.m_inputAttachments.GetSize()
							);
							subpassInputAttachments[i] = subpassAttachmentViews.GetView().GetSubViewFrom(
								subpassAttachmentViews.GetSize() - subpassInfo.m_inputAttachments.GetSize()
							);
						}

						AttachmentIndex subpassAttachmentIndex{0};
						for ([[maybe_unused]] const SubpassAttachmentReference subpassAttachmentReference : subpassInfo.m_outputAttachments)
						{
							subpassAttachmentResolutions.EmplaceBack(subpassInfo.m_imageResolutions[subpassAttachmentIndex++]);
						}
						const ArrayView<const Math::Vector2ui, uint16> subpassOutputAttachmentResolutions =
							subpassAttachmentResolutions.GetView().GetSubViewFrom(
								subpassAttachmentResolutions.GetSize() - subpassInfo.m_outputAttachments.GetSize()
							);

						for ([[maybe_unused]] const SubpassAttachmentReference subpassAttachmentReference : subpassInfo.m_inputOutputAttachments)
						{
							subpassAttachmentResolutions.EmplaceBack(subpassInfo.m_imageResolutions[subpassAttachmentIndex++]);
						}
						const ArrayView<const Math::Vector2ui, uint16> subpassOutputInputAttachmentResolutions =
							subpassAttachmentResolutions.GetView().GetSubViewFrom(
								subpassAttachmentResolutions.GetSize() - subpassInfo.m_inputOutputAttachments.GetSize()
							);

						for ([[maybe_unused]] const SubpassAttachmentReference subpassAttachmentReference : subpassInfo.m_inputAttachments)
						{
							subpassAttachmentResolutions.EmplaceBack(subpassInfo.m_imageResolutions[subpassAttachmentIndex++]);
						}
						const ArrayView<const Math::Vector2ui, uint16> subpassInputAttachmentResolutions =
							subpassAttachmentResolutions.GetView().GetSubViewFrom(
								subpassAttachmentResolutions.GetSize() - subpassInfo.m_inputAttachments.GetSize()
							);

						m_computePassInfo.m_stage.OnComputePassAttachmentsLoaded(
							subpassOutputAttachmentMappings.GetView(),
							subpassOutputAttachmentResolutions,
							subpassOutputAttachments.GetView(),
							subpassOutputInputAttachmentMappings.GetView(),
							subpassOutputInputAttachmentResolutions,
							subpassOutputInputAttachments.GetView(),
							subpassInputAttachmentMappings.GetView(),
							subpassInputAttachmentResolutions,
							subpassInputAttachments.GetView(),
							subpassIndex
						);

						subpassAttachmentMappingViews.Clear();
						subpassAttachmentViews.Clear();
						subpassAttachmentResolutions.Clear();
					}

					framegraph.OnPendingCompilationTaskCompleted();
				}
				break;
			}
		}
		else
		{
			framegraph.OnPendingCompilationTaskCompleted();
		}
	}

	PassInfo::PassInfo(const StageType type, const Optional<Stage*> pStage, const Optional<SceneViewDrawer*> pSceneViewDrawer)
		: m_type(type)
		, m_pSceneViewDrawer(pSceneViewDrawer)
	{
		switch (type)
		{
			case StageType::RenderPass:
			case StageType::ExplicitRenderPass:
				new (&m_renderPassInfo) RenderPassInfo();
				break;
			case StageType::Generic:
				new (&m_genericPassInfo) GenericPassInfo{*pStage};
				break;
			case StageType::Compute:
				new (&m_computePassInfo) ComputePassInfo{*pStage};
				break;
		}
	}

	PassInfo::~PassInfo()
	{
		switch (m_type)
		{
			case StageType::RenderPass:
			case StageType::ExplicitRenderPass:
				m_renderPassInfo.~RenderPassInfo();
				break;
			case StageType::Generic:
				m_genericPassInfo.~GenericPassInfo();
				break;
			case StageType::Compute:
				m_computePassInfo.~ComputePassInfo();
				break;
		}
	}

	Optional<Stage*> PassInfo::GetStage() const
	{
		switch (m_type)
		{
			case StageType::RenderPass:
			case StageType::ExplicitRenderPass:
				return m_renderPassInfo.m_pPass.Get();
			case StageType::Generic:
				return m_genericPassInfo.m_pPass.Get();
			case StageType::Compute:
				return m_computePassInfo.m_pPass.Get();
		}
		ExpectUnreachable();
	}

	bool PassInfo::ContainsStage(const Stage& stage) const
	{
		switch (m_type)
		{
			case StageType::RenderPass:
			case StageType::ExplicitRenderPass:
				return m_renderPassInfo.m_subpasses.ContainsIf(
					[&stage](const RenderSubpassInfo& subpassInfo)
					{
						return subpassInfo.m_stages.Contains(stage);
					}
				);
			case StageType::Generic:
				return &m_genericPassInfo.m_stage == &stage;
			case StageType::Compute:
				return m_computePassInfo.m_subpasses.ContainsIf(
					[&stage](const ComputeSubpassInfo& subpassInfo)
					{
						return subpassInfo.m_stages.Contains(stage);
					}
				);
		}
		ExpectUnreachable();
	}

	ComputePassInfo::ComputePassInfo(Stage& stage)
		: m_stage(stage)
	{
	}
}
