#pragma once

#include "3rdparty/jolt/Jolt.h"
#include "3rdparty/jolt/Core/JobSystem.h"
#include "3rdparty/jolt/Core/FixedSizeFreeList.h"

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/Mutexes/ConditionVariable.h>

namespace ngine::Threading
{
	struct JobManager;
}

namespace ngine::Physics
{
	struct Barrier;
	struct Semaphore
	{
		Semaphore();
		~Semaphore();

		/// Get the current value of the semaphore
		[[nodiscard]] int GetValue() const
		{
			return m_count;
		}

		void Acquire(uint32 inNumber = 1);
		void Release(uint32 inNumber = 1);
	private:
#define USE_WINDOWS_SEMAPHORE PLATFORM_WINDOWS

#if USE_WINDOWS_SEMAPHORE
		// On windows we use a semaphore object since it is more efficient than a lock and a condition variable
		alignas(JPH_CACHE_LINE_SIZE) Threading::Atomic<int> m_count{0
		};                 ///< We increment mCount for every release, to acquire we decrement the count. If
		                   ///< the count is negative we know that we are waiting on the actual semaphore.
		void* m_semaphore; ///< The semaphore is an expensive construct so we only acquire/release it if we know that we need to wait/have
		                   ///< waiting threads
#else
		// Other platforms: Emulate a semaphore using a mutex, condition variable and count
		Threading::Mutex m_lock;
		Threading::ConditionVariable m_waitVariable;
		int m_count = 0;
#endif
	};

	struct Barrier final : public JPH::JobSystem::Barrier
	{
		Barrier();
		virtual ~Barrier();

		virtual void AddJob(const JPH::JobHandle& inJob) override;
		virtual void AddJobs(const JPH::JobHandle* inHandles, uint32 inNumHandles) override;
		virtual void OnJobFinished(JPH::JobSystem::Job* inJob) override;

		/// Check if there are any jobs in the job barrier
		inline bool IsEmpty() const
		{
			return m_jobReadIndex == m_jobWriteIndex;
		}

		void Wait();

		/// Flag to indicate if a barrier has been handed out
		std::atomic<bool> m_isInUse{false};
	protected:
		/// Jobs queue for the barrier
		static constexpr uint32 cMaxJobs = 1024;
		static_assert(JPH::IsPowerOf2(cMaxJobs));           // We do bit operations and require max jobs to be a power of 2
		std::atomic<JPH::JobSystem::Job*> m_jobs[cMaxJobs]; ///< List of jobs that are part of this barrier, nullptrs for empty slots
		alignas(JPH_CACHE_LINE_SIZE) std::atomic<uint32> m_jobReadIndex{0
		}; ///< First job that could be valid (modulo cMaxJobs), can be nullptr if other thread is still working on adding the job
		alignas(JPH_CACHE_LINE_SIZE) std::atomic<uint32> m_jobWriteIndex{0}; ///< First job that can be written (modulo cMaxJobs)
		std::atomic<int> m_numToAcquire{0}; ///< Number of times the semaphore has been released, the barrier should acquire the semaphore this
		                                    ///< many times (written at the same time as m_jobWriteIndex so ok to put in same cache line)
		Semaphore m_semaphore;              ///< Semaphore used by finishing jobs to signal the barrier that they're done
	};

	struct Job final : public JPH::JobSystem::Job, public Threading::Job
	{
		Job(
			const char* inJobName,
			JPH::ColorArg inColor,
			JPH::JobSystem* inJobSystem,
			const JPH::JobSystem::JobFunction& inJobFunction,
			uint32 inNumDependencies
		);
		virtual ~Job();

		[[nodiscard]] virtual Result OnExecute(Threading::JobRunnerThread& thread) override;
		virtual void OnFinishedExecution([[maybe_unused]] Threading::JobRunnerThread& thread) override;
	};

	struct JobSystem final : public JPH::JobSystem
	{
		JobSystem(Threading::JobManager& jobManager);
		virtual ~JobSystem() = default;

		virtual int GetMaxConcurrency() const override;

		virtual JobHandle
		CreateJob(const char* inName, JPH::ColorArg inColor, const JobFunction& inJobFunction, uint32 inNumDependencies = 0) override;

		virtual Barrier* CreateBarrier() override;
		virtual void DestroyBarrier(Barrier* inBarrier) override;
		virtual void WaitForJobs(Barrier* inBarrier) override;
	protected:
		virtual void QueueJob(Job* inJob) override;
		virtual void QueueJobs(Job** inJobs, uint32 inNumJobs) override;
		virtual void FreeJob(Job* inJob) override;
	protected:
		Threading::JobManager& m_jobManager;

		/// Array of jobs (fixed size)
		using AvailableJobs = JPH::FixedSizeFreeList<ngine::Physics::Job>;
		AvailableJobs m_availableJobs;

		Array<ngine::Physics::Barrier, 8> m_barriers;
	};
}
