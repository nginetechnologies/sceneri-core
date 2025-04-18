#pragma once

#include <Renderer/PushConstants/PushConstant.h>
#include <Renderer/PushConstants/PushConstantPreset.h>

namespace ngine::Rendering
{
	struct PushConstantDefinition
	{
		bool Serialize(const Serialization::Reader serializer);
		bool Serialize(Serialization::Writer serializer) const;

		[[nodiscard]] PushConstantType GetType() const
		{
			return m_defaultValue.GetType();
		}

		uint8 m_size;
		uint8 m_alignment;
		EnumFlags<ShaderStage> m_shaderStages;
		PushConstant m_defaultValue;
		PushConstantPreset m_preset = PushConstantPreset::Unknown;
	};
}
