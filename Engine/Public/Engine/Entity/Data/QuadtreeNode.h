#pragma once

#include <Engine/Entity/Data/Component.h>

namespace ngine
{
	struct SceneQuadtreeNode;
}

namespace ngine::Entity
{
	struct Component2D;
}

namespace ngine::Entity::Data
{
	struct QuadtreeNode final : public Component
	{
		using Node = SceneQuadtreeNode;

		QuadtreeNode(const Deserializer& deserializer) = delete;
		QuadtreeNode(const QuadtreeNode&, const Cloner&) = delete;
		QuadtreeNode(Node& node)
			: m_node(node)
		{
		}
		QuadtreeNode& operator=(Node& node)
		{
			m_node = node;
			return *this;
		}

		[[nodiscard]] Node& Get() const
		{
			return m_node;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::QuadtreeNode>;

		ReferenceWrapper<Node> m_node;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::QuadtreeNode>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::QuadtreeNode>(
			"353CEF28-DA25-42FB-9C02-4A19A0E6F016"_guid,
			MAKE_UNICODE_LITERAL("Quadtree Node"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface
		);
	};
}
