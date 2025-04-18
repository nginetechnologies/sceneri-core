#pragma once

#include <Renderer/Assets/StaticMesh/VertexTangents.h>
#include <Renderer/Assets/StaticMesh/StaticObject.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Math/Angle.h>
#include <Common/Math/Tangents.h>
#include <Common/Math/Vector2.h>

namespace ngine::Rendering::Primitives::Utilities
{
	using SegmentSizeType = uint16;
	using SideSizeType = uint16;

	struct Side
	{
		Math::Anglef sin;
		Math::Anglef cos;
	};

	inline static void CalculateSides(const RestrictedArrayView<Utilities::Side, Utilities::SideSizeType> sides)
	{
		const float sideSlice = Math::MultiplicativeInverse(static_cast<float>(sides.GetSize()));
		// Precache sides
		for (auto it = sides.begin(), end = sides.end(); it != end; ++it)
		{
			const Math::Anglef angle = Math::PI * 2.f * sideSlice * static_cast<float>(sides.GetIteratorIndex(it));
			it->sin = angle.Sin();
			it->cos = angle.Cos();
		}
	}

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
			const Side& __restrict side = sides[it];

			const Math::Vector3f newVertexPosition =
				{side.cos.GetRadians() * radius.GetMeters(), side.sin.GetRadians() * radius.GetMeters(), height.GetMeters()};
			vertexPositions[vertexIndex] = newVertexPosition;

			vertexNormals[vertexIndex] = {
				orientation,
				Math::CompressedTangent(Math::Zero, 1.f).m_tangent // Those are generate at the end.
			};

			const SideSizeType sideIndex = it;
			const float sideSlice = Math::MultiplicativeInverse(static_cast<float>(sides.GetSize()));
			const float sideRatio = static_cast<float>(sideIndex) * sideSlice;
			vertexTextureCoordinates[vertexIndex] = Math::Vector2f(sideRatio, sideRatio);
		}
	}

	inline static void GenerateCircleIndices(
		Rendering::Index& indicesIndex,
		Rendering::Index start,
		const Math::Vector3f orientation,
		const RestrictedArrayView<Utilities::Side, Utilities::SideSizeType> sides,
		RestrictedArrayView<Rendering::Index, Rendering::Index> indices
	)
	{
		int lastIndexStart = start;
		for (Rendering::Index i = 0; i < sides.GetSize() - 2u;
		     ++i) // -2u because 2 less sides are required when generating a fan from the first vertex position.
		{
			if (orientation.IsEquivalentTo(Math::Down))
			{
				indices[indicesIndex] = lastIndexStart;
				indices[indicesIndex + 1] = lastIndexStart + i + 2;
				indices[indicesIndex + 2] = lastIndexStart + i + 1;
			}
			else
			{
				indices[indicesIndex] = lastIndexStart;
				indices[indicesIndex + 1] = lastIndexStart + i + 1;
				indices[indicesIndex + 2] = lastIndexStart + i + 2;
			}
			indicesIndex += 3;
		}
	}

	inline static Rendering::Index GetCircleVertexCount(SideSizeType sideCount)
	{
		return sideCount;
	}
	inline static Rendering::Index GetCircleIndicesCount(SideSizeType sideCount)
	{
		return (sideCount - 2u) * 3u;
	}
}
