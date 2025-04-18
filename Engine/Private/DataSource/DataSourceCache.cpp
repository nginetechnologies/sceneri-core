#include "DataSource/DataSourceCache.h"
#include "DataSource/PropertySourceCache.h"

#include "DataSource/DataSourceInterface.h"
#include "DataSource/PropertySourceInterface.h"
#include "DataSource/DataSourceState.h"

#include <Common/System/Query.h>

namespace ngine::DataSource
{
	Cache::Cache()
	{
		System::Query& systemQuery = System::Query::GetInstance();
		systemQuery.RegisterSystem(*this);
	}

	Cache::~Cache()
	{
		System::Query& systemQuery = System::Query::GetInstance();
		systemQuery.DeregisterSystem<Cache>();
	}

	void Cache::OnCreated(const Identifier identifier, Interface& dataSource)
	{
		Assert(m_dataSources[identifier].IsInvalid());
		m_dataSources[identifier] = dataSource;
		// Trigger a data changed event in case any listeners were added
		OnDataChanged(identifier);
	}

	Identifier Cache::Register(const Guid dataSourceGuid)
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

	Identifier Cache::Find(const Guid dataSourceGuid) const
	{
		Threading::SharedLock lock(m_guidLookupMapMutex);
		auto it = m_guidLookupMap.Find(dataSourceGuid);
		if (it != m_guidLookupMap.end())
		{
			return it->second;
		}
		return {};
	}

	Identifier Cache::FindOrRegister(const Guid dataSourceGuid)
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

	void Cache::Deregister(const Identifier identifier, const Guid dataSourceGuid)
	{
		m_dataSources[identifier] = Invalid;
		{
			Threading::UniqueLock lock(m_guidLookupMapMutex);
			auto it = m_guidLookupMap.Find(dataSourceGuid);
			Assert(it != m_guidLookupMap.end());
			if (LIKELY(it != m_guidLookupMap.end()))
			{
				m_guidLookupMap.Remove(it);
			}
		}
		{
			Threading::UniqueLock lock(m_idLookupMapMutex);
			auto it = m_idLookupMap.Find(identifier);
			Assert(it != m_idLookupMap.end());
			if (LIKELY(it != m_idLookupMap.end()))
			{
				m_idLookupMap.Remove(it);
			}
		}
		{
			Threading::UniqueLock lock(m_onDataSourceChangedEventsMapMutex);
			auto it = m_onDataSourceChangedEventsMap.Find(identifier);
			if (it != m_onDataSourceChangedEventsMap.end())
			{
				m_onDataSourceChangedEventsMap.Remove(it);
			}
		}
		m_identifierStorage.ReturnIdentifier(identifier);
	}

	Guid Cache::FindGuid(const Identifier identifier) const
	{
		Threading::SharedLock lock(m_idLookupMapMutex);
		auto it = m_idLookupMap.Find(identifier);
		if (it != m_idLookupMap.end())
		{
			return it->second;
		}
		return {};
	}

	StateIdentifier Cache::RegisterState(const Guid stateGuid)
	{
		const StateIdentifier identifier = m_stateIdentifierStorage.AcquireIdentifier();
		Assert(identifier.IsValid());
		if (LIKELY(identifier.IsValid()))
		{
			{
				Threading::UniqueLock lock(m_stateGuidLookupMapMutex);
				Assert(!m_stateGuidLookupMap.Contains(stateGuid));
				m_stateGuidLookupMap.Emplace(Guid(stateGuid), StateIdentifier(identifier));
			}
			{
				Threading::UniqueLock lock(m_stateIdLookupMapMutex);
				Assert(!m_stateIdLookupMap.Contains(identifier));
				m_stateIdLookupMap.Emplace(StateIdentifier(identifier), Guid(stateGuid));
			}
		}
		else
		{
			Assert(FindState(stateGuid).IsValid());
		}
		return identifier;
	}

	StateIdentifier Cache::FindState(const Guid dataSourceGuid) const
	{
		Threading::SharedLock lock(m_stateGuidLookupMapMutex);
		auto it = m_stateGuidLookupMap.Find(dataSourceGuid);
		if (it != m_stateGuidLookupMap.end())
		{
			return it->second;
		}
		return {};
	}

	StateIdentifier Cache::FindOrRegisterState(const Guid stateGuid)
	{
		{
			const StateIdentifier identifier = FindState(stateGuid);
			if (identifier.IsValid())
			{
				return identifier;
			}
		}

		Threading::UniqueLock lock(m_stateGuidLookupMapMutex);
		{
			auto it = m_stateGuidLookupMap.Find(stateGuid);
			if (it != m_stateGuidLookupMap.end())
			{
				return it->second;
			}
		}

		const StateIdentifier identifier = m_stateIdentifierStorage.AcquireIdentifier();
		Assert(identifier.IsValid());
		if (LIKELY(identifier.IsValid()))
		{
			Assert(!m_stateGuidLookupMap.Contains(stateGuid));
			m_stateGuidLookupMap.Emplace(Guid(stateGuid), StateIdentifier(identifier));

			{
				Threading::UniqueLock idLock(m_stateIdLookupMapMutex);
				Assert(!m_stateIdLookupMap.Contains(identifier));
				m_stateIdLookupMap.Emplace(StateIdentifier(identifier), Guid(stateGuid));
			}
		}
		return identifier;
	}

	void Cache::DeregisterState(const StateIdentifier identifier, const Guid stateGuid)
	{
		m_states[identifier] = Invalid;
		{
			Threading::UniqueLock lock(m_stateGuidLookupMapMutex);
			auto it = m_stateGuidLookupMap.Find(stateGuid);
			if (LIKELY(it != m_stateGuidLookupMap.end()))
			{
				m_stateGuidLookupMap.Remove(it);
			}
		}
		{
			Threading::UniqueLock lock(m_stateIdLookupMapMutex);
			auto it = m_stateIdLookupMap.Find(identifier);
			if (LIKELY(it != m_stateIdLookupMap.end()))
			{
				m_stateIdLookupMap.Remove(it);
			}
		}
		m_stateIdentifierStorage.ReturnIdentifier(identifier);
	}

	Guid Cache::FindStateGuid(const StateIdentifier identifier) const
	{
		Threading::SharedLock lock(m_stateIdLookupMapMutex);
		auto it = m_stateIdLookupMap.Find(identifier);
		if (it != m_stateIdLookupMap.end())
		{
			return it->second;
		}
		return {};
	}

	PropertyIdentifier Cache::RegisterProperty(String&& propertyName)
	{
		const PropertyIdentifier identifier = m_propertyIdentifierStorage.AcquireIdentifier();
		Assert(identifier.IsValid());
		if (LIKELY(identifier.IsValid()))
		{
			Threading::UniqueLock lock(m_propertyIdentifierLookupMapMutex);
			Assert(!m_propertyIdentifierLookupMap.Contains(propertyName));
			m_propertyIdentifierLookupMap.Emplace(Forward<String>(propertyName), PropertyIdentifier(identifier));
		}
		return identifier;
	}

	void Cache::DeregisterProperty(const PropertyIdentifier identifier, const ConstStringView propertyName)
	{
		Assert(identifier.IsValid());
		Threading::UniqueLock lock(m_propertyIdentifierLookupMapMutex);
		auto it = m_propertyIdentifierLookupMap.Find(propertyName);
		Assert(it != m_propertyIdentifierLookupMap.end());
		if (it != m_propertyIdentifierLookupMap.end())
		{
			m_propertyIdentifierLookupMap.Remove(it);
		}
		m_propertyIdentifierStorage.ReturnIdentifier(identifier);
	}

	PropertyIdentifier Cache::FindPropertyIdentifier(const ConstStringView propertyName) const
	{
		Threading::SharedLock lock(m_propertyIdentifierLookupMapMutex);
		auto it = m_propertyIdentifierLookupMap.Find(propertyName);
		if (it != m_propertyIdentifierLookupMap.end())
		{
			return it->second;
		}
		return {};
	}

	PropertyIdentifier Cache::FindOrRegisterPropertyIdentifier(const ConstStringView propertyName)
	{
		{
			const PropertyIdentifier existingIdentifier = FindPropertyIdentifier(propertyName);
			if (existingIdentifier.IsValid())
			{
				return existingIdentifier;
			}
		}

		Threading::UniqueLock lock(m_propertyIdentifierLookupMapMutex);
		{
			auto it = m_propertyIdentifierLookupMap.Find(propertyName);
			if (it != m_propertyIdentifierLookupMap.end())
			{
				return it->second;
			}
		}

		const PropertyIdentifier identifier = m_propertyIdentifierStorage.AcquireIdentifier();
		Assert(identifier.IsValid());
		if (LIKELY(identifier.IsValid()))
		{
			m_propertyIdentifierLookupMap.Emplace(String(propertyName), PropertyIdentifier(identifier));
		}
		return identifier;
	}

	String Cache::FindPropertyName(const PropertyIdentifier propertyIdentifier) const
	{
		Threading::SharedLock lock(m_propertyIdentifierLookupMapMutex);
		for (auto it = m_propertyIdentifierLookupMap.begin(), endIt = m_propertyIdentifierLookupMap.end(); it != endIt; ++it)
		{
			if (it->second == propertyIdentifier)
			{
				return it->first;
			}
		}
		return {};
	}

	void Cache::AddOnChangedListener(const Identifier identifier, OnChangedListenerData&& listener)
	{
		{
			Threading::SharedLock lock(m_onDataSourceChangedEventsMapMutex);
			if (auto it = m_onDataSourceChangedEventsMap.Find(identifier); it != m_onDataSourceChangedEventsMap.end())
			{
				it->second->Emplace(Forward<OnChangedListenerData>(listener));
				return;
			}
		}

		Threading::UniqueLock lock(m_onDataSourceChangedEventsMapMutex);
		auto it = m_onDataSourceChangedEventsMap.Find(identifier);
		if (it == m_onDataSourceChangedEventsMap.end())
		{
			it = m_onDataSourceChangedEventsMap.Emplace(identifier, UniquePtr<OnChangedEvent>::Make());
		}
		it->second->Emplace(Forward<OnChangedListenerData>(listener));
	}

	void Cache::RemoveOnChangedListener(const Identifier identifier, const OnChangedEvent::ListenerIdentifier listenerIdentifier)
	{
		Threading::SharedLock lock(m_onDataSourceChangedEventsMapMutex);
		if (auto it = m_onDataSourceChangedEventsMap.Find(identifier); it != m_onDataSourceChangedEventsMap.end())
		{
			it->second->Remove(listenerIdentifier);
		}
	}

	void Cache::ReplaceOnChangedListener(
		const Identifier identifier, const OnChangedEvent::ListenerIdentifier previousListenerIdentifier, OnChangedListenerData&& newListener
	)
	{
		{
			Threading::SharedLock lock(m_onDataSourceChangedEventsMapMutex);
			if (auto it = m_onDataSourceChangedEventsMap.Find(identifier); it != m_onDataSourceChangedEventsMap.end())
			{
				it->second->Remove(previousListenerIdentifier);
				it->second->Emplace(Forward<OnChangedListenerData>(newListener));
				return;
			}
		}

		Threading::UniqueLock lock(m_onDataSourceChangedEventsMapMutex);
		auto it = m_onDataSourceChangedEventsMap.Find(identifier);
		if (it == m_onDataSourceChangedEventsMap.end())
		{
			it = m_onDataSourceChangedEventsMap.Emplace(identifier, UniquePtr<OnChangedEvent>::Make());
		}
		else
		{
			it->second->Remove(previousListenerIdentifier);
		}
		it->second->Emplace(Forward<OnChangedListenerData>(newListener));
	}

	void Cache::OnDataChanged(const Identifier identifier) const
	{
		Threading::SharedLock lock(m_onDataSourceChangedEventsMapMutex);
		if (auto it = m_onDataSourceChangedEventsMap.Find(identifier); it != m_onDataSourceChangedEventsMap.end())
		{
			OnChangedEvent& event = *it->second;
			lock.Unlock();
			event();
		}
	}

	void Cache::AddRequestMoreDataListener(const Identifier identifier, OnRequestMoreDataListenerData&& listener)
	{
		{
			Threading::SharedLock lock(m_requestDataSourceMoreDataMapMutex);
			if (auto it = m_requestDataSourceMoreDataMap.Find(identifier); it != m_requestDataSourceMoreDataMap.end())
			{
				it->second->Emplace(Forward<OnRequestMoreDataListenerData>(listener));
				return;
			}
		}

		Threading::UniqueLock lock(m_requestDataSourceMoreDataMapMutex);
		auto it = m_requestDataSourceMoreDataMap.Find(identifier);
		if (it == m_requestDataSourceMoreDataMap.end())
		{
			it = m_requestDataSourceMoreDataMap.Emplace(identifier, UniquePtr<OnRequestMoreDataEvent>::Make());
		}
		it->second->Emplace(Forward<OnRequestMoreDataListenerData>(listener));
	}

	void
	Cache::RemoveRequestMoreDataListener(const Identifier identifier, const OnRequestMoreDataEvent::ListenerIdentifier listenerIdentifier)
	{
		Threading::SharedLock lock(m_requestDataSourceMoreDataMapMutex);
		if (auto it = m_requestDataSourceMoreDataMap.Find(identifier); it != m_requestDataSourceMoreDataMap.end())
		{
			it->second->Remove(listenerIdentifier);
		}
	}

	void Cache::RequestMoreData(
		const Identifier identifier,
		const uint32 maximumCount,
		const PropertyIdentifier sortedPropertyIdentifier,
		const SortingOrder sortingOrder,
		const Optional<const Tag::Query*> pTagQuery,
		const SortedQueryIndices& sortedQuery,
		const GenericDataIndex lastDataIndex
	) const
	{
		Threading::SharedLock lock(m_requestDataSourceMoreDataMapMutex);
		if (auto it = m_requestDataSourceMoreDataMap.Find(identifier); it != m_requestDataSourceMoreDataMap.end())
		{
			const OnRequestMoreDataEvent& event = *it->second;
			lock.Unlock();
			event(maximumCount, sortedPropertyIdentifier, sortingOrder, pTagQuery, sortedQuery, lastDataIndex);
		}
	}

	void Interface::OnDataChanged()
	{
		Cache& cache = System::Get<Cache>();
		cache.OnDataChanged(m_identifier);
	}

	State::State(const Guid stateGuid, const DataSource::Identifier dataSourceIdentifier)
		: m_identifier(System::Get<Cache>().FindOrRegisterState(stateGuid))
		, m_dataSourceIdentifier(dataSourceIdentifier)
	{
		System::Get<Cache>().OnStateCreated(m_identifier, *this);
	}

	State::~State()
	{
		Cache& cache = System::Get<Cache>();
		cache.DeregisterState(m_identifier, cache.FindStateGuid(m_identifier));
	}

	void State::OnDataChanged()
	{
		Cache& cache = System::Get<Cache>();
		cache.OnDataChanged(m_dataSourceIdentifier);
	}

	void Cache::OnStateCreated(const StateIdentifier identifier, State& state)
	{
		m_states[identifier] = state;
	}
}

namespace ngine::PropertySource
{
	void Cache::OnCreated(const Identifier identifier, Interface& dataSource)
	{
		Assert(m_dataSources[identifier].IsInvalid());
		m_dataSources[identifier] = dataSource;
		// Trigger a data changed event in case any listeners were added
		OnDataChanged(identifier);
	}

	void Interface::OnDataChanged()
	{
		DataSource::Cache& cache = System::Get<DataSource::Cache>();
		cache.GetPropertySourceCache().OnDataChanged(m_identifier);
	}
}

namespace ngine
{
	template struct TSaltedIdentifierStorage<DataSource::Identifier>;
	template struct TIdentifierArray<Optional<DataSource::Interface*>, DataSource::Identifier>;
	template struct UnorderedMap<Guid, DataSource::Identifier, Guid::Hash>;
	template struct UnorderedMap<DataSource::Identifier, Guid, DataSource::Identifier::Hash>;

	// template struct TSaltedIdentifierStorage<DataSource::PropertyIdentifier>;
	template struct UnorderedMap<String, DataSource::PropertyIdentifier, String::Hash>;

	template struct TSaltedIdentifierStorage<DataSource::StateIdentifier>;
	template struct TIdentifierArray<Optional<DataSource::State*>, DataSource::StateIdentifier>;
	template struct UnorderedMap<Guid, DataSource::StateIdentifier, Guid::Hash>;
	template struct UnorderedMap<DataSource::StateIdentifier, Guid, DataSource::StateIdentifier::Hash>;

	// template struct TSaltedIdentifierStorage<PropertySource::Identifier>;
	template struct TIdentifierArray<Optional<PropertySource::Interface*>, PropertySource::Identifier>;
	// template struct UnorderedMap<Guid, PropertySource::Identifier, Guid::Hash>;
	// template struct UnorderedMap<PropertySource::Identifier, Guid, PropertySource::Identifier::Hash>;
}
