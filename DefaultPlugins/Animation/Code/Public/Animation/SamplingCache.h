#pragma once

#include "3rdparty/ozz/animation/runtime/sampling_job.h"

namespace ngine::Animation
{
	struct SamplingCache final : public ozz::animation::SamplingCache
	{
		using BaseType = ozz::animation::SamplingCache;
		using BaseType::BaseType;
		using BaseType::operator=;
	};
}
