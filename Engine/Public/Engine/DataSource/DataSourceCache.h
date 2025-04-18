#pragma once

#include "DataSourceIdentifier.h"
#include "DataSourcePropertyIdentifier.h"
#include "DataSourceStateIdentifier.h"
#include "PropertySourceCache.h"
#include "SortingOrder.h"
#include "CachedQuery.h"

#include <Common/Storage/Identifier.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/System/SystemType.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Threading/Mutexes/UniqueLock.h>
#include <Common/Threading/Mutexes/SharedLock.h>
#include <Common/Function/ThreadSafeEvent.h>

namespace ngine::DataSource
{
	struct Interface;
	struct PropertyInterface;
	struct State;
}

namespace ngine::Tag
{
	struct Query;
}

namespace ngine
{
	extern template struct TSaltedIdentifierStorage<DataSource::Identifier>;
	extern template struct TIdentifierArray<Optional<DataSource::Interface*>, DataSource::Identifier>;
	extern template struct UnorderedMap<Guid, DataSource::Identifier, Guid::Hash>;
	extern template struct UnorderedMap<DataSource::Identifier, Guid, DataSource::Identifier::Hash>;

	extern template struct TSaltedIdentifierStorage<DataSource::PropertyIdentifier>;
	extern template struct UnorderedMap<String, DataSource::PropertyIdentifier, String::Hash>;

	extern template struct TSaltedIdentifierStorage<DataSource::StateIdentifier>;
	extern template struct TIdentifierArray<Optional<DataSource::State*>, DataSource::StateIdentifier>;
	extern template struct UnorderedMap<Guid, DataSource::StateIdentifier, Guid::Hash>;
	extern template struct UnorderedMap<DataSource::StateIdentifier, Guid, DataSource::StateIdentifier::Hash>;
}

namespace ngine::DataSource
{
	//! Cache responsible for keeping track of available data sources and their identifiers
	struct Cache
	{
		inline static constexpr System::Type SystemType = System::Type::DataSourceCache;

		Cache();
		~Cache();

		Identifier Register(const Guid dataSourceGuid);
		[[nodiscard]] Identifier Find(const Guid dataSourceGuid) const;
		[[nodiscard]] Identifier FindOrRegister(const Guid dataSourceGuid);
		void Deregister(const Identifier identifier, const Guid dataSourceGuid);
		[[nodiscard]] Optional<Interface*> Get(const Identifier identifier) const
		{
			return m_dataSources[identifier];
		}
		[[nodiscard]] Guid FindGuid(const Identifier identifier) const;

		void OnCreated(const Identifier identifier, Interface& dataSource);

		StateIdentifier RegisterState(const Guid stateGuid);
		[[nodiscard]] StateIdentifier FindState(const Guid dataSourceGuid) const;
		[[nodiscard]] StateIdentifier FindOrRegisterState(const Guid stateGuid);
		void DeregisterState(const StateIdentifier identifier, const Guid stateGuid);
		[[nodiscard]] Optional<State*> GetState(const StateIdentifier identifier) const
		{
			return m_states[identifier];
		}
		[[nodiscard]] Guid FindStateGuid(const StateIdentifier identifier) const;

		void OnStateCreated(const StateIdentifier identifier, State& state);

		[[nodiscard]] PropertyIdentifier RegisterProperty(String&& propertyName);
		void DeregisterProperty(const PropertyIdentifier identifier, const ConstStringView propertyName);

		[[nodiscard]] PropertyIdentifier FindPropertyIdentifier(const ConstStringView propertyName) const;

		[[nodiscard]] PropertyIdentifier FindOrRegisterPropertyIdentifier(const ConstStringView propertyName);
		[[nodiscard]] String FindPropertyName(const PropertyIdentifier propertyIdentifier) const;

		using OnChangedEvent = ThreadSafe::Event<void(void*), 24>;
		using OnChangedListenerData = OnChangedEvent::ListenerData;
		void AddOnChangedListener(const Identifier identifier, OnChangedListenerData&& listener);
		void RemoveOnChangedListener(const Identifier identifier, const OnChangedEvent::ListenerIdentifier listenerIdentifier);
		void ReplaceOnChangedListener(
			const Identifier identifier, const OnChangedEvent::ListenerIdentifier previousListenerIdentifier, OnChangedListenerData&& newListener
		);
		void OnDataChanged(const Identifier identifier) const;

		using OnRequestMoreDataEvent = ThreadSafe::Event<
			void(
				void*,
				const uint32 maximumCount,
				const PropertyIdentifier sortedPropertyIdentifier,
				const SortingOrder sortingOrder,
				const Optional<const Tag::Query*> pTagQuery,
				const SortedQueryIndices& sortedQuery,
				const GenericDataIndex lastDataIndex
			),
			24>;
		using OnRequestMoreDataListenerData = OnRequestMoreDataEvent::ListenerData;
		void AddRequestMoreDataListener(const Identifier identifier, OnRequestMoreDataListenerData&& liAddRequestMoreDataListenerstener);
		void RemoveRequestMoreDataListener(const Identifier identifier, const OnRequestMoreDataEvent::ListenerIdentifier listenerIdentifier);
		void RequestMoreData(
			const Identifier identifier,
			const uint32 maximumCount,
			const PropertyIdentifier sortedPropertyIdentifier,
			const SortingOrder sortingOrder,
			const Optional<const Tag::Query*> pTagQuery,
			const SortedQueryIndices& sortedQuery,
			const GenericDataIndex lastDataIndex
		) const;

		[[nodiscard]] const PropertySource::Cache& GetPropertySourceCache() const
		{
			return m_propertySourceCache;
		}
		[[nodiscard]] PropertySource::Cache& GetPropertySourceCache()
		{
			return m_propertySourceCache;
		}
	protected:
		TSaltedIdentifierStorage<Identifier> m_identifierStorage;
		TIdentifierArray<Optional<Interface*>, Identifier> m_dataSources;
		mutable Threading::SharedMutex m_guidLookupMapMutex;
		UnorderedMap<Guid, Identifier, Guid::Hash> m_guidLookupMap;
		mutable Threading::SharedMutex m_idLookupMapMutex;
		UnorderedMap<Identifier, Guid, Identifier::Hash> m_idLookupMap;

		mutable Threading::SharedMutex m_onDataSourceChangedEventsMapMutex;
		UnorderedMap<Identifier, UniquePtr<OnChangedEvent>, Identifier::Hash> m_onDataSourceChangedEventsMap;

		mutable Threading::SharedMutex m_requestDataSourceMoreDataMapMutex;
		UnorderedMap<Identifier, UniquePtr<OnRequestMoreDataEvent>, Identifier::Hash> m_requestDataSourceMoreDataMap;

		TSaltedIdentifierStorage<PropertyIdentifier> m_propertyIdentifierStorage;
		mutable Threading::SharedMutex m_propertyIdentifierLookupMapMutex;
		UnorderedMap<String, PropertyIdentifier, String::Hash> m_propertyIdentifierLookupMap;

		PropertySource::Cache m_propertySourceCache;

		TSaltedIdentifierStorage<StateIdentifier> m_stateIdentifierStorage;
		TIdentifierArray<Optional<State*>, StateIdentifier> m_states;
		mutable Threading::SharedMutex m_stateGuidLookupMapMutex;
		UnorderedMap<Guid, StateIdentifier, Guid::Hash> m_stateGuidLookupMap;
		mutable Threading::SharedMutex m_stateIdLookupMapMutex;
		UnorderedMap<StateIdentifier, Guid, StateIdentifier::Hash> m_stateIdLookupMap;
	};
}
