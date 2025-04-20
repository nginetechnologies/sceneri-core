#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Widgets
{
	enum class WidgetFlags : uint32
	{
		IsHidden = 1 << 0,
		IsParentHidden = 1 << 1,
		HasLoadedResources = 1 << 2,
		AreResourcesUpToDate = 1 << 3,
		HasStartedLoadingResources = 1 << 4,
		IsInputDisabled = 1 << 5,
		ShouldAutoApplyStyle = 1 << 6,
		//! Whether this widget is the root widget from an asset
		IsAssetRootWidget = 1 << 7,
		//! Pretend like the widget never existed
		//! A widget with this flag will not be rendered nor affect layout
		IsIgnoredFromStyle = 1 << 8,
		IsHiddenFromStyle = 1 << 9,
		//! Blocks pointer inputs from being passed back to any parent
		BlockPointerInputs = 1 << 10,
		//! The widget will be ignored, effectively equals display none, so the widget doesn't affect layout and won't be rendered
		IsIgnored = 1 << 11,
		HasHoverFocus = 1 << 12,
		IsHoverFocusInChildren = 1 << 13,
		HasInputFocus = 1 << 14,
		IsInputFocusInChildren = 1 << 15,
		HasActiveFocus = 1 << 16,
		IsActiveFocusInChildren = 1 << 17,
		IsToggledOff = 1 << 18,
		IsToggledOffFromParent = 1 << 19,
		FailedValidation = 1 << 20,
		FailedValidationFromParent = 1 << 21,
		HasRequirements = 1 << 22,
		HasRequirementsFromParent = 1 << 23,
		HasCustomFramegraph = 1 << 24,
		HasCustomFramegraphFromParent = 1 << 25,

		IsToggledOffFromAnySource = IsToggledOff | IsToggledOffFromParent,
		FailedValidationFromAnySource = FailedValidation | FailedValidationFromParent,
		HasRequirementsFromAnySource = HasRequirements | HasRequirementsFromParent,
		IsIgnoredFromAnySource = IsIgnoredFromStyle | IsIgnored,
		IsHiddenFromAnySource = IsHidden | IsParentHidden | IsHiddenFromStyle | IsIgnoredFromAnySource
	};

	ENUM_FLAG_OPERATORS(WidgetFlags);
}
