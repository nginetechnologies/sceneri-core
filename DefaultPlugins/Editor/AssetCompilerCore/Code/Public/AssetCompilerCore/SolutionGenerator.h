#pragma once

#define SUPPORT_GENERATE_SOLUTION PLATFORM_DESKTOP && !PLATFORM_APPLE_MACCATALYST
#if SUPPORT_GENERATE_SOLUTION

namespace ngine
{
	struct EngineInfo;
	struct ProjectInfo;
	struct PluginInfo;
}

#include <Common/Platform/Type.h>

namespace ngine::AssetCompiler
{
	[[nodiscard]] bool GenerateEngineSolution(const EngineInfo& engineInfo, const Platform::Type platform);
	[[nodiscard]] bool GenerateProjectCMakeLists(const ProjectInfo& projectInfo, const EngineInfo& engineInfo);
	[[nodiscard]] bool GenerateProjectSolution(
		const ProjectInfo& projectInfo, const EngineInfo& engineInfo, const bool includeEngineSource, const Platform::Type platform
	);
	[[nodiscard]] bool GeneratePluginCMakeLists(const PluginInfo& pluginInfo);
	[[nodiscard]] bool GeneratePluginSolution(const PluginInfo& pluginInfo, const EngineInfo& engineInfo, const Platform::Type platform);
}

#endif
