#pragma once

#include <Common/Reflection/Registry.h>
#include <Engine/DataSource/DataSourceInterface.h>
#include <Engine/DataSource/DataSourcePropertyIdentifier.h>
#include <Engine/Tag/TagContainer.h>

#include <Common/Threading/AtomicInteger.h>
#include <Common/Threading/AtomicPtr.h>

#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Storage/AtomicIdentifierMask.h>

namespace ngine
{
	struct Engine;
}

namespace ngine::Reflection
{
	using TypeIdentifier = TIdentifier<uint32, 12>;
	using TypeMask = IdentifierMask<TypeIdentifier>;
	using AtomicTypeMask = Threading::AtomicIdentifierMask<TypeIdentifier>;

	struct EngineRegistry final : public Registry, public DataSource::Interface
	{
		inline static constexpr Guid DataSourceGuid = "{0B74B8E7-0724-4A04-9ABA-13F3F474C74A}"_guid;

		EngineRegistry();
		~EngineRegistry();

		[[nodiscard]] uint16 GetTypeCount() const
		{
			return m_typeCount;
		}

		// DataSource::Interface
		virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const override final;
		[[nodiscard]] virtual GenericDataIndex GetDataCount() const override
		{
			return m_typeCount;
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
		virtual PropertyValue GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const override;
		// ~DataSource::Interface

		void RegisterDynamicType(const Guid guid, const TypeInterface& typeInterface);
		template<typename Type>
		void RegisterDynamicType();

		//! Appends the specified tag to all assets in the mask
		void SetTagTypes(const Tag::Identifier tag, const TypeMask& mask)
		{
			m_tags.Set(tag, mask);
		}
		//! Clears the specified tag for all assets in the mask
		void ClearTagTypes(const Tag::Identifier tag, const TypeMask& mask)
		{
			m_tags.Clear(tag, mask);
		}
	protected:
		Threading::Atomic<uint16> m_typeCount;
		TSaltedIdentifierStorage<TypeIdentifier> m_typeIdentifiers;

		using TypeContainer = TIdentifierArray<Guid, TypeIdentifier>;
		TypeContainer m_typeGuids{Memory::Zeroed};

		using TagContainer = Tag::AtomicMaskContainer<TypeIdentifier>;
		TagContainer m_tags;

		DataSource::PropertyIdentifier m_typeNamePropertyIdentifier;
		DataSource::PropertyIdentifier m_typeGuidPropertyIdentifier;
		DataSource::PropertyIdentifier m_typeTagPropertyIdentifier;
		DataSource::PropertyIdentifier m_typeThumbnailPropertyIdentifier;

		struct FunctionsDataSource final : public DataSource::Interface
		{
			inline static constexpr Guid DataSourceGuid = "ac84d31d-1d91-48c4-b8b6-9c975c84d6ae"_guid;

			FunctionsDataSource();
			~FunctionsDataSource();

			// DataSource::Interface
			virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const override final;
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
			virtual PropertyValue GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const override;
			// ~DataSource::Interface
		protected:
			DataSource::PropertyIdentifier m_functionIdentifierPropertyIdentifier;
			DataSource::PropertyIdentifier m_functionNamePropertyIdentifier;
			DataSource::PropertyIdentifier m_functionAssetGuidPropertyIdentifier;
		};
		FunctionsDataSource m_functionsDataSource;

		struct EventsDataSource final : public DataSource::Interface
		{
			inline static constexpr Guid DataSourceGuid = "2cbab8bd-7d3c-48b4-a02c-031b4254ee0f"_guid;

			EventsDataSource();
			~EventsDataSource();

			// DataSource::Interface
			virtual void CacheQuery(const Query& query, CachedQuery& cachedQueryOut) const override final;
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
			virtual PropertyValue GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const override;
			// ~DataSource::Interface
		protected:
			DataSource::PropertyIdentifier m_eventIdentifierPropertyIdentifier;
			DataSource::PropertyIdentifier m_eventNamePropertyIdentifier;
			DataSource::PropertyIdentifier m_eventAssetGuidPropertyIdentifier;
		};
		EventsDataSource m_eventsDataSource;
	};
}
