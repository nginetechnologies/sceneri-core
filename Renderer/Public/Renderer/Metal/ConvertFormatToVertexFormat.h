#pragma once

#include <Renderer/Metal/Includes.h>
#include <Renderer/Format.h>

namespace ngine::Rendering
{
#if RENDERER_METAL
	[[nodiscard]] constexpr MTLVertexFormat ConvertFormatToVertexFormat(const Format format)
	{
		switch (format)
		{
			case Format::R8G8_UINT:
				return MTLVertexFormatUChar2;
			case Format::R8G8B8_UINT:
				return MTLVertexFormatUChar3;
			case Format::R8G8B8A8_UINT_PACK8:
				return MTLVertexFormatUChar4;

			case Format::R8G8_SINT:
				return MTLVertexFormatChar2;
			case Format::R8G8B8_SINT:
				return MTLVertexFormatChar3;
			case Format::R8G8B8A8_SINT_PACK8:
				return MTLVertexFormatChar4;

			case Format::R8G8_UNORM:
				return MTLVertexFormatUChar2Normalized;
			case Format::R8G8B8_UNORM:
				return MTLVertexFormatUChar3Normalized;
			case Format::R8G8B8A8_UNORM_PACK8:
				return MTLVertexFormatUChar4Normalized;

			case Format::R8G8_SNORM:
				return MTLVertexFormatChar2Normalized;
			case Format::R8G8B8_SNORM:
				return MTLVertexFormatChar3Normalized;
			case Format::R8G8B8A8_SNORM_PACK8:
				return MTLVertexFormatChar4Normalized;

			case Format::R16G16_UINT:
				return MTLVertexFormatUShort2;
			case Format::R16G16B16_UINT:
				return MTLVertexFormatUShort3;
			case Format::R16G16B16A16_UINT:
				return MTLVertexFormatUShort4;

			case Format::R16G16_SINT:
				return MTLVertexFormatShort2;
			case Format::R16G16B16_SINT:
				return MTLVertexFormatShort3;
			case Format::R16G16B16A16_SINT:
				return MTLVertexFormatShort4;

			case Format::R16G16_UNORM:
				return MTLVertexFormatUShort2Normalized;
			case Format::R16G16B16_UNORM:
				return MTLVertexFormatUShort3Normalized;
			case Format::R16G16B16A16_UNORM:
				return MTLVertexFormatUShort4Normalized;

			case Format::R16G16_SNORM:
				return MTLVertexFormatShort2Normalized;
			case Format::R16G16B16_SNORM:
				return MTLVertexFormatShort3Normalized;
			case Format::R16G16B16A16_SNORM:
				return MTLVertexFormatShort4Normalized;

			case Format::R16G16_SFLOAT:
				return MTLVertexFormatHalf2;
			case Format::R16G16B16_SFLOAT:
				return MTLVertexFormatHalf3;
			case Format::R16G16B16A16_SFLOAT:
				return MTLVertexFormatHalf4;

			case Format::R32_SFLOAT:
				return MTLVertexFormatFloat;
			case Format::R32G32_SFLOAT:
				return MTLVertexFormatFloat2;
			case Format::R32G32B32_SFLOAT:
				return MTLVertexFormatFloat3;
			case Format::R32G32B32A32_SFLOAT:
				return MTLVertexFormatFloat4;

			case Format::R32_SINT:
				return MTLVertexFormatInt;
			case Format::R32G32_SINT:
				return MTLVertexFormatInt2;
			case Format::R32G32B32_SINT:
				return MTLVertexFormatInt3;
			case Format::R32G32B32A32_SINT:
				return MTLVertexFormatInt4;

			case Format::R32_UINT:
				return MTLVertexFormatUInt;
			case Format::R32G32_UINT:
				return MTLVertexFormatUInt2;
			case Format::R32G32B32_UINT:
				return MTLVertexFormatUInt3;
			case Format::R32G32B32A32_UINT:
				return MTLVertexFormatUInt4;

			case Format::R10G10B10A2_SNORM:
				return MTLVertexFormatInt1010102Normalized;
			case Format::R10G10B10A2_UNORM:
				return MTLVertexFormatUInt1010102Normalized;

			default:
				Assert(false, "Unsupported vertex format!");
				return MTLVertexFormatInvalid;
		}
	}
#endif
}
