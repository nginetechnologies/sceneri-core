#include "Reflection/Registry.h"

#include <Common/Memory/Any.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Reflection/GenericType.h>
#include <Common/Reflection/TypeInterface.h>

#include <Engine/Asset/AssetManager.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Tag/TagContainer.inl>
#include <Engine/DataSource/PropertyValue.h>
#include <Engine/DataSource/DataSourceCache.h>

#include <Common/Asset/VirtualAsset.h>
#include <Common/System/Query.h>
#include <Common/Memory/OffsetOf.h>

namespace ngine::Reflection
{
	EngineRegistry::EngineRegistry()
		: Registry(Registry::Initializer::Initialize)
		, Interface(System::Get<DataSource::Cache>().FindOrRegister(DataSourceGuid))
		, m_typeCount(Registry::GetTypeCount())
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.OnCreated(m_identifier, *this);

		m_typeNamePropertyIdentifier = dataSourceCache.RegisterProperty("type_name");
		m_typeGuidPropertyIdentifier = dataSourceCache.RegisterProperty("type_guid");
		m_typeTagPropertyIdentifier = dataSourceCache.RegisterProperty("type_tag");
		m_typeThumbnailPropertyIdentifier = dataSourceCache.RegisterProperty("type_thumbnail_guid");

		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

		// Register types
		for (const Registry::TypeContainer::PairType& typePair : m_types)
		{
			const TypeIdentifier identifier = m_typeIdentifiers.AcquireIdentifier();
			m_typeGuids[identifier] = typePair.first;

			// Register a tag for the type itself
			{
				const Tag::Identifier tagIdentifier = tagRegistry.FindOrRegister(
					typePair.first,
					typePair.second->GetName(),
					"#D946EF66"_colorf,
					Tag::Flags::ReflectedType | Tag::Flags::VisibleToUI | Tag::Flags::Transient
				);
				Reflection::TypeMask typeMask;
				typeMask.Set(identifier);
				SetTagTypes(tagIdentifier, typeMask);
			}

			for (const Tag::Guid tagGuid : typePair.second->GetTags())
			{
				const Tag::Identifier tagIdentifier = tagRegistry.FindOrRegister(tagGuid);
				Reflection::TypeMask typeMask;
				typeMask.Set(identifier);
				SetTagTypes(tagIdentifier, typeMask);
			}
		}
	}

	EngineRegistry::~EngineRegistry()
	{
		m_tags.Destroy(System::Get<Tag::Registry>());

		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.Deregister(dataSourceCache.Find(DataSourceGuid), DataSourceGuid);
		dataSourceCache.DeregisterProperty(m_typeNamePropertyIdentifier, "type_name");
		dataSourceCache.DeregisterProperty(m_typeGuidPropertyIdentifier, "type_guid");
		dataSourceCache.DeregisterProperty(m_typeTagPropertyIdentifier, "type_tag");
		dataSourceCache.DeregisterProperty(m_typeThumbnailPropertyIdentifier, "type_thumbnail_guid");
	}

	void EngineRegistry::RegisterDynamicType(const Guid guid, const TypeInterface& typeInterface)
	{
		Registry::RegisterDynamicType(guid, typeInterface);

		const TypeIdentifier identifier = m_typeIdentifiers.AcquireIdentifier();
		m_typeGuids[identifier] = guid;
		m_typeCount++;

		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
		for (const Tag::Guid tagGuid : typeInterface.GetTags())
		{
			const Tag::Identifier tagIdentifier = tagRegistry.FindOrRegister(tagGuid);
			Reflection::TypeMask typeMask;
			typeMask.Set(identifier);
			SetTagTypes(tagIdentifier, typeMask);
		}
	}

	void EngineRegistry::CacheQuery(const Tag::Query& query, CachedQuery& cachedQueryOut) const
	{
		TypeMask& selectedTypes = reinterpret_cast<TypeMask&>(cachedQueryOut);
		selectedTypes.ClearAll();

		if (query.m_allowedItems.IsValid())
		{
			selectedTypes = TypeMask(*query.m_allowedItems);
		}

		const TagContainer::ConstRestrictedView tags = m_tags.GetView();

		if (query.m_allowedFilterMask.AreAnySet())
		{
			TypeMask allowedFilterMask;
			for (const Tag::Identifier::IndexType tagIndex : query.m_allowedFilterMask.GetSetBitsIterator())
			{
				const AtomicTypeMask* pTagAssets = tags[Tag::Identifier::MakeFromValidIndex(tagIndex)];
				if (pTagAssets != nullptr)
				{
					allowedFilterMask |= *pTagAssets;
				}
			}

			if (!query.m_allowedItems.IsValid())
			{
				selectedTypes |= allowedFilterMask;
			}
			else
			{
				selectedTypes &= allowedFilterMask;
			}
		}

		// Default to showing all entries if no allowed masks were set
		if (!query.m_allowedItems.IsValid() && query.m_allowedFilterMask.AreNoneSet())
		{
			for (const TypeIdentifier typeIdentifier : m_typeIdentifiers.GetView())
			{
				if (m_typeGuids[typeIdentifier].IsValid())
				{
					selectedTypes.Set(typeIdentifier);
				}
			}
		}

		for (const Tag::Identifier::IndexType tagIndex : query.m_disallowedFilterMask.GetSetBitsIterator())
		{
			const Threading::Atomic<AtomicTypeMask*> pTagAssets = tags[Tag::Identifier::MakeFromValidIndex(tagIndex)];
			if (pTagAssets == nullptr)
			{
				continue;
			}

			selectedTypes.Clear(*pTagAssets);
		}

		if (query.m_requiredFilterMask.AreAnySet())
		{
			for (const Tag::Identifier::IndexType tagIndex : query.m_requiredFilterMask.GetSetBitsIterator())
			{
				const Threading::Atomic<AtomicTypeMask*> pTagAssets = tags[Tag::Identifier::MakeFromValidIndex(tagIndex)];
				if (pTagAssets != nullptr)
				{
					selectedTypes &= *pTagAssets;
				}
				else
				{
					selectedTypes.ClearAll();
				}
			}
		}
	}

	void EngineRegistry::IterateData(const CachedQuery& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset) const
	{
		const TypeMask& selectedElements = reinterpret_cast<const TypeMask&>(query);

		const TypeMask::SetBitsIterator iterator = selectedElements.GetSetBitsIterator();
		const TypeMask::SetBitsIterator::Iterator begin = iterator.begin() + (uint16)offset.GetMinimum();
		const TypeMask::SetBitsIterator::Iterator end =
			Math::Min(iterator.end(), iterator.begin() + (uint16)offset.GetMinimum() + (uint16)offset.GetSize());

		for (TypeMask::SetBitsIterator::Iterator it = begin; it != end; ++it)
		{
			const TypeIdentifier::IndexType assetIndex = *it;
			TypeIdentifier typeIdentifier = m_typeIdentifiers.GetActiveIdentifier(TypeIdentifier::MakeFromValidIndex(assetIndex));
			callback(typeIdentifier);

			--offset;
			if (offset.GetSize() == 0)
			{
				break;
			}
		}
	}

	void
	EngineRegistry::IterateData(const SortedQueryIndices& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset) const
	{
		for (const GenericDataIndex identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			TypeIdentifier typeIdentifier = m_typeIdentifiers.GetActiveIdentifier(TypeIdentifier::MakeFromValidIndex(identifierIndex));
			callback(typeIdentifier);
		}
	}

	DataSource::PropertyValue EngineRegistry::GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const
	{
		const TypeIdentifier typeIdentifier = data.GetExpected<TypeIdentifier>();
		const Guid typeGuid = m_typeGuids[typeIdentifier];

		if (const Optional<const TypeInterface*> pTypeInterface = FindTypeInterface(typeGuid))
		{
			if (identifier == m_typeNamePropertyIdentifier)
			{
				return pTypeInterface->GetName();
			}
			else if (identifier == m_typeGuidPropertyIdentifier)
			{
				return pTypeInterface->GetGuid();
			}
			else if (identifier == m_typeTagPropertyIdentifier)
			{
				const Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
				return tagRegistry.FindIdentifier(pTypeInterface->GetGuid());
			}
			/*else if (identifier == m_typeThumbnailPropertyIdentifier)
			{
			  if (pTypeInterface->m_assetTypeGuid.IsValid())
			  {
			    return pTypeInterface->m_thumbnailGuid;
			  }
			}*/
		}

		return {};
	}

	// Functions data source
	EngineRegistry::FunctionsDataSource::FunctionsDataSource()
		: Interface(System::Get<DataSource::Cache>().FindOrRegister(DataSourceGuid))
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.OnCreated(m_identifier, *this);

		m_functionIdentifierPropertyIdentifier = dataSourceCache.RegisterProperty("function_identifier");
		m_functionNamePropertyIdentifier = dataSourceCache.RegisterProperty("function_name");
		m_functionAssetGuidPropertyIdentifier = dataSourceCache.RegisterProperty("function_asset_guid");

		const EngineRegistry& registry = Memory::GetConstOwnerFromMember(*this, &EngineRegistry::m_functionsDataSource);
		for (const FunctionIdentifier functionIdentifier : registry.m_functionIdentifiers.GetView())
		{
			// if (typeInterface.GetFlags().IsNotSet(Reflection::TypeFlags::DisableUserInterfaceInstantiation))
			{
				const Guid functionGuid = registry.m_functionGuids[functionIdentifier];
				if (const Optional<const FunctionInfo*> pFunctionInfo = registry.FindFunctionDefinition(functionGuid))
				{
					System::Get<Asset::Manager>().RegisterAsset(
						functionGuid,
						Asset::DatabaseEntry{
							"26fc2d42-e856-4a02-a454-b81c06434672"_guid,
							functionGuid,
							IO::Path::Merge(functionGuid.ToString().GetView(), MAKE_PATH(".function"), Asset::VirtualAsset::FileExtension),
							UnicodeString(pFunctionInfo->m_displayName),
							UnicodeString{}
						},
						Asset::Identifier{}
					);
				}
			}
		}
	}

	EngineRegistry::FunctionsDataSource::~FunctionsDataSource()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.Deregister(dataSourceCache.Find(DataSourceGuid), DataSourceGuid);

		dataSourceCache.DeregisterProperty(m_functionIdentifierPropertyIdentifier, "function_identifier");
		dataSourceCache.DeregisterProperty(m_functionNamePropertyIdentifier, "function_name");
		dataSourceCache.DeregisterProperty(m_functionAssetGuidPropertyIdentifier, "function_asset_guid");
	}

	DataSource::GenericDataIndex EngineRegistry::FunctionsDataSource::GetDataCount() const
	{
		const EngineRegistry& registry = Memory::GetConstOwnerFromMember(*this, &EngineRegistry::m_functionsDataSource);
		return (GenericDataIndex)registry.m_functions.GetSize();
	}

	void EngineRegistry::FunctionsDataSource::CacheQuery(const Tag::Query& query, CachedQuery& cachedQueryOut) const
	{
		const EngineRegistry& registry = Memory::GetConstOwnerFromMember(*this, &EngineRegistry::m_functionsDataSource);

		FunctionMask& selectedFunctions = reinterpret_cast<FunctionMask&>(cachedQueryOut);
		selectedFunctions.ClearAll();

		if (query.m_allowedItems.IsValid())
		{
			selectedFunctions = FunctionMask(*query.m_allowedItems);
		}

		if (query.m_allowedFilterMask.AreAnySet())
		{
			FunctionMask allowedFilterMask;

			// Workaround while we don't have function tags
			// TODO: Move to Function definition
			const Guid uiVisibleTagGuid = "31a21582-9784-495b-bf34-e018a4bdac1b"_guid;
			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
			if (query.m_allowedFilterMask.IsSet(tagRegistry.FindOrRegister(uiVisibleTagGuid)))
			{
				for (const FunctionIdentifier functionIdentifier : registry.m_functionIdentifiers.GetView())
				{
					const Guid functionGuid = registry.m_functionGuids[functionIdentifier];
					if (functionGuid.IsValid())
					{
						if (const Optional<const FunctionInfo*> pFunctionInfo = registry.FindFunctionDefinition(functionGuid))
						{
							if (pFunctionInfo->m_flags.IsSet(Reflection::FunctionFlags::VisibleToUI))
							{
								allowedFilterMask.Set(functionIdentifier);
							}
						}
					}
				}
			}

			/*for (const Tag::Identifier::IndexType tagIndex : query.m_allowedFilterMask.GetSetBitsIterator())
			{
			  const AtomicTypeMask* pTagAssets = tags[Tag::Identifier::MakeFromValidIndex(tagIndex)];
			  if (pTagAssets != nullptr)
			  {
			    allowedFilterMask |= *pTagAssets;
			  }
			}*/

			if (!query.m_allowedItems.IsValid())
			{
				selectedFunctions |= allowedFilterMask;
			}
			else
			{
				selectedFunctions &= allowedFilterMask;
			}
		}

		// Default to showing all entries if no allowed masks were set
		if (!query.m_allowedItems.IsValid() && query.m_allowedFilterMask.AreNoneSet())
		{
			for (const FunctionIdentifier functionIdentifier : registry.m_functionIdentifiers.GetView())
			{
				const Guid functionGuid = registry.m_functionGuids[functionIdentifier];
				if (functionGuid.IsValid())
				{
					if (const Optional<const FunctionInfo*> pFunctionInfo = registry.FindFunctionDefinition(functionGuid))
					{
						selectedFunctions.Set(functionIdentifier);
					}
				}
			}
		}

		/*for (const Tag::Identifier::IndexType tagIndex : query.m_disallowedFilterMask.GetSetBitsIterator())
		{
		  const Threading::Atomic<AtomicTypeMask*> pTagAssets = tags[Tag::Identifier::MakeFromValidIndex(tagIndex)];
		  if (pTagAssets == nullptr)
		  {
		    continue;
		  }

		  selectedFunctions.Clear(*pTagAssets);
		}*/

		/*if (query.m_requiredFilterMask.AreAnySet())
		{
		  for (const FunctionIdentifier::IndexType assetIndex : cachedQueryOut.GetSetBitsIterator())
		  {
		    const FunctionIdentifier functionIdentifier =
		registry.m_functionIdentifiers.GetActiveIdentifier(FunctionIdentifier::MakeFromValidIndex(assetIndex));

		    for (const Tag::Identifier::IndexType tagIndex : query.m_requiredFilterMask.GetSetBitsIterator())
		    {
		      const Tag::Identifier tagIdentifier = Tag::Identifier::MakeFromValidIndex(tagIndex);
		      if (!m_tags.IsSet(tagIdentifier, functionIdentifier))
		      {
		        selectedFunctions.Clear(functionIdentifier);
		        break;
		      }
		    }
		  }
		}*/
	}

	void EngineRegistry::FunctionsDataSource::IterateData(
		const CachedQuery& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset
	) const
	{
		const EngineRegistry& registry = Memory::GetConstOwnerFromMember(*this, &EngineRegistry::m_functionsDataSource);
		const FunctionMask& selectedElements = reinterpret_cast<const FunctionMask&>(query);

		const FunctionMask::SetBitsIterator iterator = selectedElements.GetSetBitsIterator();
		const FunctionMask::SetBitsIterator::Iterator begin = iterator.begin() + (uint16)offset.GetMinimum();
		const FunctionMask::SetBitsIterator::Iterator end =
			Math::Min(iterator.end(), iterator.begin() + (uint16)offset.GetMinimum() + (uint16)offset.GetSize());

		for (FunctionMask::SetBitsIterator::Iterator it = begin; it != end; ++it)
		{
			const FunctionIdentifier::IndexType assetIndex = *it;
			FunctionIdentifier functionIdentifier =
				registry.m_functionIdentifiers.GetActiveIdentifier(FunctionIdentifier::MakeFromValidIndex(assetIndex));
			callback(functionIdentifier);

			--offset;
			if (offset.GetSize() == 0)
			{
				break;
			}
		}
	}

	void EngineRegistry::FunctionsDataSource::IterateData(
		const SortedQueryIndices& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset
	) const
	{
		const EngineRegistry& registry = Memory::GetConstOwnerFromMember(*this, &EngineRegistry::m_functionsDataSource);
		for (const GenericDataIndex identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			FunctionIdentifier functionIdentifier =
				registry.m_functionIdentifiers.GetActiveIdentifier(FunctionIdentifier::MakeFromValidIndex(identifierIndex));
			callback(functionIdentifier);
		}
	}

	DataSource::PropertyValue
	EngineRegistry::FunctionsDataSource::GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const
	{
		const EngineRegistry& registry = Memory::GetConstOwnerFromMember(*this, &EngineRegistry::m_functionsDataSource);
		FunctionIdentifier functionIdentifier = data.GetExpected<FunctionIdentifier>();
		const Guid functionGuid = registry.m_functionGuids[functionIdentifier];

		if (const Optional<const FunctionInfo*> pFunctionInfo = registry.FindFunctionDefinition(functionGuid))
		{
			if (identifier == m_functionIdentifierPropertyIdentifier)
			{
				return GenericDataIdentifier::MakeFromValidIndex(functionIdentifier.GetFirstValidIndex());
			}
			else if (identifier == m_functionNamePropertyIdentifier)
			{
				return pFunctionInfo->m_displayName;
			}
			else if (identifier == m_functionAssetGuidPropertyIdentifier)
			{
				return (Asset::Guid)functionGuid;
			}
		}

		return {};
	}

	// Events data source
	EngineRegistry::EventsDataSource::EventsDataSource()
		: Interface(System::Get<DataSource::Cache>().FindOrRegister(DataSourceGuid))
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.OnCreated(m_identifier, *this);

		m_eventIdentifierPropertyIdentifier = dataSourceCache.RegisterProperty("event_identifier");
		m_eventNamePropertyIdentifier = dataSourceCache.RegisterProperty("event_name");
		m_eventAssetGuidPropertyIdentifier = dataSourceCache.RegisterProperty("event_asset_guid");

		const EngineRegistry& registry = Memory::GetConstOwnerFromMember(*this, &EngineRegistry::m_eventsDataSource);
		for (const FunctionIdentifier functionIdentifier : registry.m_functionIdentifiers.GetView())
		{
			// if (typeInterface.GetFlags().IsNotSet(Reflection::TypeFlags::DisableUserInterfaceInstantiation))
			{
				const Guid functionGuid = registry.m_functionGuids[functionIdentifier];
				if (const Optional<const EventInfo*> pEventInfo = registry.FindEventDefinition(functionGuid))
				{
					System::Get<Asset::Manager>().RegisterAsset(
						functionGuid,
						Asset::DatabaseEntry{
							"f74004a9-23e3-4552-9bce-c8bcaaafb15e"_guid,
							functionGuid,
							IO::Path::Merge(functionGuid.ToString().GetView(), MAKE_PATH(".event"), Asset::VirtualAsset::FileExtension),
							UnicodeString(pEventInfo->m_displayName),
							UnicodeString{}
						},
						Asset::Identifier{}
					);
				}
			}
		}
	}

	EngineRegistry::EventsDataSource::~EventsDataSource()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.Deregister(dataSourceCache.Find(DataSourceGuid), DataSourceGuid);

		dataSourceCache.DeregisterProperty(m_eventIdentifierPropertyIdentifier, "event_identifier");
		dataSourceCache.DeregisterProperty(m_eventNamePropertyIdentifier, "event_name");
		dataSourceCache.DeregisterProperty(m_eventAssetGuidPropertyIdentifier, "event_asset_guid");
	}

	DataSource::GenericDataIndex EngineRegistry::EventsDataSource::GetDataCount() const
	{
		const EngineRegistry& registry = Memory::GetConstOwnerFromMember(*this, &EngineRegistry::m_eventsDataSource);
		return (GenericDataIndex)registry.m_functions.GetSize();
	}

	void EngineRegistry::EventsDataSource::CacheQuery(const Tag::Query& query, CachedQuery& cachedQueryOut) const
	{
		const EngineRegistry& registry = Memory::GetConstOwnerFromMember(*this, &EngineRegistry::m_eventsDataSource);

		FunctionMask& selectedFunctions = reinterpret_cast<FunctionMask&>(cachedQueryOut);
		selectedFunctions.ClearAll();

		if (query.m_allowedItems.IsValid())
		{
			selectedFunctions = FunctionMask(*query.m_allowedItems);
		}

		if (query.m_allowedFilterMask.AreAnySet())
		{
			FunctionMask allowedFilterMask;

			// Workaround while we don't have event tags
			// TODO: Move to Event definition
			const Guid uiVisibleTagGuid = "d7a7d2e6-ffb1-4c2c-8c1f-d6c3a32e3876"_guid;
			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
			if (query.m_allowedFilterMask.IsSet(tagRegistry.FindOrRegister(uiVisibleTagGuid)))
			{
				for (const FunctionIdentifier functionIdentifier : registry.m_functionIdentifiers.GetView())
				{
					const Guid functionGuid = registry.m_functionGuids[functionIdentifier];
					if (functionGuid.IsValid())
					{
						if (const Optional<const EventInfo*> pEventInfo = registry.FindEventDefinition(functionGuid))
						{
							if (pEventInfo->m_flags.IsSet(Reflection::EventFlags::VisibleToUI))
							{
								allowedFilterMask.Set(functionIdentifier);
							}
						}
					}
				}
			}

			/*for (const Tag::Identifier::IndexType tagIndex : query.m_allowedFilterMask.GetSetBitsIterator())
			{
			  const AtomicTypeMask* pTagAssets = tags[Tag::Identifier::MakeFromValidIndex(tagIndex)];
			  if (pTagAssets != nullptr)
			  {
			    allowedFilterMask |= *pTagAssets;
			  }
			}*/

			if (!query.m_allowedItems.IsValid())
			{
				selectedFunctions |= allowedFilterMask;
			}
			else
			{
				selectedFunctions &= allowedFilterMask;
			}
		}

		// Default to showing all entries if no allowed masks were set
		if (!query.m_allowedItems.IsValid() && query.m_allowedFilterMask.AreNoneSet())
		{
			for (const FunctionIdentifier functionIdentifier : registry.m_functionIdentifiers.GetView())
			{
				const Guid functionGuid = registry.m_functionGuids[functionIdentifier];
				if (functionGuid.IsValid())
				{
					if (const Optional<const EventInfo*> pEventInfo = registry.FindEventDefinition(functionGuid))
					{
						selectedFunctions.Set(functionIdentifier);
					}
				}
			}
		}

		/*for (const Tag::Identifier::IndexType tagIndex : query.m_disallowedFilterMask.GetSetBitsIterator())
		{
		  const Threading::Atomic<AtomicTypeMask*> pTagAssets = tags[Tag::Identifier::MakeFromValidIndex(tagIndex)];
		  if (pTagAssets == nullptr)
		  {
		    continue;
		  }

		  selectedFunctions.Clear(*pTagAssets);
		}*/

		/*if (query.m_requiredFilterMask.AreAnySet())
		{
		  for (const FunctionIdentifier::IndexType assetIndex : cachedQueryOut.GetSetBitsIterator())
		  {
		    const FunctionIdentifier functionIdentifier =
		registry.m_functionIdentifiers.GetActiveIdentifier(FunctionIdentifier::MakeFromValidIndex(assetIndex));

		    for (const Tag::Identifier::IndexType tagIndex : query.m_requiredFilterMask.GetSetBitsIterator())
		    {
		      const Tag::Identifier tagIdentifier = Tag::Identifier::MakeFromValidIndex(tagIndex);
		      if (!m_tags.IsSet(tagIdentifier, functionIdentifier))
		      {
		        selectedFunctions.Clear(functionIdentifier);
		        break;
		      }
		    }
		  }
		}*/
	}

	void EngineRegistry::EventsDataSource::IterateData(
		const CachedQuery& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset
	) const
	{
		const EngineRegistry& registry = Memory::GetConstOwnerFromMember(*this, &EngineRegistry::m_eventsDataSource);
		const FunctionMask& selectedElements = reinterpret_cast<const FunctionMask&>(query);

		const FunctionMask::SetBitsIterator iterator = selectedElements.GetSetBitsIterator();
		const FunctionMask::SetBitsIterator::Iterator begin = iterator.begin() + (uint16)offset.GetMinimum();
		const FunctionMask::SetBitsIterator::Iterator end =
			Math::Min(iterator.end(), iterator.begin() + (uint16)offset.GetMinimum() + (uint16)offset.GetSize());

		for (FunctionMask::SetBitsIterator::Iterator it = begin; it != end; ++it)
		{
			const FunctionIdentifier::IndexType assetIndex = *it;
			FunctionIdentifier functionIdentifier =
				registry.m_functionIdentifiers.GetActiveIdentifier(FunctionIdentifier::MakeFromValidIndex(assetIndex));
			callback(functionIdentifier);

			--offset;
			if (offset.GetSize() == 0)
			{
				break;
			}
		}
	}

	void EngineRegistry::EventsDataSource::IterateData(
		const SortedQueryIndices& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset
	) const
	{
		const EngineRegistry& registry = Memory::GetConstOwnerFromMember(*this, &EngineRegistry::m_eventsDataSource);
		for (const GenericDataIndex identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			FunctionIdentifier functionIdentifier =
				registry.m_functionIdentifiers.GetActiveIdentifier(FunctionIdentifier::MakeFromValidIndex(identifierIndex));
			callback(functionIdentifier);
		}
	}

	DataSource::PropertyValue
	EngineRegistry::EventsDataSource::GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const
	{
		const EngineRegistry& registry = Memory::GetConstOwnerFromMember(*this, &EngineRegistry::m_eventsDataSource);
		FunctionIdentifier functionIdentifier = data.GetExpected<FunctionIdentifier>();
		const Guid eventGuid = registry.m_functionGuids[functionIdentifier];

		if (const Optional<const EventInfo*> pEventInfo = registry.FindEventDefinition(eventGuid))
		{
			if (identifier == m_eventIdentifierPropertyIdentifier)
			{
				return GenericDataIdentifier::MakeFromValidIndex(functionIdentifier.GetFirstValidIndex());
			}
			else if (identifier == m_eventNamePropertyIdentifier)
			{
				return pEventInfo->m_displayName;
			}
			else if (identifier == m_eventAssetGuidPropertyIdentifier)
			{
				return (Asset::Guid)eventGuid;
			}
		}

		return {};
	}
}
