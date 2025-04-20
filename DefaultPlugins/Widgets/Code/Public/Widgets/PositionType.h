#pragma once

#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::Widgets
{
	enum class PositionType : uint8
	{
		//! Follow normal layout rules, ignore position
		Static,
		//! Follow normal layout rules, but apply position
		Relative,
		//! Ignore layout rules, but apply position relative to its parent
		Absolute,
		//! Ignore layout rules and only allow setting position externally
		Dynamic,
		Default = Static
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::PositionType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::PositionType>(
			"e442a61a-153d-437b-9f19-9a757ef5ca06"_guid,
			MAKE_UNICODE_LITERAL("Widget Position Type"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{Widgets::PositionType::Static, MAKE_UNICODE_LITERAL("Static")},
				Reflection::EnumTypeEntry{Widgets::PositionType::Relative, MAKE_UNICODE_LITERAL("Relative")},
				Reflection::EnumTypeEntry{Widgets::PositionType::Absolute, MAKE_UNICODE_LITERAL("Absolute")},
				Reflection::EnumTypeEntry{Widgets::PositionType::Dynamic, MAKE_UNICODE_LITERAL("Dynamic")}
			}}
		);
	};
}
