#pragma once

#include <Engine/Asset/AssetType.h>

#include <Widgets/WidgetTemplateIdentifier.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::Threading
{
	struct JobBatch;
}

namespace ngine::Widgets
{
	struct Scene;
	struct Widget;

	struct Template
	{
		Template() = default;
		Template(const Template&) = delete;
		Template& operator=(const Template&) = delete;
		Template(Template&&) = default;
		Template& operator=(Template&&) = default;
		~Template();

		Vector<ByteType, size> m_data;
		Optional<Widget*> m_pWidget;
	};

	struct WidgetCache final : public Asset::Type<TemplateIdentifier, Template>
	{
		using BaseType = Asset::Type<TemplateIdentifier, Template>;

		WidgetCache();
		~WidgetCache();

		void Reset();
		void Reset(const TemplateIdentifier identifier);

		[[nodiscard]] TemplateIdentifier FindOrRegister(const Asset::Guid guid);

		using LoadEvent = ThreadSafe::Event<EventCallbackResult(void*, const TemplateIdentifier), 24, false>;
		using LoadListenerData = LoadEvent::ListenerData;
		using LoadListenerIdentifier = LoadEvent::ListenerIdentifier;

		enum class LoadFlags : uint8
		{
			//! Whether the callback should always be deferred to a new job, even if the widget was already loaded
			AlwaysDeferCallback = 1 << 0,
			Default = AlwaysDeferCallback
		};
		[[nodiscard]] Threading::JobBatch TryLoad(
			const TemplateIdentifier identifier, LoadListenerData&& newListenerData, const EnumFlags<LoadFlags> loadFlags = LoadFlags::Default
		);

		[[nodiscard]] bool HasLoaded(const TemplateIdentifier identifier) const;
	protected:
#if DEVELOPMENT_BUILD
		virtual void OnAssetModified(const Asset::Guid assetGuid, const IdentifierType identifier, const IO::PathView filePath) override;
#endif

		Threading::AtomicIdentifierMask<TemplateIdentifier> m_loadingWidgets;
		Threading::AtomicIdentifierMask<TemplateIdentifier> m_loadedWidgets;

		Threading::SharedMutex m_widgetRequesterMutex;
		UnorderedMap<TemplateIdentifier, UniquePtr<LoadEvent>, TemplateIdentifier::Hash> m_widgetRequesterMap;

		// The scene in which we instantiate scene components used for templates
		UniquePtr<Widgets::Scene> m_pTemplateScene;
	};

	ENUM_FLAG_OPERATORS(WidgetCache::LoadFlags);
}
