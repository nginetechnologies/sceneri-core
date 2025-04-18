#pragma once

#include <Engine/Entity/RenderItemIdentifier.h>
#include <Engine/Entity/ComponentIdentifier.h>
#include <Engine/Entity/RenderItemMask.h>

#include <Renderer/Buffers/StorageBuffer.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>

#include <Common/Math/ForwardDeclarations/Transform.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/FixedIdentifierArrayView.h>
#include <Common/Math/Vector3.h>
#include <Common/Math/Matrix3x3.h>
#include <Common/Math/ForwardDeclarations/WorldCoordinate.h>
#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/FlatVector.h>

namespace ngine::Entity
{
	struct SceneRegistry;
}

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
}

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct CommandQueueView;
	struct CommandEncoderView;
	struct PerFrameStagingBuffer;

	struct TransformBuffer
	{
		TransformBuffer(LogicalDevice& logicalDevice, const uint32 maximumInstanceCount);
		TransformBuffer(const TransformBuffer&) = delete;
		TransformBuffer& operator=(const TransformBuffer&) = delete;
		TransformBuffer(TransformBuffer&&) = default;
		TransformBuffer& operator=(TransformBuffer&&) = default;

		using RenderItemIndexType = typename Entity::RenderItemIdentifier::IndexType;
		using StoredRotationType = Math::Matrix3x3f;
		using StoredCoordinateType = Math::TVector3<Math::WorldCoordinateUnitType>;

		void Destroy(LogicalDevice& logicalDevice);

		void StartFrame(const Rendering::CommandEncoderView graphicsCommandEncoder);

		void OnRenderItemsBecomeVisible(
			Entity::SceneRegistry& sceneRegistry,
			LogicalDevice& logicalDevice,
			Entity::RenderItemMask& renderItems,
			const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount,
			const FixedIdentifierArrayView<Entity::ComponentIdentifier::IndexType, Entity::RenderItemIdentifier> renderItemComponentIdentifiers,
			const CommandQueueView graphicsCommandQueue,
			const CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		);
		void OnVisibleRenderItemTransformsChanged(
			Entity::SceneRegistry& sceneRegistry,
			LogicalDevice& logicalDevice,
			Entity::RenderItemMask& renderItems,
			const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount,
			const FixedIdentifierArrayView<Entity::ComponentIdentifier::IndexType, Entity::RenderItemIdentifier> renderItemComponentIdentifiers,
			const CommandQueueView graphicsCommandQueue,
			const CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		);

		void Reset();

		[[nodiscard]] bool IsValid() const
		{
			return m_buffers.GetView().All(
				[](const BufferView buffer)
				{
					return buffer.IsValid();
				}
			);
		}

		[[nodiscard]] BufferView GetRotationMatrixBuffer() const
		{
			return m_buffers[(uint8)Buffers::RotationMatrix];
		}
		[[nodiscard]] BufferView GetLocationBuffer() const
		{
			return m_buffers[(uint8)Buffers::Location];
		}
		[[nodiscard]] BufferView GetPreviousRotationMatrixBuffer() const
		{
			return m_buffers[(uint8)Buffers::PreviousRotationMatrix];
		}
		[[nodiscard]] BufferView GetPreviousLocationBuffer() const
		{
			return m_buffers[(uint8)Buffers::PreviousLocation];
		}

		[[nodiscard]] size GetRotationMatrixBufferSize() const
		{
			return m_buffers[(uint8)Buffers::RotationMatrix].GetSize();
		}
		[[nodiscard]] size GetLocationBufferSize() const
		{
			return m_buffers[(uint8)Buffers::Location].GetSize();
		}

		[[nodiscard]] DescriptorSetLayoutView GetTransformDescriptorSetLayout() const
		{
			return m_transformDescriptorSetLayout;
		}
		[[nodiscard]] DescriptorSetView GetTransformDescriptorSet() const
		{
			return m_transformDescriptorSet;
		}
	protected:
		[[nodiscard]] Array<BufferView, 4> GetBuffers() const
		{
			return {GetRotationMatrixBuffer(), GetLocationBuffer(), GetPreviousRotationMatrixBuffer(), GetPreviousLocationBuffer()};
		}
	protected:
		enum class Buffers : uint8
		{
			RotationMatrix,
			Location,
			PreviousRotationMatrix,
			PreviousLocation,
			Count
		};

		Array<StorageBuffer, (uint8)Buffers::Count> m_buffers;

		DescriptorSetLayout m_transformDescriptorSetLayout;
		DescriptorSet m_transformDescriptorSet;
		Threading::EngineJobRunnerThread* m_pDescriptorSetLoadingThread = nullptr;

		Math::Range<RenderItemIndexType> m_modifiedInstanceRange = Math::Range<RenderItemIndexType>::Make(0, 0);
	};
}
