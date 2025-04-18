#pragma once

#include "ProceduralMeshIdentifier.h"

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/Vector2.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>
#include <Common/Function/Function.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/EnumFlags.h>

#include <Engine/Asset/AssetType.h>

#include <Renderer/Assets/StaticMesh/StaticMeshIdentifier.h>
#include <Renderer/Index.h>

namespace ngine
{
	struct Engine;
}

namespace ngine::Rendering
{
	struct StaticObject;
}

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Rendering
{
	struct ProceduralMeshCache
	{
		using Hash = uint64;

		ProceduralMeshCache();
		~ProceduralMeshCache();

		using CreationCallback = Function<StaticObject(void), 24>;
		[[nodiscard]] ProceduralMeshIdentifier FindOrRegisterInstance(const Hash hash, CreationCallback&& callback);
		[[nodiscard]] ProceduralMeshIdentifier FindInstance(const Hash hash) const;
		void RemoveInstance(const ProceduralMeshIdentifier identifier);

		[[nodiscard]] Optional<StaticMeshIdentifier> GetInstanceStaticObject(const ProceduralMeshIdentifier identifier) const
		{
			if (m_meshes[identifier].IsValid())
			{
				return m_meshes[identifier];
			}
			else
			{
				return Invalid;
			}
		}
	protected:
		mutable Threading::SharedMutex m_instanceLookupMutex;
		UnorderedMap<Hash, ProceduralMeshIdentifier> m_instanceLookupMap;
		TSaltedIdentifierStorage<ProceduralMeshIdentifier> m_instanceIdentifiers;

		TIdentifierArray<StaticMeshIdentifier, ProceduralMeshIdentifier> m_meshes;
	};
}
