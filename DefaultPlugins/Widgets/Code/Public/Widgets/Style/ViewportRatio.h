#pragma once

#include <Common/Math/Ratio.h>

namespace ngine::Widgets
{
	struct ViewportWidthRatio : public Math::Ratiof
	{
		using BaseType = Math::Ratiof;
		using BaseType::BaseType;
		using BaseType::operator=;
	};
	struct ViewportHeightRatio : public Math::Ratiof
	{
		using BaseType = Math::Ratiof;
		using BaseType::BaseType;
		using BaseType::operator=;
	};
	struct ViewportMinimumRatio : public Math::Ratiof
	{
		using BaseType = Math::Ratiof;
		using BaseType::BaseType;
		using BaseType::operator=;
	};
	struct ViewportMaximumRatio : public Math::Ratiof
	{
		using BaseType = Math::Ratiof;
		using BaseType::BaseType;
		using BaseType::operator=;
	};
}
