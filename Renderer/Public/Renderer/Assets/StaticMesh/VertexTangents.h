#pragma once

#include <Common/Memory/Containers/ArrayView.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>

#include <Renderer/Index.h>
#include <Renderer/Assets/StaticMesh/VertexNormals.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexTextureCoordinate.h>

namespace ngine::Rendering::VertexTangents
{
	bool Generate(
		const RestrictedArrayView<Rendering::Index, Rendering::Index> indices,
		const RestrictedArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositions,
		const RestrictedArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormals,
		const RestrictedArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinates,
		const double angularThreshold = 180.0
	);
}
