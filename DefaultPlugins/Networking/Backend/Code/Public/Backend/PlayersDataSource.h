#pragma once

#include <Backend/PersistentUserIdentifier.h>
#include <Backend/PerSessionUserIdentifier.h>

#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/DataSourcePropertyIdentifier.h>
#include <Engine/Entity/ComponentMask.h>
#include <Engine/DataSource/DataSourceCache.h>

#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Asset/Guid.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Any.h>
#include <Common/Tag/TagGuid.h>
#include <Common/Time/Timestamp.h>

namespace ngine
{
	namespace DataSource
	{
		struct Cache;
	}
}

namespace ngine::Networking::Backend
{
	struct Game;

	using UserMask = IdentifierMask<PerSessionUserIdentifier>;

	namespace Tags
	{
		//! Template provided by other users, can either be played directly or remixed after cloning
		inline static constexpr Guid ProjectTemplateTagGuid = "fa104e6b-1155-490d-acf2-055503f26a83"_tag;
		//! Local project that can be edited in place, or played (with the option to continue editing)
		inline static constexpr Guid EditableLocalProjectTagGuid = "99aca0e9-ef55-405d-8988-94165b912a08"_tag;
		//! Local project that can be played in place, or remixed if after cloning
		inline static constexpr Guid PlayableLocalProjectTagGuid = "88079d24-b66b-4a0f-aed7-6699dafb1e91"_tag;
	}
}

namespace ngine::Networking::Backend::DataSource
{
	using namespace ngine::DataSource;

	struct Players final : public DataSource::Interface
	{
		struct PlayerInfo
		{
			PersistentUserIdentifier internalIdentifier;
			PerSessionUserIdentifier identifier;

			UnicodeString username;
			UnicodeString description;
			Asset::Guid thumbnailGuid;
			Asset::Guid thumbnailEditGuid;
			Time::Timestamp accountCreationDate;

			uint64 followerCount = 0;
			uint64 followingCount = 0;
			uint64 gameCount = 0;
			Tag::Identifier playerTagIdentifier;
			Tag::Mask tags;

			bool followedByMainPlayer = false;
		};

		inline static constexpr Guid DataSourceGuid = "B50EA2A1-7493-4FEF-9F0B-4A287AFB3B69"_guid;

		inline static constexpr Guid LocalPlayerTagGuid = "44be35b5-5c1c-49b9-a203-784b5b5ea4c6"_guid;
		inline static constexpr Guid AdminTagGuid = "f112ea3f-2a9f-4506-aaff-aadd76a1ec4d"_guid;
		inline static constexpr Guid GameCreatorGuid = "e0832729-3be7-407f-a352-0e0d75ceef4c"_guid;
		inline static constexpr Guid AvatarAssetTag = "8d00f0ca-170e-47b9-83ae-274492f7be7e"_guid;

		Players(Game& game);
		virtual ~Players();

		// DataSource::Interface
		virtual void LockRead() override final;
		virtual void UnlockRead() override final;

		virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const override final;
		virtual bool
		SortQuery(const CachedQuery&, [[maybe_unused]] const PropertyIdentifier filteredPropertyIdentifier, const SortingOrder, SortedQueryIndices&)
			override final;
		[[nodiscard]] virtual GenericDataIndex GetDataCount() const override final;
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

		template<typename Callback>
		void VisitPlayerInfo(const Tag::Identifier tag, Callback&& callback)
		{
			Threading::SharedLock playersLock(m_playersMutex);
			for (const typename PerSessionUserIdentifier::IndexType playerIdentifierIndex : m_playerMask.GetSetBitsIterator())
			{
				const PerSessionUserIdentifier playerIdentifier = PerSessionUserIdentifier::MakeFromValidIndex(playerIdentifierIndex);

				const decltype(m_players)::const_iterator it = m_players.Find(playerIdentifier);
				if (Optional<const PlayerInfo*> pPlayerInfo = Optional<const PlayerInfo*>(&it->second, it != m_players.end()))
				{
					if (pPlayerInfo->playerTagIdentifier == tag)
					{
						callback(pPlayerInfo);
					}
				}
			}
		}

		template<typename Callback>
		auto VisitPlayerInfo(const PerSessionUserIdentifier playerIdentifier, Callback&& callback) const
		{
			Threading::SharedLock lock(m_playersMutex);
			const decltype(m_players)::const_iterator it = m_players.Find(playerIdentifier);
			return callback(Optional<const PlayerInfo*>(&it->second, it != m_players.end()));
		}

		template<typename Callback>
		auto VisitPlayerInfo(const PerSessionUserIdentifier playerIdentifier, Callback&& callback)
		{
			Threading::SharedLock lock(m_playersMutex);
			const decltype(m_players)::iterator it = m_players.Find(playerIdentifier);
			return callback(Optional<PlayerInfo*>(&it->second, it != m_players.end()));
		}

		template<typename Callback>
		auto VisitPlayerInfo(const PersistentUserIdentifier internalPlayerIdentifier, Callback&& callback) const
		{
			Threading::SharedLock lock(m_identifierLookupMapMutex);
			const auto it = m_identifierLookupMap.Find(internalPlayerIdentifier);
			if (it != m_identifierLookupMap.end())
			{
				const PerSessionUserIdentifier playerIdentifier = it->second;
				lock.Unlock();
				return VisitPlayerInfo(playerIdentifier, Forward<Callback>(callback));
			}
			else
			{
				return callback(Invalid);
			}
		}

		template<typename Callback>
		auto VisitPlayerInfo(const PersistentUserIdentifier internalPlayerIdentifier, Callback&& callback)
		{
			Threading::SharedLock lock(m_identifierLookupMapMutex);
			const auto it = m_identifierLookupMap.Find(internalPlayerIdentifier);
			if (it != m_identifierLookupMap.end())
			{
				const PerSessionUserIdentifier playerIdentifier = it->second;
				lock.Unlock();
				return VisitPlayerInfo(playerIdentifier, Forward<Callback>(callback));
			}
			else
			{
				return callback(Invalid);
			}
		}

		template<typename Callback>
		auto FindOrEmplacePlayer(const PersistentUserIdentifier internalPlayerIdentifier, Callback&& callback)
		{
			{
				Threading::SharedLock lock(m_identifierLookupMapMutex);
				const auto it = m_identifierLookupMap.Find(internalPlayerIdentifier);
				if (it != m_identifierLookupMap.end())
				{
					const PerSessionUserIdentifier playerIdentifier = it->second;
					lock.Unlock();

					Threading::SharedLock playersLock(m_playersMutex);
					auto playerIt = m_players.Find(playerIdentifier);
					Assert(playerIt != m_players.end());
					return callback(playerIt->second);
				}
			}

			Threading::UniqueLock lock(m_identifierLookupMapMutex);
			const auto it = m_identifierLookupMap.Find(internalPlayerIdentifier);
			if (it != m_identifierLookupMap.end())
			{
				const PerSessionUserIdentifier playerIdentifier = it->second;
				lock.Unlock();

				Threading::SharedLock playersLock(m_playersMutex);
				const auto playerIt = m_players.Find(playerIdentifier);
				Assert(playerIt != m_players.end());
				return callback(playerIt->second);
			}
			else
			{
				const PerSessionUserIdentifier playerIdentifier = m_playerIdentifiers.AcquireIdentifier();

				m_identifierLookupMap.Emplace(internalPlayerIdentifier, PerSessionUserIdentifier{playerIdentifier});
				m_playerMask.Set(playerIdentifier);

				Threading::UniqueLock playersLock(m_playersMutex);
				const auto playerIt = m_players.Emplace(playerIdentifier, PlayerInfo{internalPlayerIdentifier, playerIdentifier});
				return callback(playerIt->second);
			}
		}
	private:
		Players(Game& game, ngine::DataSource::Cache& dataSourceCache);
	private:
		Game& m_game;

		DataSource::PropertyIdentifier m_playerIdentifierPropertyIdentifier;
		DataSource::PropertyIdentifier m_playerGenericIdentifierPropertyIdentifier;
		DataSource::PropertyIdentifier m_playerNamePropertyIdentifier;
		DataSource::PropertyIdentifier m_playerDescriptionPropertyIdentifier;
		DataSource::PropertyIdentifier m_playerThumbnailPropertyIdentifier;
		DataSource::PropertyIdentifier m_playerThumbnailEditChoicePropertyIdentifier;
		DataSource::PropertyIdentifier m_playerFollowerCountPropertyIdentifier;
		DataSource::PropertyIdentifier m_playerFollowingCountPropertyIdentifier;
		DataSource::PropertyIdentifier m_playerGameCountPropertyIdentifier;
		DataSource::PropertyIdentifier m_playerTagPropertyIdentifier;
		DataSource::PropertyIdentifier m_playerFollowButtonTextPropertyIdentifier;
		DataSource::PropertyIdentifier m_playerCurrencyBalance;
		DataSource::PropertyIdentifier m_playerCartTotalPrice;
		DataSource::PropertyIdentifier m_playerNumberAssetsInCart;
		DataSource::PropertyIdentifier m_playerIsFollowed;
		DataSource::PropertyIdentifier m_playerIsNotFollowed;
		DataSource::PropertyIdentifier m_playerMemberSincePropertyIdentifier;

		TSaltedIdentifierStorage<PerSessionUserIdentifier> m_playerIdentifiers;
		mutable Threading::SharedMutex m_identifierLookupMapMutex;
		UnorderedMap<PersistentUserIdentifier, PerSessionUserIdentifier, PersistentUserIdentifier::Hash> m_identifierLookupMap;
		mutable Threading::SharedMutex m_playersMutex;
		UnorderedMap<PerSessionUserIdentifier, PlayerInfo, PerSessionUserIdentifier::Hash> m_players;

		Threading::AtomicIdentifierMask<PerSessionUserIdentifier> m_playerMask;
	};
}
