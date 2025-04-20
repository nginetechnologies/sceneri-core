#pragma once

#include <Common/Asset/Guid.h>
#include <Common/Memory/Optional.h>

namespace ngine::Entity
{
	struct ComponentSoftReference;
}

namespace ngine::DataSource
{
	struct Dynamic;
}

namespace ngine::PropertySource
{
	struct Dynamic;
}

namespace ngine::Widgets
{
	struct ContextMenuBuilder
	{
		ContextMenuBuilder() = default;
		enum class DeferredType : uint8
		{
			Deferred
		};
		inline static constexpr DeferredType Deferred{DeferredType::Deferred};
		ContextMenuBuilder(DeferredType)
			: m_isDeferred(true)
		{
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_isDeferred || m_pDataSource.IsValid() || m_pPropertySource.IsValid();
		}
		[[nodiscard]] bool IsDeferred() const
		{
			return m_isDeferred;
		}

		void AddEntry(const Asset::Guid iconAssetGuid, const ConstUnicodeStringView title, const Guid eventGuid);
		void AddEntry(const ConstUnicodeStringView title, const Guid eventGuid);

		//! Sets the asset that is related to this context menu (i.e. right-click an asset)
		void SetAsset(const Asset::Guid assetGuid);
		//! Sets the component that is related to this context menu (i.e. right-click an component)
		void SetComponent(const Entity::ComponentSoftReference component);

		[[nodiscard]] Optional<ngine::DataSource::Dynamic*> GetDataSource() const
		{
			return m_pDataSource;
		}
		[[nodiscard]] Optional<ngine::PropertySource::Dynamic*> GetPropertySource() const
		{
			return m_pPropertySource;
		}

		[[nodiscard]] Asset::Guid GetContextMenuAsset() const
		{
			return m_contextMenuAsset;
		}
		void SetContextMenuAsset(const Asset::Guid guid)
		{
			m_contextMenuAsset = guid;
		}
		[[nodiscard]] Asset::Guid GetContextMenuEntryAsset() const
		{
			return m_contextMenuEntryAsset;
		}
		void SetContextMenuEntryAsset(const Asset::Guid guid)
		{
			m_contextMenuEntryAsset = guid;
		}
	protected:
		[[nodiscard]] ngine::DataSource::Dynamic& GetOrCreateDataSource();
		[[nodiscard]] ngine::PropertySource::Dynamic& GetOrCreatePropertySource();
	protected:
		bool m_isDeferred{false};
		Optional<ngine::DataSource::Dynamic*> m_pDataSource;
		Optional<ngine::PropertySource::Dynamic*> m_pPropertySource;

		Asset::Guid m_contextMenuAsset{"cb282422-cdb8-4929-952b-0c9649fbe941"_asset};
		Asset::Guid m_contextMenuEntryAsset{"a9d18d3e-09fe-458c-9816-ba51ca957b6a"_asset};
	};
}
