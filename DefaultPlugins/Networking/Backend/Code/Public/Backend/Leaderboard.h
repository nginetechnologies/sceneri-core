#pragma once

#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/DataSourceIdentifier.h>
#include <Engine/DataSource/DataSourcePropertyIdentifier.h>

#include <Backend/PersistentUserIdentifier.h>

#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/CallbackResult.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Variant.h>
#include <Common/Function/Event.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Math/Range.h>

#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Threading/Mutexes/SharedMutex.h>

namespace ngine::DataSource
{
	struct Cache;
}

namespace ngine::Networking::Backend
{
	struct Plugin;

	struct Leaderboard final : public DataSource::Interface
	{
		enum class Flags : uint8
		{
			EnableGameAPIWrites = 1 << 0,
			OverwriteScoreOnSubmit = 1 << 1,
			HasMetadata = 1 << 2,
			Ascending = 1 << 3,
			//! Whether this is a non-player leaderboard that could be attached to guilds, non-player entities etc
			Generic = 1 << 4
		};

		struct Player
		{
			uint64 m_backendIdentifier;
		};

		struct Entry
		{
			uint64 m_rank;
			uint64 m_score;
			String m_metadata;
			Variant<PersistentUserIdentifier, String> m_identifier;
		};

		Leaderboard() = delete;
		Leaderboard(Backend::Plugin& plugin, const Guid guid, const EnumFlags<Flags> flags = {});
		~Leaderboard();

		// DataSource::Interface
		virtual void LockRead() override;
		virtual void UnlockRead() override;

		virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const override final;
		virtual bool SortQuery(
			const CachedQuery& cachedQuery,
			const PropertyIdentifier filteredPropertyIdentifier,
			const SortingOrder order,
			SortedQueryIndices& cachedSortedQueryOut
		) override final;
		[[nodiscard]] virtual GenericDataIndex GetDataCount() const override;
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
		// ~DataSource::Interface

		using Callback = Function<void(const bool), 24>;
		void Create(const ConstStringView name, Callback&& callback);
		void Get(const Math::Range<uint32> range, Callback&& callback);

		void SubmitScore(uint64 score, const ConstStringView metadata);
		void SubmitScoreOrCreate(uint64 score, const ConstStringView metadata);
	private:
		Leaderboard(Backend::Plugin& plugin, const Guid guid, DataSource::Cache& dataSourceCache, const EnumFlags<Flags> flags);
		[[nodiscard]] static Entry ParseEntry(const Serialization::Reader reader);
		bool EmplaceEntry(Entry&& entry);
	private:
		const Guid m_guid;

		Backend::Plugin& m_backend;
		DataSource::PropertyIdentifier m_entryRankPropertyIdentifier;
		DataSource::PropertyIdentifier m_entryNamePropertyIdentifier;
		DataSource::PropertyIdentifier m_entryThumbnailPropertyIdentifier;
		DataSource::PropertyIdentifier m_entryScorePropertyIdentifier;
		DataSource::PropertyIdentifier m_entryTimePropertyIdentifier;

		uint64 m_backendIdentifier;
		String m_name;
		const EnumFlags<Flags> m_flags;

		Threading::SharedMutex m_entryMutex;
		InlineVector<Entry, 10, GenericDataIndex> m_entries;
	};

	ENUM_FLAG_OPERATORS(Leaderboard::Flags);
}
