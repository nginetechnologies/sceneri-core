#pragma once

#include <Renderer/PipelineStageFlags.h>
#include <Renderer/AccessFlags.h>

#include <Common/EnumFlags.h>

namespace ngine::Rendering
{
	enum class DependencyFlags : uint32
	{
		ByRegion = 1
	};

	inline static constexpr uint32 ExternalSubpass = ~0u;

	struct SubpassDependency
	{
		uint32 m_subpassSource;
		uint32 m_subpassTarget;
		EnumFlags<PipelineStageFlags> m_stageFlagsSource;
		EnumFlags<PipelineStageFlags> m_stageFlagsTarget;
		EnumFlags<AccessFlags> m_accessFlagsSource;
		EnumFlags<AccessFlags> m_accessFlagsTarget;
		EnumFlags<DependencyFlags> m_dependencyFlags;
	};
}
