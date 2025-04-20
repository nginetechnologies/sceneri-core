#pragma once

namespace ngine::Widgets
{
	enum class ElementSizingType : uint8
	{
		//! Border and padding should be applied on top of preferred width & height
		ContentBox,
		//! Preferred width & height should subtract the border and padding
		BorderBox,
		Default = ContentBox
	};
}
