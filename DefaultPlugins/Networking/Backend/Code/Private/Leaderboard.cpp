#include "Leaderboard.h"

#include <Common/Serialization/Writer.h>

#include <Common/IO/URI.h>
#include <Common/System/Query.h>

#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/DataSource/PropertyValue.h>

#include <Backend/Plugin.h>
#include <Http/ResponseCode.h>

#include <Common/Time/Formatter.h>
#include <Common/Memory/Any.h>
#include <Common/Serialization/Guid.h>
#include <Common/Reflection/GenericType.h>
#include <Common/IO/Log.h>

namespace ngine::Networking::Backend
{
	Leaderboard::Leaderboard(Backend::Plugin& plugin, const Guid guid, DataSource::Cache& dataSourceCache, const EnumFlags<Flags> flags)
		: Interface(dataSourceCache.FindOrRegister(/*guid*/ "fae3a490-8f9a-4410-b6c2-8cf6fafd3fdb"_guid))
		, m_guid(guid)
		, m_backend(plugin)
		, m_entryRankPropertyIdentifier(dataSourceCache.FindOrRegisterPropertyIdentifier("leaderboard_entry_rank"))
		, m_entryNamePropertyIdentifier(dataSourceCache.FindOrRegisterPropertyIdentifier("leaderboard_entry_name"))
		, m_entryThumbnailPropertyIdentifier(dataSourceCache.FindOrRegisterPropertyIdentifier("leaderboard_entry_thumbnail_guid"))
		, m_entryScorePropertyIdentifier(dataSourceCache.FindOrRegisterPropertyIdentifier("leaderboard_entry_score"))
		, m_entryTimePropertyIdentifier(dataSourceCache.FindOrRegisterPropertyIdentifier("leaderboard_entry_time"))
		, m_flags(flags)
	{
		dataSourceCache.OnCreated(m_identifier, *this);
	}

	Leaderboard::Leaderboard(Backend::Plugin& plugin, const Guid guid, const EnumFlags<Flags> flags)
		: Leaderboard(plugin, guid, System::Get<DataSource::Cache>(), flags)
	{
	}

	Leaderboard::~Leaderboard()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.Deregister(m_identifier, dataSourceCache.FindGuid(m_identifier));
		/*dataSourceCache.DeregisterProperty(m_entryRankPropertyIdentifier, "leaderboard_entry_rank");
		dataSourceCache.DeregisterProperty(m_entryNamePropertyIdentifier, "leaderboard_entry_name");
		  dataSourceCache.DeregisterProperty(m_entryThumbnailPropertyIdentifier, "leaderboard_entry_thumbnail_guid");
		dataSourceCache.DeregisterProperty(m_entryScorePropertyIdentifier, "leaderboard_entry_score");
		dataSourceCache.DeregisterProperty(m_entryTimePropertyIdentifier, "leaderboard_entry_time");*/
	}

	void Leaderboard::SubmitScore(uint64 score, const ConstStringView metadata)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);
		const PersistentUserIdentifier playerIdentifier = m_backend.GetGame().GetLocalPlayerInternalIdentifier();
		writer.Serialize("member_id", String().Format("{}", playerIdentifier.Get()));
		writer.Serialize("score", score);
		if (metadata.HasElements())
		{
			writer.Serialize("metadata", metadata);
		}

		const String url = String().Format("server/leaderboards/{}/submit", m_guid.ToString());
		const ConstStringView urlView = url.GetView();
		const IO::URI uri = IO::URI(IO::URI::StringType(urlView));

		m_backend.QueueServerRequest(
			HTTP::RequestType::Post,
			uri,
			writer.SaveToBuffer<String>(),
			[this](
				[[maybe_unused]] const bool success,
				[[maybe_unused]] const HTTP::ResponseCode responseCode,
				[[maybe_unused]] const Serialization::Reader responseReader
			) mutable
			{
				if (success)
				{
					Entry entry = ParseEntry(responseReader);
					bool changed{false};
					{
						Threading::UniqueLock lock(m_entryMutex);
						changed = EmplaceEntry(Move(entry));
					}

					if (changed)
					{
						OnDataChanged();
					}
				}
				else
				{
					LogError("Score submission failed!");
				}
			}
		);
	}

	void Leaderboard::SubmitScoreOrCreate(uint64 score, const ConstStringView metadata)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);
		const PersistentUserIdentifier playerIdentifier = m_backend.GetGame().GetLocalPlayerInternalIdentifier();
		writer.Serialize("member_id", String().Format("{}", playerIdentifier.Get()));
		writer.Serialize("score", score);
		if (metadata.HasElements())
		{
			writer.Serialize("metadata", metadata);
		}

		const String url = String().Format("server/leaderboards/{}/submit", m_guid.ToString());
		const ConstStringView urlView = url.GetView();
		const IO::URI uri = IO::URI(IO::URI::StringType(urlView));

		m_backend.QueueServerRequest(
			HTTP::RequestType::Post,
			uri,
			writer.SaveToBuffer<String>(),
			[this, playerIdentifier, score, metadata = String(metadata)](
				[[maybe_unused]] const bool success,
				[[maybe_unused]] const HTTP::ResponseCode responseCode,
				[[maybe_unused]] const Serialization::Reader responseReader
			) mutable
			{
				if (success)
				{
					Entry entry = ParseEntry(responseReader);
					entry.m_identifier = playerIdentifier;
					bool changed{false};
					{
						Threading::UniqueLock lock(m_entryMutex);
						changed = EmplaceEntry(Move(entry));
					}

					if (changed)
					{
						OnDataChanged();
					}
				}
				else if (responseCode == HTTP::ResponseCodeType::NotFound)
				{
					Assert(metadata.IsEmpty() || m_flags.IsSet(Flags::HasMetadata));
					Create(
						String{},
						[this, playerIdentifier, score, metadata = Move(metadata)]([[maybe_unused]] const bool success)
						{
							if (success)
							{
								// Try submitting score again
								Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
								Serialization::Writer writer(serializedData);
								writer.Serialize("member_id", String().Format("{}", playerIdentifier.Get()));
								writer.Serialize("score", score);
								if (metadata.HasElements())
								{
									writer.Serialize("metadata", metadata);
								}

								const String url = String().Format("server/leaderboards/{}/submit", m_guid.ToString());
								const ConstStringView urlView = url.GetView();
								const IO::URI uri = IO::URI(IO::URI::StringType(urlView));

								m_backend.QueueServerRequest(
									HTTP::RequestType::Post,
									uri,
									writer.SaveToBuffer<String>(),
									[](
										[[maybe_unused]] const bool success,
										[[maybe_unused]] const HTTP::ResponseCode responseCode,
										[[maybe_unused]] const Serialization::Reader responseReader
									)
									{
									}
								);
							}
						}
					);
				}
				else
				{
					LogError("Score submission failed!");
				}
			}
		);
	}

	void Leaderboard::Create(const ConstStringView name, Callback&& callback)
	{
		const EnumFlags<Flags> flags = m_flags;

		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);
		writer.Serialize("name", name);
		writer.Serialize("key", m_guid.ToString());
		writer.Serialize("type", flags.IsSet(Flags::Generic) ? ConstStringView{"generic"} : ConstStringView{"player"});
		writer.Serialize("direction_method", flags.IsSet(Flags::Ascending) ? ConstStringView{"ascending"} : ConstStringView{"descending"});
		writer.Serialize("enable_game_api_writes", flags.IsSet(Leaderboard::Flags::EnableGameAPIWrites));
		writer.Serialize("overwrite_score_on_submit", flags.IsSet(Leaderboard::Flags::OverwriteScoreOnSubmit));
		writer.Serialize("has_metadata", flags.IsSet(Leaderboard::Flags::HasMetadata));

		Optional<Backend::Plugin*> pBackend = System::FindPlugin<Backend::Plugin>();
		Assert(pBackend.IsValid());
		if (LIKELY(pBackend.IsValid()))
		{
			pBackend->QueueServerRequest(
				HTTP::RequestType::Post,
				MAKE_URI("server/leaderboards"),
				writer.SaveToBuffer<String>(),
				[this,
			   callback = Move(callback
			   )](const bool success, [[maybe_unused]] const HTTP::ResponseCode responseCode, const Serialization::Reader responseReader)
				{
					if (success)
					{
						m_backendIdentifier = *responseReader.Read<uint64>("id");
						callback(true);
					}
					else
					{
						String responseData = responseReader.GetValue().SaveToReadableBuffer<String>();
						LogError("Couldn't create leaderboard! Response code {} Data {}", responseCode.m_code, responseData);
						callback(false);
					}
				}
			);
		}
	}

	Leaderboard::Entry Leaderboard::ParseEntry(const Serialization::Reader reader)
	{
		Entry entry{*reader.Read<uint64>("rank"), *reader.Read<uint64>("score"), reader.ReadWithDefaultValue<String>("metadata", {})};

		if (const Optional<Serialization::Reader> playerReader = reader.FindSerializer("player"))
		{
			const PersistentUserIdentifier playerIdentifier{*playerReader->Read<uint64>("id")};
			entry.m_identifier = playerIdentifier;

			Backend::Plugin& backend = *System::FindPlugin<Backend::Plugin>();
			Backend::DataSource::Players& playersDataSource = backend.GetGame().GetPlayersDataSource();

			playersDataSource.FindOrEmplacePlayer(
				playerIdentifier,
				[playerReader](Backend::DataSource::Players::PlayerInfo& playerInfo)
				{
					const UnicodeString playerName{playerReader->ReadWithDefaultValue<ConstStringView>("name", {})};
					if (playerInfo.username != playerName && playerName.HasElements())
					{
						playerInfo.username = playerName;
					}
				}
			);
		}
		else
		{
			entry.m_identifier = *reader.Read<String>("member_id");
		}

		return Move(entry);
	}

	bool Leaderboard::EmplaceEntry(Entry&& entry)
	{
		OptionalIterator<Entry> pExistingEntry = m_entries.FindIf(
			[&memberIdentifier = entry.m_identifier](const Entry& existingEntry)
			{
				return existingEntry.m_identifier == memberIdentifier;
			}
		);
		if (pExistingEntry.IsValid())
		{
			const bool changed = pExistingEntry->m_rank != entry.m_rank || pExistingEntry->m_score != entry.m_score ||
			                     pExistingEntry->m_metadata != entry.m_metadata;
			if (changed)
			{
				pExistingEntry->m_rank = entry.m_rank;
				pExistingEntry->m_score = entry.m_score;
				pExistingEntry->m_metadata = Move(entry.m_metadata);
			}
			return changed;
		}
		else
		{
			m_entries.EmplaceBack(Move(entry));
			return true;
		}
	}

	void Leaderboard::Get(const Math::Range<uint32> range, Callback&& callback)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);

		const String url =
			String().Format("server/leaderboards/{}/list?count={}&after={}", m_guid.ToString(), range.GetSize(), range.GetMinimum());
		const ConstStringView urlView = url.GetView();
		const IO::URI uri = IO::URI(IO::URI::StringType(urlView));

		Optional<Backend::Plugin*> pBackend = System::FindPlugin<Backend::Plugin>();
		Assert(pBackend.IsValid());
		if (LIKELY(pBackend.IsValid()))
		{
			pBackend->QueueServerRequest(
				HTTP::RequestType::Get,
				uri,
				{},
				[this,
			   callback = Move(callback
			   )](const bool success, [[maybe_unused]] const HTTP::ResponseCode responseCode, const Serialization::Reader responseReader)
				{
					if (success)
					{
						if (const Optional<Serialization::Reader> entriesReader = responseReader.FindSerializer("items"))
						{
							Threading::UniqueLock lock(m_entryMutex);
							m_entries.Reserve(m_entries.GetSize() + (GenericDataIndex)entriesReader->GetArraySize());

							bool changedAny{false};
							for (const Serialization::Reader entryReader : entriesReader->GetArrayView())
							{
								changedAny |= EmplaceEntry(ParseEntry(entryReader));
							}
							if (changedAny)
							{
								OnDataChanged();
							}
						}

						callback(true);
					}
					else
					{
						String responseData = responseReader.GetValue().SaveToReadableBuffer<String>();
						LogError("Couldn't get leaderboard! Response code {} Data {}", responseCode.m_code, responseData);
						callback(false);
					}
				}
			);
		}
	}

	void Leaderboard::LockRead()
	{
		[[maybe_unused]] const bool wasLocked = m_entryMutex.LockShared();
		Assert(wasLocked);
	}

	void Leaderboard::UnlockRead()
	{
		m_entryMutex.UnlockShared();
	}

	void Leaderboard::CacheQuery(const Query&, CachedQuery& cachedQueryOut) const
	{
		cachedQueryOut.ClearAll();
		for (GenericDataIndex entryIndex = 0, entryCount = m_entries.GetSize(); entryIndex < entryCount; ++entryIndex)
		{
			cachedQueryOut.Set(GenericDataIdentifier::MakeFromValidIndex(entryIndex));
		}
	}

	DataSource::GenericDataIndex Leaderboard::GetDataCount() const
	{
		return (DataSource::GenericDataIndex)m_entries.GetSize();
	}

	void
	Leaderboard::IterateData(const CachedQuery&, IterationCallback&& callback, const Math::Range<DataSource::GenericDataIndex> offset) const
	{
		for (const Entry& entry : m_entries.GetSubView(offset.GetMinimum(), offset.GetMinimum() + offset.GetSize()))
		{
			callback(entry);
		}
	}

	void Leaderboard::IterateData(const SortedQueryIndices& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset) const
	{
		for (const GenericDataIdentifier::IndexType identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			callback(m_entries[identifierIndex]);
		}
	}

	bool Leaderboard::SortQuery(
		const CachedQuery& cachedQuery,
		const PropertyIdentifier filteredPropertyIdentifier,
		const SortingOrder order,
		SortedQueryIndices& cachedSortedQueryOut
	)
	{
		if (filteredPropertyIdentifier == m_entryRankPropertyIdentifier)
		{
			cachedSortedQueryOut.Clear();
			cachedSortedQueryOut.Reserve(cachedQuery.GetNumberOfSetBits());

			for (CachedQuery::BitIndexType identifierIndex : cachedQuery.GetSetBitsIterator())
			{
				cachedSortedQueryOut.EmplaceBack(identifierIndex);
			}

			Algorithms::Sort(
				(GenericDataIndex*)cachedSortedQueryOut.begin(),
				(GenericDataIndex*)cachedSortedQueryOut.end(),
				[this, order](const GenericDataIndex leftIndex, const GenericDataIndex rightIndex)
				{
					ReferenceWrapper<const Entry> leftEntry = m_entries[leftIndex];
					ReferenceWrapper<const Entry> rightEntry = m_entries[rightIndex];

					if (order == DataSource::SortingOrder::Descending)
					{
						return leftEntry->m_rank < rightEntry->m_rank;
					}
					else
					{
						return rightEntry->m_rank < leftEntry->m_rank;
					}
				}
			);

			return true;
		}
		return false;
	}

	DataSource::PropertyValue Leaderboard::GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const
	{
		const Entry& entry = data.GetExpected<Entry>();
		if (identifier == m_entryRankPropertyIdentifier)
		{
			return UnicodeString().Format("{}", entry.m_rank);
		}
		else if (identifier == m_entryNamePropertyIdentifier)
		{
			if (const Optional<const PersistentUserIdentifier*> pPlayerIdentifier = entry.m_identifier.Get<PersistentUserIdentifier>())
			{
				Backend::Plugin& backend = *System::FindPlugin<Backend::Plugin>();
				const Backend::DataSource::Players& playersDataSource = backend.GetGame().GetPlayersDataSource();
				UnicodeString playerName;
				playersDataSource.VisitPlayerInfo(
					*pPlayerIdentifier,
					[&playerName](const Optional<const Backend::DataSource::Players::PlayerInfo*> pPlayerInfo)
					{
						if (pPlayerInfo.IsValid())
						{
							playerName = pPlayerInfo->username;
						}
					}
				);
				if (playerName.HasElements())
				{
					return Move(playerName);
				}
			}
			else
			{
				Assert(false, "TODO: Get generic entry names");
			}
		}
		else if (identifier == m_entryThumbnailPropertyIdentifier)
		{
			if (const Optional<const PersistentUserIdentifier*> pPlayerIdentifier = entry.m_identifier.Get<PersistentUserIdentifier>())
			{
				Backend::Plugin& backend = *System::FindPlugin<Backend::Plugin>();
				const Backend::DataSource::Players& playersDataSource = backend.GetGame().GetPlayersDataSource();
				Asset::Guid playerThumbnailAsset;
				playersDataSource.VisitPlayerInfo(
					*pPlayerIdentifier,
					[&playerThumbnailAsset](const Optional<const Backend::DataSource::Players::PlayerInfo*> pPlayerInfo)
					{
						if (pPlayerInfo.IsValid())
						{
							playerThumbnailAsset = pPlayerInfo->thumbnailGuid;
						}
					}
				);
				if (playerThumbnailAsset.IsValid())
				{
					return playerThumbnailAsset;
				}
			}
			else
			{
				Assert(false, "TODO: Get generic entry thumbnails");
			}
		}
		else if (identifier == m_entryScorePropertyIdentifier)
		{
			return UnicodeString().Format("{}", entry.m_score);
		}
		else if (identifier == m_entryTimePropertyIdentifier)
		{
			Time::Formatter formatter(Time::Durationf::FromMilliseconds(entry.m_score));
			const int64 hours = formatter.GetHours();
			if (hours > 0)
			{
				return UnicodeString()
				  .Format("{:02}:{:02}:{:02}:{:02}", hours, formatter.GetMinutes(), formatter.GetSeconds(), formatter.GetMilliseconds());
			}
			else
			{
				return UnicodeString().Format("{:02}:{:02}:{:02}", formatter.GetMinutes(), formatter.GetSeconds(), formatter.GetMilliseconds());
			}
		}

		return {};
	}
}
