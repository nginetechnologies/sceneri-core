#include "Scene/TransformBuffer.h"
#include "Scene/InstanceBuffer.h"

#include <Engine/Entity/Data/WorldTransform.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Scene/SceneRegistry.h>

#include <Renderer/Commands/CommandEncoder.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Wrappers/BufferMemoryBarrier.h>
#include <Renderer/Stages/PerFrameStagingBuffer.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Math/Matrix3x3.h>
#include <Common/Math/Transform.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>

namespace ngine::Rendering
{
	inline static constexpr Array TransformDescriptorBindings{
		DescriptorSetLayout::Binding::MakeStorageBuffer(0, ShaderStage::Vertex),
		DescriptorSetLayout::Binding::MakeStorageBuffer(1, ShaderStage::Vertex),
		DescriptorSetLayout::Binding::MakeStorageBuffer(2, ShaderStage::Vertex),
		DescriptorSetLayout::Binding::MakeStorageBuffer(3, ShaderStage::Vertex),
	};

	TransformBuffer::TransformBuffer(LogicalDevice& logicalDevice, const uint32 maximumInstanceCount)
		: m_buffers{
			StorageBuffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				sizeof(StoredRotationType) * maximumInstanceCount,
				StorageBuffer::Flags::TransferDestination | StorageBuffer::Flags::TransferSource
			),
				StorageBuffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				sizeof(StoredCoordinateType) * maximumInstanceCount,
				StorageBuffer::Flags::TransferDestination | StorageBuffer::Flags::TransferSource
			),
			StorageBuffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				sizeof(StoredRotationType) * maximumInstanceCount,
				StorageBuffer::Flags::TransferDestination
			),
			StorageBuffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				sizeof(StoredCoordinateType) * maximumInstanceCount,
				StorageBuffer::Flags::TransferDestination
			)
		}
		, m_transformDescriptorSetLayout(logicalDevice, TransformDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		m_transformDescriptorSetLayout.SetDebugName(logicalDevice, "Transform Buffer");
		m_buffers[(uint8)Buffers::RotationMatrix].SetDebugName(logicalDevice, "Rotation Buffer");
		m_buffers[(uint8)Buffers::Location].SetDebugName(logicalDevice, "Location Buffer");
		m_buffers[(uint8)Buffers::PreviousRotationMatrix].SetDebugName(logicalDevice, "Previous Rotation Buffer");
		m_buffers[(uint8)Buffers::PreviousLocation].SetDebugName(logicalDevice, "Previous Location Buffer");
#endif
		Threading::EngineJobRunnerThread& engineThread = *Threading::EngineJobRunnerThread::GetCurrent();
		Array<DescriptorSet, 1> descriptorSets;
		const bool allocatedDescriptorSets = engineThread.GetRenderData()
		                                       .GetDescriptorPool(logicalDevice.GetIdentifier())
		                                       .AllocateDescriptorSets(
																						 logicalDevice,
																						 Array<const DescriptorSetLayoutView, 1>(m_transformDescriptorSetLayout),
																						 descriptorSets.GetView()
																					 );
		m_transformDescriptorSet = Move(descriptorSets[0]);

		Assert(allocatedDescriptorSets);
		if (allocatedDescriptorSets)
		{
			m_pDescriptorSetLoadingThread = &engineThread;
		}

		Array<DescriptorSet::BufferInfo, 4> bufferInfo{
			DescriptorSet::BufferInfo{m_buffers[0], 0, sizeof(StoredRotationType) * maximumInstanceCount},
			DescriptorSet::BufferInfo{m_buffers[1], 0, sizeof(StoredCoordinateType) * maximumInstanceCount},
			DescriptorSet::BufferInfo{m_buffers[2], 0, sizeof(StoredRotationType) * maximumInstanceCount},
			DescriptorSet::BufferInfo{m_buffers[3], 0, sizeof(StoredCoordinateType) * maximumInstanceCount}
		};
		Array<DescriptorSet::UpdateInfo, 4> descriptorUpdates{
			DescriptorSet::UpdateInfo{
				m_transformDescriptorSet,
				0,
				0,
				DescriptorType::StorageBuffer,
				ArrayView<const DescriptorSet::BufferInfo>(bufferInfo[0])
			},
			DescriptorSet::UpdateInfo{
				m_transformDescriptorSet,
				1,
				0,
				DescriptorType::StorageBuffer,
				ArrayView<const DescriptorSet::BufferInfo>(bufferInfo[1])
			},
			DescriptorSet::UpdateInfo{
				m_transformDescriptorSet,
				2,
				0,
				DescriptorType::StorageBuffer,
				ArrayView<const DescriptorSet::BufferInfo>(bufferInfo[2])
			},
			DescriptorSet::UpdateInfo{
				m_transformDescriptorSet,
				3,
				0,
				DescriptorType::StorageBuffer,
				ArrayView<const DescriptorSet::BufferInfo>(bufferInfo[3])
			}
		};

		DescriptorSet::Update(logicalDevice, descriptorUpdates);
	}

	void TransformBuffer::Destroy(LogicalDevice& logicalDevice)
	{
		if (LIKELY(m_pDescriptorSetLoadingThread != nullptr))
		{
			m_pDescriptorSetLoadingThread->GetRenderData().DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(m_transformDescriptorSet));

			m_pDescriptorSetLoadingThread->QueueExclusiveCallbackFromAnyThread(
				Threading::JobPriority::DeallocateResourcesMin,
				[&logicalDevice, transformDescriptorSetLayout = Move(m_transformDescriptorSetLayout)](Threading::JobRunnerThread&) mutable
				{
					transformDescriptorSetLayout.Destroy(logicalDevice);
				}
			);
		}
		else
		{
			m_transformDescriptorSetLayout.Destroy(logicalDevice);
		}

		for (Buffer& buffer : m_buffers)
		{
			buffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
		}
	}

	void TransformBuffer::Reset()
	{
	}

	void TransformBuffer::StartFrame(const Rendering::CommandEncoderView graphicsCommandEncoder)
	{
		const Math::Range<RenderItemIndexType> modifiedInstanceRange = m_modifiedInstanceRange;
		m_modifiedInstanceRange = Math::Range<RenderItemIndexType>::Make(0, 0);

		const RenderItemIndexType totalAffectedItemCount = modifiedInstanceRange.GetSize();

		if (totalAffectedItemCount > 0)
		{
			// Copy from current to the previous rotation buffer
			const BufferView rotationBuffer = GetRotationMatrixBuffer();
			const BufferView locationBuffer = GetLocationBuffer();
			const BufferView previousRotationBuffer = GetPreviousRotationMatrixBuffer();
			const BufferView previousLocationBuffer = GetPreviousLocationBuffer();

			const size copiedRotationDataOffset = modifiedInstanceRange.GetMinimum() * sizeof(StoredRotationType);
			const size copiedLocationDataOffset = modifiedInstanceRange.GetMinimum() * sizeof(StoredCoordinateType);
			const size copiedRotationDataSize = totalAffectedItemCount * sizeof(StoredRotationType);
			const size copiedLocationDataSize = totalAffectedItemCount * sizeof(StoredCoordinateType);

			{
				Array<Rendering::BufferMemoryBarrier, 4> barriers{
					Rendering::BufferMemoryBarrier{
						AccessFlags::VertexRead | AccessFlags::TransferWrite,
						AccessFlags::TransferRead,
						rotationBuffer,
						copiedRotationDataOffset,
						copiedRotationDataSize
					},

					Rendering::BufferMemoryBarrier{
						AccessFlags::VertexRead | AccessFlags::TransferWrite,
						AccessFlags::TransferRead,
						locationBuffer,
						copiedLocationDataOffset,
						copiedLocationDataSize
					},

					Rendering::BufferMemoryBarrier{
						AccessFlags::VertexRead | AccessFlags::TransferWrite,
						AccessFlags::TransferWrite,
						previousRotationBuffer,
						copiedRotationDataOffset,
						copiedRotationDataSize
					},

					Rendering::BufferMemoryBarrier{
						AccessFlags::VertexRead | AccessFlags::TransferWrite,
						AccessFlags::TransferWrite,
						previousLocationBuffer,
						copiedLocationDataOffset,
						copiedLocationDataSize
					}
				};

				graphicsCommandEncoder.RecordPipelineBarrier(
					PipelineStageFlags::VertexInput | PipelineStageFlags::Transfer,
					PipelineStageFlags::Transfer,
					{},
					barriers.GetView()
				);
			}

			BlitCommandEncoder blitCommandEncoder = graphicsCommandEncoder.BeginBlit();

			blitCommandEncoder.RecordCopyBufferToBuffer(
				rotationBuffer,
				previousRotationBuffer,
				Array<const BufferCopy, 1>{BufferCopy{copiedRotationDataOffset, copiedRotationDataOffset, copiedRotationDataSize}}
			);
			blitCommandEncoder.RecordCopyBufferToBuffer(
				locationBuffer,
				previousLocationBuffer,
				Array<const BufferCopy, 1>{BufferCopy{copiedLocationDataOffset, copiedLocationDataOffset, copiedLocationDataSize}}
			);

			{
				Array<Rendering::BufferMemoryBarrier, 4> barriers{
					Rendering::BufferMemoryBarrier{
						AccessFlags::TransferRead,
						AccessFlags::VertexRead,
						rotationBuffer,
						copiedRotationDataOffset,
						copiedRotationDataSize
					},

					Rendering::BufferMemoryBarrier{
						AccessFlags::TransferRead,
						AccessFlags::VertexRead,
						locationBuffer,
						copiedLocationDataOffset,
						copiedLocationDataSize
					},

					Rendering::BufferMemoryBarrier{
						AccessFlags::TransferWrite,
						AccessFlags::VertexRead,
						previousRotationBuffer,
						copiedRotationDataOffset,
						copiedRotationDataSize
					},

					Rendering::BufferMemoryBarrier{
						AccessFlags::TransferWrite,
						AccessFlags::VertexRead,
						previousLocationBuffer,
						copiedLocationDataOffset,
						copiedLocationDataSize
					}
				};

				graphicsCommandEncoder.RecordPipelineBarrier(PipelineStageFlags::Transfer, PipelineStageFlags::VertexInput, {}, barriers.GetView());
			}
		}
	}

	void TransformBuffer::OnRenderItemsBecomeVisible(
		Entity::SceneRegistry& sceneRegistry,
		LogicalDevice& logicalDevice,
		Entity::RenderItemMask& renderItems,
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount,
		const FixedIdentifierArrayView<Entity::ComponentIdentifier::IndexType, Entity::RenderItemIdentifier> renderItemComponentIdentifiers,
		const CommandQueueView graphicsCommandQueue,
		const CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		RenderItemIndexType minimumInstanceIndex = Math::NumericLimits<RenderItemIndexType>::Max;
		RenderItemIndexType maximumInstanceIndex = Math::NumericLimits<RenderItemIndexType>::Min;
		for (const RenderItemIndexType renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			minimumInstanceIndex = Math::Min(renderItemIndex, minimumInstanceIndex);
			maximumInstanceIndex = Math::Max(renderItemIndex, maximumInstanceIndex);
		}

		RenderItemIndexType totalAffectedItemCount = maximumInstanceIndex - minimumInstanceIndex + 1;

		const size copiedRotationDataOffset = minimumInstanceIndex * sizeof(StoredRotationType);
		const size copiedLocationDataOffset = minimumInstanceIndex * sizeof(StoredCoordinateType);
		const size copiedRotationDataSize = totalAffectedItemCount * sizeof(StoredRotationType);
		const size copiedLocationDataSize = totalAffectedItemCount * sizeof(StoredCoordinateType);

		const BufferView rotationBuffer = GetRotationMatrixBuffer();
		const BufferView locationBuffer = GetLocationBuffer();
		const BufferView previousRotationBuffer = GetPreviousRotationMatrixBuffer();
		const BufferView previousLocationBuffer = GetPreviousLocationBuffer();

		{
			Array<Rendering::BufferMemoryBarrier, 4> barriers{
				Rendering::BufferMemoryBarrier{
					AccessFlags::VertexRead | AccessFlags::TransferWrite,
					AccessFlags::TransferWrite,
					rotationBuffer,
					copiedRotationDataOffset,
					copiedRotationDataSize
				},

				Rendering::BufferMemoryBarrier{
					AccessFlags::VertexRead | AccessFlags::TransferWrite,
					AccessFlags::TransferWrite,
					locationBuffer,
					copiedLocationDataOffset,
					copiedLocationDataSize
				},

				Rendering::BufferMemoryBarrier{
					AccessFlags::VertexRead | AccessFlags::TransferWrite,
					AccessFlags::TransferWrite,
					previousRotationBuffer,
					copiedRotationDataOffset,
					copiedRotationDataSize
				},

				Rendering::BufferMemoryBarrier{
					AccessFlags::VertexRead | AccessFlags::TransferWrite,
					AccessFlags::TransferWrite,
					previousLocationBuffer,
					copiedLocationDataOffset,
					copiedLocationDataSize
				}
			};

			graphicsCommandEncoder.RecordPipelineBarrier(
				PipelineStageFlags::VertexInput | PipelineStageFlags::Transfer,
				PipelineStageFlags::Transfer,
				{},
				barriers.GetView()
			);
		}

		PerFrameStagingBuffer::BatchCopyContext copyRotationsContext = perFrameStagingBuffer.BeginBatchCopyToBuffer(
			sizeof(StoredRotationType) * totalAffectedItemCount,
			rotationBuffer,
			copiedRotationDataOffset
		);
		PerFrameStagingBuffer::BatchCopyContext copyPreviousRotationsContext = perFrameStagingBuffer.BeginBatchCopyToBuffer(
			sizeof(StoredRotationType) * totalAffectedItemCount,
			previousRotationBuffer,
			copiedRotationDataOffset
		);
		PerFrameStagingBuffer::BatchCopyContext copyLocationsContext = perFrameStagingBuffer.BeginBatchCopyToBuffer(
			sizeof(StoredCoordinateType) * totalAffectedItemCount,
			locationBuffer,
			copiedLocationDataOffset
		);
		PerFrameStagingBuffer::BatchCopyContext copyPreviousLocationsContext = perFrameStagingBuffer.BeginBatchCopyToBuffer(
			sizeof(StoredCoordinateType) * totalAffectedItemCount,
			previousLocationBuffer,
			copiedLocationDataOffset
		);

		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& __restrict worldTransformSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::WorldTransform>();

		for (uint32 renderItemIndex = minimumInstanceIndex, renderItemIndexEnd = maximumInstanceIndex + 1;
		     renderItemIndex != renderItemIndexEnd;
		     ++renderItemIndex)
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const Entity::ComponentIdentifier componentIdentifier = Entity::ComponentIdentifier::MakeFromIndex(
				Math::Max(renderItemComponentIdentifiers[renderItemIdentifier], (Entity::ComponentIdentifier::IndexType)1u)
			);

			const uint32 index = renderItemIndex - minimumInstanceIndex;

			const Entity::Data::WorldTransform& __restrict worldTransformComponent =
				worldTransformSceneData.GetComponentImplementationUnchecked(componentIdentifier);
			const Math::WorldTransform transform = worldTransformComponent;

			const StoredRotationType rotation = transform.GetRotationMatrix();
			perFrameStagingBuffer.BatchCopyToBuffer(
				logicalDevice,
				copyRotationsContext,
				graphicsCommandQueue,
				ConstByteView::Make(rotation),
				index * sizeof(StoredRotationType)
			);
			perFrameStagingBuffer.BatchCopyToBuffer(
				logicalDevice,
				copyPreviousRotationsContext,
				graphicsCommandQueue,
				ConstByteView::Make(rotation),
				index * sizeof(StoredRotationType)
			);

			const StoredCoordinateType location = transform.GetLocation();
			perFrameStagingBuffer.BatchCopyToBuffer(
				logicalDevice,
				copyLocationsContext,
				graphicsCommandQueue,
				ConstByteView::Make(location),
				index * sizeof(StoredCoordinateType)
			);
			perFrameStagingBuffer.BatchCopyToBuffer(
				logicalDevice,
				copyPreviousLocationsContext,
				graphicsCommandQueue,
				ConstByteView::Make(location),
				index * sizeof(StoredCoordinateType)
			);
		}

		perFrameStagingBuffer.EndBatchCopyToBuffer(copyRotationsContext, graphicsCommandEncoder);
		perFrameStagingBuffer.EndBatchCopyToBuffer(copyPreviousRotationsContext, graphicsCommandEncoder);
		perFrameStagingBuffer.EndBatchCopyToBuffer(copyLocationsContext, graphicsCommandEncoder);
		perFrameStagingBuffer.EndBatchCopyToBuffer(copyPreviousLocationsContext, graphicsCommandEncoder);

		{
			Array<Rendering::BufferMemoryBarrier, 4> barriers{
				Rendering::BufferMemoryBarrier{
					AccessFlags::TransferWrite,
					AccessFlags::VertexRead,
					rotationBuffer,
					copiedRotationDataOffset,
					copiedRotationDataSize
				},
				Rendering::BufferMemoryBarrier{
					AccessFlags::TransferWrite,
					AccessFlags::VertexRead,
					locationBuffer,
					copiedLocationDataOffset,
					copiedLocationDataSize
				},
				Rendering::BufferMemoryBarrier{
					AccessFlags::TransferWrite,
					AccessFlags::VertexRead,
					previousRotationBuffer,
					copiedRotationDataOffset,
					copiedRotationDataSize
				},
				Rendering::BufferMemoryBarrier{
					AccessFlags::TransferWrite,
					AccessFlags::VertexRead,
					previousLocationBuffer,
					copiedLocationDataOffset,
					copiedLocationDataSize
				}
			};

			graphicsCommandEncoder.RecordPipelineBarrier(PipelineStageFlags::Transfer, PipelineStageFlags::VertexInput, {}, barriers.GetView());
		}

		if (m_modifiedInstanceRange.GetSize() > 0)
		{
			minimumInstanceIndex = Math::Min(m_modifiedInstanceRange.GetMinimum(), minimumInstanceIndex);
			maximumInstanceIndex = Math::Max(m_modifiedInstanceRange.GetMaximum(), maximumInstanceIndex);
			totalAffectedItemCount = maximumInstanceIndex - minimumInstanceIndex + 1;
		}
		m_modifiedInstanceRange = Math::Range<RenderItemIndexType>::Make(minimumInstanceIndex, totalAffectedItemCount);
	}

	void TransformBuffer::OnVisibleRenderItemTransformsChanged(
		Entity::SceneRegistry& sceneRegistry,
		LogicalDevice& logicalDevice,
		Entity::RenderItemMask& renderItems,
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount,
		const FixedIdentifierArrayView<Entity::ComponentIdentifier::IndexType, Entity::RenderItemIdentifier> renderItemComponentIdentifiers,
		const CommandQueueView graphicsCommandQueue,
		const CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		RenderItemIndexType minimumInstanceIndex = Math::NumericLimits<RenderItemIndexType>::Max;
		RenderItemIndexType maximumInstanceIndex = Math::NumericLimits<RenderItemIndexType>::Min;
		for (const RenderItemIndexType renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			minimumInstanceIndex = Math::Min(renderItemIndex, minimumInstanceIndex);
			maximumInstanceIndex = Math::Max(renderItemIndex, maximumInstanceIndex);
		}

		RenderItemIndexType totalAffectedItemCount = maximumInstanceIndex - minimumInstanceIndex + 1;

		const size copiedRotationDataOffset = minimumInstanceIndex * sizeof(StoredRotationType);
		const size copiedLocationDataOffset = minimumInstanceIndex * sizeof(StoredCoordinateType);
		const size copiedRotationDataSize = totalAffectedItemCount * sizeof(StoredRotationType);
		const size copiedLocationDataSize = totalAffectedItemCount * sizeof(StoredCoordinateType);

		const BufferView rotationBuffer = GetRotationMatrixBuffer();
		const BufferView locationBuffer = GetLocationBuffer();

		{
			Array<Rendering::BufferMemoryBarrier, 2> barriers{
				Rendering::BufferMemoryBarrier{
					AccessFlags::VertexRead | AccessFlags::TransferWrite,
					AccessFlags::TransferWrite,
					rotationBuffer,
					copiedRotationDataOffset,
					copiedRotationDataSize
				},

				Rendering::BufferMemoryBarrier{
					AccessFlags::VertexRead | AccessFlags::TransferWrite,
					AccessFlags::TransferWrite,
					locationBuffer,
					copiedLocationDataOffset,
					copiedLocationDataSize
				}
			};

			graphicsCommandEncoder.RecordPipelineBarrier(
				PipelineStageFlags::VertexInput | PipelineStageFlags::Transfer,
				PipelineStageFlags::Transfer,
				{},
				barriers.GetView()
			);
		}

		PerFrameStagingBuffer::BatchCopyContext copyRotationsContext = perFrameStagingBuffer.BeginBatchCopyToBuffer(
			sizeof(StoredRotationType) * totalAffectedItemCount,
			rotationBuffer,
			copiedRotationDataOffset
		);
		PerFrameStagingBuffer::BatchCopyContext copyLocationsContext = perFrameStagingBuffer.BeginBatchCopyToBuffer(
			sizeof(StoredCoordinateType) * totalAffectedItemCount,
			locationBuffer,
			copiedLocationDataOffset
		);

		Entity::ComponentTypeSceneData<Entity::Data::WorldTransform>& __restrict worldTransformSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::WorldTransform>();

		for (uint32 renderItemIndex = minimumInstanceIndex, renderItemIndexEnd = maximumInstanceIndex + 1;
		     renderItemIndex != renderItemIndexEnd;
		     ++renderItemIndex)
		{
			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			const Entity::ComponentIdentifier componentIdentifier = Entity::ComponentIdentifier::MakeFromIndex(
				Math::Max(renderItemComponentIdentifiers[renderItemIdentifier], (Entity::ComponentIdentifier::IndexType)1u)
			);

			const uint32 index = renderItemIndex - minimumInstanceIndex;

			const Entity::Data::WorldTransform& __restrict worldTransformComponent =
				worldTransformSceneData.GetComponentImplementationUnchecked(componentIdentifier);
			const Math::WorldTransform transform = worldTransformComponent;

			const StoredRotationType rotation = transform.GetRotationMatrix();
			perFrameStagingBuffer.BatchCopyToBuffer(
				logicalDevice,
				copyRotationsContext,
				graphicsCommandQueue,
				ConstByteView::Make(rotation),
				index * sizeof(StoredRotationType)
			);

			const StoredCoordinateType location = transform.GetLocation();
			perFrameStagingBuffer.BatchCopyToBuffer(
				logicalDevice,
				copyLocationsContext,
				graphicsCommandQueue,
				ConstByteView::Make(location),
				index * sizeof(StoredCoordinateType)
			);
		}

		perFrameStagingBuffer.EndBatchCopyToBuffer(copyRotationsContext, graphicsCommandEncoder);
		perFrameStagingBuffer.EndBatchCopyToBuffer(copyLocationsContext, graphicsCommandEncoder);

		{
			Array<Rendering::BufferMemoryBarrier, 2> barriers{
				Rendering::BufferMemoryBarrier{
					AccessFlags::TransferWrite,
					AccessFlags::VertexRead,
					rotationBuffer,
					copiedRotationDataOffset,
					copiedRotationDataSize
				},
				Rendering::BufferMemoryBarrier{
					AccessFlags::TransferWrite,
					AccessFlags::VertexRead,
					locationBuffer,
					copiedLocationDataOffset,
					copiedLocationDataSize
				},
			};

			graphicsCommandEncoder.RecordPipelineBarrier(PipelineStageFlags::Transfer, PipelineStageFlags::VertexInput, {}, barriers.GetView());
		}

		if (m_modifiedInstanceRange.GetSize() > 0)
		{
			minimumInstanceIndex = Math::Min(m_modifiedInstanceRange.GetMinimum(), minimumInstanceIndex);
			maximumInstanceIndex = Math::Max(m_modifiedInstanceRange.GetMaximum(), maximumInstanceIndex);
			totalAffectedItemCount = maximumInstanceIndex - minimumInstanceIndex + 1;
		}
		m_modifiedInstanceRange = Math::Range<RenderItemIndexType>::Make(minimumInstanceIndex, totalAffectedItemCount);
	}

	InstanceBuffer::InstanceBuffer(
		LogicalDevice& logicalDevice,
		const InstanceIndexType maximumInstanceCount,
		const uint32 elementDataSize,
		const EnumFlags<Buffer::UsageFlags> bufferUsageFlags
	)
		: m_buffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				elementDataSize * maximumInstanceCount,
				bufferUsageFlags | Buffer::UsageFlags::TransferSource | Buffer::UsageFlags::TransferDestination,
				MemoryFlags::DeviceLocal | MemoryFlags::AllocateDeviceAddress * bufferUsageFlags.IsSet(Buffer::UsageFlags::ShaderDeviceAddress)
			)
		, m_capacity(maximumInstanceCount)
	{
	}

	void InstanceBuffer::Destroy(LogicalDevice& logicalDevice)
	{
		Threading::EngineJobRunnerThread& engineThread = *Threading::EngineJobRunnerThread::GetCurrent();
		engineThread.GetRenderData().DestroyBuffer(logicalDevice.GetIdentifier(), Move(m_buffer));
	}

	void InstanceBuffer::StartBatchUpload(
		const uint32 firstInstanceIndex,
		const uint32 instanceCount,
		const CommandEncoderView graphicsCommandEncoder,
		const uint32 elementDataSize
	)
	{
		{
			Array<Rendering::BufferMemoryBarrier, 1> barriers{Rendering::BufferMemoryBarrier{
				AccessFlags::VertexRead | AccessFlags::TransferWrite,
				AccessFlags::TransferWrite,
				m_buffer,
				firstInstanceIndex * elementDataSize,
				elementDataSize * instanceCount
			}};

			graphicsCommandEncoder.RecordPipelineBarrier(
				PipelineStageFlags::VertexInput | PipelineStageFlags::Transfer,
				PipelineStageFlags::Transfer,
				{},
				barriers.GetView()
			);
		}
	}

	void InstanceBuffer::EndBatchUpload(
		const uint32 firstInstanceIndex,
		const uint32 instanceCount,
		const CommandEncoderView graphicsCommandEncoder,
		const uint32 elementDataSize
	)
	{
		{
			Array<Rendering::BufferMemoryBarrier, 1> barriers{Rendering::BufferMemoryBarrier{
				AccessFlags::TransferWrite,
				AccessFlags::VertexRead,
				m_buffer,
				firstInstanceIndex * elementDataSize,
				elementDataSize * instanceCount
			}};

			graphicsCommandEncoder.RecordPipelineBarrier(PipelineStageFlags::Transfer, PipelineStageFlags::VertexInput, {}, barriers.GetView());
		}
	}

	void InstanceBuffer::MoveElement(
		const uint32 sourceInstanceIndex,
		const uint32 targetInstanceIndex,
		const CommandEncoderView graphicsCommandEncoder,
		const uint32 elementDataSize,
		PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
		Assert(sourceInstanceIndex != targetInstanceIndex);

		const BufferView buffer = m_buffer;

		{
			Array<Rendering::BufferMemoryBarrier, 2> barriers{
				Rendering::BufferMemoryBarrier{
					AccessFlags::VertexRead | AccessFlags::TransferWrite,
					AccessFlags::TransferRead,
					buffer,
					sourceInstanceIndex * elementDataSize,
					elementDataSize
				},
				Rendering::BufferMemoryBarrier{
					AccessFlags::VertexRead | AccessFlags::TransferWrite,
					AccessFlags::TransferWrite,
					buffer,
					targetInstanceIndex * elementDataSize,
					elementDataSize
				}
			};

			graphicsCommandEncoder.RecordPipelineBarrier(
				PipelineStageFlags::VertexInput | PipelineStageFlags::Transfer,
				PipelineStageFlags::Transfer,
				{},
				barriers.GetView()
			);
		}

		perFrameStagingBuffer.CopyToBuffer(
			graphicsCommandEncoder,
			sourceInstanceIndex * elementDataSize,
			buffer,
			targetInstanceIndex * elementDataSize,
			buffer,
			elementDataSize
		);

		{
			Array<Rendering::BufferMemoryBarrier, 2> barriers{
				Rendering::BufferMemoryBarrier{
					AccessFlags::TransferWrite,
					AccessFlags::VertexRead,
					buffer,
					targetInstanceIndex * elementDataSize,
					elementDataSize
				},
				Rendering::BufferMemoryBarrier{
					AccessFlags::TransferRead,
					AccessFlags::VertexRead,
					buffer,
					sourceInstanceIndex * elementDataSize,
					elementDataSize
				}
			};

			graphicsCommandEncoder.RecordPipelineBarrier(PipelineStageFlags::Transfer, PipelineStageFlags::VertexInput, {}, barriers.GetView());
		}
	}
}
