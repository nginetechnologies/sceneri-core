#pragma once

#include "PropertySourceIdentifier.h"
#include <Common/Storage/Identifier.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Threading/Mutexes/UniqueLock.h>
#include <Common/Threading/Mutexes/SharedLock.h>
#include <Common/Function/ThreadSafeEvent.h>

namespace ngine::PropertySource
{
	struct Interface;
}

namespace ngine
{
	extern template struct TSaltedIdentifierStorage<PropertySource::Identifier>;
	extern template struct TIdentifierArray<Optional<PropertySource::Interface*>, PropertySource::Identifier>;
	extern template struct UnorderedMap<Guid, PropertySource::Identifier, Guid::Hash>;
	extern template struct UnorderedMap<PropertySource::Identifier, Guid, PropertySource::Identifier::Hash>;
}

namespace ngine::PropertySource
{
	//! Cache responsible for keeping track of available data sources and their identifiers
	struct Cache
	{
		Identifier Register(const Guid dataSourceGuid)
		{
			const Identifier identifier = m_identifierStorage.AcquireIdentifier();
			Assert(identifier.IsValid());
			if (LIKELY(identifier.IsValid()))
			{
				{
					Threading::UniqueLock lock(m_guidLookupMapMutex);
					Assert(!m_guidLookupMap.Contains(dataSourceGuid));
					m_guidLookupMap.Emplace(Guid(dataSourceGuid), Identifier(identifier));
				}
				{
					Threading::UniqueLock lock(m_idLookupMapMutex);
					Assert(!m_idLookupMap.Contains(identifier));
					m_idLookupMap.Emplace(Identifier(identifier), Guid(dataSourceGuid));
				}
			}
			else
			{
				Assert(Find(dataSourceGuid).IsValid());
			}
			return identifier;
		}
		[[nodiscard]] Identifier FindOrRegister(const Guid dataSourceGuid)
		{
			{
				const Identifier identifier = Find(dataSourceGuid);
				if (identifier.IsValid())
				{
					return identifier;
				}
			}

			Threading::UniqueLock lock(m_guidLookupMapMutex);
			{
				auto it = m_guidLookupMap.Find(dataSourceGuid);
				if (it != m_guidLookupMap.end())
				{
					return it->second;
				}
			}

			const Identifier identifier = m_identifierStorage.AcquireIdentifier();
			Assert(identifier.IsValid());
			if (LIKELY(identifier.IsValid()))
			{
				Assert(!m_guidLookupMap.Contains(dataSourceGuid));
				m_guidLookupMap.Emplace(Guid(dataSourceGuid), Identifier(identifier));

				{
					Threading::UniqueLock idLock(m_idLookupMapMutex);
					Assert(!m_idLookupMap.Contains(identifier));
					m_idLookupMap.Emplace(Identifier(identifier), Guid(dataSourceGuid));
				}
			}
			return identifier;
		}
		void Deregister(const Identifier identifier, const Guid dataSourceGuid)
		{
			m_dataSources[identifier] = Invalid;
			{
				Threading::UniqueLock lock(m_guidLookupMapMutex);
				auto it = m_guidLookupMap.Find(dataSourceGuid);
				if (it != m_guidLookupMap.end())
				{
					m_guidLookupMap.Remove(it);
				}
			}
			{
				Threading::UniqueLock lock(m_idLookupMapMutex);
				auto it = m_idLookupMap.Find(identifier);
				if (it != m_idLookupMap.end())
				{
					m_idLookupMap.Remove(it);
				}
			}
			{
				Threading::UniqueLock lock(m_onPropertySourceChangedEventsMapMutex);
				auto it = m_onPropertySourceChangedEventsMap.Find(identifier);
				if (it != m_onPropertySourceChangedEventsMap.end())
				{
					m_onPropertySourceChangedEventsMap.Remove(it);
				}
			}
			m_dataSources[identifier] = {};
			m_identifierStorage.ReturnIdentifier(identifier);
		}

		void OnCreated(const Identifier identifier, Interface& dataSource);

		[[nodiscard]] Optional<Interface*> Get(const Identifier identifier) const
		{
			return m_dataSources[identifier];
		}
		[[nodiscard]] Identifier Find(const Guid dataSourceGuid) const
		{
			Threading::SharedLock lock(m_guidLookupMapMutex);
			auto it = m_guidLookupMap.Find(dataSourceGuid);
			if (it != m_guidLookupMap.end())
			{
				return it->second;
			}
			return {};
		}
		[[nodiscard]] Guid FindGuid(const Identifier identifier) const
		{
			Threading::SharedLock lock(m_idLookupMapMutex);
			auto it = m_idLookupMap.Find(identifier);
			if (it != m_idLookupMap.end())
			{
				return it->second;
			}
			return {};
		}

		using OnChangedEvent = ThreadSafe::Event<void(void*), 24>;
		using OnChangedListenerData = OnChangedEvent::ListenerData;
		void AddOnChangedListener(const Identifier identifier, OnChangedListenerData&& listener)
		{
			{
				Threading::SharedLock lock(m_onPropertySourceChangedEventsMapMutex);
				if (auto it = m_onPropertySourceChangedEventsMap.Find(identifier); it != m_onPropertySourceChangedEventsMap.end())
				{
					it->second->Emplace(Forward<OnChangedListenerData>(listener));
					return;
				}
			}

			Threading::UniqueLock lock(m_onPropertySourceChangedEventsMapMutex);
			auto it = m_onPropertySourceChangedEventsMap.Find(identifier);
			if (it == m_onPropertySourceChangedEventsMap.end())
			{
				it = m_onPropertySourceChangedEventsMap.Emplace(identifier, UniquePtr<OnChangedEvent>::Make());
			}
			it->second->Emplace(Forward<OnChangedListenerData>(listener));
		}
		void RemoveOnChangedListener(const Identifier identifier, const OnChangedEvent::ListenerIdentifier listenerIdentifier)
		{
			Threading::SharedLock lock(m_onPropertySourceChangedEventsMapMutex);
			if (auto it = m_onPropertySourceChangedEventsMap.Find(identifier); it != m_onPropertySourceChangedEventsMap.end())
			{
				it->second->Remove(listenerIdentifier);
			}
		}
		void ReplaceOnChangedListener(
			const Identifier identifier, const OnChangedEvent::ListenerIdentifier previousListenerIdentifier, OnChangedListenerData&& newListener
		)
		{
			{
				Threading::SharedLock lock(m_onPropertySourceChangedEventsMapMutex);
				if (auto it = m_onPropertySourceChangedEventsMap.Find(identifier); it != m_onPropertySourceChangedEventsMap.end())
				{
					it->second->Remove(previousListenerIdentifier);
					it->second->Emplace(Forward<OnChangedListenerData>(newListener));
					return;
				}
			}

			Threading::UniqueLock lock(m_onPropertySourceChangedEventsMapMutex);
			auto it = m_onPropertySourceChangedEventsMap.Find(identifier);
			if (it == m_onPropertySourceChangedEventsMap.end())
			{
				it = m_onPropertySourceChangedEventsMap.Emplace(identifier, UniquePtr<OnChangedEvent>::Make());
			}
			else
			{
				it->second->Remove(previousListenerIdentifier);
			}
			it->second->Emplace(Forward<OnChangedListenerData>(newListener));
		}
		void OnDataChanged(const Identifier identifier) const
		{
			Threading::SharedLock lock(m_onPropertySourceChangedEventsMapMutex);
			if (auto it = m_onPropertySourceChangedEventsMap.Find(identifier); it != m_onPropertySourceChangedEventsMap.end())
			{
				OnChangedEvent& event = *it->second;
				lock.Unlock();
				event();
			}
		}
	protected:
		TSaltedIdentifierStorage<Identifier> m_identifierStorage;
		TIdentifierArray<Optional<Interface*>, Identifier> m_dataSources;
		mutable Threading::SharedMutex m_guidLookupMapMutex;
		UnorderedMap<Guid, Identifier, Guid::Hash> m_guidLookupMap;
		mutable Threading::SharedMutex m_idLookupMapMutex;
		UnorderedMap<Identifier, Guid, Identifier::Hash> m_idLookupMap;

		mutable Threading::SharedMutex m_onPropertySourceChangedEventsMapMutex;
		UnorderedMap<Identifier, UniquePtr<OnChangedEvent>, Identifier::Hash> m_onPropertySourceChangedEventsMap;
	};
}
