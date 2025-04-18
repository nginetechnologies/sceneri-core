#pragma once

#include <Renderer/Assets/StaticMesh/StaticMeshIdentifier.h>
#include <Renderer/Assets/StaticMesh/StaticObject.h>
#include <Renderer/Assets/StaticMesh/StaticMeshFlags.h>

#include <Common/Function/ThreadSafeEvent.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/Storage/Identifier.h>

namespace ngine::Rendering
{
	struct StaticObject;

	struct StaticMesh
	{
		using Flags = StaticMeshFlags;

		explicit StaticMesh(
			const StaticMeshIdentifier identifier, const StaticMeshIdentifier templateIdentifier, const EnumFlags<Flags> flags = {}
		);
		StaticMesh(const StaticMesh& other) = delete;
		StaticMesh& operator=(const StaticMesh& other) = delete;
		StaticMesh(StaticMesh&& other) = default;
		StaticMesh& operator=(StaticMesh&& other) = default;
		~StaticMesh() = default;

		[[nodiscard]] StaticMeshIdentifier GetIdentifier() const
		{
			return m_identifier;
		}
		//! Gets the identifier of the mesh this mesh was cloned from
		[[nodiscard]] StaticMeshIdentifier GetTemplateIdentifier() const
		{
			return m_templateIdentifier;
		}
		[[nodiscard]] EnumFlags<Flags> GetFlags() const
		{
			return m_flags.GetFlags();
		}
		[[nodiscard]] const Math::BoundingBox GetBoundingBox() const
		{
			return m_object.m_boundingBox;
		}
		void SetBoundingBox(const Math::BoundingBox boundingBox)
		{
			m_object.m_boundingBox = boundingBox;
		}

		[[nodiscard]] Rendering::Index GetVertexCount() const
		{
			return m_object.GetVertexCount();
		}

		[[nodiscard]] const ArrayView<VertexPosition, Index> GetVertexPositions() LIFETIME_BOUND
		{
			return m_object.GetVertexElementView<VertexPosition>();
		}
		[[nodiscard]] const ArrayView<const VertexPosition, Index> GetVertexPositions() const LIFETIME_BOUND
		{
			return m_object.GetVertexElementView<VertexPosition>();
		}
		[[nodiscard]] const ArrayView<VertexNormals, Index> GetVertexNormals() LIFETIME_BOUND
		{
			return m_object.GetVertexElementView<VertexNormals>();
		}
		[[nodiscard]] const ArrayView<const VertexNormals, Index> GetVertexNormals() const LIFETIME_BOUND
		{
			return m_object.GetVertexElementView<VertexNormals>();
		}
		[[nodiscard]] const ArrayView<VertexTextureCoordinate, Index> GetVertexTextureCoordinates() LIFETIME_BOUND
		{
			return m_object.GetVertexElementView<VertexTextureCoordinate>();
		}
		[[nodiscard]] const ArrayView<const VertexTextureCoordinate, Index> GetVertexTextureCoordinates() const LIFETIME_BOUND
		{
			return m_object.GetVertexElementView<VertexTextureCoordinate>();
		}
		[[nodiscard]] ConstByteView GetVertexData() const LIFETIME_BOUND;

		[[nodiscard]] const ArrayView<Index, Index> GetIndices() LIFETIME_BOUND
		{
			return m_object.GetIndices();
		}
		[[nodiscard]] const ArrayView<const Index, Index> GetIndices() const LIFETIME_BOUND
		{
			return m_object.GetIndices();
		}

		[[nodiscard]] bool IsLoaded() const
		{
			return m_flags.IsSet(Flags::WasLoaded);
		}
		[[nodiscard]] bool DidLoadingFail() const
		{
			return m_flags.IsSet(Flags::FailedLoading);
		}
		[[nodiscard]] bool HasFinishedLoading() const
		{
			return m_flags.AreAnySet(Flags::HasFinishedLoading);
		}
		[[nodiscard]] bool IsClone() const
		{
			return m_flags.IsSet(Flags::IsClone);
		}
		[[nodiscard]] bool ShouldAllowCpuVertexAccess() const
		{
			return m_flags.IsSet(Flags::AllowCpuVertexAccess);
		}

		mutable ThreadSafe::Event<void(void*), 24> OnBoundingBoxChanged;

		StaticObject SetStaticObjectData(StaticObject&& object);

		void OnLoaded()
		{
			[[maybe_unused]] const bool wasSet = m_flags.TrySetFlags(Flags::WasLoaded);
			// No need for assert, we are allowed to reload meshes and have the old version working until the last second
			// Assert(wasSet);
		}
		void OnLoadingFailed()
		{
			[[maybe_unused]] const bool wasSet = m_flags.TrySetFlags(Flags::FailedLoading);
			Assert(wasSet);
		}

		[[nodiscard]] const Rendering::StaticObject& GetStaticObjectData() const
		{
			return m_object;
		}
		[[nodiscard]] Rendering::StaticObject& GetStaticObjectData()
		{
			return m_object;
		}
	protected:
		StaticMeshIdentifier m_identifier;
		StaticMeshIdentifier m_templateIdentifier;
		Rendering::StaticObject m_object;
		AtomicEnumFlags<Flags> m_flags;
	};
}
