#pragma once

namespace ngine::Widgets
{
	enum class WrapType : uint8
	{
		//! Always layout entries in the primary direction
		NoWrap,
		//! Wrap when we hit the limit of the primary direction
		Wrap,
		//! Wrap when we hit the limit of the primary direction, inverted sorting order
		WrapReverse
	};
}
