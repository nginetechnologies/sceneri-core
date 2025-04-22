#include <Common/Platform/DllExport.h>
#include <Common/Memory/UniqueRef.h>
#include <Common/Project System/ProjectInfo.h>
#include <Common/Project System/PluginDatabase.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <Engine/EngineSystems.h>

namespace ngine
{
	using namespace ngine::Asset::Literals;
	inline static constexpr ngine::Asset::Guid launcherPluginGuid = "84cb6ecd-1f00-4178-8434-5853cebb8601"_asset;
}

ngine::UniquePtr<ngine::EngineSystems> CreateEngine(const ngine::CommandLine::InitializationParameters& commandLineParameters)
{
	using namespace ngine;
	UniquePtr<EngineSystems> pEngineSystems = UniquePtr<EngineSystems>::Make(commandLineParameters);
	Threading::JobBatch loadDefaultResourcesBatch = pEngineSystems->m_engine.LoadDefaultResources();
	Threading::IntermediateStage& finishedPluginLoadStage = Threading::CreateIntermediateStage();
	loadDefaultResourcesBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
		[&engine = pEngineSystems->m_engine, &jobManager = pEngineSystems->m_jobManager, &finishedPluginLoadStage](Threading::JobRunnerThread&)
		{
			Engine::PluginLoadResult pluginLoadResult = engine.LoadPlugin(ngine::launcherPluginGuid);
			Assert(pluginLoadResult.pPluginInstance.IsValid());
			if (LIKELY(pluginLoadResult.pPluginInstance.IsValid()))
			{
				pluginLoadResult.jobBatch.QueueAsNewFinishedStage(finishedPluginLoadStage);
				jobManager.Queue(pluginLoadResult.jobBatch, Threading::JobPriority::LoadPlugin);
			}
		},
		Threading::JobPriority::LoadPlugin,
		"Load Launcher plug-in"
	));
	loadDefaultResourcesBatch.QueueAsNewFinishedStage(finishedPluginLoadStage);
	pEngineSystems->m_startupJobBatch.QueueAfterStartStage(loadDefaultResourcesBatch);

	return pEngineSystems;
}

#include "../../Common/Main.inl"
