#pragma once

#include "AttachmentReference.h"

#include <Common/Memory/Containers/ArrayView.h>
#include <Common/Memory/Optional.h>

namespace ngine::Rendering
{
	struct AttachmentReference;

	using InputAttachmentReferences = ArrayView<const AttachmentReference, uint16>;
	using ColorAttachmentReferences = ArrayView<const AttachmentReference, uint16>;
	using ResolveAttachmentReferences = ArrayView<const AttachmentReference, uint16>;
	using DepthAttachmentReference = Optional<const AttachmentReference*>;

	struct SubpassDescription
	{
		SubpassDescription() = default;

		constexpr SubpassDescription(
			const InputAttachmentReferences inputAttachments,
			const ColorAttachmentReferences colorAttachments,
			const ResolveAttachmentReferences resolveAttachments,
			const DepthAttachmentReference depthStencilAttachment
		)
			: m_inputAttachmentCount(inputAttachments.GetSize())
			, m_pInputAttachments(inputAttachments.GetData())
			, m_colorAttachmentCount(colorAttachments.GetSize())
			, m_pColorAttachments(colorAttachments.GetData())
			, m_pResolveAttachments(resolveAttachments.GetData())
			, m_depthStencilAttachment(depthStencilAttachment)
			, m_preserveAttachmentCount(0)
			, m_pPreserveAttachments(nullptr)
		{
			Assert(resolveAttachments.IsEmpty() || resolveAttachments.GetSize() == m_colorAttachmentCount);
		}

#if RENDERER_VULKAN
		uint32 m_flags = 0;
		uint32 m_bindPoint = 0;
#endif

		[[nodiscard]] ArrayView<const AttachmentReference, uint8> GetInputAttachments() const
		{
			return {m_pInputAttachments, (uint8)m_inputAttachmentCount};
		}

		[[nodiscard]] ArrayView<const AttachmentReference, uint8> GetColorAttachments() const
		{
			return {m_pColorAttachments, (uint8)m_colorAttachmentCount};
		}

		[[nodiscard]] ArrayView<const AttachmentReference, uint8> GetResolveAttachments() const
		{
			return {m_pResolveAttachments, m_pResolveAttachments != nullptr ? (uint8)m_colorAttachmentCount : (uint8)0u};
		}

		[[nodiscard]] Optional<const AttachmentReference*> GetDepthStencilAttachment() const
		{
			return m_depthStencilAttachment;
		}
	protected:
		uint32 m_inputAttachmentCount = 0;
		const AttachmentReference* m_pInputAttachments = nullptr;
		uint32 m_colorAttachmentCount = 0;
		const AttachmentReference* m_pColorAttachments = nullptr;
		const AttachmentReference* m_pResolveAttachments = nullptr;
		DepthAttachmentReference m_depthStencilAttachment;
		uint32 m_preserveAttachmentCount = 0;
		const uint32* m_pPreserveAttachments = nullptr;
	};
}
