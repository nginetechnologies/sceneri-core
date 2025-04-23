#include "PlayViewMode.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/InputComponent.h>
#include <Engine/Input/Actions/ActionMonitor.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Window/Window.h>

#include <Widgets/Widget.h>
#include <Widgets/WidgetFlags.h>
#include <Widgets/ToolWindow.h>
#include <Widgets/Documents/SceneDocument3D.h>

#include <Engine/Context/Utils.h>
#include <Engine/DataSource/PropertyValue.h>
#include <Engine/Entity/ComponentValue.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Serialization/ComponentValue.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/Devices/Mouse/Mouse.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Context/EventManager.inl>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Project/Project.h>

#include <Renderer/Window/Window.h>

#include <Components/SpawnPoint.h>
#include <Tags.h>

#include <Components/SceneRules/SceneRules.h>

#include <PhysicsCore/Components/Data/SceneComponent.h>
#include <PhysicsCore/Plugin.h>

#include <GameFramework/Plugin.h>

#include <Backend/Plugin.h>

#include <Analytics/Plugin.h>

#include <AudioCore/Components/SoundListenerComponent.h>
#include <Renderer/Window/DocumentData.h>

#include <NetworkingCore/Components/LocalClientComponent.h>

#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Asset/Format/Guid.h>
#include <Common/Project System/ProjectAssetFormat.h>
#include <Common/Network/Format/Address.h>

namespace ngine::GameFramework
{
	PlayViewModeBase::PlayViewModeBase(
		Widgets::Document::Scene3D& sceneWidget,
		const ClientIdentifier clientIdentifier,
		const PropertySource::Identifier propertySourceIdentifier
	)
		: Interface(propertySourceIdentifier)
		, m_sceneWidget(&sceneWidget)
		, m_clientIdentifier(clientIdentifier)
	{
		m_showCursorAction.OnStart.Bind(*this, &PlayViewModeBase::OnShowCursor);
		m_clickViewAction.OnStart.Bind(*this, &PlayViewModeBase::OnHideCursor);
		m_pauseAction.OnStart.Bind(*this, &PlayViewModeBase::OnPause);
	}

	PlayViewModeBase::~PlayViewModeBase()
	{
		UnsubscribeFromUIEvents();
	}

	void PlayViewModeBase::LoadWidget(const Asset::Guid widgetAssetGuid)
	{
		Assert(m_pPlayModeWidget.IsInvalid(), "Should only load one play mode widget at the time!");
		// Deserialize the play mode viewport
		Threading::JobBatch jobBatch = Widgets::Widget::Deserialize(
			widgetAssetGuid,
			m_sceneWidget->GetSceneRegistry(),
			m_sceneWidget,
			[this]([[maybe_unused]] const Optional<Widgets::Widget*> pPlayModeWidget)
			{
				m_pPlayModeWidget = pPlayModeWidget;
				if (Ensure(pPlayModeWidget.IsValid()))
				{
					pPlayModeWidget->Hide();
				}
				OnWidgetLoaded();
			},
			Invalid
		);
		if (jobBatch.IsValid())
		{
			System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::UserInterfaceAction);
		}
	}

	PropertySource::PropertyValue PlayViewModeBase::GetDataProperty(const DataSource::PropertyIdentifier identifier) const
	{
		if (identifier == m_exitGameplayTextPropertyIdentifier)
		{
			Assert(m_sceneWidget.IsValid());
			if (LIKELY(m_sceneWidget.IsValid()))
			{
				if (m_sceneWidget->IsSceneEditable())
				{
					return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Back to Remixing"));
				}
				else
				{
					return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Back to Explore"));
				}
			}
		}
		else if (identifier == m_remixTextPropertyIdentifier)
		{
			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
			Asset::Manager& assetManager = System::Get<Asset::Manager>();

			const Optional<Scene*> pScene = m_sceneWidget.IsValid() ? m_sceneWidget->GetSceneView().GetSceneChecked() : nullptr;
			const Asset::Guid sceneAssetGuid = pScene.IsValid() ? pScene->GetGuid() : Asset::Guid{};

			const Tag::Identifier editableLocalProjectTagIdentifier =
				tagRegistry.FindOrRegister(Networking::Backend::Tags::EditableLocalProjectTagGuid);
			const Tag::Identifier localCreatorTagIdentifier =
				tagRegistry.FindOrRegister(Networking::Backend::DataSource::Players::LocalPlayerTagGuid);

			const Asset::Identifier assetIdentifier = assetManager.GetAssetIdentifier(sceneAssetGuid);

			const bool isExistingLocalProject = assetIdentifier.IsValid() &&
			                                    assetManager.IsTagSet(editableLocalProjectTagIdentifier, assetIdentifier);
			const bool isLocalCreatorProject = assetIdentifier.IsValid() && assetManager.IsTagSet(localCreatorTagIdentifier, assetIdentifier);

			if (isExistingLocalProject | isLocalCreatorProject | m_sceneWidget->IsSceneEditable())
			{
				return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Edit"));
			}
			else
			{
				return ConstUnicodeStringView(MAKE_UNICODE_LITERAL("Remix"));
			}
		}
		else if (identifier == m_remixIconPropertyIdentifier)
		{
			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
			Asset::Manager& assetManager = System::Get<Asset::Manager>();

			const Optional<Scene*> pScene = m_sceneWidget.IsValid() ? m_sceneWidget->GetSceneView().GetSceneChecked() : nullptr;
			const Asset::Guid sceneAssetGuid = pScene.IsValid() ? pScene->GetGuid() : Asset::Guid{};

			const Tag::Identifier editableLocalProjectTagIdentifier =
				tagRegistry.FindOrRegister(Networking::Backend::Tags::EditableLocalProjectTagGuid);
			const Tag::Identifier localCreatorTagIdentifier =
				tagRegistry.FindOrRegister(Networking::Backend::DataSource::Players::LocalPlayerTagGuid);

			const Asset::Identifier assetIdentifier = assetManager.GetAssetIdentifier(sceneAssetGuid);

			const bool isExistingLocalProject = assetIdentifier.IsValid() &&
			                                    assetManager.IsTagSet(editableLocalProjectTagIdentifier, assetIdentifier);
			const bool isLocalCreatorProject = assetIdentifier.IsValid() && assetManager.IsTagSet(localCreatorTagIdentifier, assetIdentifier);

			if (isExistingLocalProject | isLocalCreatorProject | m_sceneWidget->IsSceneEditable())
			{
				// Edit
				return "c5bd0ad1-b27a-17e0-782c-68bbe391d890"_asset;
			}
			else
			{
				// Remix
				return "da1db5c7-8409-8da0-72ea-a7e840842bc8"_asset;
			}
		}
		else if (identifier == m_isRemixingPropertyIdentifier)
		{
			return m_sceneWidget.IsValid() && m_sceneWidget->IsSceneEditable();
		}
		else if (identifier == m_isNotRemixingPropertyIdentifier)
		{
			return m_sceneWidget.IsValid() && !m_sceneWidget->IsSceneEditable();
		}

		return {};
	}

	inline static constexpr Guid OnGameplayResumedEventGuid = "c44018ad-4a9e-4674-adc4-afec4bfed5e2"_guid;
	inline static constexpr Guid OnGameplayPausedEventGuid = "77f8861f-ddf7-498a-94b0-96484ac8e6dc"_guid;

	void PlayViewModeBase::OnRestartGameplayAction()
	{
		Context::EventManager eventManager(*m_sceneWidget, m_sceneWidget->GetSceneRegistry());
		eventManager.Notify(OnGameplayResumedEventGuid);

		if (Optional<SceneRules*> pSceneRules = SceneRules::Find(m_pSceneView->GetScene().GetRootComponent());
		    pSceneRules.IsValid() && m_clientIdentifier.IsValid())
		{
			pSceneRules->OnLocalPlayerRequestRestart(m_clientIdentifier);
		}

		HideCursor();
	}

	void PlayViewModeBase::OnResumeGameplayAction()
	{
		if (IsActive())
		{
			Context::EventManager eventManager(*m_sceneWidget, m_sceneWidget->GetSceneRegistry());
			eventManager.Notify(OnGameplayResumedEventGuid);

			if (Optional<SceneRules*> pSceneRules = SceneRules::Find(m_pSceneView->GetScene().GetRootComponent());
			    pSceneRules.IsValid() && m_clientIdentifier.IsValid())
			{
				pSceneRules->OnLocalPlayerRequestResume(m_clientIdentifier);
			}

			HideCursor();
		}
	}

	void PlayViewModeBase::OnPauseGameplayAction()
	{
		if (IsActive())
		{
			Context::EventManager eventManager(*m_sceneWidget, m_sceneWidget->GetSceneRegistry());
			eventManager.Notify(OnGameplayPausedEventGuid);

			if (Optional<SceneRules*> pSceneRules = SceneRules::Find(m_pSceneView->GetScene().GetRootComponent());
			    pSceneRules.IsValid() && m_clientIdentifier.IsValid())
			{
				pSceneRules->OnLocalPlayerRequestPause(m_clientIdentifier);
			}

			ShowCursor();
		}
	}

	void PlayViewModeBase::OnWidgetLoaded()
	{
		if (IsActive())
		{
			m_pPlayModeWidget->MakeVisible();

			Context::EventManager eventManager(*m_sceneWidget, m_sceneWidget->GetSceneRegistry());
			Guid eventId = m_sceneWidget->IsSceneEditable() ? "3420e96f-316b-416e-b39c-f81b1d235791"_guid
			                                                : "08a7b351-4f3b-4c04-b7f9-5006bf6dcd6e"_guid;
			eventManager.Notify(eventId);
		}
	}

	void PlayViewModeBase::OnActivated(Rendering::SceneViewBase& sceneView)
	{
		Assert(m_clientIdentifier.IsValid());

		m_pSceneView = &static_cast<Rendering::SceneView&>(sceneView);

		PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
		playerManager.AddPlayer(m_clientIdentifier, m_pSceneView);

		HideCursor();

		// Register input
		const Optional<Input::Monitor*> pInputMonitor = GetInputMonitorInternal();
		Assert(pInputMonitor.IsValid());
		if (LIKELY(pInputMonitor.IsValid()))
		{
			Input::ActionMonitor& actionMonitor = static_cast<Input::ActionMonitor&>(*pInputMonitor);

			const Input::Manager& inputManager = System::Get<Input::Manager>();
			const Input::MouseDeviceType& mouseDeviceType =
				inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
			const Input::KeyboardDeviceType& keyboardDeviceType =
				inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());

			m_showCursorAction.BindInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::LeftAlt));

			m_clickViewAction.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonPressInputIdentifier(Input::MouseButton::Left));
			m_clickViewAction
				.BindInput(actionMonitor, mouseDeviceType, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Left));

			m_pauseAction.BindInput(actionMonitor, keyboardDeviceType, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::Escape));

			// Switch keyboard and gamepad action monitor
			inputManager.IterateDeviceInstances(
				[&actionMonitor, &inputManager](Input::DeviceInstance& instance) -> Memory::CallbackResult
				{
					if (instance.GetTypeIdentifier() == inputManager.GetKeyboardDeviceTypeIdentifier())
					{
						if (instance.GetActiveMonitor() != &actionMonitor)
						{
							instance.SetActiveMonitor(actionMonitor, inputManager.GetDeviceType(inputManager.GetKeyboardDeviceTypeIdentifier()));
						}
					}
					else if (instance.GetTypeIdentifier() == inputManager.GetGamepadDeviceTypeIdentifier())
					{
						if (instance.GetActiveMonitor() != &actionMonitor)
						{
							instance.SetActiveMonitor(actionMonitor, inputManager.GetDeviceType(inputManager.GetGamepadDeviceTypeIdentifier()));
						}
					}
					return Memory::CallbackResult::Continue;
				}
			);
		}

		if (m_pPlayModeWidget.IsValid())
		{
			m_pPlayModeWidget->MakeVisible();

			Context::EventManager eventManager(*m_sceneWidget, m_sceneWidget->GetSceneRegistry());
			Guid eventId = m_sceneWidget->IsSceneEditable() ? "3420e96f-316b-416e-b39c-f81b1d235791"_guid
			                                                : "08a7b351-4f3b-4c04-b7f9-5006bf6dcd6e"_guid;
			eventManager.Notify(eventId);
		}

		SubscribeToUIEvents();

		// TODO: Require a graph on disk, and register the play mode via the project file and / or scene
		// The graph should implement PlayViewModeBase
		// Then add support for properties on the graph
		// Then add a 'Spawn Point Class' property so that the type of spawn point queried can be changed
		// Also add a 'Player Class' property.
	}

	void PlayViewModeBase::OnSceneAssigned(SceneBase& scene, Rendering::SceneViewBase& sceneView)
	{
		Optional<SceneRules*> pSceneRules = SceneRules::Find(scene.GetRootComponent());
		if (pSceneRules.IsValid() && m_clientIdentifier.IsValid())
		{
			pSceneRules->OnLocalPlayerJoined(m_clientIdentifier);
		}

		if (m_pPlayModeWidget != nullptr)
		{
			m_pPlayModeWidget->MakeVisible();

			Context::EventManager eventManager(*m_sceneWidget, m_sceneWidget->GetSceneRegistry());
			Guid eventId = m_sceneWidget->IsSceneEditable() ? "3420e96f-316b-416e-b39c-f81b1d235791"_guid
			                                                : "08a7b351-4f3b-4c04-b7f9-5006bf6dcd6e"_guid;
			eventManager.Notify(eventId);
		}

		if (m_pSpectatorCamera == nullptr)
		{
			Entity::ComponentTypeSceneData<Entity::CameraComponent>& cameraTypeSceneData =
				*scene.GetEntitySceneRegistry().GetOrCreateComponentTypeData<Entity::CameraComponent>();
			Optional<Entity::CameraComponent*> pCamera = cameraTypeSceneData.CreateInstance(
				Entity::CameraComponent::Initializer{Entity::Component3D::Initializer{static_cast<Scene3D&>(scene).GetRootComponent()}}
			);

			// Add listener component so audio is following the editor camera.
			pCamera->CreateDataComponent<Audio::SoundListenerComponent>(
				Entity::Data::Component3D::DynamicInitializer{*pCamera, scene.GetEntitySceneRegistry()}
			);
			m_pSpectatorCamera = pCamera;
		}

		static_cast<Rendering::SceneView&>(sceneView).AssignCamera(*m_pSpectatorCamera);
	}

	void PlayViewModeBase::OnDeactivated(const Optional<SceneBase*> pScene, [[maybe_unused]] Rendering::SceneViewBase& sceneView)
	{
		if (m_pPlayModeWidget.IsValid())
		{
			m_pPlayModeWidget->Destroy(m_pPlayModeWidget->GetSceneRegistry());
			m_pPlayModeWidget = nullptr;
		}

		m_pSceneView = nullptr;

		ShowCursor();

		// Unregister input
		if (const Optional<Input::Monitor*> pInputMonitor = GetInputMonitorInternal())
		{
			Input::ActionMonitor& actionMonitor = static_cast<Input::ActionMonitor&>(*pInputMonitor);

			const Input::Manager& inputManager = System::Get<Input::Manager>();
			const Input::MouseDeviceType& mouseDeviceType =
				inputManager.GetDeviceType<Input::MouseDeviceType>(inputManager.GetMouseDeviceTypeIdentifier());
			const Input::KeyboardDeviceType& keyboardDeviceType =
				inputManager.GetDeviceType<Input::KeyboardDeviceType>(inputManager.GetKeyboardDeviceTypeIdentifier());

			m_showCursorAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::LeftAlt));

			m_clickViewAction.UnbindInput(actionMonitor, mouseDeviceType.GetButtonPressInputIdentifier(Input::MouseButton::Left));
			m_clickViewAction.UnbindInput(actionMonitor, mouseDeviceType.GetButtonReleaseInputIdentifier(Input::MouseButton::Left));

			m_pauseAction.UnbindInput(actionMonitor, keyboardDeviceType.GetInputIdentifier(Input::KeyboardInput::Escape));
		}

		UnsubscribeFromUIEvents();

		if (pScene.IsValid())
		{
			Optional<SceneRules*> pSceneRules = GameFramework::SceneRules::Find(pScene->GetRootComponent());
			if (pSceneRules.IsValid() && m_clientIdentifier.IsValid())
			{
				pSceneRules->OnLocalPlayerLeft(m_clientIdentifier);
			}
		}

		if (m_pSpectatorCamera != nullptr)
		{
			m_pSpectatorCamera->Destroy(m_pSpectatorCamera->GetSceneRegistry());
			m_pSpectatorCamera = nullptr;
		}

		if (m_clientIdentifier.IsValid())
		{
			PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
			playerManager.RemovePlayer(m_clientIdentifier);
		}
	}

	void PlayViewModeBase::OnSceneUnloading(SceneBase& scene, Rendering::SceneViewBase&)
	{
		Optional<SceneRules*> pSceneRules = SceneRules::Find(scene.GetRootComponent());
		if (pSceneRules.IsValid() && m_clientIdentifier.IsValid())
		{
			pSceneRules->OnLocalPlayerLeft(m_clientIdentifier);
		}

		if (m_clientIdentifier.IsValid())
		{
			PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
			playerManager.RemovePlayer(m_clientIdentifier);
		}

		m_pSpectatorCamera = nullptr;
	}

	void PlayViewModeBase::OnSceneLoaded(SceneBase& scene, Rendering::SceneViewBase&)
	{
		if (Optional<SceneRules*> pSceneRules = SceneRules::Find(scene.GetRootComponent());
		    pSceneRules.IsValid() && m_clientIdentifier.IsValid())
		{
			pSceneRules->OnLocalPlayerLoadedScene(m_clientIdentifier);
		}
		else
		{
			Context::EventManager eventManager(m_sceneWidget->GetRootWidget(), m_sceneWidget->GetSceneRegistry());
			// Hide the loading screen
			eventManager.Notify("ea5387e5-a4ca-4ab8-afff-061fab27ca5d"_guid);
		}
	}

	void PlayViewModeBase::OnShowCursor(const Input::DeviceIdentifier)
	{
		ShowCursor();
	}

	void PlayViewModeBase::OnHideCursor(const Input::DeviceIdentifier)
	{
		HideCursor();
	}

	void PlayViewModeBase::ShowCursor()
	{
		if (m_pSceneView != nullptr)
		{
			if (Optional<Rendering::Window*> pWindow = m_pSceneView->GetOutput().GetWindow())
			{
				pWindow->SetCursorLockPosition(false);
				// pWindow->SetCursorVisibility(true);
				// pWindow->SetCursorConstrainedToWindow(false);
			}
		}
	}

	void PlayViewModeBase::HideCursor()
	{
		if (m_pSceneView != nullptr)
		{
			if (Optional<Rendering::Window*> pWindow = m_pSceneView->GetOutput().GetWindow())
			{
				pWindow->SetCursorLockPosition(true);
				// pWindow->SetCursorConstrainedToWindow(true);
				// pWindow->SetCursorVisibility(false);
			}
		}
	}

	Optional<Input::Monitor*> PlayViewModeBase::GetInputMonitorInternal() const
	{
		if (m_clientIdentifier.IsValid())
		{
			PlayerManager& playerManager = System::FindPlugin<GameFramework::Plugin>()->GetPlayerManager();
			if (const Optional<PlayerInfo*> pPlayerInfo = playerManager.FindPlayerInfo(m_clientIdentifier))
			{
				return pPlayerInfo->GetInputMonitor();
			}
		}

		return Invalid;
	}

	Optional<Input::Monitor*> PlayViewModeBase::GetInputMonitor() const
	{
		Optional<Input::Monitor*> pMonitor = GetInputMonitorInternal();
		if (m_pSceneView.IsValid())
		{
			if (const Optional<Scene*> pScene = m_pSceneView->GetSceneChecked())
			{
				Optional<SceneRules*> pSceneRules = GameFramework::SceneRules::Find(pScene->GetRootComponent());
				if (pSceneRules.IsInvalid() || !pSceneRules->IsGameplayPaused())
				{
					return pMonitor;
				}
			}

			return Invalid;
		}
		else
		{
			return pMonitor;
		}
	}

	void PlayViewModeBase::SubscribeToUIEvents()
	{
		Context::EventManager eventManager(*m_sceneWidget, m_sceneWidget->GetSceneRegistry());
		eventManager.Subscribe<&PlayViewModeBase::OnRestartGameplayAction>("cb227531-c2a9-44a1-837c-b903bd2cd994"_guid, *this);
		eventManager.Subscribe<&PlayViewModeBase::OnResumeGameplayAction>("6137cf64-bc37-4f06-98a7-e072c13838d4"_guid, *this);
		eventManager.Subscribe<&PlayViewModeBase::OnPauseGameplayAction>("90517357-3887-4a8e-b034-5360a7f3790f"_guid, *this);
		eventManager.Subscribe<&PlayViewModeBase::OnShareAction>("af6312af-4d97-4e05-a8c5-462337c9e892"_guid, *this);
		eventManager.Subscribe<&PlayViewModeBase::OnInviteAction>("2C1A7080-E7BA-460D-91A6-714D3C274034"_guid, *this);
	}

	void PlayViewModeBase::UnsubscribeFromUIEvents()
	{
		Context::EventManager eventManager(*m_sceneWidget, m_sceneWidget->GetSceneRegistry());
		eventManager.Unsubscribe("cb227531-c2a9-44a1-837c-b903bd2cd994"_guid, *this);
		eventManager.Unsubscribe("6137cf64-bc37-4f06-98a7-e072c13838d4"_guid, *this);
		eventManager.Unsubscribe("90517357-3887-4a8e-b034-5360a7f3790f"_guid, *this);
		eventManager.Unsubscribe("af6312af-4d97-4e05-a8c5-462337c9e892"_guid, *this);
		eventManager.Unsubscribe("2C1A7080-E7BA-460D-91A6-714D3C274034"_guid, *this);
	}

	void PlayViewModeBase::OnShareAction(Widgets::Widget& requestingWidget, const ArrayView<const UnicodeString> sources)
	{
		Project& currentProject = System::Get<Project>();
		const IO::PathView projectDirectory = currentProject.GetInfo()->GetDirectory();

		Networking::Analytics::Plugin& analytics = *System::FindPlugin<Networking::Analytics::Plugin>();
		Networking::Backend::Plugin& backend = *System::FindPlugin<Networking::Backend::Plugin>();
		Networking::Backend::Game& backendGameAPI = backend.GetGame();
		Networking::Backend::AssetDatabase& backendAssetDatabase = backendGameAPI.GetAssetDatabase();

		const Guid shareSessionGuid = Guid::Generate();
		const bool isAvailableOnBackend = backendAssetDatabase.VisitEntry(
			currentProject.GetGuid(),
			[&currentProject, &backendGameAPI, &analytics, sources, isEditing = m_sceneWidget->IsSceneEditable(), shareSessionGuid](
				const Optional<Networking::Backend::AssetEntry*> pBackendAssetEntry
			) -> bool
			{
				analytics.SendEvent(
					"shareAsset",
					[&currentProject, &backendGameAPI, pBackendAssetEntry, sources, isEditing, shareSessionGuid](
						Serialization::Writer attributesWriter
					)
					{
						attributesWriter.Serialize("assetTypeId", ProjectAssetFormat.assetTypeGuid);
						attributesWriter.Serialize("assetGuid", currentProject.GetGuid());
						attributesWriter.Serialize("isLocalProduct", !isEditing);
						attributesWriter.Serialize("isLocalProject", isEditing);
						attributesWriter.Serialize("shareSessionId", shareSessionGuid);

						if (pBackendAssetEntry.IsValid())
						{
							attributesWriter.Serialize("creatorId", pBackendAssetEntry->m_creatorId.Get());
							attributesWriter.Serialize("assetId", pBackendAssetEntry->m_id);
						}
						else
						{
							attributesWriter.Serialize("creatorId", backendGameAPI.GetLocalPlayerInternalIdentifier().Get());
						}

						Assert(sources.HasElements());
						if (LIKELY(sources.HasElements()))
						{
							attributesWriter.Serialize("source", sources[0]);
						}

						return true;
					}
				);
				return pBackendAssetEntry.IsValid();
			}
		);

		InlineVector<Widgets::DocumentData, 1> documents;
		if (isAvailableOnBackend)
		{
			IO::URI::StringType parameters;
			parameters.Format("project={}&share={}", (Asset::Guid)currentProject.GetGuid(), (Asset::Guid)shareSessionGuid);
			documents.EmplaceBack(requestingWidget.GetOwningWindow()->CreateShareableUri(IO::URI{Move(parameters)}));
		}
		else
		{
			documents.EmplaceBack(IO::Path(projectDirectory));
		}

		m_sceneWidget->GetOwningWindow()->ShareDocuments(documents, requestingWidget.GetContentArea());
	}

	void PlayViewModeBase::OnInviteAction(Widgets::Widget& requestingWidget, const ArrayView<const UnicodeString> sources)
	{
		const Optional<Network::Session::LocalClient*> pLocalClient = Network::Session::LocalClient::Find(
			m_sceneWidget->GetSceneView().GetScene().GetRootComponent(),
			m_sceneWidget->GetSceneView().GetScene().GetEntitySceneRegistry()
		);
		Assert(pLocalClient.IsValid());
		if (LIKELY(pLocalClient.IsValid()))
		{
			const Network::Address address = pLocalClient->GetPublicHostAddress();
			Assert(address.GetIPAddress().IsValid());
			if (LIKELY(address.GetIPAddress().IsValid()))
			{
				Networking::Analytics::Plugin& analytics = *System::FindPlugin<Networking::Analytics::Plugin>();

				const Guid shareSessionGuid = Guid::Generate();
				analytics.SendEvent(
					"inviteFriend",
					[sources, shareSessionGuid](Serialization::Writer attributesWriter)
					{
						attributesWriter.Serialize("shareSessionId", shareSessionGuid);

						Assert(sources.HasElements());
						if (LIKELY(sources.HasElements()))
						{
							attributesWriter.Serialize("source", sources[0]);
						}

						return true;
					}
				);

				InlineVector<Widgets::DocumentData, 1> documents;
				IO::URI::StringType parameters;
				parameters.Format("remote_session={}&share={}", address, (Asset::Guid)shareSessionGuid);
				documents.EmplaceBack(requestingWidget.GetOwningWindow()->CreateShareableUri(IO::URI{Move(parameters)}));
				m_sceneWidget->GetOwningWindow()->ShareDocuments(documents, requestingWidget.GetContentArea());
			}
		}
	}

	PlayViewMode::PlayViewMode(Widgets::Document::Scene3D& sceneWidget, const ClientIdentifier clientIdentifier)
		: BaseType(
				sceneWidget,
				clientIdentifier,
				System::Get<ngine::DataSource::Cache>().GetPropertySourceCache().Register(
					Context::Utils::GetGuid(DataSourceGuid, sceneWidget, sceneWidget.GetSceneRegistry())
				)
			)
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		PropertySource::Cache& propertySourceCache = dataSourceCache.GetPropertySourceCache();
		propertySourceCache.OnCreated(m_identifier, *this);
		m_exitGameplayTextPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("exit_gameplay_text");
		m_remixTextPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("remix_text");
		m_remixIconPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("remix_icon");
		m_isRemixingPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("is_remixing");
		m_isNotRemixingPropertyIdentifier = dataSourceCache.FindOrRegisterPropertyIdentifier("is_not_remixing");
	}
	PlayViewMode::~PlayViewMode()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.GetPropertySourceCache().Deregister(m_identifier, dataSourceCache.GetPropertySourceCache().FindGuid(m_identifier));
	}

	void PlayViewMode::OnActivated(Rendering::SceneViewBase& sceneView)
	{
		LoadWidget("80df3192-2709-4b6b-860b-384d3097888f"_asset);

		BaseType::OnActivated(sceneView);

		Context::EventManager eventManager(*m_sceneWidget, m_sceneWidget->GetSceneRegistry());
		eventManager.Subscribe<&PlayViewMode::ReturnToExplore>("c3a1fca6-57b5-41fb-b352-78a68daae200"_guid, *this);
	}

	void PlayViewMode::OnDeactivated(const Optional<SceneBase*> pScene, Rendering::SceneViewBase& sceneView)
	{
		BaseType::OnDeactivated(pScene, sceneView);

		Context::EventManager eventManager(*m_sceneWidget, m_sceneWidget->GetSceneRegistry());
		eventManager.Unsubscribe("c3a1fca6-57b5-41fb-b352-78a68daae200"_guid, *this);
	}

	void PlayViewMode::OnPause(const Input::DeviceIdentifier)
	{
		ShowCursor();

		// Show the pause menu
		Context::EventManager eventManager(*m_sceneWidget, m_sceneWidget->GetSceneRegistry());
		eventManager.Notify("adeb86b6-2fc4-4aaa-9a1c-2e430e3f13c5"_guid);
		// Notify gameplay pause
		eventManager.Notify("90517357-3887-4a8e-b034-5360a7f3790f"_guid);
	}

	void PlayViewMode::ReturnToExplore()
	{
		Context::EventManager eventManager(*m_sceneWidget, m_sceneWidget->GetSceneRegistry());
		eventManager.Notify("49aae2b3-9aa9-40c9-accd-18d4431ce507"_guid);
	}
}
