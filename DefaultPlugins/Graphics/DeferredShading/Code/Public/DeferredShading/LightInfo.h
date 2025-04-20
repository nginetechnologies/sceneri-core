#pragma once

#include "Features.h"
#include "LightTypes.h"

#include <Common/Math/Matrix4x4.h>

namespace ngine::Rendering
{
	inline static constexpr uint8 MaximumCascadeCount = 4;
	inline static constexpr uint8 MaximumShadowViewMatrixCount = Math::Max(MaximumCascadeCount, (uint8)6u);

	struct alignas(16) LightHeader
	{
		uint32 count;
	};

	struct PointLightInfo
	{
		Math::Vector4f positionAndRadius;
		Math::Vector4f colorAndInverseSquareRadius;
		int shadowMapIndex;
		Array<Math::Matrix4x4f, 6> viewMatrices;
	};

	struct SpotLightInfo
	{
		Math::Vector4f positionAndRadius;
		Math::Vector4f colorAndInverseSquareRadius;
		int shadowMapIndex;
		Math::Matrix4x4f viewMatrix;
	};

	struct DirectionalLightInfo
	{
#if !ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
		Math::Vector4f cascadeSplitDepths;
#endif
		Math::UnalignedVector3<float> direction;
		uint32 cascadeCount = 0;
		alignas(16) Math::UnalignedVector3<float> color;
		int32 shadowMapIndex = -1;
#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
		uint32 shadowMatrixIndex;
#else
		Array<Math::Matrix4x4f, MaximumCascadeCount> viewMatrices;
#endif
	};

	inline static constexpr Array<size, 3> LightHeaderSizes = {
		size(0),
		size(0),
#if RENDERER_WEBGPU
		sizeof(LightHeader)
#else
		size(0),
#endif
	};
	inline static constexpr Array<size, 3> LightStructSizes = {sizeof(PointLightInfo), sizeof(SpotLightInfo), sizeof(DirectionalLightInfo)};

	inline static constexpr bool SupportCubemapArrays = false;

	inline static constexpr uint16 MaximumPointLightCount = 32;
	inline static constexpr uint16 MaximumSpotLightCount = 2;
	inline static constexpr uint16 MaximumDirectionalLightCount = 1;
	inline static constexpr uint8 MaximumEnvironmentLightCount = SupportCubemapArrays ? 8 : 1;

	inline static constexpr size PointLightBufferSize = LightHeaderSizes[0] + LightStructSizes[0] * MaximumPointLightCount;
	inline static constexpr size SpotLightBufferSize = LightHeaderSizes[1] + LightStructSizes[1] * MaximumSpotLightCount;
	inline static constexpr size DirectionalLightBufferSize = LightHeaderSizes[2] + LightStructSizes[2] * MaximumDirectionalLightCount;

	inline static constexpr Array<uint16, (uint8)LightTypes::Count> MaximumLightCounts = {
		MaximumPointLightCount, MaximumSpotLightCount, MaximumDirectionalLightCount, MaximumEnvironmentLightCount
	};
}
