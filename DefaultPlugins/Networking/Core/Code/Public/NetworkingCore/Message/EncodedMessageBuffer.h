#pragma once

#include "MessageBuffer.h"

#include <Common/Memory/Containers/BitView.h>

namespace ngine::Network
{
	struct EncodedMessageBuffer
	{
		EncodedMessageBuffer() = default;
		EncodedMessageBuffer(MessageBuffer&& messageBuffer, const BitView unusedBitView)
			: m_buffer(Forward<MessageBuffer>(messageBuffer))
			, m_usedByteCount((uint32)Math::Ceil((float)(m_buffer.GetDataSize() * 8 - unusedBitView.GetCount())))
		{
			Assert(m_usedByteCount > 0);
		}
		explicit EncodedMessageBuffer(const ByteView data)
			: m_buffer{data}
			, m_usedByteCount(data.GetDataSize())
		{
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_buffer.IsValid();
		}

		[[nodiscard]] size GetUsedDataSize() const
		{
			return m_usedByteCount;
		}

		[[nodiscard]] ByteView ReleaseOwnership()
		{
			ByteView byteView{m_buffer.ReleaseOwnership()};
			return byteView.GetSubView(size(0), m_usedByteCount);
		}
		[[nodiscard]] ByteView GetView()
		{
			ByteView byteView{m_buffer.GetView()};
			return byteView.GetSubView(size(0), m_usedByteCount);
		}
	protected:
		MessageBuffer m_buffer;
		size m_usedByteCount{0};
	};
}
