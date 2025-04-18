#pragma once

#include "AttachmentLoadType.h"
#include "AttachmentStoreType.h"

#include <Renderer/ImageLayout.h>
#include <Renderer/SampleCount.h>
#include <Renderer/Format.h>

namespace ngine::Rendering
{
	struct AttachmentDescription
	{
		constexpr AttachmentDescription() = default;
		constexpr AttachmentDescription(
			const Format format,
			const SampleCount sampleCount,
			const AttachmentLoadType loadType,
			const AttachmentStoreType storeType,
			const AttachmentLoadType stencilLoadType,
			const AttachmentStoreType stencilStoreType,
			const ImageLayout initialLayout,
			const ImageLayout finalLayout
		)
			: m_format(format)
			, m_sampleCount(sampleCount)
			, m_loadType(loadType)
			, m_storeType(storeType)
			, m_stencilLoadType(stencilLoadType)
			, m_stencilStoreType(stencilStoreType)
			, m_initialLayout(initialLayout)
			, m_finalLayout(finalLayout)
		{
		}

#if RENDERER_VULKAN
		uint32 m_flags = 0u;
#endif
		Format m_format{Format::Invalid};
		SampleCount m_sampleCount{SampleCount::One};
		AttachmentLoadType m_loadType{AttachmentLoadType::Undefined};
		AttachmentStoreType m_storeType{AttachmentStoreType::Undefined};
		AttachmentLoadType m_stencilLoadType{AttachmentLoadType::Undefined};
		AttachmentStoreType m_stencilStoreType{AttachmentStoreType::Undefined};
		ImageLayout m_initialLayout{ImageLayout::Undefined};
		ImageLayout m_finalLayout{ImageLayout::Undefined};
	};
}
