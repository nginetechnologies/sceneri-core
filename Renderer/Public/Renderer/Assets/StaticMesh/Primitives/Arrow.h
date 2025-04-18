#pragma once

#include "Utilities.h"
#include "Cylinder.h"
#include "Cone.h"

#include <Renderer/Assets/StaticMesh/VertexTangents.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Math/Angle.h>
#include <Common/Math/Tangents.h>

namespace ngine::Rendering::Primitives
{
	struct Arrow
	{
		static Rendering::Index GetTotalVertexCount(Utilities::SideSizeType sideCount)
		{
			// Cylinder tube + two caps botton and top + top cone vertex
			return Cylinder::GetTubeBodyVertexCount(2, sideCount) + Utilities::GetCircleVertexCount(sideCount) * 2 + 1;
		}

		static Rendering::Index GetTotalIndicesCount(Utilities::SideSizeType sideCount)
		{
			return Cylinder::GetSidesIndexCount(2, (uint8)sideCount) + Utilities::GetCircleIndicesCount(sideCount) + (sideCount * 3);
		}

		static StaticObject Create(
			const Math::Radiusf bodyRadius,
			const Math::Radiusf tipRadius,
			const Math::Lengthf bodyHeight,
			Math::Lengthf tipHeight,
			const Utilities::SideSizeType sideCount
		)
		{
			// Convert from height of the tip itself to the location of the tip vertex
			tipHeight += bodyHeight;

			InlineVector<Rendering::Primitives::Utilities::Side, 8> sides;
			sides.Resize(sideCount);

			Utilities::CalculateSides(sides.GetView());

			StaticObject staticObject;
			auto e = GetTotalVertexCount(sideCount);
			staticObject.Resize(Memory::Uninitialized, e, GetTotalIndicesCount(sideCount));

			ArrayView<VertexPosition, Index> vertexPositions = staticObject.GetVertexElementView<VertexPosition>();
			ArrayView<VertexNormals, Index> vertexNormals = staticObject.GetVertexElementView<VertexNormals>();
			ArrayView<VertexTextureCoordinate, Index> vertexTextureCoordinates = staticObject.GetVertexElementView<VertexTextureCoordinate>();
			ArrayView<Index, Index> indices = staticObject.GetIndices();

			// Generate vertex data
			Rendering::Index vertexIndex = 0;

			// Create base cap of the cylinder
			Utilities::GenerateCircleVertices(
				vertexIndex,
				bodyRadius,
				0_meters /* bottom */,
				Math::Vector3f(Math::Down),
				sides.GetView(),
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates
			);

			Cylinder::GenerateTubeBodyVertices(
				vertexIndex,
				bodyHeight,
				bodyRadius,
				2,
				sides.GetView(),
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates
			);

			Utilities::GenerateCircleVertices(
				vertexIndex,
				tipRadius,
				bodyHeight,
				Math::Vector3f(Math::Down),
				sides.GetView(),
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates
			);

			Cone::GenerateConeVertices(vertexIndex, tipHeight, vertexPositions, vertexNormals, vertexTextureCoordinates);

			Rendering::Index indicesIndex = 0;
			uint32 segmentStart = 0;
			uint32 segmentEnd = sideCount;

			Utilities::GenerateCircleIndices(indicesIndex, segmentStart, Math::Down, sides.GetView(), indices);

			Cylinder::GenerateTubeBodyIndices(indicesIndex, 2, sideCount, segmentStart, segmentEnd, indices);

			Cone::GenerateConeIndices(indicesIndex, segmentStart, segmentEnd, vertexIndex - 1, indices);

			VertexTangents::Generate(indices, vertexPositions, vertexNormals, vertexTextureCoordinates);

			staticObject.CalculateAndSetBoundingBox();

			return staticObject;
		}
	};
}
