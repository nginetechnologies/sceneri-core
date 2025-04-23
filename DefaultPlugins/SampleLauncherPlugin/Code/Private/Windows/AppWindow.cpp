#include "AppWindow.h"
#include "MainAppWindow.h"
#include "Document/SceneDocument.h"

#include <Engine/Engine.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/RootComponent.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Data/WorldTransform2D.h>
#include <Engine/Entity/Scene/ComponentTemplateCache.h>
#include <Engine/Entity/Manager.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/RootSceneComponent2D.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Context/EventManager.inl>

#include <Renderer/Window/DocumentData.h>

#include <Widgets/Widget.inl>
#include <Widgets/RootWidget.h>
#include <Widgets/Style/Entry.h>
#include <Widgets/Documents/WidgetDocument.h>
#include <Widgets/Documents/WidgetDocument.h>
#include <Widgets/Data/Layout.h>
#include <Widgets/Data/GridLayout.h>
#include <Widgets/Data/DataSource.h>
#include <Widgets/Data/PropertySource.h>

#include <Backend/Plugin.h>
#include <Backend/LoadAssetFromBackendJob.h>

#include <NetworkingCore/Components/ClientComponent.h>
#include <NetworkingCore/Components/LocalClientComponent.h>
#include <NetworkingCore/Components/HostComponent.h>
#include <NetworkingCore/Components/LocalHostComponent.h>

#include <Common/Project System/ProjectInfo.h>
#include <Common/Project System/ProjectAssetFormat.h>
#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/IO/Format/URI.h>

namespace ngine::App::UI::Document
{
	using namespace Widgets;

	Window::Window(Initializer&& initializer)
		: Widgets::Document::Window(Forward<Initializer>(initializer))
		, m_mainWindow(initializer.m_mainWindow)
	{
	}

	Optional<Document::Document::Widget*> Window::
		CreateDocumentWidget([[maybe_unused]] const DocumentData& documentData, [[maybe_unused]] const Guid assetTypeGuid, const EnumFlags<OpenDocumentFlags>)
	{
		const bool isProject =
			assetTypeGuid == ProjectAssetFormat.assetTypeGuid ||
			documentData.Visit(
				[](const IO::Path& documentPath)
				{
					return documentPath.GetRightMostExtension() == ProjectAssetFormat.metadataFileExtension;
				},
				[](const IO::URI& uri)
				{
					return uri.GetRightMostExtension() ==
			           IO::URI(IO::URI::StringType(ProjectAssetFormat.metadataFileExtension.GetStringView())).GetView().GetStringView();
				},
				[](const Asset::Guid assetGuid)
				{
					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
					return assetManager.GetAssetPath(assetGuid).GetRightMostExtension() == ProjectAssetFormat.metadataFileExtension ||
			           assetLibrary.GetAssetPath(assetGuid).GetRightMostExtension() == ProjectAssetFormat.metadataFileExtension;
				},
				[](const Asset::Identifier assetIdentifier)
				{
					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					return assetManager.GetAssetPath(assetIdentifier).GetRightMostExtension() == ProjectAssetFormat.metadataFileExtension;
				},
				[](const Network::Address)
				{
					Assert(false, "Should never enter CreateDocumentWidget with a remote address");
					return false;
				},
				[]() -> bool
				{
					return false;
				}
			);

		static Optional<Document::Scene*> pExistingViewportWidget = nullptr;
		if (isProject)
		{
			if (pExistingViewportWidget.IsInvalid())
			{
				UniquePtr<Widgets::Style::Entry> pStyle = []()
				{
					UniquePtr<Widgets::Style::Entry> pStyle{Memory::ConstructInPlace};
					Widgets::Style::Entry::ModifierValues& modifier = pStyle->EmplaceExactModifierMatch(Widgets::Style::Modifier::None);
					modifier.ParseFromCSS("width: 100%; height: 100%; position: absolute");
					pStyle->OnValueTypesAdded(modifier.GetValueTypeMask());
					pStyle->OnValueTypesAdded(modifier.GetDynamicValueTypeMask());
					return pStyle;
				}();

				pExistingViewportWidget = GetRootWidget().EmplaceChildWidget<Document::Scene>(
					Document::Scene::Initializer{Widget::Initializer{GetRootWidget(), Widget::Flags{}, Move(pStyle)}}
				);
				Threading::JobBatch createViewportJobBatch;
				pExistingViewportWidget->LoadResources(GetEntitySceneRegistry(), &createViewportJobBatch);
				if (createViewportJobBatch.IsValid())
				{
					Threading::JobRunnerThread::GetCurrent()->Queue(createViewportJobBatch);
				}
			}
		}

		return pExistingViewportWidget;
	}

	Threading::JobBatch Window::ResolveMissingAssetDocument(
		const Asset::Guid assetGuid,
		const Asset::Guid parentAssetGuid,
		const EnumFlags<OpenDocumentFlags> openDocumentFlags,
		Optional<Widget*> pDocumentWidget
	)
	{
		// First check if the asset(s) are available in the asset library
		Asset::Manager& assetManager = System::Get<Asset::Manager>();
		Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
		if (parentAssetGuid.IsValid())
		{
			const Asset::Identifier parentAssetIdentifier = assetManager.Import(
				Asset::LibraryReference{parentAssetGuid, assetLibrary.GetAssetTypeGuid(parentAssetGuid)},
				Asset::ImportingFlags::FullHierarchy
			);
			if (!parentAssetIdentifier.IsValid())
			{
				if (const Optional<Networking::Backend::Plugin*> pBackendPlugin = System::FindPlugin<Networking::Backend::Plugin>())
				{
					// Try importing from backend
					Vector<Asset::Guid> assets(Memory::Reserve, 2);
					assets.EmplaceBack(assetGuid);
					if (parentAssetGuid.IsValid())
					{
						assets.EmplaceBack(parentAssetGuid);
					}

					Threading::JobBatch jobBatch{Threading::JobBatch::IntermediateStage};
					Threading::IntermediateStage& intermediateStage = Threading::CreateIntermediateStage();
					intermediateStage.AddSubsequentStage(jobBatch.GetFinishedStage());

					// Request from backend
					Networking::Backend::Game& gameAPI = pBackendPlugin->GetGame();
					gameAPI.RequestAssets(
						Move(assets),
						{},
						[this, documentGuid = assetGuid, parentAssetGuid, openDocumentFlags, &intermediateStage](
							[[maybe_unused]] const bool responseSuccess
						) mutable
						{
							Assert(responseSuccess);

							// First import the assets from the asset library
							Asset::Manager& assetManager = System::Get<Asset::Manager>();
							Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
							{
								[[maybe_unused]] const Asset::Identifier documentAssetIdentifier = assetManager.Import(
									Asset::LibraryReference{documentGuid, assetLibrary.GetAssetTypeGuid(documentGuid)},
									Asset::ImportingFlags::FullHierarchy
								);
								Assert(documentAssetIdentifier.IsValid());
							}
							{
								[[maybe_unused]] const Asset::Identifier parentAssetIdentifier = assetManager.Import(
									Asset::LibraryReference{parentAssetGuid, assetLibrary.GetAssetTypeGuid(parentAssetGuid)},
									Asset::ImportingFlags::FullHierarchy
								);
								Assert(parentAssetIdentifier.IsValid());
							}
							Threading::JobBatch jobBatch = OpenDocuments(Array{DocumentData{documentGuid, parentAssetGuid}}, openDocumentFlags);
							jobBatch.QueueAsNewFinishedStage(intermediateStage);
							Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
							thread.Queue(jobBatch);
						}
					);
					return jobBatch;
				}
			}
		}

		const Asset::Identifier assetIdentifier = assetManager.Import(
			Asset::LibraryReference{assetGuid, assetLibrary.GetAssetTypeGuid(assetGuid)},
			Asset::ImportingFlags::FullHierarchy
		);
		if (!assetIdentifier.IsValid())
		{
			if (const Optional<Networking::Backend::Plugin*> pBackendPlugin = System::FindPlugin<Networking::Backend::Plugin>())
			{
				// Try importing from backend
				Vector<Asset::Guid> assets(Memory::Reserve, 1);
				assets.EmplaceBack(assetGuid);

				Threading::JobBatch jobBatch{Threading::JobBatch::IntermediateStage};
				Threading::IntermediateStage& intermediateStage = Threading::CreateIntermediateStage();
				intermediateStage.AddSubsequentStage(jobBatch.GetFinishedStage());

				// Request from backend
				Networking::Backend::Game& gameAPI = pBackendPlugin->GetGame();
				gameAPI.RequestAssets(
					Move(assets),
					{},
					[this, documentGuid = assetGuid, parentAssetGuid, openDocumentFlags, &intermediateStage](
						[[maybe_unused]] const bool responseSuccess
					) mutable
					{
						Assert(responseSuccess);

						// First import the assets from the asset library
						Asset::Manager& assetManager = System::Get<Asset::Manager>();
						Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
						{
							[[maybe_unused]] const Asset::Identifier documentAssetIdentifier = assetManager.Import(
								Asset::LibraryReference{documentGuid, assetLibrary.GetAssetTypeGuid(documentGuid)},
								Asset::ImportingFlags::FullHierarchy
							);
							Assert(documentAssetIdentifier.IsValid());
						}
						Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
						Threading::JobBatch jobBatch = OpenDocuments(Array{DocumentData{documentGuid, parentAssetGuid}}, openDocumentFlags);
						jobBatch.QueueAsNewFinishedStage(intermediateStage);
						thread.Queue(jobBatch);
					}
				);
				return jobBatch;
			}
		}

		// Both assets were imported, try again
		return OpenDocuments(Array{DocumentData{assetGuid, parentAssetGuid}}, openDocumentFlags, pDocumentWidget);
	}

	Asset::Identifier Window::CreateAssetDocumentInternal(
		const Guid assetTypeGuid, UnicodeString&& documentName, IO::Path&& documentPath, const Optional<const DocumentData*> pTemplateDocument
	)
	{
		UNUSED(assetTypeGuid);
		UNUSED(documentName);
		UNUSED(documentPath);
		UNUSED(pTemplateDocument);
		Assert(false, "App::Window does not support asset creation!");
		return {};
	}

	Threading::JobBatch Window::OpenDocumentFromAddress(
		const Network::Address address, EnumFlags<OpenDocumentFlags> openDocumentFlags, const Optional<Widget*> pDocumentWidget
	)
	{
		Entity::RootComponent& rootComponent = GetRootComponent();
		const Entity::DataComponentResult<Network::Session::LocalClient> localClientQuery =
			rootComponent.FindFirstDataComponentOfTypeInChildrenRecursive<Network::Session::LocalClient>(GetEntitySceneRegistry());
		Assert(localClientQuery.IsValid());
		if (LIKELY(localClientQuery.IsValid()))
		{
			// Make sure we stop the local server if we were hosting

			[[maybe_unused]] const bool startedConnecting =
				localClientQuery.m_pDataComponent->Connect(*localClientQuery.m_pDataComponentOwner, address);
			Assert(startedConnecting);

			// TODO: See if we want to remember the open document flags and document widget for when the server tells us which project / scene to
			// load.
			UNUSED(openDocumentFlags);
			UNUSED(pDocumentWidget);
		}

		return {};
	}

	const Entity::SceneRegistry& Window::GetEntitySceneRegistry() const
	{
		return m_mainWindow.GetEntitySceneRegistry();
	}
	Entity::SceneRegistry& Window::GetEntitySceneRegistry()
	{
		return m_mainWindow.GetEntitySceneRegistry();
	}

	const Widgets::Scene& Window::GetScene() const
	{
		return m_mainWindow.GetScene();
	}
	Widgets::Scene& Window::GetScene()
	{
		return m_mainWindow.GetScene();
	}

	const Entity::RootComponent& Window::GetRootComponent() const
	{
		return m_mainWindow.GetRootComponent();
	}
	Entity::RootComponent& Window::GetRootComponent()
	{
		return m_mainWindow.GetRootComponent();
	}

	IO::URI Window::CreateShareableUri(IO::ConstURIView parameters) const
	{
		IO::URI::StringType encodedParameters = IO::URI::StringType::Escape(parameters.GetStringView());
		// Wrap in AppsFlyer deep link
		// This allows iOS and Android users to use the link even when the app isn't installed
		IO::URI targetURI = BaseType::CreateShareableUri(IO::URI{Move(encodedParameters)});
		IO::URI::StringType shareableUriString;
		shareableUriString
			.Format("https://sceneri.onelink.me/tBAl?pid=app_share&af_web_dp={}&deep_link_value={}&af_dp={}", targetURI, targetURI, targetURI);
		return IO::URI{Move(shareableUriString)};
	}

	MainWindow::MainWindow(Initializer&& initializer)
		: Window(Window::Initializer{Forward<Initializer>(initializer), *this})
		, m_entitySceneRegistry()
		, m_rootComponent(*m_entitySceneRegistry.CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(m_entitySceneRegistry))
		, m_clientComponent(*m_entitySceneRegistry.GetOrCreateComponentTypeData<Network::Session::Client>()->CreateInstance(
				Network::Session::Client::Initializer{Entity::HierarchyComponentBase::DynamicInitializer{m_rootComponent, m_entitySceneRegistry}}
			))
		, m_scene(m_entitySceneRegistry, m_clientComponent, *this, Guid::Generate(), SceneFlags::IsDisabled)
	{
		Entity::SceneRegistry& sceneRegistry = m_entitySceneRegistry;
		sceneRegistry.CreateComponentTypeData<Widgets::Data::FlexLayout>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::GridLayout>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::DataSource>();
		sceneRegistry.CreateComponentTypeData<Widgets::Data::PropertySource>();

		m_rootComponent.CreateDataComponent<Context::Data::Component>(
			m_entitySceneRegistry,
			Context::Data::Component::Initializer{m_rootComponent, m_entitySceneRegistry}
		);

		Initialize(initializer.m_jobBatch);
	}

	MainWindow::~MainWindow()
	{
	}

	void MainWindow::Initialize(Threading::JobBatch& jobBatch)
	{
		Entity::RootComponent& rootComponent = GetRootComponent();
		Entity::SceneRegistry& sceneRegistry = m_entitySceneRegistry;

		Network::Session::LocalClient& localClient = *m_clientComponent.CreateDataComponent<Network::Session::LocalClient>(
			sceneRegistry,
			Network::Session::LocalClient::Initializer{Entity::Data::HierarchyComponent::DynamicInitializer{m_clientComponent, sceneRegistry}}
		);

		BaseType::Initialize(jobBatch);

		UniquePtr<Widgets::Style::Entry> pStyle = []()
		{
			UniquePtr<Widgets::Style::Entry> pStyle{Memory::ConstructInPlace};
			Widgets::Style::Entry::ModifierValues& modifier = pStyle->EmplaceExactModifierMatch(Widgets::Style::Modifier::None);
			modifier.ParseFromCSS("width: 100%; height: 100%; position: absolute");
			pStyle->OnValueTypesAdded(modifier.GetValueTypeMask());
			pStyle->OnValueTypesAdded(modifier.GetDynamicValueTypeMask());
			return pStyle;
		}();
		Widgets::Widget& rootWidget = GetRootWidget();
		Widgets::Document::WidgetDocument& widgetDocument =
			*rootWidget.EmplaceChildWidget<Widgets::Document::WidgetDocument>(Widgets::Document::Scene2D::Initializer{
				Widgets::Widget::Initializer{rootWidget, Widgets::Widget::Flags::IsInputDisabled, Move(pStyle)},
				Widgets::Document::Scene2D::Flags{}
			});
		rootWidget.RecalculateHierarchy();
		InvalidateFramegraph();

		widgetDocument.AssignScene(rootWidget.GetRootScene());

		const Optional<Networking::Backend::Plugin*> pBackend = System::FindPlugin<Networking::Backend::Plugin>();
		if (pBackend.IsValid())
		{
			using namespace Networking::Backend;
			pBackend->GetGame().CheckCanSignInAsync(
				SignInFlags::Guest,
				[this, &backend = *pBackend](const EnumFlags<SignInFlags> signInFlags)
				{
					backend.GetGame().SignIn(
						signInFlags & ~SignInFlags::Success,
						[]([[maybe_unused]] const EnumFlags<SignInFlags> signInFlags)
						{
							Assert(signInFlags.IsSet(SignInFlags::SignedIn), "Failed guest sign in");
						},
						*this
					);
				},
				*this
			);
		}

#if PLATFORM_WEB
		IO::URI windowURI = GetUri();
		if (windowURI.GetView().HasQueryParameter(IO::ConstURIView{MAKE_URI_LITERAL("project")}))
		{
			if (pBackend.IsValid())
			{
				Networking::Backend::Game& gameAPI = pBackend->GetGame();
				gameAPI.QueuePostSessionRegistrationCallback(
					[this, windowURI = Move(windowURI)](const EnumFlags<Networking::Backend::SignInFlags>) mutable
					{
						Threading::JobBatch jobBatch = OpenDocuments(Array{DocumentData{Move(windowURI)}}, OpenDocumentFlags{});
						Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
					}
				);
			}
			else
			{
				Threading::JobBatch openDocumentsJobBatch = OpenDocuments(Array{DocumentData{Move(windowURI)}}, OpenDocumentFlags{});
				jobBatch.QueueAsNewFinishedStage(openDocumentsJobBatch);
			}
		}
#endif

		// Default to starting a server with a maximum user count of 4
		const uint8 maximumUserCount = 4;
		Entity::ComponentTypeSceneData<Network::Session::Host>& sessionHostSceneData =
			*sceneRegistry.GetOrCreateComponentTypeData<Network::Session::Host>();
		Network::Session::Host& hostComponent = *sessionHostSceneData.CreateInstance(
			Network::Session::Host::Initializer{Entity::HierarchyComponentBase::DynamicInitializer{rootComponent, sceneRegistry}}
		);

		[[maybe_unused]] Network::Session::LocalHost& sessionHostComponent = *hostComponent.CreateDataComponent<Network::Session::LocalHost>(
			sceneRegistry,
			Network::Session::LocalHost::Initializer{
				Entity::Data::HierarchyComponent::DynamicInitializer{hostComponent, sceneRegistry},
				Network::AnyIPAddress,
				maximumUserCount
			}
		);
		{
			[[maybe_unused]] const bool startedConnecting =
				localClient.Connect(m_clientComponent, Network::Address(MAKE_URI_LITERAL("localhost")));
			Assert(startedConnecting);
		}
	}

	void MainWindow::Close()
	{
		Engine& engine = System::Get<Engine>();
		engine.OnBeforeQuit.Add(
			*this,
			[](MainWindow& window)
			{
				window.Window::Close();
			}
		);
		engine.Quit();
	}
}
