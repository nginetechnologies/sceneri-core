// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#pragma once

#include <Core/Reference.h>
#include <Core/Result.h>
#include <ObjectStream/SerializableObject.h>

JPH_NAMESPACE_BEGIN

class StreamIn;
class StreamOut;
#ifdef JPH_DEBUG_RENDERER
class DebugRenderer;
#endif // JPH_DEBUG_RENDERER

/// The path for a path constraint. It allows attaching two bodies to each other while giving the second body the freedom to move along a path relative to the first.
class PathConstraintPath : public SerializableObject, public RefTarget<PathConstraintPath>
{
public:
	JPH_DECLARE_SERIALIZABLE_ABSTRACT(PathConstraintPath)

	using PathResult = Result<Ref<PathConstraintPath>>;

	/// Virtual destructor to ensure that derived types get their destructors called
	virtual				~PathConstraintPath() override = default;

	/// Gets the max fraction along the path. I.e. sort of the length of the path.
	virtual float		GetPathMaxFraction() const = 0;

	/// Get the globally closest point on the curve (Could be slow!)
	/// @param inPosition Position to find closest point for
	/// @return Fraction of closest point along the path
	virtual float		GetClosestPoint(Vec3Arg inPosition) const = 0;

	/// Given the fraction along the path, get the point, tangent and normal.
	/// @param inFraction Fraction along the path [0, GetPathMaxFraction()].
	/// @param outPathPosition Returns the closest position to inSearchPosition on the path.
	/// @param outPathTangent Returns the tangent to the path at outPathPosition (the vector that follows the direction of the path)
	/// @param outPathNormal Return the normal to the path at outPathPosition (a vector that's perpendicular to outPathTangent)
	/// @param outPathBinormal Returns the binormal to the path at outPathPosition (a vector so that normal cross tangent = binormal)
	virtual void		GetPointOnPath(float inFraction, Vec3 &outPathPosition, Vec3 &outPathTangent, Vec3 &outPathNormal, Vec3 &outPathBinormal) const = 0;

	/// If the path is looping or not. If a path is looping, the first and last point are automatically connected to eachother. They should not be the same points.
	void				SetIsLooping(bool inIsLooping)						{ mIsLooping = inIsLooping; }
	bool				IsLooping() const									{ return mIsLooping; }

#ifdef JPH_DEBUG_RENDERER
	/// Draw the path relative to inBaseTransform. Used for debug purposes.
	void				DrawPath(DebugRenderer *inRenderer, Mat44Arg inBaseTransform) const;
#endif // JPH_DEBUG_RENDERER

	/// Saves the contents of the path in binary form to inStream.
	virtual void		SaveBinaryState(StreamOut &inStream) const;

	/// Creates a Shape of the correct type and restores its contents from the binary stream inStream.
	static PathResult	sRestoreFromBinaryState(StreamIn &inStream);

protected:
	/// This function should not be called directly, it is used by sRestoreFromBinaryState.
	virtual void		RestoreBinaryState(StreamIn &inStream);

private:
	/// If the path is looping or not. If a path is looping, the first and last point are automatically connected to eachother. They should not be the same points.
	bool				mIsLooping = false;
};

JPH_NAMESPACE_END
