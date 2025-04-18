#pragma once

#include "SubresourceStates.h"

namespace ngine::Rendering
{
	struct AttachmentInfo : public SubresourceStates
	{
		[[nodiscard]] bool IsFinalLayoutLocked() const
		{
			return m_isFinalLayoutLocked;
		}

		void LockFinalLayout()
		{
			m_isFinalLayoutLocked = true;
		}

		void OnUsed(const PassAttachmentReference attachmentReference);

		void TransitionLayout(
			const ArrayView<PassInfo, PassIndex> passes,
			const SubresourceState previousSubresourceState,
			const SubresourceState newSubresourceState,
			const ImageSubresourceRange subresourceRange
		);

		// Requests the layout that this attachment is in when a render pass starts
		// Render passes automatically transition to subpass layouts when subpasses start so in most cases we can simply return what the
		// previous layout was, and subpasses will do the rest. Clearing happens outside of subpasses, so we have to ensure that we return a
		// writable image layout.
		[[nodiscard]] SubresourceState RequestInitialRenderPassLayout(
			const ArrayView<PassInfo, PassIndex> passes,
			const PassAttachmentReference attachmentReference,
			const EnumFlags<PipelineStageFlags> newPipelineStageFlags,
			const EnumFlags<AccessFlags> newAccessFlags,
			const ImageLayout preferredInitialImageLayout,
			const QueueFamilyIndex newQueueFamilyIndex,
			const ImageSubresourceRange subresourceRange
		);

		void RequestOrTransitionLayout(
			const ArrayView<PassInfo, PassIndex> passes,
			const PassAttachmentReference attachmentReference,
			const EnumFlags<PipelineStageFlags> newPipelineStageFlags,
			const EnumFlags<AccessFlags> newAccessFlags,
			const ImageLayout requestedImageLayout,
			const QueueFamilyIndex newQueueFamilyIndex,
			const ImageSubresourceRange subresourceRange
		);
	protected:
		PassAttachmentReference m_previousPassAttachmentReference;
		bool m_isFinalLayoutLocked{false};
	};
}
