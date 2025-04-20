#pragma once

#include <Widgets/Data/Component.h>

#include <Engine/DataSource/DataSourceIdentifier.h>
#include <Engine/DataSource/DataSourcePropertyIdentifier.h>
#include <Engine/DataSource/DataSourceStateIdentifier.h>
#include <Engine/DataSource/SortingOrder.h>
#include <Engine/Tag/TagMask.h>

#include <Common/Memory/Variant.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Storage/Identifier.h>
#include <Common/Guid.h>
#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::DataSource
{
	struct Dynamic;
}

namespace ngine::Widgets::Data
{
	struct DataSource : public Widgets::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 11>;

		enum class Flags : uint8
		{
			IsDynamic = 1 << 0,
			SortDescending = 1 << 1,
			//! Whether to apply the first data inside the data source to our owning widget
			//! Only relevant when restricting a data source to one limit, becoming a pseudo PropertySource
			Inline = 1 << 2,
			Global = 1 << 3
		};

		struct DynamicTagQuery
		{
			DynamicTagQuery() = default;
			DynamicTagQuery(const DynamicTagQuery& other)
				: m_allowedFilterMaskPropertyIdentifier(other.m_allowedFilterMaskPropertyIdentifier)
				, m_requiredFilterMaskPropertyIdentifier(other.m_requiredFilterMaskPropertyIdentifier)
				, m_disallowedFilterMaskPropertyIdentifier(other.m_disallowedFilterMaskPropertyIdentifier)
				, m_pTagQuery(other.m_pTagQuery.IsValid() ? UniquePtr<Tag::Query>::Make(*other.m_pTagQuery) : UniquePtr<Tag::Query>{})
			{
			}

			[[nodiscard]] inline bool IsValid() const
			{
				return m_allowedFilterMaskPropertyIdentifier.IsValid() | m_requiredFilterMaskPropertyIdentifier.IsValid() |
				       m_disallowedFilterMaskPropertyIdentifier.IsValid();
			}

			ngine::DataSource::PropertyIdentifier m_allowedFilterMaskPropertyIdentifier;
			ngine::DataSource::PropertyIdentifier m_requiredFilterMaskPropertyIdentifier;
			ngine::DataSource::PropertyIdentifier m_disallowedFilterMaskPropertyIdentifier;

			UniquePtr<Tag::Query> m_pTagQuery;
		};

		using BaseType = Widgets::Data::Component;

		struct Initializer : public Widgets::Data::Component::Initializer
		{
			using BaseType = Widgets::Data::Component::Initializer;
			Initializer(BaseType&& baseInitializer, const ngine::DataSource::Identifier dataSourceIdentifier, const Guid dataSourceAssetGuid = {})
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_dataSource(dataSourceIdentifier)
				, m_dataSourceAssetGuid(dataSourceAssetGuid)
			{
			}
			Initializer(BaseType&& baseInitializer, ngine::DataSource::Dynamic& dataSource, const Guid dataSourceAssetGuid = {})
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_dataSource(ReferenceWrapper<ngine::DataSource::Dynamic>(dataSource))
				, m_dataSourceAssetGuid(dataSourceAssetGuid)
			{
			}

			Variant<ngine::DataSource::Identifier, ReferenceWrapper<ngine::DataSource::Dynamic>> m_dataSource;
			Guid m_dataSourceAssetGuid;
		};
		DataSource(Initializer&& initializer);
		DataSource(const Deserializer& deserializer);
		DataSource(Widget& owner, const Serialization::Reader dataSourceReader, const Serialization::Reader widgetReader);
		DataSource(const DataSource& templateComponent, const Cloner& cloner);

		DataSource(const DataSource&) = delete;
		DataSource& operator=(const DataSource&) = delete;
		DataSource(DataSource&&) = delete;
		DataSource& operator=(DataSource&&) = delete;
		~DataSource();

		void OnCreated(Widget& owner);
		void OnParentCreated(Widget& owner);
		void OnDestroying(Widget& owner);

		void SetDataSourceAssetGuid(Widget& owner, Asset::Guid dataSourceAssetGuid);
		void SetDataSource(Widget& owner, ngine::DataSource::Identifier dataSourceIdentifier);
		bool Serialize(const Serialization::Reader reader, const Serialization::Reader widgetReader, Widget& owner);

		void SetTagQuery(Widget& owner, UniquePtr<Tag::Query>&& query);
		void SetRequiredTags(Widget& owner, const Tag::RequiredMask& mask);
		void SetAllowedTags(Widget& owner, const Tag::AllowedMask& mask);
		void SetDisallowedTags(Widget& owner, const Tag::DisallowedMask& mask);
		void SetAllowedItems(Widget& owner, const ngine::DataSource::GenericDataMask& mask);
		void ClearAllowedItems(Widget& owner);
		void SetDisallowedItems(Widget& owner, const ngine::DataSource::GenericDataMask& mask);
		void ClearDisallowedItems(Widget& owner);

		[[nodiscard]] ngine::DataSource::Identifier GetDataSourceIdentifier() const
		{
			return m_dataSourceIdentifier;
		}
		[[nodiscard]] Optional<ngine::DataSource::Dynamic*> GetDynamicDataSource() const;
		[[nodiscard]] ngine::DataSource::StateIdentifier GetDataSourceStateIdentifier() const
		{
			return m_dataSourceStateIdentifier;
		}
		[[nodiscard]] Guid GetDataSourceAssetGuid() const
		{
			return m_dataSourceAssetGuid;
		}

		[[nodiscard]] DynamicTagQuery& GetDynamicTagQuery()
		{
			return m_dynamicTagQuery;
		}
		[[nodiscard]] Optional<const Tag::Query*> GetConstTagQuery()
		{
			return m_pTagQuery.Get();
		}
		[[nodiscard]] Optional<const Tag::Query*> GetTagQuery() const
		{
			if (m_dynamicTagQuery.m_pTagQuery.IsValid())
			{
				return m_dynamicTagQuery.m_pTagQuery.Get();
			}
			else
			{
				return m_pTagQuery.Get();
			}
		}

		[[nodiscard]] uint32 GetMaximumDataCount() const
		{
			return m_maximumDataCount;
		}
		[[nodiscard]] ngine::DataSource::PropertyIdentifier GetSortedPropertyIdentifier() const
		{
			return m_sortedPropertyIdentifier;
		}
		[[nodiscard]] ngine::DataSource::SortingOrder GetSortingOrder() const
		{
			return m_flags.IsSet(Flags::SortDescending) ? ngine::DataSource::SortingOrder::Descending
			                                            : ngine::DataSource::SortingOrder::Ascending;
		}
		[[nodiscard]] bool IsInline() const
		{
			return m_flags.IsSet(Flags::Inline);
		}

		bool SerializeCustomData(Serialization::Writer, const Widget& parent) const;
		void DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& parent);
	protected:
		[[nodiscard]] ngine::DataSource::Identifier
		SerializeInternal(Widget& owner, const Serialization::Reader reader, const Serialization::Reader widgetReader);

		void UpdateInlineElement(Widget& owner);
		void OnDataChanged(Widget& owner);
	protected:
		EnumFlags<Flags> m_flags;
		ngine::DataSource::Identifier m_dataSourceIdentifier;
		Guid m_dataSourceGuid;
		Guid m_dataSourceStateGuid;
		ngine::DataSource::StateIdentifier m_dataSourceStateIdentifier;
		Guid m_dataSourceAssetGuid = {};
		UniquePtr<Tag::Query> m_pTagQuery;
		DynamicTagQuery m_dynamicTagQuery;

		ngine::DataSource::PropertyIdentifier m_sortedPropertyIdentifier;
		uint32 m_maximumDataCount{Math::NumericLimits<uint32>::Max};
	};
	ENUM_FLAG_OPERATORS(DataSource::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::DataSource>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::DataSource>(
			"{4145C2D9-3B92-4E89-847B-3A5E9D0BF62C}"_guid, MAKE_UNICODE_LITERAL("Widget Data Source"), TypeFlags::DisableDynamicInstantiation
		);
	};
}
