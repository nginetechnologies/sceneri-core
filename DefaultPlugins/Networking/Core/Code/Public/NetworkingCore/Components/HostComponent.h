#pragma once

#include <Engine/Entity/HierarchyComponent.h>

#include <Common/Storage/Identifier.h>

namespace ngine::Entity
{
	struct SceneRegistry;
}

namespace ngine::Network::Session
{
	struct Host final : public Entity::HierarchyComponent<Entity::HierarchyComponentBase>
	{
		using InstanceIdentifier = TIdentifier<uint32, 9>;
		using BaseType = Entity::HierarchyComponent<Entity::HierarchyComponentBase>;
		using ParentType = HierarchyComponentBase;

		Host(Initializer&& initializer);

		[[nodiscard]] virtual PURE_STATICS Entity::SceneRegistry& GetSceneRegistry() const override
		{
			return m_sceneRegistry;
		}
	private:
		Entity::SceneRegistry& m_sceneRegistry;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::Session::Host>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::Session::Host>(
			"{DF66D73D-B1B0-4271-B4FE-FD870E460A7D}"_guid,
			MAKE_UNICODE_LITERAL("Host Component"),
			Reflection::TypeFlags::DisableDynamicInstantiation | Reflection::TypeFlags::DisableDeletionFromUserInterface |
				Reflection::TypeFlags::DisableDynamicCloning
		);
	};
}
