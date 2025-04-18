#include "DataSource/DynamicPropertySource.h"

#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertyValue.h>
#include <Engine/DataSource/DataSourceCache.h>

#include <Common/Algorithms/Sort.h>
#include <Common/System/Query.h>
#include <Common/Reflection/GenericType.h>

#include <Common/Serialization/Guid.h>
#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>

namespace ngine::PropertySource
{
	Dynamic::Dynamic(const Identifier identifier)
		: Interface(identifier)
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.GetPropertySourceCache().OnCreated(identifier, *this);
	}

	Dynamic::Dynamic(const Identifier identifier, const Dynamic& other)
		: Interface(identifier)
	{
		{
			Threading::SharedLock lock(other.m_dataMutex);
			m_propertyMap = other.m_propertyMap;
		}

		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.GetPropertySourceCache().OnCreated(identifier, *this);
	}

	Dynamic::~Dynamic()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.GetPropertySourceCache().Deregister(m_identifier, dataSourceCache.GetPropertySourceCache().FindGuid(m_identifier));

		// Make sure we don't destroy before usage of the mutex is complete
		Threading::UniqueLock lock(m_dataMutex);
	}

	void Dynamic::AddDataProperty(PropertyIdentifier propertyIdentifier, PropertyValue&& value)
	{
		m_propertyMap.EmplaceOrAssign(propertyIdentifier, Forward<PropertyValue>(value));
	}

	void Dynamic::ClearData()
	{
		m_propertyMap.Clear();
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

	PropertyValue Dynamic::GetDataProperty(const PropertyIdentifier propertyIdentifier) const
	{
		const auto& propertyIt = m_propertyMap.Find(propertyIdentifier);
		if (propertyIt != m_propertyMap.end())
		{
			return propertyIt->second;
		}
		return {};
	}

	bool Dynamic::Serialize(const Serialization::Reader serializer, DataSource::Cache& dataSourceCache)
	{
		if (const Optional<Serialization::Reader> propertiesReader = serializer.FindSerializer("properties"))
		{
			LockWrite();
			for (Serialization::Member<Serialization::Reader> propertyMember : propertiesReader->GetMemberView())
			{
				const PropertyIdentifier propertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier(propertyMember.key);
				if (propertyIdentifier.IsValid())
				{
					const Serialization::Reader propertySerializer = propertyMember.value;
					if (const Optional<Serialization::Reader> stringReader = propertySerializer.FindSerializer("string"))
					{
						UnicodeString value = stringReader->ReadInPlaceWithDefaultValue<UnicodeString>({});
						if (value.HasElements())
						{
							m_propertyMap.Emplace(propertyIdentifier, Move(value));
						}
					}
					else if (const Optional<Serialization::Reader> assetGuidReader = propertySerializer.FindSerializer("asset"))
					{
						Asset::Guid value = assetGuidReader->ReadInPlaceWithDefaultValue<Asset::Guid>({});
						if (value.IsValid())
						{
							m_propertyMap.Emplace(propertyIdentifier, Move(value));
						}
					}
					else if (const Optional<Serialization::Reader> guidReader = propertySerializer.FindSerializer("guid"))
					{
						Guid value = guidReader->ReadInPlaceWithDefaultValue<Guid>({});
						if (value.IsValid())
						{
							m_propertyMap.Emplace(propertyIdentifier, Move(value));
						}
					}
					else if (const Optional<Serialization::Reader> boolReader = propertySerializer.FindSerializer("bool"))
					{
						bool value = boolReader->ReadInPlaceWithDefaultValue<bool>(false);
						m_propertyMap.Emplace(propertyIdentifier, value);
					}
					else
					{
						Assert(false, "Dynamic property serializer not implemented.");
					}
				}
			}
			UnlockWrite();
		}
		return true;
	}

	bool Dynamic::Serialize(Serialization::Writer writer, DataSource::Cache& dataSourceCache) const
	{
		Threading::UniqueLock lock(m_dataMutex);
		if (m_propertyMap.HasElements())
		{
			return writer.SerializeObjectWithCallback(
				"properties",
				[&propertyMap = m_propertyMap, &dataSourceCache](Serialization::Writer writer)
				{
					bool wroteAny = false;
					for (auto it = propertyMap.begin(), endIt = propertyMap.end(); it != endIt; ++it)
					{
						wroteAny |= writer.SerializeObjectWithCallback(
							dataSourceCache.FindPropertyName(it->first),
							[&propertyValue = it->second](Serialization::Writer writer)
							{
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
							}
						);
					}
					return wroteAny;
				}
			);
		}
		else
		{
			return false;
		}
	}
}
