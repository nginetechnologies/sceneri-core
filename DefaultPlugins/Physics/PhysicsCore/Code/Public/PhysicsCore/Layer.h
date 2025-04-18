#pragma once

#include <Common/EnumFlagOperators.h>
#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::Physics
{
	enum class Layer : uint8
	{
		Static,
		Dynamic,
		Queries,
		Triggers,
		Gravity,
		Count,
		Invalid
	};

	enum class LayerMask : uint8
	{
		None,
		Static = 1 << (uint8)Layer::Static,
		Dynamic = 1 << (uint8)Layer::Dynamic,
		Queries = 1 << (uint8)Layer::Queries,
		Triggers = 1 << (uint8)Layer::Triggers,
		Gravity = 1 << (uint8)Layer::Gravity
	};
	ENUM_FLAG_OPERATORS(LayerMask);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::Layer>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Layer>(
			"3a732291-cc71-4de4-ab39-13bbccba2dde"_guid,
			MAKE_UNICODE_LITERAL("Widget Layout Type"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{Physics::Layer::Static, MAKE_UNICODE_LITERAL("Static")},
				Reflection::EnumTypeEntry{Physics::Layer::Dynamic, MAKE_UNICODE_LITERAL("Dynamic")},
				Reflection::EnumTypeEntry{Physics::Layer::Queries, MAKE_UNICODE_LITERAL("Queries")},
				Reflection::EnumTypeEntry{Physics::Layer::Triggers, MAKE_UNICODE_LITERAL("Triggers")},
				Reflection::EnumTypeEntry{Physics::Layer::Gravity, MAKE_UNICODE_LITERAL("Gravity")},
			}}
		);
	};
}
