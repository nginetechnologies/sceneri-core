#include "Entity/Manager.h"

#include "Engine.h"

#include <Engine/Asset/AssetManager.h>
#include "Entity/ComponentTypeSceneData.h"
#include "Entity/ComponentRegistry.h"

#include <Common/Memory/OffsetOf.h>
#include <Common/System/Query.h>

namespace ngine::Entity
{
	Manager::Manager()
		: m_registry()
		, m_sceneTemplateCache(System::Get<Asset::Manager>())
	{
	}
	Manager::~Manager() = default;
}
