#pragma once

#include <Engine/Entity/Data/Component.h>

namespace ngine
{
	struct SceneOctreeNode;
}

namespace ngine::Entity::Data
{
	struct OctreeNode final : public Component
	{
		OctreeNode(const Deserializer& deserializer) = delete;
		OctreeNode(const OctreeNode&, const Cloner&) = delete;
		OctreeNode(SceneOctreeNode& node)
			: m_node(node)
		{
		}
		OctreeNode& operator=(SceneOctreeNode& node)
		{
			m_node = node;
			return *this;
		}

		[[nodiscard]] SceneOctreeNode& Get() const
		{
			return m_node;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::OctreeNode>;

		ReferenceWrapper<SceneOctreeNode> m_node;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::OctreeNode>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::OctreeNode>(
			"7BB8CAD1-7D29-4F58-AED9-0970E3BEF3F0"_guid,
			MAKE_UNICODE_LITERAL("Octree Node"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface
		);
	};
}
