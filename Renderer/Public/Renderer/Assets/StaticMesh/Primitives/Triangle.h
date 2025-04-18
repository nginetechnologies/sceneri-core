#pragma once

#include "Utilities.h"

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Math/Tangents.h>

namespace ngine::Rendering::Primitives
{
	struct Triangle
	{
		inline static constexpr Rendering::Index VertexCount = 3;
		inline static constexpr Rendering::Index IndexCount = 3;

		static StaticObject Create(const Math::Vector2f half_extent)
		{
			Assert(half_extent.x > 0 && half_extent.y > 0);

			StaticObject staticObject;
			staticObject.Resize(Memory::Uninitialized, VertexCount, IndexCount);

			const Array<VertexPosition, VertexCount, Index, Index> vertexPositions{
				VertexPosition{0, -half_extent.y, 0.f},
				VertexPosition{half_extent.x, half_extent.y, 0.f},
				VertexPosition{-half_extent.x, half_extent.y, 0.f}
			};
			staticObject.GetVertexElementView<VertexPosition>().CopyFrom(vertexPositions.GetDynamicView());

			const Array<VertexNormals, VertexCount, Index, Index> vertexNormals{
				VertexNormals{Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Right, 1.f).m_tangent}
			};
			staticObject.GetVertexElementView<VertexNormals>().CopyFrom(vertexNormals.GetDynamicView());

			const Array<VertexTextureCoordinate, VertexCount, Index, Index> textureCoordinates{
				Math::Vector2f{0.5f, 0.f},
				Math::Vector2f{1.0f, 1.0f},
				Math::Vector2f{0.f, 1.f}
			};
			staticObject.GetVertexElementView<VertexTextureCoordinate>().CopyFrom(textureCoordinates.GetDynamicView());

			const Array<Rendering::Index, IndexCount, Index, Index> indices{2u, 1u, 0u};
			staticObject.GetIndices().CopyFrom(indices.GetDynamicView());

			staticObject.CalculateAndSetBoundingBox();

			return staticObject;
		}
	};
}
