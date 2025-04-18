#include "Entity/ComponentRegistry.h"
#include "Entity/Component3D.h"

#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertyValue.h>
#include <Engine/Entity/ComponentTypeInterface.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Tag/TagContainer.inl>

#include <Common/System/Query.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Optional.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Reflection/GenericType.h>
#include <Common/Guid.h>
#include <Common/Asset/VirtualAsset.h>

namespace ngine::Entity
{
	struct Storage
	{
		UnorderedMap<Guid, UniquePtr<ComponentTypeInterface>, Guid::Hash> m_map;
	};

	/* static */ Storage& GetTypeStorage()
	{
		static Storage storage;
		return storage;
	}

	/*static*/ bool ComponentRegistry::Register(UniquePtr<ComponentTypeInterface>&& type)
	{
		Storage& storage = GetTypeStorage();
		const Guid guid = type->GetTypeInterface().GetGuid();
		Assert(!storage.m_map.Contains(guid));
		storage.m_map.Emplace(Guid(guid), Forward<UniquePtr<ComponentTypeInterface>>(type));
		return true;
	}

	ComponentRegistry::ComponentRegistry()
		: Interface(System::Get<DataSource::Cache>().FindOrRegister(DataSourceGuid))
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.OnCreated(m_identifier, *this);
		m_typeIdentifierPropertyIdentifier = dataSourceCache.RegisterProperty("component_type_id");
		m_typeGenericIdentifierPropertyIdentifier = dataSourceCache.RegisterProperty("component_type_generic_id");
		m_typeNamePropertyIdentifier = dataSourceCache.RegisterProperty("component_type_name");
		m_typeGuidPropertyIdentifier = dataSourceCache.RegisterProperty("component_type_guid");
		m_typeAssetGuidPropertyIdentifier = dataSourceCache.RegisterProperty("component_type_asset_guid");
		m_typeAssetIdentifierPropertyIdentifier = dataSourceCache.RegisterProperty("component_type_asset_id");
		m_typeThumbnailPropertyIdentifier = dataSourceCache.RegisterProperty("component_type_thumbnail_guid");

		Storage& storage = GetTypeStorage();
		m_lookupMap.Reserve(storage.m_map.GetSize());

		for (decltype(storage.m_map)::iterator it = storage.m_map.begin(), end = storage.m_map.end(); it != end; ++it)
		{
			Register(it->second.Get());
		}
	}

	ComponentRegistry::~ComponentRegistry()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.Deregister(m_identifier, dataSourceCache.FindGuid(m_identifier));

		dataSourceCache.DeregisterProperty(m_typeIdentifierPropertyIdentifier, "component_type_id");
		dataSourceCache.DeregisterProperty(m_typeGenericIdentifierPropertyIdentifier, "component_type_generic_id");
		dataSourceCache.DeregisterProperty(m_typeNamePropertyIdentifier, "component_type_name");
		dataSourceCache.DeregisterProperty(m_typeGuidPropertyIdentifier, "component_type_guid");
		dataSourceCache.DeregisterProperty(m_typeAssetGuidPropertyIdentifier, "component_type_asset_guid");
		dataSourceCache.DeregisterProperty(m_typeAssetIdentifierPropertyIdentifier, "component_type_asset_id");
		dataSourceCache.DeregisterProperty(m_typeThumbnailPropertyIdentifier, "component_type_thumbnail_guid");
	}

	void ComponentRegistry::Register(const Optional<ComponentTypeInterface*> pType)
	{
		const Guid guid = pType->GetTypeInterface().GetGuid();
		const ComponentTypeIdentifier identifier = m_componentTypeIdentifiers.AcquireIdentifier();
		Assert(identifier.IsValid());
		if (LIKELY(identifier.IsValid()))
		{
			pType->SetIdentifier(identifier);

			Assert(!m_lookupMap.Contains(guid));
			m_lookupMap.Emplace(Guid(guid), ComponentTypeIdentifier(identifier));
			m_componentTypes[identifier] = pType;
			++m_componentTypeCount;
			m_componentMask.Set(identifier);

			const ComponentTypeInterface& typeInfo = *m_componentTypes[identifier];
			const Reflection::TypeInterface& typeInterface = typeInfo.GetTypeInterface();
			const ComponentTypeExtension* pExtension = typeInfo.GetTypeExtension();
			if (pExtension && pExtension->m_categoryGuid.IsValid() && typeInterface.GetFlags().IsNotSet(Reflection::TypeFlags::DisableUserInterfaceInstantiation))
			{
				Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

				const Tag::Identifier assetTypeTagIdentifier = tagRegistry.FindOrRegister(
					pExtension->m_categoryGuid,
					typeInterface.GetName(),
					"#84CC1666"_colorf,
					Tag::Flags::ComponentType | Tag::Flags::VisibleToUI | Tag::Flags::Transient
				);
				m_tags.Set(assetTypeTagIdentifier, identifier);
			}

			if (typeInterface.GetFlags().IsNotSet(Reflection::TypeFlags::DisableUserInterfaceInstantiation))
			{
				Guid typeGuid = pExtension != nullptr ? pExtension->m_assetTypeOverrideGuid : Guid{};
				if (typeGuid.IsInvalid())
				{
					typeGuid = typeInterface.Implements(Reflection::GetTypeGuid<Entity::HierarchyComponentBase>())
					             ? Reflection::GetTypeGuid<HierarchyComponentBase>()
					             : Reflection::GetTypeGuid<Entity::Data::Component>();
				}

				System::Get<Asset::Manager>().RegisterAsset(
					guid,
					Asset::DatabaseEntry{
						typeGuid,
						guid,
						IO::Path::Merge(guid.ToString().GetView(), MAKE_PATH(".component"), Asset::VirtualAsset::FileExtension),
						UnicodeString(typeInterface.GetName()),
						UnicodeString{typeInterface.GetDescription()},
						pExtension != nullptr ? pExtension->m_iconAssetGuid : Asset::Guid()
					},
					Asset::Identifier{}
				);
			}
		}
	}

	void ComponentRegistry::Deregister(const Guid typeGuid)
	{
		auto it = m_lookupMap.Find(typeGuid);
		Assert(it != m_lookupMap.end());
		if (LIKELY(it != m_lookupMap.end()))
		{
			const ComponentTypeIdentifier typeIdentifier = it->second;
			m_lookupMap.Remove(it);
			m_componentTypes[typeIdentifier] = {};
			m_componentTypeCount--;
			m_componentMask.Clear(typeIdentifier);

			// TODO: Deregister tag and asset
			/*const ComponentTypeInterface& typeInfo = *m_componentTypes[identifier];
			const Reflection::TypeInterface& typeInterface = typeInfo.GetTypeInterface();
			const ComponentTypeExtension* pExtension = typeInfo.GetTypeExtension();
			if (pExtension && pExtension->m_categoryGuid.IsValid() &&
			typeInterface.GetFlags().IsNotSet(Reflection::TypeFlags::DisableUserInterfaceInstantiation))
			{
			    Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

			    const Tag::Identifier assetTypeTagIdentifier = tagRegistry.FindOrRegister(
			        pExtension->m_categoryGuid,
			        typeInterface.GetName(),
			        "#84CC1666"_colorf,
			        Tag::Flags::ComponentType | Tag::Flags::VisibleToUI | Tag::Flags::Transient
			    );
			    m_tags.Set(assetTypeTagIdentifier, identifier);
			}

			if (typeInterface.GetFlags().IsNotSet(Reflection::TypeFlags::DisableUserInterfaceInstantiation))
			{
			    Guid typeGuid = pExtension != nullptr ? pExtension->m_assetTypeOverrideGuid : Guid{};
			    if (typeGuid.IsInvalid())
			    {
			        typeGuid = typeInterface.Implements(Reflection::GetTypeGuid<Entity::HierarchyComponentBase>())
			                     ? Reflection::GetTypeGuid<HierarchyComponentBase>()
			                     : Reflection::GetTypeGuid<Entity::Data::Component>();
			    }

			    System::Get<Asset::Manager>().RegisterAsset(
			        guid,
			        Asset::DatabaseEntry{
			            typeGuid,
			            guid,
			            IO::Path::Merge(guid.ToString().GetView(), MAKE_PATH(".component"), Asset::VirtualAsset::FileExtension),
			            UnicodeString(typeInterface.GetName()),
			            UnicodeString{typeInterface.GetDescription()},
			            pExtension != nullptr ? pExtension->m_iconAssetGuid : Asset::Guid()
			        },
			        IO::PathView{}
			    );
			}*/
		}
	}

	Guid ComponentRegistry::GetGuid(const ComponentTypeIdentifier identifier) const
	{
		if (const Optional<const ComponentTypeInterface*> pTypeInterface = Get(identifier))
		{
			return pTypeInterface->GetTypeInterface().GetGuid();
		}
		return {};
	}

	void ComponentRegistry::IterateData(const CachedQuery& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset) const
	{
		const ComponentTypeMask& selectedComponentTypes = reinterpret_cast<const ComponentTypeMask&>(query);

		uint32 count = 0;
		const ComponentTypeMask::SetBitsIterator iterator =
			selectedComponentTypes.GetSetBitsIterator(0, m_componentTypeIdentifiers.GetMaximumUsedElementCount());
		for (auto it : iterator)
		{
			if (count >= offset.GetMinimum())
			{
				ComponentTypeIdentifier componentTypeIdentifier =
					m_componentTypeIdentifiers.GetActiveIdentifier(ComponentTypeIdentifier::MakeFromValidIndex(it));
				callback(componentTypeIdentifier);
				if (count >= offset.GetMaximum())
				{
					break;
				}
			}
			++count;
		}
	}

	void
	ComponentRegistry::IterateData(const SortedQueryIndices& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset) const
	{
		for (const GenericDataIndex identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			ComponentTypeIdentifier typeIdentifier =
				m_componentTypeIdentifiers.GetActiveIdentifier(ComponentTypeIdentifier::MakeFromValidIndex(identifierIndex));
			callback(typeIdentifier);
		}
	}

	DataSource::PropertyValue ComponentRegistry::GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const
	{
		ComponentTypeIdentifier componentTypeIdentifier = data.GetExpected<ComponentTypeIdentifier>();
		if (identifier == m_typeIdentifierPropertyIdentifier)
		{
			return componentTypeIdentifier;
		}
		else if (identifier == m_typeGenericIdentifierPropertyIdentifier)
		{
			const ComponentTypeIdentifier::IndexType index = componentTypeIdentifier.GetFirstValidIndex();
			return DataSource::GenericDataIdentifier::MakeFromValidIndex(index);
		}
		else if (identifier == m_typeNamePropertyIdentifier)
		{
			return m_componentTypes[componentTypeIdentifier]->GetTypeInterface().GetName();
		}
		else if (identifier == m_typeGuidPropertyIdentifier)
		{
			return m_componentTypes[componentTypeIdentifier]->GetTypeInterface().GetGuid();
		}
		else if (identifier == m_typeAssetGuidPropertyIdentifier)
		{
			if (Optional<const ComponentTypeExtension*> pExtension = m_componentTypes[componentTypeIdentifier]->GetTypeExtension())
			{
				if (pExtension->m_assetOverrideGuid.IsValid())
				{
					return pExtension->m_assetOverrideGuid;
				}
			}
			return Asset::Guid(m_componentTypes[componentTypeIdentifier]->GetTypeInterface().GetGuid());
		}
		else if (identifier == m_typeAssetIdentifierPropertyIdentifier)
		{
			Asset::Guid assetGuid = Asset::Guid(m_componentTypes[componentTypeIdentifier]->GetTypeInterface().GetGuid());
			if (Optional<const ComponentTypeExtension*> pExtension = m_componentTypes[componentTypeIdentifier]->GetTypeExtension())
			{
				if (pExtension->m_assetOverrideGuid.IsValid())
				{
					assetGuid = pExtension->m_assetOverrideGuid;
				}
			}
			if (assetGuid.IsValid())
			{
				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				return assetManager.GetAssetIdentifier(assetGuid);
			}
			return {};
		}
		else if (identifier == m_typeThumbnailPropertyIdentifier)
		{
			if (Optional<const ComponentTypeExtension*> pExtension = m_componentTypes[componentTypeIdentifier]->GetTypeExtension())
			{
				return pExtension->m_iconAssetGuid;
			}
		}
		return {};
	}

	static_assert(DataSource::GenericDataIdentifier::MaximumCount >= ComponentTypeIdentifier::MaximumCount);
	void ComponentRegistry::CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const
	{
		ComponentTypeMask& __restrict selectedComponentTypes = reinterpret_cast<ComponentTypeMask&>(cachedQueryOut);
		selectedComponentTypes.ClearAll();

		if (query.m_allowedItems.IsValid())
		{
			selectedComponentTypes = ComponentTypeMask(*query.m_allowedItems);
		}

		const Tag::AtomicMaskContainer<ComponentTypeIdentifier>::ConstRestrictedView tags = m_tags.GetView();
		if (query.m_allowedFilterMask.AreAnySet())
		{
			ComponentTypeMask allowedFilterMask;
			for (const Tag::Identifier::IndexType categoryIndex : query.m_allowedFilterMask.GetSetBitsIterator())
			{
				const Tag::Identifier categoryIdentifier = Tag::Identifier::MakeFromValidIndex(categoryIndex);
				const Threading::Atomic<Threading::AtomicIdentifierMask<ComponentTypeIdentifier>*> pTagCategories = tags[categoryIdentifier];
				if (pTagCategories != nullptr)
				{
					allowedFilterMask |= *pTagCategories;
				}
			}

			if (!query.m_allowedItems.IsValid())
			{
				selectedComponentTypes |= allowedFilterMask;
			}
			else
			{
				selectedComponentTypes &= allowedFilterMask;
			}
		}

		// Default to showing all entries if no allowed masks were set
		if (!query.m_allowedItems.IsValid() && query.m_allowedFilterMask.AreNoneSet())
		{
			selectedComponentTypes = m_componentMask;
		}

		if (query.m_allowedItems.IsValid())
		{
			ComponentTypeMask filteredComonentTypes = selectedComponentTypes;
			for (const ComponentIdentifier::IndexType componentIndex :
			     selectedComponentTypes.GetSetBitsIterator(0, m_componentTypeIdentifiers.GetMaximumUsedElementCount()))
			{
				const ComponentTypeIdentifier componentIdentifier = ComponentTypeIdentifier::MakeFromValidIndex(componentIndex);
				if (!query.m_allowedItems->IsSet(DataSource::GenericDataIdentifier::MakeFromValidIndex(componentIndex)))
				{
					filteredComonentTypes.Clear(componentIdentifier);
				}
			}
			selectedComponentTypes = filteredComonentTypes;
		}
	}
}

namespace ngine
{
	extern template struct UnorderedMap<Guid, Entity::ComponentTypeIdentifier, Guid::Hash>;
}
