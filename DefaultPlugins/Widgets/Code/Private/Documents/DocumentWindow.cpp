#include <Widgets/Documents/DocumentWindow.h>
#include <Widgets/Documents/DocumentWidget.h>

#include <Engine/Engine.h>
#include <Engine/Project/Project.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Event/EventManager.h>
#include <Engine/Tag/TagRegistry.h>

#include <Renderer/Window/DocumentData.h>

#include <Http/LoadAssetFromNetworkJob.h>

#include <Common/System/Query.h>
#include <Common/Asset/Asset.h>
#include <Common/Asset/AssetOwners.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/IO/FileIterator.h>
#include <Common/Project System/ProjectAssetFormat.h>
#include <Common/Serialization/SerializedData.h>
#include <Common/IO/Log.h>

namespace ngine::Widgets::Document
{
	Threading::JobBatch
	Window::OpenDocuments(const ArrayView<const DocumentData> documents, const EnumFlags<OpenDocumentFlags> openDocumentFlags)
	{
		return OpenDocuments(documents, openDocumentFlags, Invalid);
	}

	Threading::JobBatch Window::OpenDocumentFromPathInternal(
		const IO::Path& documentPath, EnumFlags<OpenDocumentFlags> openDocumentFlags, const Optional<Widget*> pDocumentWidget
	)
	{
		Engine& engine = System::Get<Engine>();
		Asset::Manager& assetManager = System::Get<Asset::Manager>();

		Asset::Guid assetGuid;

		Asset::Owners assetOwners(documentPath, Asset::Context(EngineInfo(engine.GetInfo())));
		if (assetOwners.m_context.GetProject().IsValid() && System::Get<Project>().GetGuid() != assetOwners.m_context.GetProject()->GetGuid())
		{
			const ProjectInfo& projectInfo = *assetOwners.m_context.GetProject();
			Assert(
				!System::Get<Project>().IsValid() || System::Get<Project>().GetGuid() == projectInfo.GetGuid(),
				"Loading assets from two projects is currently not supported!"
			);

			bool isValid = assetManager.VisitAssetEntry(
				projectInfo.GetGuid(),
				[&projectInfo](const Optional<Asset::DatabaseEntry*> pProjectAssetEntry)
				{
					if (pProjectAssetEntry.IsValid())
					{
						pProjectAssetEntry->m_path = projectInfo.GetConfigFilePath();
					}
					return pProjectAssetEntry.IsValid();
				}
			);
			const Asset::Identifier projectRootFolderAssetIdentifier =
				assetManager.FindOrRegisterFolder(projectInfo.GetDirectory(), Asset::Identifier{});
			if (!isValid)
			{
				assetManager.RegisterAsset(projectInfo.GetGuid(), Asset::DatabaseEntry{projectInfo}, projectRootFolderAssetIdentifier);
			}

			if (projectInfo.GetAssetDatabaseGuid().IsValid())
			{
				const IO::Path assetDatabaseConfigPath = IO::Path::Combine(
					projectInfo.GetDirectory(),
					IO::Path::Merge(projectInfo.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
				);
				Asset::Database assetDatabase(assetDatabaseConfigPath, assetDatabaseConfigPath.GetParentPath());
				assetDatabase.IterateAssets(
					[&assetManager](const Asset::Guid assetGuid, const Asset::DatabaseEntry&)
					{
						assetManager.RemoveAsyncLoadCallback(assetGuid);
						return Memory::CallbackResult::Continue;
					}
				);

				isValid = assetManager.VisitAssetEntry(
					projectInfo.GetAssetDatabaseGuid(),
					[&projectInfo](const Optional<Asset::DatabaseEntry*> pProjectAssetDatabaseEntry)
					{
						if (pProjectAssetDatabaseEntry.IsValid())
						{
							pProjectAssetDatabaseEntry->m_path = IO::Path::Combine(
								projectInfo.GetDirectory(),
								IO::Path::Merge(projectInfo.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
							);
						}
						return pProjectAssetDatabaseEntry.IsValid();
					}
				);
				if (!isValid)
				{
					assetManager.RegisterAsset(
						projectInfo.GetAssetDatabaseGuid(),
						Asset::DatabaseEntry{Asset::Database::AssetFormat.assetTypeGuid, Guid{}, IO::Path(assetDatabaseConfigPath)},
						projectRootFolderAssetIdentifier
					);
				}
			}

			if (assetOwners.m_context.GetProject()->GetConfigFilePath() != documentPath)
			{
				const Engine::ProjectLoadResult projectLoadResult = engine.LoadProject(ProjectInfo(projectInfo));
				Assert(projectLoadResult.pProject.IsValid());
				if (LIKELY(projectLoadResult.pProject.IsValid()))
				{
					Threading::IntermediateStage& intermediateStage = Threading::CreateIntermediateStage();
					projectLoadResult.jobBatch.GetFinishedStage().AddSubsequentStage(Threading::CreateCallback(
						[this, documentPath, openDocumentFlags, pDocumentWidget, &intermediateStage](Threading::JobRunnerThread& thread)
						{
							Threading::JobBatch jobBatch = OpenDocumentFromPathInternal(documentPath, openDocumentFlags, pDocumentWidget);
							jobBatch.QueueAsNewFinishedStage(intermediateStage);
							thread.Queue(jobBatch);
						},
						Threading::JobPriority::LoadProject
					));
					return Threading::JobBatch{
						Threading::JobBatch::ManualDependencies,
						projectLoadResult.jobBatch.GetStartStage(),
						intermediateStage
					};
				}
				return projectLoadResult.jobBatch;
			}
		}
		else if (assetOwners.m_context.GetPlugin().IsValid() && !engine.IsPluginLoaded(assetOwners.m_context.GetPlugin()->GetGuid()))
		{
			const Engine::PluginLoadResult pluginLoadResult = engine.LoadPlugin(assetOwners.m_context.GetPlugin()->GetGuid());
			Assert(pluginLoadResult.pPluginInstance.IsValid());
			if (pluginLoadResult.pPluginInstance.IsValid())
			{
				Threading::IntermediateStage& intermediateStage = Threading::CreateIntermediateStage();
				pluginLoadResult.jobBatch.GetFinishedStage().AddSubsequentStage(Threading::CreateCallback(
					[this, documentPath, openDocumentFlags, pDocumentWidget, &intermediateStage](Threading::JobRunnerThread& thread)
					{
						Threading::JobBatch jobBatch = OpenDocumentFromPathInternal(documentPath, openDocumentFlags, pDocumentWidget);
						jobBatch.QueueAsNewFinishedStage(intermediateStage);
						thread.Queue(jobBatch);
					},
					Threading::JobPriority::LoadPlugin
				));
				return Threading::JobBatch{Threading::JobBatch::ManualDependencies, pluginLoadResult.jobBatch.GetStartStage(), intermediateStage};
			}
			return pluginLoadResult.jobBatch;
		}

		const Asset::Identifier rootFolderAssetIdentifier =
			assetManager.FindOrRegisterFolder(assetOwners.GetDatabaseRootDirectory(), Asset::Identifier{});
		if (documentPath.IsDirectory())
		{
			IO::FileIterator::TraverseDirectoryRecursive(
				documentPath,
				[&assetManager, rootFolderAssetIdentifier](IO::Path&& filePath) -> IO::FileIterator::TraversalResult
				{
					const IO::PathView fileExtension = filePath.GetRightMostExtension();

					if (fileExtension == Asset::Asset::FileExtension)
					{
						Serialization::Data assetData(filePath);
						if (assetData.IsValid())
						{
							Asset::Asset asset(assetData, Forward<IO::Path>(filePath));
							if (asset.IsValid())
							{
								const bool isValid = assetManager.VisitAssetEntry(
									asset.GetGuid(),
									[&asset, &assetManager](const Optional<Asset::DatabaseEntry*> pAssetEntry)
									{
										if (pAssetEntry.IsValid())
										{
											// Override the local asset
											*pAssetEntry = Asset::DatabaseEntry{asset};
											assetManager.RemoveAsyncLoadCallback(asset.GetGuid());
										}
										return pAssetEntry.IsValid();
									}
								);
								if (!isValid)
								{
									assetManager.RegisterAsset(asset.GetGuid(), Asset::DatabaseEntry{asset}, rootFolderAssetIdentifier);
								}
							}
						}
					}

					return IO::FileIterator::TraversalResult::Continue;
				}
			);

			IO::Path mainAssetFilePath = IO::Path::Combine(documentPath, IO::Path::Merge(MAKE_PATH("Main"), documentPath.GetAllExtensions()));
			if (!mainAssetFilePath.Exists())
			{
				mainAssetFilePath = IO::Path::Combine(documentPath, documentPath.GetFileName());
			}
			Serialization::Data mainAssetData(mainAssetFilePath);
			if (mainAssetData.IsValid())
			{
				assetGuid = Asset::Asset(mainAssetData, IO::Path(mainAssetFilePath)).GetGuid();
			}
		}
		else
		{
			assetGuid = assetManager.GetAssetGuid(documentPath);
			if (assetGuid.IsInvalid())
			{
				// Attempt to match the guid from the asset file if present
				Serialization::Data assetData(documentPath);
				if (assetData.IsValid())
				{
					Asset::Asset asset(assetData, IO::Path(documentPath));
					assetGuid = asset.GetGuid();
					if (asset.IsValid())
					{
						const bool isValid = assetManager.VisitAssetEntry(
							assetGuid,
							[&assetManager, &asset, assetGuid](const Optional<Asset::DatabaseEntry*> pAssetEntry)
							{
								if (pAssetEntry.IsValid())
								{
									// Override the local asset
									*pAssetEntry = Asset::DatabaseEntry{asset};
									assetManager.RemoveAsyncLoadCallback(assetGuid);
								}
								return pAssetEntry.IsValid();
							}
						);
						if (!isValid)
						{
							assetManager.RegisterAsset(asset.GetGuid(), Asset::DatabaseEntry{asset}, rootFolderAssetIdentifier);
						}
					}
				}
			}
		}

		Assert(assetGuid.IsValid());
		return OpenDocuments(Array{DocumentData{assetGuid}}, openDocumentFlags, pDocumentWidget);
	}

	Threading::JobBatch Window::OpenDocumentFromURIInternal(
		const IO::URI& documentURI, EnumFlags<OpenDocumentFlags> openDocumentFlags, const Optional<Widget*> pDocumentWidget
	)
	{
		IO::URI::StringType uriString{documentURI.GetView().GetStringView()};

		// Remove the engine protocol prefix if it exists
		// Note: Removing // separately as some platforms pre-remove the slashes.
		IO::URI::StringType uriPrefix = IO::URI::StringType::Merge(IO::URI::StringType(EngineInfo::AsciiName), MAKE_URI_LITERAL(":"));
		IO::URI::ConstStringViewType foundRange = uriString.GetView().FindFirstRange(uriPrefix);
		if (foundRange.HasElements())
		{
			uriString.Remove(foundRange);
			foundRange = uriString.GetView().FindFirstRange(MAKE_URI_LITERAL("//"));
			if (foundRange.HasElements())
			{
				uriString.Remove(foundRange);
			}
		}

		IO::URI uri(Move(uriString));

		const IO::Path fileExtensions(IO::Path::StringType(uri.GetAllExtensions().GetStringView()));
		if (fileExtensions == ProjectAssetFormat.metadataFileExtension)
		{
			const Guid projectGuid = Guid::Generate();
			IO::Path targetDirectory =
				IO::Path::Combine(IO::Path::GetTemporaryDirectory(), MAKE_PATH("DownloadedDocuments"), projectGuid.ToString().GetView());
			IO::Path targetFilePath = IO::Path::Combine(targetDirectory, IO::Path(IO::Path::StringType(uri.GetFileName().GetStringView())));

			Threading::IntermediateStage& intermediateStage = Threading::CreateIntermediateStage();
			Networking::HTTP::LoadAssetFromNetworkJob* pDownloadFileJob = new Networking::HTTP::LoadAssetFromNetworkJob(
				projectGuid,
				IO::Path(targetFilePath),
				IO::URI(uri),
				Threading::JobPriority::LoadProject,
				[this, targetFilePath, projectSourceURI = uri, openDocumentFlags, pDocumentWidget, &intermediateStage](const ConstByteView fileData
			  ) mutable
				{
					Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
					if (!fileData.HasElements())
					{
						Assert(false);
						intermediateStage.SignalExecutionFinishedAndDestroying(thread);
						return;
					}

					Serialization::Data assetData = Serialization::Data(
						ConstStringView{reinterpret_cast<const char*>(fileData.GetData()), static_cast<uint32>(fileData.GetDataSize())}
					);
					if (!assetData.IsValid())
					{
						Assert(false);
						OnDocumentOpeningFailed();
						intermediateStage.SignalExecutionFinishedAndDestroying(thread);
						return;
					}

					ProjectInfo projectInfo(assetData, IO::Path(targetFilePath));
					if (!projectInfo.IsValid())
					{
						Assert(false);
						OnDocumentOpeningFailed();
						intermediateStage.SignalExecutionFinishedAndDestroying(thread);
						return;
					}

					const IO::Path localProjectPath(IO::Path::Combine(
						IO::Path::GetApplicationCacheDirectory(),
						MAKE_PATH("CachedNetworkProjects"),
						projectInfo.GetGuid().ToString().GetView()
					));
					localProjectPath.CreateDirectories();

					const IO::Path localProjectConfigPath =
						IO::Path::Combine(localProjectPath, IO::Path(IO::Path::StringType(projectSourceURI.GetFileName().GetStringView())));
					if (!assetData.SaveToFile(localProjectConfigPath, Serialization::SavingFlags{}))
					{
						Assert(false);
						OnDocumentOpeningFailed();
						intermediateStage.SignalExecutionFinishedAndDestroying(thread);
						return;
					}

					projectInfo.SetMetadataFilePath(IO::Path(localProjectConfigPath));

					const IO::URIView projectSourceRootURI = projectSourceURI.GetParentPath();
					const IO::Path localAssetsDatabasePath =
						IO::Path::Combine(localProjectPath, IO::Path::Merge(MAKE_PATH("Assets"), Asset::Database::AssetFormat.metadataFileExtension));

					Networking::HTTP::LoadAssetFromNetworkJob* pDownloadAssetsDatabaseJob = new Networking::HTTP::LoadAssetFromNetworkJob(
						projectInfo.GetAssetDatabaseGuid(),
						IO::Path(localAssetsDatabasePath),
						IO::URI::Combine(
							projectSourceRootURI,
							IO::URI::Merge(MAKE_URI("Assets"), Asset::Database::AssetFormat.metadataFileExtension.GetStringView())
						),
						Threading::JobPriority::LoadProject,
						[this,
				     localProjectPath,
				     projectSourceRootURI = IO::URI(projectSourceRootURI),
				     projectInfo = Move(projectInfo),
				     openDocumentFlags,
				     pDocumentWidget,
				     &intermediateStage](const ConstByteView fileData) mutable
						{
							Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
							if (!fileData.HasElements())
							{
								Assert(false);
								OnDocumentOpeningFailed();
								intermediateStage.SignalExecutionFinishedAndDestroying(thread);
								return;
							}

							Serialization::Data assetData = Serialization::Data(
								ConstStringView{reinterpret_cast<const char*>(fileData.GetData()), static_cast<uint32>(fileData.GetDataSize())}
							);
							if (!assetData.IsValid())
							{
								Assert(false);
								OnDocumentOpeningFailed();
								intermediateStage.SignalExecutionFinishedAndDestroying(thread);
								return;
							}

							Asset::Manager& assetManager = System::Get<Asset::Manager>();

							const Asset::Identifier projectRootFolderAssetIdentifier =
								assetManager.FindOrRegisterFolder(projectInfo.GetDirectory(), Asset::Identifier{});
							assetManager.RegisterAsset(projectInfo.GetGuid(), Asset::DatabaseEntry{projectInfo}, projectRootFolderAssetIdentifier);

							if (projectInfo.GetAssetDatabaseGuid().IsValid())
							{
								assetManager.RegisterAsset(
									projectInfo.GetAssetDatabaseGuid(),
									Asset::DatabaseEntry{
										Asset::Database::AssetFormat.assetTypeGuid,
										ngine::Guid{},
										IO::Path::Combine(
											projectInfo.GetDirectory(),
											IO::Path::Merge(projectInfo.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
										)
									},
									projectRootFolderAssetIdentifier
								);
							}

							Threading::JobBatch jobBatch = OpenDocuments(
								Array<Widgets::DocumentData, 1>{Widgets::DocumentData{projectInfo.GetGuid()}}.GetView(),
								openDocumentFlags,
								pDocumentWidget
							);
							jobBatch.QueueAsNewFinishedStage(intermediateStage);
							thread.Queue(jobBatch);
						}
					);
					pDownloadAssetsDatabaseJob->Queue(thread);
				}
			);
			return Threading::JobBatch{Threading::JobBatch::ManualDependencies, *pDownloadFileJob, intermediateStage};
		}
		else if (const Optional<Network::Address> address = Network::Address(uri))
		{
			return OpenDocumentFromAddressInternal(*address, openDocumentFlags, pDocumentWidget);
		}
		else
		{
			// No other types currently supported
			// Technically we could support assets, but we would need to traverse the parent paths to find a project
			// And it's not a super common case so let's ignore it for now.
			Assert(false);
			OnDocumentOpeningFailed();
			return {};
		}
	}

	Threading::JobBatch Window::OpenDocumentFromAddressInternal(
		const Network::Address address, EnumFlags<OpenDocumentFlags> openDocumentFlags, const Optional<Widget*> pDocumentWidget
	)
	{
		return OpenDocumentFromAddress(address, openDocumentFlags, pDocumentWidget);
	}

	Threading::JobBatch Window::OpenDocuments(
		const ArrayView<const DocumentData> documents, EnumFlags<OpenDocumentFlags> openDocumentFlags, Optional<Widget*> pDocumentWidget
	)
	{
		Asset::Manager& assetManager = System::Get<Asset::Manager>();

		Threading::JobBatch jobBatch{Threading::JobBatch::IntermediateStage};

		for (const DocumentData& document : documents)
		{
			// Attempt to create the document widget as early as possible
			// May fail if we don't have enough information to detect the type of widget. We'll try again later.
			if (pDocumentWidget.IsInvalid())
			{
				pDocumentWidget = CreateDocumentWidget(document, {}, openDocumentFlags);
			}

			Asset::Guid assetGuid;
			DocumentData::Reference projectReference = document.m_projectReference;

			if (!document.Visit(
						[this, openDocumentFlags, pDocumentWidget, &jobBatch](const IO::Path& documentAssetPath) -> bool
						{
							jobBatch.QueueAfterStartStage(OpenDocumentFromPathInternal(documentAssetPath, openDocumentFlags, pDocumentWidget));
							return false;
						},
						[this, &openDocumentFlags, pDocumentWidget, &assetManager, &jobBatch, &assetGuid, &projectReference](
							const IO::URI& documentAssetURI
						) mutable -> bool
						{
							if (const IO::ConstURIView requestedProject =
				            documentAssetURI.GetView().GetQueryParameterValue(IO::ConstURIView{MAKE_URI_LITERAL("project")});
				          requestedProject.HasElements())
							{
								const IO::ConstURIView requestedDocument =
									documentAssetURI.GetView().GetQueryParameterValue(IO::ConstURIView{MAKE_URI_LITERAL("document")});

								const Asset::Guid projectGuid = Guid::TryParse(requestedProject.GetStringView());
								const Asset::Guid documentGuid = Guid::TryParse(requestedDocument.GetStringView());
								const bool enableEditing = documentAssetURI.GetView().HasQueryParameter(IO::ConstURIView{MAKE_URI_LITERAL("edit")});

								Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

								Guid editableLocalProjectTagGuid = "99aca0e9-ef55-405d-8988-94165b912a08"_tag;
								const Tag::Identifier editableLocalProjectTagIdentifier = tagRegistry.FindOrRegister(editableLocalProjectTagGuid);
								const Asset::Identifier projectAssetIdentifier = assetManager.GetAssetIdentifier(projectGuid);
								const bool isExistingLocalProject = assetManager.IsTagSet(editableLocalProjectTagIdentifier, projectAssetIdentifier);

								openDocumentFlags = enableEditing ? (isExistingLocalProject ? OpenDocumentFlags::EnableEditing
					                                                                  : (OpenDocumentFlags::EnableEditing | OpenDocumentFlags::Clone))
					                                        : EnumFlags<OpenDocumentFlags>{};

								if (documentGuid.IsValid())
								{
									assetGuid = documentGuid;
									projectReference = projectGuid;
								}
								else
								{
									assetGuid = projectGuid;
								}
								return true;
							}
							else if (const IO::ConstURIView requestedDocument =
				                 documentAssetURI.GetView().GetQueryParameterValue(IO::ConstURIView{MAKE_URI_LITERAL("document")});
				               requestedDocument.HasElements())
							{
								const Asset::Guid documentGuid = Guid::TryParse(requestedDocument.GetStringView());
								const bool enableEditing = documentAssetURI.GetView().HasQueryParameter(IO::ConstURIView{MAKE_URI_LITERAL("edit")});
								openDocumentFlags = (Rendering::Window::OpenDocumentFlags::EnableEditing | Rendering::Window::OpenDocumentFlags::Clone) *
					                          enableEditing;
								assetGuid = documentGuid;
								return true;
							}
							else if (const IO::ConstURIView requestedSession =
				                 documentAssetURI.GetView().GetQueryParameterValue(IO::ConstURIView{MAKE_URI_LITERAL("remote_session")});
				               requestedSession.HasElements())
							{
								if (const Optional<Network::Address> address = Network::Address(IO::URI::StringType(requestedSession.GetStringView())))
								{
									jobBatch.QueueAfterStartStage(OpenDocumentFromAddressInternal(*address, openDocumentFlags, pDocumentWidget));
								}
								return false;
							}
							else if (const IO::ConstURIView requestedEvent =
				                 documentAssetURI.GetView().GetQueryParameterValue(IO::ConstURIView{MAKE_URI_LITERAL("event")});
				               requestedEvent.HasElements())
							{
								uint32 numberOfArguments = 0;

								ConstStringView firstArgumentValue;
								ConstStringView secondArgumentValue;

								if (const IO::ConstURIView tokenView =
					            documentAssetURI.GetView().GetQueryParameterValue(IO::ConstURIView(MAKE_URI_LITERAL("token")));
					          tokenView.HasElements())
								{
									firstArgumentValue = tokenView.GetStringView();
									numberOfArguments++;
								}

								if (const IO::ConstURIView userIdView =
					            documentAssetURI.GetView().GetQueryParameterValue(IO::ConstURIView(MAKE_URI_LITERAL("user_id")));
					          userIdView.HasElements())
								{
									secondArgumentValue = userIdView.GetStringView();
									numberOfArguments++;
								}

								const Guid eventGuid = Guid::TryParse(requestedEvent.GetStringView());

								if (eventGuid.IsValid())
								{
									UnicodeString firstArgValueUnicode{firstArgumentValue};
									UnicodeString secondArgValueUnicode{secondArgumentValue};
									ngine::Events::Manager& eventsManager = System::Get<ngine::Events::Manager>();

									switch (numberOfArguments)
									{
										case 0:
											eventsManager.NotifyAll(eventsManager.FindOrRegisterEvent(eventGuid));
											break;
										case 1:
											eventsManager.NotifyAll(eventsManager.FindOrRegisterEvent(eventGuid), firstArgValueUnicode);
											break;
										case 2:
											eventsManager.NotifyAll(eventsManager.FindOrRegisterEvent(eventGuid), firstArgValueUnicode, secondArgValueUnicode);
											break;
									}
								}
								return false;
							}
							else if (documentAssetURI.GetView().GetStringView().StartsWith(MAKE_URI_LITERAL("profile/")))
							{
								Assert(false, "TODO");
								return false;
							}
							else
							{
								jobBatch.QueueAfterStartStage(OpenDocumentFromURIInternal(documentAssetURI, openDocumentFlags, pDocumentWidget));
								return false;
							}
						},
						[&assetGuid](const Asset::Guid documentAssetGuid) -> bool
						{
							assetGuid = documentAssetGuid;
							return true;
						},
						[&assetGuid, &assetManager](const Asset::Identifier documentAssetIdentifier) -> bool
						{
							assetGuid = assetManager.GetAssetGuid(documentAssetIdentifier);
							return true;
						},
						[this, openDocumentFlags, pDocumentWidget, &jobBatch](const Network::Address address) -> bool
						{
							jobBatch.QueueAfterStartStage(OpenDocumentFromAddressInternal(address, openDocumentFlags, pDocumentWidget));
							return false;
						},
						[]() -> bool
						{
							ExpectUnreachable();
						}
					))
			{
				continue;
			}

			struct AssetInfo
			{
				IO::Path path;
				Guid assetTypeGuid;
			};
			const AssetInfo assetInfo = assetManager.VisitAssetEntry(
				assetGuid,
				[](const Optional<const Asset::DatabaseEntry*> pAssetEntry) -> AssetInfo
				{
					if (pAssetEntry.IsValid())
					{
						return AssetInfo{pAssetEntry->m_path, pAssetEntry->m_assetTypeGuid};
					}
					else
					{
						return AssetInfo{};
					}
				}
			);

			if (assetInfo.path.HasElements())
			{
				DocumentData newDocument{DocumentData::Reference{assetGuid}, DocumentData::Reference{projectReference}};

				// Attempt to create the document widget again
				if (pDocumentWidget.IsInvalid())
				{
					pDocumentWidget = CreateDocumentWidget(newDocument, assetInfo.assetTypeGuid, openDocumentFlags);
					if (UNLIKELY_ERROR(pDocumentWidget.IsInvalid()))
					{
						LogMessage("Failed to open document as a widget document type could not be found!");
						OnDocumentOpeningFailed();
						continue;
					}
				}

				Threading::IntermediateStage& intermediateStage = Threading::CreateIntermediateStage();
				intermediateStage.AddSubsequentStage(jobBatch.GetFinishedStage());

				Threading::Job* pLoadMetadataJob = assetManager.RequestAsyncLoadAssetMetadata(
					assetGuid,
					Threading::JobPriority::LoadProject,
					[this, document = Move(newDocument), openDocumentFlags, &documentWidget = *pDocumentWidget, &intermediateStage](
						const ConstByteView data
					) mutable
					{
						const Asset::Guid assetGuid = *document.Get<Asset::Guid>();

						Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
						if (UNLIKELY(!data.HasElements()))
						{
							LogWarning("Asset data was empty when opening document asset {0}!", assetGuid.ToString());
							OnDocumentOpeningFailed();
							intermediateStage.SignalExecutionFinishedAndDestroying(thread);
							return;
						}

						Serialization::Data assetData(
							ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
						);
						if (UNLIKELY(!assetData.IsValid()))
						{
							LogWarning("Asset data was invalid when loading asset {0}!", assetGuid.ToString());
							OnDocumentOpeningFailed();
							intermediateStage.SignalExecutionFinishedAndDestroying(thread);
							return;
						}

						Asset::Manager& assetManager = System::Get<Asset::Manager>();
						Optional<Asset::DatabaseEntry> assetEntry = assetManager.VisitAssetEntry(
							assetGuid,
							[](const Optional<const Asset::DatabaseEntry*> pAssetEntry) -> Optional<Asset::DatabaseEntry>
							{
								return pAssetEntry.IsValid() ? *pAssetEntry : Optional<Asset::DatabaseEntry>{Invalid};
							}
						);
						Assert(assetEntry.IsValid());
						if (assetEntry.IsValid())
						{
							Threading::JobBatch jobBatch = documentWidget.OpenDocumentAssetInternal(document, assetData, *assetEntry, openDocumentFlags);
							jobBatch.QueueAsNewFinishedStage(intermediateStage);
							thread.Queue(jobBatch);
						}
						else
						{
							LogWarning("Failed to find asset {}", assetGuid.ToString());
							OnDocumentOpeningFailed();
							intermediateStage.SignalExecutionFinishedAndDestroying(thread);
						}
					}
				);
				if (pLoadMetadataJob != nullptr)
				{
					jobBatch.QueueAfterStartStage(*pLoadMetadataJob);
				}
			}
			else if (openDocumentFlags.IsNotSet(OpenDocumentFlags::SkipResolveMissingAssets))
			{
				jobBatch.QueueAfterStartStage(ResolveMissingAssetDocument(
					assetGuid,
					projectReference.HasValue() ? projectReference.GetExpected<Asset::Guid>() : Asset::Guid{},
					openDocumentFlags | OpenDocumentFlags::SkipResolveMissingAssets,
					pDocumentWidget
				));
			}
			else
			{
				LogWarning("Failed to find asset {}", assetGuid.ToString());
				OnDocumentOpeningFailed();
			}
		}

		return jobBatch;
	}

	Asset::Identifier Window::CreateDocument(
		const Guid assetTypeGuid,
		UnicodeString&& documentName,
		IO::Path&& documentPath,
		const EnumFlags<CreateDocumentFlags> createDocumentFlags,
		const Optional<const DocumentData*> pTemplateDocument
	)
	{
		return CreateDocument(
			assetTypeGuid,
			Forward<UnicodeString>(documentName),
			Forward<IO::Path>(documentPath),
			createDocumentFlags,
			Invalid,
			pTemplateDocument
		);
	}

	Asset::Identifier Window::CreateDocument(
		const Guid assetTypeGuid,
		UnicodeString&& documentName,
		IO::Path&& documentPath,
		const EnumFlags<CreateDocumentFlags> createDocumentFlags,
		Optional<Widget*> pDocumentWidget,
		const Optional<const DocumentData*> pTemplateDocument
	)
	{
		// Attempt to create the document widget as early as possible
		// May fail if we don't have enough information to detect the type of widget. We'll try again later.
		if (createDocumentFlags.IsSet(CreateDocumentFlags::Open) & pDocumentWidget.IsInvalid())
		{
			pDocumentWidget = CreateDocumentWidget(DocumentData{}, assetTypeGuid, OpenDocumentFlags::EnableEditing);
		}

		const Asset::Identifier assetIdentifier =
			CreateAssetDocumentInternal(assetTypeGuid, Forward<UnicodeString>(documentName), Forward<IO::Path>(documentPath), pTemplateDocument);
		if (createDocumentFlags.IsSet(CreateDocumentFlags::Open))
		{
			Assert(assetIdentifier.IsValid());
			if (LIKELY(assetIdentifier.IsValid()))
			{
				// Try to create the document widget again if necessary
				if (pDocumentWidget.IsInvalid())
				{
					pDocumentWidget = CreateDocumentWidget(DocumentData{assetIdentifier}, assetTypeGuid, OpenDocumentFlags::EnableEditing);
				}

				Assert(pDocumentWidget.IsValid());
				if (LIKELY(pDocumentWidget.IsValid()))
				{
					Threading::JobBatch jobBatch =
						OpenDocuments(Array{DocumentData{assetIdentifier}}, OpenDocumentFlags::EnableEditing, pDocumentWidget);
					Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
				}
			}
		}
		return assetIdentifier;
	}

	Threading::JobBatch Window::ResolveMissingAssetDocument(
		const Asset::Guid assetGuid,
		const Asset::Guid parentAssetGuid,
		const EnumFlags<OpenDocumentFlags> openDocumentFlags,
		Optional<Widget*> pDocumentWidget
	)
	{
		UNUSED(assetGuid);
		UNUSED(parentAssetGuid);
		UNUSED(openDocumentFlags);
		UNUSED(pDocumentWidget);
		return {};
	}
}
