#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>

namespace ngine::Entity
{
	struct Component2D;
}

namespace ngine::Entity::Data
{
	struct Component2D : public HierarchyComponent
	{
		using BaseType = HierarchyComponent;
		using RootType = Component2D;
		using ParentType = Entity::Component2D;

		using BaseType::BaseType;

		struct DynamicInitializer : public Component::DynamicInitializer
		{
			using BaseType = Component::DynamicInitializer;
			using BaseType::BaseType;

			[[nodiscard]] Entity::Component2D& GetParent() const;
		};
		struct Initializer : public Component::Initializer
		{
			using BaseType = Component::Initializer;
			using BaseType::BaseType;

			[[nodiscard]] Entity::Component2D& GetParent() const;
		};

		struct Deserializer : public Component::Deserializer
		{
			using BaseType = Component::Deserializer;
			using BaseType::BaseType;

			[[nodiscard]] Entity::Component2D& GetParent() const;
		};

		struct Cloner : public Component::Cloner
		{
			using BaseType = Component::Cloner;
			using BaseType::BaseType;

			[[nodiscard]] Entity::Component2D& GetParent() const;
		};

		Component2D() = default;
		Component2D(Initializer&&)
		{
		}
		Component2D(const DynamicInitializer&)
		{
		}
		Component2D(const Deserializer&)
		{
		}
		Component2D(const Component&, const Cloner&)
		{
		}
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::Component2D>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::Component2D>(
			"{3E367CF1-4764-40FF-92A0-7FC48D9FEC42}"_guid, MAKE_UNICODE_LITERAL("2D Data Component"), TypeFlags::IsAbstract
		);
	};
}
