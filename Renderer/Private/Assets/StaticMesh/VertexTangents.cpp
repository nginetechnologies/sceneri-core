#include "Assets/StaticMesh/VertexTangents.h"

#include <Common/Math/Vector2.h>

#include <3rdparty/mikktspace/mikktspace.h>

namespace ngine::Rendering::VertexTangents
{
	struct MikkTSpaceContext : SMikkTSpaceContext
	{
		MikkTSpaceContext(
			const RestrictedArrayView<const Rendering::Index, Rendering::Index> indices,
			const RestrictedArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositions,
			const RestrictedArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormals,
			const RestrictedArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinates
		)
			: m_indices(indices)
			, m_vertexPositions(vertexPositions)
			, m_vertexNormals(vertexNormals)
			, m_vertexTextureCoordinates(vertexTextureCoordinates)
		{
			m_interface.m_getNumFaces = getNumFaces;
			m_interface.m_getNumVerticesOfFace = getNumVerticesOfFace;
			m_interface.m_getPosition = getPosition;
			m_interface.m_getNormal = getNormal;
			m_interface.m_getTexCoord = getTexCoord;
			m_interface.m_setTSpaceBasic = setTSpaceBasic;
			m_interface.m_setTSpace = nullptr;
			m_pInterface = &m_interface;
			m_pUserData = this;
		}

		RestrictedArrayView<const Rendering::Index, Rendering::Index> m_indices;
		RestrictedArrayView<Rendering::VertexPosition, Rendering::Index> m_vertexPositions;
		RestrictedArrayView<Rendering::VertexNormals, Rendering::Index> m_vertexNormals;
		RestrictedArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> m_vertexTextureCoordinates;
		SMikkTSpaceInterface m_interface;

		[[nodiscard]] bool computeTangents(const double angularThreshold) const
		{
			return (bool)genTangSpace(this, static_cast<float>(angularThreshold));
		}

		static int getNumFaces(const SMikkTSpaceContext* pContext)
		{
			return static_cast<int>(static_cast<const MikkTSpaceContext*>(pContext)->m_indices.GetSize() / 3);
		}

		static int getNumVerticesOfFace(const SMikkTSpaceContext*, [[maybe_unused]] const int faceIndex)
		{
			return 3;
		}

		static void getPosition(const SMikkTSpaceContext* pContext, float fvPosOut[], const int faceIndex, const int vertexIndex)
		{
			const MikkTSpaceContext& context = *static_cast<const MikkTSpaceContext*>(pContext);
			const Rendering::VertexPosition& __restrict vertexPosition =
				context.m_vertexPositions[context.m_indices[faceIndex * 3 + vertexIndex]];
			fvPosOut[0] = vertexPosition.x;
			fvPosOut[1] = vertexPosition.y;
			fvPosOut[2] = vertexPosition.z;
		}

		static void getNormal(const SMikkTSpaceContext* pContext, float fvNormOut[], const int faceIndex, const int vertexIndex)
		{
			const MikkTSpaceContext& context = *static_cast<const MikkTSpaceContext*>(pContext);
			const Rendering::VertexNormals& vertexNormals = context.m_vertexNormals[context.m_indices[faceIndex * 3 + vertexIndex]];
			const Math::Vector3f normal = vertexNormals.normal;

			fvNormOut[0] = normal.x;
			fvNormOut[1] = normal.y;
			fvNormOut[2] = normal.z;
		}

		static void setTSpaceBasic(
			const SMikkTSpaceContext* pContext, const float fvTangent[], const float fSign, const int faceIndex, const int vertexIndex
		)
		{
			const MikkTSpaceContext& context = *static_cast<const MikkTSpaceContext*>(pContext);
			Rendering::VertexNormals& vertexNormals = context.m_vertexNormals[context.m_indices[faceIndex * 3 + vertexIndex]];
			const Math::Vector3f tangent = Math::Vector3f(fvTangent[0], fvTangent[1], fvTangent[2]);
			if (!tangent.IsZero())
			{
				vertexNormals.tangent = {tangent, fSign};
			}
		}

		static void getTexCoord(const SMikkTSpaceContext* pContext, float fvTexcOut[], const int faceIndex, const int vertexIndex)
		{
			const MikkTSpaceContext& context = *static_cast<const MikkTSpaceContext*>(pContext);
			const Rendering::VertexTextureCoordinate& vertexTextureCoordinate =
				context.m_vertexTextureCoordinates[context.m_indices[faceIndex * 3 + vertexIndex]];
			fvTexcOut[0] = vertexTextureCoordinate.x;
			fvTexcOut[1] = vertexTextureCoordinate.y;
		}
	};

	bool Generate(
		const RestrictedArrayView<Rendering::Index, Rendering::Index> indices,
		const RestrictedArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositions,
		const RestrictedArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormals,
		const RestrictedArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinates,
		const double angularThreshold
	)
	{
		MikkTSpaceContext context{indices, vertexPositions, vertexNormals, vertexTextureCoordinates};
		return context.computeTangents(angularThreshold);
	}
}
