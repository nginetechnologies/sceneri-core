#include <Widgets/Data/DataSource.h>
#include <Widgets/Data/Layout.h>
#include <Widgets/Data/GridLayout.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Data/Component.inl>
#include <Engine/DataSource/DynamicDataSource.h>
#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/Serialization/DataSourcePropertyIdentifier.h>
#include <Engine/DataSource/Serialization/DataSourcePropertyMask.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Context/Utils.h>

#include <Widgets/Widget.h>
#include <Widgets/WidgetScene.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Containers/Format/String.h>

namespace ngine::Widgets::Data
{
	DataSource::DataSource(Initializer&& initializer)
		: BaseType(Forward<Widgets::Data::Component::Initializer>(initializer))
		, m_dataSourceAssetGuid(initializer.m_dataSourceAssetGuid)
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		if (const Optional<ngine::DataSource::Identifier*> dataSourceIdentifier = initializer.m_dataSource.Get<ngine::DataSource::Identifier>())
		{
			m_dataSourceIdentifier = *dataSourceIdentifier;
			m_dataSourceGuid = dataSourceCache.FindGuid(*dataSourceIdentifier);
		}
		else if (const Optional<ReferenceWrapper<ngine::DataSource::Dynamic>*> pDataSource = initializer.m_dataSource.Get<ReferenceWrapper<ngine::DataSource::Dynamic>>())
		{
			ngine::DataSource::Dynamic& dataSource = *pDataSource;
			m_dataSourceIdentifier = dataSource.GetIdentifier();
			m_dataSourceGuid = dataSourceCache.FindGuid(m_dataSourceIdentifier);
			m_flags |= Flags::IsDynamic;
		}

		dataSourceCache.AddOnChangedListener(
			m_dataSourceIdentifier,
			ngine::DataSource::Cache::OnChangedListenerData{
				*this,
				[&owner = initializer.GetParent()](DataSource& dataSource)
				{
					dataSource.OnDataChanged(owner);
				}
			}
		);
	}

	DataSource::DataSource(const Deserializer& deserializer)
		: BaseType(deserializer)
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		m_dataSourceIdentifier = SerializeInternal(deserializer.GetParent(), deserializer.m_reader, deserializer.m_reader);

		if (deserializer.m_reader.IsObject() && deserializer.m_reader.HasSerializer("entries"))
		{
			ngine::DataSource::Dynamic* pDynamicDataSource = new ngine::DataSource::Dynamic(m_dataSourceIdentifier);
			[[maybe_unused]] const bool wasRead = deserializer.m_reader.SerializeInPlace(*pDynamicDataSource, dataSourceCache);
			Assert(wasRead);
			m_flags |= Flags::IsDynamic;
		}

		dataSourceCache.AddOnChangedListener(
			m_dataSourceIdentifier,
			ngine::DataSource::Cache::OnChangedListenerData{
				*this,
				[&owner = deserializer.GetParent()](DataSource& dataSource)
				{
					dataSource.OnDataChanged(owner);
				}
			}
		);
	}

	DataSource::DataSource(Widget& owner, const Serialization::Reader dataSourceReader, const Serialization::Reader widgetReader)
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		m_dataSourceIdentifier = SerializeInternal(owner, dataSourceReader, widgetReader);

		if (dataSourceReader.IsObject() && dataSourceReader.HasSerializer("entries"))
		{
			ngine::DataSource::Dynamic* pDynamicDataSource = new ngine::DataSource::Dynamic(m_dataSourceIdentifier);
			[[maybe_unused]] const bool wasRead = dataSourceReader.SerializeInPlace(*pDynamicDataSource, dataSourceCache);
			Assert(wasRead);
			m_flags |= Flags::IsDynamic;
		}

		dataSourceCache.AddOnChangedListener(
			m_dataSourceIdentifier,
			ngine::DataSource::Cache::OnChangedListenerData{
				*this,
				[&owner](DataSource& dataSource)
				{
					dataSource.OnDataChanged(owner);
				}
			}
		);
	}

	DataSource::DataSource(const DataSource& templateComponent, const Cloner& cloner)
		: BaseType(templateComponent, cloner)
		, m_flags(templateComponent.m_flags)
		, m_dataSourceGuid(templateComponent.m_dataSourceGuid)
		, m_dataSourceStateGuid(templateComponent.m_dataSourceStateGuid)
		, m_dataSourceStateIdentifier(System::Get<ngine::DataSource::Cache>().FindOrRegisterState(
				Context::Utils::GetGuid(templateComponent.m_dataSourceStateGuid, cloner.GetParent(), cloner.GetSceneRegistry())
			))
		, m_dataSourceAssetGuid(templateComponent.m_dataSourceAssetGuid)
		, m_pTagQuery(
				templateComponent.m_pTagQuery.IsValid() ? UniquePtr<Tag::Query>::Make(*templateComponent.m_pTagQuery) : UniquePtr<Tag::Query>{}
			)
		, m_dynamicTagQuery(templateComponent.m_dynamicTagQuery)
		, m_sortedPropertyIdentifier(templateComponent.m_sortedPropertyIdentifier)
		, m_maximumDataCount(templateComponent.m_maximumDataCount)
	{
		Guid dataSourceGuid = m_dataSourceGuid;
		if (dataSourceGuid.IsInvalid())
		{
			dataSourceGuid = Guid::Generate();
		}

		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		ngine::DataSource::Identifier dataSourceIdentifier;
		if (m_flags.IsSet(Flags::Global))
		{
			dataSourceIdentifier = dataSourceCache.FindOrRegister(dataSourceGuid);
		}
		else if (dataSourceIdentifier = dataSourceCache.Find(dataSourceGuid); !dataSourceIdentifier.IsValid())
		{
			dataSourceIdentifier =
				dataSourceCache.FindOrRegister(Context::Utils::GetGuid(dataSourceGuid, cloner.GetParent(), cloner.GetSceneRegistry()));
		}
		m_dataSourceIdentifier = dataSourceIdentifier;
		Assert(m_dataSourceIdentifier.IsValid());

		Assert(
			m_flags.IsNotSet(Flags::IsDynamic) || dataSourceCache.FindGuid(dataSourceIdentifier) != m_dataSourceGuid,
			"Dynamic data source must use unique identifiers"
		);

		if (m_flags.IsSet(Flags::IsDynamic))
		{
			const Optional<ngine::DataSource::Interface*> pTemplateDataSource = dataSourceCache.Get(templateComponent.m_dataSourceIdentifier);
			Assert(pTemplateDataSource.IsValid());
			if (LIKELY(pTemplateDataSource.IsValid()))
			{
				ngine::DataSource::Dynamic& dynamicTemplate = static_cast<ngine::DataSource::Dynamic&>(*pTemplateDataSource);
				[[maybe_unused]] ngine::DataSource::Dynamic* pDynamicDataSource =
					new ngine::DataSource::Dynamic(dataSourceIdentifier, dynamicTemplate);
				Assert(dataSourceCache.Get(m_dataSourceIdentifier).IsValid());
			}
		}

		dataSourceCache.AddOnChangedListener(
			m_dataSourceIdentifier,
			ngine::DataSource::Cache::OnChangedListenerData{
				*this,
				[&owner = cloner.GetParent()](DataSource& dataSource)
				{
					dataSource.OnDataChanged(owner);
				}
			}
		);
	}

	DataSource::~DataSource() = default;

	ngine::DataSource::Identifier
	DataSource::SerializeInternal(Widget& owner, const Serialization::Reader reader, const Serialization::Reader widgetReader)
	{
		m_dataSourceAssetGuid = widgetReader.ReadWithDefaultValue<Guid>("data_source_asset", Guid());

		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		Guid dataSourceGuid = reader.ReadInPlaceWithDefaultValue<Guid>(Guid{});
		if (dataSourceGuid.IsValid())
		{
			m_dataSourceGuid = dataSourceGuid;
		}
		else if (const Optional<Guid> guid = reader.Read<Guid>("guid"))
		{
			dataSourceGuid = *guid;
			m_dataSourceGuid = dataSourceGuid;
		}
		else
		{
			dataSourceGuid = Guid::Generate();
		}

		ngine::DataSource::Identifier dataSourceIdentifier;
		m_flags.Clear(Flags::Global);
		if (reader.IsObject() && reader.ReadWithDefaultValue<bool>("global", false))
		{
			dataSourceIdentifier = dataSourceCache.FindOrRegister(dataSourceGuid);
			m_flags |= Flags::Global;
		}
		else if (dataSourceIdentifier = dataSourceCache.Find(dataSourceGuid); !dataSourceIdentifier.IsValid())
		{
			dataSourceGuid = Context::Utils::GetGuid(dataSourceGuid, owner, owner.GetSceneRegistry());
			dataSourceIdentifier = dataSourceCache.FindOrRegister(dataSourceGuid);
		}

		if (reader.IsObject() && reader.ReadWithDefaultValue<bool>("inline", false))
		{
			if (m_flags.IsNotSet(Flags::Inline))
			{
				m_flags |= Flags::Inline;
			}
		}
		else if (m_flags.IsSet(Flags::Inline))
		{
			m_flags.Clear(Flags::Inline);
			dataSourceCache.RemoveOnChangedListener(dataSourceIdentifier, this);
		}

		if (const Optional<Guid> stateGuid = widgetReader.Read<Guid>("data_source_state"))
		{
			m_dataSourceStateGuid = *stateGuid;
			m_dataSourceStateIdentifier =
				dataSourceCache.FindOrRegisterState(Context::Utils::GetGuid(m_dataSourceStateGuid, owner, owner.GetSceneRegistry()));
		}

		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
		Tag::Query tagQuery;

		bool hasDynamicTags{false};

		if (const Optional<Serialization::Reader> allowedTagsReader = widgetReader.FindSerializer("data_allowed_tags"))
		{
			if (allowedTagsReader->IsString())
			{
				hasDynamicTags |= allowedTagsReader->SerializeInPlace(m_dynamicTagQuery.m_allowedFilterMaskPropertyIdentifier, dataSourceCache);
			}
			else
			{
				allowedTagsReader->SerializeInPlace(tagQuery.m_allowedFilterMask, tagRegistry);
			}
		}

		if (const Optional<Serialization::Reader> disallowedTagsReader = widgetReader.FindSerializer("data_disallowed_tags"))
		{
			if (disallowedTagsReader->IsString())
			{
				hasDynamicTags |=
					disallowedTagsReader->SerializeInPlace(m_dynamicTagQuery.m_disallowedFilterMaskPropertyIdentifier, dataSourceCache);
			}
			else
			{
				disallowedTagsReader->SerializeInPlace(tagQuery.m_disallowedFilterMask, tagRegistry);
			}
		}

		if (const Optional<Serialization::Reader> requiredTagsReader = widgetReader.FindSerializer("data_required_tags"))
		{
			if (requiredTagsReader->IsString())
			{
				hasDynamicTags |= requiredTagsReader->SerializeInPlace(m_dynamicTagQuery.m_requiredFilterMaskPropertyIdentifier, dataSourceCache);
			}
			else
			{
				requiredTagsReader->SerializeInPlace(tagQuery.m_requiredFilterMask, tagRegistry);
			}
		}

		if (hasDynamicTags)
		{
			m_dynamicTagQuery.m_pTagQuery.CreateInPlace(Move(tagQuery));
		}

		if (tagQuery.IsActive())
		{
			m_pTagQuery.CreateInPlace(Move(tagQuery));
		}

		widgetReader.Serialize("data_max_count", m_maximumDataCount);

		if (const Optional<ConstStringView> sortedPropertyName = widgetReader.Read<ConstStringView>("data_sort_property"))
		{
			m_sortedPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier(*sortedPropertyName);

			switch (widgetReader.ReadWithDefaultValue("data_sort_order", ngine::DataSource::SortingOrder::Ascending))
			{
				case ngine::DataSource::SortingOrder::Ascending:
					m_flags.Clear(Flags::SortDescending);
					break;
				case ngine::DataSource::SortingOrder::Descending:
					m_flags.Set(Flags::SortDescending);
					break;
			}
		}

		return dataSourceIdentifier;
	}

	bool DataSource::Serialize(const Serialization::Reader reader, const Serialization::Reader widgetReader, Widget& owner)
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

		if (m_dataSourceIdentifier.IsValid())
		{
			if (m_flags.IsSet(Flags::IsDynamic))
			{
				const Optional<ngine::DataSource::Interface*> pDataSource = dataSourceCache.Get(m_dataSourceIdentifier);
				Assert(pDataSource.IsValid());
				if (LIKELY(pDataSource.IsValid()))
				{
					ngine::DataSource::Dynamic* pDynamic = &static_cast<ngine::DataSource::Dynamic&>(*pDataSource);
					delete pDynamic;
				}
			}
		}

		const ngine::DataSource::Identifier newDataSourceIdentifier = SerializeInternal(owner, reader, widgetReader);

		m_dataSourceIdentifier = newDataSourceIdentifier;
		m_flags.Set(Flags::IsDynamic);

		ngine::DataSource::Dynamic* pDynamicDataSource = new ngine::DataSource::Dynamic(newDataSourceIdentifier);
		reader.SerializeInPlace(*pDynamicDataSource, dataSourceCache);
		return true;
	}

	bool DataSource::SerializeCustomData(Serialization::Writer writer, const Widget&) const
	{
		writer.Serialize("guid", m_dataSourceGuid);
		writer.Serialize("data_source_asset", m_dataSourceAssetGuid);
		writer.SerializeWithDefaultValue("global", m_flags.IsSet(Flags::Global), false);
		writer.SerializeWithDefaultValue("inline", m_flags.IsSet(Flags::Inline), false);

		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		if (m_dataSourceStateIdentifier.IsValid())
		{
			writer.Serialize("data_source_state", m_dataSourceStateGuid);
		}

		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
		if (m_dynamicTagQuery.m_allowedFilterMaskPropertyIdentifier.IsValid())
		{
			String format;
			format.Format("{{{}}}", dataSourceCache.FindPropertyName(m_dynamicTagQuery.m_allowedFilterMaskPropertyIdentifier));
			writer.Serialize("data_allowed_tags", format);
		}
		else if (m_pTagQuery.IsValid() && m_pTagQuery->m_allowedFilterMask.AreAnySet())
		{
			Tag::Mask mask = m_pTagQuery->m_allowedFilterMask;
			writer.SerializeArrayWithCallback(
				"data_allowed_tags",
				[&mask, &tagRegistry](Serialization::Writer writer, [[maybe_unused]] const uint32 index)
				{
					const Tag::Identifier tagIdentifier = Tag::Identifier::MakeFromValidIndex(mask.GetFirstSetIndex());
					mask.Clear(tagIdentifier);
					writer.SerializeInPlace<Guid>(tagRegistry.GetAssetGuid(tagIdentifier));
					return true;
				},
				mask.GetNumberOfSetBits()
			);
		}

		if (m_dynamicTagQuery.m_disallowedFilterMaskPropertyIdentifier.IsValid())
		{
			String format;
			format.Format("{{{}}}", dataSourceCache.FindPropertyName(m_dynamicTagQuery.m_disallowedFilterMaskPropertyIdentifier));
			writer.Serialize("data_disallowed_tags", format);
		}
		else if (m_pTagQuery.IsValid() && m_pTagQuery->m_disallowedFilterMask.AreAnySet())
		{
			Tag::Mask mask = m_pTagQuery->m_disallowedFilterMask;
			writer.SerializeArrayWithCallback(
				"data_disallowed_tags",
				[&mask, &tagRegistry](Serialization::Writer writer, [[maybe_unused]] const uint32 index)
				{
					const Tag::Identifier tagIdentifier = Tag::Identifier::MakeFromValidIndex(mask.GetFirstSetIndex());
					mask.Clear(tagIdentifier);
					writer.SerializeInPlace<Guid>(tagRegistry.GetAssetGuid(tagIdentifier));
					return true;
				},
				mask.GetNumberOfSetBits()
			);
		}

		if (m_dynamicTagQuery.m_requiredFilterMaskPropertyIdentifier.IsValid())
		{
			String format;
			format.Format("{{{}}}", dataSourceCache.FindPropertyName(m_dynamicTagQuery.m_requiredFilterMaskPropertyIdentifier));
			writer.Serialize("data_required_tags", format);
		}
		else if (m_pTagQuery.IsValid() && m_pTagQuery->m_requiredFilterMask.AreAnySet())
		{
			Tag::Mask mask = m_pTagQuery->m_requiredFilterMask;
			writer.SerializeArrayWithCallback(
				"data_required_tags",
				[&mask, &tagRegistry](Serialization::Writer writer, [[maybe_unused]] const uint32 index)
				{
					const Tag::Identifier tagIdentifier = Tag::Identifier::MakeFromValidIndex(mask.GetFirstSetIndex());
					mask.Clear(tagIdentifier);
					writer.SerializeInPlace<Guid>(tagRegistry.GetAssetGuid(tagIdentifier));
					return true;
				},
				mask.GetNumberOfSetBits()
			);
		}

		if (m_maximumDataCount != Math::NumericLimits<uint32>::Max)
		{
			writer.Serialize("data_max_count", m_maximumDataCount);
		}

		if (m_sortedPropertyIdentifier.IsValid())
		{
			writer.Serialize("data_sort_property", dataSourceCache.FindPropertyName(m_sortedPropertyIdentifier));
		}

		if (m_flags.IsSet(Flags::SortDescending))
		{
			writer.Serialize("data_sort_order", (uint8)ngine::DataSource::SortingOrder::Descending);
		}

		if (m_flags.IsSet(Flags::IsDynamic))
		{
			const Optional<ngine::DataSource::Interface*> pDataSource = dataSourceCache.Get(m_dataSourceIdentifier);
			Assert(pDataSource.IsValid());
			if (LIKELY(pDataSource.IsValid()))
			{
				ngine::DataSource::Dynamic& dynamic = static_cast<ngine::DataSource::Dynamic&>(*pDataSource);
				dynamic.Serialize(writer, dataSourceCache);
			}
		}

		return true;
	}

	void DataSource::DeserializeCustomData(const Optional<Serialization::Reader> pReader, Widget& owner)
	{
		if (pReader.IsValid())
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

			if (m_dataSourceIdentifier.IsValid())
			{
				if (m_flags.IsSet(Flags::IsDynamic))
				{
					const Optional<ngine::DataSource::Interface*> pDataSource = dataSourceCache.Get(m_dataSourceIdentifier);
					Assert(pDataSource.IsValid());
					if (LIKELY(pDataSource.IsValid()))
					{
						ngine::DataSource::Dynamic* pDynamic = &static_cast<ngine::DataSource::Dynamic&>(*pDataSource);
						delete pDynamic;
					}
				}
			}

			m_dataSourceIdentifier = SerializeInternal(owner, *pReader, *pReader);

			if (pReader->IsObject() && pReader->HasSerializer("entries"))
			{
				ngine::DataSource::Dynamic* pDynamicDataSource = new ngine::DataSource::Dynamic(m_dataSourceIdentifier);
				[[maybe_unused]] const bool wasRead = pReader->SerializeInPlace(*pDynamicDataSource, dataSourceCache);
				Assert(wasRead);
				m_flags |= Flags::IsDynamic;
				Assert(dataSourceCache.Get(m_dataSourceIdentifier).IsValid());
			}
		}
	}

	void DataSource::UpdateInlineElement(Widget& owner)
	{
		Assert(m_flags.IsSet(Flags::Inline));
		Assert(m_maximumDataCount == 1, "Inline data sources are only relevant with one possible element");
		// Trigger update from this data source, owner will find our data
		owner.UpdateFromDataSource(owner.GetSceneRegistry());
	}

	void DataSource::OnCreated(Widget& owner)
	{
		if (m_flags.IsSet(Flags::Inline) && !owner.GetRootScene().IsTemplate())
		{
			UpdateInlineElement(owner);
		}
	}

	void DataSource::OnParentCreated(Widget& owner)
	{
		if (m_flags.IsSet(Flags::Inline) && !owner.GetRootScene().IsTemplate())
		{
			UpdateInlineElement(owner);
		}
	}

	void DataSource::OnDestroying(Widget&)
	{
		if (m_dataSourceIdentifier.IsValid())
		{
			ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
			if (m_flags.IsSet(Flags::IsDynamic))
			{
				const Optional<ngine::DataSource::Interface*> pDataSource = dataSourceCache.Get(m_dataSourceIdentifier);
				Assert(pDataSource.IsValid());
				if (LIKELY(pDataSource.IsValid()))
				{
					ngine::DataSource::Dynamic* pDynamic = &static_cast<ngine::DataSource::Dynamic&>(*pDataSource);
					delete pDynamic;
				}
			}

			dataSourceCache.RemoveOnChangedListener(m_dataSourceIdentifier, this);
		}
	}

	void DataSource::OnDataChanged(Widget& owner)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (m_flags.IsSet(Flags::Inline) && !owner.GetRootScene().IsTemplate())
		{
			UpdateInlineElement(owner);
		}
		else if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = owner.FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
		{
			pFlexLayoutComponent->OnDataSourceChanged(owner);
		}
		else if (const Optional<Data::GridLayout*> pGridLayoutComponent = owner.FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
		{
			pGridLayoutComponent->OnDataSourceChanged(owner);
		}
	}

	void DataSource::SetDataSourceAssetGuid(Widget& owner, Asset::Guid dataSourceAssetGuid)
	{
		m_dataSourceAssetGuid = dataSourceAssetGuid;

		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = owner.FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
		{
			pFlexLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}
		else if (const Optional<Data::GridLayout*> pGridLayoutComponent = owner.FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
		{
			pGridLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}
	}

	void DataSource::SetDataSource(Widget& owner, ngine::DataSource::Identifier newDataSourceIdentifier)
	{
		if (m_dataSourceIdentifier == newDataSourceIdentifier)
		{
			return;
		}

		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();

		if (m_dataSourceIdentifier.IsValid())
		{
			if (m_flags.IsSet(Flags::IsDynamic))
			{
				const Optional<ngine::DataSource::Interface*> pDataSource = dataSourceCache.Get(m_dataSourceIdentifier);
				Assert(pDataSource.IsValid());
				if (LIKELY(pDataSource.IsValid()))
				{
					ngine::DataSource::Dynamic* pDynamic = &static_cast<ngine::DataSource::Dynamic&>(*pDataSource);
					delete pDynamic;
				}
			}

			dataSourceCache.RemoveOnChangedListener(m_dataSourceIdentifier, this);
		}

		m_dataSourceIdentifier = newDataSourceIdentifier;
		m_flags.Clear(Flags::IsDynamic);

		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = owner.FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
		{
			pFlexLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}
		else if (const Optional<Data::GridLayout*> pGridLayoutComponent = owner.FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
		{
			pGridLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}

		if (m_flags.IsSet(Flags::Inline) && !owner.GetRootScene().IsTemplate())
		{
			UpdateInlineElement(owner);
		}

		dataSourceCache.AddOnChangedListener(
			m_dataSourceIdentifier,
			ngine::DataSource::Cache::OnChangedListenerData{
				*this,
				[&owner](DataSource& dataSource)
				{
					dataSource.OnDataChanged(owner);
				}
			}
		);
	}

	void DataSource::SetTagQuery(Widget& owner, UniquePtr<Tag::Query>&& query)
	{
		m_pTagQuery = Forward<UniquePtr<Tag::Query>>(query);

		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = owner.FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
		{
			pFlexLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}
		else if (const Optional<Data::GridLayout*> pGridLayoutComponent = owner.FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
		{
			pGridLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}

		if (m_flags.IsSet(Flags::Inline) && !owner.GetRootScene().IsTemplate())
		{
			UpdateInlineElement(owner);
		}
	}

	void DataSource::SetRequiredTags(Widget& owner, const Tag::RequiredMask& mask)
	{
		if (m_pTagQuery.IsInvalid())
		{
			m_pTagQuery = UniquePtr<Tag::Query>::Make();
		}
		m_pTagQuery->m_requiredFilterMask = mask;

		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = owner.FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
		{
			pFlexLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}
		else if (const Optional<Data::GridLayout*> pGridLayoutComponent = owner.FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
		{
			pGridLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}

		if (m_flags.IsSet(Flags::Inline) && !owner.GetRootScene().IsTemplate())
		{
			UpdateInlineElement(owner);
		}
	}

	void DataSource::SetAllowedTags(Widget& owner, const Tag::AllowedMask& mask)
	{
		if (m_pTagQuery.IsInvalid())
		{
			m_pTagQuery = UniquePtr<Tag::Query>::Make();
		}
		m_pTagQuery->m_allowedFilterMask = mask;

		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = owner.FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
		{
			pFlexLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}
		else if (const Optional<Data::GridLayout*> pGridLayoutComponent = owner.FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
		{
			pGridLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}

		if (m_flags.IsSet(Flags::Inline) && !owner.GetRootScene().IsTemplate())
		{
			UpdateInlineElement(owner);
		}
	}

	void DataSource::SetDisallowedTags(Widget& owner, const Tag::DisallowedMask& mask)
	{
		if (m_pTagQuery.IsInvalid())
		{
			m_pTagQuery = UniquePtr<Tag::Query>::Make();
		}
		m_pTagQuery->m_disallowedFilterMask = mask;

		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = owner.FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
		{
			pFlexLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}
		else if (const Optional<Data::GridLayout*> pGridLayoutComponent = owner.FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
		{
			pGridLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}

		if (m_flags.IsSet(Flags::Inline) && !owner.GetRootScene().IsTemplate())
		{
			UpdateInlineElement(owner);
		}
	}

	void DataSource::SetAllowedItems(Widget& owner, const ngine::DataSource::GenericDataMask& __restrict mask)
	{
		if (m_pTagQuery.IsInvalid())
		{
			m_pTagQuery = UniquePtr<Tag::Query>::Make();
		}
		m_pTagQuery->m_allowedItems = mask;

		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = owner.FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
		{
			pFlexLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}
		else if (const Optional<Data::GridLayout*> pGridLayoutComponent = owner.FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
		{
			pGridLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}

		if (m_flags.IsSet(Flags::Inline) && !owner.GetRootScene().IsTemplate())
		{
			UpdateInlineElement(owner);
		}
	}

	void DataSource::ClearAllowedItems(Widget& owner)
	{
		if (m_pTagQuery.IsValid())
		{
			m_pTagQuery->m_allowedItems = Invalid;

			Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
			if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = owner.FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
			{
				pFlexLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
			}
			else if (const Optional<Data::GridLayout*> pGridLayoutComponent = owner.FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
			{
				pGridLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
			}
		}

		if (m_flags.IsSet(Flags::Inline) && !owner.GetRootScene().IsTemplate())
		{
			UpdateInlineElement(owner);
		}
	}

	void DataSource::SetDisallowedItems(Widget& owner, const ngine::DataSource::GenericDataMask& __restrict mask)
	{
		if (m_pTagQuery.IsInvalid())
		{
			m_pTagQuery = UniquePtr<Tag::Query>::Make();
		}
		m_pTagQuery->m_disallowedItems = mask;

		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = owner.FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
		{
			pFlexLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}
		else if (const Optional<Data::GridLayout*> pGridLayoutComponent = owner.FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
		{
			pGridLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
		}

		if (m_flags.IsSet(Flags::Inline) && !owner.GetRootScene().IsTemplate())
		{
			UpdateInlineElement(owner);
		}
	}

	void DataSource::ClearDisallowedItems(Widget& owner)
	{
		if (m_pTagQuery.IsValid())
		{
			m_pTagQuery->m_disallowedItems = Invalid;

			Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
			if (const Optional<Data::FlexLayout*> pFlexLayoutComponent = owner.FindDataComponentOfType<Data::FlexLayout>(sceneRegistry))
			{
				pFlexLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
			}
			else if (const Optional<Data::GridLayout*> pGridLayoutComponent = owner.FindDataComponentOfType<Data::GridLayout>(sceneRegistry))
			{
				pGridLayoutComponent->OnDataSourcePropertiesChanged(owner, sceneRegistry);
			}
		}

		if (m_flags.IsSet(Flags::Inline) && !owner.GetRootScene().IsTemplate())
		{
			UpdateInlineElement(owner);
		}
	}

	Optional<ngine::DataSource::Dynamic*> DataSource::GetDynamicDataSource() const
	{
		ngine::DataSource::Cache& dataSourceCache = System::Get<ngine::DataSource::Cache>();
		const Optional<ngine::DataSource::Interface*> pDataSource = dataSourceCache.Get(m_dataSourceIdentifier);
		return Optional<ngine::DataSource::Dynamic*>{
			static_cast<ngine::DataSource::Dynamic*>(pDataSource.Get()),
			m_flags.IsSet(Flags::IsDynamic)
		};
	}

	[[maybe_unused]] const bool wasDataSourceTypeRegistered = Reflection::Registry::RegisterType<Data::DataSource>();
	[[maybe_unused]] const bool wasDataSourceComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::DataSource>>::Make());
}
