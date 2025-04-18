#pragma once

#include <Common/Threading/Jobs/JobRunnerThread.h>

#include <Renderer/Threading/JobRunnerData.h>

namespace ngine
{
	struct Engine;
}

namespace ngine::Threading
{
	struct EngineJobManager;

	struct EngineJobRunnerThread final : public JobRunnerThread
	{
		using JobRunnerThread::JobRunnerThread;
		virtual ~EngineJobRunnerThread() = default;

		[[nodiscard]] Rendering::JobRunnerData& GetRenderData()
		{
			return m_renderData;
		}
		[[nodiscard]] const Rendering::JobRunnerData& GetRenderData() const
		{
			return m_renderData;
		}

		[[nodiscard]] PURE_STATICS static Optional<EngineJobRunnerThread*> GetCurrent()
		{
			Optional<JobRunnerThread*> pBaseType = JobRunnerThread::GetCurrent();
			return static_cast<EngineJobRunnerThread*>(pBaseType.Get());
		}

		[[nodiscard]] EngineJobManager& GetJobManager() const;
	protected:
		friend Rendering::JobRunnerData;
		Rendering::JobRunnerData m_renderData;
	};
}
