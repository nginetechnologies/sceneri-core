#pragma once

#include <Common/Memory/Containers/ZeroTerminatedStringView.h>

namespace ngine::Networking::HTTP
{
	enum class RequestType
	{
		Get,
		Post,
		Put,
		Patch,
		Delete,
		Headers
	};

	[[nodiscard]] static constexpr ConstZeroTerminatedStringView GetRequestTypeString(RequestType type)
	{
		switch (type)
		{
			case RequestType::Get:
				return "GET";
			case RequestType::Post:
				return "POST";
			case RequestType::Put:
				return "PUT";
			case RequestType::Patch:
				return "PATCH";
			case RequestType::Delete:
				return "DELETE";
			case RequestType::Headers:
				return "HEADERS";
		}

		ExpectUnreachable();
	}
}
