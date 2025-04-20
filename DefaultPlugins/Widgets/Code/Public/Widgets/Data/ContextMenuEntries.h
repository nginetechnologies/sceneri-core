#pragma once

#include <Widgets/Data/Component.h>
#include <Widgets/EventData.h>

#include <Common/Memory/Containers/String.h>
#include <Common/Asset/Guid.h>

namespace ngine::Widgets
{
	struct DataSourceProperties;
}

namespace ngine::Widgets::Data
{
	struct ContextMenuEntries : public Widgets::Data::Component
	{
		using InstanceIdentifier = TIdentifier<uint32, 11>;

		using BaseType = Widgets::Data::Component;

		ContextMenuEntries(const Deserializer& deserializer);
		ContextMenuEntries(const ContextMenuEntries& templateComponent, const Cloner& cloner);

		ContextMenuEntries(const ContextMenuEntries&) = delete;
		ContextMenuEntries& operator=(const ContextMenuEntries&) = delete;
		ContextMenuEntries(ContextMenuEntries&&) = delete;
		ContextMenuEntries& operator=(ContextMenuEntries&&) = delete;
		~ContextMenuEntries() = default;

		void UpdateFromDataSource(Widget& owner, const DataSourceProperties& dataSourceProperties);

		struct Entry : public Widgets::EventData
		{
			Entry() = default;
			Entry(Entry&& entry) = default;
			Entry(const Entry&) = delete;
			Entry& operator=(Entry&&) = default;
			Entry& operator=(const Entry&) = delete;
			Entry(EventData&& eventData, UnicodeString&& title, const Asset::Guid iconAssetGuid)
				: EventData(Forward<EventData>(eventData))
				, m_title(Forward<UnicodeString>(title))
				, m_iconAssetGuid(iconAssetGuid)
			{
			}
			Entry(const Entry& other, Widget& widget)
				: EventData(other, widget)
				, m_title(other.m_title)
				, m_iconAssetGuid(other.m_iconAssetGuid)
			{
			}

			UnicodeString m_title;
			Asset::Guid m_iconAssetGuid;
		};
		[[nodiscard]] ArrayView<const Entry, uint16> GetEntries() const
		{
			return m_entries;
		}
	protected:
		Vector<Entry, uint16> m_entries;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Data::ContextMenuEntries>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Data::ContextMenuEntries>(
			"{D2CF9F98-1F36-4B66-A862-680071EC08D9}"_guid,
			MAKE_UNICODE_LITERAL("Context Menu Entries"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicDeserialization | TypeFlags::DisableWriteToDisk
		);
	};
}
