#include "Renderer/Assets/StaticMesh/ProceduralMeshCache.h"

#include <Common/Math/Vector2.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Endian.h>
#include <Common/Math/Hash.h>
#include <Common/Math/Vector2/Hash.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <Common/System/Query.h>
#include <Renderer/Renderer.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/StaticMesh/StaticObject.h>

namespace ngine::Rendering
{
	ProceduralMeshCache::ProceduralMeshCache()
	{
	}

	ProceduralMeshCache::~ProceduralMeshCache()
	{
		Threading::UniqueLock lock(m_instanceLookupMutex);
		m_instanceLookupMap.Clear();
	}

	ProceduralMeshIdentifier ProceduralMeshCache::FindOrRegisterInstance(const Hash instanceHash, CreationCallback&& callback)
	{
		{
			Threading::SharedLock sharedLock(m_instanceLookupMutex);
			decltype(m_instanceLookupMap)::const_iterator it = m_instanceLookupMap.Find(instanceHash);
			if (it != m_instanceLookupMap.end())
			{
				return it->second;
			}
		}

		ProceduralMeshIdentifier identifier;
		{
			Threading::UniqueLock lock(m_instanceLookupMutex);
			decltype(m_instanceLookupMap)::const_iterator it = m_instanceLookupMap.Find(instanceHash);
			if (it != m_instanceLookupMap.end())
			{
				return it->second;
			}

			identifier = m_instanceIdentifiers.AcquireIdentifier();
			m_instanceLookupMap.Emplace(instanceHash, ProceduralMeshIdentifier(identifier));
		}

		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		Rendering::StaticMeshIdentifier meshIdentifier = meshCache.Create(
			[callback = Forward<CreationCallback>(callback)]([[maybe_unused]] const Rendering::StaticMeshIdentifier meshIdentifier
		  ) mutable -> Threading::JobBatch
			{
				return Threading::CreateCallback(
					[meshIdentifier, callback = Forward<CreationCallback>(callback)](Threading::JobRunnerThread&) mutable
					{
						Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
						[[maybe_unused]] Rendering::StaticMesh& mesh = *meshCache.GetAssetData(meshIdentifier).m_pMesh;

						mesh.SetStaticObjectData(callback());

						meshCache.OnMeshLoaded(meshIdentifier);
					},
					Threading::JobPriority::UserInterfaceAction
				);
			}
		);

		m_meshes[identifier] = meshIdentifier;
		return identifier;
	}

	ProceduralMeshIdentifier ProceduralMeshCache::FindInstance(const Hash instanceHash) const
	{
		Threading::SharedLock sharedLock(m_instanceLookupMutex);
		decltype(m_instanceLookupMap)::const_iterator it = m_instanceLookupMap.Find(instanceHash);
		if (it != m_instanceLookupMap.end())
		{
			return it->second;
		}
		return {};
	}

	void ProceduralMeshCache::RemoveInstance(const ProceduralMeshIdentifier identifier)
	{
		m_instanceIdentifiers.ReturnIdentifier(identifier);
	}
}
