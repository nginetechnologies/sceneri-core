#pragma once

#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::Physics
{
	enum class BodyType : uint8
	{
		Static,
		Kinematic,
		Dynamic
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::BodyType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::BodyType>(
			"8a9d7989-cb39-4c63-8b96-b583e5a312c3"_guid,
			MAKE_UNICODE_LITERAL("Physics Body Type"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{Physics::BodyType::Static, MAKE_UNICODE_LITERAL("Fixed")},
				Reflection::EnumTypeEntry{Physics::BodyType::Kinematic, MAKE_UNICODE_LITERAL("Controlled")},
				Reflection::EnumTypeEntry{Physics::BodyType::Dynamic, MAKE_UNICODE_LITERAL("Movable")}
			}}
		);
	};
}
