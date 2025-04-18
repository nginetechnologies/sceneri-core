#pragma once

#include "Cylinder.h"
#include "Sphere.h"

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Math/Power.h>
#include <Common/Math/Tangents.h>

namespace ngine::Rendering::Primitives
{
	struct Capsule
	{
		using SegmentSizeType = uint16;
		using SideSizeType = uint16;

		[[nodiscard]] static constexpr Rendering::Index GetTotalVertexCount(const SegmentSizeType segmentCount, const SideSizeType sideCount)
		{
			// Full sphere vertex count plus one addtional ring since halfspheres both need a middle ring.
			const Rendering::Index sphereVertexCount = Sphere::GetTotalVertexCount(sideCount, sideCount) + sideCount;
			return sphereVertexCount + Cylinder::GetTubeBodyVertexCount(segmentCount, sideCount);
		}

		[[nodiscard]] static constexpr Rendering::Index GetTotalIndexCount(const SegmentSizeType segmentCount, const SideSizeType sideCount)
		{
			return Sphere::GetTotalIndexCount(sideCount, sideCount) + Cylinder::GetSidesIndexCount(segmentCount, sideCount);
		}

		static StaticObject Create(
			const Math::Radiusf radius,
			const Math::Lengthf halfHeight,
			const SegmentSizeType segmentCount,
			const Utilities::SideSizeType sideCount
		)
		{
			Assert(radius > 0_meters);
			Assert(halfHeight > 0_meters);
			Assert(radius.GetMeters() <= halfHeight.GetMeters());

			InlineVector<Rendering::Primitives::Utilities::Side, 8> sides;
			sides.Resize(sideCount);

			Primitives::Utilities::CalculateSides(sides.GetView());

			StaticObject staticObject;
			staticObject
				.Resize(Memory::Uninitialized, GetTotalVertexCount(segmentCount, sideCount) * 2, GetTotalIndexCount(segmentCount, sideCount));

			ArrayView<VertexPosition, Index> vertexPositions = staticObject.GetVertexElementView<VertexPosition>();
			ArrayView<VertexNormals, Index> vertexNormals = staticObject.GetVertexElementView<VertexNormals>();
			ArrayView<VertexTextureCoordinate, Index> vertexTextureCoordinates = staticObject.GetVertexElementView<VertexTextureCoordinate>();
			ArrayView<Index, Index> indices = staticObject.GetIndices();

			const float relativeHeight = halfHeight.GetMeters() - radius.GetMeters();

			Rendering::Index vertexIndex = 0;
			Sphere::GenerateHalfSphereVertices(
				vertexIndex,
				radius,
				sideCount,
				sideCount,
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates,
				Math::Vector3f(0.0f, 0.0f, -relativeHeight),
				false
			);

			const Rendering::Index sphereEnd = vertexIndex;

			Cylinder::GenerateTubeBodyVerticesHalfHeight(
				vertexIndex,
				Math::Lengthf::FromMeters(relativeHeight),
				radius,
				segmentCount,
				sides.GetView(),
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates
			);

			const Rendering::Index cylinderEnd = vertexIndex;

			Sphere::GenerateHalfSphereVertices(
				vertexIndex,
				radius,
				sideCount,
				sideCount,
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates,
				Math::Vector3f(0.0f, 0.0f, relativeHeight),
				true
			);

			Rendering::Index indicesIndex = 0;
			uint32 segmentStart = 0;

			Sphere::GenerateHalfSphereIndices(indicesIndex, sideCount, sideCount, indices, segmentStart, false);

			segmentStart = sphereEnd - sideCount;
			uint32 segmentEnd = segmentStart + sideCount;
			Cylinder::GenerateTubeBodyIndices(indicesIndex, segmentCount, sideCount, segmentStart, segmentEnd, indices);

			Sphere::GenerateHalfSphereIndices(indicesIndex, sideCount, sideCount, indices, cylinderEnd, true);

			staticObject.CalculateAndSetBoundingBox();

			return staticObject;
		}
	};
}
