#pragma once

#include "DepthInfo.h"

namespace ngine::Widgets
{
	// Helper to ensure correct order based on CSS rules
	// Using bitfields to prioritize from left to right in order of members
	union DepthValue
	{
		inline static constexpr uint8 ComputedDepthIndexBitCount = DepthInfo::ComputedDepthIndexBitCount;
		inline static constexpr uint8 NegativeZIndexBitCount = 8;
		inline static constexpr uint8 MaximumNegativeZIndex = (1u << NegativeZIndexBitCount) - 1;
		inline static constexpr uint8 PositiveZIndexBitCount = 8;
		inline static constexpr uint8 MaximumPositiveZIndex = (1u << PositiveZIndexBitCount) - 1;
		inline static constexpr uint8 TotalBooleanBitCount = 2;
		inline static constexpr uint8 TotalRequiredBitCount = DepthInfo::ComputedDepthIndexBitCount + TotalBooleanBitCount +
		                                                      PositiveZIndexBitCount + NegativeZIndexBitCount;

		using UnderlyingType = Memory::UnsignedIntegerType<TotalRequiredBitCount>;
		inline static constexpr uint8 TotalBitCount = sizeof(UnderlyingType) * 8;

		[[nodiscard]] PURE_STATICS float GetDepthRatio(const float minimumDepth, const float maximumDepth) const;

		[[nodiscard]] bool operator==(const DepthValue other) const
		{
			return m_value == other.m_value;
		}
		[[nodiscard]] bool operator!=(const DepthValue other) const
		{
			return m_value != other.m_value;
		}

		// Note: reverse layout of order
		struct
		{
			// Hierarchy depth index
			// Incremented once for each parent, and once for each sibling (and their children) that precedes us
			UnderlyingType m_computedDepthIndex : DepthInfo::ComputedDepthIndexBitCount;

			// Stacking contexts with z-index: auto and z-index: 0
			UnderlyingType m_zeroZIndexValue : 1;
			// Stacking contexts with positive (non-zero) z-index
			UnderlyingType m_positiveZIndexValue : PositiveZIndexBitCount;

			// Non-positioned floating descendants
			// uint32 m_isNonPositionedFloating : 1;

			// Non-positioned descendants, i.e. PositionType::Static
			UnderlyingType m_isNonPositioned : 1;

			// Stacking contexts with negative z-indices
			UnderlyingType m_negativeZIndexValue : NegativeZIndexBitCount;
		};
		UnderlyingType m_value;
	};
	static_assert(sizeof(DepthValue) == sizeof(DepthValue::UnderlyingType));
}
