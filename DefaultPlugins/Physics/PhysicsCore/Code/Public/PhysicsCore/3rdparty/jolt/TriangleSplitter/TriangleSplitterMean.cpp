// SPDX-FileCopyrightText: 2021 Jorrit Rouwe
// SPDX-License-Identifier: MIT

#include <Jolt.h>

#include <TriangleSplitter/TriangleSplitterMean.h>

#include <Common/Platform/CompilerWarnings.h>

PUSH_CLANG_WARNINGS
DISABLE_CLANG_WARNING("-Wimplicit-int-float-conversion")

JPH_NAMESPACE_BEGIN

TriangleSplitterMean::TriangleSplitterMean(const VertexList& inVertices, const IndexedTriangleList& inTriangles)
	: TriangleSplitter(inVertices, inTriangles)
{
}

bool TriangleSplitterMean::Split(const Range& inTriangles, Range& outLeft, Range& outRight)
{
	// Calculate mean value for these triangles
	Vec3 mean = Vec3::sZero();
	for (uint t = inTriangles.mBegin; t < inTriangles.mEnd; ++t)
		mean += Vec3(mCentroids[mSortedTriangleIdx[t]]);
	mean *= 1.0f / inTriangles.Count();

	// Calculate deviation
	Vec3 deviation = Vec3::sZero();
	for (uint t = inTriangles.mBegin; t < inTriangles.mEnd; ++t)
	{
		Vec3 delta = Vec3(mCentroids[mSortedTriangleIdx[t]]) - mean;
		deviation += delta * delta;
	}
	deviation *= 1.0f / inTriangles.Count();

	// Calculate split plane
	uint dimension = deviation.GetHighestComponentIndex();
	float split = mean[dimension];

	return SplitInternal(inTriangles, dimension, split, outLeft, outRight);
}

JPH_NAMESPACE_END

POP_CLANG_WARNINGS
