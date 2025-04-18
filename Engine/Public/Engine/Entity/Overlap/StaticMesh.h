#pragma once

#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Pipelines/CullMode.h>

#include <Common/Math/Primitives/Triangle.h>
#include <Common/Math/Primitives/Overlap.h>

namespace ngine::Math
{
	template<typename PrimitiveType>
	[[nodiscard]] bool Overlaps(
		const Rendering::StaticMesh& __restrict mesh, const PrimitiveType& __restrict primitive, const EnumFlags<Rendering::CullMode> cullMode
	)
	{
		const ArrayView<const Rendering::VertexPosition, Rendering::Index> vertices = mesh.GetVertexPositions();

		if (cullMode.AreAllSet(Rendering::CullMode::FrontAndBack))
		{
			return false;
		}
		else if (cullMode.IsSet(Rendering::CullMode::Back))
		{
			for (RestrictedArrayView<const Rendering::Index, Rendering::Index> indices = mesh.GetIndices(); indices.HasElements(); indices += 3u)
			{
				const bool overlaps = Math::Overlaps<Rendering::VertexPosition>(
					primitive,
					Math::Trianglef{vertices[indices[0]], vertices[indices[2]], vertices[indices[1]]}
				);
				if (overlaps)
				{
					return true;
				}
			}
		}
		else if (cullMode.IsSet(Rendering::CullMode::Back))
		{
			for (RestrictedArrayView<const Rendering::Index, Rendering::Index> indices = mesh.GetIndices(); indices.HasElements(); indices += 3u)
			{
				const bool overlaps = Math::Overlaps<Rendering::VertexPosition>(
					primitive,
					Math::Trianglef{vertices[indices[0]], vertices[indices[1]], vertices[indices[2]]}
				);
				if (overlaps)
				{
					return true;
				}
			}
		}
		else
		{
			for (RestrictedArrayView<const Rendering::Index, Rendering::Index> indices = mesh.GetIndices(); indices.HasElements(); indices += 3u)
			{
				const bool overlapsFront = Math::Overlaps<Rendering::VertexPosition>(
					primitive,
					Math::Trianglef{vertices[indices[0]], vertices[indices[2]], vertices[indices[1]]}
				);
				if (overlapsFront)
				{
					return true;
				}
				const bool overlapsBack = Math::Overlaps<Rendering::VertexPosition>(
					primitive,
					Math::Trianglef{vertices[indices[0]], vertices[indices[1]], vertices[indices[2]]}
				);
				if (overlapsBack)
				{
					return true;
				}
			}
		}

		return false;
	}

	template<typename PrimitiveType>
	[[nodiscard]] bool
	Overlaps(const PrimitiveType& primitive, const Rendering::StaticMesh& __restrict mesh, const EnumFlags<Rendering::CullMode> cullMode)
	{
		return Overlaps(mesh, primitive, cullMode);
	}
}
