#pragma once

#include <Widgets/Data/Component.h>

#include <Engine/DataSource/DataSourceIdentifier.h>
#include <Engine/DataSource/CachedQuery.h>
#include <Engine/DataSource/DataSourcePropertyIdentifier.h>
#include <Engine/DataSource/SortingOrder.h>
#include <Engine/Tag/TagMask.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Storage/Identifier.h>
#include <Common/Guid.h>

namespace ngine::Widgets::Data
{
	struct DataSourceEntry : public Widgets::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 14>;

		using BaseType = Widgets::Data::Component;

		struct Initializer : public Widgets::Data::Component::Initializer
		{
			using BaseType = Widgets::Data::Component::Initializer;
			Initializer(BaseType&& baseInitializer, const Optional<ngine::DataSource::GenericDataIndex> dataIndex = Invalid)
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_index(dataIndex)
			{
			}
			Optional<ngine::DataSource::GenericDataIndex> m_index;
		};
		DataSourceEntry(Initializer&& initializer);
		DataSourceEntry(const Deserializer& deserializer);
		DataSourceEntry(const DataSourceEntry& templateComponent, const Cloner& cloner);

		DataSourceEntry(const DataSourceEntry&) = delete;
		DataSourceEntry& operator=(const DataSourceEntry&) = delete;
		DataSourceEntry(DataSourceEntry&&) = delete;
		DataSourceEntry& operator=(DataSourceEntry&&) = delete;
		~DataSourceEntry();

		void UpdateDataIndex(const ngine::DataSource::GenericDataIndex);
		[[nodiscard]] Optional<ngine::DataSource::GenericDataIndex> GetDataIndex() const
		{
			return {m_index, m_index != Math::NumericLimits<ngine::DataSource::GenericDataIndex>::Max};
		}
	protected:
		ngine::DataSource::GenericDataIndex m_index = Math::NumericLimits<ngine::DataSource::GenericDataIndex>::Max;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::DataSourceEntry>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::DataSourceEntry>(
			"{f1ce81ac-b973-4e6f-8c3e-0cf73c2284f4}"_guid,
			MAKE_UNICODE_LITERAL("Widgets Data Source Entry"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicDeserialization | TypeFlags::DisableWriteToDisk
		);
	};
}
