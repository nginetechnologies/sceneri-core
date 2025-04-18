#pragma once

#include <Renderer/PushConstants/PushConstantType.h>

#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/Memory/Variant.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Color.h>
#include <Renderer/PushConstants/ForwardDeclarations/PushConstantValue.h>

namespace ngine::Rendering
{
	struct PushConstant
	{
		bool Serialize(const Serialization::Reader serializer);
		bool Serialize(Serialization::Writer serializer) const;

		[[nodiscard]] PushConstantType GetType() const
		{
			return (PushConstantType)m_value.GetActiveIndex();
		}

		PushConstantValue m_value;
	};
}
