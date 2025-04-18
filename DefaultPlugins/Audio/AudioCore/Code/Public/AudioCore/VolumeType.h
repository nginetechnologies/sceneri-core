#pragma once

#include <Common/Math/Ratio.h>

namespace ngine::Audio
{
	// Represents the volume of the played audio (loudness) in a procent range.
	// A float from 0.0f - 1.0f where 0.0 is no volume and 1.0f is the maximum.
	struct Volume : public Math::Ratiof
	{
		inline static constexpr Guid TypeGuid = "1fbf28af-5914-47fb-b3c7-5620523ea217"_guid;

		using BaseType = Math::Ratiof;
		using BaseType::BaseType;
		Volume(const BaseType value)
			: BaseType(value)
		{
		}
	};
}
