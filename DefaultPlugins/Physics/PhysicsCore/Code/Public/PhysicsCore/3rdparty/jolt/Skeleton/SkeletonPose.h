// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Skeleton/Skeleton.h>
#include <Skeleton/SkeletalAnimation.h>

JPH_NAMESPACE_BEGIN

#ifdef JPH_DEBUG_RENDERER
class DebugRenderer;
#endif // JPH_DEBUG_RENDERER

/// Instance of a skeleton, contains the pose the current skeleton is in
class SkeletonPose
{
public:
	JPH_OVERRIDE_NEW_DELETE

	using JointState = SkeletalAnimation::JointState;
	using JointStateVector = Array<JointState>;
	using Mat44Vector = Array<Mat44>;

	///@name Skeleton
	///@{
	void						SetSkeleton(const Skeleton *inSkeleton);
	const Skeleton *			GetSkeleton() const														{ return mSkeleton; }
	///@}

	///@name Properties of the joints
	///@{
	uint						GetJointCount() const													{ return (uint)mJoints.size(); }
	const JointStateVector &	GetJoints() const														{ return mJoints; }
	JointStateVector &			GetJoints()																{ return mJoints; }
	const JointState &			GetJoint(int inJoint) const												{ return mJoints[inJoint]; }
	JointState &				GetJoint(int inJoint)													{ return mJoints[inJoint]; }
	///@}

	///@name Joint matrices
	///@{
	const Mat44Vector &			GetJointMatrices() const												{ return mJointMatrices; }
	Mat44Vector &				GetJointMatrices()														{ return mJointMatrices; }
	const Mat44 &				GetJointMatrix(int inJoint) const										{ return mJointMatrices[inJoint]; }
	Mat44 &						GetJointMatrix(int inJoint)												{ return mJointMatrices[inJoint]; }
	///@}

	/// Convert the joint states to joint matrices
	void						CalculateJointMatrices();

	/// Convert joint matrices to joint states
	void						CalculateJointStates();

	/// Outputs the joint matrices in local space (ensure that outMatrices has GetJointCount() elements, assumes that values in GetJoints() is up to date)
	void						CalculateLocalSpaceJointMatrices(Mat44 *outMatrices) const;

#ifdef JPH_DEBUG_RENDERER
	/// Draw settings
	struct DrawSettings
	{
		bool					mDrawJoints = true;
		bool					mDrawJointOrientations = true;
		bool					mDrawJointNames = false;
	};

	/// Draw current pose
	void						Draw(const DrawSettings &inDrawSettings, DebugRenderer *inRenderer, Mat44Arg inOffset = Mat44::sIdentity()) const;
#endif // JPH_DEBUG_RENDERER

private:
	RefConst<Skeleton>			mSkeleton;																///< Skeleton definition
	JointStateVector			mJoints;																///< Local joint orientations (local to parent Joint)
	Mat44Vector					mJointMatrices;															///< Local joint matrices (local to world matrix)
};

JPH_NAMESPACE_END
