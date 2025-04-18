#pragma once

#include "3rdparty/ozz/base/io/stream.h"

#include <Common/Memory/Containers/ByteView.h>

namespace ngine::Animation
{
	struct DataStream final : public ozz::io::Stream
	{
		DataStream(const ByteView data)
			: m_data(data)
		{
		}

		virtual bool opened() const override
		{
			return true;
		}

		virtual size_t Read(void* _buffer, size_t _size) override
		{
			if (m_position + _size > m_data.GetDataSize())
			{
				return 0;
			}

			const bool wasRead =
				m_data.GetSubView(m_position, _size).ReadIntoView(ArrayView<ByteType, size>{reinterpret_cast<ByteType*>(_buffer), _size});
			m_position += _size;
			return wasRead ? _size : 0;
		}

		virtual size_t Write(const void* _buffer, size_t _size) override
		{
			if (m_position + _size > m_data.GetDataSize())
			{
				return 0;
			}

			const ConstByteView sourceView{reinterpret_cast<const ByteType*>(_buffer), _size};
			const ByteView targetView = m_data.GetSubView(m_position, _size);
			const bool wasWritten = sourceView.ReadIntoView(ArrayView<ByteType, size>{targetView.GetData(), targetView.GetDataSize()});
			m_position += _size;
			return wasWritten ? _size : 0;
		}

		virtual int Seek(int _offset, Origin _origin) override
		{
			switch (_origin)
			{
				case kCurrent:
					Assert(m_position + _offset <= m_data.GetDataSize());
					m_position += _offset;
					break;
				case kEnd:
					Assert(m_data.GetDataSize() + _offset <= m_data.GetDataSize());
					m_position = m_data.GetDataSize() + _offset;
					break;
				case kSet:
					Assert(_offset >= 0);
					Assert((size)_offset <= m_data.GetDataSize());
					m_position = _offset;
					break;
				default:
					ExpectUnreachable();
			}
			return 1;
		}

		virtual int Tell() const override
		{
			return (int)m_position;
		}

		virtual size_t Size() const override
		{
			return m_data.GetDataSize();
		}

		const ByteView m_data;
		size m_position = 0;
	};
}
