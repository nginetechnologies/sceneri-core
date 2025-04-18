#pragma once

#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::Rendering
{
	enum class PushConstantPreset : uint8
	{
		Unknown,
		DiffuseColor,
		EmissiveColor,
		AmbientColor,
		ReflectiveColor,
		Metalness,
		Roughness,
		Emissive,
		Speed
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Rendering::PushConstantPreset>
	{
		inline static constexpr auto Type = Reflection::Reflect<Rendering::PushConstantPreset>(
			"23f993f6-8aa5-48c1-a17d-1f5fd6d0e2e1"_guid,
			MAKE_UNICODE_LITERAL("Push Constant Preset"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{Rendering::PushConstantPreset::DiffuseColor, MAKE_UNICODE_LITERAL("DiffuseColor")},
				Reflection::EnumTypeEntry{Rendering::PushConstantPreset::AmbientColor, MAKE_UNICODE_LITERAL("AmbientColor")},
				Reflection::EnumTypeEntry{Rendering::PushConstantPreset::AmbientColor, MAKE_UNICODE_LITERAL("AmbientColor")},
				Reflection::EnumTypeEntry{Rendering::PushConstantPreset::ReflectiveColor, MAKE_UNICODE_LITERAL("ReflectiveColor")},
				Reflection::EnumTypeEntry{Rendering::PushConstantPreset::Metalness, MAKE_UNICODE_LITERAL("Metalness")},
				Reflection::EnumTypeEntry{Rendering::PushConstantPreset::Roughness, MAKE_UNICODE_LITERAL("Roughness")},
				Reflection::EnumTypeEntry{Rendering::PushConstantPreset::Emissive, MAKE_UNICODE_LITERAL("Emissive")},
				Reflection::EnumTypeEntry{Rendering::PushConstantPreset::Speed, MAKE_UNICODE_LITERAL("Speed")},
			}}
		);
	};
}
