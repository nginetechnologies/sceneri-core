#pragma once

#include "Utilities.h"

#include <Renderer/Assets/StaticMesh/VertexTangents.h>
#include <Renderer/Assets/StaticMesh/StaticObject.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Math/Angle.h>
#include <Common/Math/Tangents.h>

namespace ngine::Rendering::Primitives
{
	struct Torus
	{
		static Index GetTotalVertexCount(Utilities::SideSizeType sideCount)
		{
			return sideCount * sideCount;
		}

		static Index GetTotalIndicesCount(Utilities::SideSizeType sideCount)
		{
			return sideCount * sideCount * 6;
		}

		static StaticObject Create(const Math::Radiusf radius, const Math::Lengthf thickness, const Utilities::SideSizeType sideCount)
		{
			Assert(radius > 0_meters);
			Assert(thickness > 0_meters);

			const Index vertexCount = GetTotalVertexCount(sideCount);
			const Index indexCount = GetTotalIndicesCount(sideCount);

			StaticObject staticObject;
			staticObject.Resize(Memory::Uninitialized, vertexCount, indexCount);

			ArrayView<VertexPosition, Index> vertexPositions = staticObject.GetVertexElementView<VertexPosition>();
			ArrayView<VertexNormals, Index> vertexNormals = staticObject.GetVertexElementView<VertexNormals>();
			ArrayView<VertexTextureCoordinate, Index> vertexTextureCoordinates = staticObject.GetVertexElementView<VertexTextureCoordinate>();

			const float radiusUnit = radius.GetUnits();

			Index vertexIndex = 0u;
			// Generate vertices
			for (Index i = 0u; i < sideCount; ++i)
			{
				for (Index j = 0u; j < sideCount; ++j)
				{
					const Math::Vector2f uv = (Math::Vector2f((float)j, (float)i) / sideCount) * Math::PI.GetRadians() * 2.0f;
					float cosOfX;
					float cosOfY;
					const float sinOfX = Math::SinCos(uv.x, cosOfX);
					const float sinOfY = Math::SinCos(uv.y, cosOfY);
					const Math::Vector2f xy = Math::Vector2f(radiusUnit + thickness.GetMeters() * cosOfY) * Math::Vector2f(sinOfX, cosOfX);
					const float z = thickness.GetMeters() * sinOfY;
					vertexPositions[vertexIndex] = VertexPosition(xy.x, xy.y, z);

					const Math::Vector3f center(radiusUnit * cosOfY * cosOfX, radiusUnit * cosOfY * sinOfX, 0.0f);
					const Math::Vector3f normal = (center - Math::Vector3f(xy.x, xy.y, z)).GetNormalized();
					vertexNormals[vertexIndex] = {normal, Math::CompressedTangent(Math::Zero, 1.0f).m_tangent};

					vertexTextureCoordinates[vertexIndex] = uv;

					vertexIndex++;
				}
			}

			// Generate indicies
			ArrayView<Index, Index> indices = staticObject.GetIndices();
			Index indicesIndex = 0u;
			Index segmentStart = 0u;
			Index segmentEnd = sideCount;

			for (Index j = 0u; j < sideCount; ++j)
			{
				for (Index i = segmentStart; i < segmentEnd; ++i)
				{
					Index nextIndex = i + 1u;
					nextIndex = nextIndex == segmentEnd ? segmentStart : nextIndex; // Wrap around

					indices[indicesIndex] = (sideCount + i) % vertexCount;
					indices[indicesIndex + 1u] = nextIndex;
					indices[indicesIndex + 2u] = i;
					indices[indicesIndex + 3u] = (sideCount + i) % vertexCount;
					indices[indicesIndex + 4u] = (sideCount + nextIndex) % vertexCount;
					indices[indicesIndex + 5u] = nextIndex;
					indicesIndex += 6u;
				}

				segmentEnd += sideCount;
				segmentStart += sideCount;
			}

			VertexTangents::Generate(indices, vertexPositions, vertexNormals, vertexTextureCoordinates);

			staticObject.CalculateAndSetBoundingBox();

			return Move(staticObject);
		}
	};
}
