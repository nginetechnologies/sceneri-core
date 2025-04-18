#pragma once

#include "3rdparty/ozz/base/io/stream.h"

#include <Common/IO/FileView.h>
#include <Common/Math/CoreNumericTypes.h>

namespace ngine::Animation
{
	struct FileStream final : public ozz::io::Stream
	{
		FileStream(IO::FileView file)
			: m_file(file)
		{
		}

		virtual bool opened() const override
		{
			return m_file.IsValid();
		}

		virtual size_t Read(void* _buffer, size_t _size) override
		{
			return m_file.Read(reinterpret_cast<ByteType*>(_buffer), _size) ? _size : 0;
		}

		virtual size_t Write(const void* _buffer, size_t _size) override
		{
			return m_file.Write(ArrayView<const ByteType, size>{reinterpret_cast<const ByteType*>(_buffer), _size}) ? _size : 0;
		}

		virtual int Seek(int _offset, Origin _origin) override
		{
			IO::SeekOrigin origin;
			switch (_origin)
			{
				case kCurrent:
					origin = IO::SeekOrigin::CurrentPosition;
					break;
				case kEnd:
					origin = IO::SeekOrigin::EndOfFile;
					break;
				case kSet:
					origin = IO::SeekOrigin::StartOfFile;
					break;
				default:
					ExpectUnreachable();
			}
			return m_file.Seek(_offset, origin) ? 0 : 1;
		}

		virtual int Tell() const override
		{
			return static_cast<int>(m_file.Tell());
		}

		virtual size_t Size() const override
		{
			return (size)m_file.GetSize();
		}

		IO::FileView m_file;
	};
}
