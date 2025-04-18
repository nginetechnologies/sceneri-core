#pragma once

#include "Engine/DataSource/DataSourceInterface.h"
#include "Engine/DataSource/SortingOrder.h"
#include "Engine/DataSource/DataSourcePropertyIdentifier.h"
#include "Engine/DataSource/GenericIdentifier.h"
#include "Engine/DataSource/DataSourceIdentifier.h"

#include <Engine/Tag/TagMask.h>

#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Asset/Guid.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Optional.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

namespace ngine::DataSource
{
	struct Cache;

	struct Dynamic final : public Interface
	{
		Dynamic(const Identifier identifier);
		Dynamic(const Identifier identifier, const Dynamic& other);
		virtual ~Dynamic();

		[[nodiscard]] GenericDataIdentifier CreateDataIdentifier();
		void AddDataProperty(GenericDataIdentifier dataIdentifier, PropertyIdentifier propertyIdentifier, PropertyValue&& value);
		[[nodiscard]] Optional<const PropertyValue*>
		GetDataProperty(GenericDataIdentifier dataIdentifier, PropertyIdentifier propertyIdentifier) const;
		[[nodiscard]] Optional<const PropertyValue*> GetDataProperty(GenericDataIndex dataIndex, PropertyIdentifier propertyIdentifier) const;
		[[nodiscard]] GenericDataIdentifier FindDataIdentifier(PropertyIdentifier PropertyIdentifier, const PropertyValue& value) const;
		void ClearData();

		virtual void LockRead() override;
		virtual void UnlockRead() override;
		void LockWrite();
		void UnlockWrite();

		virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const override;

		[[nodiscard]] virtual bool
		SortQuery(const CachedQuery&, [[maybe_unused]] const PropertyIdentifier filteredPropertyIdentifier, const SortingOrder, SortedQueryIndices&)
			override;

		[[nodiscard]] virtual GenericDataIndex GetDataCount() const override;

		virtual void IterateData(
			const CachedQuery& query,
			IterationCallback&& callback,
			const Math::Range<GenericDataIndex> offset =
				Math::Range<GenericDataIndex>::MakeStartToEnd(0u, Math::NumericLimits<GenericDataIndex>::Max - 1u)
		) const override;

		virtual void IterateData(
			const SortedQueryIndices& query,
			IterationCallback&& callback,
			const Math::Range<GenericDataIndex> offset =
				Math::Range<GenericDataIndex>::MakeStartToEnd(0u, Math::NumericLimits<GenericDataIndex>::Max - 1u)
		) const override;

		[[nodiscard]] virtual PropertyValue GetDataProperty(const Data data, const PropertyIdentifier identifier) const override;

		bool Serialize(const Serialization::Reader serializer, Cache& cache);
		bool Serialize(Serialization::Writer serializer, Cache& cache) const;
	protected:
		using PropertyMap = UnorderedMap<PropertyIdentifier, PropertyValue, PropertyIdentifier::Hash>;
		Vector<PropertyMap> m_data;
		TSaltedIdentifierStorage<GenericDataIdentifier> m_identifiers;
		Threading::AtomicIdentifierMask<GenericDataIdentifier> m_dataMask;
		mutable Threading::SharedMutex m_dataMutex;
	};
}
