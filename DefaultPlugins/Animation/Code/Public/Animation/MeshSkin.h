#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>
#include <Common/Math/Matrix4x4.h>
#include <Renderer/Index.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>

namespace ngine::IO
{
	struct FileView;
}

namespace ngine::Rendering
{
	struct VertexNormals;
}

namespace ngine::Animation
{
	struct SkeletonInstance;

	struct MeshSkin
	{
		MeshSkin() = default;
		MeshSkin(const ConstByteView data);
		MeshSkin(const ConstByteView data, const bool);
		MeshSkin(Vector<uint16, uint16>&& jointRemappingIndices, Vector<Math::Matrix4x4f, uint16>&& inverseBindPoses)
			: m_jointRemappingIndices(Forward<decltype(jointRemappingIndices)>(jointRemappingIndices))
			, m_inverseBindPoses(Forward<decltype(inverseBindPoses)>(inverseBindPoses))
		{
		}

		struct Part
		{
			Part(
				const Rendering::Index vertexCount,
				const ArrayView<const uint16, Rendering::Index> jointIndices,
				const ArrayView<const float, Rendering::Index> jointWeights
			)
				: m_vertexCount(vertexCount)
				, m_jointIndices(jointIndices)
				, m_jointWeights(jointWeights)
			{
			}
			Part(
				const Rendering::Index vertexCount, Vector<uint16, Rendering::Index>&& jointIndices, Vector<float, Rendering::Index>&& jointWeights
			)
				: m_vertexCount(vertexCount)
				, m_jointIndices(Forward<decltype(jointIndices)>(jointIndices))
				, m_jointWeights(Forward<decltype(jointWeights)>(jointWeights))
			{
			}
			Part(ConstByteView& data);
			Part(ConstByteView& data, const bool);

			void WriteToFile(const IO::FileView outputFile) const;

			[[nodiscard]] Rendering::Index GetVertexCount() const
			{
				return m_vertexCount;
			}

			[[nodiscard]] uint16 GetMaximumJointsPerVertex() const
			{
				Expect(m_vertexCount > 0);
				return (uint16)(m_jointIndices.GetSize() / m_vertexCount);
			}

			[[nodiscard]] ArrayView<const uint16, Rendering::Index> GetJointIndices() const
			{
				return m_jointIndices;
			}

			[[nodiscard]] ArrayView<const float, Rendering::Index> GetJointWeights() const
			{
				return m_jointWeights;
			}
		protected:
			Rendering::Index m_vertexCount;
			Vector<uint16, Rendering::Index> m_jointIndices;
			Vector<float, Rendering::Index> m_jointWeights;
		};

		[[nodiscard]] ArrayView<const Part, uint16> GetParts() const
		{
			return m_parts;
		}

		template<typename... Args>
		void EmplacePart(Args&&... args)
		{
			m_parts.EmplaceBack(Forward<Args>(args)...);
		}

		[[nodiscard]] ArrayView<const uint16, uint16> GetJointRemappingIndices() const
		{
			return m_jointRemappingIndices;
		}

		// Returns the highest joint number used in the skeleton.
		[[nodiscard]] uint16 GetHighestJointRemappingIndex() const
		{
			// Takes advantage that joint_remaps is sorted.
			return m_jointRemappingIndices.HasElements() ? m_jointRemappingIndices.GetLastElement() : 0;
		}

		[[nodiscard]] ArrayView<const Math::Matrix4x4f, uint16> GetInverseBindPoses() const
		{
			return m_inverseBindPoses;
		}
		[[nodiscard]] ArrayView<Math::Matrix4x4f, uint16> GetInverseBindPoses()
		{
			return m_inverseBindPoses;
		}

		void WriteToFile(const IO::FileView outputFile) const;

		void ProcessSkinning(
			const SkeletonInstance& skeletonInstance,
			ArrayView<Math::Matrix4x4f, uint16> skinningMatrices,
			const Rendering::VertexPosition* pSourceVertexPositionBuffer,
			Rendering::VertexPosition* pVertexPositionBuffer,
			const Rendering::VertexNormals* pSourceVertexNormalsBuffer,
			Rendering::VertexNormals* pVertexNormalsBuffer
		) const;
		[[nodiscard]] bool ValidateVertexBuffers(
			const ArrayView<const Rendering::VertexPosition, Rendering::Index> vertexPositions,
			const ArrayView<const Rendering::VertexNormals, Rendering::Index> vertexNormals
		);

		static Vector<Part, uint16> ReadParts(ConstByteView& data);
		static Vector<Part, uint16> ReadParts(ConstByteView& data, const bool);

		[[nodiscard]] bool IsValid() const
		{
			return m_parts.HasElements();
		}
	protected:
		Vector<Part, uint16> m_parts;

		// Joints remapping indices. As a skin might be influenced by a part of the
		// skeleton only, joint indices and inverse bind pose matrices are reordered
		// to contain only used ones. Note that this array is sorted.
		Vector<uint16, uint16> m_jointRemappingIndices;
		// Inverse bind-pose matrices.
		Vector<Math::Matrix4x4f, uint16> m_inverseBindPoses;
	};
}
