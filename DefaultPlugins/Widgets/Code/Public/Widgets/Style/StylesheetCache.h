#pragma once

#include <Engine/Asset/AssetType.h>

#include <Widgets/Style/StylesheetIdentifier.h>

#include <Common/Asset/Guid.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Storage/AtomicIdentifierMask.h>

namespace ngine::Asset
{
	struct Manager;
}

namespace ngine::Reflection
{
	struct Registry;
}

namespace ngine::IO
{
	struct FileView;
	struct Path;
}

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Widgets::Style
{
	struct ComputedStylesheet;

	struct StylesheetData
	{
		StylesheetData();
		StylesheetData(const StylesheetData&) = delete;
		StylesheetData& operator=(const StylesheetData&) = delete;
		StylesheetData(StylesheetData&&);
		StylesheetData& operator=(StylesheetData&&) = delete;
		~StylesheetData();

		UniquePtr<ComputedStylesheet> pStylesheet;
	};

	struct StylesheetCache final : public Asset::Type<StylesheetIdentifier, StylesheetData>
	{
		using BaseType = Asset::Type<StylesheetIdentifier, StylesheetData>;

		StylesheetCache(Asset::Manager& assetManager);
		~StylesheetCache();

		[[nodiscard]] StylesheetIdentifier FindOrRegister(const Asset::Guid assetGuid);

		using StylesheetRequesters = ThreadSafe::Event<EventCallbackResult(void*, const Optional<const ComputedStylesheet*>), 24, false>;

		[[nodiscard]] Threading::Job* FindOrLoad(const StylesheetIdentifier identifier, StylesheetRequesters::ListenerData&& listenerData);
	protected:
#if DEVELOPMENT_BUILD
		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override;
#endif
	protected:
		Threading::SharedMutex m_stylesheetRequesterMutex;
		UnorderedMap<StylesheetIdentifier, UniqueRef<StylesheetRequesters>, StylesheetIdentifier::Hash> m_stylesheetRequesterMap;
		Threading::AtomicIdentifierMask<StylesheetIdentifier> m_loadingStylesheets;
	};
}
