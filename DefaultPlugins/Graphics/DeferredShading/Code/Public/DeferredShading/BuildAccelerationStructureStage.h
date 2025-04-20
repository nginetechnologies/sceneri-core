#pragma once

#include <Renderer/Stages/RenderItemStage.h>
#include <Renderer/Assets/StaticMesh/StaticMeshIdentifier.h>
#include <Renderer/Buffers/Buffer.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Scene/InstanceBuffer.h>
#include <Renderer/Wrappers/AccelerationStructure.h>
#include <Renderer/Metal/Includes.h>

#include <Common/Asset/Guid.h>
#include <Common/Memory/Containers/ByteView.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Function/Event.h>

namespace ngine::Rendering
{
	struct SceneView;
	struct ShadowsStage;

	struct BuildAccelerationStructureStage final : public Rendering::RenderItemStage
	{
		inline static constexpr Asset::Guid Guid = "baec125a-0fd4-4b29-96cb-258bf61b4156"_asset;

		BuildAccelerationStructureStage(SceneView& sceneView);
		virtual ~BuildAccelerationStructureStage();

#if RENDERER_SUPPORTS_RAYTRACING
		[[nodiscard]] AccelerationStructureView GetInstancesAccelerationStructure() const
		{
			return m_instancesAccelerationStructureData.m_accelerationStructure;
		}

		[[nodiscard]] IdentifierArrayView<const PrimitiveAccelerationStructure, StaticMeshIdentifier> GetMeshAccelerationStructures() const;
#endif
	protected:
		// RenderItemStage
		virtual void OnRenderItemsBecomeVisible(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnVisibleRenderItemsReset(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnRenderItemsBecomeHidden(
			const Entity::RenderItemMask& renderItems,
			SceneBase& scene,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnVisibleRenderItemTransformsChanged(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		[[nodiscard]] virtual Threading::JobBatch LoadRenderItemsResources(const Entity::RenderItemMask& renderItems) override;
		virtual void OnSceneUnloaded() override;

		[[nodiscard]] virtual QueueFamily GetRecordedQueueFamily() const override
		{
			return QueueFamily::Graphics;
		}
		[[nodiscard]] virtual bool ShouldRecordCommands() const override;
		virtual void RecordCommands(const CommandEncoderView commandEncoder) override;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::AccelerationStructureBuild;
		}

		virtual void OnDisabled(Rendering::CommandEncoderView, PerFrameStagingBuffer&) override
		{
		}
		virtual void OnEnable(Rendering::CommandEncoderView, PerFrameStagingBuffer&) override
		{
		}

		virtual void OnActiveCameraPropertiesChanged(
			const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		// ~RenderItemStage

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Build Acceleration Structure Stage";
		}
#endif
	protected:
		void BuildGeometry(const Rendering::CommandEncoderView graphicsCommandEncoder);
		void BuildInstances(const Rendering::CommandEncoderView graphicsCommandEncoder);
	protected:
		friend ShadowsStage; // temp

		SceneView& m_sceneView;

#if RENDERER_SUPPORTS_RAYTRACING
		InstanceBuffer m_instanceBuffer;
		Buffer m_instanceCountBuffer;

		struct QueuedGeometryBuild
		{
			StaticMeshIdentifier m_meshIdentifier;

			InlineVector<uint32, 1> m_queuedMeshTriangleCounts;
			InlineVector<PrimitiveAccelerationStructure::TriangleGeometryDescriptor, 1> m_queuedMeshAccelerationStructureGeometry;
			InlineVector<PrimitiveAccelerationStructureBuildRangeInfo, 1> m_queuedMeshBuildRanges;
		};

		struct PendingAccelerationStructureData
		{
			Buffer m_scratchBuffer;
			StagingBuffer m_stagingBuffer;
			IdentifierMask<Entity::RenderItemIdentifier> m_pendingRenderItems;
		};

		struct PrimitiveAccelerationStructureData
		{
			// TODO: Move this out so it doesn't exist when we finish compiling
			PendingAccelerationStructureData m_pendingData;

			Buffer m_buffer;
			AccelerationStructureView::ResourceIdentifier m_resourceIdentifier;
		};

		struct InstanceAccelerationStructureData
		{
			// TODO: Move this out so it doesn't exist when we finish compiling
			PendingAccelerationStructureData m_pendingData;

			Buffer m_buffer;
			InstanceAccelerationStructure m_accelerationStructure;
			AccelerationStructureView::ResourceIdentifier m_resourceIdentifier;
		};

		Threading::Mutex m_queuedGeometryBuildsMutex;
		Vector<QueuedGeometryBuild> m_queuedGeometryBuilds;
		bool m_shouldRebuildInstances{true};

		IdentifierMask<StaticMeshIdentifier> m_builtMeshes;
		IdentifierMask<StaticMeshIdentifier> m_loadedMeshes;
		Threading::AtomicIdentifierMask<StaticMeshIdentifier> m_buildingMeshes;
		IdentifierMask<Entity::RenderItemIdentifier> m_processedRenderItems;
		TIdentifierArray<PrimitiveAccelerationStructureData, StaticMeshIdentifier> m_meshAccelerationStructureData;
		TIdentifierArray<PrimitiveAccelerationStructure, StaticMeshIdentifier> m_meshAccelerationStructures;

		TIdentifierArray<InstanceBuffer::InstanceIndexType, Entity::RenderItemIdentifier> m_instanceIndices;
		Array<Entity::RenderItemIdentifier::IndexType, 60000, uint32, uint32> m_instanceRenderItems;

		InstanceAccelerationStructureData m_instancesAccelerationStructureData;
#endif
	};
}
