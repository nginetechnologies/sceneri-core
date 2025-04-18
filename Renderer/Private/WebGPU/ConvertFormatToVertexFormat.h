#pragma once

#include <Renderer/WebGPU/Includes.h>
#include <Renderer/Format.h>

namespace ngine::Rendering
{
#if RENDERER_WEBGPU
	[[nodiscard]] constexpr WGPUVertexFormat ConvertFormatToVertexFormat(const Format format)
	{
		switch (format)
		{
			case Format::R8G8_UINT:
				return WGPUVertexFormat_Uint8x2;
			case Format::R8G8B8A8_UINT_PACK8:
				return WGPUVertexFormat_Uint8x4;

			case Format::R8G8_SINT:
				return WGPUVertexFormat_Sint8x2;
			case Format::R8G8B8A8_SINT_PACK8:
				return WGPUVertexFormat_Sint8x4;

			case Format::R8G8_UNORM:
				return WGPUVertexFormat_Unorm8x2;
			case Format::R8G8B8A8_UNORM_PACK8:
				return WGPUVertexFormat_Unorm8x4;

			case Format::R8G8_SNORM:
				return WGPUVertexFormat_Snorm8x2;
			case Format::R8G8B8A8_SNORM_PACK8:
				return WGPUVertexFormat_Snorm8x4;

			case Format::R16G16_UINT:
				return WGPUVertexFormat_Uint16x2;
			case Format::R16G16B16A16_UINT:
				return WGPUVertexFormat_Uint16x4;

			case Format::R16G16_SINT:
				return WGPUVertexFormat_Sint16x2;
			case Format::R16G16B16A16_SINT:
				return WGPUVertexFormat_Sint16x4;

			case Format::R16G16_UNORM:
				return WGPUVertexFormat_Unorm16x2;
			case Format::R16G16B16A16_UNORM:
				return WGPUVertexFormat_Unorm16x4;

			case Format::R16G16_SNORM:
				return WGPUVertexFormat_Snorm16x2;
			case Format::R16G16B16A16_SNORM:
				return WGPUVertexFormat_Snorm16x4;

			case Format::R16G16_SFLOAT:
				return WGPUVertexFormat_Float16x2;
			case Format::R16G16B16A16_SFLOAT:
				return WGPUVertexFormat_Float16x4;

			case Format::R32_SFLOAT:
				return WGPUVertexFormat_Float32;
			case Format::R32G32_SFLOAT:
				return WGPUVertexFormat_Float32x2;
			case Format::R32G32B32_SFLOAT:
				return WGPUVertexFormat_Float32x3;
			case Format::R32G32B32A32_SFLOAT:
				return WGPUVertexFormat_Float32x4;

			case Format::R32_UINT:
				return WGPUVertexFormat_Uint32;
			case Format::R32G32_UINT:
				return WGPUVertexFormat_Uint32x2;
			case Format::R32G32B32_UINT:
				return WGPUVertexFormat_Uint32x3;
			case Format::R32G32B32A32_UINT:
				return WGPUVertexFormat_Uint32x4;

			case Format::R32_SINT:
				return WGPUVertexFormat_Sint32;
			case Format::R32G32_SINT:
				return WGPUVertexFormat_Sint32x2;
			case Format::R32G32B32_SINT:
				return WGPUVertexFormat_Sint32x3;
			case Format::R32G32B32A32_SINT:
				return WGPUVertexFormat_Sint32x4;

			default:
				Assert(false, "Unsupported vertex format!");
#if RENDERER_WEBGPU_DAWN
				return WGPUVertexFormat_Force32;
#else
				return WGPUVertexFormat_Undefined;
#endif
		}
	}
#endif
}
