#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Input
{
	enum class MoveTextCursorFlags : uint16
	{
		//! Move the cursor left by one character
		Left = 1 << 0,
		//! Move the cursor right by one character
		Right = 1 << 1,
		//! Move the cursor up by one line
		Up = 1 << 2,
		//! Move the cursor down by one line
		Down = 1 << 3,
		// Move the cursor to the beginning of the previous word
		PreviousWord = 1 << 4,
		// Move the cursor to the beginning of the next word
		NextWord = 1 << 5,
		// Move the cursor to the beginning of the current line
		BeginningOfCurrentLine = 1 << 6,
		// Move the cursor to the end of the current line
		EndOfCurrentLine = 1 << 7,
		// Move the cursor to the beginning of the current paragraph
		BeginningOfCurrentParagraph = 1 << 8,
		// Move the cursor to the end of the current paragraph
		EndOfCurrentParagraph = 1 << 9,
		//! Moves the cursor to the beginning of the document
		BeginningOfDocument = 1 << 10,
		//! Moves the cursor to the end of the document
		EndOfDocument = 1 << 11,
		//! Applies the movement flags to the selection
		ApplyToSelection = 1 << 12,
		//! Apply to the full text (i.e. ctrl / cmd + a + ApplySelection to select all)
		All = 1 << 13
	};
	ENUM_FLAG_OPERATORS(MoveTextCursorFlags);
}
