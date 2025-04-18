#pragma once

#include "Utilities.h"

#include <Renderer/Assets/StaticMesh/VertexTangents.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Math/Angle.h>
#include <Common/Math/Tangents.h>

namespace ngine::Rendering::Primitives
{
	struct Pyramid
	{
		inline static constexpr Rendering::Index VertexCount = 5;
		inline static constexpr Rendering::Index IndexCount = 18;

		static StaticObject Create(const Math::Radiusf radius, const Math::Lengthf height)
		{
			StaticObject staticObject;
			staticObject.Resize(Memory::Uninitialized, VertexCount, IndexCount);

			const float rawRadius = radius.GetMeters();
			const Array<VertexPosition, 5, Index, Index> vertexPositions{
				VertexPosition{-rawRadius, -rawRadius, 0.f},
				VertexPosition{rawRadius, -rawRadius, 0.f},
				VertexPosition{rawRadius, rawRadius, 0.f},
				VertexPosition{-rawRadius, rawRadius, 0.f},
				VertexPosition{0.0f, 0.0f, height.GetMeters()}
			};
			staticObject.GetVertexElementView<VertexPosition>().CopyFrom(vertexPositions.GetDynamicView());

			const Array<VertexNormals, 5, Index, Index> vertexNormals{
				VertexNormals{Math::Vector3f(Math::Down), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Down), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Down), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Down), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Right, 1.f).m_tangent}
			};
			staticObject.GetVertexElementView<VertexNormals>().CopyFrom(vertexNormals.GetDynamicView());

			const Array<VertexTextureCoordinate, 5, Index, Index> textureCoordinates{
				Math::Vector2f{1.0f, 1.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{1.0f, 1.0f}
			};
			staticObject.GetVertexElementView<VertexTextureCoordinate>().CopyFrom(textureCoordinates.GetDynamicView());

			const Array<Rendering::Index, 18, Index, Index> indices{
				/*bottom*/
				0u,
				2u,
				1u,
				3u,
				2u,
				0u,
				/*side 1*/
				0u,
				1u,
				4u,
				/*side 2*/
				1u,
				2u,
				4u,
				/*side 3*/
				2u,
				3u,
				4u,
				/*side 4*/
				3u,
				0u,
				4u,
			};
			staticObject.GetIndices().CopyFrom(indices.GetDynamicView());

			staticObject.CalculateAndSetBoundingBox();

			return staticObject;
		}
	};
}
