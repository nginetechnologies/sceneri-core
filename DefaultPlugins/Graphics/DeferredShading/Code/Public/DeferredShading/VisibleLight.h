#pragma once

#include "LightInfo.h"

namespace ngine::Rendering
{
	struct VisibleLight
	{
		int32 shadowMapIndex = -1;
#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
		uint32 directionalShadowMatrixIndex = 0;
#endif
		using CascadeIndexType = Memory::NumericSize<MaximumCascadeCount>;
		CascadeIndexType cascadeCount = 1;
		ReferenceWrapper<const Entity::LightSourceComponent> light;
		Array<Math::Matrix4x4f, MaximumShadowViewMatrixCount> shadowSampleViewProjectionMatrices;
		Array<float, MaximumCascadeCount> cascadeSplitDepths;
	};
}
