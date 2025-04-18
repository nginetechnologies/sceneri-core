#pragma once

#include <Renderer/Buffers/BufferView.h>
#include <Renderer/Index.h>
#include <Common/Platform/TrivialABI.h>

namespace ngine::Rendering
{
	struct TRIVIAL_ABI RenderMeshView
	{
		RenderMeshView() = default;
		RenderMeshView(const BufferView vertexBuffer, const BufferView indexBuffer, const Index vertexCount, const Index indexCount)
			: m_vertexBuffer(vertexBuffer)
			, m_indexBuffer(indexBuffer)
			, m_vertexCount(vertexCount)
			, m_indexCount(indexCount)
		{
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_indexCount != 0;
		}

		[[nodiscard]] BufferView GetVertexBuffer() const
		{
			return m_vertexBuffer;
		}
		[[nodiscard]] BufferView GetIndexBuffer() const
		{
			return m_indexBuffer;
		}
		[[nodiscard]] Index GetIndexCount() const
		{
			return m_indexCount;
		}

		[[nodiscard]] Index GetVertexCount() const
		{
			return m_vertexCount;
		}
		[[nodiscard]] Index GetTriangleCount() const
		{
			return m_indexCount / 3;
		}
	protected:
		BufferView m_vertexBuffer;
		BufferView m_indexBuffer;
		Index m_vertexCount = 0;
		Index m_indexCount = 0;
	};
}
