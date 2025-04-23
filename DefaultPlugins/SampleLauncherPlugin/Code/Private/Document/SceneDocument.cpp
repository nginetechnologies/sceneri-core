#include <SamplerLauncher/Document/SceneDocument.h>
#include <Widgets/Documents/DocumentWindow.h>

#include <Engine/Engine.h>
#include <Engine/Scene/Scene.h>
#include <Engine/Scene/Scene3DAssetType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/RootComponent.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Asset/AssetManager.h>

#include <Renderer/Window/DocumentData.h>

#include <Widgets/Style/Entry.h>
#include <Widgets/LoadResourcesResult.h>

#include <GameFramework/PlayViewMode.h>

#include <Backend/Plugin.h>
#include <Backend/LoadAssetFromBackendJob.h>

#include <NetworkingCore/Components/LocalClientComponent.h>
#include <NetworkingCore/Components/ClientComponent.h>

#include <Common/System/Query.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Asset/AssetDatabaseEntry.h>
#include <Common/Project System/ProjectInfo.h>
#include <Common/Project System/ProjectAssetFormat.h>
#include <Common/IO/Log.h>

namespace ngine::App::UI::Document
{
	using namespace Widgets::Literals;

	Scene::Scene(Initializer&& initializer)
		: Widgets::Document::Scene3D(Forward<Initializer>(initializer))
	{
	}

	void Scene::OnCreated()
	{
		BaseType::OnCreated();

		Optional<Network::Session::LocalClient*> pLocalClientComponent = Network::Session::LocalClient::Find(GetParent(), GetSceneRegistry());
		Assert(pLocalClientComponent.IsValid());
		if (LIKELY(pLocalClientComponent.IsValid()))
		{
			m_clientIdentifier = pLocalClientComponent->GetIdentifier();
		}

		Rendering::SceneView& sceneView = GetSceneView();
		GameFramework::PlayViewMode& playMode = sceneView.RegisterMode<GameFramework::PlayViewMode>(*this, m_clientIdentifier);

		sceneView.ChangeMode(playMode);

		Threading::JobBatch jobBatch;
		m_framegraphBuilder.Create(m_sceneView, jobBatch);
		if (jobBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
		}
	}

	Scene::~Scene() = default;

	void Scene::OnFramegraphInvalidated()
	{
		GetSceneView().InvalidateFramegraph();
	}

	void Scene::BuildFramegraph(Rendering::FramegraphBuilder& framegraphBuilder)
	{
		m_framegraphBuilder.Build(framegraphBuilder, m_sceneView);
	}

	void Scene::OnFramegraphBuilt()
	{
		GetSceneView().OnFramegraphBuilt();
	}

	void Scene::OnSceneChanged()
	{
		Rendering::SceneView& sceneView = GetSceneView();
		if (Optional<SceneViewModeBase*> pPlayViewMode = sceneView.FindMode(GameFramework::PlayViewMode::TypeGuid))
		{
			if (sceneView.GetCurrentMode() != pPlayViewMode)
			{
				sceneView.ChangeMode(*pPlayViewMode);
			}
		}
	}

	Asset::Reference Scene::GetDocumentAssetReference() const
	{
		if (const Optional<ngine::Scene3D*> pScene = GetSceneView().GetSceneChecked())
		{
			const Asset::Guid sceneAssetGuid = pScene->GetGuid();
			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			return {sceneAssetGuid, assetManager.GetAssetTypeGuid(sceneAssetGuid)};
		}
		else
		{
			return {};
		}
	}

	ArrayView<const Guid, uint16> Scene::GetSupportedDocumentAssetTypeGuids() const
	{
		constexpr Array supportedDocumentAssetTypeGuids{ProjectAssetFormat.assetTypeGuid, Scene3DAssetType::AssetFormat.assetTypeGuid};
		return supportedDocumentAssetTypeGuids.GetView();
	}

	Threading::JobBatch Scene::OpenDocumentAssetInternal(
		const Widgets::DocumentData& document,
		const Serialization::Data& assetData,
		const Asset::DatabaseEntry& assetEntry,
		const EnumFlags<Widgets::OpenDocumentFlags> openDocumentFlags
	)
	{
		if (assetEntry.m_assetTypeGuid == ProjectAssetFormat.assetTypeGuid)
		{
			return OpenProjectDocumentInternal(*document.Get<Asset::Guid>(), assetData, assetEntry, openDocumentFlags);
		}
		else if (assetEntry.m_componentTypeGuid == Reflection::GetTypeGuid<Entity::RootSceneComponent>())
		{
			Threading::JobBatch sceneLoadJobBatch;

			Entity::DataComponentResult<Network::Session::LocalClient> localClientQueryResult =
				Network::Session::LocalClient::Find(GetParent(), GetSceneRegistry());
			Assert(localClientQueryResult.IsValid());

			Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
			const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(*document.Get<Asset::Guid>());
			UniquePtr<ngine::Scene3D> pScene{
				Memory::ConstructInPlace,
				GetSceneRegistry(),
				localClientQueryResult.m_pDataComponentOwner,
				sceneTemplateIdentifier,
				sceneLoadJobBatch,
				SceneFlags{},
				GetOwningWindow()->GetMaximumScreenRefreshRate()
			};
			ngine::Scene3D& scene = *pScene;

			AssignScene(Move(pScene));

			GetSceneView().Enable();

			if (sceneLoadJobBatch.IsValid())
			{
				sceneLoadJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
					[this, &scene](Threading::JobRunnerThread&) mutable
					{
						OnSceneFinishedLoading(scene);
					},
					Threading::JobPriority::UserInterfaceLoading
				));
			}
			return sceneLoadJobBatch;
		}
		else
		{
			return {};
		}
	}

	void Scene::CloseDocument()
	{
		Assert(false, "TODO");
	}

	Threading::JobBatch
	Scene::OpenProjectDocumentInternal(ProjectInfo&& projectInfo, const EnumFlags<Widgets::OpenDocumentFlags> openDocumentFlags)
	{
		const Asset::Guid defaultSceneAssetGuid = projectInfo.GetDefaultSceneGuid();
		const Engine::ProjectLoadResult projectLoadResult = System::Get<Engine>().LoadProject(Move(projectInfo));
		if (UNLIKELY(projectLoadResult.pProject.IsInvalid()))
		{
			LogError("Failed to load project!");
			return {};
		}
		Threading::IntermediateStage& intermediateStage = Threading::CreateIntermediateStage();
		projectLoadResult.jobBatch.GetFinishedStage().AddSubsequentStage(Threading::CreateCallback(
			[this, defaultSceneAssetGuid, openDocumentFlags, &intermediateStage](Threading::JobRunnerThread& thread) mutable
			{
				Assert(defaultSceneAssetGuid.IsValid());
				Threading::JobBatch jobBatch = static_cast<Widgets::Document::Window&>(*GetOwningWindow())
			                                   .OpenDocuments(
																					 Array<Widgets::DocumentData, 1>{Widgets::DocumentData{defaultSceneAssetGuid}}.GetView(),
																					 openDocumentFlags,
																					 this
																				 );
				jobBatch.QueueAsNewFinishedStage(intermediateStage);
				thread.Queue(jobBatch);
			},
			Threading::JobPriority::LoadProject
		));
		return {Threading::JobBatch::ManualDependencies, projectLoadResult.jobBatch.GetStartStage(), intermediateStage};
	}

	Threading::JobBatch Scene::OpenProjectDocumentInternal(
		const Asset::Guid assetGuid,
		const Serialization::Data& assetData,
		const Asset::DatabaseEntry& assetEntry,
		const EnumFlags<Widgets::OpenDocumentFlags> openDocumentFlags
	)
	{
		ProjectInfo templateProjectInfo(assetData, IO::Path(assetEntry.m_path));

		Asset::Manager& assetManager = System::Get<Asset::Manager>();

		if (!assetManager.HasAsset(templateProjectInfo.GetAssetDatabaseGuid()))
		{
			assetManager.Import(
				Asset::LibraryReference{templateProjectInfo.GetAssetDatabaseGuid(), Asset::Database::AssetFormat.assetTypeGuid},
				Asset::ImportingFlags{}
			);
		}

		Threading::JobBatch jobBatch{
			Threading::JobBatch::ManualDependencies,
			Threading::CreateIntermediateStage(),
			Threading::CreateIntermediateStage()
		};
		Threading::Job*
			pLoadAssetDatabaseJob = assetManager
		                            .RequestAsyncLoadAssetMetadata(
																	templateProjectInfo.GetAssetDatabaseGuid(),
																	Threading::JobPriority::LoadProject,
																	[this,
		                               assetGuid,
		                               openDocumentFlags,
		                               templateProjectInfo = Move(templateProjectInfo),
		                               &intermediateStage = jobBatch.GetFinishedStage()](const ConstByteView data) mutable
																	{
																		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
																		if (UNLIKELY(!data.HasElements()))
																		{
																			LogWarning("Asset data was empty when opening document asset {0}!", assetGuid.ToString());
																			intermediateStage.SignalExecutionFinishedAndDestroying(thread);
																			return;
																		}

																		Serialization::Data assetData(ConstStringView{
																			reinterpret_cast<const char*>(data.GetData()),
																			static_cast<uint32>(data.GetDataSize() / sizeof(char))
																		});
																		if (UNLIKELY(!assetData.IsValid()))
																		{
																			LogWarning("Asset data was invalid when loading asset {0}!", assetGuid.ToString());
																			intermediateStage.SignalExecutionFinishedAndDestroying(thread);
																			return;
																		}

																		Asset::Database templateAssetDatabase(assetData, templateProjectInfo.GetDirectory());

																		Asset::Manager& assetManager = System::Get<Asset::Manager>();
																		const Asset::Identifier projectRootFolderAssetIdentifier =
																			assetManager.FindOrRegisterFolder(templateProjectInfo.GetDirectory(), Asset::Identifier{});
																		// Register assets for async loading
																		templateAssetDatabase.IterateAssets(
																			[&assetManager, projectRootFolderAssetIdentifier](
																				const Asset::Guid assetGuid,
																				const Asset::DatabaseEntry& assetEntry
																			)
																			{
																				if(!assetManager.HasAsset(assetGuid) && !assetManager.GetAssetLibrary().HasAsset(assetGuid) && !assetManager.HasAsyncLoadCallback(assetGuid) && !assetEntry.m_path.Exists())
																				{
																					assetManager.GetAssetLibrary()
																						.RegisterAsset(assetGuid, Asset::DatabaseEntry{assetEntry}, projectRootFolderAssetIdentifier);

																					assetManager.RegisterAsyncLoadCallback(
																						assetGuid,
																						[&gameAPI = System::FindPlugin<Networking::Backend::Plugin>()->GetGame()](
																							const Asset::Guid assetGuid,
																							const IO::PathView path,
																							Threading::JobPriority priority,
																							IO::AsyncLoadCallback&& callback,
																							const ByteView target,
																							const Math::Range<size> dataRange
																						) -> Optional<Threading::Job*>
																						{
																							return new Networking::Backend::LoadAssetJob(
																								gameAPI,
																								assetGuid,
																								IO::Path(path),
																								priority,
																								Forward<IO::AsyncLoadCallback>(callback),
																								target,
																								dataRange
																							);
																						}
																					);
																				}
																				return Memory::CallbackResult::Continue;
																			}
																		);

																		if (const Optional<Networking::Backend::Plugin*> pBackend = System::FindPlugin<Networking::Backend::Plugin>())
																		{
																			// Request all assets from backend
																			Vector<Asset::Guid> assetGuids(Memory::Reserve, templateAssetDatabase.GetImportedAssetCount());
																			templateAssetDatabase.IterateImportedAssets(
																				[&assetGuids, &assetManager, &backend = *pBackend](Asset::Guid assetGuid)
																				{
																					if(!assetManager.HasAsset(assetGuid) && !assetManager.GetAssetLibrary().HasAsset(assetGuid) && !assetManager.HasAsyncLoadCallback(assetGuid))
																					{
																						assetManager.RegisterAsyncLoadCallback(
																							assetGuid,
																							[&gameAPI = backend.GetGame()](
																								const Asset::Guid assetGuid,
																								const IO::PathView path,
																								Threading::JobPriority priority,
																								IO::AsyncLoadCallback&& callback,
																								const ByteView target,
																								const Math::Range<size> dataRange
																							) -> Optional<Threading::Job*>
																							{
																								return new Networking::Backend::LoadAssetJob(
																									gameAPI,
																									assetGuid,
																									IO::Path(path),
																									priority,
																									Forward<IO::AsyncLoadCallback>(callback),
																									target,
																									dataRange
																								);
																							}
																						);
																					}

																					assetGuids.EmplaceBack(assetGuid);
																					return Memory::CallbackResult::Continue;
																				}
																			);
																			Networking::Backend::Game& gameAPI = pBackend->GetGame();

																			gameAPI.RequestAssets(
																				Move(assetGuids),
																				{},
																				[this, templateProjectInfo = Move(templateProjectInfo), openDocumentFlags, &intermediateStage](
																					[[maybe_unused]] const bool responseSuccess
																				) mutable
																				{
																					Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
																					Assert(responseSuccess);
																					Threading::JobBatch jobBatch =
																						OpenProjectDocumentInternal(Move(templateProjectInfo), openDocumentFlags);
																					jobBatch.QueueAsNewFinishedStage(intermediateStage);
																					thread.Queue(jobBatch);
																				}
																			);
																		}
																		else
																		{
																			Threading::JobBatch jobBatch =
																				OpenProjectDocumentInternal(Move(templateProjectInfo), openDocumentFlags);
																			jobBatch.QueueAsNewFinishedStage(intermediateStage);
																			thread.Queue(jobBatch);
																		}
																	}
																);
		if (pLoadAssetDatabaseJob != nullptr)
		{
			jobBatch.GetStartStage().AddSubsequentStage(*pLoadAssetDatabaseJob);
		}
		return jobBatch;
	}

	[[maybe_unused]] const bool wasAppSceneDocumentTypeRegistered = Reflection::Registry::RegisterType<Scene>();
	[[maybe_unused]] const bool wasAppSceneDocumentComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Scene>>::Make());
}
