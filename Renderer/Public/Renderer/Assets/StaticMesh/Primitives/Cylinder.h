#pragma once

#include "Utilities.h"

#include <Renderer/Assets/StaticMesh/VertexTangents.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Math/Angle.h>
#include <Common/Math/Tangents.h>

namespace ngine::Rendering::Primitives
{
	namespace Internal
	{
		static void GenerateTubeBodyVertices(
			Rendering::Index& vertexIndex,
			const Math::Lengthf height,
			const Math::Radiusf radius,
			const Utilities::SegmentSizeType segmentCount,
			const RestrictedArrayView<Utilities::Side, Utilities::SideSizeType> sides,
			const RestrictedArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositions,
			const RestrictedArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormals,
			const RestrictedArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinates,
			bool isHalfHeight
		)
		{
			const float fullHeight = isHalfHeight ? height.GetMeters() * 2 : height.GetMeters();
			const float sideSlice = Math::MultiplicativeInverse(static_cast<float>(sides.GetSize()));
			const float relativeHeight = fullHeight / (float)(segmentCount - 1u); // -1u because we count from 0.
			// Generate vertex data for tube body.
			for (Utilities::SegmentSizeType segmentIndex = 0; segmentIndex < segmentCount; ++segmentIndex)
			{
				for (decltype(sides)::const_iterator it = sides.begin(), end = sides.end(); it != end; ++it, ++vertexIndex)
				{
					const Utilities::Side& __restrict side = *it;

					// If half height, offset z so the objects pivot is the center of the cylinder.
					const float offset = isHalfHeight ? height.GetMeters() : 0.0f;
					const float z = ((float)segmentIndex * relativeHeight) - offset;
					const Math::Vector3f newVertexPosition =
						{side.cos.GetRadians() * radius.GetMeters(), side.sin.GetRadians() * radius.GetMeters(), z};
					vertexPositions[vertexIndex] = newVertexPosition;

					const Math::Vector3f normal = Math::Vector3f(newVertexPosition.x, newVertexPosition.y, 0.0f).GetNormalized();
					vertexNormals[vertexIndex] = {normal, Math::CompressedTangent(Math::Zero, 1.f).m_tangent};

					const Utilities::SideSizeType sideIndex = sides.GetIteratorIndex(it);
					const float sideRatio = static_cast<float>(sideIndex) * sideSlice;

					vertexTextureCoordinates[vertexIndex] = {sideRatio, (float)segmentIndex * fullHeight};
				};
			}
		}
	}

	struct Cylinder
	{
		[[nodiscard]] static constexpr Rendering::Index
		GetSidesIndexCount(const Utilities::SegmentSizeType numSegments, const Utilities::SideSizeType numSides)
		{
			// Segement count is +1 because we have to connect the duplicated caps verticies with the tube verticies, which are at the same
			// position but have different normals and texure coordinates.
			return (numSegments + 1u) * numSides * 6u;
		}
		[[nodiscard]] static constexpr Rendering::Index GetCapIndexCount(const Utilities::SideSizeType numSides)
		{
			return ((numSides - 2u) * 3u) * 2u;
		}
		[[nodiscard]] static constexpr Rendering::Index
		GetTotalIndexCount(const Utilities::SegmentSizeType numSegments, const Utilities::SideSizeType numSides)
		{
			return GetSidesIndexCount(numSegments, numSides) + GetCapIndexCount(numSides);
		}
		[[nodiscard]] static constexpr Rendering::Index
		GetTubeBodyVertexCount(const Utilities::SegmentSizeType numSegments, const Utilities::SideSizeType numSides)
		{
			return numSides * numSegments;
		}
		[[nodiscard]] static constexpr Rendering::Index GetCapVertexCount(const Utilities::SideSizeType numSides)
		{
			return numSides * 2u; // Top and bottom;
		}
		[[nodiscard]] static constexpr Rendering::Index
		GetTotalVertexCount(const Utilities::SegmentSizeType numSegments, const Utilities::SideSizeType numSides)
		{
			return GetTubeBodyVertexCount(numSegments, numSides) + GetCapVertexCount(numSides);
		}

		static void GenerateTubeBodyVertices(
			Rendering::Index& vertexIndex,
			const Math::Lengthf height,
			const Math::Radiusf radius,
			const Utilities::SegmentSizeType segmentCount,
			const RestrictedArrayView<Utilities::Side, Utilities::SideSizeType> sides,
			const RestrictedArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositions,
			const RestrictedArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormals,
			const RestrictedArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinates
		)
		{
			return Internal::GenerateTubeBodyVertices(
				vertexIndex,
				height,
				radius,
				segmentCount,
				sides,
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates,
				false
			);
		}

		static void GenerateTubeBodyVerticesHalfHeight(
			Rendering::Index& vertexIndex,
			const Math::Lengthf halfheight,
			const Math::Radiusf radius,
			const Utilities::SegmentSizeType segmentCount,
			const RestrictedArrayView<Utilities::Side, Utilities::SideSizeType> sides,
			const RestrictedArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositions,
			const RestrictedArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormals,
			const RestrictedArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinates
		)
		{
			return Internal::GenerateTubeBodyVertices(
				vertexIndex,
				halfheight,
				radius,
				segmentCount,
				sides,
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates,
				true
			);
		}

		static void GenerateTubeBodyIndices(
			Rendering::Index& indicesIndex,
			const Utilities::SegmentSizeType segmentCount,
			const uint32 sideCount,
			Rendering::Index& segmentStart,
			Rendering::Index& segmentEnd,
			RestrictedArrayView<Rendering::Index, Rendering::Index> indices
		)
		{
			// Segement count is +1 because we have to connect the duplicated caps verticies with the tube verticies, which are at the same
			// position but have different normals and texure coordinates.
			for (Utilities::SegmentSizeType segmentIndex = 0; segmentIndex < segmentCount + 1; ++segmentIndex)
			{
				// Generate indices for tube body.
				for (Rendering::Index i = segmentStart; i < segmentEnd; ++i)
				{
					uint32 nextIndex = i + 1;
					nextIndex = nextIndex == segmentEnd ? segmentStart : nextIndex; // Wrap around
					indices[indicesIndex] = i;
					indices[indicesIndex + 1] = nextIndex;
					indices[indicesIndex + 2] = sideCount + i;
					indices[indicesIndex + 3] = nextIndex;
					indices[indicesIndex + 4] = sideCount + nextIndex;
					indices[indicesIndex + 5] = sideCount + i;
					indicesIndex += 6;
				}
				segmentEnd += sideCount;
				segmentStart += sideCount;
			}
		}

		static StaticObject Create(
			const Math::Radiusf radius, const Math::Lengthf halfHeight, Utilities::SegmentSizeType segmentCount, Utilities::SideSizeType sideCount
		)
		{
			Assert(radius > 0_meters);
			Assert(halfHeight > 0_meters);

			// Segement count has to be atleast two which includes the caps.
			Assert(segmentCount >= 2);
			segmentCount = Math::Max(Utilities::SegmentSizeType(2), segmentCount);

			InlineVector<Rendering::Primitives::Utilities::Side, 8> sides;
			sides.Resize(sideCount);

			Primitives::Utilities::CalculateSides(sides.GetView());

			StaticObject staticObject;
			staticObject.Resize(Memory::Uninitialized, GetTotalVertexCount(segmentCount, sideCount), GetTotalIndexCount(segmentCount, sideCount));

			ArrayView<VertexPosition, Index> vertexPositions = staticObject.GetVertexElementView<VertexPosition>();
			ArrayView<VertexNormals, Index> vertexNormals = staticObject.GetVertexElementView<VertexNormals>();
			ArrayView<VertexTextureCoordinate, Index> vertexTextureCoordinates = staticObject.GetVertexElementView<VertexTextureCoordinate>();
			ArrayView<Index, Index> indices = staticObject.GetIndices();

			// Generate vertex data
			Rendering::Index vertexIndex = 0;

			// Create base cap of the cylinder
			Primitives::Utilities::GenerateCircleVertices(
				vertexIndex,
				radius,
				-halfHeight,
				Math::Down,
				sides.GetView(),
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates
			);

			GenerateTubeBodyVerticesHalfHeight(
				vertexIndex,
				halfHeight,
				radius,
				segmentCount,
				sides.GetView(),
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates
			);

			// Create base cap of the cylinder
			Primitives::Utilities::GenerateCircleVertices(
				vertexIndex,
				radius,
				halfHeight,
				Math::Up,
				sides.GetView(),
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates
			);

			// Generate indices
			Rendering::Index indicesIndex = 0;
			uint32 segmentStart = 0;
			uint32 segmentEnd = sideCount;

			// Generate base cap indices of the cylinder
			Primitives::Utilities::GenerateCircleIndices(indicesIndex, segmentStart, Math::Down, sides.GetView(), indices);

			GenerateTubeBodyIndices(indicesIndex, segmentCount, sideCount, segmentStart, segmentEnd, indices);

			// Generate top cap indices of the cylinder
			Primitives::Utilities::GenerateCircleIndices(indicesIndex, segmentStart, Math::Up, sides.GetView(), indices);

			VertexTangents::Generate(indices, vertexPositions, vertexNormals, vertexTextureCoordinates);

			staticObject.CalculateAndSetBoundingBox();

			return staticObject;
		}
	};
}
