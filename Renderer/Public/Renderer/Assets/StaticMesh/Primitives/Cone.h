#pragma once

#include "Utilities.h"

#include <Renderer/Assets/StaticMesh/VertexTangents.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Math/Angle.h>
#include <Common/Math/Tangents.h>

namespace ngine::Rendering::Primitives
{
	struct Cone
	{
		static void GenerateConeVertices(
			Rendering::Index& vertexIndex,
			const Math::Lengthf height,
			const RestrictedArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositions,
			const RestrictedArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormals,
			const RestrictedArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinates
		)
		{
			// Top of the cone
			vertexPositions[vertexIndex] = Math::Vector3f(0, 0, height.GetMeters());
			vertexNormals[vertexIndex] = {Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Zero, 1.f).m_tangent};
			vertexTextureCoordinates[vertexIndex] = Math::Vector2f(0.0f, height.GetMeters());
			vertexIndex++;
		}

		static void GenerateConeIndices(
			Rendering::Index& indicesIndex,
			Rendering::Index& segmentStart,
			Rendering::Index& segmentEnd,
			const Rendering::Index coneVertexIndex,
			RestrictedArrayView<Rendering::Index, Rendering::Index> indices
		)
		{
			for (Rendering::Index i = segmentStart; i < segmentEnd; ++i)
			{
				uint32 nextIndex = i + 1;
				nextIndex = nextIndex == segmentEnd ? segmentStart : nextIndex; // Wrap around
				indices[indicesIndex] = i;
				indices[indicesIndex + 1] = nextIndex;
				indices[indicesIndex + 2] = coneVertexIndex;
				indicesIndex += 3;
			}
		}

		static Rendering::Index GetTotalVertexCount(Utilities::SideSizeType sideCount)
		{
			return Utilities::GetCircleVertexCount(sideCount) + 1;
		}

		static Rendering::Index GetTotalIndicesCount(Utilities::SideSizeType sideCount)
		{
			return Utilities::GetCircleIndicesCount(sideCount) + (sideCount * 3);
		}

		static StaticObject Create(const Math::Radiusf radius, const Math::Lengthf height, const Utilities::SideSizeType sideCount)
		{
			Assert(radius > 0_meters);
			Assert(height > 0_meters);

			InlineVector<Rendering::Primitives::Utilities::Side, 8> sides;
			sides.Resize(sideCount);

			Utilities::CalculateSides(sides.GetView());

			StaticObject staticObject;
			staticObject.Resize(Memory::Uninitialized, GetTotalVertexCount(sideCount), GetTotalIndicesCount(sideCount));

			ArrayView<VertexPosition, Index> vertexPositions = staticObject.GetVertexElementView<VertexPosition>();
			ArrayView<VertexNormals, Index> vertexNormals = staticObject.GetVertexElementView<VertexNormals>();
			ArrayView<VertexTextureCoordinate, Index> vertexTextureCoordinates = staticObject.GetVertexElementView<VertexTextureCoordinate>();
			ArrayView<Index, Index> indices = staticObject.GetIndices();

			// Generate vertex data
			Rendering::Index vertexIndex = 0;

			// Create base cap of the cylinder
			Utilities::GenerateCircleVertices(
				vertexIndex,
				radius,
				0_meters /* bottom */,
				Math::Vector3f(Math::Down),
				sides.GetView(),
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates
			);

			GenerateConeVertices(vertexIndex, height, vertexPositions, vertexNormals, vertexTextureCoordinates);

			Rendering::Index indicesIndex = 0;
			uint32 segmentStart = 0;
			uint32 segmentEnd = sideCount;

			Utilities::GenerateCircleIndices(indicesIndex, segmentStart, Math::Down, sides.GetView(), indices);

			GenerateConeIndices(indicesIndex, segmentStart, segmentEnd, vertexIndex - 1, indices);

			VertexTangents::Generate(indices, vertexPositions, vertexNormals, vertexTextureCoordinates);

			staticObject.CalculateAndSetBoundingBox();

			return staticObject;
		}
	};
}
