#pragma once

#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/Identifier.h>
#include <Common/Memory/Optional.h>
#include <Common/Memory/UniquePtr.h>

#include <Renderer/Stages/RenderItemStage.h>
#include <Renderer/Assets/StaticMesh/StaticMeshIdentifier.h>
#include <Renderer/Assets/Stage/SceneRenderStageIdentifier.h>
#include <Renderer/Scene/InstanceBuffer.h>
#include <Renderer/Assets/StaticMesh/RenderMeshView.h>

namespace ngine::Entity
{
	struct HierarchyComponentBase;
	struct SceneRegistry;
}

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine
{
	struct SceneBase;
}

namespace ngine::Rendering
{
	struct SceneViewBase;

	struct VisibleRenderItems
	{
		struct InstanceGroup
		{
			using IdentifierIndexType = uint32;

			InstanceGroup(LogicalDevice& logicalDevice, const InstanceBuffer::InstanceIndexType maximumInstanceCount)
				: m_instanceBuffer(logicalDevice, maximumInstanceCount, sizeof(IdentifierIndexType), Buffer::UsageFlags::VertexBuffer)
			{
			}
			InstanceGroup(const InstanceGroup&) = delete;
			InstanceGroup& operator=(const InstanceGroup&) = delete;
			InstanceGroup(InstanceGroup&&) = default;
			InstanceGroup& operator=(InstanceGroup&&) = default;
			virtual ~InstanceGroup() = default;

			virtual void Destroy(LogicalDevice& logicalDevice);

			Rendering::InstanceBuffer m_instanceBuffer;

			enum class SupportResult : uint8
			{
				//! Indicates that we can't find out support *yet*. Call again.
				Unknown,
				Unsupported,
				Supported
			};

			virtual SupportResult SupportsComponent(
				const LogicalDevice& logicalDevice, const Entity::HierarchyComponentBase&, Entity::SceneRegistry& sceneRegistry
			) const = 0;
		};

		virtual ~VisibleRenderItems();

		void AddRenderItems(
			SceneViewBase& sceneView,
			SceneBase& scene,
			LogicalDevice& logicalDevice,
			const SceneRenderStageIdentifier stageIdentifier,
			const Entity::RenderItemMask& renderItems,
			const uint32 maximumInstanceCount,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		);
		void ResetRenderItems(
			SceneViewBase& sceneView,
			SceneBase& scene,
			LogicalDevice& logicalDevice,
			const SceneRenderStageIdentifier stageIdentifier,
			const Entity::RenderItemMask& renderItems,
			const uint32 maximumInstanceCount,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		);
		void RemoveRenderItems(
			LogicalDevice& logicalDevice,
			const Entity::RenderItemMask& renderItems,
			SceneBase& scene,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		);

		void OnSceneUnloaded(LogicalDevice& logicalDevice, SceneBase& scene);
		void Destroy(LogicalDevice& logicalDevice);

		using VisibleInstanceGroupIdentifier = TIdentifier<uint32, 11>;
		VisibleInstanceGroupIdentifier FindOrCreateInstanceGroup(
			LogicalDevice& logicalDevice,
			const Entity::HierarchyComponentBase& renderItem,
			Entity::SceneRegistry& sceneRegistry,
			const uint32 maximumInstanceCount
		);
		struct InstanceGroupQueryResult
		{
			VisibleInstanceGroupIdentifier m_identifier;
			InstanceGroup::SupportResult m_result;
		};
		[[nodiscard]] InstanceGroupQueryResult FindInstanceGroup(
			const LogicalDevice& logicalDevice, const Entity::HierarchyComponentBase& renderItem, Entity::SceneRegistry& sceneRegistry
		) const;

		using VisibleInstanceGroups = TIdentifierArray<UniquePtr<InstanceGroup>, VisibleInstanceGroupIdentifier>;
		[[nodiscard]] VisibleInstanceGroups::ConstDynamicView GetVisibleItems() const
		{
			return m_instanceGroupIdentifiers.GetValidElementView(m_visibleInstanceGroups.GetView());
		}

		[[nodiscard]] VisibleInstanceGroupIdentifier::IndexType GetInstanceGroupCount() const
		{
			return m_instanceGroupCount;
		}
		[[nodiscard]] bool HasVisibleItems() const
		{
			return m_instanceGroupCount > 0;
		}
		[[nodiscard]] uint32 GetInstanceCount() const
		{
			return m_instanceCount;
		}
	private:
		void AddRenderItemsInternal(
			SceneBase& scene,
			LogicalDevice& logicalDevice,
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		);

		virtual UniquePtr<InstanceGroup> CreateInstanceGroup(
			LogicalDevice& logicalDevice,
			const Entity::HierarchyComponentBase& renderItem,
			Entity::SceneRegistry& sceneRegistry,
			const uint32 maximumInstanceCount
		) = 0;

		[[nodiscard]] VisibleInstanceGroupIdentifier CreateInstanceGroupInternal(
			LogicalDevice& logicalDevice,
			const Entity::HierarchyComponentBase& renderItem,
			Entity::SceneRegistry& sceneRegistry,
			const uint32 maximumInstanceCount
		);

		struct RenderItemInfo
		{
			VisibleInstanceGroupIdentifier::IndexType m_visibleInstanceGroupIdentifierIndex;
			InstanceBuffer::InstanceIndexType m_instanceIndex;
		};

		void RemoveRenderItemFromInstanceGroup(
			LogicalDevice& logicalDevice,
			InstanceGroup& instanceGroup,
			const VisibleInstanceGroupIdentifier instanceGroupIdentifier,
			RenderItemInfo& renderItemInfo,
			SceneBase& scene,
			const CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		);
	protected:
		TSaltedIdentifierStorage<VisibleInstanceGroupIdentifier> m_instanceGroupIdentifiers;
		VisibleInstanceGroupIdentifier::IndexType m_instanceGroupCount = 0;
		uint32 m_instanceCount{0};
		VisibleInstanceGroups m_visibleInstanceGroups{Memory::Zeroed};
		using RenderItems = TIdentifierArray<RenderItemInfo, Entity::RenderItemIdentifier>;
		RenderItems m_renderItemInfo{Memory::Zeroed};

		Array<Entity::RenderItemIdentifier::IndexType, 10000, uint32, uint32> m_tempInstanceRenderItems;
	};
}
