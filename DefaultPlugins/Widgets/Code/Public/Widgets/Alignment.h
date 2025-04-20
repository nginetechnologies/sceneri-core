#pragma once

#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::Widgets
{
	enum class Alignment : uint8
	{
		Start,
		Center,
		End,
		Stretch,
		Inherit
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Alignment>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Alignment>(
			"57B8D7F4-5E53-47BC-A95B-F0F5228A8AD2"_guid,
			MAKE_UNICODE_LITERAL("Widget Alignment"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{Widgets::Alignment::Start, MAKE_UNICODE_LITERAL("Start")},
				Reflection::EnumTypeEntry{Widgets::Alignment::Center, MAKE_UNICODE_LITERAL("Center")},
				Reflection::EnumTypeEntry{Widgets::Alignment::End, MAKE_UNICODE_LITERAL("End")},
				Reflection::EnumTypeEntry{Widgets::Alignment::Stretch, MAKE_UNICODE_LITERAL("Stretch")}
			}}
		);
	};
}
