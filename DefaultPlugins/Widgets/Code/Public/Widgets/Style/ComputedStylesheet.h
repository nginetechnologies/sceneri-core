#pragma once

#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Asset/AssetFormat.h>

#include <Widgets/Style/Entry.h>

namespace ngine::Reflection
{
	struct Registry;
}

namespace ngine
{
	extern template struct UnorderedMap<Guid, Widgets::Style::Entry, Guid::Hash>;
}

namespace ngine::Widgets::Style
{
	struct ComputedStylesheet
	{
		inline static constexpr Asset::Format AssetFormat = {
			"{478C98C1-7DDC-4F5E-8C6F-9B94D941FF76}"_guid, MAKE_PATH(".computedstylesheet.nasset")
		};
		inline static constexpr Asset::Format CSSAssetFormat = {
			"{4A976EFB-3161-426A-B717-0955CF30641A}"_guid, MAKE_PATH(".css.nasset"), MAKE_PATH(".css")
		};

		ComputedStylesheet() = default;
		ComputedStylesheet(ComputedStylesheet&&) = default;
		ComputedStylesheet& operator=(ComputedStylesheet&&) = default;

		void ParseFromCSS(const ConstStringView styleString);

		using EntryMap = UnorderedMap<Guid, Entry, Guid::Hash>;

		[[nodiscard]] uint32 GetEntryCount() const
		{
			return m_entries.GetSize();
		}
		[[nodiscard]] const Entry& GetEntry(const Guid id) const LIFETIME_BOUND;
		[[nodiscard]] Optional<Entry*> FindEntry(const Guid id) LIFETIME_BOUND;
		[[nodiscard]] Optional<const Entry*> FindEntry(const Guid id) const LIFETIME_BOUND;

		bool Serialize(const Serialization::Reader reader);
	protected:
		EntryMap m_entries;
	};
}
