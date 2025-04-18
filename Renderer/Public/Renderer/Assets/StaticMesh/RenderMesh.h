#pragma once

#include "RenderMeshView.h"

#include <Renderer/Buffers/VertexBuffer.h>
#include <Renderer/Buffers/IndexBuffer.h>

#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>

namespace ngine
{
	struct Engine;
}

namespace ngine::IO
{
	struct LoadingThreadOnly;
}

namespace ngine::Asset
{
	struct Guid;
}

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct LogicalDevice;
	struct RenderMeshView;
	struct CommandEncoderView;
	struct StagingBuffer;

	struct RenderMesh
	{
		enum DummyType
		{
			Dummy
		};

		RenderMesh() = default;
		RenderMesh(
			const DummyType,
			LogicalDevice& logicalDevice,
			const CommandEncoderView transferCommandBuffer,
			const CommandEncoderView graphicsCommandEncoder,
			StagingBuffer& stagingBufferOut
		);
		RenderMesh(
			LogicalDevice& logicalDevice,
			const CommandEncoderView transferCommandBuffer,
			const CommandEncoderView graphicsCommandEncoder,
			const Index vertexCount,
			ConstByteView vertexData,
			ArrayView<const Index, Index> indices,
			StagingBuffer& stagingBufferOut,
			const bool allowCpuAccess = false
		);
		RenderMesh(const RenderMesh& other) = delete;
		RenderMesh& operator=(const RenderMesh&) = delete;
		RenderMesh(RenderMesh&& other) = default;
		RenderMesh& operator=(RenderMesh&& other) = default;

		void Destroy(LogicalDevice& logicalDevice);

		[[nodiscard]] Index GetVertexCount() const
		{
			return m_vertexCount;
		}
		[[nodiscard]] Index GetIndexCount() const
		{
			return m_indexCount;
		}
		[[nodiscard]] Index GetTriangleCount() const
		{
			return m_indexCount / 3;
		}
		[[nodiscard]] VertexBuffer& GetVertexBuffer()
		{
			return m_vertexBuffer;
		}
		[[nodiscard]] IndexBuffer& GetIndexBuffer()
		{
			return m_indexBuffer;
		}
		[[nodiscard]] BufferView GetVertexBuffer() const
		{
			return m_vertexBuffer;
		}
		[[nodiscard]] BufferView GetIndexBuffer() const
		{
			return m_indexBuffer;
		}
		[[nodiscard]] bool IsValid() const
		{
			return m_vertexBuffer.IsValid();
		}

		[[nodiscard]] operator RenderMeshView() const
		{
			return RenderMeshView{m_vertexBuffer, m_indexBuffer, m_vertexCount, m_indexCount};
		}

		[[nodiscard]] size GetVertexBufferOffset() const
		{
			return m_vertexBuffer.GetOffset();
		}
		[[nodiscard]] size GetVertexBufferSize() const
		{
			return m_vertexBuffer.GetSize();
		}
		[[nodiscard]] size GetIndexBufferOffset() const
		{
			return m_indexBuffer.GetOffset();
		}
		[[nodiscard]] size GetIndexBufferSize() const
		{
			return m_indexBuffer.GetSize();
		}
	protected:
		VertexBuffer m_vertexBuffer;
		IndexBuffer m_indexBuffer;
		Index m_vertexCount;
		Index m_indexCount;
	};
}
