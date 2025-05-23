// SPDX-FileCopyrightText: 2022 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Core/Reference.h>
#include <Skeleton/Skeleton.h>

JPH_NAMESPACE_BEGIN

/// Class that is able to map a low detail (ragdoll) skeleton to a high detail (animation) skeleton and vice versa
class SkeletonMapper : public RefTarget<SkeletonMapper>
{
public:
	/// A joint that maps 1-on-1 to a joint in the other skeleton
	class Mapping
	{
	public:
		Mapping() = default;
		Mapping(int inJointIdx1, int inJointIdx2, Mat44Arg inJoint1To2)
			: mJointIdx1(inJointIdx1)
			, mJointIdx2(inJointIdx2)
			, mJoint1To2(inJoint1To2)
			, mJoint2To1(inJoint1To2.Inversed())
		{
		}

		int mJointIdx1;   ///< Index of joint from skeleton 1
		int mJointIdx2;   ///< Corresponding index of joint from skeleton 2
		Mat44 mJoint1To2; ///< Transforms this joint from skeleton 1 to 2
		Mat44 mJoint2To1; ///< Inverse of the transform above
	};

	/// A joint chain that starts with a 1-on-1 mapped joint and ends with a 1-on-1 mapped joint with intermediate joints that cannot be
	/// mapped
	class Chain
	{
	public:
		Chain() = default;
		Chain(Array<int>&& inJointIndices1, Array<int>&& inJointIndices2)
			: mJointIndices1(std::move(inJointIndices1))
			, mJointIndices2(std::move(inJointIndices2))
		{
		}

		Array<int> mJointIndices1; ///< Joint chain from skeleton 1
		Array<int> mJointIndices2; ///< Corresponding joint chain from skeleton 2
	};

	/// Joints that could not be mapped from skeleton 1 to 2
	class Unmapped
	{
	public:
		Unmapped() = default;
		Unmapped(int inJointIdx, int inParentJointIdx)
			: mJointIdx(inJointIdx)
			, mParentJointIdx(inParentJointIdx)
		{
		}

		int mJointIdx;       ///< Joint index of unmappable joint
		int mParentJointIdx; ///< Parent joint index of unmappable joint
	};

	/// A function that is called to determine if a joint can be mapped from source to target skeleton
	using CanMapJoint = function<bool(const Skeleton*, int, const Skeleton*, int)>;

	/// Default function that checks if the names of the joints are equal
	static bool sDefaultCanMapJoint(const Skeleton* inSkeleton1, int inIndex1, const Skeleton* inSkeleton2, int inIndex2)
	{
		return inSkeleton1->GetJoint(inIndex1).mName == inSkeleton2->GetJoint(inIndex2).mName;
	}

	/// Initialize the skeleton mapper. Skeleton 1 should be the (low detail) ragdoll skeleton and skeleton 2 the (high detail) animation
	/// skeleton. We assume that each joint in skeleton 1 can be mapped to a joint in skeleton 2 (if not mapping from animation skeleton to
	/// ragdoll skeleton will be undefined). Skeleton 2 should have the same hierarchy as skeleton 1 but can contain extra joints between
	/// those in skeleton 1 and it can have extra joints at the root and leaves of the skeleton.
	/// @param inSkeleton1 Source skeleton to map from.
	/// @param inNeutralPose1 Neutral pose of the source skeleton (model space)
	/// @param inSkeleton2 Target skeleton to map to.
	/// @param inNeutralPose2 Neutral pose of the target skeleton (model space), inNeutralPose1 and inNeutralPose2 must match as closely as
	/// possible, preferably the position of the mappable joints should be identical.
	/// @param inCanMapJoint Function that checks if joints in skeleton 1 and skeleton 2 are equal.
	void Initialize(
		const Skeleton* inSkeleton1,
		const Mat44* inNeutralPose1,
		const Skeleton* inSkeleton2,
		const Mat44* inNeutralPose2,
		const CanMapJoint& inCanMapJoint = sDefaultCanMapJoint
	);

	/// Map a pose. Joints that were directly mappable will be copied in model space from pose 1 to pose 2. Any joints that are only present
	/// in skeleton 2 will get their model space transform calculated through the local space transforms of pose 2. Joints that are part of a
	/// joint chain between two mapped joints will be reoriented towards the next joint in skeleton 1. This means that it is possible for
	/// unmapped joints to have some animation, but very extreme animation poses will show artifacts.
	/// @param inPose1ModelSpace Pose on skeleton 1 in model space
	/// @param inPose2LocalSpace Pose on skeleton 2 in local space (used for the joints that cannot be mapped)
	/// @param outPose2ModelSpace Model space pose on skeleton 2 (the output of the mapping)
	void Map(const Mat44* inPose1ModelSpace, const Mat44* inPose2LocalSpace, Mat44* outPose2ModelSpace) const;

	/// Reverse map a pose, this will only use the mappings and not the chains (it assumes that all joints in skeleton 1 are mapped)
	/// @param inPose2ModelSpace Model space pose on skeleton 2
	/// @param outPose1ModelSpace When the function returns this will contain the model space pose for skeleton 1
	void MapReverse(const Mat44* inPose2ModelSpace, Mat44* outPose1ModelSpace) const;

	using MappingVector = Array<Mapping>;
	using ChainVector = Array<Chain>;
	using UnmappedVector = Array<Unmapped>;

	///@name Access to the mapped joints
	///@{
	const MappingVector& GetMappings() const
	{
		return mMappings;
	}
	MappingVector& GetMappings()
	{
		return mMappings;
	}
	const ChainVector& GetChains() const
	{
		return mChains;
	}
	ChainVector& GetChains()
	{
		return mChains;
	}
	const UnmappedVector& GetUnmapped() const
	{
		return mUnmapped;
	}
	UnmappedVector& GetUnmapped()
	{
		return mUnmapped;
	}
	///@}
private:
	/// Joint mappings
	MappingVector mMappings;
	ChainVector mChains;
	UnmappedVector mUnmapped; ///< Joint indices that could not be mapped from 1 to 2 (these are indices in 2)
};

JPH_NAMESPACE_END
