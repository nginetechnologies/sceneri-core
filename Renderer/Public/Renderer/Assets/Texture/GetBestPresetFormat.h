#pragma once

#include "TexturePreset.h"
#include <Renderer/Format.h>
#include <Renderer/FormatInfo.h>
#include <Common/Platform/Type.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Vector2/Mod.h>

namespace ngine::Rendering
{
	[[nodiscard]] inline constexpr EnumFlags<TextureAsset::BinaryType> GetSupportedTextureBinaryTypes(const Platform::Type platform)
	{
		switch (platform)
		{
			case Platform::Type::Windows:
			case Platform::Type::Linux:
			case Platform::Type::macOS:
			case Platform::Type::macCatalyst:
			case Platform::Type::Web:
				return TextureAsset::BinaryType::ASTC | TextureAsset::BinaryType::BC;
			case Platform::Type::iOS:
			case Platform::Type::visionOS:
			case Platform::Type::Android:
				return TextureAsset::BinaryType::ASTC;
			case Platform::Type::All:
			case Platform::Type::Apple:
				// case Platform::Type::Count:
				ExpectUnreachable();
		}

		ExpectUnreachable();
	}

	[[nodiscard]] inline Format
	GetBestPresetFormat(const TextureAsset::BinaryType binaryType, const TexturePreset preset, const Math::Vector2ui resolution)
	{
		Assert(preset != TexturePreset::Explicit);

		switch (binaryType)
		{
			case TextureAsset::BinaryType::ASTC:
			{
				switch (preset)
				{
					case TexturePreset::BRDF:
						return Format::R16G16_SFLOAT;
					case TexturePreset::EnvironmentCubemapSpecular:
						return Format::ASTC_4X4_LDR;
					default:
					{
						constexpr Array availableFormats{
							Format::ASTC_8X8_LDR,
							Format::ASTC_5X4_LDR,
							Format::ASTC_6X5_LDR,
							Format::ASTC_8X5_LDR,
							Format::ASTC_8X6_LDR,
							Format::ASTC_10X5_LDR,
							Format::ASTC_10X6_LDR,
							Format::ASTC_10X8_LDR,
							Format::ASTC_12X10_LDR,
						};
						for (const Format availableFormat : availableFormats.GetView())
						{
							const FormatInfo formatInfo = GetFormatInfo(availableFormat);
							const Math::TBoolVector2<uint32> isDivisibleByBlockExtent =
								Math::Mod(resolution, Math::Vector2ui{formatInfo.m_blockExtent.x, formatInfo.m_blockExtent.y}) == Math::Zero;
							if (isDivisibleByBlockExtent.AreAllSet())
							{
								return availableFormat;
							}
						}

						return Format::Invalid;
					}
				}
			}
			case TextureAsset::BinaryType::BC:
			{
				switch (preset)
				{
					case TexturePreset::Greyscale8:
					case TexturePreset::Metalness:
					case TexturePreset::Roughness:
					case TexturePreset::AmbientOcclusion:
					case TexturePreset::Alpha:
					case TexturePreset::EmissionFactor:
						return Format::BC4_R_UNORM;
					case TexturePreset::GreyscaleWithAlpha8:
						return Format::BC5_RG_UNORM;
					case TexturePreset::Diffuse:
					case TexturePreset::Normals:
					case TexturePreset::EmissionColor:
						return Format::BC1_RGB_UNORM;
					case TexturePreset::DiffuseWithAlphaMask:
						return Format::BC1_RGBA_UNORM;
					case TexturePreset::DiffuseWithAlphaTransparency:
						return Format::BC3_RGBA_UNORM;
					case TexturePreset::EnvironmentCubemapDiffuseHDR:
					case TexturePreset::EnvironmentCubemapSpecular:
						return Format::BC6H_RGB_UFLOAT;
					case TexturePreset::Depth:
						return Format::R8_UNORM;
					case TexturePreset::BRDF:
						return Format::R16G16_SFLOAT;

					case TexturePreset::Unknown:
					case TexturePreset::Explicit:
					case TexturePreset::Count:
						return Format::Invalid;
				}
			}
			break;
			case TextureAsset::BinaryType::Uncompressed:
				ExpectUnreachable();
			case TextureAsset::BinaryType::Count:
			case Rendering::TextureAsset::BinaryType::End:
				ExpectUnreachable();
		}

		ExpectUnreachable();
	}
}
