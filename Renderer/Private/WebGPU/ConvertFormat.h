#pragma once

#include <Renderer/WebGPU/Includes.h>
#include <Renderer/Format.h>

namespace ngine::Rendering
{
#if RENDERER_WEBGPU
	[[nodiscard]] inline constexpr WGPUTextureFormat ConvertFormat(const Format format)
	{
		switch (format)
		{
			case Format::R8_UNORM:
				return WGPUTextureFormat_R8Unorm;
			case Format::R8_SNORM:
				return WGPUTextureFormat_R8Snorm;
			case Format::R8_UINT:
				return WGPUTextureFormat_R8Uint;
			case Format::R8_SINT:
				return WGPUTextureFormat_R8Sint;

			case Format::R16_UINT:
				return WGPUTextureFormat_R16Uint;
			case Format::R16_SINT:
				return WGPUTextureFormat_R16Sint;
			case Format::R16_SFLOAT:
				return WGPUTextureFormat_R16Float;
#if RENDERER_WEBGPU_DAWN
			case Format::R16_UNORM:
				return WGPUTextureFormat_R16Unorm;
			case Format::R16_SNORM:
				return WGPUTextureFormat_R16Snorm;
#endif

			case Format::R8G8_UNORM:
				return WGPUTextureFormat_RG8Unorm;
			case Format::R8G8_SNORM:
				return WGPUTextureFormat_RG8Snorm;
			case Format::R8G8_UINT:
				return WGPUTextureFormat_RG8Uint;
			case Format::R8G8_SINT:
				return WGPUTextureFormat_RG8Sint;

			case Format::R32_UINT:
				return WGPUTextureFormat_R32Uint;
			case Format::R32_SINT:
				return WGPUTextureFormat_R32Sint;
			case Format::R32_SFLOAT:
				return WGPUTextureFormat_R32Float;

			case Format::R16G16_UINT:
				return WGPUTextureFormat_RG16Uint;
			case Format::R16G16_SINT:
				return WGPUTextureFormat_RG16Sint;
			case Format::R16G16_SFLOAT:
				return WGPUTextureFormat_RG16Float;
#if RENDERER_WEBGPU_DAWN
			case Format::R16G16_UNORM:
				return WGPUTextureFormat_RG16Unorm;
			case Format::R16G16_SNORM:
				return WGPUTextureFormat_RG16Snorm;
#endif

			case Format::R8G8B8A8_UNORM_PACK8:
				return WGPUTextureFormat_RGBA8Unorm;
			case Format::R8G8B8A8_SRGB_PACK8:
				return WGPUTextureFormat_RGBA8UnormSrgb;
			case Format::R8G8B8A8_SNORM_PACK8:
				return WGPUTextureFormat_RGBA8Snorm;
			case Format::R8G8B8A8_UINT_PACK8:
				return WGPUTextureFormat_RGBA8Uint;
			case Format::R8G8B8A8_SINT_PACK8:
				return WGPUTextureFormat_RGBA8Sint;

			case Format::B8G8R8A8_UNORM:
				return WGPUTextureFormat_BGRA8Unorm;
			case Format::B8G8R8A8_SRGB:
				return WGPUTextureFormat_BGRA8UnormSrgb;

			case Format::R10G10B10A2_UNORM:
				return WGPUTextureFormat_RGB10A2Unorm;
#if RENDERER_WEBGPU_WGPU_NATIVE
			case Format::R10G10B10A2_UINT:
				return WGPUTextureFormat_RGB10A2Uint;
#endif
			case Format::B10G10R10A2_UNORM:
				return WGPUTextureFormat_RGB10A2Unorm;
				// case Format::B10G10R10A2_UINT:
				//	return WGPUTextureFormat_RGB10A2Uint;

			case Format::R11G11B10_UFLOAT:
				return WGPUTextureFormat_RG11B10Ufloat;
			case Format::R9G9B9E5_UFLOAT:
				return WGPUTextureFormat_RGB9E5Ufloat;

			case Format::R32G32_SFLOAT:
				return WGPUTextureFormat_RG32Float;
			case Format::R32G32_UINT:
				return WGPUTextureFormat_RG32Uint;
			case Format::R32G32_SINT:
				return WGPUTextureFormat_RG32Sint;

			case Format::R16G16B16A16_UINT:
				return WGPUTextureFormat_RGBA16Uint;
			case Format::R16G16B16A16_SINT:
				return WGPUTextureFormat_RGBA16Sint;
			case Format::R16G16B16A16_SFLOAT:
				return WGPUTextureFormat_RGBA16Float;
#if RENDERER_WEBGPU_DAWN
			case Format::R16G16B16A16_UNORM:
				return WGPUTextureFormat_RGBA16Unorm;
			case Format::R16G16B16A16_SNORM:
				return WGPUTextureFormat_RGBA16Snorm;
#endif

			case Format::R32G32B32A32_SFLOAT:
				return WGPUTextureFormat_RGBA32Float;
			case Format::R32G32B32A32_UINT:
				return WGPUTextureFormat_RGBA32Uint;
			case Format::R32G32B32A32_SINT:
				return WGPUTextureFormat_RGBA32Sint;

			case Format::S8_UINT:
				return WGPUTextureFormat_Stencil8;

			case Format::D16_UNORM:
				return WGPUTextureFormat_Depth16Unorm;
			case Format::D24_UNORM:
				return WGPUTextureFormat_Depth24Plus;
			case Format::D24_UNORM_S8_UINT:
				return WGPUTextureFormat_Depth24PlusStencil8;
			case Format::D32_SFLOAT:
				return WGPUTextureFormat_Depth32Float;
			case Format::D32_SFLOAT_S8_UINT:
				return WGPUTextureFormat_Depth32FloatStencil8;

			case Format::BC1_RGB_UNORM:
				return WGPUTextureFormat_BC1RGBAUnorm;
			case Format::BC1_RGB_SRGB:
				return WGPUTextureFormat_BC1RGBAUnormSrgb;
			case Format::BC1_RGBA_UNORM:
				return WGPUTextureFormat_BC1RGBAUnorm;
			case Format::BC1_RGBA_SRGB:
				return WGPUTextureFormat_BC1RGBAUnormSrgb;

			case Format::BC2_RGBA_UNORM:
				return WGPUTextureFormat_BC2RGBAUnorm;
			case Format::BC2_RGBA_SRGB:
				return WGPUTextureFormat_BC2RGBAUnormSrgb;

			case Format::BC3_RGBA_UNORM:
				return WGPUTextureFormat_BC3RGBAUnorm;
			case Format::BC3_RGBA_SRGB:
				return WGPUTextureFormat_BC3RGBAUnormSrgb;

			case Format::BC4_R_UNORM:
				return WGPUTextureFormat_BC4RUnorm;
			case Format::BC4_R_SNORM:
				return WGPUTextureFormat_BC4RSnorm;

			case Format::BC5_RG_UNORM:
				return WGPUTextureFormat_BC5RGUnorm;
			case Format::BC5_RG_SNORM:
				return WGPUTextureFormat_BC5RGSnorm;

			case Format::BC6H_RGB_UFLOAT:
				return WGPUTextureFormat_BC6HRGBUfloat;
			case Format::BC6H_RGB_SFLOAT:
				return WGPUTextureFormat_BC6HRGBFloat;

			case Format::BC7_RGBA_UNORM:
				return WGPUTextureFormat_BC7RGBAUnorm;
			case Format::BC7_RGBA_SRGB:
				return WGPUTextureFormat_BC7RGBAUnormSrgb;

			case Rendering::Format::ASTC_4X4_LDR:
				return WGPUTextureFormat_ASTC4x4Unorm;
			case Format::ASTC_4X4_SRGB:
				return WGPUTextureFormat_ASTC4x4UnormSrgb;

			case Format::ASTC_5X4_LDR:
				return WGPUTextureFormat_ASTC5x4Unorm;
			case Format::ASTC_5X4_SRGB:
				return WGPUTextureFormat_ASTC5x4UnormSrgb;

			case Format::ASTC_5X5_LDR:
				return WGPUTextureFormat_ASTC5x5Unorm;
			case Format::ASTC_5X5_SRGB:
				return WGPUTextureFormat_ASTC5x5UnormSrgb;

			case Format::ASTC_6X5_LDR:
				return WGPUTextureFormat_ASTC6x5Unorm;
			case Format::ASTC_6X5_SRGB:
				return WGPUTextureFormat_ASTC6x5UnormSrgb;

			case Format::ASTC_6X6_LDR:
				return WGPUTextureFormat_ASTC6x6Unorm;
			case Format::ASTC_6X6_SRGB:
				return WGPUTextureFormat_ASTC6x6UnormSrgb;

			case Format::ASTC_8X5_LDR:
				return WGPUTextureFormat_ASTC8x5Unorm;
			case Format::ASTC_8X5_SRGB:
				return WGPUTextureFormat_ASTC8x5UnormSrgb;

			case Format::ASTC_8X6_LDR:
				return WGPUTextureFormat_ASTC8x6Unorm;
			case Format::ASTC_8X6_SRGB:
				return WGPUTextureFormat_ASTC8x6UnormSrgb;

			case Format::ASTC_8X8_LDR:
				return WGPUTextureFormat_ASTC8x8Unorm;
			case Format::ASTC_8X8_SRGB:
				return WGPUTextureFormat_ASTC8x8UnormSrgb;

			case Format::ASTC_10X5_LDR:
				return WGPUTextureFormat_ASTC10x5Unorm;
			case Format::ASTC_10X5_SRGB:
				return WGPUTextureFormat_ASTC10x5UnormSrgb;

			case Format::ASTC_10X6_LDR:
				return WGPUTextureFormat_ASTC10x6Unorm;
			case Format::ASTC_10X6_SRGB:
				return WGPUTextureFormat_ASTC10x6UnormSrgb;

			case Format::ASTC_10X8_LDR:
				return WGPUTextureFormat_ASTC10x8Unorm;
			case Format::ASTC_10X8_SRGB:
				return WGPUTextureFormat_ASTC10x8UnormSrgb;

			case Format::ASTC_10X10_LDR:
				return WGPUTextureFormat_ASTC10x10Unorm;
			case Format::ASTC_10X10_SRGB:
				return WGPUTextureFormat_ASTC10x10UnormSrgb;

			case Format::ASTC_12X10_LDR:
				return WGPUTextureFormat_ASTC12x10Unorm;
			case Format::ASTC_12X10_SRGB:
				return WGPUTextureFormat_ASTC12x10UnormSrgb;

			case Format::ASTC_12X12_LDR:
				return WGPUTextureFormat_ASTC12x12Unorm;
			case Format::ASTC_12X12_SRGB:
				return WGPUTextureFormat_ASTC12x12UnormSrgb;

			default:
				return WGPUTextureFormat_Undefined;
		}
	}

	[[nodiscard]] inline constexpr Format ConvertFormat(const WGPUTextureFormat format)
	{
		switch (format)
		{
			case WGPUTextureFormat_R8Unorm:
				return Format::R8_UNORM;
			case WGPUTextureFormat_R8Snorm:
				return Format::R8_SNORM;
			case WGPUTextureFormat_R8Uint:
				return Format::R8_UINT;
			case WGPUTextureFormat_R8Sint:
				return Format::R8_SINT;

			case WGPUTextureFormat_R16Uint:
				return Format::R16_UINT;
			case WGPUTextureFormat_R16Sint:
				return Format::R16_SINT;
			case WGPUTextureFormat_R16Float:
				return Format::R16_SFLOAT;
#if RENDERER_WEBGPU_DAWN
			case WGPUTextureFormat_R16Unorm:
				return Format::R16_UNORM;
			case WGPUTextureFormat_R16Snorm:
				return Format::R16_SNORM;
#endif

			case WGPUTextureFormat_RG8Unorm:
				return Format::R8G8_UNORM;
			case WGPUTextureFormat_RG8Snorm:
				return Format::R8G8_SNORM;
			case WGPUTextureFormat_RG8Uint:
				return Format::R8G8_UINT;
			case WGPUTextureFormat_RG8Sint:
				return Format::R8G8_SINT;

			case WGPUTextureFormat_R32Uint:
				return Format::R32_UINT;
			case WGPUTextureFormat_R32Sint:
				return Format::R32_SINT;
			case WGPUTextureFormat_R32Float:
				return Format::R32_SFLOAT;

			case WGPUTextureFormat_RG16Uint:
				return Format::R16G16_UINT;
			case WGPUTextureFormat_RG16Sint:
				return Format::R16G16_SINT;
			case WGPUTextureFormat_RG16Float:
				return Format::R16G16_SFLOAT;
#if RENDERER_WEBGPU_DAWN
			case WGPUTextureFormat_RG16Unorm:
				return Format::R16G16_UNORM;
			case WGPUTextureFormat_RG16Snorm:
				return Format::R16G16_SNORM;
#endif

			case WGPUTextureFormat_RGBA8Unorm:
				return Format::R8G8B8A8_UNORM_PACK8;
			case WGPUTextureFormat_RGBA8UnormSrgb:
				return Format::R8G8B8A8_SRGB_PACK8;
			case WGPUTextureFormat_RGBA8Snorm:
				return Format::R8G8B8A8_SNORM_PACK8;
			case WGPUTextureFormat_RGBA8Uint:
				return Format::R8G8B8A8_UINT_PACK8;
			case WGPUTextureFormat_RGBA8Sint:
				return Format::R8G8B8A8_SINT_PACK8;

			case WGPUTextureFormat_BGRA8Unorm:
				return Format::B8G8R8A8_UNORM;
			case WGPUTextureFormat_BGRA8UnormSrgb:
				return Format::B8G8R8A8_SRGB;

			case WGPUTextureFormat_RGB10A2Unorm:
				return Format::R10G10B10A2_UNORM;
			case WGPUTextureFormat_RGB10A2Uint:
				return Format::R10G10B10A2_UINT;
				// case WGPUTextureFormat_RGB10A2Unorm:
				//	return Format::B10G10R10A2_UNORM;
				//  case Format::B10G10R10A2_UINT:
				//	return WGPUTextureFormat_RGB10A2Uint;

			case WGPUTextureFormat_RG11B10Ufloat:
				return Format::R11G11B10_UFLOAT;
			case WGPUTextureFormat_RGB9E5Ufloat:
				return Format::R9G9B9E5_UFLOAT;

			case WGPUTextureFormat_RG32Float:
				return Format::R32G32_SFLOAT;
			case WGPUTextureFormat_RG32Uint:
				return Format::R32G32_UINT;
			case WGPUTextureFormat_RG32Sint:
				return Format::R32G32_SINT;

			case WGPUTextureFormat_RGBA16Uint:
				return Format::R16G16B16A16_UINT;
			case WGPUTextureFormat_RGBA16Sint:
				return Format::R16G16B16A16_SINT;
			case WGPUTextureFormat_RGBA16Float:
				return Format::R16G16B16A16_SFLOAT;
#if RENDERER_WEBGPU_DAWN
			case WGPUTextureFormat_RGBA16Unorm:
				return Format::R16G16B16A16_UNORM;
			case WGPUTextureFormat_RGBA16Snorm:
				return Format::R16G16B16A16_SNORM;
#endif

			case WGPUTextureFormat_RGBA32Float:
				return Format::R32G32B32A32_SFLOAT;
			case WGPUTextureFormat_RGBA32Uint:
				return Format::R32G32B32A32_UINT;
			case WGPUTextureFormat_RGBA32Sint:
				return Format::R32G32B32A32_SINT;

#if RENDERER_WEBGPU_DAWN
			case WGPUTextureFormat_R8BG8Biplanar420Unorm:
				return Format::Invalid;
			case WGPUTextureFormat_R8BG8A8Triplanar420Unorm:
				return Format::Invalid;
			case WGPUTextureFormat_R10X6BG10X6Biplanar420Unorm:
				return Format::Invalid;
			case WGPUTextureFormat_R8BG8Biplanar422Unorm:
				return Format::Invalid;
			case WGPUTextureFormat_R8BG8Biplanar444Unorm:
				return Format::Invalid;
			case WGPUTextureFormat_R10X6BG10X6Biplanar422Unorm:
				return Format::Invalid;
			case WGPUTextureFormat_R10X6BG10X6Biplanar444Unorm:
				return Format::Invalid;
			case WGPUTextureFormat::WGPUTextureFormat_External:
				return Format::Invalid;
#endif

			case WGPUTextureFormat_Stencil8:
				return Format::S8_UINT;

			case WGPUTextureFormat_Depth16Unorm:
				return Format::D16_UNORM;
			case WGPUTextureFormat_Depth24Plus:
				return Format::D24_UNORM;
			case WGPUTextureFormat_Depth24PlusStencil8:
				return Format::D24_UNORM_S8_UINT;
			case WGPUTextureFormat_Depth32Float:
				return Format::D32_SFLOAT;
			case WGPUTextureFormat_Depth32FloatStencil8:
				return Format::D32_SFLOAT_S8_UINT;

			// case WGPUTextureFormat_BC1RGBAUnorm:
			//	return Format::BC1_RGB_UNORM;
			// case WGPUTextureFormat_BC1RGBAUnormSrgb:
			//	return Format::BC1_RGB_SRGB;
			case WGPUTextureFormat_BC1RGBAUnorm:
				return Format::BC1_RGBA_UNORM;
			case WGPUTextureFormat_BC1RGBAUnormSrgb:
				return Format::BC1_RGBA_SRGB;

			case WGPUTextureFormat_BC2RGBAUnorm:
				return Format::BC2_RGBA_UNORM;
			case WGPUTextureFormat_BC2RGBAUnormSrgb:
				return Format::BC2_RGBA_SRGB;

			case WGPUTextureFormat_BC3RGBAUnorm:
				return Format::BC3_RGBA_UNORM;
			case WGPUTextureFormat_BC3RGBAUnormSrgb:
				return Format::BC3_RGBA_SRGB;

			case WGPUTextureFormat_BC4RUnorm:
				return Format::BC4_R_UNORM;
			case WGPUTextureFormat_BC4RSnorm:
				return Format::BC4_R_SNORM;

			case WGPUTextureFormat_BC5RGUnorm:
				return Format::BC5_RG_UNORM;
			case WGPUTextureFormat_BC5RGSnorm:
				return Format::BC5_RG_SNORM;

			case WGPUTextureFormat_BC6HRGBUfloat:
				return Format::BC6H_RGB_UFLOAT;
			case WGPUTextureFormat_BC6HRGBFloat:
				return Format::BC6H_RGB_SFLOAT;

			case WGPUTextureFormat_BC7RGBAUnorm:
				return Format::BC7_RGBA_UNORM;
			case WGPUTextureFormat_BC7RGBAUnormSrgb:
				return Format::BC7_RGBA_SRGB;

			case WGPUTextureFormat_ASTC4x4Unorm:
				return Rendering::Format::ASTC_4X4_LDR;
			case WGPUTextureFormat_ASTC4x4UnormSrgb:
				return Format::ASTC_4X4_SRGB;

			case WGPUTextureFormat_ASTC5x4Unorm:
				return Format::ASTC_5X4_LDR;
			case WGPUTextureFormat_ASTC5x4UnormSrgb:
				return Format::ASTC_5X4_SRGB;

			case WGPUTextureFormat_ASTC5x5Unorm:
				return Format::ASTC_5X5_LDR;
			case WGPUTextureFormat_ASTC5x5UnormSrgb:
				return Format::ASTC_5X5_SRGB;

			case WGPUTextureFormat_ASTC6x5Unorm:
				return Format::ASTC_6X5_LDR;
			case WGPUTextureFormat_ASTC6x5UnormSrgb:
				return Format::ASTC_6X5_SRGB;

			case WGPUTextureFormat_ASTC6x6Unorm:
				return Format::ASTC_6X6_LDR;
			case WGPUTextureFormat_ASTC6x6UnormSrgb:
				return Format::ASTC_6X6_SRGB;

			case WGPUTextureFormat_ASTC8x5Unorm:
				return Format::ASTC_8X5_LDR;
			case WGPUTextureFormat_ASTC8x5UnormSrgb:
				return Format::ASTC_8X5_SRGB;

			case WGPUTextureFormat_ASTC8x6Unorm:
				return Format::ASTC_8X6_LDR;
			case WGPUTextureFormat_ASTC8x6UnormSrgb:
				return Format::ASTC_8X6_SRGB;

			case WGPUTextureFormat_ASTC8x8Unorm:
				return Format::ASTC_8X8_LDR;
			case WGPUTextureFormat_ASTC8x8UnormSrgb:
				return Format::ASTC_8X8_SRGB;

			case WGPUTextureFormat_ASTC10x5Unorm:
				return Format::ASTC_10X5_LDR;
			case WGPUTextureFormat_ASTC10x5UnormSrgb:
				return Format::ASTC_10X5_SRGB;

			case WGPUTextureFormat_ASTC10x6Unorm:
				return Format::ASTC_10X6_LDR;
			case WGPUTextureFormat_ASTC10x6UnormSrgb:
				return Format::ASTC_10X6_SRGB;

			case WGPUTextureFormat_ASTC10x8Unorm:
				return Format::ASTC_10X8_LDR;
			case WGPUTextureFormat_ASTC10x8UnormSrgb:
				return Format::ASTC_10X8_SRGB;

			case WGPUTextureFormat_ASTC10x10Unorm:
				return Format::ASTC_10X10_LDR;
			case WGPUTextureFormat_ASTC10x10UnormSrgb:
				return Format::ASTC_10X10_SRGB;

			case WGPUTextureFormat_ASTC12x10Unorm:
				return Format::ASTC_12X10_LDR;
			case WGPUTextureFormat_ASTC12x10UnormSrgb:
				return Format::ASTC_12X10_SRGB;

			case WGPUTextureFormat_ASTC12x12Unorm:
				return Format::ASTC_12X12_LDR;
			case WGPUTextureFormat_ASTC12x12UnormSrgb:
				return Format::ASTC_12X12_SRGB;

			case WGPUTextureFormat_Undefined:
			case WGPUTextureFormat_Force32:
				return Format::Invalid;

			case WGPUTextureFormat_ETC2RGB8Unorm:
			case WGPUTextureFormat_ETC2RGB8UnormSrgb:
			case WGPUTextureFormat_ETC2RGB8A1Unorm:
			case WGPUTextureFormat_ETC2RGB8A1UnormSrgb:
			case WGPUTextureFormat_ETC2RGBA8Unorm:
			case WGPUTextureFormat_ETC2RGBA8UnormSrgb:
			case WGPUTextureFormat_EACR11Unorm:
			case WGPUTextureFormat_EACR11Snorm:
			case WGPUTextureFormat_EACRG11Unorm:
			case WGPUTextureFormat_EACRG11Snorm:
				return Format::Invalid;
		}
		ExpectUnreachable();
	}
#endif
}
