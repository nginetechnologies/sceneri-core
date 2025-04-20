#include <Widgets/Data/DataSourceEntry.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Data/Component.inl>
#include <Engine/DataSource/DynamicDataSource.h>
#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/Serialization/DataSourcePropertyIdentifier.h>
#include <Engine/DataSource/Serialization/DataSourcePropertyMask.h>
#include <Engine/DataSource/DataSourceCache.h>

#include <Widgets/Widget.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Widgets::Data
{
	DataSourceEntry::DataSourceEntry(Initializer&& initializer)
		: BaseType(Forward<Widgets::Data::Component::Initializer>(initializer))
		, m_index(initializer.m_index.IsValid() ? *initializer.m_index : Math::NumericLimits<ngine::DataSource::GenericDataIndex>::Max)
	{
	}

	DataSourceEntry::DataSourceEntry(const Deserializer& deserializer)
		: BaseType(deserializer)
	{
	}

	DataSourceEntry::DataSourceEntry(const DataSourceEntry& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
	{
	}

	DataSourceEntry::~DataSourceEntry()
	{
	}

	void DataSourceEntry::UpdateDataIndex(const ngine::DataSource::GenericDataIndex dataIndex)
	{
		m_index = dataIndex;
	}

	[[maybe_unused]] const bool wasDataConsumerTypeRegistered = Reflection::Registry::RegisterType<Data::DataSourceEntry>();
	[[maybe_unused]] const bool wasDataConsumerComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::DataSourceEntry>>::Make());
}
