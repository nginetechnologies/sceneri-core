#pragma once

namespace ngine::Rendering
{
	enum class CursorType : uint8
	{
		Arrow,
		ResizeHorizontal,
		ResizeVertical,
		NotPermitted,
		Hand,
		TextEdit
	};
}
