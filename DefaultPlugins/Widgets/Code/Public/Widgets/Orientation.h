#pragma once

#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::Widgets
{
	enum class Orientation : uint8
	{
		Horizontal,
		Vertical
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Orientation>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Orientation>(
			"aad06137-9fad-4d74-b096-c3384a0678dd"_guid,
			MAKE_UNICODE_LITERAL("Widget Orientation"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{Widgets::Orientation::Horizontal, MAKE_UNICODE_LITERAL("→")},
				Reflection::EnumTypeEntry{Widgets::Orientation::Vertical, MAKE_UNICODE_LITERAL("↓")}
			}}
		);
	};
}
