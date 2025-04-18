#pragma once

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Math/Tangents.h>
#include <Common/Math/Angle.h>
#include <Common/Math/SinCos.h>
#include <Common/Math/Vector2.h>

#include <Renderer/Assets/StaticMesh/StaticObject.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexTextureCoordinate.h>
#include <Renderer/Assets/StaticMesh/VertexNormals.h>
#include <Renderer/Assets/StaticMesh/VertexTangents.h>
#include <Renderer/Index.h>

namespace ngine::Rendering::Primitives
{
	struct Sphere
	{
		static void GenerateHalfSphereVertices(
			Rendering::Index& vertexIndex,
			const Math::Radiusf radius,
			const uint32 latitudes,
			const uint32 longitudes,
			const RestrictedArrayView<Rendering::VertexPosition, Rendering::Index> vertexPositionsOut,
			const RestrictedArrayView<Rendering::VertexNormals, Rendering::Index> vertexNormalsOut,
			const RestrictedArrayView<Rendering::VertexTextureCoordinate, Rendering::Index> vertexTextureCoordinatesOut,
			const Math::Vector3f offset,
			const bool isTop
		)
		{
			const float inverseLength = Math::MultiplicativeInverse(radius.GetMeters());
			const float deltaLatitude = Math::PI.GetRadians() / (float)latitudes;
			const float deltaLongitude = 2 * Math::PI.GetRadians() / (float)longitudes;
			const float inverseLongitude = Math::MultiplicativeInverse((float)longitudes);
			const float inverseLatitude = Math::MultiplicativeInverse((float)latitudes);

			uint32 end;
			uint32 start;
			if (!isTop)
			{
				end = latitudes / 2;
				start = 0;
			}
			else
			{
				end = latitudes;
				start = latitudes / 2;
			}

			for (uint32 i = start; i <= end; ++i)
			{
				float latitudeAngle = Math::PI.GetRadians() / 2 - (float)i * deltaLatitude;
				float cosResult;
				float z = radius.GetMeters() * Math::SinCos(latitudeAngle, cosResult) * -1.0f;
				float xy = radius.GetMeters() * cosResult;

				for (uint32 j = 0; j <= longitudes; ++j)
				{
					float longitudeAngle = (float)j * deltaLongitude;

					Math::Vector2f result = Math::Vector2f{xy, xy} * Math::Vector2f{Math::Cos(longitudeAngle), Math::Sin(longitudeAngle)};
					VertexPosition vertex = VertexPosition(result.x, result.y, z) + offset;
					vertexPositionsOut[vertexIndex] = vertex;

					vertexTextureCoordinatesOut[vertexIndex] = Math::Vector2f{(float)j, (float)i} * Math::Vector2f{inverseLongitude, inverseLatitude};

					Math::Vector3f normal(vertex * Math::Vector3f{inverseLength});
					vertexNormalsOut[vertexIndex] = VertexNormals{normal, Math::CompressedTangent(Math::Zero, 1.f).m_tangent};
					++vertexIndex;
				}
			}
		}

		static void GenerateHalfSphereIndices(
			Rendering::Index& index,
			const uint32 latitudes,
			const uint32 longitudes,
			const RestrictedArrayView<Rendering::Index, Rendering::Index> indicesOut,
			const uint32 startIndex,
			const bool isTop
		)
		{
			uint32 end;
			uint32 start;
			uint32 offset = 0;
			if (!isTop)
			{
				end = latitudes / 2;
				start = 0;
			}
			else
			{
				end = latitudes;
				start = latitudes / 2;
				offset = start * (longitudes + 1);
			}

			Rendering::Index k1, k2 = 0;
			for (uint32 i = start; i < end; ++i)
			{
				k1 = i * (longitudes + 1);
				k2 = k1 + longitudes + 1;
				k1 -= offset;
				k2 -= offset;
				k1 += startIndex;
				k2 += startIndex;
				for (uint32 j = 0; j < longitudes; ++j, ++k1, ++k2)
				{
					if (i != 0)
					{
						indicesOut[index] = k1;
						indicesOut[index + 1] = k1 + 1;
						indicesOut[index + 2] = k2;
						index += 3;
					}

					if (i != (latitudes - 1))
					{
						indicesOut[index] = k1 + 1;
						indicesOut[index + 1] = k2 + 1;
						indicesOut[index + 2] = k2;
						index += 3;
					}
				}
			}

			start = k2;
		}

		static StaticObject Create(const Math::Radiusf radius, const uint32 latitudes, const uint32 longitudes)
		{
			Assert(radius > 0_meters);
			Assert(latitudes > 0);
			Assert(longitudes > 0);

			StaticObject staticObject;
			staticObject.Resize(Memory::Uninitialized, GetTotalVertexCount(latitudes, longitudes), GetTotalIndexCount(latitudes, longitudes));

			ArrayView<VertexPosition, Index> vertexPositions = staticObject.GetVertexElementView<VertexPosition>();
			ArrayView<VertexNormals, Index> vertexNormals = staticObject.GetVertexElementView<VertexNormals>();
			ArrayView<VertexTextureCoordinate, Index> vertexTextureCoordinates = staticObject.GetVertexElementView<VertexTextureCoordinate>();
			ArrayView<Index, Index> indices = staticObject.GetIndices();

			const float inverseLength = Math::MultiplicativeInverse(radius.GetMeters());
			const float deltaLatitude = Math::PI.GetRadians() / (float)latitudes;
			const float deltaLongitude = 2 * Math::PI.GetRadians() / (float)longitudes;
			const float inverseLongitude = Math::MultiplicativeInverse((float)longitudes);
			const float inverseLatitude = Math::MultiplicativeInverse((float)latitudes);

			uint32 index = 0;
			for (uint32 i = 0; i <= latitudes; ++i)
			{
				float latitudeAngle = Math::PI.GetRadians() / 2 - (float)i * deltaLatitude;
				float cosResult;
				float z = radius.GetMeters() * Math::SinCos(latitudeAngle, cosResult);
				float xy = radius.GetMeters() * cosResult;

				for (uint32 j = 0; j <= longitudes; ++j)
				{
					float longitudeAngle = (float)j * deltaLongitude;

					float longitudeAngleCos;
					float longitudeAngleSin = Math::SinCos(longitudeAngle, longitudeAngleCos);

					Math::Vector2f result = Math::Vector2f{xy, xy} * Math::Vector2f{longitudeAngleCos, longitudeAngleSin};
					VertexPosition vertex = VertexPosition(result.x, result.y, z);
					vertexPositions[index] = vertex;

					vertexTextureCoordinates[index] = Math::Vector2f{(float)j, (float)i} * Math::Vector2f{inverseLongitude, inverseLatitude};

					Math::Vector3f normal(vertex * Math::Vector3f{inverseLength});
					vertexNormals[index] = VertexNormals{normal, Math::CompressedTangent(Math::Zero, 1.f).m_tangent};
					++index;
				}
			}
			Assert(index == vertexPositions.GetSize());

			Rendering::Index k1, k2;
			index = 0;
			for (uint32 i = 0; i < latitudes; ++i)
			{
				k1 = i * (longitudes + 1);
				k2 = k1 + longitudes + 1;
				for (uint32 j = 0; j < longitudes; ++j, ++k1, ++k2)
				{
					if (i != 0)
					{
						indices[index] = k1;
						indices[index + 1] = k2;
						indices[index + 2] = k1 + 1;
						index += 3;
					}

					if (i != (latitudes - 1))
					{
						indices[index] = k1 + 1;
						indices[index + 1] = k2;
						indices[index + 2] = k2 + 1;
						index += 3;
					}
				}
			}
			Assert(index == indices.GetSize());

			VertexTangents::Generate(indices, vertexPositions, vertexNormals, vertexTextureCoordinates);

			staticObject.CalculateAndSetBoundingBox();

			return staticObject;
		}

		[[nodiscard]] static constexpr Rendering::Index GetTotalVertexCount(const uint32 latitudes, const uint32 longitudes)
		{
			return (latitudes + 1) * (longitudes + 1);
		}

		[[nodiscard]] static constexpr Rendering::Index GetTotalIndexCount(const uint32 latitudes, const uint32 longitudes)
		{
			return (latitudes * longitudes * 6) - (latitudes * 6);
		}
	};
}
