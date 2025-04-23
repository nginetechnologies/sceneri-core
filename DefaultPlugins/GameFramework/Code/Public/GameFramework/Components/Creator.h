#pragma once

#include <Engine/Entity/Data/Component.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/ComponentSoftReference.h>

namespace ngine::Entity
{
	struct SceneRegistry;
	struct Component3D;
}

namespace ngine::GameFramework
{
	struct Creator final : public Entity::Data::Component
	{
		using BaseType = Entity::Data::Component;

		struct Initializer : public Entity::Data::Component::Initializer
		{
			using BaseType = Entity::Data::Component::Initializer;

			Initializer(BaseType&& initializer, Entity::SceneComponent& creator)
				: BaseType(Forward<BaseType>(initializer))
				, m_creator(creator)
			{
			}

			Entity::SceneComponent& m_creator;
		};

		Creator(Initializer&& initializer);
		virtual ~Creator()
		{
		}

		[[nodiscard]] Optional<Entity::SceneComponent*> Find(const Entity::SceneRegistry& sceneRegistry) const;
	private:
		Entity::ComponentSoftReference m_creator;
	};

	[[nodiscard]] Optional<Entity::SceneComponent*> FindCreator(const Entity::Component3D& component);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::Creator>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::Creator>(
			"1286de33-2d0c-4669-9647-3e6dcfd1b40b"_guid,
			MAKE_UNICODE_LITERAL("Creator Component"),
			Reflection::TypeFlags::DisableDynamicInstantiation | Reflection::TypeFlags::DisableDynamicCloning |
				Reflection::TypeFlags::DisableDynamicDeserialization | Reflection::TypeFlags::DisableWriteToDisk
		);
	};
}
