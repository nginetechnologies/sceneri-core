#pragma once

#include <Renderer/WebGPU/Includes.h>
#include <Renderer/Wrappers/ImageMappingType.h>

namespace ngine::Rendering
{
#if RENDERER_WEBGPU
	[[nodiscard]] inline constexpr WGPUTextureViewDimension ConvertImageMappingType(const ImageMappingType type)
	{
		switch (type)
		{
			case ImageMappingType::OneDimensional:
				return WGPUTextureViewDimension_1D;
			case ImageMappingType::TwoDimensional:
				return WGPUTextureViewDimension_2D;
			case ImageMappingType::ThreeDimensional:
				return WGPUTextureViewDimension_3D;
			case ImageMappingType::Cube:
				return WGPUTextureViewDimension_Cube;
			case ImageMappingType::OneDimensionalArray:
				Assert(false, "Not supported");
				return WGPUTextureViewDimension_Undefined;
			case ImageMappingType::TwoDimensionalArray:
				return WGPUTextureViewDimension_2DArray;
			case ImageMappingType::CubeArray:
				return WGPUTextureViewDimension_CubeArray;
		}
		ExpectUnreachable();
	}
#endif
}
