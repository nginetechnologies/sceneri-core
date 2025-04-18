#pragma once

#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Pipelines/CullMode.h>

#include <Common/Math/Primitives/Triangle.h>
#include <Common/Math/Primitives/Intersect.h>
#include <Common/Math/Primitives/Intersect/Result.h>
#include <Common/Math/Primitives/Overlap.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>

namespace ngine::Math
{
	template<typename PrimitiveType>
	[[nodiscard]] TIntersectionResult<Rendering::VertexPosition> Intersects(
		const Rendering::StaticMesh& __restrict mesh, const PrimitiveType& __restrict primitive, const EnumFlags<Rendering::CullMode> cullMode
	)
	{
		using Result = TIntersectionResult<Rendering::VertexPosition>;

		const RestrictedArrayView<const Rendering::VertexPosition, Rendering::Index> vertexPositions = mesh.GetVertexPositions();

		if (cullMode.AreAllSet(Rendering::CullMode::FrontAndBack))
		{
			return Result{Math::IntersectionType::NoIntersection};
		}
		else if (cullMode.IsSet(Rendering::CullMode::Back))
		{
			for (RestrictedArrayView<const Rendering::Index, Rendering::Index> indices = mesh.GetIndices(); indices.HasElements(); indices += 3u)
			{
				const Result frontFaceResult = Math::Intersects<Rendering::VertexPosition>(
					primitive,
					Math::Trianglef{vertexPositions[indices[0]], vertexPositions[indices[2]], vertexPositions[indices[1]]}
				);
				if (frontFaceResult.m_type != Math::IntersectionType::NoIntersection)
				{
					return frontFaceResult;
				}
			}
		}
		else if (cullMode.IsSet(Rendering::CullMode::Back))
		{
			for (RestrictedArrayView<const Rendering::Index, Rendering::Index> indices = mesh.GetIndices(); indices.HasElements(); indices += 3u)
			{
				const Math::IntersectionResultf backFaceResult = Math::Intersects<Rendering::VertexPosition>(
					primitive,
					Math::Trianglef{vertexPositions[indices[0]], vertexPositions[indices[1]], vertexPositions[indices[2]]}
				);
				if (backFaceResult.m_type != Math::IntersectionType::NoIntersection)
				{
					return backFaceResult;
				}
			}
		}
		else
		{
			for (RestrictedArrayView<const Rendering::Index, Rendering::Index> indices = mesh.GetIndices(); indices.HasElements(); indices += 3u)
			{
				const Result frontFaceResult = Math::Intersects<Rendering::VertexPosition>(
					primitive,
					Math::Trianglef{vertexPositions[indices[0]], vertexPositions[indices[2]], vertexPositions[indices[1]]}
				);
				if (frontFaceResult.m_type != Math::IntersectionType::NoIntersection)
				{
					return frontFaceResult;
				}
				const Math::IntersectionResultf backFaceResult = Math::Intersects<Rendering::VertexPosition>(
					primitive,
					Math::Trianglef{vertexPositions[indices[0]], vertexPositions[indices[1]], vertexPositions[indices[2]]}
				);
				if (backFaceResult.m_type != Math::IntersectionType::NoIntersection)
				{
					return frontFaceResult;
				}
			}
		}

		return Result{Math::IntersectionType::NoIntersection};
	}

	template<typename PrimitiveType>
	[[nodiscard]] TIntersectionResult<Rendering::VertexPosition>
	Intersects(const PrimitiveType& primitive, const Rendering::StaticMesh& __restrict mesh, const EnumFlags<Rendering::CullMode> cullMode)
	{
		return Intersects(mesh, primitive, cullMode);
	}
}
