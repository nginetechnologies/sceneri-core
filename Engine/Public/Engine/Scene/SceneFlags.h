#pragma once

#include <Common/Math/CoreNumericTypes.h>
#include <Common/EnumFlagOperators.h>

namespace ngine
{
	enum class SceneFlags : uint8
	{
		IsDisabled = 1 << 0,
		IsTemplate = 1 << 1,
		IsModifyingFrameGraph = 1 << 2,
		IsEditing = 1 << 3
	};

	ENUM_FLAG_OPERATORS(SceneFlags);
}
