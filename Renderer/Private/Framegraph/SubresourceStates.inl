#pragma once

#include "Framegraph/SubresourceStates.h"

namespace ngine::Rendering
{
	template<typename Callback>
	inline void SubresourceStatesBase::VisitUniformSubresourceRanges(
		const ImageSubresourceRange subresourceRange,
		Callback&& callback,
		const ImageSubresourceRange fullSubresourceRanges,
		const uint8 bucketIndex
	) const
	{
		Assert(fullSubresourceRanges.Contains(subresourceRange));
		using SubresourceIndex = uint8;
		constexpr SubresourceIndex maximumSubresourceCount = 255;
		Bitset<maximumSubresourceCount> remainingSubresourceCount;
		for (const ImageAspectFlags aspectFlag : subresourceRange.m_aspectMask)
		{
			for (const ArrayRange::UnitType arrayLayerIndex : subresourceRange.m_arrayRange)
			{
				for (const MipRange::UnitType mipLevelIndex : subresourceRange.m_mipRange)
				{
					const uint32 subresourceIndex =
						GetSubresourceIndex(aspectFlag, mipLevelIndex, arrayLayerIndex, fullSubresourceRanges, bucketIndex);
					Assert(subresourceIndex < maximumSubresourceCount);
					remainingSubresourceCount.Set((SubresourceIndex)subresourceIndex);
				}
			}
		}
		Assert(remainingSubresourceCount.AreAnySet());

		do
		{
			const SubresourceIndex firstSubresourceIndex = remainingSubresourceCount.GetFirstSetIndex();
			const ImageSubresourceRange firstSubresourceElementRange = GetSubresourceElementRange(firstSubresourceIndex, fullSubresourceRanges);
			const MipRange::UnitType firstMipIndex = firstSubresourceElementRange.m_mipRange.GetIndex();
			const ArrayRange::UnitType firstArrayIndex = firstSubresourceElementRange.m_arrayRange.GetIndex();
			SubresourceState firstSubresourceState = GetSubresourceState(
				firstSubresourceElementRange.m_aspectMask.GetFlags(),
				firstMipIndex,
				firstArrayIndex,
				fullSubresourceRanges,
				bucketIndex
			);
			for (const SubresourceIndex subresourceIndex : remainingSubresourceCount.GetSetBitsIterator())
			{
				const ImageSubresourceRange subresourceElementRange = GetSubresourceElementRange(subresourceIndex, fullSubresourceRanges);
				const MipRange::UnitType mipIndex = subresourceElementRange.m_mipRange.GetIndex();
				const ArrayRange::UnitType arrayIndex = subresourceElementRange.m_arrayRange.GetIndex();

				const SubresourceState subresourceState =
					GetSubresourceState(subresourceElementRange.m_aspectMask.GetFlags(), mipIndex, arrayIndex, fullSubresourceRanges, bucketIndex);
				const ImageSubresourceRange transitionedSubresourceRange{
					subresourceRange.m_aspectMask,
					MipRange(firstMipIndex, mipIndex - firstMipIndex + 1),
					ArrayRange(firstArrayIndex, arrayIndex - firstArrayIndex + 1)
				};
				if (firstSubresourceState != subresourceState)
				{
					// Transition all resources until this one
					remainingSubresourceCount.ClearAll(
						Math::Range<SubresourceIndex>::Make(firstSubresourceIndex, subresourceIndex - firstSubresourceIndex)
					);
					callback(firstSubresourceState, transitionedSubresourceRange);
					if (remainingSubresourceCount.AreAnySet())
					{
						break;
					}
					else
					{
						return;
					}
				}

				// Ensure that we select the most recent stage
				if (subresourceState.m_attachmentReference.passIndex > firstSubresourceState.m_attachmentReference.passIndex)
				{
					firstSubresourceState.m_attachmentReference = subresourceState.m_attachmentReference;
				}
				// Inherit all stage and access flags for the barrier
				firstSubresourceState.m_pipelineStageFlags |= subresourceState.m_pipelineStageFlags;
				firstSubresourceState.m_accessFlags |= subresourceState.m_accessFlags;

				const SubresourceIndex sequentialSubresourceCount = (subresourceIndex - firstSubresourceIndex) + 1;
				if (*remainingSubresourceCount.GetLastSetIndex() == firstSubresourceIndex + sequentialSubresourceCount - 1)
				{
					// Transition all resources until this one
					remainingSubresourceCount.ClearAll(Math::Range<SubresourceIndex>::Make(firstSubresourceIndex, sequentialSubresourceCount));
					// TODO: First find the first complete mip range
					// If it is complete, then check the next complete array range.
					callback(firstSubresourceState, transitionedSubresourceRange);
					return;
				}
			}
		} while (remainingSubresourceCount.AreAnySet());
		ExpectUnreachable();
	}

	template<typename Callback>
	inline void SubresourceStates::VisitUniformSubresourceRanges(
		const ImageSubresourceRange subresourceRange, Callback&& callback, const Bucket bucket
	) const
	{
		SubresourceStatesBase::VisitUniformSubresourceRanges(subresourceRange, Forward<Callback>(callback), m_subresourceRange, (uint8)bucket);
	}
}
