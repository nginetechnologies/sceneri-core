#pragma once

#include <Common/Guid.h>

namespace ngine::Rendering
{
	struct StageGuid : public ngine::Guid
	{
		using BaseType = ngine::Guid;
		using BaseType::BaseType;
		StageGuid(const Guid guid)
			: Guid(guid)
		{
		}
	};
}
