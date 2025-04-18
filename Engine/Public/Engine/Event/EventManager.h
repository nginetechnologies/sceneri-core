#pragma once

#include "Identifier.h"

#include <Common/System/SystemType.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/DynamicEvent.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Threading/Mutexes/SharedMutex.h>

namespace ngine::Events
{
	using Mask = IdentifierMask<Identifier>;
	using Function = Scripting::VM::DynamicFunction;
	using Delegate = Scripting::VM::DynamicDelegate;
	using Event = Scripting::VM::DynamicEvent;
	using DynamicArgument = Scripting::VM::Register;
	inline static constexpr uint8 MaxiumArgumentCount = Scripting::VM::RegisterCount - 1;
	using ListenerUserData = Event::ListenerUserData;
}

namespace ngine
{
	extern template struct TSaltedIdentifierStorage<Events::Identifier>;
	extern template struct UnorderedMap<Guid, Events::Identifier, Guid::Hash>;
}

namespace ngine::Events
{
	struct Manager
	{
		inline static constexpr System::Type SystemType = System::Type::EventManager;

		[[nodiscard]] Identifier RegisterEvent()
		{
			const Identifier identifier = m_identifiers.AcquireIdentifier();
			return identifier;
		}

		Identifier RegisterEvent(const Guid guid)
		{
			Assert(guid.IsValid());
			const Identifier identifier = m_identifiers.AcquireIdentifier();
			{
				Threading::UniqueLock lock(m_lookupMapMutex);
				Assert(!m_lookupMap.Contains(guid));
				m_lookupMap.Emplace(Guid(guid), Identifier(identifier));
			}
			return identifier;
		}

		[[nodiscard]] Identifier FindEvent(const Guid guid)
		{
			Assert(guid.IsValid());
			Threading::SharedLock lock(m_lookupMapMutex);
			auto it = m_lookupMap.Find(guid);
			if (it != m_lookupMap.end())
			{
				return it->second;
			}
			return {};
		}

		Identifier FindOrRegisterEvent(const Guid guid)
		{
			Assert(guid.IsValid());
			{
				const Identifier existingIdentifier = FindEvent(guid);
				if (existingIdentifier.IsValid())
				{
					return existingIdentifier;
				}
			}

			Threading::UniqueLock lock(m_lookupMapMutex);
			{
				auto it = m_lookupMap.Find(guid);
				if (it != m_lookupMap.end())
				{
					return it->second;
				}
			}

			const Identifier identifier = m_identifiers.AcquireIdentifier();
			{
				Assert(!m_lookupMap.Contains(guid));
				m_lookupMap.Emplace(Guid(guid), Identifier(identifier));
			}
			return identifier;
		}
		void Deregister(const Identifier identifier)
		{
			{
				Threading::UniqueLock lock(m_eventMutexes[identifier]);
				m_events[identifier].Clear();
			}
			m_identifiers.ReturnIdentifier(identifier);
		}

		void Deregister(const Identifier identifier, const Guid guid)
		{
			{
				Threading::UniqueLock lock(m_eventMutexes[identifier]);
				m_events[identifier].Clear();
			}
			m_identifiers.ReturnIdentifier(identifier);

			{
				Threading::UniqueLock lock(m_lookupMapMutex);
				auto it = m_lookupMap.Find(guid);
				if (it != m_lookupMap.end())
				{
					m_lookupMap.Remove(it);
				}
			}
		}
		template<typename... Args>
		void NotifyAll(const Identifier identifier, Args&&... args)
		{
			// TODO: Queue on jobs if necessary
			Threading::SharedLock lock(m_eventMutexes[identifier]);
			m_events[identifier](Forward<Args>(args)...);
		}
		void NotifyAll(
			const Identifier identifier,
			const DynamicArgument R1,
			const DynamicArgument R2,
			const DynamicArgument R3,
			const DynamicArgument R4,
			const DynamicArgument R5
		)
		{
			// TODO: Queue on jobs if necessary
			Threading::SharedLock lock(m_eventMutexes[identifier]);
			m_events[identifier](R1, R2, R3, R4, R5);
		}
		template<typename... Args>
		void NotifyAll(const Mask& events, Args&&... args)
		{
			// TODO: Queue on jobs if necessary

			for (const Identifier::IndexType identifierIndex : events.GetSetBitsIterator())
			{
				Threading::SharedLock lock(m_eventMutexes[Identifier::MakeFromValidIndex(identifierIndex)]);
				m_events[Identifier::MakeFromValidIndex(identifierIndex)](Forward<Args>(args)...);
			}
		}
		void NotifyAll(
			const Mask& events,
			const DynamicArgument R1,
			const DynamicArgument R2,
			const DynamicArgument R3,
			const DynamicArgument R4,
			const DynamicArgument R5
		)
		{
			// TODO: Queue on jobs if necessary
			for (const Identifier::IndexType identifierIndex : events.GetSetBitsIterator())
			{
				Threading::SharedLock lock(m_eventMutexes[Identifier::MakeFromValidIndex(identifierIndex)]);
				m_events[Identifier::MakeFromValidIndex(identifierIndex)](R1, R2, R3, R4, R5);
			}
		}
		template<typename InstanceIdentifierType, typename... Args>
		bool NotifyOne(const Identifier identifier, const InstanceIdentifierType& instanceIdentifier, Args&&... args)
		{
			Threading::SharedLock lock(m_eventMutexes[identifier]);
			return m_events[identifier].BroadcastTo(instanceIdentifier, Forward<Args>(args)...);
		}
		bool NotifyOne(
			const Identifier identifier,
			const ListenerUserData listenerUserData,
			const DynamicArgument R1,
			const DynamicArgument R2,
			const DynamicArgument R3,
			const DynamicArgument R4,
			const DynamicArgument R5
		)
		{
			Threading::SharedLock lock(m_eventMutexes[identifier]);
			return m_events[identifier].BroadcastTo(listenerUserData, R1, R2, R3, R4, R5);
		}

		void Subscribe(const Identifier identifier, const ListenerUserData listenerIdentifier, const Function callback)
		{
			Threading::UniqueLock lock(m_eventMutexes[identifier]);
			m_events[identifier].Emplace(Delegate(listenerIdentifier, callback));
		}

		void SubscribeWithDuplicates(const Identifier identifier, const ListenerUserData listenerIdentifier, const Function callback)
		{
			Threading::UniqueLock lock(m_eventMutexes[identifier]);
			m_events[identifier].EmplaceWithDuplicates(Delegate{listenerIdentifier, callback});
		}

		template<auto Callback, typename ObjectType>
		void Subscribe(const Identifier identifier, ObjectType& object)
		{
			Threading::UniqueLock lock(m_eventMutexes[identifier]);
			m_events[identifier].Emplace(Delegate::Make<Callback>(object));
		}

		template<auto Callback, typename ObjectType>
		void SubscribeWithDuplicates(const Identifier identifier, ObjectType& object)
		{
			Threading::UniqueLock lock(m_eventMutexes[identifier]);
			m_events[identifier].EmplaceWithDuplicates(Delegate::Make<Callback>(object));
		}

		template<
			typename InstanceIdentifierType,
			typename CallbackObject,
			typename = EnableIf<TypeTraits::HasFunctionCallOperator<CallbackObject>>>
		void Subscribe(const Identifier identifier, const InstanceIdentifierType instanceIdentifier, CallbackObject&& callback)
		{
			Threading::UniqueLock lock(m_eventMutexes[identifier]);
			m_events[identifier].Emplace(
				Delegate::Make<InstanceIdentifierType, CallbackObject>(instanceIdentifier, Forward<CallbackObject>(callback))
			);
		}

		template<
			typename InstanceIdentifierType,
			typename CallbackObject,
			typename = EnableIf<TypeTraits::HasFunctionCallOperator<CallbackObject>>>
		void SubscribeWithDuplicates(const Identifier identifier, const InstanceIdentifierType instanceIdentifier, CallbackObject&& callback)
		{
			Threading::UniqueLock lock(m_eventMutexes[identifier]);
			m_events[identifier].EmplaceWithDuplicates(
				Delegate::Make<InstanceIdentifierType, CallbackObject>(instanceIdentifier, Forward<CallbackObject>(callback))
			);
		}

		template<typename InstanceIdentifierType>
		[[nodiscard]] bool IsSubscribed(const Identifier identifier, const InstanceIdentifierType listenerIdentifier) const
		{
			Threading::SharedLock lock(m_eventMutexes[identifier]);
			return m_events[identifier].Contains<InstanceIdentifierType>(listenerIdentifier);
		}

		[[nodiscard]] bool IsSubscribed(const Identifier identifier, const ListenerUserData listenerIdentifier) const
		{
			Threading::SharedLock lock(m_eventMutexes[identifier]);
			return m_events[identifier].Contains(listenerIdentifier);
		}

		bool Unsubscribe(const Identifier identifier, const ListenerUserData listenerIdentifier)
		{
			Threading::UniqueLock lock(m_eventMutexes[identifier]);
			return m_events[identifier].Remove(listenerIdentifier);
		}
		template<typename ObjectType>
		bool Unsubscribe(const Identifier identifier, const ObjectType& object)
		{
			Threading::UniqueLock lock(m_eventMutexes[identifier]);
			return m_events[identifier].Remove(object);
		}

		//! Unsubscribes all listeners to the specific event using the specified listener identifier
		void UnsubscribeAll(const Identifier identifier, const ListenerUserData listenerIdentifier)
		{
			Threading::UniqueLock lock(m_eventMutexes[identifier]);
			m_events[identifier].RemoveAll(listenerIdentifier);
		}
		//! Unsubscribes all listeners to the specific event using the specified listener identifier
		template<typename ObjectType>
		void UnsubscribeAll(const Identifier identifier, const ObjectType& object)
		{
			Threading::UniqueLock lock(m_eventMutexes[identifier]);
			m_events[identifier].RemoveAll(object);
		}
	protected:
		TSaltedIdentifierStorage<Identifier> m_identifiers;
		TIdentifierArray<Event, Identifier> m_events;
		mutable TIdentifierArray<Threading::SharedMutex, Identifier> m_eventMutexes;

		mutable Threading::SharedMutex m_lookupMapMutex;
		UnorderedMap<Guid, Identifier, Guid::Hash> m_lookupMap;
	};
}
