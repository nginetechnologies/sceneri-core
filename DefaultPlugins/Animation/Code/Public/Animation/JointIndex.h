#pragma once

#include <Common/Math/NumericLimits.h>

namespace ngine::Animation
{
	using JointIndex = uint16;
	inline static constexpr JointIndex InvalidJointIndex = Math::NumericLimits<JointIndex>::Max;
}
