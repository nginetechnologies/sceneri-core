#pragma once

#include "PerLogicalDeviceSceneData.h"

#include <Engine/Entity/RenderItemIdentifier.h>

#include <Common/Storage/IdentifierArray.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/UniquePtr.h>

#include <Renderer/Devices/LogicalDeviceIdentifier.h>
#include <Renderer/Assets/StaticMesh/StaticMeshIdentifier.h>
#include <Renderer/Constants.h>
#include <Renderer/Stages/RenderItemStageMask.h>

namespace ngine
{
	namespace Entity
	{
		struct HierarchyComponentBase;
		struct CameraComponent;
	}

	struct Scene3D;
}

namespace ngine::Rendering
{
	struct SceneView;
	struct LogicalDevice;
	struct RenderMaterialInstance;

	struct SceneData
	{
		SceneData();
		SceneData(const SceneData&) = delete;
		SceneData& operator=(const SceneData&) = delete;
		SceneData(SceneData&&) = delete;
		SceneData& operator=(SceneData&&) = delete;
		~SceneData();

		[[nodiscard]] PURE_LOCALS_AND_POINTERS Scene3D& GetScene();
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const Scene3D& GetScene() const;

		void OnRenderItemAdded(Entity::HierarchyComponentBase& renderItem);
		void OnRenderItemRemoved(const Entity::RenderItemIdentifier renderItemIdentifier);
		void OnRenderItemEnabled(Entity::HierarchyComponentBase& renderItem);
		void OnRenderItemDisabled(const Entity::RenderItemIdentifier renderItemIdentifier);
		void OnRenderItemTransformChanged(Entity::HierarchyComponentBase& renderItem) const;
		void OnRenderItemStageMaskEnabled(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& stages) const;
		void OnRenderItemStageMaskDisabled(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& stages) const;
		void OnRenderItemStageMaskChanged(
			const Entity::RenderItemIdentifier renderItemIdentifier,
			const RenderItemStageMask& enabledStages,
			const RenderItemStageMask& disabledStages
		) const;
		void OnRenderItemStageMaskReset(const Entity::RenderItemIdentifier renderItemIdentifier, const RenderItemStageMask& resetStages) const;
		void OnRenderItemStageEnabled(
			const Entity::RenderItemIdentifier renderItemIdentifier, const SceneRenderStageIdentifier stageIdentifier
		) const;
		void OnRenderItemStageDisabled(
			const Entity::RenderItemIdentifier renderItemIdentifier, const SceneRenderStageIdentifier stageIdentifier
		) const;
		void
		OnRenderItemStageReset(const Entity::RenderItemIdentifier renderItemIdentifier, const SceneRenderStageIdentifier stageIdentifier) const;
	};
}
