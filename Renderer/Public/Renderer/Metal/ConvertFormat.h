#pragma once

#include <Renderer/Metal/Includes.h>
#include <Renderer/Format.h>

namespace ngine::Rendering
{
#if RENDERER_METAL
	[[nodiscard]] inline constexpr MTLPixelFormat ConvertFormat(const Format format)
	{
		switch (format)
		{
			case Rendering::Format::A8_UNORM:
				return MTLPixelFormatA8Unorm;

			case Rendering::Format::R8_UNORM:
				return MTLPixelFormatR8Unorm;
			case Rendering::Format::R8_SNORM:
				return MTLPixelFormatR8Snorm;
			case Rendering::Format::R8_UINT:
				return MTLPixelFormatR8Uint;
			case Rendering::Format::R8_SINT:
				return MTLPixelFormatR8Sint;
			case Rendering::Format::R8_SRGB:
				return MTLPixelFormatR8Unorm_sRGB;

			case Rendering::Format::R16_UNORM:
				return MTLPixelFormatR16Unorm;
			case Rendering::Format::R16_SNORM:
				return MTLPixelFormatR16Snorm;
			case Rendering::Format::R16_UINT:
				return MTLPixelFormatR16Uint;
			case Rendering::Format::R16_SINT:
				return MTLPixelFormatR16Sint;
			case Rendering::Format::R16_SFLOAT:
				return MTLPixelFormatR16Float;

			case Rendering::Format::R8G8_UNORM:
				return MTLPixelFormatRG8Unorm;
			case Rendering::Format::R8G8_SNORM:
				return MTLPixelFormatRG8Snorm;
			case Rendering::Format::R8G8_UINT:
				return MTLPixelFormatRG8Uint;
			case Rendering::Format::R8G8_SINT:
				return MTLPixelFormatRG8Sint;
			case Rendering::Format::R8G8_SRGB:
				return MTLPixelFormatRG8Unorm_sRGB;

			case Rendering::Format::B5G6R5_UNORM:
				return MTLPixelFormatB5G6R5Unorm;
			case Rendering::Format::A1B5G5R5_UNORM_PACK16:
				return MTLPixelFormatA1BGR5Unorm;
			case Rendering::Format::A4R4G4B4_UNORM_PACK16:
				return MTLPixelFormatABGR4Unorm;
			case Rendering::Format::B5G5R5A1_UNORM:
				return MTLPixelFormatBGR5A1Unorm;

			case Rendering::Format::R32_UINT:
				return MTLPixelFormatR32Uint;
			case Rendering::Format::R32_SINT:
				return MTLPixelFormatR32Sint;
			case Rendering::Format::R32_SFLOAT:
				return MTLPixelFormatR32Float;

			case Rendering::Format::R16G16_UNORM:
				return MTLPixelFormatRG16Unorm;
			case Rendering::Format::R16G16_SNORM:
				return MTLPixelFormatRG16Snorm;
			case Rendering::Format::R16G16_UINT:
				return MTLPixelFormatRG16Uint;
			case Rendering::Format::R16G16_SINT:
				return MTLPixelFormatRG16Sint;
			case Rendering::Format::R16G16_SFLOAT:
				return MTLPixelFormatRG16Float;

			case Rendering::Format::R8G8B8A8_UNORM_PACK8:
				return MTLPixelFormatRGBA8Unorm;
			case Rendering::Format::R8G8B8A8_SRGB_PACK8:
				return MTLPixelFormatRGBA8Unorm_sRGB;
			case Rendering::Format::R8G8B8A8_SNORM_PACK8:
				return MTLPixelFormatRGBA8Snorm;
			case Rendering::Format::R8G8B8A8_UINT_PACK8:
				return MTLPixelFormatRGBA8Uint;
			case Rendering::Format::R8G8B8A8_SINT_PACK8:
				return MTLPixelFormatRGBA8Sint;

			case Rendering::Format::B8G8R8A8_UNORM:
				return MTLPixelFormatBGRA8Unorm;
			case Rendering::Format::B8G8R8A8_SRGB:
				return MTLPixelFormatBGRA8Unorm_sRGB;

			case Rendering::Format::R10G10B10A2_UNORM:
				return MTLPixelFormatRGB10A2Unorm;
			case Rendering::Format::R10G10B10A2_UINT:
				return MTLPixelFormatRGB10A2Uint;

			case Rendering::Format::R11G11B10_UFLOAT:
				return MTLPixelFormatRG11B10Float;
			case Rendering::Format::R9G9B9E5_UFLOAT:
				return MTLPixelFormatRGB9E5Float;

			case Rendering::Format::B10G10R10A2_UNORM:
				return MTLPixelFormatBGR10A2Unorm;

			case Rendering::Format::B10G10R10_XR:
				return MTLPixelFormatBGR10_XR;
			case Rendering::Format::B10G10R10_XR_SRGB:
				return MTLPixelFormatBGR10_XR_sRGB;

			case Rendering::Format::R32G32_UINT:
				return MTLPixelFormatRG32Uint;
			case Rendering::Format::R32G32_SINT:
				return MTLPixelFormatRG32Sint;
			case Rendering::Format::R32G32_SFLOAT:
				return MTLPixelFormatRG32Float;

			case Rendering::Format::R16G16B16A16_UNORM:
				return MTLPixelFormatRGBA16Unorm;
			case Rendering::Format::R16G16B16A16_SNORM:
				return MTLPixelFormatRGBA16Snorm;
			case Rendering::Format::R16G16B16A16_UINT:
				return MTLPixelFormatRGBA16Uint;
			case Rendering::Format::R16G16B16A16_SINT:
				return MTLPixelFormatRGBA16Sint;
			case Rendering::Format::R16G16B16A16_SFLOAT:
				return MTLPixelFormatRGBA16Float;

			case Rendering::Format::B10G10R10A10_XR:
				return MTLPixelFormatBGRA10_XR;
			case Rendering::Format::B10G10R10A10_XR_SRGB:
				return MTLPixelFormatBGRA10_XR_sRGB;

			case Rendering::Format::R32G32B32A32_UINT:
				return MTLPixelFormatRGBA32Uint;
			case Rendering::Format::R32G32B32A32_SINT:
				return MTLPixelFormatRGBA32Sint;
			case Rendering::Format::R32G32B32A32_SFLOAT:
				return MTLPixelFormatRGBA32Float;

#if !PLATFORM_APPLE_IOS
			case Rendering::Format::BC1_RGBA_UNORM:
			case Rendering::Format::BC1_RGB_UNORM:
				return MTLPixelFormatBC1_RGBA;
			case Rendering::Format::BC1_RGBA_SRGB:
			case Rendering::Format::BC1_RGB_SRGB:
				return MTLPixelFormatBC1_RGBA_sRGB;

			case Rendering::Format::BC2_RGBA_UNORM:
				return MTLPixelFormatBC2_RGBA;
			case Rendering::Format::BC2_RGBA_SRGB:
				return MTLPixelFormatBC2_RGBA_sRGB;

			case Rendering::Format::BC3_RGBA_UNORM:
				return MTLPixelFormatBC3_RGBA;
			case Rendering::Format::BC3_RGBA_SRGB:
				return MTLPixelFormatBC3_RGBA_sRGB;

			case Rendering::Format::BC4_R_UNORM:
				return MTLPixelFormatBC4_RUnorm;
			case Rendering::Format::BC4_R_SNORM:
				return MTLPixelFormatBC4_RSnorm;

			case Rendering::Format::BC5_RG_UNORM:
				return MTLPixelFormatBC5_RGUnorm;
			case Rendering::Format::BC5_RG_SNORM:
				return MTLPixelFormatBC5_RGSnorm;

			case Rendering::Format::BC6H_RGB_UFLOAT:
				return MTLPixelFormatBC6H_RGBUfloat;
			case Rendering::Format::BC6H_RGB_SFLOAT:
				return MTLPixelFormatBC6H_RGBFloat;

			case Rendering::Format::BC7_RGBA_UNORM:
				return MTLPixelFormatBC7_RGBAUnorm;
			case Rendering::Format::BC7_RGBA_SRGB:
				return MTLPixelFormatBC7_RGBAUnorm_sRGB;
#endif

			case Rendering::Format::ASTC_4X4_SRGB:
				return MTLPixelFormatASTC_4x4_sRGB;
			case Rendering::Format::ASTC_5X4_SRGB:
				return MTLPixelFormatASTC_5x4_sRGB;
			case Rendering::Format::ASTC_5X5_SRGB:
				return MTLPixelFormatASTC_5x5_sRGB;
			case Rendering::Format::ASTC_6X5_SRGB:
				return MTLPixelFormatASTC_6x5_sRGB;
			case Rendering::Format::ASTC_6X6_SRGB:
				return MTLPixelFormatASTC_6x6_sRGB;
			case Rendering::Format::ASTC_8X5_SRGB:
				return MTLPixelFormatASTC_8x5_sRGB;
			case Rendering::Format::ASTC_8X6_SRGB:
				return MTLPixelFormatASTC_8x6_sRGB;
			case Rendering::Format::ASTC_8X8_SRGB:
				return MTLPixelFormatASTC_8x8_sRGB;
			case Rendering::Format::ASTC_10X5_SRGB:
				return MTLPixelFormatASTC_10x5_sRGB;
			case Rendering::Format::ASTC_10X6_SRGB:
				return MTLPixelFormatASTC_10x6_sRGB;
			case Rendering::Format::ASTC_10X8_SRGB:
				return MTLPixelFormatASTC_10x8_sRGB;
			case Rendering::Format::ASTC_10X10_SRGB:
				return MTLPixelFormatASTC_10x10_sRGB;
			case Rendering::Format::ASTC_12X10_SRGB:
				return MTLPixelFormatASTC_12x10_sRGB;
			case Rendering::Format::ASTC_12X12_SRGB:
				return MTLPixelFormatASTC_12x12_sRGB;

			case Rendering::Format::ASTC_4X4_LDR:
				return MTLPixelFormatASTC_4x4_LDR;
			case Rendering::Format::ASTC_5X4_LDR:
				return MTLPixelFormatASTC_5x4_LDR;
			case Rendering::Format::ASTC_5X5_LDR:
				return MTLPixelFormatASTC_5x5_LDR;
			case Rendering::Format::ASTC_6X5_LDR:
				return MTLPixelFormatASTC_6x5_LDR;
			case Rendering::Format::ASTC_6X6_LDR:
				return MTLPixelFormatASTC_6x6_LDR;
			case Rendering::Format::ASTC_8X5_LDR:
				return MTLPixelFormatASTC_8x5_LDR;
			case Rendering::Format::ASTC_8X6_LDR:
				return MTLPixelFormatASTC_8x6_LDR;
			case Rendering::Format::ASTC_8X8_LDR:
				return MTLPixelFormatASTC_8x8_LDR;
			case Rendering::Format::ASTC_10X5_LDR:
				return MTLPixelFormatASTC_10x5_LDR;
			case Rendering::Format::ASTC_10X6_LDR:
				return MTLPixelFormatASTC_10x6_LDR;
			case Rendering::Format::ASTC_10X8_LDR:
				return MTLPixelFormatASTC_10x8_LDR;
			case Rendering::Format::ASTC_10X10_LDR:
				return MTLPixelFormatASTC_10x10_LDR;
			case Rendering::Format::ASTC_12X10_LDR:
				return MTLPixelFormatASTC_12x10_LDR;
			case Rendering::Format::ASTC_12X12_LDR:
				return MTLPixelFormatASTC_12x12_LDR;

			case Rendering::Format::ASTC_4X4_HDR:
				return MTLPixelFormatASTC_4x4_HDR;
			case Rendering::Format::ASTC_5X4_HDR:
				return MTLPixelFormatASTC_5x4_HDR;
			case Rendering::Format::ASTC_5X5_HDR:
				return MTLPixelFormatASTC_5x5_HDR;
			case Rendering::Format::ASTC_6X5_HDR:
				return MTLPixelFormatASTC_6x5_HDR;
			case Rendering::Format::ASTC_6X6_HDR:
				return MTLPixelFormatASTC_6x6_HDR;
			case Rendering::Format::ASTC_8X5_HDR:
				return MTLPixelFormatASTC_8x5_HDR;
			case Rendering::Format::ASTC_8X6_HDR:
				return MTLPixelFormatASTC_8x6_HDR;
			case Rendering::Format::ASTC_8X8_HDR:
				return MTLPixelFormatASTC_8x8_HDR;
			case Rendering::Format::ASTC_10X5_HDR:
				return MTLPixelFormatASTC_10x5_HDR;
			case Rendering::Format::ASTC_10X6_HDR:
				return MTLPixelFormatASTC_10x6_HDR;
			case Rendering::Format::ASTC_10X8_HDR:
				return MTLPixelFormatASTC_10x8_HDR;
			case Rendering::Format::ASTC_10X10_HDR:
				return MTLPixelFormatASTC_10x10_HDR;
			case Rendering::Format::ASTC_12X10_HDR:
				return MTLPixelFormatASTC_12x10_HDR;
			case Rendering::Format::ASTC_12X12_HDR:
				return MTLPixelFormatASTC_12x12_HDR;

			case Rendering::Format::D16_UNORM:
				return MTLPixelFormatDepth16Unorm;
			case Rendering::Format::D32_SFLOAT:
				return MTLPixelFormatDepth32Float;

			case Rendering::Format::S8_UINT:
				return MTLPixelFormatStencil8;

#if (!PLATFORM_APPLE_IOS && !PLATFORM_APPLE_VISIONOS) || PLATFORM_APPLE_MACCATALYST
			case Rendering::Format::D24_UNORM_S8_UINT:
				return MTLPixelFormatDepth24Unorm_Stencil8;
#endif

			case Rendering::Format::D32_SFLOAT_S8_UINT:
				return MTLPixelFormatDepth32Float_Stencil8;
			default:
				return MTLPixelFormatInvalid;
		}
	}

	[[nodiscard]] inline Format ConvertFormat(const MTLPixelFormat metalFormat)
	{
		switch (metalFormat)
		{
			case MTLPixelFormatInvalid:
				return Format::Invalid;

			case MTLPixelFormatA8Unorm:
				return Format::A8_UNORM;

			case MTLPixelFormatR8Unorm:
				return Format::R8_UNORM;
			case MTLPixelFormatR8Unorm_sRGB:
				return Format::R8_SRGB;
			case MTLPixelFormatR8Snorm:
				return Format::R8_SNORM;
			case MTLPixelFormatR8Uint:
				return Format::R8_UINT;
			case MTLPixelFormatR8Sint:
				return Format::R8_SINT;

			case MTLPixelFormatR16Unorm:
				return Format::R16_UNORM;
			case MTLPixelFormatR16Snorm:
				return Format::R16_SNORM;
			case MTLPixelFormatR16Uint:
				return Format::R16_UINT;
			case MTLPixelFormatR16Sint:
				return Format::R16_SINT;
			case MTLPixelFormatR16Float:
				return Format::R16_SFLOAT;

			case MTLPixelFormatRG8Unorm:
				return Format::R8G8_UNORM;
			case MTLPixelFormatRG8Unorm_sRGB:
				return Format::R8G8_SRGB;
			case MTLPixelFormatRG8Snorm:
				return Format::R8G8_SNORM;
			case MTLPixelFormatRG8Uint:
				return Format::R8G8_UINT;
			case MTLPixelFormatRG8Sint:
				return Format::R8G8_SINT;

			case MTLPixelFormatB5G6R5Unorm:
				return Format::B5G6R5_UNORM;
			case MTLPixelFormatA1BGR5Unorm:
				return Format::A1R5G5B5_UNORM;
			case MTLPixelFormatBGR5A1Unorm:
				return Format::A1R5G5B5_UNORM;

			case MTLPixelFormatR32Uint:
				return Format::R32_UINT;
			case MTLPixelFormatR32Sint:
				return Format::R32_SINT;
			case MTLPixelFormatR32Float:
				return Format::R32_SFLOAT;

			case MTLPixelFormatRG16Unorm:
				return Format::R16G16_UNORM;
			case MTLPixelFormatRG16Snorm:
				return Format::R16G16_SNORM;
			case MTLPixelFormatRG16Uint:
				return Format::R16G16_UINT;
			case MTLPixelFormatRG16Sint:
				return Format::R16G16_SINT;
			case MTLPixelFormatRG16Float:
				return Format::R16G16_SFLOAT;

			case MTLPixelFormatRGBA8Unorm:
				return Format::R8G8B8A8_UNORM_PACK8;
			case MTLPixelFormatRGBA8Unorm_sRGB:
				return Format::R8G8B8A8_SRGB_PACK8;
			case MTLPixelFormatRGBA8Snorm:
				return Format::R8G8B8A8_SNORM_PACK8;
			case MTLPixelFormatRGBA8Uint:
				return Format::R8G8B8A8_UINT_PACK8;
			case MTLPixelFormatRGBA8Sint:
				return Format::R8G8B8A8_SINT_PACK8;

			case MTLPixelFormatBGRA8Unorm:
				return Format::B8G8R8A8_UNORM;
			case MTLPixelFormatBGRA8Unorm_sRGB:
				return Format::B8G8R8A8_SRGB;

			case MTLPixelFormatRGB10A2Unorm:
				return Format::R10G10B10A2_UNORM;
			case MTLPixelFormatRGB10A2Uint:
				return Format::R10G10B10A2_UINT;

			case MTLPixelFormatRG11B10Float:
				return Format::R11G11B10_UFLOAT;
			case MTLPixelFormatRGB9E5Float:
				return Format::R9G9B9E5_UFLOAT;

			case MTLPixelFormatBGR10A2Unorm:
				return Format::B10G10R10A2_UNORM;

			case MTLPixelFormatRG32Uint:
				return Format::R32G32_UINT;
			case MTLPixelFormatRG32Sint:
				return Format::R32G32_SINT;
			case MTLPixelFormatRG32Float:
				return Format::R32G32_SFLOAT;

			case MTLPixelFormatRGBA16Unorm:
				return Format::R16G16B16A16_UNORM;
			case MTLPixelFormatRGBA16Snorm:
				return Format::R16G16B16A16_SNORM;
			case MTLPixelFormatRGBA16Uint:
				return Format::R16G16B16A16_UINT;
			case MTLPixelFormatRGBA16Sint:
				return Format::R16G16B16A16_SINT;
			case MTLPixelFormatRGBA16Float:
				return Format::R16G16B16A16_SFLOAT;

			case MTLPixelFormatRGBA32Uint:
				return Format::R32G32B32A32_UINT;
			case MTLPixelFormatRGBA32Sint:
				return Format::R32G32B32A32_SINT;
			case MTLPixelFormatRGBA32Float:
				return Format::R32G32B32A32_SFLOAT;

			case MTLPixelFormatBC1_RGBA:
				return Format::BC1_RGBA_UNORM;
			case MTLPixelFormatBC1_RGBA_sRGB:
				return Format::BC1_RGBA_SRGB;

			case MTLPixelFormatBC2_RGBA:
				return Format::BC2_RGBA_UNORM;
			case MTLPixelFormatBC2_RGBA_sRGB:
				return Format::BC2_RGBA_SRGB;

			case MTLPixelFormatBC3_RGBA:
				return Format::BC3_RGBA_UNORM;
			case MTLPixelFormatBC3_RGBA_sRGB:
				return Format::BC3_RGBA_SRGB;

			case MTLPixelFormatBC4_RUnorm:
				return Format::BC4_R_UNORM;
			case MTLPixelFormatBC5_RGUnorm:
				return Format::BC5_RG_UNORM;

			case MTLPixelFormatBC6H_RGBFloat:
				return Format::BC6H_RGB_SFLOAT;
			case MTLPixelFormatBC6H_RGBUfloat:
				return Format::BC6H_RGB_UFLOAT;
			case MTLPixelFormatBC7_RGBAUnorm:
				return Format::BC7_RGBA_UNORM;
			case MTLPixelFormatBC7_RGBAUnorm_sRGB:
				return Format::BC7_RGBA_SRGB;

			case MTLPixelFormatASTC_4x4_sRGB:
				return Format::ASTC_4X4_SRGB;
			case MTLPixelFormatASTC_5x4_sRGB:
				return Format::ASTC_5X4_SRGB;
			case MTLPixelFormatASTC_5x5_sRGB:
				return Format::ASTC_5X5_SRGB;
			case MTLPixelFormatASTC_6x5_sRGB:
				return Format::ASTC_6X5_SRGB;
			case MTLPixelFormatASTC_6x6_sRGB:
				return Format::ASTC_6X6_SRGB;
			case MTLPixelFormatASTC_8x5_sRGB:
				return Format::ASTC_8X5_SRGB;
			case MTLPixelFormatASTC_8x6_sRGB:
				return Format::ASTC_8X6_SRGB;
			case MTLPixelFormatASTC_8x8_sRGB:
				return Format::ASTC_8X8_SRGB;
			case MTLPixelFormatASTC_10x5_sRGB:
				return Format::ASTC_10X5_SRGB;
			case MTLPixelFormatASTC_10x6_sRGB:
				return Format::ASTC_10X6_SRGB;
			case MTLPixelFormatASTC_10x8_sRGB:
				return Format::ASTC_10X8_SRGB;
			case MTLPixelFormatASTC_10x10_sRGB:
				return Format::ASTC_10X10_SRGB;
			case MTLPixelFormatASTC_12x10_sRGB:
				return Format::ASTC_12X10_SRGB;
			case MTLPixelFormatASTC_12x12_sRGB:
				return Format::ASTC_12X12_SRGB;

			case MTLPixelFormatASTC_4x4_LDR:
				return Format::ASTC_4X4_LDR;
			case MTLPixelFormatASTC_5x4_LDR:
				return Format::ASTC_5X4_LDR;
			case MTLPixelFormatASTC_5x5_LDR:
				return Format::ASTC_5X5_LDR;
			case MTLPixelFormatASTC_6x5_LDR:
				return Format::ASTC_6X5_LDR;
			case MTLPixelFormatASTC_6x6_LDR:
				return Format::ASTC_6X6_LDR;
			case MTLPixelFormatASTC_8x5_LDR:
				return Format::ASTC_8X5_LDR;
			case MTLPixelFormatASTC_8x6_LDR:
				return Format::ASTC_8X6_LDR;
			case MTLPixelFormatASTC_8x8_LDR:
				return Format::ASTC_8X8_LDR;
			case MTLPixelFormatASTC_10x5_LDR:
				return Format::ASTC_10X5_LDR;
			case MTLPixelFormatASTC_10x6_LDR:
				return Format::ASTC_10X6_LDR;
			case MTLPixelFormatASTC_10x8_LDR:
				return Format::ASTC_10X8_LDR;
			case MTLPixelFormatASTC_10x10_LDR:
				return Format::ASTC_10X10_LDR;
			case MTLPixelFormatASTC_12x10_LDR:
				return Format::ASTC_12X10_LDR;
			case MTLPixelFormatASTC_12x12_LDR:
				return Format::ASTC_12X12_LDR;

			case MTLPixelFormatASTC_4x4_HDR:
				return Format::ASTC_4X4_HDR;
			case MTLPixelFormatASTC_5x4_HDR:
				return Format::ASTC_5X4_HDR;
			case MTLPixelFormatASTC_5x5_HDR:
				return Format::ASTC_5X5_HDR;
			case MTLPixelFormatASTC_6x5_HDR:
				return Format::ASTC_6X5_HDR;
			case MTLPixelFormatASTC_6x6_HDR:
				return Format::ASTC_6X6_HDR;
			case MTLPixelFormatASTC_8x5_HDR:
				return Format::ASTC_8X5_HDR;
			case MTLPixelFormatASTC_8x6_HDR:
				return Format::ASTC_8X6_HDR;
			case MTLPixelFormatASTC_8x8_HDR:
				return Format::ASTC_8X8_HDR;
			case MTLPixelFormatASTC_10x5_HDR:
				return Format::ASTC_10X5_HDR;
			case MTLPixelFormatASTC_10x6_HDR:
				return Format::ASTC_10X6_HDR;
			case MTLPixelFormatASTC_10x8_HDR:
				return Format::ASTC_10X8_HDR;
			case MTLPixelFormatASTC_10x10_HDR:
				return Format::ASTC_10X10_HDR;
			case MTLPixelFormatASTC_12x10_HDR:
				return Format::ASTC_12X10_HDR;
			case MTLPixelFormatASTC_12x12_HDR:
				return Format::ASTC_12X12_HDR;

			case MTLPixelFormatDepth16Unorm:
				return Format::D16_UNORM;
			case MTLPixelFormatDepth32Float:
				return Format::D32_SFLOAT;

			case MTLPixelFormatStencil8:
				return Format::S8_UINT;

#if !PLATFORM_APPLE_VISIONOS && !PLATFORM_APPLE_IOS
			case MTLPixelFormatDepth24Unorm_Stencil8:
				return Format::D24_UNORM_S8_UINT;
#endif
			case MTLPixelFormatDepth32Float_Stencil8:
				return Format::D32_SFLOAT_S8_UINT;

			default:
				Assert(false, "Unsupported format");
				return Format::Invalid;
		}
	}
#endif
}
