#pragma once

#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::Widgets
{
	enum class LayoutType : uint8
	{
		None,
		Block,
		Flex,
		Grid
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::LayoutType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::LayoutType>(
			"bd3b864b-4bf6-43ca-bc69-a85165249a33"_guid,
			MAKE_UNICODE_LITERAL("Widget Layout Type"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				// Reflection::EnumTypeEntry{Widgets::LayoutType::None, MAKE_UNICODE_LITERAL("None")},
				Reflection::EnumTypeEntry{Widgets::LayoutType::Block, MAKE_UNICODE_LITERAL("Block")},
				Reflection::EnumTypeEntry{Widgets::LayoutType::Flex, MAKE_UNICODE_LITERAL("Flex")},
				Reflection::EnumTypeEntry{Widgets::LayoutType::Grid, MAKE_UNICODE_LITERAL("Grid")}
			}}
		);
	};
}
