#pragma once

#include <Engine/Asset/Identifier.h>
#include <Engine/Asset/Mask.h>
#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/Tag/TagContainer.h>

#include <Common/Asset/AssetDatabase.h>
#include <Common/Storage/IdentifierArray.h>

#include <Common/Function/Event.h>
#include <Common/Memory/UniqueRef.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Math/Range.h>
#include <Common/Math/NumericLimits.h>
#include <Common/Time/Timestamp.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Storage/AtomicIdentifierMask.h>

#include <Common/IO/AsyncLoadCallback.h>
#include <Common/Threading/Jobs/JobPriority.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Threading/AtomicPtr.h>

namespace ngine
{
	extern template struct UnorderedMap<Asset::Guid, Asset::Identifier, Asset::Guid::Hash>;
	extern template struct TSaltedIdentifierStorage<Asset::Identifier>;

	extern template struct Tag::AtomicMaskContainer<Asset::Identifier>;

	extern template struct UnorderedMap<IO::Path, Asset::Guid, IO::Path::Hash>;
	extern template struct UnorderedMap<IO::Path, Asset::Identifier, IO::Path::Hash>;
}

namespace ngine::Asset
{
	struct EngineAssetDatabase : protected Database, public DataSource::Interface
	{
		EngineAssetDatabase(const Guid dataSourceGuid);
		virtual ~EngineAssetDatabase();

		static void InitializePropertyIdentifiers();

		[[nodiscard]] bool Load(
			const Serialization::Reader databaseReader,
			const IO::PathView databaseRootDirectory,
			const Identifier rootFolderAssetIdentifier,
			const ArrayView<const Tag::Identifier, uint8> tagIdentifiers = {}
		);

		[[nodiscard]] Database& GetDatabase()
		{
			return *this;
		}

		[[nodiscard]] Identifier::IndexType GetMaximumUsedElementCount() const
		{
			return m_assetIdentifiers.GetMaximumUsedElementCount();
		}

		[[nodiscard]] Guid GetAssetGuid(const Identifier identifier) const
		{
			return m_assets[identifier];
		}
		[[nodiscard]] Identifier GetAssetIdentifier(const Guid assetGuid) const
		{
			Threading::UniqueLock lock(m_assetIdentifierLookupMapMutex);
			auto it = m_assetIdentifierLookupMap.Find(assetGuid);
			if (it != m_assetIdentifierLookupMap.end())
			{
				return it->second;
			}
			return {};
		}

		[[nodiscard]] Guid GetAssetTypeGuid(const Guid assetGuid) const
		{
			Threading::SharedLock lock(m_assetDatabaseMutex);
			if (const Optional<const DatabaseEntry*> pEntry = Database::GetAssetEntry(assetGuid))
			{
				return pEntry->m_assetTypeGuid;
			}
			else
			{
				return {};
			}
		}

		[[nodiscard]] Guid GetAssetComponentTypeGuid(const Guid assetGuid) const
		{
			Threading::SharedLock lock(m_assetDatabaseMutex);
			if (const Optional<const DatabaseEntry*> pEntry = Database::GetAssetEntry(assetGuid))
			{
				return pEntry->m_componentTypeGuid;
			}
			else
			{
				return {};
			}
		}

		[[nodiscard]] IO::Path GetAssetPath(const Guid assetGuid) const
		{
			Threading::SharedLock lock(m_assetDatabaseMutex);
			if (const Optional<const DatabaseEntry*> pEntry = Database::GetAssetEntry(assetGuid))
			{
				return pEntry->m_path;
			}
			else
			{
				return {};
			}
		}

		[[nodiscard]] IO::Path GetAssetPath(const Identifier assetIdentifier) const
		{
			return GetAssetPath(GetAssetGuid(assetIdentifier));
		}

		[[nodiscard]] IO::Path GetAssetBinaryPath(const Guid assetGuid) const
		{
			Threading::SharedLock lock(m_assetDatabaseMutex);
			if (const Optional<const DatabaseEntry*> pEntry = Database::GetAssetEntry(assetGuid))
			{
				return IO::Path(pEntry->GetBinaryFilePath());
			}
			else
			{
				return {};
			}
		}

		[[nodiscard]] IO::Path::StringType GetAssetName(const Guid assetGuid) const
		{
			Threading::SharedLock lock(m_assetDatabaseMutex);
			if (const Optional<const DatabaseEntry*> pEntry = Database::GetAssetEntry(assetGuid))
			{
				return pEntry->GetName();
			}
			else
			{
				return {};
			}
		}

		[[nodiscard]] IO::Path::StringType GetAssetNameFromPath(const Guid assetGuid) const
		{
			Threading::SharedLock lock(m_assetDatabaseMutex);
			if (const Optional<const DatabaseEntry*> pEntry = Database::GetAssetEntry(assetGuid))
			{
				return pEntry->GetNameFromPath();
			}
			else
			{
				return {};
			}
		}

		template<typename Callback>
		auto VisitAssetEntry(const Guid assetGuid, Callback&& callback) const
		{
			Threading::SharedLock lock(m_assetDatabaseMutex);
			if (const Optional<const DatabaseEntry*> pEntry = Database::GetAssetEntry(assetGuid))
			{
				return callback(pEntry);
			}
			else
			{
				return callback(Invalid);
			}
		}

		template<typename Callback>
		auto VisitAssetEntry(const Guid assetGuid, Callback&& callback)
		{
			Threading::SharedLock lock(m_assetDatabaseMutex);
			if (const Optional<DatabaseEntry*> pEntry = Database::GetAssetEntry(assetGuid))
			{
				return callback(pEntry);
			}
			else
			{
				return callback(Invalid);
			}
		}

		[[nodiscard]] bool HasAsset(const Guid assetGuid) const
		{
			Threading::SharedLock lock(m_assetDatabaseMutex);
			return Database::HasAsset(assetGuid);
		}

		void Reserve(const uint32 count);

		Identifier RegisterAsset(
			const Guid assetGuid,
			DatabaseEntry&& entry,
			const Identifier rootFolderAssetIdentifier,
			const ArrayView<const Tag::Identifier, uint8> tagIdentifiers = {}
		);
		Identifier FindOrRegisterFolder(const IO::PathView folderPath, const Identifier rootFolderAssetIdentifier);
		void RemoveAsset(const Guid assetGuid);

		template<typename Callback>
		void IterateAssetsOfAssetType(Callback&& callback, const ArrayView<const TypeGuid> typeGuids)
		{
			Threading::SharedLock lock(m_assetDatabaseMutex);
			Database::IterateAssetsOfAssetType<Callback>(Forward<Callback>(callback), typeGuids);
		}

		template<typename Callback>
		void IterateAssetsOfAssetType(Callback&& callback, const TypeGuid typeGuid)
		{
			Threading::SharedLock lock(m_assetDatabaseMutex);
			Database::IterateAssetsOfAssetType<Callback>(Forward<Callback>(callback), typeGuid);
		}

		//! Appends the specified tag to all assets in the mask
		void SetTagAssets(const Tag::Identifier tag, const Mask& mask)
		{
			m_tags.Set(tag, mask);
		}
		//! Appends the specified tag to the specified asset
		void SetTagAsset(const Tag::Identifier tag, const Identifier assetIdentifier)
		{
			m_tags.Set(tag, assetIdentifier);
		}
		//! Clears the specified tag for all assets in the mask
		void ClearTagAssets(const Tag::Identifier tag, const Mask& mask)
		{
			m_tags.Clear(tag, mask);
		}
		//! Clears the specified tag for the specified asset
		void ClearTagAsset(const Tag::Identifier tag, const Identifier assetIdentifier)
		{
			m_tags.Clear(tag, assetIdentifier);
		}
		//! Checks whether the specified tag is set on the asset
		[[nodiscard]] bool IsTagSet(const Tag::Identifier tag, const Identifier assetIdentifier) const
		{
			return m_tags.IsSet(tag, assetIdentifier);
		}
		[[nodiscard]] bool HasAnyAssetsWithTag(const Tag::Identifier tag) const
		{
			return m_tags.AreAnySet(tag);
		}

		Event<void(void*, const Mask& assetsMask), 24> OnAssetsAdded;
		Event<void(void*, const Mask& assetsMask), 24> OnAssetsRemoved;

		// DataSource::Interface
		virtual void LockRead() override;
		virtual void UnlockRead() override;

		virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const override final;
		[[nodiscard]] virtual bool SortQuery(
			const CachedQuery& cachedQuery,
			const PropertyIdentifier filteredPropertyIdentifier,
			const SortingOrder order,
			SortedQueryIndices& cachedSortedQueryOut
		) override final;
		[[nodiscard]] virtual GenericDataIndex GetDataCount() const override final
		{
			return m_assetCount;
		}
		virtual void IterateData(
			const CachedQuery& query,
			IterationCallback&& callback,
			const Math::Range<GenericDataIndex> offset =
				Math::Range<GenericDataIndex>::MakeStartToEnd(0u, Math::NumericLimits<GenericDataIndex>::Max - 1u)
		) const override final;
		virtual void IterateData(
			const SortedQueryIndices& query,
			IterationCallback&& callback,
			const Math::Range<GenericDataIndex> offset =
				Math::Range<GenericDataIndex>::MakeStartToEnd(0u, Math::NumericLimits<GenericDataIndex>::Max - 1u)
		) const override final;
		virtual PropertyValue GetDataProperty(const Data data, const PropertyIdentifier identifier) const override final;

		using Interface::OnDataChanged;
		// ~DataSource::Interface

		using ExtensionCallback = Function<PropertyValue(const Identifier), 24>;
		void RegisterDataPropertyCallback(const DataSource::PropertyIdentifier identifier, ExtensionCallback&& callback);

		using SortingCallback =
			Function<void(const CachedQuery& cachedQuery, const SortingOrder order, SortedQueryIndices& cachedSortedQueryOut), 24>;
		void RegisterDataPropertySortingCallback(const DataSource::PropertyIdentifier identifier, SortingCallback&& callback);

		using Database::HasAsset;
		[[nodiscard]] bool HasAsset(const IO::PathView path) const
		{
			Threading::SharedLock lock(m_identifierLookupMapMutex);
			return m_identifierLookupMap.Contains(path);
		}

		[[nodiscard]] Identifier GetAssetIdentifier(const IO::PathView path) const
		{
			Threading::SharedLock lock(m_identifierLookupMapMutex);
			decltype(m_identifierLookupMap)::const_iterator it = m_identifierLookupMap.Find(path);
			if (it == m_identifierLookupMap.end())
			{
				return Identifier();
			}
			return it->second;
		}
		[[nodiscard]] Guid GetAssetGuid(const IO::PathView path) const
		{
			const Identifier identifier = GetAssetIdentifier(path);
			if (identifier.IsValid())
			{
				return GetAssetGuid(identifier);
			}
			else
			{
				return {};
			}
		}

		[[nodiscard]] Identifier GetAssetParentIdentifier(const Identifier childIdentifier) const
		{
			return Identifier::MakeFromIndex(m_assetParentIndices[childIdentifier]);
		}
		[[nodiscard]] Identifier GetAssetRootParentIdentifier(const Identifier childIdentifier) const
		{
			Identifier parentIdentifier{childIdentifier};
			while (m_assetParentIndices[parentIdentifier] != 0)
			{
				parentIdentifier = Identifier::MakeFromIndex(m_assetParentIndices[parentIdentifier]);
			}
			return parentIdentifier;
		}

		template<typename Callback>
		void IterateAssetChildren(const Identifier identifier, Callback&& callback)
		{
			for (const Identifier::IndexType& assetParentIndex : m_assetIdentifiers.GetValidElementView(m_assetParentIndices.GetView()))
			{
				if (assetParentIndex == identifier.GetIndex())
				{
					callback(Identifier::MakeFromValidIndex(m_assetParentIndices.GetView().GetIteratorIndex(&assetParentIndex)));
				}
			}
		}
	protected:
		Identifier RegisterAssetInternal(
			const Guid assetGuid,
			DatabaseEntry&& entry,
			const Identifier rootFolderAssetIdentifier,
			const ArrayView<const Tag::Identifier, uint8> tagIdentifiers = {}
		);

		Identifier FindOrRegisterFolderInternal(const IO::PathView folderPath, const Identifier rootFolderAssetIdentifier);

		void RegisterAssetFolders(
			const Identifier assetIdentifier,
			const IO::PathView assetPath,
			const Identifier rootFolderAssetIdentifier,
			const ArrayView<const Tag::Identifier, uint8> tagIdentifiers = {}
		);
	protected:
		mutable Threading::SharedMutex m_assetDatabaseMutex;
		Threading::Atomic<GenericDataIndex> m_assetCount = 0;

		mutable Threading::SharedMutex m_assetIdentifierLookupMapMutex;
		UnorderedMap<Guid, Identifier, Guid::Hash> m_assetIdentifierLookupMap;
		Threading::Atomic<uint32> m_requestedAssetCapacity{0};

		TSaltedIdentifierStorage<Identifier> m_assetIdentifiers;

		using AssetContainer = TIdentifierArray<Guid, Identifier>;
		using TagContainer = Tag::AtomicMaskContainer<Identifier>;

		AssetContainer m_assets{Memory::Zeroed};
		TagContainer m_tags;

		mutable Threading::SharedMutex m_identifierLookupMapMutex;
		UnorderedMap<IO::Path, Identifier, IO::Path::Hash> m_identifierLookupMap;
		//! Array containing the parent index of each asset, relating to the folder / path hierarchy
		TIdentifierArray<Identifier::IndexType, Identifier> m_assetParentIndices{Memory::Zeroed};

		UnorderedMap<DataSource::PropertyIdentifier, ExtensionCallback, DataSource::PropertyIdentifier::Hash> m_propertyExtensions;
		UnorderedMap<DataSource::PropertyIdentifier, SortingCallback, DataSource::PropertyIdentifier::Hash> m_propertySortingExtensions;
	};
}
