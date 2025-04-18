#pragma once

#include "BarrierCommandEncoderView.h"

#include <Renderer/PipelineStageFlags.h>
#include <Renderer/Wrappers/ImageMemoryBarrier.h>

#include <Common/EnumFlags.h>
#include <Common/Memory/Containers/InlineVector.h>

namespace ngine::Rendering
{
	struct RenderTexture;
	struct SubresourceStatesBase;

	//! Command encoder that can record barriers and transition image layouts
	struct BarrierCommandEncoder : public BarrierCommandEncoderView
	{
		BarrierCommandEncoder() = default;
		BarrierCommandEncoder(const BarrierCommandEncoder&) = delete;
		BarrierCommandEncoder& operator=(const BarrierCommandEncoder&) = delete;
		BarrierCommandEncoder([[maybe_unused]] BarrierCommandEncoder&& other) noexcept
		{
			m_commandEncoder = other.m_commandEncoder;
			other.m_commandEncoder = {};
		}
		BarrierCommandEncoder& operator=(BarrierCommandEncoder&& other) noexcept;
		~BarrierCommandEncoder();

		void TransitionImageLayout(
			const EnumFlags<PipelineStageFlags> targetPipelineStageFlags,
			const EnumFlags<AccessFlags> targetAccessMask,
			const ImageLayout newLayout,
			const ImageView image,
			SubresourceStatesBase& imageSubresourceStates,
			const ImageSubresourceRange subresourceRange,
			const ImageSubresourceRange fullSubresourceRange
		);
		void TransitionImageLayout(
			const EnumFlags<PipelineStageFlags> targetPipelineStageFlags,
			const EnumFlags<AccessFlags> targetAccessMask,
			const ImageLayout newLayout,
			RenderTexture& texture,
			const ImageSubresourceRange subresourceRange
		);
		void TransitionImageLayout(
			const EnumFlags<PipelineStageFlags> targetPipelineStageFlags,
			const EnumFlags<AccessFlags> targetAccessMask,
			const ImageLayout newLayout,
			const QueueFamilyIndex targetQueueFamilyIndex,
			const ImageView image,
			SubresourceStatesBase& imageSubresourceStates,
			const ImageSubresourceRange subresourceRange,
			const ImageSubresourceRange fullSubresourceRange
		);
		void TransitionImageLayout(
			const EnumFlags<PipelineStageFlags> targetPipelineStageFlags,
			const EnumFlags<AccessFlags> targetAccessMask,
			const ImageLayout newLayout,
			const QueueFamilyIndex targetQueueFamilyIndex,
			RenderTexture& texture,
			const ImageSubresourceRange subresourceRange
		);

		void End();
	protected:
		BarrierCommandEncoder(const BarrierCommandEncoderView barrierCommandEncoder)
			: BarrierCommandEncoderView(barrierCommandEncoder)
		{
		}
		friend CommandEncoder;
		friend CommandEncoderView;

		using ImageMemoryBarriers = InlineVector<ImageMemoryBarrier, 3>;
		struct Barrier
		{
			EnumFlags<PipelineStageFlags> m_sourceStages;
			EnumFlags<PipelineStageFlags> m_targetStages;
			ImageMemoryBarriers m_imageMemoryBarriers;
		};

		void Emplace(Barrier&& barrier);
	protected:
		InlineVector<Barrier, 2> m_barriers;
	};
}
