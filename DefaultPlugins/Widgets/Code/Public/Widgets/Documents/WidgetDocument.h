#pragma once

#include <Widgets/Documents/SceneDocument2D.h>

#include <Common/Memory/UniqueRef.h>

namespace ngine::Widgets
{
	struct Scene;
	struct QuadtreeTraversalStage;
	struct WidgetDrawingStage;
	struct CopyToScreenStage;
}

namespace ngine::Rendering
{
	struct Framegraph;
}

namespace ngine::Widgets::Document
{
	//! Represents a widget that can open other widgets as documents
	struct WidgetDocument : public Document::Scene2D
	{
		using BaseType = Document::Scene2D;
		WidgetDocument(Initializer&& initializer);
		WidgetDocument(const Deserializer& deserializer);
		WidgetDocument(const WidgetDocument&) = delete;
		WidgetDocument(WidgetDocument&&) = delete;
		WidgetDocument& operator=(const WidgetDocument&) = delete;
		WidgetDocument& operator=(WidgetDocument&&) = delete;
		~WidgetDocument();

		// Widget
		virtual void OnFramegraphInvalidated() override;
		virtual void BuildFramegraph(Rendering::FramegraphBuilder&) override;
		virtual void OnFramegraphBuilt() override;
		virtual void OnSceneChanged() override;
		// ~Widget

		void OnCreated();

		[[nodiscard]] Rendering::Stage& GetQuadtreeTraversalStage();
		[[nodiscard]] Rendering::Stage& GetWidgetDrawToRenderTargetStage();
		[[nodiscard]] Rendering::Stage& GetWidgetCopyToScreenStage();

		[[nodiscard]] Optional<Widgets::Widget*> GetEditedWidget() const
		{
			return m_pEditedWidget;
		}
	protected:
		void EnableFramegraph(Widgets::Scene& scene, Rendering::Framegraph& framegraph);
		void DisableFramegraph(Widgets::Scene& scene, Rendering::Framegraph& framegraph);

		[[nodiscard]] virtual Asset::Reference GetDocumentAssetReference() const override;
		[[nodiscard]] virtual ArrayView<const Guid, uint16> GetSupportedDocumentAssetTypeGuids() const override;

		virtual Threading::JobBatch OpenDocumentAssetInternal(
			const DocumentData& document,
			const Serialization::Data& assetData,
			const Asset::DatabaseEntry& assetEntry,
			const EnumFlags<OpenDocumentFlags> openDocumentFlags
		) override;
		virtual void CloseDocument() override;

		virtual void SetEditedWidget(const Optional<Widgets::Widget*> pWidget)
		{
			m_pEditedWidget = pWidget;
		}
	protected:
		// TODO: Move quadtree traversal to our Scene2D parent type when it no longer references Widget
		UniqueRef<Widgets::QuadtreeTraversalStage> m_pQuadtreeTraversalStage;
		UniqueRef<Widgets::WidgetDrawingStage> m_pWidgetDrawingStage;
		UniqueRef<Widgets::CopyToScreenStage> m_pWidgetsCopyToScreenStage;

		Optional<Widgets::Widget*> m_pEditedWidget;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Widgets::Document::WidgetDocument>
	{
		inline static constexpr auto Type = Reflection::Reflect<Widgets::Document::WidgetDocument>(
			"{D7E304BB-4AA3-46DE-8323-D73CA8366394}"_guid,
			MAKE_UNICODE_LITERAL("Widget Document"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning
		);
	};
}
