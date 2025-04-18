#pragma once

#include "Engine/DataSource/PropertySourceInterface.h"
#include "Engine/DataSource/DataSourcePropertyIdentifier.h"
#include "Engine/DataSource/PropertySourceIdentifier.h"

#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Optional.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

namespace ngine::DataSource
{
	struct Cache;
}

namespace ngine::PropertySource
{
	struct Dynamic final : public Interface
	{
		Dynamic(const Identifier identifier);
		Dynamic(const Identifier identifier, const Dynamic& other);
		virtual ~Dynamic();

		void AddDataProperty(PropertyIdentifier propertyIdentifier, PropertyValue&& value);
		void ClearData();

		virtual void LockRead() override;
		virtual void UnlockRead() override;
		void LockWrite();
		void UnlockWrite();

		[[nodiscard]] virtual PropertyValue GetDataProperty(const PropertyIdentifier identifier) const override;

		bool Serialize(const Serialization::Reader serializer, DataSource::Cache& dataSourceCache);
		bool Serialize(Serialization::Writer writer, DataSource::Cache& dataSourceCache) const;
	protected:
		UnorderedMap<PropertyIdentifier, PropertyValue, PropertyIdentifier::Hash> m_propertyMap;
		mutable Threading::SharedMutex m_dataMutex;
	};
}
