#pragma once

#include <Engine/Context/EventManager.h>
#include "Engine/Context/Utils.h"
#include "Engine/Context/Context.h"

#include <Engine/Event/EventManager.h>

#include <Common/System/Query.h>

namespace ngine::Entity
{
	struct HierarchyComponentBase;
	struct SceneRegistry;
}

namespace ngine::Context
{
	template<auto Callback, typename ObjectType>
	/* static */ void
	EventManager::Subscribe(Guid guid, ObjectType& object, Entity::HierarchyComponentBase& component, Entity::SceneRegistry& registry)
	{
		guid = Utils::GetGuid(guid, component, registry);

		Events::Manager& eventManager = System::Get<Events::Manager>();
		auto evid = eventManager.FindOrRegisterEvent(guid);
		eventManager.Subscribe<Callback>(evid, object);
	}

	template<auto Callback, typename ObjectType>
	void EventManager::Subscribe(Guid guid, ObjectType& object)
	{
		if (m_pContext)
		{
			guid = m_pContext->GetOrEmplaceGuid(guid);
		}

		Events::Manager& eventManager = System::Get<Events::Manager>();
		auto identifier = eventManager.FindOrRegisterEvent(guid);
		eventManager.Subscribe<Callback>(identifier, object);
	}

	template<typename... Args>
	void EventManager::Notify(Guid guid, Args&&... args)
	{
		if (m_pContext)
		{
			guid = m_pContext->GetOrEmplaceGuid(guid);
		}

		Events::Manager& manager = System::Get<Events::Manager>();
		manager.NotifyAll(manager.FindOrRegisterEvent(guid), Forward<Args>(args)...);
	}

	template<typename InstanceIdentifierType, typename CallbackObject>
	EnableIf<TypeTraits::HasFunctionCallOperator<CallbackObject>>
	EventManager::Subscribe(Guid guid, const InstanceIdentifierType instanceIdentifier, CallbackObject&& callback)
	{
		if (m_pContext)
		{
			guid = m_pContext->GetOrEmplaceGuid(guid);
		}

		Events::Manager& eventManager = System::Get<Events::Manager>();
		auto evid = eventManager.FindOrRegisterEvent(guid);
		eventManager.Subscribe(evid, instanceIdentifier, Forward<CallbackObject>(callback));
	}

	template<typename ObjectType>
	void EventManager::Unsubscribe(Guid guid, ObjectType& object)
	{
		if (m_pContext)
		{
			guid = m_pContext->GetOrEmplaceGuid(guid);
		}

		Events::Manager& eventManager = System::Get<Events::Manager>();
		eventManager.Unsubscribe(eventManager.FindOrRegisterEvent(guid), object);
	}

	template<typename InstanceIdentifierType, typename CallbackObject>
	/* static */ EnableIf<TypeTraits::HasFunctionCallOperator<CallbackObject>> EventManager::Subscribe(
		Guid guid,
		const InstanceIdentifierType instanceIdentifier,
		Entity::HierarchyComponentBase& component,
		Entity::SceneRegistry& registry,
		CallbackObject&& callback
	)
	{
		guid = Utils::GetGuid(guid, component, registry);

		Events::Manager& eventManager = System::Get<Events::Manager>();
		auto evid = eventManager.FindOrRegisterEvent(guid);
		eventManager.Subscribe(evid, instanceIdentifier, Forward<CallbackObject>(callback));
	}

	template<typename ObjectType>
	/* static */ void
	EventManager::Unsubscribe(Guid guid, ObjectType& object, Entity::HierarchyComponentBase& component, Entity::SceneRegistry& registry)
	{
		guid = Utils::GetGuid(guid, component, registry);

		Events::Manager& eventManager = System::Get<Events::Manager>();
		eventManager.Unsubscribe(eventManager.FindOrRegisterEvent(guid), object);
	}
}
