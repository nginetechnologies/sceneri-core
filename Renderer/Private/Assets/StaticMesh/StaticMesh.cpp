#include "Assets/StaticMesh/StaticMesh.h"
#include "Assets/StaticMesh/MeshAssetType.h"

#include <Renderer/Assets/StaticMesh/VertexNormals.h>

#include <Common/Math/Vector2.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Rendering
{
	StaticMesh::StaticMesh(const StaticMeshIdentifier identifier, const StaticMeshIdentifier templateIdentifier, const EnumFlags<Flags> flags)
		: m_identifier(identifier)
		, m_templateIdentifier(templateIdentifier)
		, m_flags(flags & (Flags::AllowCpuVertexAccess | Flags::IsClone))
	{
	}

	StaticObject StaticMesh::SetStaticObjectData(StaticObject&& object)
	{
		Assert(object.GetVertexCount() > 0);
		Assert(object.GetIndexCount() > 0);
		Assert(!object.m_boundingBox.IsZero());
		StaticObject previousObject = Move(m_object);
		m_object = Forward<StaticObject>(object);
		OnBoundingBoxChanged();
		return previousObject;
	}

	ConstByteView StaticMesh::GetVertexData() const LIFETIME_BOUND
	{
		const uint32 dataSize = static_cast<uint32>(
			reinterpret_cast<uintptr>(GetVertexTextureCoordinates().end().Get()) - reinterpret_cast<uintptr>(GetVertexPositions().begin().Get())
		);
		return ConstByteView{reinterpret_cast<const ByteType*>(GetVertexPositions().GetData()), dataSize};
	}

	[[maybe_unused]] const bool wasSceneTypeRegistered = Reflection::Registry::RegisterType<MeshSceneAssetType>();
	[[maybe_unused]] const bool wasPartTypeRegistered = Reflection::Registry::RegisterType<MeshPartAssetType>();
}
