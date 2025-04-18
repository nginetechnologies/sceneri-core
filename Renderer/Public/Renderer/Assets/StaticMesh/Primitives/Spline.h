#pragma once

#include "Utilities.h"

#include <Renderer/Assets/StaticMesh/VertexTangents.h>

#include <Renderer/Assets/StaticMesh/StaticObject.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Math/Angle.h>
#include <Common/Math/Tangents.h>
#include <Common/Math/Primitives/Spline.h>
#include <Common/Math/Transform.h>

namespace ngine::Rendering::Primitives
{
	struct Spline
	{
		using SplineType = Math::Splinef;

		[[nodiscard]] static constexpr Rendering::Index
		GetTotalVertexCount(const Utilities::SegmentSizeType numSegments, const Utilities::SideSizeType numSides)
		{
			return (numSides + 1u) * numSegments;
		}
		[[nodiscard]] PURE_STATICS static constexpr Rendering::Index
		GetTotalIndexCount(const Utilities::SegmentSizeType numSegments, const Utilities::SideSizeType numSides)
		{
			return (numSegments - 1u) * numSides * 6u;
		}

		static void CreateVerticesForSegment(
			const float length,
			Rendering::Index& vertexIndex,
			const ArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositions,
			const ArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormals,
			const ArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinates,
			const Math::LocalTransform& __restrict segmentTransform,
			const float sideSlice,
			const float radius,
			const ArrayView<const Utilities::Side, uint16> sides
		)
		{
			constexpr float uvTilingX = 1.f;

			const Rendering::VertexPosition& firstVertexPosition = vertexPositions[vertexIndex];
			const Rendering::VertexNormals& firstVertexNormals = vertexNormals[vertexIndex];
			const Rendering::VertexTextureCoordinate& firstVertexTextureCoordinate = vertexTextureCoordinates[vertexIndex];

			for (const Utilities::Side& __restrict side : sides)
			{
				const Math::Vector3f angle = Math::Vector3f{side.cos.GetRadians(), 0, side.sin.GetRadians()}.GetNormalized();
				Math::Vector3f vertexOffset = segmentTransform.TransformDirectionWithoutScale(angle);
				vertexOffset.Normalize();
				vertexPositions[vertexIndex] = segmentTransform.GetLocation() + vertexOffset * radius;
				vertexNormals[vertexIndex] = {vertexOffset, Math::CompressedTangent(Math::Zero, 1.f).m_tangent};

				const uint16 sideIndex = sides.GetIteratorIndex(Memory::GetAddressOf(side));
				const float sideRatio = static_cast<float>(sideIndex) * sideSlice;
				vertexTextureCoordinates[vertexIndex] = {sideRatio * uvTilingX, length};

				++vertexIndex;
			}

			vertexPositions[vertexIndex] = firstVertexPosition;
			vertexNormals[vertexIndex] = firstVertexNormals;
			vertexTextureCoordinates[vertexIndex] = firstVertexTextureCoordinate;
			vertexTextureCoordinates[vertexIndex].x = sides.GetSize() * sideSlice * uvTilingX;
			++vertexIndex;
		}

		static StaticObject Create(const SplineType& spline, const float radius, const Utilities::SideSizeType sideCount)
		{
			Assert(spline.GetPointCount() > 0);
			Assert(radius > 0);

			FlatVector<Utilities::Side, 256> splineSides(Memory::ConstructWithSize, Memory::Uninitialized, sideCount);
			Utilities::CalculateSides(splineSides.GetView());

			const Utilities::SegmentSizeType segmentCount = (Utilities::SegmentSizeType)spline.CalculateSegmentCount();

			const Rendering::Index vertexCount = GetTotalVertexCount(segmentCount, sideCount);
			const Rendering::Index indexCount = GetTotalIndexCount(segmentCount, sideCount);

			Rendering::StaticObject staticObject;
			staticObject.Resize(Memory::Uninitialized, vertexCount, indexCount);

			ArrayView<Rendering::Index, Rendering::Index> indicesView = staticObject.GetIndices();

			Rendering::Index vertexIndex = 0;

			float length = 0.0f;
			const float sideSlice = Math::MultiplicativeInverse(static_cast<float>(sideCount));
			spline.IterateAdjustedSplinePoints(
				[radius,
			   splineSides = splineSides.GetView(),
			   &vertexIndex,
			   &length,
			   sideSlice,
			   positions = staticObject.GetVertexElementView<Rendering::VertexPosition>(),
			   normals = staticObject.GetVertexElementView<Rendering::VertexNormals>(),
			   textureCoordinates = staticObject.GetVertexElementView<Rendering::VertexTextureCoordinate>()](
					const SplineType::Spline::Point&,
					const SplineType::Spline::Point&,
					const Math::Vector3f currentBezierPoint,
					const Math::Vector3f nextBezierPoint,
					const Math::Vector3f direction,
					const Math::Vector3f normal
				) mutable
				{
					const Math::LocalTransform::StoredRotationType segmentRotation(
						Math::Matrix3x3f(normal, direction, normal.Cross(direction).GetNormalized()).GetOrthonormalized()
					);
					const Math::LocalTransform segmentTransform = Math::LocalTransform(segmentRotation, currentBezierPoint);

					CreateVerticesForSegment(
						length,
						vertexIndex,
						positions,
						normals,
						textureCoordinates,
						segmentTransform,
						sideSlice,
						radius,
						splineSides
					);

					length += (currentBezierPoint - nextBezierPoint).GetLength();
				}
			);

			Assert(vertexIndex == vertexCount);
			const Rendering::Index endVertexIndex = GetTotalVertexCount(segmentCount - 1u, sideCount);

			for (Rendering::Index startIndex = 0u; startIndex < endVertexIndex; startIndex += sideCount + 1u)
			{
				for (Rendering::Index i = startIndex, n = startIndex + sideCount; i < n; ++i, indicesView += 6u)
				{
					indicesView[0] = i;
					indicesView[2] = indicesView[3] = indicesView[0] + 1u;
					indicesView[1] = indicesView[4] = indicesView[2] + sideCount;
					indicesView[5] = indicesView[1] + 1u;
				}
			}

			Assert(indicesView.IsEmpty());
			Assert(staticObject.GetIndices().All(
				[vertexCount](const Rendering::Index index)
				{
					return index < vertexCount;
				}
			));

			staticObject.CalculateAndSetBoundingBox();

			return staticObject;
		}
	};
}
