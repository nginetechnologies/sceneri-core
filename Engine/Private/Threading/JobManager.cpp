#include <Engine/Threading/JobManager.h>
#include <Engine/Threading/JobRunnerThread.h>

#include <Common/Memory/OffsetOf.h>
#include <Common/System/Query.h>

namespace ngine::Threading
{
	EngineJobManager::EngineJobManager()
		: JobManager()
	{
		StartRunners();
	}

	void EngineJobManager::CreateJobRunners(const uint16 count)
	{
		m_pJobThreadData = Memory::AllocateAligned(count * sizeof(Threading::EngineJobRunnerThread), alignof(Threading::EngineJobRunnerThread));
		char* pData = reinterpret_cast<char*>(m_pJobThreadData);
		for (uint16 i = 0; i < count; ++i)
		{
			new (pData) Threading::EngineJobRunnerThread(*this);
			m_jobThreads.EmplaceBack(*reinterpret_cast<Threading::JobRunnerThread*>(pData));
			pData += sizeof(Threading::EngineJobRunnerThread);
		}
	}

	void EngineJobManager::DestroyJobRunners()
	{
		for (Threading::JobRunnerThread& thread : m_jobThreads)
		{
			static_cast<Threading::EngineJobRunnerThread&>(thread).~EngineJobRunnerThread();
		}

		Memory::DeallocateAligned(m_pJobThreadData, sizeof(Threading::JobRunnerThread));
	}

	EngineJobManager& EngineJobRunnerThread::GetJobManager() const
	{
		return static_cast<EngineJobManager&>(JobRunnerThread::GetJobManager());
	}
}
