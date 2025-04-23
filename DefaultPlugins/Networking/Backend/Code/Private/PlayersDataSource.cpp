#include "PlayersDataSource.h"
#include "Game.h"

#include <Common/System/Query.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertyValue.h>
#include <Engine/Tag/TagRegistry.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Variant.h>
#include <Common/Asset/Guid.h>

#include <Backend/Plugin.h>

namespace ngine::Networking::Backend::DataSource
{
	Players::Players(Game& game, ngine::DataSource::Cache& dataSourceCache)
		: Interface(dataSourceCache.FindOrRegister(DataSourceGuid))
		, m_game(game)
		, m_playerIdentifierPropertyIdentifier(dataSourceCache.RegisterProperty("player_id"))
		, m_playerGenericIdentifierPropertyIdentifier(dataSourceCache.RegisterProperty("player_generic_id"))
		, m_playerNamePropertyIdentifier(dataSourceCache.RegisterProperty("player_name"))
		, m_playerDescriptionPropertyIdentifier(dataSourceCache.RegisterProperty("player_description"))
		, m_playerThumbnailPropertyIdentifier(dataSourceCache.RegisterProperty("player_thumbnail_guid"))
		, m_playerThumbnailEditChoicePropertyIdentifier(dataSourceCache.RegisterProperty("player_thumbnail_edit_guid"))
		, m_playerFollowerCountPropertyIdentifier(dataSourceCache.RegisterProperty("player_follower_count"))
		, m_playerFollowingCountPropertyIdentifier(dataSourceCache.RegisterProperty("player_following_count"))
		, m_playerGameCountPropertyIdentifier(dataSourceCache.RegisterProperty("player_game_count"))
		, m_playerTagPropertyIdentifier(dataSourceCache.RegisterProperty("player_tag"))
		, m_playerFollowButtonTextPropertyIdentifier(dataSourceCache.RegisterProperty("player_follow_button_text"))
		, m_playerCurrencyBalance(dataSourceCache.RegisterProperty("player_currency_balance"))
		, m_playerCartTotalPrice(dataSourceCache.RegisterProperty("player_cart_total_price"))
		, m_playerNumberAssetsInCart(dataSourceCache.RegisterProperty("player_number_assets_in_cart"))
		, m_playerIsFollowed(dataSourceCache.RegisterProperty("player_followed"))
		, m_playerIsNotFollowed(dataSourceCache.RegisterProperty("player_not_followed"))
		, m_playerMemberSincePropertyIdentifier(dataSourceCache.RegisterProperty("player_member_since"))
	{
		dataSourceCache.OnCreated(m_identifier, *this);
	}

	Players::Players(Game& game)
		: Players(game, System::Get<ngine::DataSource::Cache>())
	{
	}

	Players::~Players()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.Deregister(dataSourceCache.Find(DataSourceGuid), dataSourceCache.FindGuid(m_identifier));

		dataSourceCache.DeregisterProperty(m_playerIdentifierPropertyIdentifier, "player_id");
		dataSourceCache.DeregisterProperty(m_playerGenericIdentifierPropertyIdentifier, "player_generic_id");
		dataSourceCache.DeregisterProperty(m_playerNamePropertyIdentifier, "player_name");
		dataSourceCache.DeregisterProperty(m_playerDescriptionPropertyIdentifier, "player_description");
		dataSourceCache.DeregisterProperty(m_playerThumbnailPropertyIdentifier, "player_thumbnail_guid");
		dataSourceCache.DeregisterProperty(m_playerThumbnailEditChoicePropertyIdentifier, "player_thumbnail_edit_guid");
		dataSourceCache.DeregisterProperty(m_playerFollowerCountPropertyIdentifier, "player_follower_count");
		dataSourceCache.DeregisterProperty(m_playerFollowingCountPropertyIdentifier, "player_following_count");
		dataSourceCache.DeregisterProperty(m_playerGameCountPropertyIdentifier, "player_game_count");
		dataSourceCache.DeregisterProperty(m_playerTagPropertyIdentifier, "player_tag");
		dataSourceCache.DeregisterProperty(m_playerFollowButtonTextPropertyIdentifier, "player_follow_button_text");
		dataSourceCache.DeregisterProperty(m_playerCurrencyBalance, "player_currency_balance");
		dataSourceCache.DeregisterProperty(m_playerCartTotalPrice, "player_cart_total_price");
		dataSourceCache.DeregisterProperty(m_playerNumberAssetsInCart, "player_number_assets_in_cart");
		dataSourceCache.DeregisterProperty(m_playerIsFollowed, "player_followed");
		dataSourceCache.DeregisterProperty(m_playerIsNotFollowed, "player_not_followed");
		dataSourceCache.DeregisterProperty(m_playerMemberSincePropertyIdentifier, "player_member_since");
	}

	void Players::LockRead()
	{
		[[maybe_unused]] const bool wasLocked = m_playersMutex.LockShared();
		Assert(wasLocked);
	}

	void Players::UnlockRead()
	{
		m_playersMutex.UnlockShared();
	}

	static_assert(DataSource::GenericDataIdentifier::MaximumCount >= PerSessionUserIdentifier::MaximumCount);
	DataSource::GenericDataIndex Players::GetDataCount() const
	{
		Threading::SharedLock lock(m_playersMutex);
		return (DataSource::GenericDataIndex)m_players.GetSize();
	}

	void Players::IterateData(const CachedQuery& cachedQuery, IterationCallback&& callback, const Math::Range<GenericDataIndex> offset) const
	{
		const UserMask& selectedPlayers = reinterpret_cast<const UserMask&>(cachedQuery);

		const UserMask::SetBitsIterator iterator = selectedPlayers.GetSetBitsIterator();
		const UserMask::SetBitsIterator::Iterator begin = iterator.begin() + (uint16)offset.GetMinimum();
		const UserMask::SetBitsIterator::Iterator end =
			Math::Min(iterator.end(), iterator.begin() + (uint16)offset.GetMinimum() + (uint16)offset.GetSize());

		for (UserMask::SetBitsIterator::Iterator it = begin; it != end; ++it)
		{
			const PerSessionUserIdentifier::IndexType playerIdentifierIndex = *it;
			const PerSessionUserIdentifier playerIdentifier = PerSessionUserIdentifier::MakeFromValidIndex(playerIdentifierIndex);

			if (auto playerIt = m_players.Find(playerIdentifier); playerIt != m_players.end())
			{
				callback(playerIt->second);
			}
		}
	}

	void Players::IterateData(const SortedQueryIndices& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset) const
	{
		for (const Identifier::IndexType identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			const PerSessionUserIdentifier playerIdentifier = PerSessionUserIdentifier::MakeFromValidIndex(identifierIndex);
			if (auto playerIt = m_players.Find(playerIdentifier); playerIt != m_players.end())
			{
				callback(playerIt->second);
			}
		}
	}

	DataSource::PropertyValue Players::GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const
	{
		const PlayerInfo& playerInfo = data.GetExpected<PlayerInfo>();
		if (identifier == m_playerIdentifierPropertyIdentifier)
		{
			return playerInfo.identifier;
		}
		else if (identifier == m_playerGenericIdentifierPropertyIdentifier)
		{
			const PerSessionUserIdentifier::IndexType index = playerInfo.identifier.GetFirstValidIndex();
			return DataSource::GenericDataIdentifier::MakeFromValidIndex(index);
		}
		else if (identifier == m_playerNamePropertyIdentifier)
		{
			return playerInfo.username;
		}
		else if (identifier == m_playerDescriptionPropertyIdentifier)
		{
			return playerInfo.description;
		}
		else if (identifier == m_playerThumbnailPropertyIdentifier)
		{
			return playerInfo.thumbnailGuid;
		}
		else if (identifier == m_playerThumbnailEditChoicePropertyIdentifier)
		{
			return playerInfo.thumbnailEditGuid;
		}
		else if (identifier == m_playerFollowerCountPropertyIdentifier)
		{
			return UnicodeString().Format("{}", playerInfo.followerCount);
		}
		else if (identifier == m_playerFollowingCountPropertyIdentifier)
		{
			return UnicodeString().Format("{}", playerInfo.followingCount);
		}
		else if (identifier == m_playerIsFollowed)
		{
			return playerInfo.followedByMainPlayer;
		}
		else if (identifier == m_playerIsNotFollowed)
		{
			return !playerInfo.followedByMainPlayer;
		}
		else if (identifier == m_playerGameCountPropertyIdentifier)
		{
			return UnicodeString().Format("{}", playerInfo.gameCount);
		}
		else if (identifier == m_playerTagPropertyIdentifier)
		{
			return playerInfo.playerTagIdentifier;
		}
		else if (identifier == m_playerFollowButtonTextPropertyIdentifier)
		{
			if (playerInfo.identifier == m_game.GetLocalPlayerIdentifier())
			{
				return UnicodeString("Edit");
			}
			else if (playerInfo.followedByMainPlayer)
			{
				return UnicodeString("Unfollow");
			}
			else
			{
				return UnicodeString("Follow");
			}
		}
		else if (identifier == m_playerCurrencyBalance)
		{
			return UnicodeString().Format("{}", m_game.GetCurrency().GetBalance());
		}
		else if (identifier == m_playerCartTotalPrice)
		{
			return UnicodeString().Format("{}", m_game.GetCurrency().GetCartTotalPrice());
		}
		else if (identifier == m_playerNumberAssetsInCart)
		{
			return UnicodeString().Format("{}", m_game.GetCurrency().GetNumberAssetsInCart());
		}
		else if (identifier == m_playerMemberSincePropertyIdentifier)
		{
			return UnicodeString(playerInfo.accountCreationDate.Format("{:%B %Y}").GetView());
		}

		return {};
	}

	void Players::CacheQuery(const Query& query, CachedQuery& __restrict cachedQueryOut) const
	{
		UserMask& __restrict selectedPlayers = reinterpret_cast<UserMask&>(cachedQueryOut);
		selectedPlayers.ClearAll();

		if (query.m_allowedItems.IsValid())
		{
			selectedPlayers = UserMask(*query.m_allowedItems);
		}

		if (query.m_allowedFilterMask.AreAnySet())
		{
			UserMask allowedFilterMask;
			for (const typename PerSessionUserIdentifier::IndexType playerIdentifierIndex : m_playerMask.GetSetBitsIterator())
			{
				const PerSessionUserIdentifier playerIdentifier = PerSessionUserIdentifier::MakeFromValidIndex(playerIdentifierIndex);

				if (auto playerIt = m_players.Find(playerIdentifier); playerIt != m_players.end())
				{
					if (playerIt->second.tags.AreAnySet(query.m_allowedFilterMask))
					{
						allowedFilterMask.Set(playerIdentifier);
					}
				}
			}

			if (!query.m_allowedItems.IsValid())
			{
				selectedPlayers |= allowedFilterMask;
			}
			else
			{
				selectedPlayers &= allowedFilterMask;
			}
		}

		// Default to showing all entries if no allowed masks were set
		if (!query.m_allowedItems.IsValid() && query.m_allowedFilterMask.AreNoneSet())
		{
			selectedPlayers |= m_playerMask;
		}

		const bool hasRequired = query.m_requiredFilterMask.AreAnySet();
		const bool hasDisallowed = query.m_requiredFilterMask.AreAnySet();
		for (const typename PerSessionUserIdentifier::IndexType playerIdentifierIndex : m_playerMask.GetSetBitsIterator())
		{
			const PerSessionUserIdentifier playerIdentifier = PerSessionUserIdentifier::MakeFromValidIndex(playerIdentifierIndex);

			if (auto playerIt = m_players.Find(playerIdentifier); playerIt != m_players.end())
			{
				if (hasDisallowed && playerIt->second.tags.AreAnySet(query.m_disallowedFilterMask))
				{
					selectedPlayers.Clear(playerIdentifier);
				}
				if (hasRequired && !playerIt->second.tags.AreAllSet(query.m_requiredFilterMask))
				{
					selectedPlayers.Clear(playerIdentifier);
				}
			}
		}
	}

	bool Players::SortQuery(
		const CachedQuery& cachedQuery,
		const PropertyIdentifier filteredPropertyIdentifier,
		const SortingOrder order,
		SortedQueryIndices& cachedSortedQueryOut
	)
	{
		if (filteredPropertyIdentifier == m_playerFollowerCountPropertyIdentifier)
		{
			const UserMask& __restrict playerMask = reinterpret_cast<const UserMask&>(cachedQuery);
			cachedSortedQueryOut.Clear();
			cachedSortedQueryOut.Reserve(playerMask.GetNumberOfSetBits());

			for (PerSessionUserIdentifier::IndexType playerIdentifierIndex : playerMask.GetSetBitsIterator())
			{
				cachedSortedQueryOut.EmplaceBack(playerIdentifierIndex);
			}

			Algorithms::Sort(
				(PerSessionUserIdentifier::IndexType*)cachedSortedQueryOut.begin(),
				(PerSessionUserIdentifier::IndexType*)cachedSortedQueryOut.end(),
				[this,
			   order,
			   playersEnd = m_players.end()](const Asset::Identifier::IndexType leftIndex, const Asset::Identifier::IndexType rightIndex)
				{
					const PerSessionUserIdentifier leftPlayerIdentifier = PerSessionUserIdentifier::MakeFromValidIndex(leftIndex);
					const PerSessionUserIdentifier rightPlayerIdentifier = PerSessionUserIdentifier::MakeFromValidIndex(rightIndex);

					const decltype(m_players)::iterator leftPlayerIt = m_players.Find(leftPlayerIdentifier);
					const decltype(m_players)::iterator rightPlayerIt = m_players.Find(rightPlayerIdentifier);

					const Optional<const PlayerInfo*> pLeftPlayerInfo = Optional<PlayerInfo*>(&leftPlayerIt->second, leftPlayerIt != playersEnd);
					const Optional<const PlayerInfo*> pRightPlayerInfo = Optional<PlayerInfo*>(&rightPlayerIt->second, rightPlayerIt != playersEnd);

					const uint64 leftCount = pLeftPlayerInfo.IsValid() ? pLeftPlayerInfo->followerCount : 0;
					const uint64 rightCount = pRightPlayerInfo.IsValid() ? pRightPlayerInfo->followerCount : 0;

					if (order == DataSource::SortingOrder::Descending)
					{
						return leftCount < rightCount;
					}
					else
					{
						return rightCount < leftCount;
					}
				}
			);
			return true;
		}

		return false;
	}

	// TODO: Trending tag
}
