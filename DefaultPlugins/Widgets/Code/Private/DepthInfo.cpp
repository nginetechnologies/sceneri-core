#include "DepthInfo.h"
#include "DepthValue.h"

#include <Widgets/Style/CombinedEntry.h>

namespace ngine::Widgets
{
	DepthInfo::DepthInfo(
		const DepthInfo parentDepthInfo,
		const DepthInfo precedingWidgetDepthInfo,
		const Style::CombinedEntry& style,
		const EnumFlags<Style::Modifier> activeModifiers
	)
		: DepthInfo(parentDepthInfo, precedingWidgetDepthInfo, style, style.GetMatchingModifiers(activeModifiers))
	{
	}

	DepthInfo::DepthInfo(
		const DepthInfo parentDepthInfo,
		const DepthInfo precedingWidgetDepthInfo,
		const Style::CombinedEntry& style,
		const Style::CombinedMatchingEntryModifiersView matchingModifiers
	)
		: DepthInfo(parentDepthInfo, precedingWidgetDepthInfo, 0, false, false)
	{
		bool shouldCreateNewLocalScope{false};

		// CSS: Element with an opacity value less than 1 -> new stacking context
		if (const Optional<const Math::Ratiof*> pOpacity = style.Find<Math::Ratiof>(Style::ValueTypeIdentifier::Opacity, matchingModifiers);
		    pOpacity.IsValid() && *pOpacity < 1.f)
		{
			shouldCreateNewLocalScope = true;
		}

		const PositionType positionType =
			style.GetWithDefault<PositionType>(Style::ValueTypeIdentifier::PositionType, PositionType::Default, matchingModifiers);

		if (const Optional<const Style::EntryValue*> pDepthOffsetValue = style.Find(Style::ValueTypeIdentifier::DepthOffset, matchingModifiers);
		    pDepthOffsetValue.IsValid() && positionType != PositionType::Static)
		{
			int32 depthOffset = pDepthOffsetValue->GetExpected<int32>();
			depthOffset = Math::Clamp(depthOffset, (int32)MinimumCustomDepthIndex, (int32)MaximumCustomDepthIndex);

			const int16 localDepth = (int16)depthOffset;
			const uint32 customDepthIndex = Math::Abs(localDepth);
			const uint32 customDepthSign = Math::SignNonZero(localDepth);

			m_customDepthIndex = customDepthIndex;
			m_customDepthSign = customDepthSign;

			// CSS: Element with a position value absolute or relative and z-index value other than auto -> new stacking context
			shouldCreateNewLocalScope |= (positionType == PositionType::Absolute) | (positionType == PositionType::Relative);

			// CSS: Element that is a child of a flex container, with z-index value other than auto. -> new stacking context
			// CSS: Element that is a child of a grid container, with z-index value other than auto. -> new stacking context
			const LayoutType layoutType =
				style.GetWithDefault<LayoutType>(Style::ValueTypeIdentifier::LayoutType, LayoutType::Block, matchingModifiers);
			shouldCreateNewLocalScope |= (layoutType == LayoutType::Flex) | (layoutType == LayoutType::Grid);
		}
		else
		{
			m_customDepthIndex = 0;
			m_customDepthSign = 1;
		}
		m_createsNewLocalScope = shouldCreateNewLocalScope;

		m_isNonPositioned = positionType == PositionType::Static;
	}

	EnumFlags<DepthInfo::ChangeFlags>
	DepthInfo::OnParentOrPrecedingWidgetChanged(const DepthInfo parentDepthInfo, const DepthInfo precedingWidgetDepthInfo)
	{
		const ScopeIndexType newScopeIndex = (ScopeIndexType)parentDepthInfo.m_scopeIndex +
		                                     (ScopeIndexType)parentDepthInfo.m_createsNewLocalScope;

		const ComputedIndexType newComputedDepthIndex = CalculateComputedDepthIndex(precedingWidgetDepthInfo);

		EnumFlags<ChangeFlags> changeFlags{
			(ChangeFlags::ComputedDepth * (m_computedDepthIndex != newComputedDepthIndex)) |
			(ChangeFlags::ScopeIndex * (m_scopeIndex != newScopeIndex))
		};

		m_scopeIndex = newScopeIndex;
		m_computedDepthIndex = newComputedDepthIndex;

		/*const CustomDepthInfo customDepth = CalculateCustomDepth(parentDepthInfo.GetCustomDepth(), GetCustomDepth());
		changeFlags |= ChangeFlags::CustomDepth * ((m_customDepthIndex = customDepth.m_index) |
		(m_customDepthSign != customDepth.m_sign));

		m_customDepthIndex = customDepth.m_index;
		m_customDepthSign = customDepth.m_sign;*/

		return changeFlags;
	}

	EnumFlags<DepthInfo::ChangeFlags>
	DepthInfo::OnStyleChanged(const Style::CombinedEntry& style, const Style::CombinedMatchingEntryModifiersView matchingModifiers)
	{
		const DepthInfo newDepthInfo(DepthInfo(), DepthInfo(), style, matchingModifiers);

		const EnumFlags<ChangeFlags> changeFlags{
			(ChangeFlags::CustomDepth *
		   ((m_customDepthIndex = newDepthInfo.m_customDepthIndex) | (m_customDepthSign != newDepthInfo.m_customDepthSign))) |
			(ChangeFlags::CreatesNewLocalScope * (m_createsNewLocalScope != newDepthInfo.m_createsNewLocalScope)) |
			(ChangeFlags::IsNonPositioned * (m_isNonPositioned != newDepthInfo.m_isNonPositioned))
		};

		m_customDepthIndex = newDepthInfo.m_customDepthIndex;
		m_customDepthSign = newDepthInfo.m_customDepthSign;
		m_createsNewLocalScope = newDepthInfo.m_createsNewLocalScope;
		m_isNonPositioned = newDepthInfo.m_isNonPositioned;
		return changeFlags;
	}

	float DepthInfo::GetDepthRatio(const float minimumDepth, const float maximumDepth) const
	{
		// From CSS standard: z-index order (excluding backgrounds, tables etc that we do differently):
		// - First, sort scopes. Within each scope:
		// 1) The element itself, if it is root or block
		// 2) Stacking contexts with negative z-indices
		// 3) Non-positioned descendants
		// 4) Non-positioned floating descendants
		// 5) Otherwise, element -> all non-positioned descendants
		// 6) All positioned decendants with z-index: auto and z-index: 0, in tree order
		// 7) Stacking contexts with positive (non-zero) z-indices, tree order if equal

		// TODO: Currently ignoring stacking contexts / scopes.
		// Change rendering to batch scopes in one render call, then store the whole batch as the scope parent's depth

		const int16 customDepth = GetCustomDepth();

		DepthValue value;
		value.m_value = 0;

		// value.m_scopeIndex = m_scopeIndex;
		const uint32 negativeZIndexValue = (uint32)-Math::Min(customDepth, 0);
		Assert(negativeZIndexValue <= DepthValue::MaximumNegativeZIndex);
		value.m_negativeZIndexValue = negativeZIndexValue;
		// value.m_isNonPositioned = m_isNonPositioned;
		//  value.m_isNonPositionedFloating = 0;
		value.m_zeroZIndexValue = customDepth == 0;
		const uint32 positiveZIndexValue = Math::Max(customDepth, 0);
		Assert(positiveZIndexValue <= DepthValue::MaximumPositiveZIndex);
		value.m_positiveZIndexValue = Math::Min(positiveZIndexValue, DepthValue::MaximumPositiveZIndex);
		value.m_computedDepthIndex = m_computedDepthIndex;

		return value.GetDepthRatio(minimumDepth, maximumDepth);
	}

	PURE_STATICS float DepthValue::GetDepthRatio(const float minimumDepth, const float maximumDepth) const
	{
		const double depthValue = (double)m_value;
		Assert(depthValue >= minimumDepth, "Decrease minimumDepth to fit the expected depth value");
		Assert(depthValue <= maximumDepth, "Increase maximumDepth to fit the expected depth value");

		if constexpr (DEBUG_BUILD)
		{
			static double maximumEncounteredDepth = 0.0;
			maximumEncounteredDepth = Math::Max(maximumEncounteredDepth, depthValue);
			static double minimumEncounteredDepth = Math::NumericLimits<double>::Max;
			minimumEncounteredDepth = Math::Min(minimumEncounteredDepth, depthValue);

			static uint32 counter = 0;
			counter++;
			// Wait for min and max encountered depth to stabilize somewhat
			if (counter > 5000)
			{
				// Assert(Math::Abs(maximumEncounteredDepth - MaximumDepth) <= 1000.0, "T");
			}
		}

		const double computedDepth = (depthValue - minimumDepth) / (maximumDepth - minimumDepth);
		return (float)computedDepth;
	}
}
