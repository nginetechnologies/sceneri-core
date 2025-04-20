#pragma once

#include <Engine/Entity/Data/Component2D.h>

namespace ngine::Widgets
{
	struct Widget;
}

namespace ngine::Widgets::Data
{
	struct Component : public Entity::Data::Component2D
	{
		using BaseType = Component2D;
		using RootType = Component;
		using ParentType = Widget;

		using BaseType::BaseType;

		struct Initializer : public Component2D::DynamicInitializer
		{
			using BaseType = Component2D::DynamicInitializer;
			using BaseType::BaseType;

			[[nodiscard]] Widget& GetParent() const;
		};
		// This Initializer is the initializer needed to instantiate data components at runtime when type info is unknown
		using DynamicInitializer = Initializer;

		struct Deserializer : public Component2D::Deserializer
		{
			using BaseType = Component2D::Deserializer;
			using BaseType::BaseType;

			[[nodiscard]] Widget& GetParent() const;
		};

		struct Cloner : public Component2D::Cloner
		{
			using BaseType = Component2D::Cloner;
			using BaseType::BaseType;

			[[nodiscard]] Widget& GetParent() const;
		};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::Component>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::Component>(
			"{2b8920a3-ef4e-971c-abc7-a10b5676e0e0}"_guid, // TODO: Replace
			MAKE_UNICODE_LITERAL("Widget Data"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::IsAbstract
		);
	};
}
