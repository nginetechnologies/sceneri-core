#pragma once

#include <Common/Math/CoreNumericTypes.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::Rendering
{
	enum class KeyboardTypeFlags : uint16
	{
		//! Generic text entry
		Text = 1 << 0,
		//! Support characters for entering URLs
		URL = 1 << 1,
		//! Support numeric entry
		Number = 1 << 2,
		//! Support decimal point entry
		Decimal = 1 << 3,
		//! Intended for entering phone numbers
		PhoneNumber = 1 << 4,
		//! Support entering email characters ('.', '+', '@'..)
		Email = 1 << 5,
		//! Intended for searching online
		WebSearch = 1 << 6,
		//! Allow negative numbers
		Signed = 1 << 7,
		//! Support entering all of ascii characters
		ASCII = 1 << 8,
		//! For entering individuals names
		Name = 1 << 9,
		//! For entering PIN numbers
		PIN = 1 << 10,
	};
	ENUM_FLAG_OPERATORS(KeyboardTypeFlags);
}
