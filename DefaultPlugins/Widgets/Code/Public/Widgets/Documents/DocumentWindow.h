#pragma once

#include <Widgets/ToolWindow.h>

namespace ngine::Asset
{
	struct DatabaseEntry;
}

namespace ngine::Network
{
	struct Address;
}

namespace ngine::Widgets::Document
{
	struct Widget;

	//! Represents a window that can open one or more documents by spawning DocumentWidget's
	struct Window : public Rendering::ToolWindow
	{
		using Widget = Widgets::Document::Widget;

		using BaseType = ToolWindow;
		using BaseType::BaseType;

		[[nodiscard]] virtual Threading::JobBatch
		OpenDocuments(const ArrayView<const DocumentData> documents, const EnumFlags<OpenDocumentFlags>) override final;
		[[nodiscard]] Threading::JobBatch OpenDocuments(
			const ArrayView<const DocumentData> documents, const EnumFlags<OpenDocumentFlags>, const Optional<Widget*> pDocumentWidget
		);

		virtual Asset::Identifier CreateDocument(
			const Guid assetTypeGuid,
			UnicodeString&& documentName,
			IO::Path&& documentPath,
			const EnumFlags<CreateDocumentFlags>,
			const Optional<const DocumentData*> pTemplateDocument
		) override final;
		Asset::Identifier CreateDocument(
			const Guid assetTypeGuid,
			UnicodeString&& documentName,
			IO::Path&& documentPath,
			const EnumFlags<CreateDocumentFlags>,
			const Optional<Widget*> pDocumentWidget,
			const Optional<const DocumentData*> pTemplateDocument
		);
	protected:
		virtual Optional<Widget*>
		CreateDocumentWidget(const DocumentData&, [[maybe_unused]] const Guid assetTypeGuid, EnumFlags<OpenDocumentFlags>)
		{
			return Invalid;
		}

		[[nodiscard]] Threading::JobBatch OpenDocumentFromPathInternal(
			const IO::Path& documentPath, EnumFlags<OpenDocumentFlags> openDocumentFlags, const Optional<Widget*> pDocumentWidget
		);
		[[nodiscard]] Threading::JobBatch OpenDocumentFromURIInternal(
			const IO::URI& documentURI, EnumFlags<OpenDocumentFlags> openDocumentFlags, const Optional<Widget*> pDocumentWidget
		);
		[[nodiscard]] Threading::JobBatch OpenDocumentFromAddressInternal(
			const Network::Address address, EnumFlags<OpenDocumentFlags> openDocumentFlags, const Optional<Widget*> pDocumentWidget
		);

		[[nodiscard]] virtual Asset::Identifier CreateAssetDocumentInternal(
			const Guid assetTypeGuid, UnicodeString&& documentName, IO::Path&& documentPath, const Optional<const DocumentData*> pTemplateDocument
		) = 0;

		//! Called when we're requesting opening a document from an address, typically to connect to a server
		[[nodiscard]] virtual Threading::JobBatch OpenDocumentFromAddress(
			const Network::Address address, EnumFlags<OpenDocumentFlags> openDocumentFlags, const Optional<Widget*> pDocumentWidget
		) = 0;

		virtual void OnDocumentOpeningFailed()
		{
		}

		[[nodiscard]] virtual Threading::JobBatch ResolveMissingAssetDocument(
			const Asset::Guid assetGuid,
			const Asset::Guid parentAssetGuid,
			const EnumFlags<OpenDocumentFlags> openDocumentFlags,
			Optional<Widget*> pDocumentWidget
		);
	};
}
