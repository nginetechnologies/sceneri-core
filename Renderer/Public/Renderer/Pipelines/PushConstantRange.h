#pragma once

#include <Renderer/ShaderStage.h>
#include <Common/EnumFlags.h>
#include <Common/Math/Range.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/TypeTraits/MemberOwnerType.h>
#include <Common/TypeTraits/MemberType.h>

namespace ngine::Rendering
{
	struct PushConstantRange
	{
		constexpr PushConstantRange(const EnumFlags<ShaderStage> stages, const Math::Range<uint32> range)
			: m_shaderStages(stages)
			, m_range(range)
		{
		}

		constexpr PushConstantRange(const EnumFlags<ShaderStage> stages, const uint32 offset, const uint32 size)
			: m_shaderStages(stages)
			, m_range(Math::Range<uint32>::Make(offset, size))
		{
		}

		EnumFlags<ShaderStage> m_shaderStages;
		Math::Range<uint32> m_range;
	};
}
