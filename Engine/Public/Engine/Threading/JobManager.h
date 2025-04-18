#pragma once

#include <Common/Threading/Jobs/JobManager.h>

namespace ngine
{
	struct Engine;
}

namespace ngine::Threading
{
	struct EngineJobRunnerThread;

	struct EngineJobManager final : public JobManager
	{
		EngineJobManager();
		using JobManager::JobManager;
		virtual ~EngineJobManager() = default;
	protected:
		virtual void CreateJobRunners(const uint16 count) override;
		virtual void DestroyJobRunners() override;
	};
}
