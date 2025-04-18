#pragma once

#include "ReflectedEvent.h"
#include <Engine/Event/EventManager.h>
#include <Common/System/Query.h>

namespace ngine
{
	template<typename... ArgumentTypes_>
	inline ReflectedEvent<void(ArgumentTypes_...)>::ReflectedEvent()
		: m_identifier(System::Get<Events::Manager>().RegisterEvent())
	{
		Assert(m_identifier.IsValid());
	}
	template<typename... ArgumentTypes_>
	inline ReflectedEvent<void(ArgumentTypes_...)>::~ReflectedEvent()
	{
		System::Get<Events::Manager>().Deregister(m_identifier);
	}

	template<typename... ArgumentTypes_>
	inline void ReflectedEvent<void(ArgumentTypes_...)>::NotifyAll(ArgumentTypes_... args)
	{
		System::Get<Events::Manager>().NotifyAll(m_identifier, Forward<ArgumentTypes_>(args)...);
	}

	template<typename... ArgumentTypes_>
	inline void ReflectedEvent<void(ArgumentTypes_...)>::operator()(ArgumentTypes_... args)
	{
		System::Get<Events::Manager>().NotifyAll(m_identifier, Forward<ArgumentTypes_>(args)...);
	}

	template<typename... ArgumentTypes_>
	template<typename InstanceIdentifierType, typename... Args>
	inline void ReflectedEvent<void(ArgumentTypes_...)>::NotifyOne(const InstanceIdentifierType& instanceIdentifier, Args... args)
	{
		System::Get<Events::Manager>().NotifyOne(m_identifier, instanceIdentifier, Forward<ArgumentTypes_>(args)...);
	}

	template<typename... ArgumentTypes_>
	template<auto Callback, typename ObjectType>
	inline void ReflectedEvent<void(ArgumentTypes_...)>::Subscribe(ObjectType& object)
	{
		System::Get<Events::Manager>().Subscribe<Callback>(m_identifier, object);
	}

	template<typename... ArgumentTypes_>
	inline void
	ReflectedEvent<void(ArgumentTypes_...)>::Subscribe(const Events::ListenerUserData listenerIdentifier, const Events::Function callback)
	{
		System::Get<Events::Manager>().Subscribe(m_identifier, listenerIdentifier, callback);
	}

	template<typename... ArgumentTypes_>
	template<typename Identifier, typename CallbackObject, typename>
	inline void ReflectedEvent<void(ArgumentTypes_...)>::Subscribe(const Identifier identifier, CallbackObject&& callback)
	{
		System::Get<Events::Manager>().Subscribe<Identifier, CallbackObject>(m_identifier, identifier, Forward<CallbackObject>(callback));
	}

	template<typename... ArgumentTypes_>
	inline bool ReflectedEvent<void(ArgumentTypes_...)>::Unsubscribe(const Events::ListenerUserData listenerIdentifier)
	{
		return System::Get<Events::Manager>().Unsubscribe(m_identifier, listenerIdentifier);
	}

	template<typename... ArgumentTypes_>
	template<typename ObjectType>
	inline bool ReflectedEvent<void(ArgumentTypes_...)>::Unsubscribe(ObjectType& object)
	{
		return System::Get<Events::Manager>().Unsubscribe(m_identifier, object);
	}
}
