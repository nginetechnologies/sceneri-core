#pragma once

#include <Renderer/Metal/Includes.h>
#include <Renderer/Format.h>

namespace ngine::Rendering
{
#if RENDERER_METAL
	[[nodiscard]] constexpr MTLAttributeFormat ConvertFormatToAttributeFormat(const Format format)
	{
		switch (format)
		{
			case Format::R8G8_UINT:
				return MTLAttributeFormatUChar2;
			case Format::R8G8B8_UINT:
				return MTLAttributeFormatUChar3;
			case Format::R8G8B8A8_UINT_PACK8:
				return MTLAttributeFormatUChar4;

			case Format::R8G8_SINT:
				return MTLAttributeFormatChar2;
			case Format::R8G8B8_SINT:
				return MTLAttributeFormatChar3;
			case Format::R8G8B8A8_SINT_PACK8:
				return MTLAttributeFormatChar4;

			case Format::R8G8_UNORM:
				return MTLAttributeFormatUChar2Normalized;
			case Format::R8G8B8_UNORM:
				return MTLAttributeFormatUChar3Normalized;
			case Format::R8G8B8A8_UNORM_PACK8:
				return MTLAttributeFormatUChar4Normalized;

			case Format::R8G8_SNORM:
				return MTLAttributeFormatChar2Normalized;
			case Format::R8G8B8_SNORM:
				return MTLAttributeFormatChar3Normalized;
			case Format::R8G8B8A8_SNORM_PACK8:
				return MTLAttributeFormatChar4Normalized;

			case Format::R16G16_UINT:
				return MTLAttributeFormatUShort2;
			case Format::R16G16B16_UINT:
				return MTLAttributeFormatUShort3;
			case Format::R16G16B16A16_UINT:
				return MTLAttributeFormatUShort4;

			case Format::R16G16_SINT:
				return MTLAttributeFormatShort2;
			case Format::R16G16B16_SINT:
				return MTLAttributeFormatShort3;
			case Format::R16G16B16A16_SINT:
				return MTLAttributeFormatShort4;

			case Format::R16G16_UNORM:
				return MTLAttributeFormatUShort2Normalized;
			case Format::R16G16B16_UNORM:
				return MTLAttributeFormatUShort3Normalized;
			case Format::R16G16B16A16_UNORM:
				return MTLAttributeFormatUShort4Normalized;

			case Format::R16G16_SNORM:
				return MTLAttributeFormatShort2Normalized;
			case Format::R16G16B16_SNORM:
				return MTLAttributeFormatShort3Normalized;
			case Format::R16G16B16A16_SNORM:
				return MTLAttributeFormatShort4Normalized;

			case Format::R16G16_SFLOAT:
				return MTLAttributeFormatHalf2;
			case Format::R16G16B16_SFLOAT:
				return MTLAttributeFormatHalf3;
			case Format::R16G16B16A16_SFLOAT:
				return MTLAttributeFormatHalf4;

			case Format::R32_SFLOAT:
				return MTLAttributeFormatFloat;
			case Format::R32G32_SFLOAT:
				return MTLAttributeFormatFloat2;
			case Format::R32G32B32_SFLOAT:
				return MTLAttributeFormatFloat3;
			case Format::R32G32B32A32_SFLOAT:
				return MTLAttributeFormatFloat4;

			case Format::R32_SINT:
				return MTLAttributeFormatInt;
			case Format::R32G32_SINT:
				return MTLAttributeFormatInt2;
			case Format::R32G32B32_SINT:
				return MTLAttributeFormatInt3;
			case Format::R32G32B32A32_SINT:
				return MTLAttributeFormatInt4;

			case Format::R32_UINT:
				return MTLAttributeFormatUInt;
			case Format::R32G32_UINT:
				return MTLAttributeFormatUInt2;
			case Format::R32G32B32_UINT:
				return MTLAttributeFormatUInt3;
			case Format::R32G32B32A32_UINT:
				return MTLAttributeFormatUInt4;

			case Format::R10G10B10A2_SNORM:
				return MTLAttributeFormatInt1010102Normalized;
			case Format::R10G10B10A2_UNORM:
				return MTLAttributeFormatUInt1010102Normalized;

			default:
				Assert(false, "Unsupported vertex format!");
				return MTLAttributeFormatInvalid;
		}
	}
#endif
}
