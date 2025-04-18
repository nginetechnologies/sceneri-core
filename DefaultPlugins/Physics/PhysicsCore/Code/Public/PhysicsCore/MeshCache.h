#pragma once

#include "MeshIdentifier.h"

#include <Renderer/Assets/StaticMesh/StaticMeshIdentifier.h>

#include <Engine/Asset/AssetType.h>

#include <Common/Function/Function.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Storage/IdentifierArray.h>

#include <PhysicsCore/3rdparty/jolt/Jolt.h>
#include <PhysicsCore/3rdparty/jolt/Core/Reference.h>

namespace JPH
{
	class Shape;
	using ShapeRef = Ref<Shape>;
}

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine::Physics
{
	using MeshLoadEvent = ThreadSafe::Event<void(void*, JPH::ShapeRef pShape), 24, false>;
	using MeshLoadListenerData = MeshLoadEvent::ListenerData;
	using MeshLoadListenerIdentifier = MeshLoadEvent::ListenerIdentifier;

	struct MeshInfo
	{
		JPH::ShapeRef m_pConvexShape;
		JPH::ShapeRef m_pTriangleMeshShape;
		MeshLoadEvent m_onLoadConvexEvent;
		MeshLoadEvent m_onLoadTriangleEvent;
	};

	struct MeshCache final : public Asset::Type<MeshIdentifier, MeshInfo>
	{
		using BaseType = Type;

		MeshCache(Asset::Manager& assetManager);
		virtual ~MeshCache();

		[[nodiscard]] MeshIdentifier FindOrRegisterAsset(const Asset::Guid guid);
		[[nodiscard]] MeshIdentifier RegisterAsset(const Asset::Guid guid);

		[[nodiscard]] MeshIdentifier FindOrRegisterAsset(const Rendering::StaticMeshIdentifier);

		using LoadCallback = Function<void(JPH::ShapeRef&& pShape), 24>;
		[[nodiscard]] Threading::JobBatch TryLoadOrCreateConvexMesh(
			const MeshIdentifier identifier, const Rendering::StaticMeshIdentifier renderMeshIdentifier, MeshLoadListenerData&& listenerData
		);
		[[nodiscard]] Threading::JobBatch TryLoadOrCreateTriangleMesh(
			const MeshIdentifier identifier, const Rendering::StaticMeshIdentifier renderMeshIdentifier, MeshLoadListenerData&& listenerData
		);

		[[nodiscard]] Threading::JobBatch TryLoadMesh(const MeshIdentifier identifier, Asset::Manager& assetManager, LoadCallback&& callback);
	protected:
#if DEVELOPMENT_BUILD
		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override;
#endif
	protected:
		Threading::AtomicIdentifierMask<MeshIdentifier> m_loadingConvexMeshes;
		Threading::AtomicIdentifierMask<MeshIdentifier> m_loadingTriangleMeshes;
		TIdentifierArray<Threading::Atomic<typename MeshIdentifier::IndexType>, Rendering::StaticMeshIdentifier> m_meshIdentifierIndexLookup{
			Memory::Zeroed
		};
	};
}
