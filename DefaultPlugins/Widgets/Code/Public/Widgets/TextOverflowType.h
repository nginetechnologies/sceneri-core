#pragma once

namespace ngine::Widgets
{
	enum class TextOverflowType : uint8
	{
		//! Default value. The text is clipped and not accessible
		Clip,
		//! Render an ellipsis ("...") to represent the clipped text
		Ellipsis,
		//! Render the given string to represent the clipped text
		String
	};
}
