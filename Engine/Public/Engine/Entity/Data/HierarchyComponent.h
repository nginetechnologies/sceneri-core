#pragma once

#include <Engine/Entity/Data/Component.h>

namespace ngine::Entity
{
	struct HierarchyComponentBase;
}

namespace ngine::Entity::Data
{
	struct HierarchyComponent : public Component
	{
		using BaseType = Component;
		using RootType = HierarchyComponent;
		using ParentType = Entity::HierarchyComponentBase;

		using BaseType::BaseType;

		struct DynamicInitializer : public Component::DynamicInitializer
		{
			using BaseType = Component::DynamicInitializer;
			using BaseType::BaseType;

			DynamicInitializer(BaseType&& baseInitializer)
				: BaseType(Forward<BaseType>(baseInitializer))
			{
			}

			[[nodiscard]] Entity::HierarchyComponentBase& GetParent() const;
		};

		struct Deserializer : public Component::Deserializer
		{
			using BaseType = Component::Deserializer;
			using BaseType::BaseType;

			[[nodiscard]] Entity::HierarchyComponentBase& GetParent() const;
		};

		struct Cloner : public Component::Cloner
		{
			using BaseType = Component::Cloner;
			using BaseType::BaseType;

			[[nodiscard]] Entity::HierarchyComponentBase& GetParent() const;
		};

		HierarchyComponent() = default;
		HierarchyComponent(Initializer&&)
		{
		}
		HierarchyComponent(const DynamicInitializer&)
		{
		}
		HierarchyComponent(const Deserializer&)
		{
		}
		HierarchyComponent(const Component&, const Cloner&)
		{
		}
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::HierarchyComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::HierarchyComponent>(
			"{0AED9203-C7DA-468A-ACC5-5B0D76A285B8}"_guid, MAKE_UNICODE_LITERAL("Hierarchy Data Component"), TypeFlags::IsAbstract
		);
	};
}
