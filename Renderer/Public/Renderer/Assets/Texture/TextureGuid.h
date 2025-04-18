#pragma once

#include <Common/Guid.h>

namespace ngine::Rendering
{
	struct TextureGuid : public ngine::Guid
	{
		using BaseType = ngine::Guid;
		using BaseType::BaseType;
		TextureGuid(const Guid guid)
			: Guid(guid)
		{
		}
	};
}
