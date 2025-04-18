#include "Engine/Tag/TagRegistry.h"
#include "Engine/Tag/TagContainer.h"

#include "Engine/Engine.h"
#include <Engine/Asset/AssetType.inl>
#include <Engine/Entity/Component.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertyValue.h>

#include <Tag/TagMaskProperty.h>

#include <Common/Memory/OffsetOf.h>
#include <Common/Reflection/Registry.inl>
#include <Common/System/Query.h>
#include <Common/Asset/TagAssetType.h>
#include <Common/Asset/VirtualAsset.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>
#include <Common/Serialization/Guid.h>

namespace ngine::Tag
{
	inline static constexpr Guid AddableTagGuid = "0f705cec-fb8a-4cfa-ab55-66dd186f5a30"_guid;
	inline static constexpr Guid VisibleTagGuid = "{5A49A9A1-5C96-4FBB-B68C-5541DB0EA763}"_guid;
	inline static constexpr Guid ReflectedTypeTagGuid = "{05f16783-e102-46ab-8bea-6595b4dcb3eb}"_guid;

	Registry::Registry()
		: Interface(System::Get<DataSource::Cache>().FindOrRegister(DataSourceGuid))
	{
		System::Query& systemQuery = System::Query::GetInstance();
		systemQuery.RegisterSystem(*this);

		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.OnCreated(m_identifier, *this);
		m_tagGenericIdentifierPropertyIdentifier = dataSourceCache.RegisterProperty("tag_generic_id");
		m_tagNamePropertyIdentifier = dataSourceCache.RegisterProperty("tag_name");
		m_tagGuidPropertyIdentifier = dataSourceCache.RegisterProperty("tag_guid");
		m_tagColorPropertyIdentifier = dataSourceCache.RegisterProperty("tag_color");

		Register(AddableTagGuid);
		Register(VisibleTagGuid);
		Register(Reflection::GetTypeGuid<Entity::Component>());
	}

	Registry::~Registry()
	{
		System::Query& systemQuery = System::Query::GetInstance();
		systemQuery.DeregisterSystem<Registry>();

		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.Deregister(m_identifier, dataSourceCache.FindGuid(m_identifier));

		dataSourceCache.DeregisterProperty(m_tagGenericIdentifierPropertyIdentifier, "tag_generic_id");
		dataSourceCache.DeregisterProperty(m_tagNamePropertyIdentifier, "tag_name");
		dataSourceCache.DeregisterProperty(m_tagGuidPropertyIdentifier, "tag_guid");
		dataSourceCache.DeregisterProperty(m_tagColorPropertyIdentifier, "tag_color");
	}

	Identifier Registry::Register(const Asset::Guid tagGuid, const EnumFlags<Flags> flags)
	{
		Assert(flags.IsNotSet(Flags::VisibleToUI) && flags.IsNotSet(Flags::Removable), "Visible tags must provide name and color!");
		return BaseType::RegisterAsset(
			tagGuid,
			[flags](const Identifier, const Asset::Guid) -> Info
			{
				return Info{Math::Color{}, flags};
			}
		);
	}

	Identifier Registry::FindOrRegister(const Asset::Guid tagGuid, const EnumFlags<Flags> flags)
	{
		Assert(flags.IsNotSet(Flags::VisibleToUI) && flags.IsNotSet(Flags::Removable), "Visible tags must provide name and color!");
		return BaseType::FindOrRegisterAsset(
			tagGuid,
			[flags](const Identifier, const Asset::Guid) -> Info
			{
				return Info{Math::Color{}, flags};
			}
		);
	}

	Identifier
	Registry::Register(const Asset::Guid tagGuid, const ConstUnicodeStringView name, const Math::Color color, const EnumFlags<Flags> flags)
	{
		const Identifier identifier = BaseType::RegisterAsset(
			tagGuid,
			[color, flags](const Identifier, const Asset::Guid) -> Info
			{
				return Info{color, flags};
			}
		);
		if (flags.IsSet(Flags::VisibleToUI))
		{
			RegisterVirtualAsset(identifier, name);
		}
		return identifier;
	}

	Identifier Registry::FindOrRegister(
		const Asset::Guid tagGuid, const ConstUnicodeStringView name, const Math::Color color, const EnumFlags<Flags> flags
	)
	{
		const Identifier identifier = BaseType::FindOrRegisterAsset(
			tagGuid,
			[color, flags](const Identifier, const Asset::Guid) -> Info
			{
				return Info{color, flags};
			}
		);
		Info& tagInfo = GetAssetData(identifier);
		tagInfo.m_flags |= flags;
		tagInfo.m_color = color;

		if (tagInfo.m_flags.IsSet(Flags::VisibleToUI))
		{
			RegisterVirtualAsset(identifier, name);
		}
		return identifier;
	}

	void Registry::RegisterVirtualAsset(const Identifier identifier, const ConstUnicodeStringView name)
	{
		Assert(GetAssetData(identifier).m_flags.IsSet(Flags::VisibleToUI));

		Asset::Manager& assetManager = System::Get<Asset::Manager>();
		const Asset::Guid assetGuid = GetAssetGuid(identifier);
		if (!assetManager.HasAsset(assetGuid))
		{
			assetManager.RegisterAsset(
				assetGuid,
				Asset::DatabaseEntry{
					TagAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Merge(assetGuid.ToString().GetView(), MAKE_PATH(".tag"), Asset::VirtualAsset::FileExtension),
					UnicodeString(name)
				},
				Asset::Identifier{}
			);
		}
	}

	void Registry::RegisterAsset(const Asset::Guid tagGuid)
	{
		// Ignore the default tag
		if (tagGuid == "0e118789-2b17-474b-b46c-4627a2d17a7f"_asset)
		{
			return;
		}

		const EnumFlags<Flags> flags{Flags::VisibleToUI | Flags::Addable | Flags::Removable};
		Asset::Manager& assetManager = System::Get<Asset::Manager>();
		if (assetManager.GetAssetPath(tagGuid).GetRightMostExtension() == Asset::VirtualAsset::FileExtension)
		{
			return;
		}

		Math::Color color = assetManager.VisitAssetEntry(
			tagGuid,
			[](const Optional<const Asset::DatabaseEntry*> pDatabaseEntry) -> Math::Color
			{
				Assert(pDatabaseEntry.IsValid());
				if (LIKELY(pDatabaseEntry.IsValid()))
				{
					auto metadataEntryIt = pDatabaseEntry->m_metaData.Find(TagAssetType::AssetFormat.assetTypeGuid);
					Assert(metadataEntryIt != pDatabaseEntry->m_metaData.end())
					{
						const Optional<const Math::Color*> color = metadataEntryIt->second.Get<Math::Color>();
						Assert(color.IsValid());
						if (LIKELY(color.IsValid()))
						{
							return *color;
						}
					}
				}
				return "#ffffff"_color;
			}
		);

		const Identifier identifier = BaseType::FindOrRegisterAsset(
			tagGuid,
			[color, flags](const Identifier, const Asset::Guid) -> Info
			{
				return Info{color, flags};
			}
		);
		Info& tagInfo = GetAssetData(identifier);
		tagInfo.m_flags |= flags;
		tagInfo.m_color = color;
	}

	DataSource::GenericDataIndex Registry::GetDataCount() const
	{
		DataSource::GenericDataIndex count{0};
		IterateElements(
			[&count](const Info& tagInfo)
			{
				count += tagInfo.m_flags.IsSet(Flags::VisibleToUI);
			}
		);
		return count;
	}

	void Registry::IterateData(const CachedQuery& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset) const
	{
		uint32 count = 0;
		const CachedQuery::SetBitsIterator iterator = query.GetSetBitsIterator(0, GetMaximumUsedIdentifierCount());
		for (auto it : iterator)
		{
			if (count >= offset.GetMinimum())
			{
				GenericDataIdentifier tagIdentifier = GenericDataIdentifier::MakeFromValidIndex(it);
				callback(tagIdentifier);
				if (count >= offset.GetMaximum())
				{
					break;
				}
			}
			++count;
		}
	}

	void Registry::IterateData(const SortedQueryIndices& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset) const
	{
		for (const GenericDataIndex identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			GenericDataIdentifier tagIdentifier = GenericDataIdentifier::MakeFromValidIndex(identifierIndex);
			callback(tagIdentifier);
		}
	}

	DataSource::PropertyValue Registry::GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const
	{
		GenericDataIdentifier genericIdentifier = data.GetExpected<GenericDataIdentifier>();
		if (identifier == m_tagNamePropertyIdentifier)
		{
			const Identifier tagIdentifier = Identifier::MakeFromValidIndex((Identifier::IndexType)genericIdentifier.GetFirstValidIndex());
			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			return UnicodeString(assetManager.GetAssetName(GetAssetGuid(tagIdentifier)));
		}
		else if (identifier == m_tagGuidPropertyIdentifier)
		{
			const Identifier tagIdentifier = Identifier::MakeFromValidIndex((Identifier::IndexType)genericIdentifier.GetFirstValidIndex());
			return GetAssetGuid(tagIdentifier);
		}
		else if (identifier == m_tagColorPropertyIdentifier)
		{
			const Identifier tagIdentifier = Identifier::MakeFromValidIndex((Identifier::IndexType)genericIdentifier.GetFirstValidIndex());
			return GetAssetData(tagIdentifier).m_color;
		}
		else if (identifier == m_tagGenericIdentifierPropertyIdentifier)
		{
			return genericIdentifier;
		}
		return {};
	}

	void Registry::CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const
	{
		cachedQueryOut.ClearAll();

		if (query.m_allowedItems.IsValid())
		{
			cachedQueryOut = *query.m_allowedItems;
		}

		const auto getFlagsFromMask = [this,
		                               addableTagIdentifier = FindIdentifier(AddableTagGuid),
		                               visibleTagIdentifier = FindIdentifier(VisibleTagGuid),
		                               componentTagIdentifier = FindIdentifier(Reflection::GetTypeGuid<Entity::Component>()),
		                               reflectedTypeTagIdentifier = FindIdentifier(ReflectedTypeTagGuid)](Mask& mask)
		{
			EnumFlags<Flags> flags;
			for (const Tag::Identifier::IndexType tagIdentifierIndex : mask.GetSetBitsIterator(0, GetMaximumUsedIdentifierCount()))
			{
				const Tag::Identifier tagIdentifier = Tag::Identifier::MakeFromValidIndex(tagIdentifierIndex);
				if (tagIdentifier == addableTagIdentifier)
				{
					flags |= Flags::Addable;
					mask.Clear(tagIdentifier);
				}
				else if (tagIdentifier == visibleTagIdentifier)
				{
					flags |= Flags::VisibleToUI;
					mask.Clear(tagIdentifier);
				}
				else if (tagIdentifier == componentTagIdentifier)
				{
					flags |= Flags::ComponentType;
					mask.Clear(tagIdentifier);
				}
				else if (tagIdentifier == reflectedTypeTagIdentifier)
				{
					flags |= Flags::ReflectedType;
					mask.Clear(tagIdentifier);
				}
			}
			return flags;
		};

		if (query.m_allowedItems.IsValid())
		{
			Mask& __restrict maskOut = reinterpret_cast<Mask&>(cachedQueryOut);
			Mask filteredTypes = maskOut;
			for (const Identifier::IndexType tagIndex : maskOut.GetSetBitsIterator(0, GetMaximumUsedIdentifierCount()))
			{
				const Identifier tagIdentifier = Identifier::MakeFromValidIndex(tagIndex);
				if (!query.m_allowedItems->IsSet(DataSource::GenericDataIdentifier::MakeFromValidIndex(tagIndex)))
				{
					filteredTypes.Clear(tagIdentifier);
				}
			}
			maskOut = filteredTypes;
		}

		if (query.m_allowedFilterMask.AreAnySet())
		{
			Mask allowedFilters{reinterpret_cast<const Mask&>(query.m_allowedFilterMask)};
			const EnumFlags<Flags> allowedFlags(getFlagsFromMask(allowedFilters));

			Mask allowedFilterMask{allowedFilters};
			IterateElements(
				[elements = GetValidElementView(), &allowedFilterMask, allowedFlags](const Info& tagInfo)
				{
					if (tagInfo.m_flags.AreAnySet(allowedFlags))
					{
						const Identifier tagIdentifier = Identifier::MakeFromValidIndex(elements.GetIteratorIndex(&tagInfo));
						allowedFilterMask.Set(tagIdentifier);
					}
				}
			);

			if (!query.m_allowedItems.IsValid())
			{
				reinterpret_cast<Mask&>(cachedQueryOut) |= allowedFilterMask;
			}
			else
			{
				reinterpret_cast<Mask&>(cachedQueryOut) &= allowedFilterMask;
			}
		}

		// Default to showing all entries if no allowed masks were set
		if (!query.m_allowedItems.IsValid() && query.m_allowedFilterMask.AreNoneSet())
		{
			IterateElements(
				[elements = GetValidElementView(), &cachedQueryOut](const Info& tagInfo)
				{
					if (tagInfo.m_flags.IsSet(Flags::VisibleToUI))
					{
						const Identifier identifier = Identifier::MakeFromValidIndex(elements.GetIteratorIndex(&tagInfo));
						cachedQueryOut.Set(GenericDataIdentifier::MakeFromValidIndex(identifier.GetFirstValidIndex()));
					}
				}
			);
		}

		const bool hasDisallowed = query.m_disallowedFilterMask.AreAnySet();
		const bool hasRequired = query.m_requiredFilterMask.AreAnySet();
		if (hasRequired || hasDisallowed)
		{
			Mask requiredFilters{reinterpret_cast<const Mask&>(query.m_requiredFilterMask)};
			const EnumFlags<Flags> requiredFlags(getFlagsFromMask(requiredFilters));
			Mask disallowedFilters{reinterpret_cast<const Mask&>(query.m_disallowedFilterMask)};
			const EnumFlags<Flags> disallowedFlags(getFlagsFromMask(disallowedFilters));

			if (requiredFilters.AreAnySet())
			{
				reinterpret_cast<Mask&>(cachedQueryOut) &= requiredFilters;
			}
			if (disallowedFilters.AreAnySet())
			{
				reinterpret_cast<Mask&>(cachedQueryOut) &= ~disallowedFilters;
			}

			for (const GenericDataIndex tagIndex : cachedQueryOut.GetSetBitsIterator(0, GetMaximumUsedIdentifierCount()))
			{
				const Identifier tagIdentifier = Identifier::MakeFromValidIndex((Identifier::IndexType)tagIndex);
				const Info& __restrict tagInfo = GetAssetData(tagIdentifier);
				if (!tagInfo.m_flags.AreAllSet(requiredFlags) | tagInfo.m_flags.AreAnySet(disallowedFlags))
				{
					cachedQueryOut.Clear(GenericDataIdentifier::MakeFromValidIndex(tagIndex));
				}
			}
		}
	}

	bool MaskProperty::Serialize(const Serialization::Reader reader)
	{
		return reader.SerializeInPlace(m_mask, m_registry);
	}

	bool MaskProperty::Serialize(Serialization::Writer writer) const
	{
		return writer.SerializeInPlace(m_mask, m_registry);
	}

	bool Mask::Serialize(const Serialization::Reader serializer, Registry& registry)
	{
		for (const Serialization::Reader guidSerializer : serializer.GetArrayView())
		{
			Asset::Guid guid;
			if (guidSerializer.SerializeInPlace(guid))
			{
				const Identifier identifier = registry.FindOrRegister(guid);
				Assert(identifier.IsValid());
				if (LIKELY(identifier.IsValid()))
				{
					Set(identifier);
				}
			}
		}
		return true;
	}

	bool Mask::Serialize(Serialization::Writer serializer, const Registry& registry) const
	{
		if (BaseType::AreNoneSet())
		{
			return false;
		}

		using BitIndexType = typename BaseType::BitIndexType;
		const BitIndexType numSetBits = GetNumberOfSetBits();
		if (numSetBits == 0)
		{
			return false;
		}

		Serialization::Value& value = serializer.GetValue();
		value = Serialization::Value(rapidjson::Type::kArrayType);
		value.Reserve((rapidjson::SizeType)numSetBits, serializer.GetDocument().GetAllocator());

		IterateSetBits(
			[&registry, &value, &serializer](const BitIndexType index)
			{
				const Guid guid = registry.GetAssetGuid(Identifier::MakeFromValidIndex(index));

				Serialization::Value guidValue;
				Serialization::Writer writer(guidValue, serializer.GetData());
				Assert(guid.IsValid());
				if (LIKELY(writer.SerializeInPlace(guid)))
				{
					value.PushBack(Move(guidValue), serializer.GetDocument().GetAllocator());
				}
				return true;
			}
		);

		return value.Size() > 0;
	}

	[[maybe_unused]] const bool wasAssetTypeRegistered = Reflection::Registry::RegisterType<TagAssetType>();
}
