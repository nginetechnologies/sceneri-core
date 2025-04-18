#pragma once

#include <Engine/Entity/Component2D.h>
#include <Engine/Scene/SceneQuadtreeNode.h>

#include <Common/Memory/UniqueRef.h>

namespace ngine::Threading
{
	struct StageBase;
}

namespace ngine::Entity
{
	struct DestroyEmptyQuadtreeNodesJob;

	struct RootSceneComponent2D final : public Component2D
	{
		using BaseType = Component2D;
		using ParentType = HierarchyComponentBase;
		using InstanceIdentifier = TIdentifier<uint32, 2, 2>;

		RootSceneComponent2D(const Optional<HierarchyComponentBase*> pParent, SceneRegistry& sceneRegistry, Scene2D& scene);
		virtual ~RootSceneComponent2D();

		[[nodiscard]] const Scene2D& GetScene() const
		{
			return m_scene;
		}
		[[nodiscard]] Scene2D& GetScene()
		{
			return m_scene;
		}

		[[nodiscard]] SceneQuadtree& GetQuadTree()
		{
			return m_quadTree;
		}

		void AddComponent(Entity::Component2D& component, const float componentDepth, const Math::Rectanglef componentBounds);
		void OnComponentWorldLocationOrBoundsChanged(
			Entity::Component2D& component,
			const float componentDepth,
			const Math::Rectanglef componentBounds,
			Entity::SceneRegistry& sceneRegistry
		);
		void RemoveComponent(Entity::Component2D& component);

		[[nodiscard]] Threading::StageBase& GetQuadtreeCleanupJob();
	protected:
		ReferenceWrapper<Scene2D> m_scene;
		SceneQuadtree m_quadTree;
		UniqueRef<DestroyEmptyQuadtreeNodesJob> m_destroyEmptyQuadtreeNodesJob;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::RootSceneComponent2D>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::RootSceneComponent2D>(
			"ab1e90b0-8085-4bda-a908-35cf10b0f013"_guid,
			MAKE_UNICODE_LITERAL("Root Scene Component 2D"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization
		);
	};
}
