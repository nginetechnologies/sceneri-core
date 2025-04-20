#pragma once

#include <Widgets/Style/Modifier.h>
#include <Widgets/Style/ForwardDeclarations/ModifierMatch.h>

#include <Common/Math/Abs.h>
#include <Common/Math/SignNonZero.h>
#include <Common/Memory/GetIntegerType.h>
#include <Common/EnumFlags.h>

namespace ngine::Widgets
{
	namespace Style
	{
		struct CombinedEntry;
	}

	struct DepthInfo
	{
		inline static constexpr uint8 ScopeIndexBitCount = 3;
		using ScopeIndexType = Memory::UnsignedIntegerType<ScopeIndexBitCount>;
		inline static constexpr ScopeIndexType MaximumScopeIndex = (1u << ScopeIndexBitCount) - 1;

		inline static constexpr uint8 ComputedDepthIndexBitCount = 16;
		using ComputedIndexType = Memory::UnsignedIntegerType<ComputedDepthIndexBitCount>;
		inline static constexpr ComputedIndexType MaximumComputedDepthIndex = (1ull << ComputedDepthIndexBitCount) - 1;

		inline static constexpr uint8 HierarchyDepthIndexBitCount = 8;
		using HierarchyIndexType = Memory::UnsignedIntegerType<HierarchyDepthIndexBitCount>;
		inline static constexpr HierarchyIndexType MaximumHierarchyDepthIndex = (1ull << HierarchyDepthIndexBitCount) - 1;

		// Note: We limit the custom depth (aka z-index).
		// Any value outside of the range will be clamped.
		inline static constexpr uint8 CustomDepthIndexBitCount = 12;
		using CustomDepthStoredType = Memory::UnsignedIntegerType<CustomDepthIndexBitCount>;
		using CustomDepthSignedType = Memory::SignedIntegerType<CustomDepthIndexBitCount>;

		inline static constexpr CustomDepthSignedType MaximumCustomDepthIndex = (1u << CustomDepthIndexBitCount) - 1;
		inline static constexpr CustomDepthSignedType MinimumCustomDepthIndex = -(CustomDepthSignedType)MaximumCustomDepthIndex;

		inline static constexpr uint8 TotalBooleanBitCount = 3;
		inline static constexpr uint8 TotalDepthBitCount = ComputedDepthIndexBitCount + HierarchyDepthIndexBitCount + CustomDepthIndexBitCount +
		                                                   TotalBooleanBitCount;

		inline static constexpr uint8 TotalRequiredBitCount = ScopeIndexBitCount + TotalDepthBitCount;
		using UnderlyingType = Memory::UnsignedIntegerType<TotalRequiredBitCount>;
		inline static constexpr uint8 TotalBitCount = sizeof(UnderlyingType) * 8;

		union CustomDepthInfo
		{
			CustomDepthInfo(const CustomDepthStoredType index, const CustomDepthStoredType sign)
				: m_index(index)
				, m_sign(sign)
			{
			}

			struct
			{
				CustomDepthStoredType m_index : CustomDepthIndexBitCount;
				CustomDepthStoredType m_sign : 1;
			};
			CustomDepthStoredType m_value;
		};

		constexpr DepthInfo()
			: m_scopeIndex(0)
			, m_computedDepthIndex(0)
			, m_customDepthIndex(0)
			, m_customDepthSign(1)
			, m_createsNewLocalScope(0)
			, m_isNonPositioned(1)
			, m_hierarchyDepthIndex(0)
			, m_remainder(0)
		{
		}
		DepthInfo(
			const DepthInfo parentDepthInfo,
			const DepthInfo precedingWidgetDepthInfo,
			const Style::CombinedEntry& style,
			const EnumFlags<Style::Modifier> activeModifiers
		);
		DepthInfo(
			const DepthInfo parentDepthInfo,
			const DepthInfo precedingWidgetDepthInfo,
			const Style::CombinedEntry& style,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers
		);
		DepthInfo(
			const DepthInfo parentDepthInfo,
			const DepthInfo precedingWidgetDepthInfo,
			const CustomDepthInfo customDepth,
			const bool createsNewLocalScope,
			const bool isNonPositioned
		)
			: m_scopeIndex(parentDepthInfo.m_scopeIndex + parentDepthInfo.m_createsNewLocalScope)
			, m_computedDepthIndex(CalculateComputedDepthIndex(precedingWidgetDepthInfo))
			, m_customDepthIndex(customDepth.m_index)
			, m_customDepthSign(customDepth.m_sign)
			, m_createsNewLocalScope(createsNewLocalScope ? 1 : 0)
			, m_isNonPositioned(isNonPositioned ? 1 : 0)
			, m_hierarchyDepthIndex(parentDepthInfo.m_hierarchyDepthIndex + 1)
			, m_remainder(0)
		{
		}
		DepthInfo(
			const DepthInfo parentDepthInfo,
			const DepthInfo precedingWidgetDepthInfo,
			const CustomDepthSignedType customDepth,
			const bool createsNewLocalScope,
			const bool isNonPositioned
		)
			: DepthInfo(
					parentDepthInfo,
					precedingWidgetDepthInfo,
					CalculateCustomDepth(parentDepthInfo.GetCustomDepth(), customDepth),
					createsNewLocalScope,
					isNonPositioned
				)
		{
		}

		enum class ChangeFlags : uint8
		{
			ComputedDepth = 1 << 0,
			ScopeIndex = 1 << 1,
			CustomDepth = 1 << 2,
			CreatesNewLocalScope = 1 << 3,
			IsNonPositioned = 1 << 4,

			ShouldRecalculateSubsequentDepthInfo = ComputedDepth
		};

		[[nodiscard]] EnumFlags<ChangeFlags>
		OnParentOrPrecedingWidgetChanged(const DepthInfo parentDepthInfo, const DepthInfo precedingWidgetDepthInfo);

		[[nodiscard]] EnumFlags<ChangeFlags>
		OnStyleChanged(const Style::CombinedEntry& style, const Style::CombinedMatchingEntryModifiersView matchingModifiers);

		[[nodiscard]] float GetDepthRatio(const float minimumDepth, const float maximumDepth) const;

		[[nodiscard]] ScopeIndexType GetScopeIndex() const
		{
			return m_scopeIndex;
		}
		[[nodiscard]] ComputedIndexType GetComputedDepthIndex() const
		{
			return m_computedDepthIndex;
		}
		[[nodiscard]] HierarchyIndexType GetHierarchyDepthIndex() const
		{
			return m_hierarchyDepthIndex;
		}
		[[nodiscard]] CustomDepthSignedType GetCustomDepth() const
		{
			return (CustomDepthSignedType)m_customDepthIndex * CustomDepthSignedType(m_customDepthSign ? 1 : -1);
		}

		[[nodiscard]] bool operator==(const DepthInfo other) const
		{
			return m_value == other.m_value;
		}
		[[nodiscard]] bool operator!=(const DepthInfo other) const
		{
			return m_value != other.m_value;
		}
	protected:
		[[nodiscard]] static ComputedIndexType CalculateComputedDepthIndex(const DepthInfo precedingWidgetDepthInfo)
		{
			Assert(precedingWidgetDepthInfo.m_computedDepthIndex < MaximumComputedDepthIndex);
			return ComputedIndexType(precedingWidgetDepthInfo.m_computedDepthIndex + 1);
		}
		[[nodiscard]] static CustomDepthInfo
		CalculateCustomDepth(const CustomDepthSignedType parentCustomDepthIndex, const CustomDepthSignedType customDepth)
		{
			int64 result = parentCustomDepthIndex + customDepth;
			Assert(result >= MinimumCustomDepthIndex && result <= MaximumCustomDepthIndex);
			result = Math::Clamp(result, (int64)MinimumCustomDepthIndex, (int64)MaximumCustomDepthIndex);
			return CustomDepthInfo{(CustomDepthStoredType)Math::Abs(result), (CustomDepthStoredType)Math::SignNonZero(result)};
		}
	protected:
		union
		{
			struct
			{
				//! Index of the depth scope. The depth index is only relative to the scope.
				//! Essentially this means that a higher scope index is always rendered on top
				UnderlyingType m_scopeIndex : ScopeIndexBitCount;
				//! Index of depth relative to the scope
				UnderlyingType m_computedDepthIndex : ComputedDepthIndexBitCount;
				//! Index of custom depth relative to the scope. Same as z-index.
				UnderlyingType m_customDepthIndex : CustomDepthIndexBitCount;
				//! Sign of the custom depth index, in case of negative depth from z-index
				UnderlyingType m_customDepthSign : 1;
				//! Whether any children / descendants should increment our scope index by 1
				UnderlyingType m_createsNewLocalScope : 1;
				//! Whether any children / descendants should increment our scope index by 1
				UnderlyingType m_isNonPositioned : 1;
				//! Number of parents up to the root widget.
				UnderlyingType m_hierarchyDepthIndex : HierarchyDepthIndexBitCount;
				//! Helper to ensure we zero the full extent of m_value
				UnderlyingType m_remainder : (TotalBitCount - TotalRequiredBitCount);
			};
			UnderlyingType m_value;
		};
	};
	static_assert(sizeof(DepthInfo) == sizeof(uint64));

	ENUM_FLAG_OPERATORS(DepthInfo::ChangeFlags);
}
