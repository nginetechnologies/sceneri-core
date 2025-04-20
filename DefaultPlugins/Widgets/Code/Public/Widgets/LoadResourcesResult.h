#pragma once

#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Widgets
{
	struct LoadResourcesResult
	{
		enum class Status : uint8
		{
			FailedRetryLater,
			QueueJobsAndTryAgain,
			Invalid,
			Valid
		};

		LoadResourcesResult(const Status status)
			: m_status(status)
		{
		}
		LoadResourcesResult(Threading::JobBatch&& jobBatch)
			: m_status(Status::QueueJobsAndTryAgain)
			, m_jobBatch(Forward<Threading::JobBatch>(jobBatch))
		{
		}

		[[nodiscard]] bool IsLoaded() const;

		Status m_status = Status::FailedRetryLater;
		Threading::JobBatch m_jobBatch;
	};
}
