#pragma once

#include <Engine/Entity/HierarchyComponent.h>

namespace ngine::Entity
{
	struct ComponentTypeSceneDataInterface;

	struct RootComponent;
	extern template struct HierarchyComponent<HierarchyComponentBase>;

	struct RootComponent final : public HierarchyComponent<HierarchyComponentBase>
	{
		using InstanceIdentifier = TIdentifier<uint32, 1>;

		using BaseType = Component;
		using ParentType = RootComponent;
		using RootType = RootComponent;

		RootComponent(SceneRegistry& sceneRegistry);
		[[nodiscard]] virtual PURE_STATICS SceneRegistry& GetSceneRegistry() const override
		{
			return m_sceneRegistry;
		}
	protected:
		SceneRegistry& m_sceneRegistry;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::RootComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::RootComponent>(
			"{6B3B1160-67BC-4D91-92BF-FC0157088042}"_guid,
			MAKE_UNICODE_LITERAL("Root Component"),
			TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicDeserialization
		);
	};
}
