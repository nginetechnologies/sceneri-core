#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::Entity::Data
{
	struct Component3D : public HierarchyComponent
	{
		using BaseType = HierarchyComponent;
		using RootType = Component3D;
		using ParentType = Entity::Component3D;

		using BaseType::BaseType;

		struct DynamicInitializer : public HierarchyComponent::DynamicInitializer
		{
			using BaseType = HierarchyComponent::DynamicInitializer;
			using BaseType::BaseType;

			[[nodiscard]] Entity::Component3D& GetParent() const;
		};
		using Initializer = HierarchyComponent::Initializer;

		struct Deserializer : public HierarchyComponent::Deserializer
		{
			using BaseType = HierarchyComponent::Deserializer;
			using BaseType::BaseType;

			[[nodiscard]] Entity::Component3D& GetParent() const;
		};

		struct Cloner : public HierarchyComponent::Cloner
		{
			using BaseType = HierarchyComponent::Cloner;
			using BaseType::BaseType;

			[[nodiscard]] Entity::Component3D& GetParent() const;
			[[nodiscard]] const Entity::Component3D& GetTemplateParent() const;
		};

		Component3D() = default;
		Component3D(Initializer&&)
		{
		}
		Component3D(const DynamicInitializer&)
		{
		}
		Component3D(const Deserializer&)
		{
		}
		Component3D(const Component&, const Cloner&)
		{
		}
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::Component3D>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::Component3D>(
			"{C089B4D8-5727-41FA-A2BE-A932F0B87491}"_guid, MAKE_UNICODE_LITERAL("3D Data Component"), TypeFlags::IsAbstract
		);
	};
}
