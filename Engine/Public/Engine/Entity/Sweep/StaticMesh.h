#pragma once

#include <Renderer/Pipelines/CullMode.h>

#include <Common/Math/Primitives/Triangle.h>
#include <Common/Math/Primitives/Sweep.h>
#include <Common/Math/Primitives/Sweep/Result.h>
#include <Common/Math/Primitives/Sweep/ForwardDeclarations/WorldResult.h>
#include <Common/Math/Primitives/Overlap.h>

namespace ngine::Math
{
	template<typename PrimitiveType>
	[[nodiscard]] TSweepResult<Rendering::VertexPosition> Sweep(
		const Rendering::StaticMesh& __restrict mesh,
		const PrimitiveType& __restrict primitive,
		const Math::Vector3f sweepDistance,
		const EnumFlags<Rendering::CullMode> cullMode
	)
	{
		using Result = TSweepResult<Rendering::VertexPosition>;
		const ArrayView<const Rendering::VertexPosition, Rendering::Index> vertices = mesh.GetVertexPositions();

		if (cullMode.AreAllSet(Rendering::CullMode::FrontAndBack))
		{
			return {};
		}
		else if (cullMode.IsSet(Rendering::CullMode::Back))
		{
			for (RestrictedArrayView<const Rendering::Index, Rendering::Index> indices = mesh.GetIndices(); indices.HasElements(); indices += 3u)
			{
				const Result result = Math::Sweep<Rendering::VertexPosition>(
					localPrimitive,
					sweepDistance,
					Math::Trianglef{vertices[indices[0]], vertices[indices[2]], vertices[indices[1]]}
				);
				if (result)
				{
					return result;
				}
			}
		}
		else if (cullMode.IsSet(Rendering::CullMode::Back))
		{
			for (RestrictedArrayView<const Rendering::Index, Rendering::Index> indices = mesh.GetIndices(); indices.HasElements(); indices += 3u)
			{
				const Result result = Math::Sweep<Rendering::VertexPosition>(
					localPrimitive,
					sweepDistance,
					Math::Trianglef{vertices[indices[0]], vertices[indices[1]], vertices[indices[2]]}
				);
				if (result)
				{
					return result;
				}
			}
		}
		else
		{
			for (RestrictedArrayView<const Rendering::Index, Rendering::Index> indices = mesh.GetIndices(); indices.HasElements(); indices += 3u)
			{
				const Result frontfaceResult = Math::Sweep<Rendering::VertexPosition>(
					localPrimitive,
					sweepDistance,
					Math::Trianglef{vertices[indices[0]], vertices[indices[2]], vertices[indices[1]]}
				);
				if (result)
				{
					return result;
				}
				const Result backfaceResult = Math::Sweep<Rendering::VertexPosition>(
					localPrimitive,
					sweepDistance,
					Math::Trianglef{vertices[indices[0]], vertices[indices[1]], vertices[indices[2]]}
				);
				if (result)
				{
					return result;
				}
			}
		}

		return {};
	}
}
