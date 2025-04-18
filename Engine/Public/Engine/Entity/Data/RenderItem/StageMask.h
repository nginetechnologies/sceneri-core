#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>
#include <Engine/Entity/RenderItemIdentifier.h>

#include <Renderer/Assets/Stage/SceneRenderStageIdentifier.h>
#include <Renderer/Stages/RenderItemStageMask.h>
#include <Renderer/Stages/RenderItemStagesProperty.h>

#include <Common/Storage/AtomicIdentifierMask.h>

namespace ngine::Entity
{
	struct Component3D;
	struct RenderItemComponent;
}

namespace ngine::Rendering
{
	// Mask indicating which rendering stages a render item will be used in
	using AtomicRenderItemStageMask = Threading::AtomicIdentifierMask<SceneRenderStageIdentifier>;
}

namespace ngine::Entity::Data::RenderItem
{
	struct StageMask final : public HierarchyComponent
	{
		using InstanceIdentifier = Entity::RenderItemIdentifier;

		StageMask() = default;
		StageMask(const Rendering::RenderItemStageMask& __restrict stageMask)
			: m_stageMask(stageMask)
		{
		}

		[[nodiscard]] operator const Rendering::AtomicRenderItemStageMask &() const
		{
			return m_stageMask;
		}
		[[nodiscard]] operator Rendering::AtomicRenderItemStageMask&()
		{
			return m_stageMask;
		}

		void EnableStages(HierarchyComponentBase& owner, const Rendering::RenderItemStageMask& newMask);
		void DisableStages(HierarchyComponentBase& owner, const Rendering::RenderItemStageMask& newMask);
		void ResetStages(HierarchyComponentBase& owner, const Rendering::RenderItemStageMask& resetStages);
		void ResetStages(HierarchyComponentBase& owner);
		void EnableStage(HierarchyComponentBase& owner, const Rendering::SceneRenderStageIdentifier stageIdentifier);
		void DisableStage(HierarchyComponentBase& owner, const Rendering::SceneRenderStageIdentifier stageIdentifier);
		void ResetStage(HierarchyComponentBase& owner, const Rendering::SceneRenderStageIdentifier stageIdentifier);

		[[nodiscard]] bool IsStageEnabled(const Rendering::SceneRenderStageIdentifier stageIdentifier) const
		{
			return m_stageMask.IsSet(stageIdentifier);
		}
		[[nodiscard]] bool IsStageDisabled(const Rendering::SceneRenderStageIdentifier stageIdentifier) const
		{
			return !IsStageEnabled(stageIdentifier);
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::RenderItem::StageMask>;
		friend Entity::Component3D;
		friend Entity::RenderItemComponent;

		Rendering::RenderItemStagesProperty GetFromProperty(Entity::HierarchyComponentBase&) const;
		void SetFromProperty(Entity::HierarchyComponentBase&, const Rendering::RenderItemStagesProperty stages);
	protected:
		Rendering::AtomicRenderItemStageMask m_stageMask;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::RenderItem::StageMask>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::RenderItem::StageMask>(
			"88b24627-cd59-4bc7-a50a-0912c36dac1e"_guid,
			MAKE_UNICODE_LITERAL("Render Item Stage Mask"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Render Stages"),
				"stages",
				"add12d3d-f149-4dfc-a8ff-6074fd7fcdc0"_guid,
				MAKE_UNICODE_LITERAL("Render Stages"),
				PropertyFlags::Transient | PropertyFlags::VisibleToParentScope,
				&Entity::Data::RenderItem::StageMask::SetFromProperty,
				&Entity::Data::RenderItem::StageMask::GetFromProperty
			)}
		);
	};
}
