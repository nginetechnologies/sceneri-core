#pragma once

namespace ngine::Widgets
{
	enum class WhiteSpaceType : uint8
	{
		//! Sequences of whitespace will collapse into a single whitespace. Text will wrap when necessary. This is default
		Normal,
		//! Sequences of whitespace will collapse into a single whitespace. Text will never wrap to the next line. The text continues on the
		//! same line
		NoWrap,

		// TODO: Implement these. Right now they are not supported.
		//! Whitespace is preserved. Text will only wrap on line breaks.
		Pre,
		//! Sequences of whitespace will collapse into a single whitespace.Text will wrap when necessary, and on line breaks
		PreLine,
		//! Whitespace is preserved. Text will wrap when necessary, and on line breaks
		PreWrap,
	};
}
