#include "MeshSkin.h"
#include "MeshSkinAssetType.h"
#include "SkinningJob.h"

#include "SkeletonInstance.h"

#include <Common/IO/FileView.h>
#include <Common/Memory/Containers/ByteView.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Reflection/Registry.inl>

#include <Renderer/Assets/StaticMesh/VertexNormals.h>

#include "3rdparty/ozz/base/maths/simd_math.h"

namespace ngine::Animation
{
	MeshSkin::MeshSkin(ConstByteView data)
		: m_parts(ReadParts(data))
		, m_jointRemappingIndices(Memory::ConstructWithSize, Memory::Uninitialized, *data.Read<const uint16>())
		, m_inverseBindPoses(Memory::ConstructWithSize, Memory::Uninitialized, *(data + sizeof(uint16)).Read<const uint16>())
	{
		data += sizeof(uint16) * 2;
		[[maybe_unused]] const bool readJointRemappingIndices = data.ReadIntoView(m_jointRemappingIndices.GetView());
		data += m_jointRemappingIndices.GetDataSize();
		Assert(readJointRemappingIndices);
		[[maybe_unused]] const bool readInverseBindPoses = data.ReadIntoView(m_inverseBindPoses.GetView());
		Assert(readInverseBindPoses);
	}

	MeshSkin::MeshSkin(ConstByteView data, const bool)
		: m_parts(ReadParts(data, false))
		, m_jointRemappingIndices(Memory::ConstructWithSize, Memory::Uninitialized, *data.Read<const uint16>())
		, m_inverseBindPoses(Memory::ConstructWithSize, Memory::Uninitialized, *(data + sizeof(uint16)).Read<const uint16>())
	{
		data += sizeof(uint16) * 2;
		[[maybe_unused]] const bool readJointRemappingIndices = data.ReadIntoView(m_jointRemappingIndices.GetView());
		data += m_jointRemappingIndices.GetDataSize();
		Assert(readJointRemappingIndices);
		[[maybe_unused]] const bool readInverseBindPoses = data.ReadIntoView(m_inverseBindPoses.GetView());
		Assert(readInverseBindPoses);
	}

	void MeshSkin::WriteToFile(const IO::FileView outputFile) const
	{
		outputFile.Write(m_parts.GetSize());
		for (const Part& part : m_parts)
		{
			part.WriteToFile(outputFile);
		}

		outputFile.Write(m_jointRemappingIndices.GetSize());
		outputFile.Write(m_inverseBindPoses.GetSize());
		outputFile.Write(m_jointRemappingIndices.GetView());
		outputFile.Write(m_inverseBindPoses.GetView());
	}

	/* static */ Vector<MeshSkin::Part, uint16> MeshSkin::ReadParts(ConstByteView& data)
	{
		Vector<Part, uint16> parts(Memory::Reserve, *data.Read<const uint16>());
		data += sizeof(uint16);
		for ([[maybe_unused]] Part& part : parts.GetAllocatedView())
		{
			parts.EmplaceBack(Part{data});
		}

		return parts;
	}

	/* static */ Vector<MeshSkin::Part, uint16> MeshSkin::ReadParts(ConstByteView& data, const bool)
	{
		Vector<Part, uint16> parts(Memory::Reserve, *data.Read<const uint16>());
		data += sizeof(uint16);
		for ([[maybe_unused]] Part& part : parts.GetAllocatedView())
		{
			parts.EmplaceBack(Part{data, true});
		}

		return parts;
	}

	MeshSkin::Part::Part(ConstByteView& data)
		: m_vertexCount(*data.Read<const Rendering::Index>())
		, m_jointIndices(Memory::ConstructWithSize, Memory::Uninitialized, *(data + sizeof(Rendering::Index)).Read<const Rendering::Index>())
		, m_jointWeights(
				Memory::ConstructWithSize, Memory::Uninitialized, *(data + sizeof(Rendering::Index) * 2).Read<const Rendering::Index>()
			)
	{
		data += sizeof(Rendering::Index) * 3;
		[[maybe_unused]] const bool readJointIndices = data.ReadIntoView(m_jointIndices.GetView());
		data += m_jointIndices.GetDataSize();
		Assert(readJointIndices);
		[[maybe_unused]] const bool readJointWeights = data.ReadIntoView(m_jointWeights.GetView());
		data += m_jointWeights.GetDataSize();
		Assert(readJointWeights);
	}

	MeshSkin::Part::Part(ConstByteView& data, const bool)
		: m_vertexCount(*data.Read<const Rendering::Index>())
		, m_jointIndices(Memory::ConstructWithSize, Memory::Uninitialized, *(data + sizeof(uint16)).Read<const uint16>())
		, m_jointWeights(
				Memory::ConstructWithSize, Memory::Uninitialized, *(data + sizeof(Rendering::Index) + sizeof(uint16)).Read<const uint16>()
			)
	{
		data += sizeof(Rendering::Index) + sizeof(uint16) * 2;
		[[maybe_unused]] const bool readJointIndices = data.ReadIntoView(m_jointIndices.GetView());
		data += m_jointIndices.GetDataSize();
		Assert(readJointIndices);
		[[maybe_unused]] const bool readJointWeights = data.ReadIntoView(m_jointWeights.GetView());
		data += m_jointWeights.GetDataSize();
		Assert(readJointWeights);
	}

	void MeshSkin::Part::WriteToFile(const IO::FileView outputFile) const
	{
		outputFile.Write(m_vertexCount);
		outputFile.Write(m_jointIndices.GetSize());
		outputFile.Write(m_jointWeights.GetSize());
		outputFile.Write(m_jointIndices.GetView());
		outputFile.Write(m_jointWeights.GetView());
	}

	bool MeshSkin::ValidateVertexBuffers(
		ArrayView<const Rendering::VertexPosition, Rendering::Index> vertexPositions,
		ArrayView<const Rendering::VertexNormals, Rendering::Index> vertexNormals
	)
	{
		for (const MeshSkin::Part& part : m_parts)
		{
			const Rendering::Index partVertexCount = part.GetVertexCount();

			if (vertexPositions.GetSize() < partVertexCount || vertexNormals.GetSize() < partVertexCount)
			{
				return false;
			}

			vertexPositions += partVertexCount;
			vertexNormals += partVertexCount;
		}

		return true;
	}

	void MeshSkin::ProcessSkinning(
		const SkeletonInstance& skeletonInstance,
		ArrayView<Math::Matrix4x4f, uint16> skinningMatrices,
		const Rendering::VertexPosition* pSourceVertexPositionBuffer,
		Rendering::VertexPosition* pVertexPositionBuffer,
		const Rendering::VertexNormals* pSourceVertexNormalsBuffer,
		Rendering::VertexNormals* pVertexNormalsBuffer
	) const
	{
		const ArrayView<const uint16, uint16> jointRemappingIndices = m_jointRemappingIndices;
		Assert(skinningMatrices.GetSize() == jointRemappingIndices.GetSize());

		const ArrayView<const Math::Matrix4x4f, uint16> inverseBindPoses = m_inverseBindPoses;
		const ArrayView<const Math::Matrix4x4f, uint16> modelSpaceMatrices = skeletonInstance.GetModelSpaceMatrices();
		for (uint16 i = 0, n = jointRemappingIndices.GetSize(); i < n; ++i)
		{
			skinningMatrices[i] = inverseBindPoses[i] * modelSpaceMatrices[jointRemappingIndices[i]];
		}

		for (const MeshSkin::Part& part : m_parts)
		{
			SkinningJob skinningJob;

			const Rendering::Index partVertexCount = part.GetVertexCount();

			skinningJob.vertex_count = partVertexCount;

			const uint16 partsMaximumJointsPerVertex = part.GetMaximumJointsPerVertex();
			skinningJob.influences_count = partsMaximumJointsPerVertex;

			skinningJob.joint_matrices = ozz::span<const ozz::math::Float4x4>{
				&reinterpret_cast<const ozz::math::Float4x4&>(*skinningMatrices.GetData()),
				skinningMatrices.GetSize()
			};

			// Setup joint's indices.
			skinningJob.joint_indices = ozz::span<const uint16>{part.GetJointIndices().GetData(), part.GetJointIndices().GetSize()};
			skinningJob.joint_indices_stride = sizeof(uint16_t) * partsMaximumJointsPerVertex;

			// Setup joint's weights.
			if (partsMaximumJointsPerVertex > 1)
			{
				skinningJob.joint_weights = ozz::span<const float>{part.GetJointWeights().GetData(), part.GetJointWeights().GetSize()};
				skinningJob.joint_weights_stride = sizeof(float) * (partsMaximumJointsPerVertex - 1);
			}

			// Setup vertex positions
			{
				const float* const pPositionsIn = reinterpret_cast<const float*>(pSourceVertexPositionBuffer);
				skinningJob.in_positions = {pPositionsIn, pPositionsIn + (partVertexCount * sizeof(Rendering::VertexPosition)) / sizeof(float)};
				skinningJob.in_positions_stride = sizeof(Rendering::VertexPosition);
				float* const pPositionsOut = reinterpret_cast<float*>(pVertexPositionBuffer);
				skinningJob.out_positions = {pPositionsOut, pPositionsOut + (partVertexCount * sizeof(Rendering::VertexPosition)) / sizeof(float)};
				skinningJob.out_positions_stride = sizeof(Rendering::VertexPosition);
			}

			// Setup normals if input are provided.
			{
				// Setup input normals, coming from the loaded mesh.
				const Math::CompressedDirectionAndSign* const pNormalsIn =
					reinterpret_cast<const Math::CompressedDirectionAndSign*>(pSourceVertexNormalsBuffer);
				skinningJob.in_normals = {
					pNormalsIn,
					pNormalsIn + ((partVertexCount * sizeof(Rendering::VertexNormals)) / sizeof(Math::CompressedDirectionAndSign))
				};
				skinningJob.in_normals_stride = sizeof(Rendering::VertexNormals);
				Math::CompressedDirectionAndSign* const pNormalsOut =
					reinterpret_cast<Math::CompressedDirectionAndSign*>(reinterpret_cast<ByteType*>(pVertexNormalsBuffer));
				skinningJob.out_normals = {
					pNormalsOut,
					pNormalsOut + ((partVertexCount * sizeof(Rendering::VertexNormals)) / sizeof(Math::CompressedDirectionAndSign))
				};
				skinningJob.out_normals_stride = sizeof(Rendering::VertexNormals);
			}

			// Setup tangents if input are provided.
			{
				// Setup input tangents, coming from the loaded mesh.
				const Math::CompressedDirectionAndSign* const pTangentsIn = reinterpret_cast<const Math::CompressedDirectionAndSign*>(
					reinterpret_cast<const ByteType*>(pSourceVertexNormalsBuffer) + OFFSET_OF(Rendering::VertexNormals, tangent)
				);
				skinningJob.in_tangents = {
					pTangentsIn,
					pTangentsIn + ((partVertexCount * sizeof(Rendering::VertexNormals)) / sizeof(Math::CompressedDirectionAndSign))
				};
				skinningJob.in_tangents_stride = sizeof(Rendering::VertexNormals);
				Math::CompressedDirectionAndSign* const pTangentsOut = reinterpret_cast<Math::CompressedDirectionAndSign*>(
					reinterpret_cast<ByteType*>(pVertexNormalsBuffer) + OFFSET_OF(Rendering::VertexNormals, tangent)
				);
				skinningJob.out_tangents = {
					pTangentsOut,
					pTangentsOut + ((partVertexCount * sizeof(Rendering::VertexNormals)) / sizeof(Math::CompressedDirectionAndSign))
				};
				skinningJob.out_tangents_stride = sizeof(Rendering::VertexNormals);
			}

			pVertexPositionBuffer += partVertexCount;
			pSourceVertexPositionBuffer += partVertexCount;
			pVertexNormalsBuffer += partVertexCount;
			pSourceVertexNormalsBuffer += partVertexCount;

			// Execute the job, which should succeed unless a parameter is invalid.
			skinningJob.Run();
		}
	}

	[[maybe_unused]] const bool wasMeshSkinAssetTypeRegistered = Reflection::Registry::RegisterType<MeshSkinAssetType>();
}
