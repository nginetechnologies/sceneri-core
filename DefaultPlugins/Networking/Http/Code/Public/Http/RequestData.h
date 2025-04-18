#pragma once

#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/ByteView.h>
#include <Common/Memory/Variant.h>
#include <Common/IO/Path.h>

namespace ngine::Networking::HTTP
{
	struct FormDataPart
	{
		using StoredData = Vector<ByteType, size>;
		using Data = Variant<ConstByteView, StoredData>;

		String m_name;
		String m_fileName;
		String m_contentType;
		Data m_data;
	};

	struct FormData
	{
		using Part = FormDataPart;

		void EmplacePart(const ConstZeroTerminatedStringView name, const ConstZeroTerminatedStringView contentType, const ConstByteView data)
		{
			m_parts.EmplaceBack(Part{name, {}, contentType, data});
		}
		void EmplacePart(const ConstZeroTerminatedStringView name, const ConstZeroTerminatedStringView contentType, const ConstStringView data)
		{
			m_parts.EmplaceBack(Part{name, {}, contentType, (ConstByteView)data});
		}
		void EmplacePart(const ConstZeroTerminatedStringView name, const ConstZeroTerminatedStringView contentType, String&& data)
		{
			m_parts.EmplaceBack(Part{
				name,
				{},
				contentType,
				FormDataPart::StoredData{ArrayView<ByteType, uint32>{reinterpret_cast<ByteType*>(data.GetData()), data.GetDataSize()}}
			});
		}
		void EmplacePart(
			const ConstZeroTerminatedStringView name,
			const ConstZeroTerminatedStringView fileName,
			const ConstZeroTerminatedStringView contentType,
			const ConstByteView data
		)
		{
			m_parts.EmplaceBack(Part{name, fileName, contentType, data});
		}
		void EmplacePart(
			const ConstZeroTerminatedStringView name,
			const ConstZeroTerminatedStringView fileName,
			const ConstZeroTerminatedStringView contentType,
			FormDataPart::StoredData&& data
		)
		{
			m_parts.EmplaceBack(Part{name, fileName, contentType, Forward<FormDataPart::StoredData>(data)});
		}

		InlineVector<Part, 8> m_parts;
	};
	using RequestBody = String;
	using RequestData = Variant<RequestBody, FormData>;
}
