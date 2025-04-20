#pragma once

#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::Widgets
{
	enum class OverflowType : uint8
	{
		//! Overflowing content will still be visible
		Visible,
		//! Any overflowing content will be clipped
		Hidden,
		//! Any overflowing content will be clipped, and a scrollbar will be added to see the rest of the content
		Scroll,
		//! Any overflowing content will be clipped, and a scrollbar will be added to see the rest of the content when necessary
		Auto
	};
	//! Currently breaking CSS rules, default should be visible.
	inline static constexpr OverflowType DefaultOverflowType = OverflowType::Hidden;
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::OverflowType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::OverflowType>(
			"3bb5952a-8681-4556-b894-05d06217aa77"_guid,
			MAKE_UNICODE_LITERAL("Widget Overflow Type"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{Widgets::OverflowType::Visible, MAKE_UNICODE_LITERAL("Visible")},
				Reflection::EnumTypeEntry{Widgets::OverflowType::Hidden, MAKE_UNICODE_LITERAL("Hidden")},
				Reflection::EnumTypeEntry{Widgets::OverflowType::Scroll, MAKE_UNICODE_LITERAL("Scrollable")}
			}}
		);
	};
}
