#include "Asset/AssetManager.h"

#include "Engine.h"
#include "Tag/TagContainer.inl"

#include <Engine/DataSource/PropertyValue.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/IO/Filesystem.h>

#include <Common/Asset/Asset.h>
#include <Common/Asset/TagAssetType.h>
#include <Common/Asset/Guid.h>
#include <Common/Asset/Reference.h>
#include <Common/Asset/LocalAssetDatabase.h>
#include <Common/IO/FileChangeListener.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/IO/Format/Path.h>
#include <Common/Asset/Format/Guid.h>
#include <Common/Asset/FolderAssetType.h>

#include <Common/Serialization/Deserialize.h>
#include <Common/Serialization/Guid.h>
#include <Common/IO/FileIterator.h>

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/RecurringAsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/IO/AsyncLoadFromDiskJob.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Project System/EngineAssetFormat.h>
#include <Common/Project System/PackagedBundle.h>
#include <Common/Project System/FindEngine.h>
#include <Common/IO/Log.h>
#include <Common/System/Query.h>

#include <Renderer/Assets/Texture/TextureAssetType.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>

namespace ngine::Asset
{
#if TRACK_ASSET_FILE_CHANGES
	struct FileChangeDetection
	{
		FileChangeDetection(Manager& manager)
			: m_manager(manager)
		{
		}

#if FILE_CHANGE_LISTENER_REQUIRES_POLLING
		void Poll()
		{
			m_fileChangeListener.CheckChanges();
		}
#endif

		void MonitorDirectory(const IO::Path& directoryPath)
		{
			IO::FileChangeListeners listeners;

			listeners.m_added = [&assetManager = m_manager](const IO::PathView directoryPath, const IO::PathView relativeChangedPath)
			{
				assetManager.NotifyFileAddedOnDisk(directoryPath, relativeChangedPath);
			};

			listeners.m_removed = [&assetManager = m_manager](const IO::PathView directoryPath, const IO::PathView relativeChangedPath)
			{
				assetManager.NotifyFileRemovedOnDisk(directoryPath, relativeChangedPath);
			};

			listeners.m_modified = [&assetManager = m_manager](const IO::PathView directoryPath, const IO::PathView relativeChangedPath)
			{
				assetManager.NotifyFileModifiedOnDisk(directoryPath, relativeChangedPath);
			};

			listeners.m_renamed =
				[&assetManager =
			     m_manager](const IO::PathView directoryPath, const IO::PathView relativePreviousPath, const IO::PathView relativeNewPath)
			{
				assetManager.NotifyFileRenamedOnDisk(directoryPath, relativePreviousPath, relativeNewPath);
			};

			m_fileChangeListener.MonitorDirectory(directoryPath, Move(listeners));
		}
	protected:
		IO::FileChangeListener m_fileChangeListener;
		Manager& m_manager;
	};
#endif

	Manager::Manager()
		: EngineAssetDatabase(DataSourceGuid)
#if TRACK_ASSET_FILE_CHANGES
		, m_pFileChangeDetection(UniqueRef<FileChangeDetection>::Make(*this))
#endif
	{
		System::Query& systemQuery = System::Query::GetInstance();
		systemQuery.RegisterSystem(*this);

#if TRACK_ASSET_FILE_CHANGES && FILE_CHANGE_LISTENER_REQUIRES_POLLING
		if (const Optional<Threading::JobManager*> pJobManager = System::Find<Threading::JobManager>())
		{
			m_scheduledTimerHandle = pJobManager->ScheduleRecurringAsync(
				5_seconds,
				[this]() -> bool
				{
					m_pFileChangeDetection->Poll();
					return true;
				},
				Threading::JobPriority::FileChangeDetection
			);
		}
#endif

		EngineAssetDatabase::InitializePropertyIdentifiers();
	}

	Manager::~Manager()
	{
		System::Get<Log>().OnInitialized.Remove(this);

		System::Query& systemQuery = System::Query::GetInstance();
		systemQuery.DeregisterSystem<Manager>();

#if TRACK_ASSET_FILE_CHANGES && FILE_CHANGE_LISTENER_REQUIRES_POLLING
		if (m_scheduledTimerHandle.IsValid())
		{
			if (const Optional<Threading::JobManager*> pJobManager = System::Find<Threading::JobManager>())
			{
				pJobManager->CancelAsyncJob(m_scheduledTimerHandle);
			}
		}
#endif
	}

	EngineManager::EngineManager()
	{
		System::Get<Log>().Open(IO::Path(IO::Path::StringType(EngineInfo::NativeName)));
	}

	EngineManager::~EngineManager()
	{
		System::Get<Log>().Close();
	}

	Threading::JobBatch EngineManager::LoadDefaultResources()
	{
		Threading::JobBatch jobBatch;
		jobBatch.QueueAfterStartStage(m_assetLibrary.LoadLocalPluginDatabase());

		if constexpr (PACKAGED_BUILD)
		{
			const IO::Path packagedBundlePath = IO::Path::Combine(System::Get<IO::Filesystem>().GetEnginePath(), PackagedBundle::FileName);
			Threading::JobBatch loadBundleBatch = *IO::CreateAsyncLoadFromDiskJob(
				packagedBundlePath,
				Threading::JobPriority::LoadProject,
				[this](const ConstByteView data)
				{
					Serialization::RootReader rootReader = Serialization::GetReaderFromBuffer(
						ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
					);
					if (UNLIKELY(!rootReader.GetData().IsValid()))
					{
						AssertMessage(false, "Failed to load packaged engine bundle!");
						return;
					}

					const Serialization::Reader reader(rootReader);

					IO::Path enginePath;
					if (const Optional<IO::Filesystem*> pFilesystem = System::Find<IO::Filesystem>())
					{
						enginePath = pFilesystem->GetEnginePath();
					}
					else
					{
						enginePath = IO::Path(ProjectSystem::FindEngineDirectoryFromExecutableFolder(IO::Path::GetExecutableDirectory()));
					}

					const Optional<Serialization::Reader> pluginDatabaseReader = reader.FindSerializer("available_plugins");
					Assert(pluginDatabaseReader.IsValid());
					if (LIKELY(pluginDatabaseReader.IsValid()))
					{
						Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

						const Identifier engineRootFolderAssetIdentifier = m_assetLibrary.FindOrRegisterFolder(enginePath, Identifier{});

						const Tag::Identifier pluginAssetTagIdentifier = tagRegistry.FindOrRegister(Library::PluginTagGuid);
						const Tag::Identifier engineAssetTagIdentifier = tagRegistry.FindOrRegister(EngineManager::EngineAssetsTagGuid);

						const IO::Path enginePluginsAssetDatabasePath = IO::Path::Combine(enginePath, EnginePluginDatabase::FileName);

						FlatVector<Tag::Identifier, 2> tags{pluginAssetTagIdentifier, engineAssetTagIdentifier};

						[[maybe_unused]] const bool wasLoaded =
							m_assetLibrary.Load(*pluginDatabaseReader, enginePath, engineRootFolderAssetIdentifier, tags.GetView());
						Assert(wasLoaded);
					}

					Engine& engine = System::Get<Engine>();

					const Optional<Serialization::Reader> engineReader = reader.FindSerializer("engine");
					Assert(engineReader.IsValid());
					if (LIKELY(engineReader.IsValid()))
					{
						engine.m_engineInfo = EngineInfo{
							IO::Path::Combine(enginePath, IO::Path::Merge(EngineInfo::NativeName.GetView(), EngineAssetFormat.metadataFileExtension)),
							*engineReader
						};
					}

					const Optional<Serialization::Reader> assetDatabasesReader = reader.FindSerializer("asset_databases");
					Assert(assetDatabasesReader.IsValid());
					if (LIKELY(assetDatabasesReader.IsValid()))
					{
						for (Serialization::Member<Serialization::Reader> assetDatabaseReader : assetDatabasesReader->GetMemberView())
						{
							IO::Path databaseRootDirectory;
							IO::Path relativePath = assetDatabaseReader.value.ReadWithDefaultValue<IO::Path>("path", IO::Path{});
							if (relativePath.HasElements())
							{
								databaseRootDirectory = IO::Path::Combine(enginePath, relativePath);
								const Identifier rootFolderAssetIdentifier = m_assetLibrary.FindOrRegisterFolder(databaseRootDirectory, Identifier{});
								[[maybe_unused]] const bool wasLoaded =
									m_assetLibrary.Load(assetDatabaseReader.value, databaseRootDirectory, rootFolderAssetIdentifier);
								Assert(wasLoaded);

								const Guid assetDatabaseGuid = *assetDatabaseReader.value.Read<Guid>("guid");
								String jsonData = assetDatabaseReader.value.GetValue().SaveToBuffer<String>();
								RegisterAsyncLoadCallback(
									assetDatabaseGuid,
									[jsonData = Move(jsonData)](
										[[maybe_unused]] const Guid assetGuid,
										[[maybe_unused]] const IO::PathView path,
										const Threading::JobPriority priority,
										IO::AsyncLoadCallback&& callback,
										[[maybe_unused]] const ByteView target,
										[[maybe_unused]] const Math::Range<size> dataRange
									) -> Optional<Threading::Job*>
									{
										return Threading::CreateCallback(
											[callback = Forward<IO::AsyncLoadCallback>(callback), jsonData = jsonData.GetView()](Threading::JobRunnerThread&)
											{
												callback(jsonData);
											},
											priority,
											"Load asset database"
										);
									}
								);
							}
							else
							{
								databaseRootDirectory = enginePath;
								const Identifier rootFolderAssetIdentifier = FindOrRegisterFolder(databaseRootDirectory, Identifier{});
								[[maybe_unused]] const bool wasLoaded = Load(
									IO::Path::Combine(
										databaseRootDirectory,
										IO::Path::Merge(MAKE_PATH("EngineAssets"), Database::AssetFormat.metadataFileExtension)
									),
									assetDatabaseReader.value,
									databaseRootDirectory,
									rootFolderAssetIdentifier
								);
								Assert(wasLoaded);
							}
						}
					}

					const Optional<Serialization::Reader> pluginsReader = reader.FindSerializer("plugins");
					Assert(pluginsReader.IsValid());
					if (LIKELY(pluginsReader.IsValid()))
					{
						for (Serialization::Member<Serialization::Reader> pluginReader : pluginsReader->GetMemberView())
						{
							const Guid pluginGuid = Guid::TryParse(pluginReader.key);
							String jsonData = pluginReader.value.GetValue().SaveToBuffer<String>();
							RegisterAsyncLoadCallback(
								pluginGuid,
								[jsonData = Move(jsonData)](
									[[maybe_unused]] const Guid assetGuid,
									[[maybe_unused]] const IO::PathView path,
									const Threading::JobPriority priority,
									IO::AsyncLoadCallback&& callback,
									[[maybe_unused]] const ByteView target,
									[[maybe_unused]] const Math::Range<size> dataRange
								) -> Optional<Threading::Job*>
								{
									return Threading::CreateCallback(
										[callback = Forward<IO::AsyncLoadCallback>(callback), jsonData = jsonData.GetView()](Threading::JobRunnerThread&)
										{
											callback(jsonData);
										},
										priority,
										"Load Plug-in Metadata"
									);
								}
							);
						}
					}

					if (Optional<ProjectInfo> projectInfo = reader.Read<ProjectInfo>("project"))
					{
						Assert(false, "TODO: Auto load project");
					}
				},
				ByteView{},
				Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
			);
			jobBatch.QueueAfterStartStage(loadBundleBatch);
		}
		else
		{
			{
				const IO::Path assetsDatabasePath = IO::Path::Combine(
					System::Get<IO::Filesystem>().GetEnginePath(),
					IO::Path::Merge(EngineInfo::EngineAssetsPath, Database::AssetFormat.metadataFileExtension)
				);
				Threading::JobBatch loadEngineAssetDatabaseBatch = *IO::CreateAsyncLoadFromDiskJob(
					assetsDatabasePath,
					Threading::JobPriority::LoadProject,
					[this, assetsDatabasePath](const ConstByteView data)
					{
						Serialization::RootReader rootReader = Serialization::GetReaderFromBuffer(
							ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
						);
						if (UNLIKELY(!rootReader.GetData().IsValid()))
						{
							AssertMessage(false, "Failed to load engine assets database!");
							return;
						}

						const Serialization::Reader reader(rootReader);

						Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
						const Tag::Identifier engineAssetTagIdentifier = tagRegistry.FindOrRegister(EngineAssetsTagGuid);

						const Identifier rootFolderAssetIdentifier = FindOrRegisterFolder(assetsDatabasePath.GetParentPath(), {});
						[[maybe_unused]] const bool loadedEngineAssets = Load(
							assetsDatabasePath,
							reader,
							assetsDatabasePath.GetParentPath(),
							rootFolderAssetIdentifier,
							Array{engineAssetTagIdentifier}
						);
						AssertMessage(loadedEngineAssets, "Failed to load engine assets!");
					},
					ByteView{},
					Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
				);

				{
					Threading::IntermediateStage& finishedLoadingEngineInfoStage = Threading::CreateIntermediateStage();
					Threading::Job& loadEngineInfoJob = Threading::CreateCallback(
						[&finishedLoadingEngineInfoStage](Threading::JobRunnerThread& thread)
						{
							Manager& assetManager = System::Get<Manager>();
							const Optional<Threading::Job*> pJob = assetManager.RequestAsyncLoadAssetMetadata(
								EngineAssetFormat.assetTypeGuid,
								Threading::JobPriority::LoadPlugin,
								[](const ConstByteView data)
								{
									Serialization::RootReader rootReader = Serialization::GetReaderFromBuffer(
										ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
									);

									Manager& assetManager = System::Get<Manager>();
									System::Get<Engine>().m_engineInfo = EngineInfo(assetManager.GetAssetPath(EngineAssetFormat.assetTypeGuid), rootReader);
								}
							);
							if (pJob.IsValid())
							{
								pJob->AddSubsequentStage(finishedLoadingEngineInfoStage);
								pJob->Queue(thread);
							}
						},
						Threading::JobPriority::LoadPlugin,
						"Load engine info"
					);
					loadEngineAssetDatabaseBatch.QueueAsNewFinishedStage(loadEngineInfoJob);
					loadEngineAssetDatabaseBatch.QueueAsNewFinishedStage(finishedLoadingEngineInfoStage);
				}
				jobBatch.QueueAfterStartStage(loadEngineAssetDatabaseBatch);
			}

			{
				Threading::JobBatch loadPluginDatabasesBatch;
				loadPluginDatabasesBatch.QueueAfterStartStage(m_assetLibrary.LoadPluginDatabase());
				jobBatch.QueueAfterStartStage(loadPluginDatabasesBatch);
			}
		}

		return jobBatch;
	}

	bool EngineAssetDatabase::Load(
		const Serialization::Reader reader,
		const IO::PathView databaseRootDirectory,
		const Identifier rootFolderAssetIdentifier,
		const ArrayView<const Tag::Identifier, uint8> tagIdentifiers
	)
	{
		{
			if (UNLIKELY(!reader.GetValue().IsObject()))
			{
				return false;
			}

			const uint32 expectedNewAssetCount = reader.GetValue().GetValue().MemberCount();
			Reserve(expectedNewAssetCount);

			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

			Mask loadedAssetsMask;
			for (Serialization::Member<Serialization::Reader> assetEntryMember : reader.GetMemberView())
			{
				const Guid assetGuid = Guid::TryParse(assetEntryMember.key);
				if (LIKELY(assetGuid.IsValid()))
				{
					Optional<DatabaseEntry> entry = assetEntryMember.value.ReadInPlace<DatabaseEntry, const IO::PathView>(databaseRootDirectory);
					Assert(entry.IsValid());
					if (entry.IsValid())
					{
						{
							Threading::SharedLock lock(m_assetIdentifierLookupMapMutex);
							auto it = m_assetIdentifierLookupMap.Find(assetGuid);
							if (it != m_assetIdentifierLookupMap.end())
							{
								const Identifier assetIdentifier = it->second;
								lock.Unlock();

								loadedAssetsMask.Set(assetIdentifier);

								for (const Tag::Guid tagGuid : entry->m_tags)
								{
									m_tags.Set(tagRegistry.FindOrRegister(tagGuid), assetIdentifier);
								}

								if (entry->m_assetTypeGuid.IsValid())
								{
									const Tag::Identifier assetTypeTagIdentifier = tagRegistry.FindOrRegister(entry->m_assetTypeGuid);
									m_tags.Set(assetTypeTagIdentifier, assetIdentifier);
								}

								for (const Tag::Identifier tagIdentifier : tagIdentifiers)
								{
									m_tags.Set(tagIdentifier, assetIdentifier);
								}

								if (entry->m_assetTypeGuid != FolderAssetType::AssetFormat.assetTypeGuid && rootFolderAssetIdentifier.IsValid())
								{
									RegisterAssetFolders(assetIdentifier, entry->m_path, rootFolderAssetIdentifier, tagIdentifiers);
								}

								{
									Threading::SharedLock databaseLock(m_assetDatabaseMutex);
									const Optional<DatabaseEntry*> pDatabaseEntry = Database::GetAssetEntry(assetGuid);
									Assert(pDatabaseEntry.IsValid());
									if (LIKELY(pDatabaseEntry.IsValid()))
									{
										if (pDatabaseEntry->m_path != entry->m_path)
										{
											// Deregister asset folder tags
											const Identifier assetFolderIdentifier = Identifier::MakeFromIndex(m_assetParentIndices[assetIdentifier]);
											if (assetFolderIdentifier.IsValid())
											{
												const Tag::Identifier folderTagIdentifier = tagRegistry.FindOrRegister(GetAssetGuid(assetFolderIdentifier));
												m_tags.Clear(folderTagIdentifier, assetIdentifier);
											}

											// Replace the path in the lookup map
											Threading::UniqueLock identifierLock(m_identifierLookupMapMutex);
											auto identifierIt = m_identifierLookupMap.Find(pDatabaseEntry->m_path);
											Assert(identifierIt != m_identifierLookupMap.end());
											if (LIKELY(identifierIt != m_identifierLookupMap.end()))
											{
												m_identifierLookupMap.Remove(identifierIt);
												m_identifierLookupMap.Emplace(IO::Path(entry->m_path), Identifier(assetIdentifier));
											}
										}

										pDatabaseEntry->Merge(Move(*entry));
									}
								}

								continue;
							}
						}

						const Identifier assetIdentifier = m_assetIdentifiers.AcquireIdentifier();
						m_assets[assetIdentifier] = assetGuid;

						{
							Threading::UniqueLock lock(m_assetIdentifierLookupMapMutex);
							m_assetIdentifierLookupMap.Emplace(Guid(assetGuid), Identifier(assetIdentifier));
						}

						loadedAssetsMask.Set(assetIdentifier);

						{
							Assert(entry->m_path.HasElements());
							Threading::UniqueLock identifierLookupLock(m_identifierLookupMapMutex);
							m_identifierLookupMap.Emplace(IO::Path(entry->m_path), Identifier(assetIdentifier));
						}

						RegisterAssetFolders(assetIdentifier, entry->m_path, rootFolderAssetIdentifier, tagIdentifiers);

						if (entry->m_assetTypeGuid.IsValid())
						{
							const Tag::Identifier assetTypeTagIdentifier = tagRegistry.FindOrRegister(entry->m_assetTypeGuid);
							m_tags.Set(assetTypeTagIdentifier, assetIdentifier);
						}

						for (const Tag::Guid tagGuid : entry->m_tags)
						{
							m_tags.Set(tagRegistry.FindOrRegister(tagGuid), assetIdentifier);
						}

						for (const Tag::Identifier tagIdentifier : tagIdentifiers)
						{
							m_tags.Set(tagIdentifier, assetIdentifier);
						}

						const Guid assetTypeGuid = entry->m_assetTypeGuid;

						{
							Threading::UniqueLock lock(m_assetDatabaseMutex);
							m_assetMap.Emplace(Guid(assetGuid), Move(*entry));
						}

						if (assetTypeGuid == TagAssetType::AssetFormat.assetTypeGuid)
						{
							tagRegistry.RegisterAsset(assetGuid);
						}
					}
				}
			}
			m_assetCount += loadedAssetsMask.GetNumberOfSetBits();
			OnAssetsAdded(loadedAssetsMask);
			OnDataChanged();
		}

		return true;
	}

	bool Manager::Load(
		[[maybe_unused]] const IO::PathView databaseFilePath,
		const Serialization::Reader reader,
		const IO::PathView databaseRootDirectory,
		const Identifier rootFolderAssetIdentifier,
		const ArrayView<const Tag::Identifier, uint8> tagIdentifiers,
		const ArrayView<const Tag::Identifier, uint8> importedTagIdentifiers
	)
	{
		{
			if (UNLIKELY(!reader.GetValue().IsObject()))
			{
				return false;
			}

			const uint32 expectedNewAssetCount = reader.GetValue().GetValue().MemberCount();
			Reserve(expectedNewAssetCount);

			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

			Mask loadedAssetsMask;
			for (Serialization::Member<Serialization::Reader> assetEntryMember : reader.GetMemberView())
			{
				const Guid assetGuid = Guid::TryParse(assetEntryMember.key);
				if (LIKELY(assetGuid.IsValid()))
				{
					Optional<DatabaseEntry> entry = assetEntryMember.value.ReadInPlace<DatabaseEntry, const IO::PathView>(databaseRootDirectory);
					Assert(entry.IsValid());
					if (entry.IsValid())
					{
						{
							Threading::SharedLock lock(m_assetIdentifierLookupMapMutex);
							auto it = m_assetIdentifierLookupMap.Find(assetGuid);
							if (it != m_assetIdentifierLookupMap.end())
							{
								const Identifier assetIdentifier = it->second;
								lock.Unlock();

								loadedAssetsMask.Set(assetIdentifier);

								for (const Tag::Guid tagGuid : entry->m_tags)
								{
									m_tags.Set(tagRegistry.FindOrRegister(tagGuid), assetIdentifier);
								}

								if (entry->m_assetTypeGuid.IsValid())
								{
									const Tag::Identifier assetTypeTagIdentifier = tagRegistry.FindOrRegister(entry->m_assetTypeGuid);
									m_tags.Set(assetTypeTagIdentifier, assetIdentifier);
								}

								for (const Tag::Identifier tagIdentifier : tagIdentifiers)
								{
									m_tags.Set(tagIdentifier, assetIdentifier);
								}

								if (entry->m_assetTypeGuid != FolderAssetType::AssetFormat.assetTypeGuid && rootFolderAssetIdentifier.IsValid())
								{
									RegisterAssetFolders(assetIdentifier, entry->m_path, rootFolderAssetIdentifier, tagIdentifiers);
								}

								{
									Threading::SharedLock databaseLock(m_assetDatabaseMutex);
									const Optional<DatabaseEntry*> pDatabaseEntry = Database::GetAssetEntry(assetGuid);
									Assert(pDatabaseEntry.IsValid());
									if (LIKELY(pDatabaseEntry.IsValid()))
									{
										if (pDatabaseEntry->m_path != entry->m_path)
										{
											// Deregister asset folder tags
											const Identifier assetFolderIdentifier = Identifier::MakeFromIndex(m_assetParentIndices[assetIdentifier]);
											if (assetFolderIdentifier.IsValid())
											{
												const Tag::Identifier folderTagIdentifier = tagRegistry.FindOrRegister(GetAssetGuid(assetFolderIdentifier));
												m_tags.Clear(folderTagIdentifier, assetIdentifier);
											}

											// Replace the path in the lookup map
											Threading::UniqueLock identifierLock(m_identifierLookupMapMutex);
											auto identifierIt = m_identifierLookupMap.Find(pDatabaseEntry->m_path);
											Assert(identifierIt != m_identifierLookupMap.end());
											if (LIKELY(identifierIt != m_identifierLookupMap.end()))
											{
												m_identifierLookupMap.Remove(identifierIt);
												m_identifierLookupMap.Emplace(IO::Path(entry->m_path), Identifier(assetIdentifier));
											}
										}

										pDatabaseEntry->Merge(Move(*entry));
									}
								}

								continue;
							}
						}

						const Identifier assetIdentifier = m_assetIdentifiers.AcquireIdentifier();
						m_assets[assetIdentifier] = assetGuid;

						{
							Threading::UniqueLock lock(m_assetIdentifierLookupMapMutex);
							m_assetIdentifierLookupMap.Emplace(Guid(assetGuid), Identifier(assetIdentifier));
						}

						loadedAssetsMask.Set(assetIdentifier);

						{
							Assert(entry->m_path.HasElements());
							Threading::UniqueLock identifierLookupLock(m_identifierLookupMapMutex);
							m_identifierLookupMap.Emplace(IO::Path(entry->m_path), Identifier(assetIdentifier));
						}

						if (rootFolderAssetIdentifier.IsValid())
						{
							RegisterAssetFolders(assetIdentifier, entry->m_path, rootFolderAssetIdentifier, tagIdentifiers);
						}

						if (entry->m_assetTypeGuid.IsValid())
						{
							const Tag::Identifier assetTypeTagIdentifier = tagRegistry.FindOrRegister(entry->m_assetTypeGuid);
							m_tags.Set(assetTypeTagIdentifier, assetIdentifier);
						}

						for (const Tag::Guid tagGuid : entry->m_tags)
						{
							m_tags.Set(tagRegistry.FindOrRegister(tagGuid), assetIdentifier);
						}

						for (const Tag::Identifier tagIdentifier : tagIdentifiers)
						{
							m_tags.Set(tagIdentifier, assetIdentifier);
						}

						const Guid assetTypeGuid = entry->m_assetTypeGuid;

						{
							Threading::UniqueLock lock(m_assetDatabaseMutex);
							m_assetMap.Emplace(Guid(assetGuid), Move(*entry));
						}

						if (assetTypeGuid == TagAssetType::AssetFormat.assetTypeGuid)
						{
							tagRegistry.RegisterAsset(assetGuid);
						}
					}
				}
			}

			// Import assets from the local asset database
			if (const Optional<Serialization::Reader> importedAssetsReader = reader.FindSerializer("imported_assets"))
			{
				for (const Optional<Guid> assetGuid : importedAssetsReader->GetArrayView<Guid>())
				{
					const Guid assetTypeGuid = m_assetLibrary.GetAssetTypeGuid(*assetGuid);
					LogWarningIf(!assetTypeGuid.IsValid(), "Failed to import asset {} as it could not be found in the asset library", *assetGuid);
					if (LIKELY(assetTypeGuid.IsValid()))
					{
						const Identifier assetIdentifier =
							Import(LibraryReference{*assetGuid, assetTypeGuid}, ImportingFlags{}, importedTagIdentifiers);
						loadedAssetsMask.Set(assetIdentifier);
					}
				}
			}

			OnLoadAssetDatabase(reader);

			m_assetCount += loadedAssetsMask.GetNumberOfSetBits();
			OnAssetsAdded(loadedAssetsMask);
			OnDataChanged();
		}

#if TRACK_ASSET_FILE_CHANGES
		m_pFileChangeDetection->MonitorDirectory(IO::Path(databaseFilePath.GetWithoutExtensions()));
#endif

		return true;
	}

	Identifier EngineAssetDatabase::FindOrRegisterFolderInternal(const IO::PathView folderPath, const Identifier rootFolderAssetIdentifier)
	{
		IO::Path rootFolderPath = rootFolderAssetIdentifier.IsValid() ? GetAssetPath(rootFolderAssetIdentifier) : IO::Path{};

		Identifier folderAssetIdentifier;
		{
			Threading::SharedLock lock(m_identifierLookupMapMutex);
			Assert(
				rootFolderAssetIdentifier.IsInvalid() || ((m_identifierLookupMap.Find(rootFolderPath) != m_identifierLookupMap.end()) &&
			                                            (m_identifierLookupMap.Find(rootFolderPath)->second == rootFolderAssetIdentifier))
			);

			auto folderIt = m_identifierLookupMap.Find(folderPath);
			if (folderIt != m_identifierLookupMap.end())
			{
				folderAssetIdentifier = folderIt->second;
			}
		}

		Assert(rootFolderAssetIdentifier.IsInvalid() || folderPath.IsRelativeTo(GetAssetPath(rootFolderAssetIdentifier)));
		if (folderAssetIdentifier.IsValid())
		{
			if (folderPath.GetRightMostExtension() == Asset::FileExtension)
			{
				// Special case for folder assets
				IO::Path mainAssetPath = IO::Path::Combine(folderPath, IO::Path::Merge(MAKE_PATH("Main"), folderPath.GetAllExtensions()));
				if (!HasAsset(mainAssetPath))
				{
					mainAssetPath = IO::Path::Combine(folderPath, folderPath.GetFileName());
				}

				{
					Threading::SharedLock lock(m_assetDatabaseMutex);
					const Optional<DatabaseEntry*> pFolderEntry = Database::GetAssetEntry(GetAssetGuid(folderAssetIdentifier));
					const Optional<DatabaseEntry*> pMainAssetEntry = Database::GetAssetEntry(GetAssetGuid(mainAssetPath));
					if (pFolderEntry.IsValid() && pMainAssetEntry.IsValid() && pFolderEntry->m_assetTypeGuid != pMainAssetEntry->m_assetTypeGuid)
					{
						pFolderEntry->SetName(pMainAssetEntry->GetName());
						pFolderEntry->m_description = pMainAssetEntry->m_description;

						if (pMainAssetEntry->m_thumbnailGuid.IsValid())
						{
							pFolderEntry->m_thumbnailGuid = pMainAssetEntry->m_thumbnailGuid;
						}
						else if (pMainAssetEntry->m_assetTypeGuid.IsValid())
						{
							pFolderEntry->m_thumbnailGuid = pMainAssetEntry->m_assetTypeGuid;
						}
					}
				}
			}
		}
		else if (folderPath.GetRightMostExtension() == Asset::FileExtension)
		{
			// Special case for folder assets
			IO::Path mainAssetPath = IO::Path::Combine(folderPath, IO::Path::Merge(MAKE_PATH("Main"), folderPath.GetAllExtensions()));
			if (!HasAsset(mainAssetPath))
			{
				mainAssetPath = IO::Path::Combine(folderPath, folderPath.GetFileName());
			}

			DatabaseEntry entry = VisitAssetEntry(
				GetAssetGuid(mainAssetPath),
				[folderPath](const Optional<const DatabaseEntry*> pAssetEntry) -> DatabaseEntry
				{
					Guid thumbnailAssetGuid = "a132ceb1-7e68-2c14-c3de-9a5cc001201f"_asset;

					if (pAssetEntry.IsValid())
					{
						if (pAssetEntry->m_thumbnailGuid.IsValid())
						{
							thumbnailAssetGuid = pAssetEntry->m_thumbnailGuid;
						}
						else if (pAssetEntry->m_assetTypeGuid.IsValid())
						{
							thumbnailAssetGuid = pAssetEntry->m_assetTypeGuid;
						}

						return DatabaseEntry{
							FolderAssetType::AssetFormat.assetTypeGuid,
							{},
							IO::Path(folderPath),
							UnicodeString(pAssetEntry->GetName()),
							UnicodeString(pAssetEntry->m_description),
							thumbnailAssetGuid,
							pAssetEntry->m_tags.GetView(),
							pAssetEntry->m_dependencies.GetView(),
							pAssetEntry->m_containerContents.GetView(),
							MetaDataStorage(pAssetEntry->m_metaData)
						};
					}
					return DatabaseEntry{
						FolderAssetType::AssetFormat.assetTypeGuid,
						{},
						IO::Path(folderPath),
						UnicodeString{},
						UnicodeString{},
						thumbnailAssetGuid
					};
				}
			);

			const Guid folderGuid = Guid::Generate();
			folderAssetIdentifier = RegisterAsset(folderGuid, Move(entry), {});
		}
		else
		{
			const Guid folderGuid = Guid::Generate();
			folderAssetIdentifier = RegisterAsset(
				folderGuid,
				DatabaseEntry{
					FolderAssetType::AssetFormat.assetTypeGuid,
					{},
					IO::Path(folderPath),
					UnicodeString{},
					UnicodeString{},
					"a132ceb1-7e68-2c14-c3de-9a5cc001201f"_asset
				},
				rootFolderAssetIdentifier
			);
		}
		return folderAssetIdentifier;
	}

	Identifier EngineAssetDatabase::FindOrRegisterFolder(const IO::PathView folderPath, const Identifier rootFolderAssetIdentifier)
	{
		return FindOrRegisterFolderInternal(folderPath, rootFolderAssetIdentifier);
	}

	void EngineAssetDatabase::RegisterAssetFolders(
		const Identifier assetIdentifier,
		IO::PathView assetPath,
		const Identifier rootFolderAssetIdentifier,
		const ArrayView<const Tag::Identifier, uint8> tagIdentifiers
	)
	{
		Assert(rootFolderAssetIdentifier.IsValid());
		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

		Identifier folderAssetIdentifier;
		Identifier childAssetIdentifier = assetIdentifier;
		do
		{
			assetPath = assetPath.GetParentPath();
			folderAssetIdentifier = FindOrRegisterFolderInternal(assetPath, rootFolderAssetIdentifier);

			m_assetParentIndices[childAssetIdentifier] = folderAssetIdentifier.GetIndex();

			const Tag::Identifier folderTagIdentifier = tagRegistry.FindOrRegister(GetAssetGuid(folderAssetIdentifier));
			m_tags.Set(folderTagIdentifier, childAssetIdentifier);
			childAssetIdentifier = folderAssetIdentifier;

			for (const Tag::Identifier tagIdentifier : tagIdentifiers)
			{
				m_tags.Set(tagIdentifier, folderAssetIdentifier);
			}
		} while (assetPath.HasElements() && folderAssetIdentifier != rootFolderAssetIdentifier);
	}

	Identifier Manager::Import(
		const LibraryReference libraryAssetReference,
		const EnumFlags<ImportingFlags> flags,
		const ArrayView<const Tag::Identifier, uint8> tagIdentifiers
	)
	{
		const DatabaseEntry libraryAssetEntry = m_assetLibrary.VisitAssetEntry(
			libraryAssetReference.GetAssetGuid(),
			[](const Optional<const DatabaseEntry*> pLibraryAssetEntry)
			{
				if (pLibraryAssetEntry.IsValid())
				{
					return *pLibraryAssetEntry;
				}
				else
				{
					return DatabaseEntry{};
				}
			}
		);
		if (libraryAssetEntry.IsValid())
		{
			return Import(libraryAssetReference, libraryAssetEntry, flags, tagIdentifiers);
		}
		else
		{
			return {};
		}
	}

	Identifier Manager::Import(
		const LibraryReference libraryAssetReference,
		const DatabaseEntry& libraryAssetEntry,
		const EnumFlags<ImportingFlags> flags,
		const ArrayView<const Tag::Identifier, uint8> tagIdentifiers
	)
	{
		Mask assetsMask;
		const Identifier identifier = Import(libraryAssetReference, libraryAssetEntry, assetsMask, flags, tagIdentifiers);
		if (identifier.IsValid())
		{
			OnAssetsImported(assetsMask, flags);
			OnDataChanged();
		}

		return identifier;
	}

	Identifier Manager::Import(
		const LibraryReference libraryAssetReference,
		const DatabaseEntry& libraryAssetEntry,
		Mask& assetsMask,
		const EnumFlags<ImportingFlags> flags,
		const ArrayView<const Tag::Identifier, uint8> tagIdentifiers
	)
	{
		// Check if the asset had already been imported
		if (const Identifier assetIdentifier = GetAssetIdentifier(libraryAssetReference.GetAssetGuid()))
		{
			if (flags.IsSet(ImportingFlags::SaveToDisk))
			{
				// Set the imported asset tag
				Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
				m_tags.Set(tagRegistry.FindOrRegister(Library::ImportedTagGuid), assetIdentifier);
			}

			for (const Tag::Identifier tagIdentifier : tagIdentifiers)
			{
				m_tags.Set(tagIdentifier, assetIdentifier);
			}

			VisitAssetEntry(
				libraryAssetReference.GetAssetGuid(),
				[&libraryAssetEntry](const Optional<DatabaseEntry*> pEntry)
				{
					Ensure(pEntry.IsValid());
					Assert(pEntry->m_assetTypeGuid == libraryAssetEntry.m_assetTypeGuid);
					pEntry->m_componentTypeGuid = libraryAssetEntry.m_componentTypeGuid;
					if (pEntry->m_path.IsEmpty())
					{
						pEntry->m_path = libraryAssetEntry.m_path;
					}
					if (pEntry->m_name.IsEmpty() && libraryAssetEntry.m_name.HasElements())
					{
						pEntry->SetName(UnicodeString{libraryAssetEntry.m_name});
					}
					if (pEntry->m_description.IsEmpty())
					{
						pEntry->m_description = libraryAssetEntry.m_description;
					}
					if (pEntry->m_thumbnailGuid.IsInvalid())
					{
						pEntry->m_thumbnailGuid = libraryAssetEntry.m_thumbnailGuid;
					}

					pEntry->m_tags.ReserveAdditionalCapacity(libraryAssetEntry.m_tags.GetSize());
					for (Tag::Guid tagGuid : libraryAssetEntry.m_tags)
					{
						pEntry->m_tags.EmplaceBackUnique(Tag::Guid{tagGuid});
					}

					pEntry->m_dependencies.ReserveAdditionalCapacity(libraryAssetEntry.m_dependencies.GetSize());
					for (Asset::Guid dependencyGuid : libraryAssetEntry.m_dependencies)
					{
						pEntry->m_dependencies.EmplaceBackUnique(Guid{dependencyGuid});
					}

					pEntry->m_containerContents.ReserveAdditionalCapacity(libraryAssetEntry.m_containerContents.GetSize());
					for (Asset::Guid contentGuid : libraryAssetEntry.m_containerContents)
					{
						pEntry->m_containerContents.EmplaceBackUnique(Guid{contentGuid});
					}
				}
			);

			return assetIdentifier;
		}

		Threading::SharedLock lock(m_assetLibrary.m_assetDatabaseMutex);
		return ImportInternal(libraryAssetReference, libraryAssetEntry, assetsMask, flags, tagIdentifiers);
	}

	Identifier Manager::ImportInternal(
		const LibraryReference libraryAssetReference,
		const DatabaseEntry& libraryAssetEntry,
		Mask& assetsMask,
		const EnumFlags<ImportingFlags> flags,
		const ArrayView<const Tag::Identifier, uint8> tagIdentifiers
	)
	{
		// Register all dependencies
		for (const Guid dependencyAssetGuid : libraryAssetEntry.m_dependencies)
		{
			AssertMessage(
				dependencyAssetGuid != libraryAssetReference.GetAssetGuid(),
				"Asset {} can't depend on itself!",
				libraryAssetReference.GetAssetGuid()
			);
			if (UNLIKELY_ERROR(dependencyAssetGuid == libraryAssetReference.GetAssetGuid()))
			{
				continue;
			}

			// Skip if the asset had already been imported
			if (HasAsset(dependencyAssetGuid))
			{
				continue;
			}

			if (const Optional<DatabaseEntry*> pDependencyLibraryAssetEntry = m_assetLibrary.GetDatabase().GetAssetEntry(dependencyAssetGuid))
			{
				[[maybe_unused]] const Identifier dependencyAssetIdentifier = ImportInternal(
					LibraryReference{dependencyAssetGuid, pDependencyLibraryAssetEntry->m_assetTypeGuid},
					*pDependencyLibraryAssetEntry,
					assetsMask,
					flags
				);
				AssertMessage(dependencyAssetIdentifier.IsValid(), "Failed to find asset dependency {}!", dependencyAssetGuid);
			}
			else
			{
				LogWarning("Failed to import asset dependency, asset {} could not be found!", dependencyAssetGuid);
			}
		}

		// Register the asset
		// Note that we don't do any copying when using the asset library.

		const Identifier assetLibraryIdentifier = m_assetLibrary.GetAssetIdentifier(libraryAssetReference.GetAssetGuid());

		// Find the root folder associated with this asset
		Identifier rootFolderAssetIdentifier;
		{
			const Identifier assetLibraryRootIdentifier = m_assetLibrary.GetAssetRootParentIdentifier(assetLibraryIdentifier);

			if (assetLibraryRootIdentifier.IsValid() && assetLibraryRootIdentifier != assetLibraryIdentifier)
			{
				const Guid assetLibraryParentGuid = m_assetLibrary.GetAssetGuid(assetLibraryRootIdentifier);
				const Optional<const DatabaseEntry*> pAssetLibraryParentEntry = m_assetLibrary.GetDatabase().GetAssetEntry(assetLibraryParentGuid);
				rootFolderAssetIdentifier =
					FindOrRegisterFolder(pAssetLibraryParentEntry.IsValid() ? pAssetLibraryParentEntry->m_path : IO::Path{}, {});
			}
		}

		const Identifier assetIdentifier = EngineAssetDatabase::RegisterAssetInternal(
			libraryAssetReference.GetAssetGuid(),
			DatabaseEntry{libraryAssetEntry},
			rootFolderAssetIdentifier,
			tagIdentifiers
		);
		if (LIKELY(assetIdentifier.IsValid()))
		{
			assetsMask.Set(assetIdentifier);

			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
			if (flags.IsSet(ImportingFlags::SaveToDisk))
			{
				// Set the imported asset tag
				m_tags.Set(tagRegistry.FindOrRegister(Library::ImportedTagGuid), assetIdentifier);
			}

			// Import all tags
			const TagContainer::DynamicView assetLibraryTags = tagRegistry.GetValidElementView(m_assetLibrary.m_tags.GetView());
			m_tags.CopyTags(assetLibraryTags, assetLibraryIdentifier, assetIdentifier);
		}

		if (libraryAssetEntry.m_thumbnailGuid.IsValid() && !HasAsset(libraryAssetEntry.m_thumbnailGuid))
		{
			if (const Optional<DatabaseEntry*> pThumbnailLibraryAssetEntry = m_assetLibrary.GetDatabase().GetAssetEntry(libraryAssetEntry.m_thumbnailGuid))
			{
				[[maybe_unused]] const Identifier thumbnailAssetIdentifier = ImportInternal(
					LibraryReference{libraryAssetEntry.m_thumbnailGuid, TextureAssetType::AssetFormat.assetTypeGuid},
					*pThumbnailLibraryAssetEntry,
					assetsMask,
					flags
				);
				AssertMessage(thumbnailAssetIdentifier.IsValid(), "Failed to find asset thumbnail {}!", libraryAssetEntry.m_thumbnailGuid);
			}
			else
			{
				LogWarning("Asset thumbnail entry {} could not be found!", libraryAssetEntry.m_thumbnailGuid);
			}
		}

		if (flags.IsSet(ImportingFlags::FullHierarchy))
		{
			for (const Asset::Guid containedAssetGuid : libraryAssetEntry.m_containerContents)
			{
				if (!HasAsset(containedAssetGuid))
				{
					if (const Optional<DatabaseEntry*> pAssetEntry = m_assetLibrary.GetDatabase().GetAssetEntry(containedAssetGuid))
					{
						[[maybe_unused]] const Identifier dependencyAssetIdentifier =
							ImportInternal(LibraryReference{containedAssetGuid, pAssetEntry->m_assetTypeGuid}, *pAssetEntry, assetsMask, flags);
						AssertMessage(dependencyAssetIdentifier.IsValid(), "Failed to find asset hierarchy dependency {}!", containedAssetGuid);
					}
					else
					{
						LogWarning("Asset hierarchy entry {} could not be found!", containedAssetGuid);
					}
				}
			}
		}

		return assetIdentifier;
	}

	void Manager::RemoveAsset(const Guid assetGuid)
	{
#if ENABLE_ASSERTS
		{
			Threading::UniqueLock lock(m_assetLoaderMapMutex);
			Assert(!m_assetLoaderMap.Contains(assetGuid), "Async load callbacks must be removed before their asset is!");
		}
#endif

		EngineAssetDatabase::RemoveAsset(assetGuid);

		OnDataChanged();
	}

	struct AssetGuid
	{
		bool Serialize(const Serialization::Reader serializer)
		{
			serializer.Serialize("guid", m_guid);
			return true;
		}

		Guid m_guid;
	};

	Optional<Threading::Job*>
	Manager::RequestAsyncLoadAssetMetadata(const Guid assetGuid, Threading::JobPriority priority, IO::AsyncLoadCallback&& callback) const
	{
		if (const Optional<const DatabaseEntry*> pEntry = GetAssetEntry(assetGuid))
		{
			return RequestAsyncLoadAssetPath(
				assetGuid,
				pEntry->m_path,
				priority,
				Forward<IO::AsyncLoadCallback>(callback),
				{},
				Math::Range<size>::MakeStartToEnd(0ull, Math::NumericLimits<size>::Max - 1)
			);
		}
		LogWarning("Failed to load non-existent asset {} metadata", assetGuid);
		callback({});
		return nullptr;
	}

	Optional<Threading::Job*> Manager::RequestAsyncLoadAssetBinary(
		const Guid assetGuid, Threading::JobPriority priority, IO::AsyncLoadCallback&& callback, const Math::Range<size> dataRange
	) const
	{
		if (const Optional<const DatabaseEntry*> pEntry = GetAssetEntry(assetGuid))
		{
			const IO::PathView binaryFilePath = pEntry->GetBinaryFilePath();
			if (binaryFilePath.HasElements())
			{
				return RequestAsyncLoadAssetPath(
					assetGuid,
					pEntry->GetBinaryFilePath(),
					priority,
					Forward<IO::AsyncLoadCallback>(callback),
					{},
					dataRange
				);
			}
			else
			{
				LogWarning("Failed to load non-existent binary for asset {}", assetGuid);
				callback({});
				return nullptr;
			}
		}
		LogWarning("Failed to load non-existent asset {} binary", assetGuid);
		callback({});
		return nullptr;
	}

	Optional<Threading::Job*> Manager::RequestAsyncLoadAssetBinary(
		const Guid assetGuid, Threading::JobPriority priority, IO::AsyncLoadCallback&& callback, const size readOffset, const ByteView target
	) const
	{
		if (const Optional<const DatabaseEntry*> pEntry = GetAssetEntry(assetGuid))
		{
			return RequestAsyncLoadAssetPath(
				assetGuid,
				pEntry->GetBinaryFilePath(),
				priority,
				Forward<IO::AsyncLoadCallback>(callback),
				target,
				Math::Range<size>::MakeStartToEnd(readOffset, readOffset + target.GetDataSize())
			);
		}
		LogWarning("Failed to load non-existent asset {} binary", assetGuid);
		callback({});
		return nullptr;
	}

	Optional<Threading::Job*> Manager::RequestAsyncLoadAssetPath(
		const Asset::Guid assetGuid,
		const IO::PathView path,
		Threading::JobPriority priority,
		IO::AsyncLoadCallback&& callback,
		ByteView target,
		const Math::Range<size> dataRange
	) const
	{
		Threading::SharedLock lock(m_assetLoaderMapMutex);
		const AssetLoaderMap::const_iterator it = m_assetLoaderMap.Find(assetGuid);
		if (it == m_assetLoaderMap.end())
		{
			lock.Unlock();
			return CreateAsyncLoadFromDiskJob(path, priority, Forward<IO::AsyncLoadCallback>(callback), target, dataRange);
		}

		return it->second(assetGuid, path, priority, Forward<IO::AsyncLoadCallback>(callback), target, dataRange);
	}

	void Manager::RegisterCopyFromSourceAsyncLoadCallback(const Guid sourceAssetGuid, const Guid targetAassetGuid)
	{
		Assert(HasAsset(sourceAssetGuid) || m_assetLibrary.HasAsset(sourceAssetGuid));
		RegisterAsyncLoadCallback(
			targetAassetGuid,
			[this, sourceAssetGuid, targetAassetGuid](
				const Asset::Guid targetAssetGuid,
				const IO::PathView path,
				Threading::JobPriority priority,
				IO::AsyncLoadCallback&& callback,
				const ByteView target,
				const Math::Range<size> dataRange
			) -> Optional<Threading::Job*>
			{
				IO::Path sourcePath = GetAssetPath(sourceAssetGuid);
				if (sourcePath.IsEmpty())
				{
					sourcePath = m_assetLibrary.GetAssetPath(sourceAssetGuid);
				}
				IO::Path targetPath = GetAssetPath(targetAssetGuid);
				if (targetPath.HasElements())
				{
					sourcePath = IO::Path::Combine(sourcePath.GetParentPath(), path.GetRelativeToParent(targetPath.GetParentPath()));

					IO::AsyncLoadCallback nestedCallback = [sourcePath,
				                                          targetPath = Move(targetPath),
				                                          targetAassetGuid,
				                                          callback = Forward<IO::AsyncLoadCallback>(callback)](const ConstByteView data)
					{
						IO::File targetFile(targetPath, IO::AccessModeFlags::WriteBinary);
						Assert(targetFile.IsValid());
						if (LIKELY(targetFile.IsValid()))
						{
							if (targetFile.Write(data))
							{
								targetFile.Close();
								if (targetPath.GetRightMostExtension() == Asset::FileExtension)
								{
									Serialization::RootReader assetDataSerializer = Serialization::GetReaderFromBuffer(
										ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
									);

									Serialization::Data& assetData = assetDataSerializer.GetData();
									Assert(assetData.IsValid() && assetData.GetDocument().IsObject());
									if (LIKELY(assetData.IsValid() && assetData.GetDocument().IsObject()))
									{
										Serialization::Writer writer(assetData);
										writer.Serialize("guid", targetAassetGuid);
										[[maybe_unused]] const bool wasSaved = assetData.SaveToFile(targetPath, Serialization::SavingFlags{});
										Assert(wasSaved);
									}
								}
							}
						}

						callback(data);
					};

					const AssetLoaderMap::const_iterator loaderIt = m_assetLoaderMap.Find(sourceAssetGuid);
					if (loaderIt == m_assetLoaderMap.end())
					{
						return CreateAsyncLoadFromDiskJob(sourcePath, priority, Move(nestedCallback), target, dataRange);
					}
					return loaderIt->second(sourceAssetGuid, sourcePath, priority, Move(nestedCallback), target, dataRange);
				}
				else
				{
					callback({});
					return nullptr;
				}
			}
		);
	}

#if TRACK_ASSET_FILE_CHANGES
	void Manager::NotifyFileAddedOnDisk(const IO::PathView directoryPath, const IO::PathView relativeChangedPath)
	{
		IO::Path filePath = IO::Path::Combine(directoryPath, relativeChangedPath);
		IO::Path metaDataFilePath(filePath);
		if (metaDataFilePath.GetRightMostExtension() != Asset::FileExtension)
		{
			metaDataFilePath = IO::Path::Merge(metaDataFilePath, Asset::FileExtension);
		}

		Asset asset;
		if (Serialization::DeserializeFromDisk(IO::Path(metaDataFilePath), asset))
		{
			if (Optional<DatabaseEntry*> pDatabaseEntry = GetAssetEntry(asset.GetGuid()))
			{
				if (pDatabaseEntry->m_path != metaDataFilePath)
				{
					LogWarning(
						"Trying to register asset {0} with same guid {1} as {2}",
						metaDataFilePath,
						asset.GetGuid().ToString().GetView(),
						pDatabaseEntry->m_path
					);
				}

				// Intentionally do nothing since the asset already exists.
				return;
			}
			else
			{
				asset.SetMetaDataFilePath(metaDataFilePath);
				RegisterAsset(asset.GetGuid(), DatabaseEntry{asset}, {});
			}
		}
	}

	void Manager::NotifyFileRemovedOnDisk(const IO::PathView directoryPath, const IO::PathView relativeChangedPath)
	{
		const IO::Path absolutePath = IO::Path::Combine(directoryPath, relativeChangedPath);

		for (decltype(Manager::m_assetMap)::const_iterator it = m_assetMap.begin(), end = m_assetMap.end(); it != end; ++it)
		{
			if (it->second.m_path == absolutePath)
			{
				RemoveAsset(it->first);
				break;
			}
		}
	}

	void Manager::NotifyFileModifiedOnDisk(const IO::PathView directoryPath, const IO::PathView relativeChangedPath)
	{
		IO::Path assetPath = IO::Path::Combine(directoryPath, relativeChangedPath);
		const Asset::Guid assetGuid = GetAssetGuid(assetPath);
		if (assetGuid.IsValid())
		{
			const Time::Timestamp lastModifiedTime = assetPath.GetLastModifiedTime();
			decltype(m_fileChangeTimestamps)::iterator timestampIt = m_fileChangeTimestamps.Find(assetGuid);
			if (timestampIt == m_fileChangeTimestamps.end())
			{
				m_fileChangeTimestamps.Emplace(Guid(assetGuid), Time::Timestamp(lastModifiedTime));
				for (AssetModifiedFunction& modifiedFunction : m_assetModifiedCallbacks)
				{
					modifiedFunction(assetGuid, assetPath);
				}
			}
			else if (lastModifiedTime != timestampIt->second)
			{
				timestampIt->second = lastModifiedTime;

				for (AssetModifiedFunction& modifiedFunction : m_assetModifiedCallbacks)
				{
					modifiedFunction(assetGuid, assetPath);
				}
			}
		}
	}

	void Manager::NotifyFileRenamedOnDisk(
		const IO::PathView directoryPath, const IO::PathView relativePreviousFilePath, const IO::PathView relativeNewFilePath
	)
	{
		const Asset::Guid assetGuid = GetAssetGuid(IO::Path::Combine(directoryPath, relativePreviousFilePath));
		if (assetGuid.IsValid())
		{
			DatabaseEntry& entry = *GetAssetEntry(assetGuid);
			entry.m_path = IO::Path(IO::Path::Combine(directoryPath, relativeNewFilePath));
		}
	}
#endif // !TRACK_ASSET_FILE_CHANGES

	bool Manager::MoveAsset(const IO::PathView path, IO::Path&& newPath)
	{
		Assert(newPath.HasElements());

		Threading::UniqueLock identifierLock(m_identifierLookupMapMutex);
		Threading::UniqueLock assetDatabaseLock(m_assetDatabaseMutex);
		decltype(m_identifierLookupMap)::const_iterator identifierIt = m_identifierLookupMap.Find(path);
		if (identifierIt == m_identifierLookupMap.end())
		{
			return false;
		}

		const Identifier assetIdentifier = identifierIt->second;
		const Guid assetGuid = GetAssetGuid(assetIdentifier);
		m_identifierLookupMap.Remove(identifierIt);

		decltype(m_assetMap)::iterator assetIt = m_assetMap.Find(assetGuid);
		Assert(assetIt != m_assetMap.end());
		if (LIKELY(assetIt != m_assetMap.end()))
		{
			assetIt->second.m_path = Forward<IO::Path>(newPath);
			m_identifierLookupMap.Emplace(IO::Path(assetIt->second.m_path), Identifier(assetIdentifier));
		}

		return true;
	}

	EngineAssetDatabase::EngineAssetDatabase(const Guid dataSourceGuid)
		: Interface(System::Get<DataSource::Cache>().FindOrRegister(dataSourceGuid))
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		dataSourceCache.OnCreated(m_identifier, *this);
	}

	static DataSource::PropertyIdentifier s_assetNamePropertyIdentifier;
	static DataSource::PropertyIdentifier s_assetIdentifierPropertyIdentifier;
	static DataSource::PropertyIdentifier s_assetGenericIdentifierPropertyIdentifier;
	static DataSource::PropertyIdentifier s_assetGuidPropertyIdentifier;
	static DataSource::PropertyIdentifier s_assetTypeGuidPropertyIdentifier;
	static DataSource::PropertyIdentifier s_assetThumbnailGuidPropertyIdentifier;
	static DataSource::PropertyIdentifier s_assetTypeThumbnailPropertyIdentifier;
	static DataSource::PropertyIdentifier s_assetHierarchyDepthPropertyIdentifier;
	static DataSource::PropertyIdentifier s_assetHierarchyOrderPropertyIdentifier;

	EngineAssetDatabase::~EngineAssetDatabase()
	{
		// TODO: Deregister properties

		m_tags.Destroy(System::Get<Tag::Registry>());
	}

	void EngineAssetDatabase::InitializePropertyIdentifiers()
	{
		DataSource::Cache& dataSourceCache = System::Get<DataSource::Cache>();
		s_assetNamePropertyIdentifier = dataSourceCache.RegisterProperty("asset_name");
		s_assetIdentifierPropertyIdentifier = dataSourceCache.RegisterProperty("asset_id");
		s_assetGenericIdentifierPropertyIdentifier = dataSourceCache.RegisterProperty("asset_generic_id");
		s_assetGuidPropertyIdentifier = dataSourceCache.RegisterProperty("asset_guid");
		s_assetTypeGuidPropertyIdentifier = dataSourceCache.RegisterProperty("asset_type_guid");
		s_assetThumbnailGuidPropertyIdentifier = dataSourceCache.RegisterProperty("asset_thumbnail_guid");
		s_assetTypeThumbnailPropertyIdentifier = dataSourceCache.RegisterProperty("asset_type_thumbnail_guid");
		s_assetHierarchyDepthPropertyIdentifier = dataSourceCache.RegisterProperty("asset_hierarchy_depth");
		s_assetHierarchyOrderPropertyIdentifier = dataSourceCache.RegisterProperty("asset_hierarchy_order");
	}

	void EngineAssetDatabase::Reserve(const uint32 count)
	{
		{
			Threading::UniqueLock lock(m_assetIdentifierLookupMapMutex);
			m_requestedAssetCapacity += count;
			m_assetIdentifierLookupMap.Reserve(m_requestedAssetCapacity);
		}

		{
			Threading::UniqueLock lock(m_identifierLookupMapMutex);
			m_identifierLookupMap.Reserve(m_requestedAssetCapacity);
		}
	}

	Identifier EngineAssetDatabase::RegisterAssetInternal(
		const Guid assetGuid,
		DatabaseEntry&& entry,
		const Identifier rootFolderAssetIdentifier,
		const ArrayView<const Tag::Identifier, uint8> tagIdentifiers
	)
	{
		Assert(assetGuid.IsValid());

		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

		{
			Threading::SharedLock lock(m_assetIdentifierLookupMapMutex);
			auto it = m_assetIdentifierLookupMap.Find(assetGuid);
			if (it != m_assetIdentifierLookupMap.end())
			{
				const Identifier assetIdentifier = it->second;
				lock.Unlock();

				for (const Tag::Guid tagGuid : entry.m_tags)
				{
					m_tags.Set(tagRegistry.FindOrRegister(tagGuid), assetIdentifier);
				}

				if (entry.m_assetTypeGuid.IsValid())
				{
					const Tag::Identifier assetTypeTagIdentifier = tagRegistry.FindOrRegister(entry.m_assetTypeGuid);
					m_tags.Set(assetTypeTagIdentifier, assetIdentifier);
				}

				for (const Tag::Identifier tagIdentifier : tagIdentifiers)
				{
					m_tags.Set(tagIdentifier, assetIdentifier);
				}

				// Deregister the previous folder asset tag
				const Identifier assetFolderIdentifier = Identifier::MakeFromIndex(m_assetParentIndices[assetIdentifier]);
				if (assetFolderIdentifier.IsValid())
				{
					const Tag::Identifier folderTagIdentifier = tagRegistry.FindOrRegister(GetAssetGuid(assetFolderIdentifier));
					m_tags.Clear(folderTagIdentifier, assetIdentifier);
				}

				if (entry.m_assetTypeGuid != FolderAssetType::AssetFormat.assetTypeGuid && rootFolderAssetIdentifier.IsValid())
				{
					RegisterAssetFolders(assetIdentifier, entry.m_path, rootFolderAssetIdentifier, tagIdentifiers);
				}

				{
					Threading::SharedLock databaseLock(m_assetDatabaseMutex);
					const Optional<DatabaseEntry*> pDatabaseEntry = Database::GetAssetEntry(assetGuid);
					Assert(pDatabaseEntry.IsValid());
					if (LIKELY(pDatabaseEntry.IsValid()))
					{
						pDatabaseEntry->m_assetTypeGuid = entry.m_assetTypeGuid;

						Assert(!pDatabaseEntry->m_componentTypeGuid.IsValid() || pDatabaseEntry->m_componentTypeGuid == entry.m_componentTypeGuid);
						pDatabaseEntry->m_componentTypeGuid = entry.m_componentTypeGuid;

						if (pDatabaseEntry->m_path != entry.m_path)
						{
							// Replace the path in the lookup map
							Threading::UniqueLock identifierLock(m_identifierLookupMapMutex);
							auto identifierIt = m_identifierLookupMap.Find(pDatabaseEntry->m_path);
							Assert(identifierIt != m_identifierLookupMap.end());
							if (LIKELY(identifierIt != m_identifierLookupMap.end()))
							{
								m_identifierLookupMap.Remove(identifierIt);
								m_identifierLookupMap.Emplace(IO::Path(entry.m_path), Identifier(assetIdentifier));
							}
						}

						pDatabaseEntry->Merge(Move(entry));
					}
				}

				return assetIdentifier;
			}
		}

		const Identifier assetIdentifier = m_assetIdentifiers.AcquireIdentifier();
		Assert(assetIdentifier.IsValid());
		if (LIKELY(assetIdentifier.IsValid()))
		{
			for (const Tag::Guid tagGuid : entry.m_tags)
			{
				m_tags.Set(tagRegistry.FindOrRegister(tagGuid), assetIdentifier);
			}

			if (entry.m_assetTypeGuid.IsValid())
			{
				const Tag::Identifier assetTypeTagIdentifier = tagRegistry.FindOrRegister(entry.m_assetTypeGuid);
				m_tags.Set(assetTypeTagIdentifier, assetIdentifier);
			}

			for (const Tag::Identifier tagIdentifier : tagIdentifiers)
			{
				m_tags.Set(tagIdentifier, assetIdentifier);
			}

			if (entry.m_path.HasElements())
			{
				{
					Threading::UniqueLock lock(m_identifierLookupMapMutex);
					m_identifierLookupMap.Emplace(IO::Path(entry.m_path), Identifier(assetIdentifier));
				}

				if (entry.m_assetTypeGuid != FolderAssetType::AssetFormat.assetTypeGuid && rootFolderAssetIdentifier.IsValid())
				{
					RegisterAssetFolders(assetIdentifier, entry.m_path, rootFolderAssetIdentifier, tagIdentifiers);
				}
			}

			{
				Threading::UniqueLock lock(m_assetDatabaseMutex);
				Database::RegisterAsset(assetGuid, Forward<DatabaseEntry>(entry), {});
			}

			m_assetCount++;

			m_assets[assetIdentifier] = assetGuid;
			{
				Threading::UniqueLock lock(m_assetIdentifierLookupMapMutex);
				m_requestedAssetCapacity++;
				m_assetIdentifierLookupMap.Emplace(Guid(assetGuid), Identifier(assetIdentifier));
			}

			if (entry.m_assetTypeGuid == TagAssetType::AssetFormat.assetTypeGuid)
			{
				tagRegistry.RegisterAsset(assetGuid);
			}
		}
		return assetIdentifier;
	}

	Identifier EngineAssetDatabase::RegisterAsset(
		const Guid assetGuid,
		DatabaseEntry&& entry,
		const Identifier rootFolderAssetIdentifier,
		const ArrayView<const Tag::Identifier, uint8> tagIdentifiers
	)
	{
		const Identifier assetIdentifier =
			RegisterAssetInternal(assetGuid, Forward<DatabaseEntry>(entry), rootFolderAssetIdentifier, tagIdentifiers);
		if (LIKELY(assetIdentifier.IsValid()))
		{
			Mask assetMask;
			assetMask.Set(assetIdentifier);
			OnAssetsAdded(assetMask);

			OnDataChanged();
		}
		return assetIdentifier;
	}

	void EngineAssetDatabase::RemoveAsset(const Guid assetGuid)
	{
		{
			Threading::UniqueLock databaseLock(m_assetDatabaseMutex);

			if (const Optional<const DatabaseEntry*> pEntry = GetAssetEntry(assetGuid))
			{
				Threading::UniqueLock lock(m_identifierLookupMapMutex);
				auto it = m_identifierLookupMap.Find(pEntry->m_path);
				if (LIKELY(it != m_identifierLookupMap.end()))
				{
					m_identifierLookupMap.Remove(it);
				}
			}
			Database::RemoveAsset(assetGuid);
		}

		Identifier assetIdentifier;
		{
			Threading::UniqueLock lock(m_assetIdentifierLookupMapMutex);
			auto it = m_assetIdentifierLookupMap.Find(assetGuid);
			Assert(it != m_assetIdentifierLookupMap.end());
			if (LIKELY(it != m_assetIdentifierLookupMap.end()))
			{
				assetIdentifier = it->second;
				m_requestedAssetCapacity--;
				m_assetIdentifierLookupMap.Remove(it);
			}
		}

		m_assets[assetIdentifier] = {};
		m_assetCount--;
		if (LIKELY(assetIdentifier.IsValid()))
		{
			m_assetIdentifiers.ReturnIdentifier(assetIdentifier);

			Mask assetsMask;
			assetsMask.Set(assetIdentifier);
			OnAssetsRemoved(assetsMask);
		}
	}

	void EngineAssetDatabase::LockRead()
	{
		// Need to ensure we can still access recursively
		/*[[maybe_unused]] const bool wasLocked = m_assetDatabaseMutex.LockShared();
		Assert(wasLocked);*/
	}

	void EngineAssetDatabase::UnlockRead()
	{
		// m_assetDatabaseMutex.UnlockShared();
	}

	static_assert(DataSource::GenericDataIdentifier::MaximumCount >= Identifier::MaximumCount);
	void EngineAssetDatabase::CacheQuery(const Query& query, CachedQuery& __restrict cachedQueryOut) const
	{
		Mask& __restrict selectedAssets = reinterpret_cast<Mask&>(cachedQueryOut);
		selectedAssets.ClearAll();

		const TagContainer::ConstRestrictedView tags = m_tags.GetView();

		if (query.m_allowedItems.IsValid())
		{
			selectedAssets = Mask(*query.m_allowedItems);
		}

		if (query.m_allowedFilterMask.AreAnySet())
		{
			Mask allowedFilterMask;
			for (const Tag::Identifier::IndexType tagIndex : query.m_allowedFilterMask.GetSetBitsIterator())
			{
				const Threading::Atomic<AtomicMask*> pTagAssets = tags[Tag::Identifier::MakeFromValidIndex(tagIndex)];
				if (pTagAssets != nullptr)
				{
					allowedFilterMask |= *pTagAssets;
				}
			}

			if (!query.m_allowedItems.IsValid())
			{
				selectedAssets |= allowedFilterMask;
			}
			else
			{
				selectedAssets &= allowedFilterMask;
			}
		}

		// Default to showing all entries if no allowed masks were set
		if (!query.m_allowedItems.IsValid() && query.m_allowedFilterMask.AreNoneSet())
		{
			for (const Identifier assetIdentifier : m_assetIdentifiers.GetView())
			{
				if (m_assets[assetIdentifier].IsValid())
				{
					selectedAssets.Set(assetIdentifier);
				}
			}
		}

		if (query.m_disallowedItems.IsValid())
		{
			selectedAssets.Clear(*query.m_disallowedItems);
		}

		for (const Tag::Identifier::IndexType tagIndex : query.m_disallowedFilterMask.GetSetBitsIterator())
		{
			const Threading::Atomic<AtomicMask*> pTagAssets = tags[Tag::Identifier::MakeFromValidIndex(tagIndex)];
			if (pTagAssets != nullptr)
			{
				selectedAssets.Clear(*pTagAssets);
			}
		}

		if (query.m_requiredFilterMask.AreAnySet())
		{
			for (const Tag::Identifier::IndexType tagIndex : query.m_requiredFilterMask.GetSetBitsIterator())
			{
				const Threading::Atomic<AtomicMask*> pTagAssets = tags[Tag::Identifier::MakeFromValidIndex(tagIndex)];
				if (pTagAssets != nullptr)
				{
					selectedAssets &= *pTagAssets;
				}
				else
				{
					selectedAssets.ClearAll();
				}
			}
		}
	}

	bool EngineAssetDatabase::SortQuery(
		const CachedQuery& cachedQuery,
		const PropertyIdentifier filteredPropertyIdentifier,
		const SortingOrder order,
		SortedQueryIndices& cachedSortedQueryOut
	)
	{
		if (filteredPropertyIdentifier == s_assetHierarchyOrderPropertyIdentifier)
		{
			const Mask& __restrict assetMask = reinterpret_cast<const Mask&>(cachedQuery);
			cachedSortedQueryOut.Clear();
			cachedSortedQueryOut.Reserve(assetMask.GetNumberOfSetBits());

			for (Identifier::IndexType assetIdentifierIndex : assetMask.GetSetBitsIterator())
			{
				cachedSortedQueryOut.EmplaceBack(assetIdentifierIndex);
			}

			Algorithms::Sort(
				(Identifier::IndexType*)cachedSortedQueryOut.begin(),
				(Identifier::IndexType*)cachedSortedQueryOut.end(),
				[this, order](const Identifier::IndexType leftIndex, const Identifier::IndexType rightIndex)
				{
					const IO::Path left = GetAssetPath(Identifier::MakeFromValidIndex(leftIndex));
					const IO::Path right = GetAssetPath(Identifier::MakeFromValidIndex(rightIndex));

					IO::PathView leftPath = left;
					IO::PathView rightPath = right;
					IO::PathView leftDirectory = leftPath.GetFirstPath();
					IO::PathView rightDirectory = rightPath.GetFirstPath();

					if (order == SortingOrder::Ascending)
					{
						do
						{
							if (leftDirectory != rightDirectory)
							{
								return leftDirectory < rightDirectory;
							}

							leftPath += leftDirectory.GetSize() + 1;
							rightPath += leftDirectory.GetSize() + 1;
							leftDirectory = leftPath.GetFirstPath();
							rightDirectory = rightPath.GetFirstPath();
						} while (leftDirectory.HasElements() && rightDirectory.HasElements());

						return rightDirectory.HasElements();
					}
					else
					{
						do
						{
							if (leftDirectory != rightDirectory)
							{
								return leftDirectory > rightDirectory;
							}

							leftPath += leftDirectory.GetSize() + 1;
							rightPath += leftDirectory.GetSize() + 1;
							leftDirectory = leftPath.GetFirstPath();
							rightDirectory = rightPath.GetFirstPath();
						} while (leftDirectory.HasElements() && rightDirectory.HasElements());

						return rightDirectory.HasElements();
					}
				}
			);
			return true;
		}
		else if (filteredPropertyIdentifier == s_assetNamePropertyIdentifier)
		{
			const Mask& __restrict assetMask = reinterpret_cast<const Mask&>(cachedQuery);
			cachedSortedQueryOut.Clear();
			cachedSortedQueryOut.Reserve(assetMask.GetNumberOfSetBits());

			for (Identifier::IndexType assetIdentifierIndex : assetMask.GetSetBitsIterator())
			{
				cachedSortedQueryOut.EmplaceBack(assetIdentifierIndex);
			}

			Algorithms::Sort(
				(Identifier::IndexType*)cachedSortedQueryOut.begin(),
				(Identifier::IndexType*)cachedSortedQueryOut.end(),
				[this, order](const Identifier::IndexType leftIndex, const Identifier::IndexType rightIndex)
				{
					const IO::Path::StringType left = GetAssetName(GetAssetGuid(Identifier::MakeFromValidIndex(leftIndex)));
					const IO::Path::StringType right = GetAssetName(GetAssetGuid(Identifier::MakeFromValidIndex(rightIndex)));

					if (order == SortingOrder::Ascending)
					{
						return left.GetView() < right.GetView();
					}
					else
					{
						return left.GetView() > right.GetView();
					}
				}
			);
			return true;
		}
		else
		{
			auto it = m_propertySortingExtensions.Find(filteredPropertyIdentifier);
			if (it != m_propertySortingExtensions.end())
			{
				const SortingCallback& callback = it->second;
				callback(cachedQuery, order, cachedSortedQueryOut);
				return true;
			}
		}
		return false;
	}

	void EngineAssetDatabase::IterateData(const CachedQuery& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset) const
	{
		const Mask& selectedAssets = reinterpret_cast<const Mask&>(query);

		uint32 count = 0;
		const Mask::SetBitsIterator iterator = selectedAssets.GetSetBitsIterator(0, GetMaximumUsedElementCount());
		for (auto it : iterator)
		{
			if (count >= offset.GetMinimum())
			{
				Identifier assetIdentifier = m_assetIdentifiers.GetActiveIdentifier(Identifier::MakeFromValidIndex(it));
				callback(assetIdentifier);
				if (count >= offset.GetMaximum())
				{
					break;
				}
			}
			++count;
		}
	}

	void EngineAssetDatabase::IterateData(
		const SortedQueryIndices& query, IterationCallback&& callback, Math::Range<GenericDataIndex> offset
	) const
	{
		for (const Identifier::IndexType identifierIndex : query.GetSubView(offset.GetMinimum(), offset.GetSize()))
		{
			Identifier assetIdentifier = m_assetIdentifiers.GetActiveIdentifier(Identifier::MakeFromValidIndex(identifierIndex));
			callback(assetIdentifier);
		}
	}

	DataSource::PropertyValue EngineAssetDatabase::GetDataProperty(const Data data, const DataSource::PropertyIdentifier identifier) const
	{
		const Identifier assetIdentifier = data.GetExpected<Identifier>();
		const Guid assetGuid = m_assets[assetIdentifier];

		if (const Optional<const DatabaseEntry*> pAssetEntry = GetAssetEntry(assetGuid))
		{
			if (identifier == s_assetNamePropertyIdentifier)
			{
				return String(pAssetEntry->GetName());
			}
			else if (identifier == s_assetGuidPropertyIdentifier)
			{
				return Guid(assetGuid);
			}
			else if (identifier == s_assetIdentifierPropertyIdentifier)
			{
				return Identifier(assetIdentifier);
			}
			else if (identifier == s_assetGenericIdentifierPropertyIdentifier)
			{
				const Identifier::IndexType index = assetIdentifier.GetFirstValidIndex();
				return DataSource::GenericDataIdentifier::MakeFromValidIndex(index);
			}
			else if (identifier == s_assetTypeGuidPropertyIdentifier)
			{
				if (pAssetEntry->m_assetTypeGuid.IsValid())
				{
					return pAssetEntry->m_assetTypeGuid;
				}
			}
			else if (identifier == s_assetThumbnailGuidPropertyIdentifier)
			{
				if (pAssetEntry->m_thumbnailGuid.IsValid())
				{
					return pAssetEntry->m_thumbnailGuid;
				}
				else if (pAssetEntry->m_assetTypeGuid == TextureAssetType::AssetFormat.assetTypeGuid)
				{
					return assetGuid;
				}
				else if (pAssetEntry->m_assetTypeGuid.IsValid() && GetAssetTypeGuid(pAssetEntry->m_assetTypeGuid) == TextureAssetType::AssetFormat.assetTypeGuid)
				{
					return (Asset::Guid)pAssetEntry->m_assetTypeGuid;
				}
			}
			else if (identifier == s_assetTypeThumbnailPropertyIdentifier)
			{
				if (pAssetEntry->m_assetTypeGuid.IsValid())
				{
					return (Asset::Guid)pAssetEntry->m_assetTypeGuid;
				}
			}
			else if (identifier == s_assetHierarchyDepthPropertyIdentifier)
			{
				uint32 depthCounter = 0;
				Identifier parentAssetIdentifier = Identifier::MakeFromIndex(m_assetParentIndices[assetIdentifier]);
				while (parentAssetIdentifier.IsValid())
				{
					++depthCounter;
					parentAssetIdentifier = Identifier::MakeFromIndex(m_assetParentIndices[parentAssetIdentifier]);
				}
				return depthCounter;
			}
			else if (identifier == s_assetHierarchyOrderPropertyIdentifier)
			{
				Assert(false, "Intended for sorting");
			}
			else
			{
				auto it = m_propertyExtensions.Find(identifier);
				if (it != m_propertyExtensions.end())
				{
					const ExtensionCallback& callback = it->second;
					return callback(assetIdentifier);
				}
			}
		}

		return {};
	}

	void EngineAssetDatabase::RegisterDataPropertyCallback(const DataSource::PropertyIdentifier identifier, ExtensionCallback&& callback)
	{
		Assert(!m_propertyExtensions.Contains(identifier));
		m_propertyExtensions.Emplace(DataSource::PropertyIdentifier{identifier}, Forward<ExtensionCallback>(callback));
	}

	void EngineAssetDatabase::RegisterDataPropertySortingCallback(const DataSource::PropertyIdentifier identifier, SortingCallback&& callback)
	{
		Assert(!m_propertySortingExtensions.Contains(identifier));
		m_propertySortingExtensions.Emplace(DataSource::PropertyIdentifier{identifier}, Forward<SortingCallback>(callback));
	}
}

namespace ngine
{
	template struct UnorderedMap<Asset::Guid, Asset::Identifier, Asset::Guid::Hash>;
	template struct TSaltedIdentifierStorage<Asset::Identifier>;

	template struct Tag::AtomicMaskContainer<Asset::Identifier>;

	template struct UnorderedMap<IO::Path, Asset::Guid, IO::Path::Hash>;
	template struct UnorderedMap<IO::Path, Asset::Identifier, IO::Path::Hash>;

	template struct UnorderedMap<Guid, Time::Timestamp, Guid::Hash>;
}
