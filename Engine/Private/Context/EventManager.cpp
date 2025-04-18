#include "Context/EventManager.h"

#include "Engine/Context/Utils.h"
#include "Engine/Context/Context.h"

#include <Engine/Entity/HierarchyComponentBase.h>

#include <Common/System/Query.h>
#include <Engine/Event/EventManager.h>

namespace ngine::Context
{
	EventManager::EventManager(Entity::HierarchyComponentBase& component, Entity::SceneRegistry& registry)
	{
		m_pContext = Utils::FindContext(component, registry).m_pDataComponent;
	}

	/* static */ void EventManager::Notify(Guid eventGuid, Entity::HierarchyComponentBase& component, Entity::SceneRegistry& registry)
	{
		eventGuid = Utils::GetGuid(eventGuid, component, registry);
		Events::Manager& manager = System::Get<Events::Manager>();
		manager.NotifyAll(manager.FindOrRegisterEvent(eventGuid));
	}

	void EventManager::Notify(Guid guid)
	{
		if (m_pContext)
		{
			guid = m_pContext->GetOrEmplaceGuid(guid);
		}
		Events::Manager& manager = System::Get<Events::Manager>();
		manager.NotifyAll(manager.FindOrRegisterEvent(guid));
	}

	void EventManager::Deregister(Guid guid)
	{
		if (m_pContext)
		{
			guid = m_pContext->GetOrEmplaceGuid(guid);
		}
		Events::Manager& manager = System::Get<Events::Manager>();
		manager.Deregister(manager.FindOrRegisterEvent(guid));
	}
}

namespace ngine
{
	template struct TSaltedIdentifierStorage<Events::Identifier>;
	template struct UnorderedMap<Guid, Events::Identifier, Guid::Hash>;
}
