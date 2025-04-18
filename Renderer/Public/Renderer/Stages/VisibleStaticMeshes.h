#pragma once

#include <Renderer/Stages/VisibleRenderItems.h>

namespace ngine::Rendering
{
	struct VisibleStaticMeshes : public VisibleRenderItems
	{
		struct InstanceGroup : public VisibleRenderItems::InstanceGroup
		{
			using BaseType = VisibleRenderItems::InstanceGroup;

			using BaseType::BaseType;
			InstanceGroup(const InstanceGroup&) = delete;
			InstanceGroup& operator=(const InstanceGroup&) = delete;
			InstanceGroup(InstanceGroup&& other)
				: BaseType(static_cast<BaseType&&>(other))
				, m_meshIdentifierIndex(other.m_meshIdentifierIndex)
				, m_renderMeshView(other.m_renderMeshView)
			{
				other.m_renderMeshView = {};
			}
			InstanceGroup& operator=(InstanceGroup&&) = default;
			virtual ~InstanceGroup()
			{
				Assert(!m_renderMeshView.IsValid(), "Destroy must be called!");
			}

			virtual void Destroy(LogicalDevice& logicalDevice) override final;

			virtual SupportResult SupportsComponent(
				const LogicalDevice& logicalDevice, const Entity::HierarchyComponentBase&, Entity::SceneRegistry& sceneRegistry
			) const override;

			Rendering::StaticMeshIdentifier::IndexType m_meshIdentifierIndex;
			Rendering::RenderMeshView m_renderMeshView;
		};

		virtual ~VisibleStaticMeshes() = default;
		Threading::JobBatch
		TryLoadRenderItemResources(LogicalDevice&, const Entity::HierarchyComponentBase& renderItem, Entity::SceneRegistry& sceneRegistry);
	protected:
		virtual UniquePtr<VisibleRenderItems::InstanceGroup> CreateInstanceGroup(
			LogicalDevice& logicalDevice,
			const Entity::HierarchyComponentBase& renderItem,
			Entity::SceneRegistry& sceneRegistry,
			const uint32 maximumInstanceCount
		) override;
	};
}
