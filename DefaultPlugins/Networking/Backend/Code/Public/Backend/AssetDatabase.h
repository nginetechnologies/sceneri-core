#pragma once

#include "PersistentUserIdentifier.h"

#include <Common/Asset/Guid.h>
#include <Common/EnumFlagOperators.h>
#include <Common/EnumFlags.h>
#include <Common/Function/Event.h>
#include <Common/IO/URI.h>
#include <Common/Memory/CallbackResult.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/Time/Timestamp.h>

namespace ngine::Asset
{
	struct Manager;
}

namespace ngine::Networking::Backend
{
	struct Game;
	using AssetIdentifier = uint32;

	enum class AssetFlags : uint8
	{
		Active = 1 << 0,
		//! Whether this asset has been synchronized and is up to date with backend data as of this session
		//! Missing this flag indicates that we might have to re-request backend data to use this asset.
		UpToDate = 1 << 1,
		Liked = 1 << 2,
		IsInCart = 1 << 3,
	};
	ENUM_FLAG_OPERATORS(AssetFlags);

	struct AssetEntry
	{
		AssetEntry(const AssetEntry&) = delete;
		AssetEntry(AssetEntry&&) = default;
		AssetEntry& operator=(const AssetEntry&) = delete;
		AssetEntry& operator=(AssetEntry&&) = default;

		using Flags = AssetFlags;

		Asset::Guid m_guid;
		uint32 m_id{0};
		AtomicEnumFlags<Flags> m_flags{Flags::Active};
		UnicodeString m_name;
		UnicodeString m_description;
		uint64 m_likesCount{0};
		uint64 m_playsCount{0};
		uint64 m_remixCount{0};
		IO::URI m_mainFileURI;
		String m_mainFileEtag;
		uint32 m_price{0};
		uint32 m_defaultVariationId{0};

		PersistentUserIdentifier m_creatorId;
		Guid m_creatorTagGuid;

		using KeyValueStorage = UnorderedMap<String, String, String::Hash>;
		KeyValueStorage m_keyValueStorage;

		InlineVector<Asset::Guid, 4> m_dependencies;

		[[nodiscard]] Optional<ConstStringView> FindStorageValue(const ConstStringView key) const
		{
			auto it = m_keyValueStorage.Find(key);
			if (it != m_keyValueStorage.end())
			{
				return it->second.GetView();
			}
			return Invalid;
		}

#if PLATFORM_APPLE_IOS || PLATFORM_APPLE_VISIONOS || PLATFORM_APPLE_MACOS
		String m_appStoreProductIdentifier;
		void* m_appleProduct = nullptr;
#endif

		String m_catalogListingId;
	};

	struct AssetVariationReference
	{
		AssetIdentifier m_assetId;
		uint32 m_variationId;
	};

	struct AssetCatalogItem
	{
		String m_catalogListingId;
		uint32 m_quantity;
	};

	struct FileURIToken
	{
		String m_token;
		Time::Timestamp m_expiresAt;
		bool Serialize(const Serialization::Reader reader);
	};

	struct AssetDatabase
	{
		AssetDatabase(Game& game);
		~AssetDatabase();

		enum class ParsingFlags : uint8
		{
			GlobalAssets = 1 << 0,
			Inventory = 1 << 1,
			//! Whether the asset data is being read from cache
			//! Otherwise we assume that we are parsing up to date backend  data
			FromCache = 1 << 2,
			Avatar = 1 << 3,
		};

		void ReserveAssets(const uint32 assetCount);
		Optional<AssetEntry*> ParseAsset(
			const Serialization::Reader reader,
			const Asset::Guid assetGuid,
			Asset::Manager& assetManager,
			const EnumFlags<ParsingFlags> parsingFlags,
			Game& gameAPI,
			Optional<Guid> tag = {}
		);
		void DeactivateAsset(const Asset::Guid assetGuid, Asset::Manager& assetManager);

		[[nodiscard]] bool HasEntry(const Guid guid) const
		{
			Threading::SharedLock lock(m_assetsMutex);
			return m_assets.Contains(guid);
		}

		[[nodiscard]] uint32 GetBackendAssetIdentifier(const Guid guid) const
		{
			Threading::SharedLock lock(m_assetsMutex);
			auto it = m_assets.Find(guid);
			if (it != m_assets.end())
			{
				return it->second.m_id;
			}
			else
			{
				return 0;
			}
		}

		template<typename Callback>
		auto VisitEntry(const Guid guid, Callback&& callback)
		{
			Threading::SharedLock lock(m_assetsMutex);
			auto it = m_assets.Find(guid);
			if (it != m_assets.end())
			{
				return callback(it->second);
			}
			else
			{
				return callback(Invalid);
			}
		}

		template<typename Callback>
		auto TryEmplaceAndVisitEntry(const Guid guid, Callback&& callback)
		{
			{
				Threading::SharedLock lock(m_assetsMutex);
				auto it = m_assets.Find(guid);
				if (it != m_assets.end())
				{
					return callback(it->second);
				}
			}

			{
				Threading::UniqueLock lock(m_assetsMutex);
				auto it = m_assets.Find(guid);
				if (it != m_assets.end())
				{
					return callback(it->second);
				}
				else
				{
					m_requestedAssetCapacity++;
					it = m_assets.Emplace(Guid(guid), AssetEntry{guid});
					return callback(it->second);
				}
			}
		}

		void MarkAllAssetsUpToDate(const Guid assetTypeGuid);

		Event<void(void*, const AssetEntry&), 24> OnAssetAdded;
	protected:
		mutable Threading::SharedMutex m_assetsMutex;
		uint32 m_requestedAssetCapacity{0};
		UnorderedMap<Guid, AssetEntry, Guid::Hash> m_assets;
	};

	ENUM_FLAG_OPERATORS(AssetDatabase::ParsingFlags);
} // namespace ngine::Networking::Backend
