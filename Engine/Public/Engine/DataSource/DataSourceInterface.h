#pragma once

#include "DataSourceIdentifier.h"
#include "DataSourcePropertyIdentifier.h"
#include "Data.h"
#include "ForwardDeclarations/PropertyValue.h"
#include "SortingOrder.h"
#include "CachedQuery.h"

#include <Common/Function/ForwardDeclarations/Function.h>
#include <Common/Math/ForwardDeclarations/Range.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierMask.h>

#include <Engine/Tag/TagMask.h>

namespace ngine::DataSource
{
	//! Interface to a data source implementation
	//! A data source contains any number of data that can be queried and filtered dynamically
	struct Interface
	{
		using PropertyValue = DataSource::PropertyValue;

		Interface(const Identifier identifier)
			: m_identifier(identifier)
		{
		}
		virtual ~Interface() = default;
		using Data = DataSource::Data;

		using Query = Tag::Query;
		using CachedQuery = DataSource::CachedQuery;
		using SortedQueryIndices = DataSource::SortedQueryIndices;
		using SortingOrder = DataSource::SortingOrder;
		using PropertyIdentifier = DataSource::PropertyIdentifier;
		using GenericDataIdentifier = DataSource::GenericDataIdentifier;
		using GenericDataIndex = DataSource::GenericDataIndex;

		[[nodiscard]] Identifier GetIdentifier() const
		{
			return m_identifier;
		}

		//! Called before starting access to lock changing of data
		virtual void LockRead()
		{
		}
		//! Called after finishing access to unlock changing of data
		virtual void UnlockRead()
		{
		}

		//! Finds all matches from the tag or item query, and applies them to a cached query
		//! Note that the cached query is only valid during a LockRead / UnlockRead context, or until OnDataChanged is called
		virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const = 0;
		//! Sorts the cached query according to the specificd property in the requested order, and stores in cachedSortedQueryOut
		//! Note that the cached query is only valid during a LockRead / UnlockRead context, or until OnDataChanged is called
		[[nodiscard]] virtual bool
		SortQuery(const CachedQuery&, [[maybe_unused]] const PropertyIdentifier filteredPropertyIdentifier, const SortingOrder, SortedQueryIndices&)
		{
			return false;
		}

		//! Gets the number of data items currently present in this data source
		[[nodiscard]] virtual GenericDataIndex GetDataCount() const = 0;

		using IterationCallback = Function<void(const Data), 24>;
		//! Calls the provided callback once for each data item in the cached query currently present in this data source
		virtual void IterateData(
			const CachedQuery& query,
			IterationCallback&& callback,
			const Math::Range<GenericDataIndex> offset =
				Math::Range<GenericDataIndex>::MakeStartToEnd(0u, Math::NumericLimits<GenericDataIndex>::Max - 1u)
		) const = 0;
		//! Calls the provided callback once for each data item in the sorted cached query currently present in this data source
		virtual void IterateData(
			const SortedQueryIndices& query,
			IterationCallback&& callback,
			const Math::Range<GenericDataIndex> offset =
				Math::Range<GenericDataIndex>::MakeStartToEnd(0u, Math::NumericLimits<GenericDataIndex>::Max - 1u)
		) const = 0;

		[[nodiscard]] virtual PropertyValue GetDataProperty(const Data data, const PropertyIdentifier identifier) const = 0;
	protected:
		void OnDataChanged();
	protected:
		Identifier m_identifier;
	};
}
