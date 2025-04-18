#pragma once

#include <Engine/Event/Identifier.h>
#include <Common/TypeTraits/IsInvocable.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/Register.h>
#include <Common/Storage/Identifier.h>

namespace ngine
{
	namespace Scripting::VM
	{
		struct DynamicDelegate;
		struct DynamicFunction;
		struct DynamicEvent;
	}

	namespace Events
	{
		using Delegate = Scripting::VM::DynamicDelegate;
		using Function = Scripting::VM::DynamicFunction;
		using Event = Scripting::VM::DynamicEvent;
		using ListenerUserData = Scripting::VM::Register;
	}

	template<typename SignatureType>
	struct ReflectedEvent;

	template<typename... ArgumentTypes_>
	struct ReflectedEvent<void(ArgumentTypes_...)>
	{
		using SignatureType = void (*)(ArgumentTypes_...);
		using ArgumentTypes = Tuple<ArgumentTypes_...>;

		using RuntimeIdentifier = Events::Identifier;

		ReflectedEvent();
		~ReflectedEvent();

		void operator()(ArgumentTypes_... args);
		void NotifyAll(ArgumentTypes_... args);
		template<typename InstanceIdentifierType, typename... Args>
		void NotifyOne(const InstanceIdentifierType& instanceIdentifier, Args... args);

		template<auto Callback, typename ObjectType>
		void Subscribe(ObjectType& object);
		void Subscribe(const Events::ListenerUserData listenerIdentifier, const Events::Function callback);
		template<
			typename Identifier,
			typename CallbackObject,
			typename =
				EnableIf<TypeTraits::HasFunctionCallOperator<CallbackObject> && TypeTraits::IsInvocable<CallbackObject, void, ArgumentTypes_...>>>
		void Subscribe(const Identifier identifier, CallbackObject&& callback);

		bool Unsubscribe(const Events::ListenerUserData listenerIdentifier);
		template<typename ObjectType>
		bool Unsubscribe(ObjectType& object);
	protected:
		RuntimeIdentifier m_identifier;
	};
}
