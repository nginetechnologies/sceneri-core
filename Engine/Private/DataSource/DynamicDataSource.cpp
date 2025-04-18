#include "DataSource/DynamicDataSource.h"

#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertyValue.h>
#include <Engine/DataSource/DataSourceCache.h>

#include <Common/Algorithms/Sort.h>
#include <Common/System/Query.h>
#include <Common/Reflection/GenericType.h>
#include <Common/Serialization/Guid.h>
#include <Common/Serialization/Reader.h>
#include <Common/Memory/Variant.h>

namespace ngine::DataSource
{
	static ngine::DataSource::PropertyIdentifier DataIdPropertyIdentifier{};

	Dynamic::Dynamic(const Identifier identifier)
		: Interface(identifier)
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();

		if (DataIdPropertyIdentifier.IsInvalid())
		{
			DataIdPropertyIdentifier = dataSourceCache.RegisterProperty("data_id");
		}

		dataSourceCache.OnCreated(identifier, *this);
	}

	Dynamic::Dynamic(const Identifier identifier, const Dynamic& other)
		: Interface(identifier)
	{
		{
			Threading::SharedLock lock(other.m_dataMutex);
			m_data = other.m_data;
			m_identifiers = other.m_identifiers;
			m_dataMask = other.m_dataMask;
		}

		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.OnCreated(identifier, *this);
	}

	Dynamic::~Dynamic()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.Deregister(m_identifier, dataSourceCache.FindGuid(m_identifier));

		// Make sure we don't destroy before usage of the mutex is complete
		Threading::UniqueLock lock(m_dataMutex);
	}

	GenericDataIdentifier Dynamic::CreateDataIdentifier()
	{
		const GenericDataIdentifier identifier = m_identifiers.AcquireIdentifier();
		m_dataMask.Set(identifier);
		m_data.EmplaceBack();

		return identifier;
	}

	void Dynamic::AddDataProperty(GenericDataIdentifier dataIdentifier, PropertyIdentifier propertyIdentifier, PropertyValue&& value)
	{
		const GenericDataIdentifier identifier = m_identifiers.GetActiveIdentifier(dataIdentifier);
		if (identifier.IsValid())
		{
			m_data[dataIdentifier.GetFirstValidIndex()].Emplace(propertyIdentifier, Forward<PropertyValue>(value));
		}
	}

	Optional<const PropertyValue*> Dynamic::GetDataProperty(GenericDataIndex dataIndex, PropertyIdentifier propertyIdentifier) const
	{
		if (m_data.IsValidIndex(dataIndex))
		{
			const PropertyMap& propertyMap = m_data[dataIndex];
			const auto& propertyIt = propertyMap.Find(propertyIdentifier);
			if (propertyIt != propertyMap.end())
			{
				return &propertyIt->second;
			}
		}
		return {};
	}

	Optional<const PropertyValue*> Dynamic::GetDataProperty(GenericDataIdentifier dataIdentifier, PropertyIdentifier propertyIdentifier) const
	{
		const GenericDataIdentifier identifier = m_identifiers.GetActiveIdentifier(dataIdentifier);
		if (identifier.IsValid())
		{
			const PropertyMap& propertyMap = m_data[identifier.GetFirstValidIndex()];
			const auto& propertyIt = propertyMap.Find(propertyIdentifier);
			if (propertyIt != propertyMap.end())
			{
				return &propertyIt->second;
			}
		}
		return {};
	}

	GenericDataIdentifier Dynamic::FindDataIdentifier(PropertyIdentifier propertyIdentifier, const PropertyValue& value) const
	{
		const uint32 count = m_data.GetSize();
		for (uint32 index = 0; index < count; ++index)
		{
			const PropertyMap& propertyMap = m_data[index];
			const auto& propertyIt = propertyMap.Find(propertyIdentifier);
			if (propertyIt != propertyMap.end())
			{
				if (propertyIt->second == value)
				{
					return GenericDataIdentifier::MakeFromValidIndex(GenericDataIdentifier::IndexType(index));
				}
			}
		}
		return {};
	}

	void Dynamic::ClearData()
	{
		m_identifiers = TSaltedIdentifierStorage<GenericDataIdentifier>();
		m_data.Clear();
	}

	void Dynamic::LockRead()
	{
		[[maybe_unused]] const bool wasLocked = m_dataMutex.LockShared();
		Assert(wasLocked);
	}

	void Dynamic::UnlockRead()
	{
		m_dataMutex.UnlockShared();
	}

	void Dynamic::LockWrite()
	{
		[[maybe_unused]] const bool wasLocked = m_dataMutex.LockExclusive();
		Assert(wasLocked);
	}

	void Dynamic::UnlockWrite()
	{
		m_dataMutex.UnlockExclusive();
		OnDataChanged();
	}

	void Dynamic::CacheQuery(const Query& query, CachedQuery& cachedQuery) const
	{
		GenericDataMask& selectedData = reinterpret_cast<GenericDataMask&>(cachedQuery);
		selectedData.ClearAll();

		if (query.m_allowedItems.IsValid())
		{
			selectedData = *query.m_allowedItems;
		}

		// Default to showing all entries if no allowed masks were set
		if (!query.m_allowedItems.IsValid() && query.m_allowedFilterMask.AreNoneSet())
		{
			for (const GenericDataIdentifier identifier : m_identifiers.GetView())
			{
				selectedData.Set(identifier);
			}
		}
	}

	bool Dynamic::SortQuery(
		const CachedQuery& cachedQuery, const PropertyIdentifier propertyIdentifier, const SortingOrder order, SortedQueryIndices& sortedQuery
	)
	{
		const GenericDataMask& __restrict dataMask = reinterpret_cast<const GenericDataMask&>(cachedQuery);
		sortedQuery.Clear();
		sortedQuery.Reserve(m_dataMask.GetNumberOfSetBits());

		for (GenericDataIdentifier::IndexType identifierIndex : dataMask.GetSetBitsIterator())
		{
			sortedQuery.EmplaceBack(identifierIndex);
		}

		const Optional<const PropertyValue*> pValue = GetDataProperty(GenericDataIdentifier::MakeFromValidIndex(0), propertyIdentifier);
		if (pValue.IsValid() && pValue->Is<UnicodeString>())
		{
			Algorithms::Sort(
				(GenericDataIdentifier::IndexType*)sortedQuery.begin(),
				(GenericDataIdentifier::IndexType*)sortedQuery.end(),
				[this,
			   propertyIdentifier,
			   order](const GenericDataIdentifier::IndexType leftIndex, const GenericDataIdentifier::IndexType rightIndex) -> bool
				{
					const Optional<const PropertyValue*> pLeft =
						GetDataProperty(GenericDataIdentifier::MakeFromValidIndex(leftIndex), propertyIdentifier);
					const Optional<const PropertyValue*> pRight =
						GetDataProperty(GenericDataIdentifier::MakeFromValidIndex(rightIndex), propertyIdentifier);

					const ConstUnicodeStringView left = pLeft ? pLeft->GetExpected<UnicodeString>().GetView() : ConstUnicodeStringView();
					const ConstUnicodeStringView right = pRight ? pRight->GetExpected<UnicodeString>().GetView() : ConstUnicodeStringView();

					if (order == DataSource::SortingOrder::Descending)
					{
						return left.LessThanCaseInsensitive(right);
					}
					else
					{
						return right.LessThanCaseInsensitive(left);
					}
				}
			);
			return true;
		}
		return false;
	}

	GenericDataIndex Dynamic::GetDataCount() const
	{
		return GenericDataIndex(m_data.GetSize());
	}

	void Dynamic::IterateData(const CachedQuery& cachedQuery, IterationCallback&& callback, const Math::Range<GenericDataIndex> offset) const
	{
		const GenericDataMask& selectedData = reinterpret_cast<const GenericDataMask&>(cachedQuery);
		if (selectedData.AreAnySet())
		{
			uint32 count = 0;
			const GenericDataMask::SetBitsIterator iterator = selectedData.GetSetBitsIterator(0, m_identifiers.GetMaximumUsedElementCount());
			for (auto it : iterator)
			{
				if (count >= offset.GetMinimum())
				{
					GenericDataIdentifier dataIdentifier = m_identifiers.GetActiveIdentifier(GenericDataIdentifier::MakeFromValidIndex(it));
					callback(dataIdentifier);
					if (count >= offset.GetMaximum())
					{
						break;
					}
				}
				++count;
			}
		}
	}

	void Dynamic::IterateData(const SortedQueryIndices& query, IterationCallback&& callback, const Math::Range<GenericDataIndex> offset) const
	{
		for (const GenericDataIndex identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			GenericDataIdentifier dataIdentifier = m_identifiers.GetActiveIdentifier(GenericDataIdentifier::MakeFromValidIndex(identifierIndex));
			callback(dataIdentifier);
		}
	}

	PropertyValue Dynamic::GetDataProperty(const Data data, const PropertyIdentifier propertyIdentifier) const
	{
		const GenericDataIdentifier dataIdentifier = m_identifiers.GetActiveIdentifier(data.GetExpected<GenericDataIdentifier>());
		if (dataIdentifier.IsValid())
		{
			if (propertyIdentifier == DataIdPropertyIdentifier)
			{
				return dataIdentifier;
			}
			else
			{
				const PropertyMap& propertyMap = m_data[dataIdentifier.GetFirstValidIndex()];
				const auto& propertyIt = propertyMap.Find(propertyIdentifier);
				if (propertyIt != propertyMap.end())
				{
					return propertyIt->second;
				}
			}
		}
		return {};
	}

	bool Dynamic::Serialize(const Serialization::Reader serializer, Cache& cache)
	{
		if (const Optional<Serialization::Reader> entriesReader = serializer.FindSerializer("entries"))
		{
			LockWrite();
			for (const Serialization::Reader entrySerializer : entriesReader->GetArrayView())
			{
				if (entrySerializer.IsString())
				{
					const GenericDataIdentifier dataIdentifier = CreateDataIdentifier();

					const ConstStringView entryName = *entrySerializer.ReadInPlace<ConstStringView>();

					const PropertyIdentifier propertyIdentifier = cache.FindOrRegisterPropertyIdentifier("entry_name");
					AddDataProperty(dataIdentifier, propertyIdentifier, ngine::DataSource::PropertyValue(UnicodeString(entryName)));
				}
				else if (const Optional<Serialization::Reader> propertiesReader = entrySerializer.FindSerializer("properties"))
				{
					const GenericDataIdentifier dataIdentifier = CreateDataIdentifier();

					for (const Serialization::Reader propertySerializer : propertiesReader->GetArrayView())
					{
						ConstStringView propertyName = propertySerializer.ReadWithDefaultValue<ConstStringView>("property", {});
						if (propertyName.HasElements())
						{
							// Remove brackets if present
							propertyName += propertyName[0] == '{';
							propertyName -= propertyName.GetLastElement() == '}';

							const PropertyIdentifier propertyIdentifier = cache.FindOrRegisterPropertyIdentifier(propertyName);
							if (propertyIdentifier.IsValid())
							{
								if (const Optional<Serialization::Reader> stringReader = propertySerializer.FindSerializer("string"))
								{
									UnicodeString value = stringReader->ReadInPlaceWithDefaultValue<UnicodeString>({});
									if (value.HasElements())
									{
										AddDataProperty(dataIdentifier, propertyIdentifier, Move(value));
									}
								}
								else if (const Optional<Serialization::Reader> assetGuidReader = propertySerializer.FindSerializer("asset"))
								{
									Asset::Guid value = assetGuidReader->ReadInPlaceWithDefaultValue<Asset::Guid>({});
									if (value.IsValid())
									{
										AddDataProperty(dataIdentifier, propertyIdentifier, Move(value));
									}
								}
								else if (const Optional<Serialization::Reader> guidReader = propertySerializer.FindSerializer("guid"))
								{
									Guid value = guidReader->ReadInPlaceWithDefaultValue<Guid>({});
									if (value.IsValid())
									{
										AddDataProperty(dataIdentifier, propertyIdentifier, Move(value));
									}
								}
								else if (const Optional<Serialization::Reader> boolReader = propertySerializer.FindSerializer("bool"))
								{
									bool value = boolReader->ReadInPlaceWithDefaultValue<bool>(false);
									AddDataProperty(dataIdentifier, propertyIdentifier, value);
								}
								else
								{
									Assert(false, "Dynamic property serializer not implemented.");
								}
							}
						}
					}
				}
			}
			UnlockWrite();
		}
		return true;
	}

	bool Dynamic::Serialize(Serialization::Writer serializer, Cache& cache) const
	{
		return serializer.SerializeArrayWithCallback(
			"entries",
			[data = m_data.GetView(), &cache](Serialization::Writer writer, const uint32 index)
			{
				const PropertyMap& propertyMap = data[index];
				return writer.SerializeArrayWithCallback(
					"properties",
					[it = propertyMap.begin(), &cache](Serialization::Writer writer, [[maybe_unused]] const uint32 index) mutable
					{
						writer.Serialize("property", cache.FindPropertyName(it->first));
						const PropertyValue& propertyValue = it->second;
						++it;
						if (const Optional<const UnicodeString*> string = propertyValue.Get<UnicodeString>())
						{
							return writer.Serialize("string", *string);
						}
						else if (const Optional<const Asset::Guid*> assetGuid = propertyValue.Get<Asset::Guid>())
						{
							return writer.Serialize("asset", *assetGuid);
						}
						else if (const Optional<const Guid*> guid = propertyValue.Get<Guid>())
						{
							return writer.Serialize("guid", *guid);
						}
						else if (const Optional<const bool*> boolean = propertyValue.Get<bool>())
						{
							return writer.Serialize("bool", *boolean);
						}
						else
						{
							Assert(false, "Dynamic property serializer not implemented.");
							return false;
						}
					},
					propertyMap.GetSize()
				);
			},
			m_data.GetSize()
		);
	}
}
