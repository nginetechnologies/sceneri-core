// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Core/Reference.h>
#include <Core/Result.h>
#include <Physics/Body/BodyCreationSettings.h>
#include <Physics/Constraints/TwoBodyConstraint.h>
#include <Skeleton/Skeleton.h>
#include <Skeleton/SkeletonPose.h>
#include <Physics/EActivation.h>

JPH_NAMESPACE_BEGIN

class Ragdoll;
class PhysicsSystem;

/// Contains the structure of a ragdoll
class RagdollSettings : public RefTarget<RagdollSettings>
{
public:
	JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(RagdollSettings)

	/// Stabilize the constraints of the ragdoll
	/// @return True on success, false on failure.
	bool								Stabilize();

	/// After the ragdoll has been fully configured, call this function to automatically create and add a GroupFilterTable collision filter to all bodies
	/// and configure them so that parent and children don't collide. 
	/// 
	/// This will:
	/// - Create a GroupFilterTable and assign it to all of the bodies in a ragdoll.
	/// - Each body in your ragdoll will get a SubGroupID that is equal to the joint index in the Skeleton that it is attached to.
	/// - Loop over all joints in the Skeleton and call GroupFilterTable::DisableCollision(joint index, parent joint index).
	/// - When a pose is provided through inJointMatrices the function will detect collisions between joints 
	/// (they must be separated by more than inMinSeparationDistance to be treated as not colliding) and automatically disable collisions.
	/// 
	/// When you create an instance using Ragdoll::CreateRagdoll pass in a unique GroupID for each ragdoll (e.g. a simple counter), note that this number 
	/// should be unique throughout the PhysicsSystem, so if you have different types of ragdolls they should not share the same GroupID.
	void								DisableParentChildCollisions(const Mat44 *inJointMatrices = nullptr, float inMinSeparationDistance = 0.0f);

	/// Saves the state of this object in binary form to inStream.
	/// @param inStream The stream to save the state to
	/// @param inSaveShapes If the shapes should be saved as well (these could be shared between ragdolls, in which case the calling application may want to write custom code to restore them)
	/// @param inSaveGroupFilter If the group filter should be saved as well (these could be shared)
	void								SaveBinaryState(StreamOut &inStream, bool inSaveShapes, bool inSaveGroupFilter) const;

	using RagdollResult = Result<Ref<RagdollSettings>>;

	/// Restore a saved ragdoll from inStream
	static RagdollResult				sRestoreFromBinaryState(StreamIn &inStream);

	/// Create ragdoll instance from these settings
	/// @return Newly created ragdoll or null when out of bodies
	Ragdoll *							CreateRagdoll(CollisionGroup::GroupID inCollisionGroup, uint64 inUserData, PhysicsSystem *inSystem) const;

	/// Access to the skeleton of this ragdoll
	const Skeleton *					GetSkeleton() const												{ return mSkeleton; }
	Skeleton *							GetSkeleton()													{ return mSkeleton; }

	/// Calculate the map needed for GetBodyIndexToConstraintIndex()
	void								CalculateBodyIndexToConstraintIndex();

	/// Get table that maps a body index to the constraint index with which it is connected to its parent. -1 if there is no constraint associated with the body.
	const Array<int> &					GetBodyIndexToConstraintIndex() const							{ return mBodyIndexToConstraintIndex; }

	/// Map a single body index to a constraint index
	int									GetConstraintIndexForBodyIndex(int inBodyIndex) const			{ return mBodyIndexToConstraintIndex[inBodyIndex]; }

	/// Calculate the map needed for GetConstraintIndexToBodyIdxPair()
	void								CalculateConstraintIndexToBodyIdxPair();

	using BodyIdxPair = pair<int, int>;

	/// Table that maps a constraint index (index in mConstraints) to the indices of the bodies that the constraint is connected to (index in mBodyIDs)
	const Array<BodyIdxPair> &			GetConstraintIndexToBodyIdxPair() const							{ return mConstraintIndexToBodyIdxPair; }

	/// Map a single constraint index (index in mConstraints) to the indices of the bodies that the constraint is connected to (index in mBodyIDs)
	BodyIdxPair							GetBodyIndicesForConstraintIndex(int inConstraintIndex) const	{ return mConstraintIndexToBodyIdxPair[inConstraintIndex]; }

	/// A single rigid body sub part of the ragdoll
	class Part : public BodyCreationSettings
	{
	public:
		JPH_DECLARE_SERIALIZABLE_NON_VIRTUAL(Part)

		Ref<TwoBodyConstraintSettings>	mToParent;
	};

	/// List of ragdoll parts
	using PartVector = Array<Part>;																	///< The constraint that connects this part to its parent part (should be null for the root)

	/// The skeleton for this ragdoll
	Ref<Skeleton>						mSkeleton;

	/// For each of the joints, the body and constraint attaching it to its parent body (1-on-1 with mSkeleton.GetJoints())
	PartVector							mParts;

private:
	/// Table that maps a body index (index in mBodyIDs) to the constraint index with which it is connected to its parent. -1 if there is no constraint associated with the body.
	Array<int>							mBodyIndexToConstraintIndex;

	/// Table that maps a constraint index (index in mConstraints) to the indices of the bodies that the constraint is connected to (index in mBodyIDs)
	Array<BodyIdxPair>					mConstraintIndexToBodyIdxPair;
};

/// Runtime ragdoll information
class Ragdoll : public RefTarget<Ragdoll>, public NonCopyable
{
public:
	JPH_OVERRIDE_NEW_DELETE

	/// Constructor
	explicit							Ragdoll(PhysicsSystem *inSystem) : mSystem(inSystem) { }

	/// Destructor
										~Ragdoll();

	/// Add bodies and constraints to the system and optionally activate the bodies
	void								AddToPhysicsSystem(EActivation inActivationMode, bool inLockBodies = true);

	/// Remove bodies and constraints from the system
	void								RemoveFromPhysicsSystem(bool inLockBodies = true);

	/// Wake up all bodies in the ragdoll
	void								Activate(bool inLockBodies = true);

	/// Set the group ID on all bodies in the ragdoll
	void								SetGroupID(CollisionGroup::GroupID inGroupID, bool inLockBodies = true);

	/// Set the ragdoll to a pose (calls BodyInterface::SetPositionAndRotation to instantly move the ragdoll)
	void								SetPose(const SkeletonPose &inPose, bool inLockBodies = true);
	
	/// Lower level version of SetPose that directly takes the world space joint matrices
	void								SetPose(const Mat44 *inJointMatrices, bool inLockBodies = true);

	/// Get the ragdoll pose (uses the world transform of the bodies to calculate the pose)
	void								GetPose(SkeletonPose &outPose, bool inLockBodies = true);

	/// Lower level version of GetPose that directly returns the world space joint matrices
	void								GetPose(Mat44 *outJointMatrices, bool inLockBodies = true);

	/// Drive the ragdoll to a specific pose by setting velocities on each of the bodies so that it will reach inPose in inDeltaTime
	void								DriveToPoseUsingKinematics(const SkeletonPose &inPose, float inDeltaTime, bool inLockBodies = true);
	
	/// Lower level version of DriveToPoseUsingKinematics that directly takes the world space joint matrices
	void								DriveToPoseUsingKinematics(const Mat44 *inJointMatrices, float inDeltaTime, bool inLockBodies = true);

	/// Drive the ragdoll to a specific pose by activating the motors on each constraint
	void								DriveToPoseUsingMotors(const SkeletonPose &inPose);

	/// Control the linear and velocity of all bodies in the ragdoll
	void								SetLinearAndAngularVelocity(Vec3Arg inLinearVelocity, Vec3Arg inAngularVelocity, bool inLockBodies = true);
	
	/// Set the world space linear velocity of all bodies in the ragdoll.
	void								SetLinearVelocity(Vec3Arg inLinearVelocity, bool inLockBodies = true);
	
	/// Add a world space velocity (in m/s) to all bodies in the ragdoll.
	void								AddLinearVelocity(Vec3Arg inLinearVelocity, bool inLockBodies = true);

	/// Add impulse to all bodies of the ragdoll (center of mass of each of them)
	void								AddImpulse(Vec3Arg inImpulse, bool inLockBodies = true);

	/// Get the position and orientation of the root of the ragdoll
	void								GetRootTransform(Vec3 &outPosition, Quat &outRotation, bool inLockBodies = true) const;

	/// Get number of bodies in the ragdoll
	size_t								GetBodyCount() const									{ return mBodyIDs.size(); }

	/// Access a body ID
	BodyID								GetBodyID(int inBodyIndex) const						{ return mBodyIDs[inBodyIndex]; }

	/// Access to the array of body IDs
	const Array<BodyID> &				GetBodyIDs() const										{ return mBodyIDs; }

	/// Get number of constraints in the ragdoll
	size_t								GetConstraintCount() const								{ return mConstraints.size(); }

	/// Access a constraint by index
	TwoBodyConstraint *					GetConstraint(int inConstraintIndex)					{ return mConstraints[inConstraintIndex]; }

	/// Access a constraint by index
	const TwoBodyConstraint *			GetConstraint(int inConstraintIndex) const				{ return mConstraints[inConstraintIndex]; }

	/// Get world space bounding box for all bodies of the ragdoll
	AABox 								GetWorldSpaceBounds(bool inLockBodies = true) const;

	/// Get the settings object that created this ragdoll
	const RagdollSettings *				GetRagdollSettings() const								{ return mRagdollSettings; }

private:
	/// For RagdollSettings::CreateRagdoll function
	friend class RagdollSettings;

	/// The settings that created this ragdoll
	RefConst<RagdollSettings>			mRagdollSettings;

	/// The bodies and constraints that this ragdoll consists of (1-on-1 with mRagdollSettings->mParts)
	Array<BodyID>						mBodyIDs;

	/// Array of constraints that connect the bodies together
	Array<Ref<TwoBodyConstraint>>		mConstraints;

	/// Cached physics system
	PhysicsSystem *						mSystem;
};

JPH_NAMESPACE_END
