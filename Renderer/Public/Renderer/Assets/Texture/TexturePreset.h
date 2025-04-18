#pragma once

#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::Rendering
{
	enum class TexturePreset : uint8
	{
		Unknown,
		// BC4 / R8
		Greyscale8 = 1,
		// BC5 / R8G8
		GreyscaleWithAlpha8 = 2,
		Diffuse = 3,
		DiffuseWithAlphaMask = 4,
		Normals = 5,
		Metalness = 6,
		Roughness = 7,
		AmbientOcclusion = 8,
		EnvironmentCubemapDiffuseHDR = 9,
		Depth = 10,
		EmissionColor = 11,
		Explicit = 12,
		EnvironmentCubemapSpecular = 13,
		Alpha = 14,
		DiffuseWithAlphaTransparency = 15,
		BRDF = 16,
		EmissionFactor = 17,

		Count
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Rendering::TexturePreset>
	{
		inline static constexpr auto Type = Reflection::Reflect<Rendering::TexturePreset>(
			"ab9d58a9-52ee-4b92-b291-5822f9d0b920"_guid,
			MAKE_UNICODE_LITERAL("Texture Preset"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{Rendering::TexturePreset::Greyscale8, MAKE_UNICODE_LITERAL("Greyscale")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::GreyscaleWithAlpha8, MAKE_UNICODE_LITERAL("Greyscale & Alpha")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::Diffuse, MAKE_UNICODE_LITERAL("Diffuse")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::DiffuseWithAlphaMask, MAKE_UNICODE_LITERAL("Diffuse & Alpha Mask")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::Normals, MAKE_UNICODE_LITERAL("Normals")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::Metalness, MAKE_UNICODE_LITERAL("Metalness")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::Roughness, MAKE_UNICODE_LITERAL("Roughness")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::AmbientOcclusion, MAKE_UNICODE_LITERAL("Ambient Occlusion")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::EnvironmentCubemapDiffuseHDR, MAKE_UNICODE_LITERAL("Cubemap Diffuse")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::Depth, MAKE_UNICODE_LITERAL("Depth")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::EmissionColor, MAKE_UNICODE_LITERAL("Emissive Color")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::EnvironmentCubemapSpecular, MAKE_UNICODE_LITERAL("Cubemap Specular")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::Alpha, MAKE_UNICODE_LITERAL("Alpha")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::DiffuseWithAlphaTransparency, MAKE_UNICODE_LITERAL("Diffuse & Alpha")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::BRDF, MAKE_UNICODE_LITERAL("BRDF")},
				Reflection::EnumTypeEntry{Rendering::TexturePreset::EmissionFactor, MAKE_UNICODE_LITERAL("Emissive Factor")}
			}}
		);
	};
}
