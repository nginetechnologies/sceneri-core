// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT


#pragma once

#include <Physics/Body/Body.h>
#include <Physics/Body/BodyManager.h>
#include <Physics/PhysicsLock.h>
#include <Core/Mutex.h>

JPH_NAMESPACE_BEGIN

/// Base class interface for locking a body. Usually you will use BodyLockRead / BodyLockWrite / BodyLockMultiRead / BodyLockMultiWrite instead.
class BodyLockInterface : public NonCopyable
{
public:
	/// Redefine MutexMask
	using MutexMask = BodyManager::MutexMask;

	/// Constructor
	explicit					BodyLockInterface(BodyManager &inBodyManager)		: mBodyManager(inBodyManager) { }
	virtual						~BodyLockInterface() = default;

	///@name Locking functions
	///@{
	virtual SharedMutex *		LockRead(const BodyID &inBodyID) const = 0;
	virtual void				UnlockRead(SharedMutex *inMutex) const = 0;
	virtual SharedMutex *		LockWrite(const BodyID &inBodyID) const = 0;
	virtual void				UnlockWrite(SharedMutex *inMutex) const = 0;
	///@}

	/// Get the mask needed to lock all bodies
	inline MutexMask			GetAllBodiesMutexMask() const
	{
		return mBodyManager.GetAllBodiesMutexMask();
	}

	///@name Batch locking functions
	///@{
	virtual MutexMask			GetMutexMask(const BodyID *inBodies, int inNumber) const = 0;
	virtual MutexMask			GetMutexMask(const BodyMask& bodies) const = 0;
	virtual void				LockRead(MutexMask inMutexMask) const = 0;
	virtual void				UnlockRead(MutexMask inMutexMask) const = 0;
	virtual void				LockWrite(MutexMask inMutexMask) const = 0;
	virtual void				UnlockWrite(MutexMask inMutexMask) const = 0;
	///@}

	bool						IsLocked(const BodyID &inBodyID) const 
	{ 
		if (PhysicsLock::sCheckIsLocked(EPhysicsLockTypes::PerBody))
		{
			return true;
		}
		else
		{
			SharedMutex& bodyMutex = mBodyManager.GetMutexForBody(inBodyID);
			return bodyMutex.is_locked();
		}
	}

	/// Convert body ID to body
	inline Body *				TryGetBody(const BodyID &inBodyID) const			{ return mBodyManager.TryGetBody(inBodyID); }

protected:
	BodyManager &				mBodyManager;
};

/// Implementation that performs no locking (assumes the lock has already been taken)
class BodyLockInterfaceNoLock final : public BodyLockInterface
{
public:
	using BodyLockInterface::BodyLockInterface;

	///@name Locking functions
	virtual SharedMutex *		LockRead(const BodyID &inBodyID) const override		{ return nullptr; }
	virtual void				UnlockRead(SharedMutex *inMutex) const override		{ /* Nothing to do */ }
	virtual SharedMutex *		LockWrite(const BodyID &inBodyID) const override	{ return nullptr; }
	virtual void				UnlockWrite(SharedMutex *inMutex) const override	{ /* Nothing to do */ }

	///@name Batch locking functions
	virtual MutexMask			GetMutexMask(const BodyID *inBodies, int inNumber) const override { return 0; }
	virtual MutexMask			GetMutexMask(const BodyMask& bodies) const override { return 0; }
	virtual void				LockRead(MutexMask inMutexMask) const override		{ /* Nothing to do */ }
	virtual void				UnlockRead(MutexMask inMutexMask) const override	{ /* Nothing to do */ }
	virtual void				LockWrite(MutexMask inMutexMask) const override		{ /* Nothing to do */ }
	virtual void				UnlockWrite(MutexMask inMutexMask) const override	{ /* Nothing to do */ }
};

/// Implementation that uses the body manager to lock the correct mutex for a body
class BodyLockInterfaceLocking final : public BodyLockInterface
{
public:
	using BodyLockInterface::BodyLockInterface;

	///@name Locking functions
	virtual SharedMutex *		LockRead(const BodyID &inBodyID) const override
	{
		SharedMutex &mutex = mBodyManager.GetMutexForBody(inBodyID);
		PhysicsLock::sLockShared(mutex, EPhysicsLockTypes::PerBody);
		return &mutex;
	}

	virtual void				UnlockRead(SharedMutex *inMutex) const override
	{
		PhysicsLock::sUnlockShared(*inMutex, EPhysicsLockTypes::PerBody);
	}
	
	virtual SharedMutex *		LockWrite(const BodyID &inBodyID) const override
	{
		SharedMutex &mutex = mBodyManager.GetMutexForBody(inBodyID);
		PhysicsLock::sLock(mutex, EPhysicsLockTypes::PerBody);
		return &mutex;
	}

	virtual void				UnlockWrite(SharedMutex *inMutex) const override
	{
		PhysicsLock::sUnlock(*inMutex, EPhysicsLockTypes::PerBody);
	}

	///@name Batch locking functions
	virtual MutexMask			GetMutexMask(const BodyID *inBodies, int inNumber) const override	
	{ 
		return mBodyManager.GetMutexMask(inBodies, inNumber);
	}

	virtual MutexMask			GetMutexMask(const BodyMask& bodies) const override	
	{ 
		return mBodyManager.GetMutexMask(bodies);
	}

	virtual void				LockRead(MutexMask inMutexMask) const override
	{
		mBodyManager.LockRead(inMutexMask);
	}

	virtual void				UnlockRead(MutexMask inMutexMask) const override
	{
		mBodyManager.UnlockRead(inMutexMask);
	}

	virtual void				LockWrite(MutexMask inMutexMask) const override
	{
		mBodyManager.LockWrite(inMutexMask);
	}

	virtual void				UnlockWrite(MutexMask inMutexMask) const override
	{
		mBodyManager.UnlockWrite(inMutexMask);
	}
};

JPH_NAMESPACE_END
