// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Core/Profiler.h>
#include <Core/NonCopyable.h>

JPH_SUPPRESS_WARNINGS_STD_BEGIN
#include <mutex>
#include <shared_mutex>
#include <thread>
JPH_SUPPRESS_WARNINGS_STD_END

JPH_NAMESPACE_BEGIN

#ifdef JPH_PLATFORM_BLUE

// On Platform Blue the mutex class is not very fast so we implement it using the official APIs
class MutexBase : public NonCopyable
{
public:
					MutexBase()
	{
		JPH_PLATFORM_BLUE_MUTEX_INIT(mMutex);
	}

					~MutexBase()
	{
		JPH_PLATFORM_BLUE_MUTEX_DESTROY(mMutex);
	}

	inline bool		try_lock()
	{
		return JPH_PLATFORM_BLUE_MUTEX_TRYLOCK(mMutex);
	}

	inline void		lock()
	{
		JPH_PLATFORM_BLUE_MUTEX_LOCK(mMutex);
	}

	inline void		unlock()
	{
		JPH_PLATFORM_BLUE_MUTEX_UNLOCK(mMutex);
	}

private:
	JPH_PLATFORM_BLUE_MUTEX		mMutex;
};

// On Platform Blue the shared_mutex class is not very fast so we implement it using the official APIs
class SharedMutexBase : public NonCopyable
{
public:
					SharedMutexBase()
	{
		JPH_PLATFORM_BLUE_RWLOCK_INIT(mRWLock);
	}

					~SharedMutexBase()
	{
		JPH_PLATFORM_BLUE_RWLOCK_DESTROY(mRWLock);
	}

	inline bool		try_lock()
	{
		return JPH_PLATFORM_BLUE_RWLOCK_TRYWLOCK(mRWLock);
	}

	inline bool		try_lock_shared()
	{
		return JPH_PLATFORM_BLUE_RWLOCK_TRYRLOCK(mRWLock);
	}

	inline void		lock()
	{
		JPH_PLATFORM_BLUE_RWLOCK_WLOCK(mRWLock);
	}

	inline void		unlock()
	{
		JPH_PLATFORM_BLUE_RWLOCK_WUNLOCK(mRWLock);
	}

	inline void		lock_shared()
	{
		JPH_PLATFORM_BLUE_RWLOCK_RLOCK(mRWLock);
	}

	inline void		unlock_shared()
	{
		JPH_PLATFORM_BLUE_RWLOCK_RUNLOCK(mRWLock);
	}

private:
	JPH_PLATFORM_BLUE_RWLOCK	mRWLock;
};

#else

// On other platforms just use the STL implementation
using MutexBase = mutex;
using SharedMutexBase = shared_mutex;

#endif // JPH_PLATFORM_BLUE

/// Very simple wrapper around MutexBase which tracks lock contention in the profiler 
/// and asserts that locks/unlocks take place on the same thread
class Mutex : public MutexBase
{
public:
	inline bool		try_lock()
	{
		JPH_ASSERT(mLockedThreadID != this_thread::get_id());
		if (MutexBase::try_lock())
		{
			JPH_IF_ENABLE_ASSERTS(mLockedThreadID = this_thread::get_id();)
			return true;
		}
		return false;
	}

	inline void		lock()
	{
		if (!try_lock())
		{
			JPH_PROFILE("Lock", 0xff00ffff);
			MutexBase::lock();
			JPH_IF_ENABLE_ASSERTS(mLockedThreadID = this_thread::get_id();)
		}
	}

	inline void		unlock()
	{
		JPH_ASSERT(mLockedThreadID == this_thread::get_id());
		JPH_IF_ENABLE_ASSERTS(mLockedThreadID = thread::id();)
		MutexBase::unlock();
	}

	static thread::id get_current_thread_id()
	{
		static thread_local thread::id currentThreadId = this_thread::get_id();
		return currentThreadId;
	}

#ifdef JPH_ENABLE_ASSERTS
	inline bool		is_locked()
	{
	return mLockedThreadID == get_current_thread_id();
	}
#endif // JPH_ENABLE_ASSERTS

private:
	JPH_IF_ENABLE_ASSERTS(thread::id mLockedThreadID;)
};

/// Very simple wrapper around SharedMutexBase which tracks lock contention in the profiler
/// and asserts that locks/unlocks take place on the same thread
class SharedMutex : public SharedMutexBase
{
public:
	static thread::id get_current_thread_id()
	{
		static thread_local thread::id currentThreadId = this_thread::get_id();
		return currentThreadId;
	}

	inline bool		try_lock()
	{
		JPH_ASSERT(mLockedThreadID != get_current_thread_id());
		if (SharedMutexBase::try_lock())
		{
			mLockedThreadID = get_current_thread_id();
			return true;
		}
		return false;
	}

	inline void		lock()
	{
		if (!try_lock())
		{
			JPH_PROFILE("WLock", 0xff00ffff);
			SharedMutexBase::lock();
			mLockedThreadID = get_current_thread_id();
		}
	}

	inline void		unlock()
	{
		JPH_ASSERT(mLockedThreadID == get_current_thread_id());
		mLockedThreadID = thread::id();
		SharedMutexBase::unlock();
	}

	inline bool		is_locked()
	{
		return mLockedThreadID != thread::id();
	}

	inline void		lock_shared()
	{
		if (!try_lock_shared())
		{
			JPH_PROFILE("RLock", 0xff00ffff);
			SharedMutexBase::lock_shared();
		}
	}

private:
	thread::id mLockedThreadID;
};

JPH_NAMESPACE_END
