#pragma once

namespace ngine::Input
{
	enum class DeleteTextType : uint8
	{
		//! Deletes the character to the left
		LeftCharacter,
		//! Deletes the character to the right
		RightCharacter,
		//! Deletes the word to the left
		LeftWord,
		//! Deletes the word to the right
		RightWord,
		//! Deletes the full line to the left of the cursor
		FullLineToLeftOfCursor,
		//! Deletes the full line to the right of the cursor
		FullLineToRightOfCursor,
	};
}
