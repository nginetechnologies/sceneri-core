#pragma once

#include <Common/Memory/Containers/ByteView.h>
#include <Common/Memory/Allocators/Allocate.h>

namespace ngine::Network
{
	struct MessageBuffer
	{
		MessageBuffer() = default;
		MessageBuffer(const uint32 dataSize)
			: m_data(reinterpret_cast<ByteType*>(Memory::Allocate(dataSize)), dataSize)
		{
		}
		explicit MessageBuffer(const ByteView data)
			: m_data{data}
		{
		}
		MessageBuffer(MessageBuffer&& other)
			: m_data(other.m_data)
		{
			other.m_data = {};
		}
		MessageBuffer& operator=(MessageBuffer&& other)
		{
			m_data = other.m_data;
			other.m_data = {};
			return *this;
		}
		MessageBuffer(const MessageBuffer& other)
			: m_data(reinterpret_cast<ByteType*>(Memory::Allocate(other.GetDataSize())), other.GetDataSize())
		{
			m_data.CopyFrom(other.m_data);
		}
		MessageBuffer& operator=(const MessageBuffer& other)
		{
			m_data = ByteView{reinterpret_cast<ByteType*>(Memory::Allocate(other.GetDataSize())), other.GetDataSize()};
			m_data.CopyFrom(other.m_data);
			return *this;
		}
		~MessageBuffer()
		{
			if (m_data.GetData() != nullptr)
			{
				Memory::Deallocate(m_data.GetData());
				m_data = {};
			}
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_data.HasElements();
		}

		[[nodiscard]] size GetDataSize() const
		{
			return m_data.GetDataSize();
		}

		[[nodiscard]] ConstByteView GetView() const
		{
			return m_data;
		}
		[[nodiscard]] ByteView GetView()
		{
			return m_data;
		}

		[[nodiscard]] ByteView ReleaseOwnership()
		{
			ByteView data{m_data};
			m_data = {};
			return data;
		}
	protected:
		ByteView m_data;
	};
}
