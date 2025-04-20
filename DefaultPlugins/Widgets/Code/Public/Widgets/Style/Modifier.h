#pragma once

#include <Common/Math/Log2.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::Widgets::Style
{
	enum class Modifier : uint16
	{
		None = 0,
		//! Set when an element is being pressed
		Active = 1 << 0,
		Hover = 1 << 1,
		Disabled = 1 << 2,
		Focused = 1 << 3,
		//! Whether the element was toggled off (or on if not set)
		//! Used for checkboxes and collapsible sections
		ToggledOff = 1 << 4,
		//! Whether the element's contents passed validation, i.e. within required range
		Valid = 1 << 5,
		//! Whether the element's contents failed validation, i.e. outside of required range
		Invalid = 1 << 6,
		//! Whether the element's contents are required
		Required = 1 << 7,
		//! Whether the element's contents are required
		Optional = 1 << 8,
		Count = Math::Log2(static_cast<uint32>(Optional)) + 1
	};
	ENUM_FLAG_OPERATORS(Modifier);
}
