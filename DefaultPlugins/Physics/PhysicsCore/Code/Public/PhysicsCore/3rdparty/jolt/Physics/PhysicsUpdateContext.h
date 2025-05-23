// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Physics/Body/BodyPair.h>
#include <Physics/Collision/ContactListener.h>
#include <Physics/Collision/BroadPhase/BroadPhase.h>
#include <Core/StaticArray.h>
#include <Core/JobSystem.h>
#include <Core/STLTempAllocator.h>

JPH_NAMESPACE_BEGIN

class PhysicsSystem;
class IslandBuilder;
class Constraint;
class TempAllocator;

/// Information used during the Update call
class PhysicsUpdateContext : public NonCopyable
{
public:
	/// Destructor
	explicit PhysicsUpdateContext(TempAllocator& inTempAllocator);
	~PhysicsUpdateContext();

	static constexpr int cMaxConcurrency = 32; ///< Maximum supported amount of concurrent jobs
	static constexpr int cMaxSubSteps = 4;     ///< Maximum supported amount of integration sub steps

	using JobHandleArray = StaticArray<JobHandle, cMaxConcurrency>;

	struct Step;

	/// Structure that contains job handles for each integration sub step
	struct SubStep
	{
		Step* mStep; ///< Step that this substeb belongs to

		bool mIsFirst;     ///< If this is the first substep in the step
		bool mIsLast;      ///< If this is the last substep in the step
		bool mIsLastOfAll; ///< If this is the last substep in the last step

		atomic<uint32> mSolveVelocityConstraintsNextIsland{
			0}; ///< Next island that needs to be processed for the solve velocity constraints step (doesn't need own cache line since position
		      ///< jobs don't run at same time)
		atomic<uint32> mSolvePositionConstraintsNextIsland{
			0}; ///< Next island that needs to be processed for the solve position constraints step (doesn't need own cache line since velocity
		      ///< jobs don't run at same time)

		/// Contains the information needed to cast a body through the scene to do continuous collision detection
		struct CCDBody
		{
			CCDBody(BodyID inBodyID1, Vec3Arg inDeltaPosition, float inLinearCastThresholdSq, float inMaxPenetration)
				: mDeltaPosition(inDeltaPosition)
				, mBodyID1(inBodyID1)
				, mLinearCastThresholdSq(inLinearCastThresholdSq)
				, mMaxPenetration(inMaxPenetration)
			{
			}

			Vec3 mDeltaPosition;            ///< Desired rotation step
			Vec3 mContactNormal;            ///< World space normal of closest hit (only valid if mFractionPlusSlop < 1)
			Vec3 mContactPointOn2;          ///< World space contact point on body 2 of closest hit (only valid if mFractionPlusSlop < 1)
			BodyID mBodyID1;                ///< Body 1 (the body that is performing collision detection)
			BodyID mBodyID2;                ///< Body 2 (the body of the closest hit, only valid if mFractionPlusSlop < 1)
			float mFraction = 1.0f;         ///< Fraction at which the hit occurred
			float mFractionPlusSlop = 1.0f; ///< Fraction at which the hit occurred + extra delta to allow body to penetrate by mMaxPenetration
			float mLinearCastThresholdSq;   ///< Maximum allowed squared movement before doing a linear cast (determined by inner radius of shape)
			float mMaxPenetration;          ///< Maximum allowed penetration (determined by inner radius of shape)
			ContactSettings mContactSettings; ///< The contact settings for this contact
		};
		atomic<uint32> mIntegrateVelocityReadIdx{0}; ///< Next active body index to take when integrating velocities
		CCDBody* mCCDBodies = nullptr;               ///< List of bodies that need to do continuous collision detection
		uint32 mCCDBodiesCapacity = 0;               ///< Capacity of the mCCDBodies list
		atomic<uint32> mNumCCDBodies = 0;            ///< Number of CCD bodies in mCCDBodies
		atomic<uint32> mNextCCDBody{0};              ///< Next unprocessed body index in mCCDBodies
		int* mActiveBodyToCCDBody = nullptr;         ///< A mapping between an index in BodyManager::mActiveBodies and the index in mCCDBodies
		uint32 mNumActiveBodyToCCDBody = 0;          ///< Number of indices in mActiveBodyToCCDBody

		JobHandleArray mSolveVelocityConstraints; ///< Solve the constraints in the velocity domain
		JobHandle mPreIntegrateVelocity;          ///< Setup integration of all body positions
		JobHandleArray mIntegrateVelocity;        ///< Integrate all body positions
		JobHandle mPostIntegrateVelocity;         ///< Finalize integration of all body positions
		JobHandle mResolveCCDContacts; ///< Updates the positions and velocities for all bodies that need continuous collision detection
		JobHandleArray mSolvePositionConstraints; ///< Solve all constraints in the position domain
		JobHandle mStartNextSubStep;              ///< Trampoline job that either kicks the next sub step or the next step
	};

	using SubSteps = StaticArray<SubStep, cMaxSubSteps>;

	struct BodyPairQueue
	{
		atomic<uint32> mWriteIdx{
			0}; ///< Next index to write in mBodyPair array (need to add thread index * mMaxBodyPairsPerQueue and modulo mMaxBodyPairsPerQueue)
		uint8 mPadding1[JPH_CACHE_LINE_SIZE - sizeof(atomic<uint32>)]; ///< Moved to own cache line to avoid conflicts with consumer jobs

		atomic<uint32> mReadIdx{
			0}; ///< Next index to read in mBodyPair array (need to add thread index * mMaxBodyPairsPerQueue and modulo mMaxBodyPairsPerQueue)
		uint8
			mPadding2[JPH_CACHE_LINE_SIZE - sizeof(atomic<uint32>)]; ///< Moved to own cache line to avoid conflicts with producer/consumer jobs
	};

	using BodyPairQueues = StaticArray<BodyPairQueue, cMaxConcurrency>;

	using JobMask = uint32; ///< A mask that has as many bits as we can have concurrent jobs
	static_assert(sizeof(JobMask) * 8 >= cMaxConcurrency);

	/// Structure that contains data needed for each collision step.
	struct Step
	{
		Step() = default;
		Step(const Step&)
		{
			JPH_ASSERT(false);
		} // vector needs a copy constructor, but we're never going to call it

		PhysicsUpdateContext* mContext; ///< The physics update context

		BroadPhase::UpdateState mBroadPhaseUpdateState; ///< Handle returned by Broadphase::UpdatePrepare

		uint32 mNumActiveBodiesAtStepStart; ///< Number of bodies that were active at the start of the physics update step. Only these bodies
		                                    ///< will receive gravity (they are the first N in the active body list).

		atomic<uint32> mConstraintReadIdx{0};                          ///< Next constraint for determine active constraints
		uint8 mPadding1[JPH_CACHE_LINE_SIZE - sizeof(atomic<uint32>)]; ///< Padding to avoid sharing cache line with the next atomic

		atomic<uint32> mNumActiveConstraints{0};                       ///< Number of constraints in the mActiveConstraints array
		uint8 mPadding2[JPH_CACHE_LINE_SIZE - sizeof(atomic<uint32>)]; ///< Padding to avoid sharing cache line with the next atomic

		atomic<uint32> mStepListenerReadIdx{0};                        ///< Next step listener to call
		uint8 mPadding3[JPH_CACHE_LINE_SIZE - sizeof(atomic<uint32>)]; ///< Padding to avoid sharing cache line with the next atomic

		atomic<uint32> mActiveBodyReadIdx{0}; ///< Index of fist active body that has not yet been processed by the broadphase
		uint8 mPadding4[JPH_CACHE_LINE_SIZE - sizeof(atomic<uint32>)]; ///< Padding to avoid sharing cache line with the next atomic

		BodyPairQueues mBodyPairQueues; ///< Queues in which to put body pairs that need to be tested by the narrowphase

		uint32 mMaxBodyPairsPerQueue; ///< Amount of body pairs that we can queue per queue

		atomic<JobMask> mActiveFindCollisionJobs; ///< A bitmask that indicates which jobs are still active

		atomic<uint> mNumBodyPairs{0}; ///< The number of body pairs found in this step (used to size the contact cache in the next step)
		atomic<uint> mNumManifolds{0}; ///< The number of manifolds found in this step (used to size the contact cache in the next step)

		// Jobs in order of execution (some run in parallel)
		JobHandle mBroadPhasePrepare;               ///< Prepares the new tree in the background
		JobHandleArray mStepListeners;              ///< Listeners to notify of the beginning of a physics step
		JobHandleArray mDetermineActiveConstraints; ///< Determine which constraints will be active during this step
		JobHandleArray mFindCollisions;             ///< Find all collisions between active bodies an the world
		JobHandle mUpdateBroadphaseFinalize;        ///< Swap the newly built tree with the current tree
		JobHandle mSetupVelocityConstraints;        ///< Calculate properties for all constraints in the constraint manager
		JobHandle mBuildIslandsFromConstraints;     ///< Go over all constraints and assign the bodies they're attached to to an island
		JobHandle mFinalizeIslands;                 ///< Finalize calculation simulation islands
		JobHandle mBodySetIslandIndex; ///< Set the current island index on each body (not used by the simulation, only for drawing purposes)
		SubSteps mSubSteps;            ///< Integration sub steps
		JobHandle mContactRemovedCallbacks; ///< Calls the contact removed callbacks
		JobHandle mStartNextStep;           ///< Job that kicks the next step (empty for the last step)
	};

	using Steps = vector<Step, STLTempAllocator<Step>>;

	/// Maximum amount of concurrent jobs on this machine
	int GetMaxConcurrency() const
	{
		const int max_concurrency = PhysicsUpdateContext::cMaxConcurrency;
		return min(max_concurrency, mJobSystem->GetMaxConcurrency());
	} ///< Need to put max concurrency in temp var as min requires a reference

	PhysicsSystem* mPhysicsSystem; ///< The physics system we belong to
	TempAllocator* mTempAllocator; ///< Temporary allocator used during the update
	JobSystem* mJobSystem;         ///< Job system that processes jobs
	JobSystem::Barrier* mBarrier;  ///< Barrier used to wait for all physics jobs to complete

	float mStepDeltaTime;         ///< Delta time for a simulation step (collision step)
	float mSubStepDeltaTime;      ///< Delta time for a simulation sub step (integration step)
	float mWarmStartImpulseRatio; ///< Ratio of this step delta time vs last step

	Constraint** mActiveConstraints =
		nullptr; ///< Constraints that were active at the start of the physics update step (activating bodies can activate constraints and we
	           ///< need a consistent snapshot). Only these constraints will be resolved.

	BodyPair* mBodyPairs = nullptr; ///< A list of body pairs found by the broadphase

	IslandBuilder* mIslandBuilder; ///< Keeps track of connected bodies and builds islands for multithreaded velocity/position update

	Steps mSteps;
};

JPH_NAMESPACE_END
