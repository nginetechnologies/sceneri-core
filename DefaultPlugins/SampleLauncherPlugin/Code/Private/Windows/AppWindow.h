#pragma once

#include <Widgets/Documents/DocumentWindow.h>

namespace ngine
{
	struct ProjectInfo;
}

namespace ngine::Asset
{
	struct DatabaseEntry;
	struct Database;
}

namespace ngine::App::UI::Document
{
	struct MainWindow;
	struct Scene3D;

	struct Window : public Widgets::Document::Window
	{
		using BaseType = Widgets::Document::Window;

		struct Initializer : public ToolWindow::Initializer
		{
			using BaseType = ToolWindow::Initializer;
			Initializer(BaseType&& baseInitializer, MainWindow& mainWindow)
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_mainWindow(mainWindow)
			{
			}

			MainWindow& m_mainWindow;
		};

		Window(Initializer&& initializer);
		virtual ~Window() = default;

		[[nodiscard]] virtual const Entity::SceneRegistry& GetEntitySceneRegistry() const override;
		[[nodiscard]] virtual Entity::SceneRegistry& GetEntitySceneRegistry() override;
		[[nodiscard]] virtual const Widgets::Scene& GetScene() const override;
		[[nodiscard]] virtual Widgets::Scene& GetScene() override;
		[[nodiscard]] virtual const Entity::RootComponent& GetRootComponent() const = 0;
		[[nodiscard]] virtual Entity::RootComponent& GetRootComponent() = 0;

		virtual IO::URI CreateShareableUri(const IO::ConstURIView parameters) const override;
	protected:
		virtual Optional<Widgets::Document::Widget*>
		CreateDocumentWidget(const DocumentData&, const Guid assetTypeGuid, EnumFlags<OpenDocumentFlags> openDocumentFlags) override;

		virtual Asset::Identifier CreateAssetDocumentInternal(
			const Guid assetTypeGuid, UnicodeString&& documentName, IO::Path&& documentPath, const Optional<const DocumentData*> pTemplateDocument
		) override;

		[[nodiscard]] virtual Threading::JobBatch ResolveMissingAssetDocument(
			const Asset::Guid assetGuid,
			const Asset::Guid parentAssetGuid,
			const EnumFlags<OpenDocumentFlags> openDocumentFlags,
			Optional<Widget*> pDocumentWidget
		) override;
		[[nodiscard]] virtual Threading::JobBatch OpenDocumentFromAddress(
			const Network::Address address, EnumFlags<OpenDocumentFlags> openDocumentFlags, const Optional<Widget*> pDocumentWidget
		) override;
	protected:
		MainWindow& m_mainWindow;
	};
}
