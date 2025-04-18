#include "Framegraph/SubresourceStates.h"
#include "Framegraph/SubresourceStates.inl"

namespace ngine::Rendering
{
	void SubresourceStatesBase::Initialize(const ImageSubresourceRange fullSubresourceRange, const uint8 bucketCount)
	{
		const uint32 totalSubresourceCount = GetTotalSubresourceCount(fullSubresourceRange);
		Assert(m_subresourceStates.GetSize() <= totalSubresourceCount * bucketCount);
		m_subresourceStates.Resize(totalSubresourceCount * bucketCount);
	}

	PURE_LOCALS_AND_POINTERS const SubresourceState& SubresourceStatesBase::GetSubresourceState(
		const ImageAspectFlags imageAspect,
		const MipRange::UnitType mipLevel,
		const ArrayRange::UnitType arrayLevel,
		ImageSubresourceRange fullSubresourceRange,
		const uint8 bucketIndex
	) const
	{
		const uint32 subresourceIndex = GetSubresourceIndex(imageAspect, mipLevel, arrayLevel, fullSubresourceRange, bucketIndex);
		const SubresourceState& state = m_subresourceStates[subresourceIndex];
		return state;
	}

	PURE_LOCALS_AND_POINTERS Optional<SubresourceState> SubresourceStatesBase::GetUniformSubresourceState(
		const ImageSubresourceRange subresourceRange, const ImageSubresourceRange fullSubresourceRange, const uint8 bucketIndex
	) const
	{
		const SubresourceState& firstSubresourceState = GetSubresourceState(
			*subresourceRange.m_aspectMask.GetFirstSetFlag(),
			subresourceRange.m_mipRange.GetIndex(),
			subresourceRange.m_arrayRange.GetIndex(),
			fullSubresourceRange,
			bucketIndex
		);
		for (const ImageAspectFlags aspectFlag : subresourceRange.m_aspectMask)
		{
			for (const ArrayRange::UnitType arrayLayerIndex : subresourceRange.m_arrayRange)
			{
				for (const MipRange::UnitType mipLevelIndex : subresourceRange.m_mipRange)
				{
					const SubresourceState& subresourceState =
						GetSubresourceState(aspectFlag, mipLevelIndex, arrayLayerIndex, fullSubresourceRange, bucketIndex);
					if (subresourceState != firstSubresourceState)
					{
						return Invalid;
					}
				}
			}
		}
		return firstSubresourceState;
	}

	void SubresourceStatesBase::SetSubresourceState(
		const ImageAspectFlags imageAspect,
		const MipRange::UnitType mipLevel,
		const ArrayRange::UnitType arrayLevel,
		const ImageSubresourceRange fullSubresourceRange,
		const SubresourceState newSubresourceState,
		const uint8 bucketIndex
	)
	{
		const uint32 subresourceIndex = GetSubresourceIndex(imageAspect, mipLevel, arrayLevel, fullSubresourceRange, bucketIndex);
		SubresourceState& state = m_subresourceStates[subresourceIndex];
		static_cast<SubresourceState&>(state) = newSubresourceState;
	}

	void SubresourceStatesBase::SetSubresourceState(
		const ImageSubresourceRange subresourceRange,
		const SubresourceState newSubresourceState,
		const ImageSubresourceRange fullSubresourceRange,
		const uint8 bucketIndex
	)
	{
		for (const ImageAspectFlags aspectFlag : subresourceRange.m_aspectMask)
		{
			for (const ArrayRange::UnitType arrayLayerIndex : subresourceRange.m_arrayRange)
			{
				for (const MipRange::UnitType mipLevelIndex : subresourceRange.m_mipRange)
				{
					SetSubresourceState(aspectFlag, mipLevelIndex, arrayLayerIndex, fullSubresourceRange, newSubresourceState, bucketIndex);
				}
			}
		}
	}

	void SubresourceStates::RegisterUsedSubresourceRange(const ImageSubresourceRange subresourceRange)
	{
		const ImageSubresourceRange previouSubresourceRange = m_subresourceRange;
		m_subresourceRange.m_aspectMask = previouSubresourceRange.m_aspectMask | subresourceRange.m_aspectMask;

		if (previouSubresourceRange.m_mipRange.GetCount() == 0)
		{
			m_subresourceRange.m_mipRange = subresourceRange.m_mipRange;
		}
		else
		{
			m_subresourceRange.m_mipRange = MipRange(
				Math::Min(previouSubresourceRange.m_mipRange.GetIndex(), subresourceRange.m_mipRange.GetIndex()),
				Math::Max(previouSubresourceRange.m_mipRange.GetEnd(), subresourceRange.m_mipRange.GetEnd())
			);
		}
		if (previouSubresourceRange.m_arrayRange.GetCount() == 0)
		{
			m_subresourceRange.m_arrayRange = subresourceRange.m_arrayRange;
		}
		else
		{
			m_subresourceRange.m_arrayRange = ArrayRange(
				Math::Min(previouSubresourceRange.m_arrayRange.GetIndex(), subresourceRange.m_arrayRange.GetIndex()),
				Math::Max(previouSubresourceRange.m_arrayRange.GetEnd(), subresourceRange.m_arrayRange.GetEnd())
			);
		}

		SubresourceStatesBase::Initialize(m_subresourceRange, (uint8)Bucket::Count);
	}
}
