#include "Scene/SceneData.h"

#include "Scene/SceneView.h"
#include "Devices/LogicalDevice.h"

#include <Common/Memory/OffsetOf.h>

#include <Engine/Entity/CameraComponent.h>
#include <Engine/Scene/Scene.h>

#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/StaticMesh/RenderMesh.h>

namespace ngine::Rendering
{
	PURE_LOCALS_AND_POINTERS Scene& SceneData::GetScene()
	{
		const ptrdiff_t offsetFromOwner = Memory::GetOffsetOf(&Scene::m_renderData);

		return *reinterpret_cast<Scene*>(reinterpret_cast<ptrdiff_t>(this) - offsetFromOwner);
	}

	PURE_LOCALS_AND_POINTERS const Scene& SceneData::GetScene() const
	{
		const ptrdiff_t offsetFromOwner = Memory::GetOffsetOf(&Scene::m_renderData);

		return *reinterpret_cast<Scene*>(reinterpret_cast<ptrdiff_t>(this) - offsetFromOwner);
	}

	SceneData::SceneData()
	{
	}

	SceneData::~SceneData()
	{
	}

	void SceneData::OnRenderItemAdded(Entity::HierarchyComponentBase& renderItem)
	{
		for (SceneViewBase& __restrict view : GetScene().GetActiveViews())
		{
			static_cast<SceneView&>(view).OnRenderItemAdded(renderItem);
		}
	}

	void SceneData::OnRenderItemRemoved(const Entity::RenderItemIdentifier renderItemIdentifier)
	{
		for (SceneViewBase& __restrict view : GetScene().GetActiveViews())
		{
			static_cast<SceneView&>(view).OnRenderItemRemoved(renderItemIdentifier);
		}
	}

	void SceneData::OnRenderItemEnabled(Entity::HierarchyComponentBase& renderItem)
	{
		for (SceneViewBase& __restrict view : GetScene().GetActiveViews())
		{
			static_cast<SceneView&>(view).OnRenderItemEnabled(renderItem);
		}
	}

	void SceneData::OnRenderItemDisabled(const Entity::RenderItemIdentifier renderItemIdentifier)
	{
		for (SceneViewBase& __restrict view : GetScene().GetActiveViews())
		{
			static_cast<SceneView&>(view).OnRenderItemDisabled(renderItemIdentifier);
		}
	}

	void SceneData::OnRenderItemTransformChanged(Entity::HierarchyComponentBase& renderItem) const
	{
		for (SceneViewBase& view : GetScene().GetActiveViews())
		{
			static_cast<SceneView&>(view).OnRenderItemTransformChanged(renderItem);
		}
	}

	void SceneData::OnRenderItemStageEnabled(
		const Entity::RenderItemIdentifier renderItemIdentifier, const SceneRenderStageIdentifier stageIdentifier
	) const
	{
		for (SceneViewBase& view : GetScene().GetActiveViews())
		{
			static_cast<SceneView&>(view).OnRenderItemStageEnabled(renderItemIdentifier, stageIdentifier);
		}
	}

	void SceneData::OnRenderItemStageDisabled(
		const Entity::RenderItemIdentifier renderItemIdentifier, const SceneRenderStageIdentifier stageIdentifier
	) const
	{
		for (SceneViewBase& view : GetScene().GetActiveViews())
		{
			static_cast<SceneView&>(view).OnRenderItemStageDisabled(renderItemIdentifier, stageIdentifier);
		}
	}

	void SceneData::OnRenderItemStageReset(
		const Entity::RenderItemIdentifier renderItemIdentifier, const SceneRenderStageIdentifier stageIdentifier
	) const
	{
		for (SceneViewBase& view : GetScene().GetActiveViews())
		{
			static_cast<SceneView&>(view).OnRenderItemStageReset(renderItemIdentifier, stageIdentifier);
		}
	}

	void SceneData::OnRenderItemStageMaskReset(
		const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& resetStages
	) const
	{
		for (SceneViewBase& view : GetScene().GetActiveViews())
		{
			static_cast<SceneView&>(view).OnRenderItemStageMaskReset(renderItemIdentifier, resetStages);
		}
	}

	void SceneData::OnRenderItemStageMaskEnabled(
		const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& resetStages
	) const
	{
		for (SceneViewBase& view : GetScene().GetActiveViews())
		{
			static_cast<SceneView&>(view).OnRenderItemStageMaskEnabled(renderItemIdentifier, resetStages);
		}
	}

	void SceneData::OnRenderItemStageMaskDisabled(
		const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& resetStages
	) const
	{
		for (SceneViewBase& view : GetScene().GetActiveViews())
		{
			static_cast<SceneView&>(view).OnRenderItemStageMaskDisabled(renderItemIdentifier, resetStages);
		}
	}

	void SceneData::OnRenderItemStageMaskChanged(
		const Entity::RenderItemIdentifier renderItemIdentifier,
		const RenderItemStageMask& enabledStages,
		const RenderItemStageMask& disabledStages
	) const
	{
		for (SceneViewBase& view : GetScene().GetActiveViews())
		{
			static_cast<SceneView&>(view).OnRenderItemStageMaskChanged(renderItemIdentifier, enabledStages, disabledStages);
		}
	}
}
