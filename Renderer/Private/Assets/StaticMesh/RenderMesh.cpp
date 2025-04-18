#include "Assets/StaticMesh/RenderMesh.h"

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Buffers/DataToBufferBatch.h>
#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Wrappers/BufferMemoryBarrier.h>
#include <Renderer/WebGPU/Includes.h>

#include <Renderer/Assets/StaticMesh/VertexNormals.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexTextureCoordinate.h>

#include <Engine/Threading/JobRunnerThread.h>

#include <Common/Math/Vector2.h>
#include <Common/Math/Tangents.h>

namespace ngine::Rendering
{
	struct DummyVertexData
	{
		Array<VertexPosition, 4> vertexPositions{
			VertexPosition{-0.5f, -0.5f, 0.f}, VertexPosition{0.5f, -0.5f, 0.f}, VertexPosition{0.5f, 0.5f, 0.f}, VertexPosition{-0.5f, 0.5f, 0.f}
		};

		Array<VertexNormals, 4> vertexNormals{
			VertexNormals{Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
			VertexNormals{Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
			VertexNormals{Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
			VertexNormals{Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Right, 1.f).m_tangent}
		};

		Array<VertexTextureCoordinate, 4> textureCoordinates{
			Math::Vector2f{1.0f, 0.0f}, Math::Vector2f{0.0f, 0.0f}, Math::Vector2f{0.0f, 1.0f}, Math::Vector2f{1.0f, 1.0f}
		};
	};
	static DummyVertexData dummyVertexData;

	static Array<Index, 6> dummyIndices = {0u, 1u, 2u, 2u, 3u, 0u};

	RenderMesh::RenderMesh(
		const DummyType,
		LogicalDevice& logicalDevice,
		const CommandEncoderView transferCommandEncoder,
		const CommandEncoderView graphicsCommandEncoder,
		StagingBuffer& stagingBufferOut
	)
		: RenderMesh(
				logicalDevice,
				transferCommandEncoder,
				graphicsCommandEncoder,
				dummyVertexData.vertexPositions.GetSize(),
				ConstByteView::Make(dummyVertexData),
				dummyIndices.GetDynamicView(),
				stagingBufferOut
			)
	{
	}

	RenderMesh::RenderMesh(
		LogicalDevice& logicalDevice,
		const CommandEncoderView transferCommandEncoder,
		const CommandEncoderView graphicsCommandEncoder,
		const Index vertexCount,
		ConstByteView vertexData,
		ArrayView<const Index, Index> indices,
		[[maybe_unused]] StagingBuffer& stagingBufferOut,
		const bool allowCpuAccess
	)
		: m_vertexBuffer(
				logicalDevice, logicalDevice.GetPhysicalDevice(), logicalDevice.GetDeviceMemoryPool(), vertexData.GetDataSize(), {}, allowCpuAccess
			)
		, m_indexBuffer(logicalDevice, logicalDevice.GetPhysicalDevice(), logicalDevice.GetDeviceMemoryPool(), indices.GetDataSize())
		, m_vertexCount(vertexCount)
		, m_indexCount(indices.GetSize())
	{
		if (LIKELY(m_vertexBuffer.IsValid() & m_indexBuffer.IsValid()))
		{
			BlitCommandEncoder blitCommandEncoder = transferCommandEncoder.BeginBlit();
			Optional<StagingBuffer> stagingBuffer;
			blitCommandEncoder.RecordCopyDataToBuffer(
				logicalDevice,
				QueueFamily::Graphics,
				Array<const DataToBufferBatch, 2>{
					DataToBufferBatch{m_vertexBuffer, Array<const DataToBuffer, 1>{DataToBuffer{0, vertexData}}},
					DataToBufferBatch{m_indexBuffer, Array<const DataToBuffer, 1>{DataToBuffer{0, ConstByteView(indices)}}}
				},
				stagingBuffer
			);
			if (stagingBuffer.IsValid())
			{
				stagingBufferOut = Move(*stagingBuffer);
			}

			const bool isUnifiedGraphicsAndTransferQueue = logicalDevice.GetCommandQueue(QueueFamily::Graphics) ==
			                                               logicalDevice.GetCommandQueue(QueueFamily::Transfer);
			if (isUnifiedGraphicsAndTransferQueue)
			{
				const Array<Rendering::BufferMemoryBarrier, 2> barriers{
					Rendering::BufferMemoryBarrier{AccessFlags::TransferWrite, AccessFlags::VertexRead, m_vertexBuffer, 0, vertexData.GetDataSize()},
					Rendering::BufferMemoryBarrier{AccessFlags::TransferWrite, AccessFlags::VertexRead, m_indexBuffer, 0, indices.GetDataSize()}
				};

				transferCommandEncoder.RecordPipelineBarrier(PipelineStageFlags::Transfer, PipelineStageFlags::VertexInput, {}, barriers.GetView());
			}
			else
			{
				{
					const Array<Rendering::BufferMemoryBarrier, 2> barriers{
						Rendering::BufferMemoryBarrier{
							AccessFlags::TransferWrite,
							AccessFlags(),
							logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Transfer),
							logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics),
							m_vertexBuffer,
							0,
							vertexData.GetDataSize()
						},
						Rendering::BufferMemoryBarrier{
							AccessFlags::TransferWrite,
							AccessFlags(),
							logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Transfer),
							logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics),
							m_indexBuffer,
							0,
							indices.GetDataSize(),
						}
					};

					transferCommandEncoder
						.RecordPipelineBarrier(PipelineStageFlags::Transfer, PipelineStageFlags::BottomOfPipe, {}, barriers.GetView());
				}

				{
					const Array<Rendering::BufferMemoryBarrier, 2> barriers{
						Rendering::BufferMemoryBarrier{
							AccessFlags(),
							AccessFlags::VertexRead,
							logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Transfer),
							logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics),
							m_vertexBuffer,
							0,
							vertexData.GetDataSize()
						},
						Rendering::BufferMemoryBarrier{
							AccessFlags(),
							AccessFlags::VertexRead,
							logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Transfer),
							logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics),
							m_indexBuffer,
							0,
							indices.GetDataSize(),
						}
					};

					graphicsCommandEncoder
						.RecordPipelineBarrier(PipelineStageFlags::TopOfPipe, PipelineStageFlags::VertexInput, {}, barriers.GetView());
				}
			}
		}
	}

	void RenderMesh::Destroy(LogicalDevice& logicalDevice)
	{
		Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
		thread.GetRenderData()
			.DestroyBuffers(logicalDevice.GetIdentifier(), Array<Buffer, 2>{Move(m_vertexBuffer), Move(m_indexBuffer)}.GetDynamicView());
	}
}
