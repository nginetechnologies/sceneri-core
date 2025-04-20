#pragma once

#include <Widgets/Widget.h>

#include <Engine/DataSource/PropertySourceInterface.h>

#include <Renderer/Window/OpenDocumentFlags.h>

#include <Common/Network/Address.h>
#include <Common/Asset/Picker.h>
#include <Common/Asset/Reference.h>

namespace ngine::Asset
{
	struct Guid;
	struct DatabaseEntry;
}

namespace ngine::Widgets
{
	struct DocumentData;
}

namespace ngine::Widgets::Document
{
	struct Window;

	//! Represents a widget that can open a document requested by Window::OpenDocuments
	struct Widget : public Widgets::Widget, public ngine::PropertySource::Interface
	{
		using BaseType = Widgets::Widget;

		Widget(Initializer&& initializer);
		Widget(const Deserializer& deserializer);
		Widget(const Widget& templateComponent, const Cloner& cloner);

		void OnCreated();

		using BaseType::GetIdentifier;

		// PropertySource::Interface
		[[nodiscard]] virtual ngine::DataSource::PropertyValue GetDataProperty(const ngine::DataSource::PropertyIdentifier identifier
		) const override;
		// ~PropertySource::Interface

		[[nodiscard]] virtual Asset::Reference GetDocumentAssetReference() const = 0;
		[[nodiscard]] virtual ArrayView<const Guid, uint16> GetSupportedDocumentAssetTypeGuids() const = 0;
		[[nodiscard]] virtual IO::URI GetDocumentURI() const
		{
			return {};
		}

		virtual void CloseDocument() = 0;
	protected:
		friend Window;
		friend struct Reflection::ReflectedType<Widget>;

		void SetAssetFromProperty(Asset::Picker asset);
		[[nodiscard]] Asset::Picker GetAssetFromProperty() const;
		[[nodiscard]] Threading::JobBatch SetDeserializedAssetFromProperty(
			const Asset::Picker asset,
			[[maybe_unused]] const Serialization::Reader objectReader,
			[[maybe_unused]] const Serialization::Reader typeReader
		);

		[[nodiscard]] virtual Threading::JobBatch OpenDocumentAssetInternal(
			const DocumentData& document,
			const Serialization::Data& assetData,
			const Asset::DatabaseEntry& assetEntry,
			const EnumFlags<OpenDocumentFlags> openDocumentFlags
		) = 0;

		virtual void OnStyleChanged(
			const Style::CombinedEntry& style,
			const Style::CombinedMatchingEntryModifiersView matchingModifiers,
			const ConstChangedStyleValuesView changedValues
		) override;
	protected:
		const ngine::DataSource::PropertyIdentifier m_documentTitlePropertyIdentifier;
		const ngine::DataSource::PropertyIdentifier m_documentUriPropertyIdentifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Document::Widget>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Document::Widget>(
			"5b63b4c4-28cd-4b81-861f-45d80cf6ca34"_asset,
			MAKE_UNICODE_LITERAL("Document Widget"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning,
			Tags{},
			Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Asset"),
				"document",
				"c27ca15b-c87f-4434-9228-b29e28ede1e7"_guid,
				MAKE_UNICODE_LITERAL("Document"),
				Reflection::PropertyFlags{},
				&Widgets::Document::Widget::SetAssetFromProperty,
				&Widgets::Document::Widget::GetAssetFromProperty,
				&Widgets::Document::Widget::SetDeserializedAssetFromProperty
			)}
		);
	};
}
