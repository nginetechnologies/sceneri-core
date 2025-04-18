#include "JobSystem.h"

#include <Common/Threading/Jobs/JobManager.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>

#include <Common/Platform/Windows.h>
#include <thread>
#include <chrono>

#include <3rdparty/jolt/Physics/PhysicsSettings.h>

namespace ngine::Physics
{
	using namespace std::literals::chrono_literals;

	/// When we switch to C++20 we can use counting_semaphore to unify this
	Semaphore::Semaphore()
#if USE_WINDOWS_SEMAPHORE
		: m_semaphore(CreateSemaphore(nullptr, 0, INT_MAX, nullptr))
#endif
	{
	}

	Semaphore::~Semaphore()
	{
#if USE_WINDOWS_SEMAPHORE
		CloseHandle(m_semaphore);
#endif
	}

	/// Release the semaphore, signalling the thread waiting on the barrier that there may be work
	void Semaphore::Release(uint32 inNumber)
	{
		Assert(inNumber > 0);

#if USE_WINDOWS_SEMAPHORE
		int old_value = m_count.FetchAdd(inNumber);
		if (old_value < 0)
		{
			int new_value = old_value + (int)inNumber;
			int num_to_release = Math::Min(new_value, 0) - old_value;
			::ReleaseSemaphore(m_semaphore, num_to_release, nullptr);
		}
#else
		Threading::UniqueLock lock(m_lock);
		m_count += (int)inNumber;
		if (inNumber > 1)
			m_waitVariable.NotifyAll();
		else
			m_waitVariable.NotifyOne();
#endif
	}

	/// Acquire the semaphore inNumber times
	void Semaphore::Acquire(uint32 inNumber)
	{
		JPH_ASSERT(inNumber > 0);

#if USE_WINDOWS_SEMAPHORE
		int old_value = m_count.FetchSubtract(inNumber);
		int new_value = old_value - (int)inNumber;
		if (new_value < 0)
		{
			int num_to_acquire = Math::Min(old_value, 0) - new_value;
			for (int i = 0; i < num_to_acquire; ++i)
				WaitForSingleObject(m_semaphore, INFINITE);
		}
#else
		Threading::UniqueLock lock(m_lock);
		m_count -= (int)inNumber;
		while (m_count < 0)
		{
			m_waitVariable.Wait(lock);
		}
#endif
	}

	// TODO: Nuke the concept of barriers, it is only used for the last WaitForJobs step in Jolt
	// We can fix this with our dependencies instead, and thus await waiting at all.
	Barrier::Barrier()
	{
		for (std::atomic<JPH::JobSystem::Job*>& j : m_jobs)
		{
			j = nullptr;
		}
	}
	Barrier::~Barrier()
	{
		JPH_ASSERT(IsEmpty());
	}

	void Barrier::AddJob(const JPH::JobHandle& inJob)
	{
		JPH_PROFILE_FUNCTION();

		bool release_semaphore = false;

		// Set the barrier on the job, this returns true if the barrier was successfully set (otherwise the job is already done and we don't
		// need to add it to our list)
		JPH::JobSystem::Job* job = inJob.GetPtr();
		if (job->SetBarrier(this))
		{
			// If the job can be executed we want to release the semaphore an extra time to allow the waiting thread to start executing it
			m_numToAcquire++;
			if (job->CanBeExecuted())
			{
				release_semaphore = true;
				m_numToAcquire++;
			}

			// Add the job to our job list
			job->AddRef();
			uint32 write_index = m_jobWriteIndex++;
			while (write_index - m_jobReadIndex >= cMaxJobs)
			{
				JPH_ASSERT(false, "Barrier full, stalling!");
				std::this_thread::sleep_for(100us);
			}
			m_jobs[write_index & (cMaxJobs - 1)] = job;
		}

		// Notify waiting thread that a new executable job is available
		if (release_semaphore)
			m_semaphore.Release();
	}

	void Barrier::AddJobs(const JPH::JobHandle* inHandles, uint32 inNumHandles)
	{
		JPH_PROFILE_FUNCTION();

		bool release_semaphore = false;

		for (const JPH::JobHandle *handle = inHandles, *handles_end = inHandles + inNumHandles; handle < handles_end; ++handle)
		{
			// Set the barrier on the job, this returns true if the barrier was successfully set (otherwise the job is already done and we don't
			// need to add it to our list)
			JPH::JobSystem::Job* job = handle->GetPtr();
			if (job->SetBarrier(this))
			{
				// If the job can be executed we want to release the semaphore an extra time to allow the waiting thread to start executing it
				m_numToAcquire++;
				if (!release_semaphore && job->CanBeExecuted())
				{
					release_semaphore = true;
					m_numToAcquire++;
				}

				// Add the job to our job list
				job->AddRef();
				uint32 write_index = m_jobWriteIndex++;
				while (write_index - m_jobReadIndex >= cMaxJobs)
				{
					JPH_ASSERT(false, "Barrier full, stalling!");
					std::this_thread::sleep_for(100us);
				}
				m_jobs[write_index & (cMaxJobs - 1)] = job;
			}
		}

		// Notify waiting thread that a new executable job is available
		if (release_semaphore)
			m_semaphore.Release();
	}

	void Barrier::OnJobFinished([[maybe_unused]] JPH::JobSystem::Job* inJob)
	{
		JPH_PROFILE_FUNCTION();

		m_semaphore.Release();
	}

	/// Wait for all jobs in this job barrier, while waiting, execute jobs that are part of this barrier on the current thread
	void Barrier::Wait()
	{
		while (m_numToAcquire > 0)
		{
			{
				JPH_PROFILE("Execute Jobs");

				// Go through all jobs
				bool has_executed;
				do
				{
					has_executed = false;

					// Loop through the jobs and erase jobs from the beginning of the list that are done
					while (m_jobReadIndex < m_jobWriteIndex)
					{
						std::atomic<JPH::JobSystem::Job*>& job = m_jobs[m_jobReadIndex & (cMaxJobs - 1)];
						JPH::JobSystem::Job* job_ptr = job.load();
						if (job_ptr == nullptr || !job_ptr->IsDone())
							break;

						// Job is finished, release it
						job_ptr->Release();
						job = nullptr;
						++m_jobReadIndex;
					}

					// Loop through the jobs and execute the first executable job
					for (uint32 index = m_jobReadIndex; index < m_jobWriteIndex; ++index)
					{
						std::atomic<JPH::JobSystem::Job*>& job = m_jobs[index & (cMaxJobs - 1)];
						JPH::JobSystem::Job* job_ptr = job.load();
						if (job_ptr != nullptr && job_ptr->CanBeExecuted())
						{
							// This will only execute the job if it has not already executed
							job_ptr->Execute();
							has_executed = true;
							break;
						}
					}

				} while (has_executed);
			}

			// Wait for another thread to wake us when either there is more work to do or when all jobs have completed
			int num_to_acquire = Math::Max(
				1,
				m_semaphore.GetValue()
			); // When there have been multiple releases, we acquire them all at the same time to avoid needlessly spinning on executing jobs
			m_semaphore.Acquire(num_to_acquire);
			m_numToAcquire -= num_to_acquire;
		}

		// All jobs should be done now, release them
		while (m_jobReadIndex < m_jobWriteIndex)
		{
			std::atomic<JPH::JobSystem::Job*>& job = m_jobs[m_jobReadIndex & (cMaxJobs - 1)];
			JPH::JobSystem::Job* job_ptr = job.load();
			JPH_ASSERT(job_ptr != nullptr && job_ptr->IsDone());
			job_ptr->Release();
			job = nullptr;
			++m_jobReadIndex;
		}
	}

	int JobSystem::GetMaxConcurrency() const
	{
		return m_jobManager.GetJobThreads().GetSize();
	}

	JPH::JobHandle
	JobSystem::CreateJob(const char* inJobName, JPH::ColorArg inColor, const JobFunction& inJobFunction, uint32 inNumDependencies)
	{
		JPH_PROFILE_FUNCTION();

		// Loop until we can get a job from the free list
		uint32 index;
		for (;;)
		{
			index = m_availableJobs.ConstructObject(inJobName, inColor, this, inJobFunction, inNumDependencies);
			if (index != AvailableJobs::cInvalidObjectIndex)
				break;
			JPH_ASSERT(false, "No jobs available!");
			std::this_thread::sleep_for(100us);
		}
		ngine::Physics::Job* job = &m_availableJobs.Get(index);

		// Construct handle to keep a reference, the job is queued below and may immediately complete
		JobHandle handle(job);

		// If there are no dependencies, queue the job now
		if (inNumDependencies == 0)
			QueueJob(job);

		// Return the handle
		return handle;
	}

	JobSystem::JobSystem(Threading::JobManager& jobManager)
		: m_jobManager(jobManager)
	{
		// Init freelist of jobs
		m_availableJobs.Init(JPH::cMaxPhysicsJobs, JPH::cMaxPhysicsJobs);
	}

	void JobSystem::FreeJob(Job* inJob)
	{
		m_availableJobs.DestructObject(static_cast<ngine::Physics::Job*>(inJob));
	}

	JobSystem::Barrier* JobSystem::CreateBarrier()
	{
		JPH_PROFILE_FUNCTION();

		// Find the first unused barrier
		for (ngine::Physics::Barrier& barrier : m_barriers)
		{
			bool expected = false;
			if (barrier.m_isInUse.compare_exchange_strong(expected, true))
				return &barrier;
		}

		return nullptr;
	}

	void JobSystem::DestroyBarrier(Barrier* inBarrier)
	{
		JPH_PROFILE_FUNCTION();

		// Check that no jobs are in the barrier
		JPH_ASSERT(static_cast<ngine::Physics::Barrier*>(inBarrier)->IsEmpty());

		// Flag the barrier as unused
		bool expected = true;
		static_cast<ngine::Physics::Barrier*>(inBarrier)->m_isInUse.compare_exchange_strong(expected, false);
		JPH_ASSERT(expected);
	}

	void JobSystem::WaitForJobs(JPH::JobSystem::Barrier* inBarrier)
	{
		JPH_PROFILE_FUNCTION();

		// Let our barrier implementation wait for the jobs
		static_cast<ngine::Physics::Barrier*>(inBarrier)->Wait();
	}

	void JobSystem::QueueJob(Job* inJob)
	{
		ngine::Physics::Job& job = static_cast<ngine::Physics::Job&>(*inJob);
		job.AddRef();
		// Assert(inJob->CanBeExecuted());
		// Assert(!inJob->IsDone());
		job.Queue(m_jobManager);
	}

	void JobSystem::QueueJobs(Job** inJobs, uint32 inNumJobs)
	{
		Threading::JobRunnerThread& currentThread = *Threading::JobRunnerThread::GetCurrent();

		FlatVector<ReferenceWrapper<Threading::Job>, 1024> jobs;
		for (Job* pJob : ArrayView<Job*>{inJobs, inNumJobs})
		{
			pJob->AddRef();
			// Assert(pJob->CanBeExecuted());
			// Assert(!pJob->IsDone());
			jobs.EmplaceBack(static_cast<ngine::Physics::Job&>(*pJob));
		}

		currentThread.QueueJobsFromThread(jobs);
	}

	Job::Job(
		const char* inJobName,
		JPH::ColorArg inColor,
		JPH::JobSystem* inJobSystem,
		const JPH::JobSystem::JobFunction& inJobFunction,
		uint32 inNumDependencies
	)
		: JPH::JobSystem::Job(inJobName, inColor, inJobSystem, inJobFunction, inNumDependencies)
		, Threading::Job(Threading::Job::Priority::Physics)
	{
	}

	Job::~Job()
	{
	}

	Threading::Job::Result Job::OnExecute(Threading::JobRunnerThread&)
	{
		JPH::JobSystem::Job::Execute();
		return Result::Finished;
	}

	void Job::OnFinishedExecution(Threading::JobRunnerThread&)
	{
		JPH::JobSystem::Job::Release();
	}
}
