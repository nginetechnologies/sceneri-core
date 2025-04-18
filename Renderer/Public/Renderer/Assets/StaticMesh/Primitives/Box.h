#pragma once

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Math/Tangents.h>

#include <Renderer/Assets/StaticMesh/StaticObject.h>
#include <Renderer/Assets/StaticMesh/VertexNormals.h>

namespace ngine::Rendering::Primitives
{
	namespace Internal
	{
		static void
		CreateVertices(Math::Vector3f halfExtent, const RestrictedArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositionsOut)
		{
			const Array<VertexPosition, 24, Index, Index> vertexPositions{
				// Back
				VertexPosition{halfExtent.x, -halfExtent.y, halfExtent.z},
				VertexPosition{-halfExtent.x, -halfExtent.y, halfExtent.z},
				VertexPosition{-halfExtent.x, -halfExtent.y, -halfExtent.z},
				VertexPosition{halfExtent.x, -halfExtent.y, -halfExtent.z},
				// Bottom
				VertexPosition{halfExtent.x, halfExtent.y, -halfExtent.z},
				VertexPosition{halfExtent.x, -halfExtent.y, -halfExtent.z},
				VertexPosition{-halfExtent.x, -halfExtent.y, -halfExtent.z},
				VertexPosition{-halfExtent.x, halfExtent.y, -halfExtent.z},
				// Left
				VertexPosition{-halfExtent.x, halfExtent.y, -halfExtent.z},
				VertexPosition{-halfExtent.x, -halfExtent.y, -halfExtent.z},
				VertexPosition{-halfExtent.x, -halfExtent.y, halfExtent.z},
				VertexPosition{-halfExtent.x, halfExtent.y, halfExtent.z},
				// Forward
				VertexPosition{-halfExtent.x, halfExtent.y, halfExtent.z},
				VertexPosition{halfExtent.x, halfExtent.y, halfExtent.z},
				VertexPosition{halfExtent.x, halfExtent.y, -halfExtent.z},
				VertexPosition{-halfExtent.x, halfExtent.y, -halfExtent.z},
				// Right
				VertexPosition{halfExtent.x, halfExtent.y, halfExtent.z},
				VertexPosition{halfExtent.x, -halfExtent.y, halfExtent.z},
				VertexPosition{halfExtent.x, -halfExtent.y, -halfExtent.z},
				VertexPosition{halfExtent.x, halfExtent.y, -halfExtent.z},
				// Top
				VertexPosition{-halfExtent.x, halfExtent.y, halfExtent.z},
				VertexPosition{-halfExtent.x, -halfExtent.y, halfExtent.z},
				VertexPosition{halfExtent.x, -halfExtent.y, halfExtent.z},
				VertexPosition{halfExtent.x, halfExtent.y, halfExtent.z},
			};
			vertexPositionsOut.CopyFrom(vertexPositions.GetDynamicView());
		}

		static void CreateNormals(const RestrictedArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormalsOut)
		{

			const Array<VertexNormals, 24, Index, Index> vertexNormals{
				// Back
				VertexNormals{Math::Vector3f(Math::Backward), Math::CompressedTangent(Math::Left, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Backward), Math::CompressedTangent(Math::Left, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Backward), Math::CompressedTangent(Math::Left, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Backward), Math::CompressedTangent(Math::Left, 1.f).m_tangent},
				// Bottom
				VertexNormals{Math::Vector3f(Math::Down), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Down), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Down), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Down), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				// Left
				VertexNormals{Math::Vector3f(Math::Left), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Left), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Left), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Left), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				// Forward
				VertexNormals{Math::Vector3f(Math::Forward), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Forward), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Forward), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Forward), Math::CompressedTangent(Math::Right, 1.f).m_tangent},
				// Right
				VertexNormals{Math::Vector3f(Math::Right), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Right), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Right), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Right), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				// Top
				VertexNormals{Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
				VertexNormals{Math::Vector3f(Math::Up), Math::CompressedTangent(Math::Backward, 1.f).m_tangent},
			};
			vertexNormalsOut.CopyFrom(vertexNormals.GetDynamicView());
		}

		static void CreateIndices(const RestrictedArrayView<Rendering::Index, Rendering::Index> indicesOut)
		{
			const Array<Rendering::Index, 36, Index, Index> indices{
				// Front
				0u,
				1u,
				2u,
				0u,
				2u,
				3u,
				// Back
				4u,
				5u,
				6u,
				4u,
				6u,
				7u,
				// Top
				8u,
				9u,
				10u,
				8u,
				10u,
				11u,
				// Bottom
				12u,
				13u,
				14u,
				12u,
				14u,
				15u,
				// Right
				16u,
				17u,
				18u,
				16u,
				18u,
				19u,
				// Left
				20u,
				21u,
				22u,
				20u,
				22u,
				23u,
			};
			indicesOut.CopyFrom(indices.GetDynamicView());
		}
	}
	struct Box
	{
		inline static constexpr Rendering::Index VertexCount = 24;
		inline static constexpr Rendering::Index IndexCount = 36;

		static StaticObject CreateUniform(const Math::Radiusf radius)
		{
			return CreateUniform(Math::Vector3f(radius.GetMeters()));
		}

		static StaticObject CreateUniform(const Math::Vector3f halfExtent)
		{
			StaticObject staticObject;
			staticObject.Resize(Memory::Uninitialized, VertexCount, IndexCount);

			Internal::CreateVertices(halfExtent, staticObject.GetVertexElementView<VertexPosition>());
			Internal::CreateNormals(staticObject.GetVertexElementView<VertexNormals>());
			Internal::CreateIndices(staticObject.GetIndices());

			// Create uniform texture coordiantes
			const Array<VertexTextureCoordinate, 24, Index, Index> textureCoordinates{
				// Back
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{1.0f, 1.0f},
				// Bottom
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{1.0f, 1.0f},
				// Left
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{1.0f, 1.0f},
				// Forward
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{1.0f, 1.0f},
				// Right
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{1.0f, 1.0f},
				// Top
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{1.0f, 1.0f},
			};
			staticObject.GetVertexElementView<VertexTextureCoordinate>().CopyFrom(textureCoordinates.GetDynamicView());

			staticObject.CalculateAndSetBoundingBox();

			return staticObject;
		}

		static StaticObject CreateUnwrapped(const float radius)
		{
			StaticObject staticObject;
			staticObject.Resize(Memory::Uninitialized, VertexCount, IndexCount);

			Internal::CreateVertices(Math::Vector3f(radius), staticObject.GetVertexElementView<VertexPosition>());
			Internal::CreateNormals(staticObject.GetVertexElementView<VertexNormals>());
			Internal::CreateIndices(staticObject.GetIndices());

			// Create unwrapped texture coordiantes
			const Array<VertexTextureCoordinate, 24, Index, Index> textureCoordinates{
				// Back
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{1.0f, 1.0f},
				// Bottom
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{1.0f, 1.0f},
				// Left
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{1.0f, 1.0f},
				// Forward
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{1.0f, 1.0f},
				// Right
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{1.0f, 1.0f},
				// Top
				Math::Vector2f{1.0f, 0.0f},
				Math::Vector2f{0.0f, 0.0f},
				Math::Vector2f{0.0f, 1.0f},
				Math::Vector2f{1.0f, 1.0f},
			};
			staticObject.GetVertexElementView<VertexTextureCoordinate>().CopyFrom(textureCoordinates.GetDynamicView());

			staticObject.CalculateAndSetBoundingBox();

			return Move(staticObject);
		}
	};
}
