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
		inline static void GenerateCircleVertices(
			Rendering::Index& vertexIndex,
			const Math::Radiusf radius,
			const Math::Lengthf height,
			const Math::Vector3f orientation,
			const RestrictedArrayView<Utilities::Side, Utilities::SideSizeType> sides,
			const RestrictedArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositions,
			const RestrictedArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormals,
			const RestrictedArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinates
		)
		{
			for (Utilities::SideSizeType it = 0; it < sides.GetSize(); ++it, ++vertexIndex)
			{
				const Utilities::Side& __restrict side = sides[it];

				const Math::Vector3f newVertexPosition =
					{side.cos.GetRadians() * radius.GetMeters(), side.sin.GetRadians() * radius.GetMeters(), height.GetMeters()};
				vertexPositions[vertexIndex] = newVertexPosition;

				vertexNormals[vertexIndex] = {
					orientation,
					Math::CompressedTangent(Math::Zero, 1.f).m_tangent // Those are generate at the end.
				};

				const Utilities::SideSizeType sideIndex = it;
				const float sideSlice = Math::MultiplicativeInverse(static_cast<float>(sides.GetSize()));
				const float sideRatio = static_cast<float>(sideIndex) * sideSlice;
				vertexTextureCoordinates[vertexIndex] = Math::Vector2f(sideRatio, sideRatio);
			}
		}

		inline static void GenerateStripIndices(
			Rendering::Index& indicesIndex,
			Rendering::Index start,
			Rendering::Index end,
			bool isOuter,
			uint32 offset,
			RestrictedArrayView<Rendering::Index, Rendering::Index> indices
		)
		{
			for (Rendering::Index i = start; i < end - 1; ++i)
			{
				if (!isOuter)
				{
					uint32 nextIndex = i + 1;
					indices[indicesIndex] = i;
					indices[indicesIndex + 1] = nextIndex;
					indices[indicesIndex + 2] = offset + i;
					indices[indicesIndex + 3] = nextIndex;
					indices[indicesIndex + 4] = offset + nextIndex;
					indices[indicesIndex + 5] = offset + i;
					indicesIndex += 6;
				}
				else
				{
					uint32 nextIndex = i + 1;
					indices[indicesIndex] = i;
					indices[indicesIndex + 1] = offset + i;
					indices[indicesIndex + 2] = nextIndex;
					indices[indicesIndex + 3] = nextIndex;
					indices[indicesIndex + 4] = offset + i;
					indices[indicesIndex + 5] = offset + nextIndex;
					indicesIndex += 6;
				}
			}
		}

		inline static void GenerateSideIndices(
			Rendering::Index& indicesIndex,
			const RestrictedArrayView<Utilities::Side, Utilities::SideSizeType> sides,
			RestrictedArrayView<Rendering::Index, Rendering::Index> indices
		)
		{
			const Utilities::SideSizeType sideCount = sides.GetSize();

			indices[indicesIndex] = 0;
			indices[indicesIndex + 1] = sideCount;
			indices[indicesIndex + 2] = sideCount * 2;
			indices[indicesIndex + 3] = sideCount;
			indices[indicesIndex + 4] = sideCount * 3;
			indices[indicesIndex + 5] = sideCount * 2;
			indicesIndex += 6;

			indices[indicesIndex] = sideCount - 1;
			indices[indicesIndex + 1] = sideCount * 3 - 1;
			indices[indicesIndex + 2] = sideCount * 2 - 1;
			indices[indicesIndex + 3] = sideCount * 2 - 1;
			indices[indicesIndex + 4] = sideCount * 3 - 1;
			indices[indicesIndex + 5] = sideCount * 4 - 1;
			indicesIndex += 6;
		}

		inline static void
		CalculateSidesWithAngle(const RestrictedArrayView<Utilities::Side, Utilities::SideSizeType> sides, Math::Anglef angle)
		{
			const float sideSlice = Math::MultiplicativeInverse(static_cast<float>(sides.GetSize() - 1));
			for (auto it = sides.begin(), end = sides.end(); it != end; ++it)
			{
				const Math::Anglef slicedAngle = angle * sideSlice * static_cast<float>(sides.GetIteratorIndex(it));
				it->sin = slicedAngle.Sin();
				it->cos = slicedAngle.Cos();
			}
		}
	}

	struct Arc
	{
		[[nodiscard]] static constexpr Rendering::Index GetTotalIndexCount(const Utilities::SideSizeType sideCount)
		{
			return ((sideCount - 1u) * 6u) * 4u + 12u; // sideCount - 1  * 6 for each arc * 4 for top, bottom, front and back + 12 for the sides;
		}

		[[nodiscard]] static constexpr Rendering::Index GetTotalVertexCount(const Utilities::SideSizeType sideCount)
		{
			return sideCount * 4u;
		}

		static StaticObject Create(
			const Math::Anglef angle,
			const Math::Lengthf halfHeight,
			const Math::Radiusf outer,
			const Math::Radiusf inner,
			Utilities::SideSizeType sideCount
		)
		{
			Assert(angle.GetRadians() != 0.0f, "Can't create an arc with a zero angle!");
			Assert(sideCount % 2 == 0, "Side count needs to be dividable by two!");
			Assert(outer > inner, "Outer radius has to be better than the inner radius!");

			StaticObject staticObject;
			staticObject.Resize(Memory::Uninitialized, GetTotalVertexCount(sideCount), GetTotalIndexCount(sideCount));

			InlineVector<Rendering::Primitives::Utilities::Side, 16> sides;
			sides.Resize(sideCount);
			Internal::CalculateSidesWithAngle(sides.GetView(), angle);

			ArrayView<VertexPosition, Index> vertexPositions = staticObject.GetVertexElementView<VertexPosition>();
			ArrayView<VertexNormals, Index> vertexNormals = staticObject.GetVertexElementView<VertexNormals>();
			ArrayView<VertexTextureCoordinate, Index> vertexTextureCoordinates = staticObject.GetVertexElementView<VertexTextureCoordinate>();
			ArrayView<Index, Index> indices = staticObject.GetIndices();

			// Generate  verticies
			Rendering::Index vertex = 0;
			// Top outer arc
			GenerateCircleVertices(
				vertex,
				outer,
				halfHeight,
				Math::Vector3f(Math::Up),
				sides.GetView(),
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates
			);
			// Top inner arc
			GenerateCircleVertices(
				vertex,
				inner,
				halfHeight,
				Math::Vector3f(Math::Up),
				sides.GetView(),
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates
			);
			// Bottom outer arc
			GenerateCircleVertices(
				vertex,
				outer,
				halfHeight * -1.0f,
				Math::Vector3f(Math::Down),
				sides.GetView(),
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates
			);
			// Bottom inner arc
			GenerateCircleVertices(
				vertex,
				inner,
				halfHeight * -1.0f,
				Math::Vector3f(Math::Down),
				sides.GetView(),
				vertexPositions,
				vertexNormals,
				vertexTextureCoordinates
			);

			// Generate indicies
			Rendering::Index indicesIndex = 0;
			// Top
			Internal::GenerateStripIndices(indicesIndex, 0, sides.GetSize(), false, sideCount, indices);
			// Bottom
			Internal::GenerateStripIndices(indicesIndex, sideCount * 2, sideCount * 3, true, sideCount, indices);
			// Front
			Internal::GenerateStripIndices(indicesIndex, 0, sides.GetSize(), true, sideCount * 2, indices);
			// Back
			Internal::GenerateStripIndices(indicesIndex, sideCount, sideCount * 2u, false, sideCount * 2, indices);
			// Sides to close the mesh.
			Internal::GenerateSideIndices(indicesIndex, sides.GetView(), indices);

			return staticObject;
		}
	};
}
