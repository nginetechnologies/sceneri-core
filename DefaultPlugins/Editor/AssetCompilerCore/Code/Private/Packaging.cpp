#include <AssetCompilerCore/Packaging.h>

#include <Common/IO/PathView.h>
#include <Common/IO/FileIterator.h>
#include <Common/IO/File.h>
#include <Common/IO/Library.h>
#include <Common/Project System/ProjectInfo.h>
#include <Common/Project System/EngineInfo.h>
#include <Common/Project System/EngineDatabase.h>
#include <Common/Project System/PluginDatabase.h>
#include <Common/Project System/PackagedBundle.h>
#include <Common/Project System/PluginInfo.h>
#include <Common/Platform/GetName.h>
#include <Common/Platform/GetBuildConfigurations.h>
#include <Common/Asset/Asset.h>
#include <Common/Asset/AssetDatabase.h>
#include <Common/Asset/Context.h>
#include <Common/Format/Guid.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Serialize.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/IO/Format/Path.h>
#include <Common/IO/Log.h>
#include <Common/Memory/AddressOf.h>

#include <Common/Time/Timestamp.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/System/Query.h>
#include <AssetCompilerCore/Plugin.h>

namespace ngine::ProjectSystem
{
	[[nodiscard]] Asset::Database CopyMetaDataAssetsIntoPackageDirectory(
		const IO::Path& sourceAssetDirectory,
		const IO::Path& targetAssetDirectory,
		const Asset::Context& context,
		Threading::Atomic<bool>& failedAnyTasksOut
	)
	{
		Asset::Database database;

		Asset::Database::FindAssetsInDirectory(
			sourceAssetDirectory,
			[&sourceAssetDirectory, &targetAssetDirectory, &database, &context, &failedAnyTasksOut](IO::Path&& path)
			{
				const Serialization::Data existingAssetData(path);

				if (!existingAssetData.IsValid())
				{
					LogError("Encountered invalid JSON in {}", path);
					failedAnyTasksOut = true;
					return;
				}

				const Asset::Asset existingAsset(existingAssetData, Move(path));

				IO::Path fullTargetAssetPath =
					IO::Path::Combine(targetAssetDirectory, existingAsset.GetMetaDataFilePath().GetRelativeToParent(sourceAssetDirectory));
				if (!fullTargetAssetPath.Exists())
				{
					IO::Path(fullTargetAssetPath.GetParentPath()).CreateDirectories();
					failedAnyTasksOut |= !existingAsset.GetMetaDataFilePath().CopyFileTo(fullTargetAssetPath);
				}
				else if (existingAsset.GetMetaDataFilePath().GetLastModifiedTime() > fullTargetAssetPath.GetLastModifiedTime())
				{
					failedAnyTasksOut |= !existingAsset.GetMetaDataFilePath().CopyFileTo(fullTargetAssetPath);
				}

				Serialization::Data targetAssetData(fullTargetAssetPath);
				Asset::Asset targetAsset(targetAssetData, Move(fullTargetAssetPath));

				if (existingAsset.HasSourceFilePath())
				{
					IO::Path sourceAssetFilePath = IO::Path(*existingAsset.ComputeAbsoluteSourceFilePath());
					if (existingAsset.GetSourceFilePath()->GetRightMostExtension() == Asset::Asset::FileExtension)
					{
						const Serialization::Data sourceAssetData(sourceAssetFilePath);
						const Asset::Asset sourceAsset(sourceAssetData, Move(sourceAssetFilePath));
						targetAsset.SetSourceAsset(sourceAsset.GetGuid());
					}
					else
					{
						targetAsset.SetSourceFilePath(Move(sourceAssetFilePath), context);
					}
				}
				else
				{
					targetAsset.SetSourceFilePath(IO::Path(existingAsset.GetMetaDataFilePath()), context);
				}

				Serialization::Serialize(targetAssetData, targetAsset);
				const bool saved = targetAssetData.SaveToFile(targetAsset.GetMetaDataFilePath(), Serialization::SavingFlags{});
				failedAnyTasksOut |= !saved;

				database.RegisterAsset(targetAsset.GetGuid(), Asset::DatabaseEntry{targetAsset}, targetAssetDirectory.GetParentPath());
			}
		);

		return database;
	}

	struct NewDatabaseInfo
	{
		Asset::Database m_database;
		IO::Path m_path;
		Asset::Context m_context;
		Asset::Context m_sourceContext;
	};

	void CompileAssetsInDatabase(
		NewDatabaseInfo& databaseInfo,
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const EnumFlags<Platform::Type> platforms,
		const Asset::Context& context,
		const Asset::Context& sourceContext
	)
	{
		LogMessage("Compiling assets in database {}", databaseInfo.m_path);
		databaseInfo.m_database.IterateAssets(
			[&assetCompiler, &failedAnyTasksOut, &currentThread, platforms, &databasePath = databaseInfo.m_path, &context, &sourceContext](
				const Asset::Guid,
				const Asset::DatabaseEntry& assetEntry
			) -> Memory::CallbackResult
			{
				IO::Path absoluteFilePath = IO::Path::Combine(databasePath.GetParentPath(), assetEntry.m_path);
				Serialization::Data targetAssetData(absoluteFilePath);
				Asset::Asset targetAsset(targetAssetData, Move(absoluteFilePath));

				if (targetAsset.HasSourceFile())
				{
					LogMessage("Queuing compile of {} ({})", assetEntry.GetName(), assetEntry.m_path);
					Threading::Job* pJob = assetCompiler.CompileMetaDataAsset(
						AssetCompiler::CompileFlags::WasDirectlyRequested,
						[&failedAnyTasksOut](
							const EnumFlags<AssetCompiler::CompileFlags> flags,
							const ArrayView<Asset::Asset> assets,
							[[maybe_unused]] const ArrayView<const Serialization::Data> assetsData
						)
						{
							if (flags.IsSet(AssetCompiler::CompileFlags::Compiled))
							{
								for (const Asset::Asset& asset : assets)
								{
									LogMessage("Compiled asset {} ({})", asset.GetName(), asset.GetMetaDataFilePath());
								}
							}
							else if (flags.IsSet(AssetCompiler::CompileFlags::UpToDate))
								;
							else if (flags.IsSet(AssetCompiler::CompileFlags::UnsupportedOnPlatform))
								;
							else if (flags.IsSet(AssetCompiler::CompileFlags::IsCollection))
								;
							else
							{
								LogError("Failed asset compilation");
								for (const Asset::Asset& asset : assets)
								{
									LogError("Failed compiling asset {} ({})", asset.GetName(), asset.GetMetaDataFilePath());
								}
								failedAnyTasksOut = true;
							}
						},
						currentThread,
						platforms,
						IO::Path(targetAsset.GetMetaDataFilePath()),
						context,
						sourceContext
					);

					if (pJob != nullptr)
					{
						pJob->Queue(currentThread);
					}
				}

				return Memory::CallbackResult::Continue;
			}
		);
	}

	[[nodiscard]] NewDatabaseInfo CopyMetaData(
		const Asset::Guid configGuid,
		Asset::DatabaseEntry&& databaseEntry,
		Asset::Context&& context,
		Asset::Context&& sourceContext,
		PackagedBundle& packagedBundle,
		const IO::Path& buildDirectory,
		const IO::Path& sourceAssetDirectory,
		const IO::Path& targetAssetDirectory,
		Threading::Atomic<bool>& failedAnyTasksOut
	)
	{
		Asset::Database database =
			CopyMetaDataAssetsIntoPackageDirectory(sourceAssetDirectory, targetAssetDirectory, context, failedAnyTasksOut);
		database.RegisterAsset(configGuid, Forward<Asset::DatabaseEntry>(databaseEntry), targetAssetDirectory.GetParentPath());

		IO::Path sourceAssetDatabasePath = IO::Path::Merge(sourceAssetDirectory, Asset::Database::AssetFormat.metadataFileExtension);
		const Asset::Database sourceAssetDatabase(sourceAssetDatabasePath, sourceAssetDirectory.GetParentPath());
		database.SetGuid(sourceAssetDatabase.GetGuid());

		IO::Path targetAssetDatabasePath = IO::Path::Merge(targetAssetDirectory, Asset::Database::AssetFormat.metadataFileExtension);
		if (!database.Save(targetAssetDatabasePath, Serialization::SavingFlags{}))
		{
			LogError("Failed to save database {}", targetAssetDatabasePath);
			failedAnyTasksOut = true;
		}

		packagedBundle.AddAssetDatabase(
			targetAssetDirectory.GetParentPath().GetRelativeToParent(buildDirectory),
			targetAssetDirectory.GetParentPath(),
			Asset::Database(database)
		);

		Assert(!context.GetEngine().IsValid() || context.GetEngine()->GetGuid().IsValid());

		return NewDatabaseInfo{
			Move(database),
			Move(targetAssetDatabasePath),
			Forward<Asset::Context>(context),
			Forward<Asset::Context>(sourceContext)
		};
	}

	[[nodiscard]] NewDatabaseInfo CopyEngineMetaData(
		const EngineInfo& engineInfo, const IO::Path& buildDirectory, PackagedBundle& packagedBundle, Threading::Atomic<bool>& failedAnyTasksOut
	)
	{
		const IO::Path sourceAssetDirectory = IO::Path::Combine(engineInfo.GetDirectory(), EngineInfo::EngineAssetsPath);
		const IO::Path targetAssetDirectory = IO::Path::Combine(buildDirectory, EngineInfo::EngineAssetsPath);

		IO::Path newConfigFilePath = IO::Path::Combine(targetAssetDirectory.GetParentPath(), engineInfo.GetConfigFilePath().GetFileName());
		{
			Serialization::Data engineInfoData(engineInfo.GetConfigFilePath());
			if (!engineInfoData.SaveToFile(newConfigFilePath, Serialization::SavingFlags{}))
			{
				LogError("Failed to save engine info {}", newConfigFilePath);
				failedAnyTasksOut = true;
			}
		}

		Asset::Context newContext{EngineInfo(IO::Path(newConfigFilePath))};

		return CopyMetaData(
			engineInfo.GetGuid(),
			Asset::DatabaseEntry{*newContext.GetEngine()},
			Move(newContext),
			Asset::Context(EngineInfo(engineInfo)),
			packagedBundle,
			buildDirectory,
			sourceAssetDirectory,
			targetAssetDirectory,
			failedAnyTasksOut
		);
	}

	[[nodiscard]] NewDatabaseInfo CopyPluginMetaData(
		const EngineInfo& targetEngine,
		const Asset::Context& sourceContext,
		const IO::Path& buildDirectory,
		EnginePluginDatabase& newAvailablePluginDatabase,
		PackagedBundle& packagedBundle,
		Threading::Atomic<bool>& failedAnyTasksOut
	)
	{
		const FlatString<37> pluginGuidString = sourceContext.GetPlugin()->GetGuid().ToString();
		IO::Path newPluginConfigFilePath = IO::Path::Combine(
			buildDirectory,
			MAKE_NATIVE_LITERAL("Plug-ins"),
			pluginGuidString.GetView(),
			IO::Path::Merge(pluginGuidString.GetView(), PluginAssetFormat.metadataFileExtension)
		);
		IO::Path(newPluginConfigFilePath.GetParentPath()).CreateDirectories();

		const IO::Path sourceAssetDirectory =
			IO::Path::Combine(sourceContext.GetPlugin()->GetDirectory(), sourceContext.GetPlugin()->GetRelativeAssetDirectory());
		const IO::Path targetAssetDirectory =
			IO::Path::Combine(newPluginConfigFilePath.GetParentPath(), sourceContext.GetPlugin()->GetRelativeAssetDirectory());

		{
			Serialization::Data pluginInfoData(sourceContext.GetPlugin()->GetConfigFilePath());
			if (!pluginInfoData.SaveToFile(newPluginConfigFilePath, Serialization::SavingFlags{}))
			{
				LogError("Failed to save plugin {}", newPluginConfigFilePath);
				failedAnyTasksOut = true;
			}
		}

		Asset::Context context;
		context.SetPlugin(PluginInfo(IO::Path(newPluginConfigFilePath)));
		context.SetEngine(EngineInfo(targetEngine));

		packagedBundle.AddPlugin(PluginInfo{*context.GetPlugin()});

		{
			IO::Path relativePluginFilePath(newPluginConfigFilePath);
			relativePluginFilePath.MakeRelativeToParent(buildDirectory);
			newAvailablePluginDatabase.RegisterPlugin(*context.GetPlugin());
			packagedBundle.GetPluginDatabase().RegisterPlugin(*context.GetPlugin());
		}

		return CopyMetaData(
			sourceContext.GetPlugin()->GetGuid(),
			Asset::DatabaseEntry{*context.GetPlugin()},
			Move(context),
			Asset::Context(sourceContext),
			packagedBundle,
			buildDirectory,
			sourceAssetDirectory,
			targetAssetDirectory,
			failedAnyTasksOut
		);
	}

	[[nodiscard]] bool CopyProjectMetaData(
		const EngineInfo& targetEngine,
		const Asset::Context& sourceContext,
		const IO::Path& buildDirectory,
		const EnginePluginDatabase& availablePluginDatabase,
		EnginePluginDatabase& newAvailablePluginDatabase,
		PackagedBundle& packagedBundle,
		Vector<NewDatabaseInfo>& assetDatabasesOut
	)
	{
		Threading::Atomic<bool> failedAnyTasksOut = false;

		const IO::Path sourceAssetDirectory =
			IO::Path::Combine(sourceContext.GetProject()->GetDirectory(), sourceContext.GetProject()->GetRelativeAssetDirectory());
		const IO::Path targetAssetDirectory = IO::Path::Combine(buildDirectory, sourceContext.GetProject()->GetRelativeAssetDirectory());

		{
			IO::Path newConfigFilePath =
				IO::Path::Combine(targetAssetDirectory.GetParentPath(), sourceContext.GetProject()->GetConfigFilePath().GetFileName());

			{
				Serialization::Data pluginInfoData(sourceContext.GetProject()->GetConfigFilePath());
				if (!pluginInfoData.SaveToFile(newConfigFilePath, Serialization::SavingFlags{}))
				{
					LogError("Failed to save plugin {}", newConfigFilePath);
					failedAnyTasksOut = true;
				}
			}

			Asset::Context context;
			context.SetProject(ProjectInfo(IO::Path(newConfigFilePath)));
			context.SetEngine(EngineInfo(targetEngine));

			assetDatabasesOut.EmplaceBack(CopyMetaData(
				sourceContext.GetProject()->GetGuid(),
				Asset::DatabaseEntry{*context.GetPlugin()},
				Move(context),
				Asset::Context(sourceContext),
				packagedBundle,
				buildDirectory,
				sourceAssetDirectory,
				targetAssetDirectory,
				failedAnyTasksOut
			));
		}

		Vector<Asset::Guid> uniquePlugins(Memory::Reserve, sourceContext.GetProject()->GetPluginGuids().GetSize());

		using TryAddUniquePluginFunction = void (*)(
			const Asset::Guid pluginGuid,
			Vector<Asset::Guid>& uniquePlugins,
			const EnginePluginDatabase& availablePluginDatabase,
			const IO::Path& buildDirectory,
			EnginePluginDatabase& newAvailablePluginDatabase,
			PackagedBundle& packagedBundle,
			Vector<NewDatabaseInfo>& assetDatabasesOut,
			const EngineInfo& targetEngine,
			const Asset::Context& sourceContext,
			Threading::Atomic<bool>& failedAnyTasksOut
		);
		static TryAddUniquePluginFunction tryAddUniquePlugin = [](
																														 const Asset::Guid pluginGuid,
																														 Vector<Asset::Guid>& uniquePlugins,
																														 const EnginePluginDatabase& availablePluginDatabase,
																														 const IO::Path& buildDirectory,
																														 EnginePluginDatabase& newAvailablePluginDatabase,
																														 PackagedBundle& packagedBundle,
																														 Vector<NewDatabaseInfo>& assetDatabasesOut,
																														 const EngineInfo& targetEngine,
																														 const Asset::Context& sourceContext,
																														 Threading::Atomic<bool>& failedAnyTasksOut
																													 )
		{
			if (!uniquePlugins.Contains(pluginGuid))
			{
				uniquePlugins.EmplaceBack(pluginGuid);

				// TODO: Context should include plugin dependencies
				Asset::Context pluginSourceContext;
				pluginSourceContext.SetEngine(EngineInfo(*sourceContext.GetEngine()));

				IO::Path sourcePluginPath = availablePluginDatabase.FindPlugin(pluginGuid);
				const PluginInfo sourcePlugin = PluginInfo(Move(sourcePluginPath));
				for (const Asset::Guid pluginDependencyGuid : sourcePlugin.GetDependencies())
				{
					tryAddUniquePlugin(
						pluginDependencyGuid,
						uniquePlugins,
						availablePluginDatabase,
						buildDirectory,
						newAvailablePluginDatabase,
						packagedBundle,
						assetDatabasesOut,
						targetEngine,
						pluginSourceContext,
						failedAnyTasksOut
					);
				}

				assetDatabasesOut.EmplaceBack(CopyPluginMetaData(
					targetEngine,
					pluginSourceContext,
					buildDirectory,
					newAvailablePluginDatabase,
					packagedBundle,
					failedAnyTasksOut
				));
			}
		};

		for (const Asset::Guid pluginGuid : sourceContext.GetProject()->GetPluginGuids())
		{
			tryAddUniquePlugin(
				pluginGuid,
				uniquePlugins,
				availablePluginDatabase,
				buildDirectory,
				newAvailablePluginDatabase,
				packagedBundle,
				assetDatabasesOut,
				targetEngine,
				sourceContext,
				failedAnyTasksOut
			);
		}

		return !failedAnyTasksOut;
	}

	[[nodiscard]] bool CopyPluginBinaries(
		const Platform::Type platform,
		const ConstNativeStringView buildConfiguration,
		const PluginInfo& sourcePlugin,
		const PluginInfo& targetPlugin
	)
	{
		if (sourcePlugin.HasBinaryDirectory())
		{
			const IO::Path pluginBinaryPath = IO::Path::Combine(
				sourcePlugin.GetDirectory(),
				sourcePlugin.GetRelativeBinaryDirectory(),
				Platform::GetName(platform),
				buildConfiguration,
				IO::Path::Merge(IO::Library::FileNamePrefix, sourcePlugin.GetGuid().ToString().GetView(), IO::Library::FileNamePostfix)
			);
			if (pluginBinaryPath.Exists())
			{
				const IO::Path newPluginBinaryPath =
					IO::Path::Combine(targetPlugin.GetDirectory(), pluginBinaryPath.GetRelativeToParent(sourcePlugin.GetDirectory()));
				newPluginBinaryPath.CreateDirectories();

				if (pluginBinaryPath.GetLastModifiedTime() > newPluginBinaryPath.GetLastModifiedTime())
				{
					if constexpr (IO::Library::IsDirectory)
					{
						return pluginBinaryPath.CopyDirectoryTo(newPluginBinaryPath);
					}
					else
					{
						return pluginBinaryPath.CopyFileTo(newPluginBinaryPath);
					}
				}
				else
				{
					return true;
				}
			}
			else
			{
				// Does not exist, compile or fail
				// TODO: Try to compile on fail if available
				LogError("Failed to copy plugin binary {}", pluginBinaryPath);
				return false;
			}
		}

		return true;
	}

	[[nodiscard]] bool CopyExecutableBinaries(
		const IO::Path& targetBinaryDirectory, const IO::Path& launcherBinaryPath, const IO::Path& configurationBinaryDirectory
	)
	{
		targetBinaryDirectory.CreateDirectories();

		if (launcherBinaryPath.Exists())
		{
			LogMessage(
				"Copying executable binaries from binary path {} and configuration binary directory {} to {}",
				launcherBinaryPath,
				configurationBinaryDirectory,
				targetBinaryDirectory
			);

			bool failedAny = false;

			for (IO::FileIterator fileIterator(configurationBinaryDirectory); !fileIterator.ReachedEnd(); fileIterator.Next())
			{
				if (fileIterator.GetCurrentFileType() != IO::FileType::File)
				{
					continue;
				}

				const IO::PathView fileName = fileIterator.GetCurrentFileName();
				const IO::PathView fileExtension = fileName.GetAllExtensions();
				if ((fileExtension == IO::Library::FileNamePostfix) || (fileExtension == IO::Library::ExecutablePostfix))
				{
					const IO::Path filePath = fileIterator.GetCurrentFilePath();
					const IO::Path newFilePath = IO::Path::Combine(targetBinaryDirectory, fileIterator.GetCurrentFileName());

					if (filePath.GetLastModifiedTime() > newFilePath.GetLastModifiedTime())
					{
						if (filePath.CopyFileTo(newFilePath))
						{
							LogMessage("Copying executable {} to {}", filePath, newFilePath);
						}
						else
						{
							LogError("Failed to copy executable {} to {}", filePath, newFilePath);
							failedAny = true;
						}
					}
				}
				else if (fileExtension == IO::Library::ApplicationPostfix)
				{
					const IO::Path filePath = fileIterator.GetCurrentFilePath();
					Assert(filePath.IsDirectory());
					const IO::Path newFilePath = IO::Path::Combine(targetBinaryDirectory, fileIterator.GetCurrentFileName());

					if (filePath.GetLastModifiedTime() > newFilePath.GetLastModifiedTime())
					{
						if (filePath.CopyDirectoryTo(newFilePath))
						{
							LogMessage("Copying application {} to {}", filePath, newFilePath);
						}
						else
						{
							LogError("Failed to copy application {} to {}", filePath, newFilePath);
							failedAny = true;
						}
					}
				}
			}

			return !failedAny;
		}
		else
		{
			LogMessage("Skipping executable binary copy from {}", launcherBinaryPath);
			return true;
		}
	}

	[[nodiscard]] void PackageProject(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		const ConstNativeStringView buildConfiguration,
		IO::Path& buildDirectory,
		const Asset::Context& sourceContext,
		EnumFlags<ProjectSystem::PackagingFlags> packagingFlags
	)
	{
		const ConstNativeStringView platformName = Platform::GetName(platform);
		if (buildDirectory.IsEmpty())
		{
			buildDirectory = IO::Path::Combine(sourceContext.GetProject()->GetDirectory(), MAKE_NATIVE_LITERAL("PackagedBuilds"), platformName);
		}

		if (packagingFlags.IsSet(PackagingFlags::CleanBuild))
		{
			if (buildDirectory.Exists())
			{
				LogMessage("Clearing build directory");
				buildDirectory.EmptyDirectoryRecursively();
			}
		}

		buildDirectory.CreateDirectories();

		EnginePluginDatabase availablePluginDatabase(
			IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), EnginePluginDatabase::FileName)
		);
		EnginePluginDatabase newAvailablePluginsDatabase(IO::Path::Combine(buildDirectory, EnginePluginDatabase::FileName));
		newAvailablePluginsDatabase.SetGuid(availablePluginDatabase.GetGuid());

		PackagedBundle packagedBundle;
		packagedBundle.GetEngineInfo() = EngineInfo{*sourceContext.GetEngine()};
		packagedBundle.GetProjectInfo() = ProjectInfo{*sourceContext.GetProject()};
		packagedBundle.GetPluginDatabase().SetFilePath(IO::Path::Combine(buildDirectory, EnginePluginDatabase::FileName));

		Vector<NewDatabaseInfo> assetDatabases(Memory::Reserve, 2 + availablePluginDatabase.GetCount());

		// Start by copying all metadata
		{
			LogMessage("Copying engine metadata");
			assetDatabases.EmplaceBack(CopyEngineMetaData(*sourceContext.GetEngine(), buildDirectory, packagedBundle, failedAnyTasksOut));
			const EngineInfo& targetEngineInfo = *assetDatabases.GetLastElement().m_context.GetEngine();
			LogMessage("Copying project metadata");
			failedAnyTasksOut |= !CopyProjectMetaData(
				targetEngineInfo,
				sourceContext,
				buildDirectory,
				availablePluginDatabase,
				newAvailablePluginsDatabase,
				packagedBundle,
				assetDatabases
			);

			if (!newAvailablePluginsDatabase.Save(Serialization::SavingFlags{}))
			{
				LogError("Failed to save asset database");
				failedAnyTasksOut = true;
			}

			LogMessage("Saving packaged bundle");
			if (!Serialization::SerializeToDisk(
						IO::Path::Combine(buildDirectory, PackagedBundle::FileName),
						packagedBundle,
						Serialization::SavingFlags{}
					))
			{
				LogError("Failed to save packaged bundle");
				failedAnyTasksOut = true;
			}
		}

		// Compile the asset compiler plug-in first
		// This is necessary as the asset compiler may have assets that are required in order to compile other asset types
		LogMessage("Compiling asset compiler assets");
		assetDatabases.RemoveFirstOccurrencePredicate(
			[&assetCompiler, &currentThread, &failedAnyTasksOut, platform](NewDatabaseInfo& assetDatabase)
			{
				if (!assetDatabase.m_context.GetPlugin().IsValid() || assetDatabase.m_context.GetPlugin()->GetGuid() != AssetCompiler::Plugin::Guid)
				{
					return ErasePredicateResult::Continue;
				}

				CompileAssetsInDatabase(
					assetDatabase,
					assetCompiler,
					currentThread,
					failedAnyTasksOut,
					platform,
					assetDatabase.m_context,
					assetDatabase.m_sourceContext
				);
				return ErasePredicateResult::Remove;
			}
		);

		// Now compile the metadata
		LogMessage("Compiling remaining assets");
		for (NewDatabaseInfo& assetDatabase : assetDatabases)
		{
			Assert(!assetDatabase.m_context.GetEngine().IsValid() || assetDatabase.m_context.GetEngine()->GetGuid().IsValid());
			CompileAssetsInDatabase(
				assetDatabase,
				assetCompiler,
				currentThread,
				failedAnyTasksOut,
				platform,
				assetDatabase.m_context,
				assetDatabase.m_sourceContext
			);
		}

		// Plug-ins are currently exclusively monolithic
		static constexpr bool copyPluginBinaries = false;
		if constexpr (copyPluginBinaries)
		{
			packagedBundle.GetPluginDatabase().IteratePlugins(
				[&failedAnyTasksOut,
			   platform,
			   buildConfiguration,
			   &availablePluginDatabase,
			   buildDirectory](const Asset::Guid pluginGuid, const IO::Path& configFilePath)
				{
					IO::Path sourcePluginPath = availablePluginDatabase.FindPlugin(pluginGuid);
					const PluginInfo sourcePlugin = PluginInfo(Move(sourcePluginPath));
					const PluginInfo targetPlugin = PluginInfo(IO::Path::Combine(buildDirectory, configFilePath));
					failedAnyTasksOut |= !CopyPluginBinaries(platform, buildConfiguration, sourcePlugin, targetPlugin);
				}
			);
		}
	}

	void PackageProjectLauncher(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		const ConstNativeStringView buildConfiguration,
		const Asset::Context& sourceContext,
		IO::Path& buildDirectory,
		EnumFlags<ProjectSystem::PackagingFlags> packagingFlags
	)
	{
		PackageProject(
			assetCompiler,
			currentThread,
			failedAnyTasksOut,
			platform,
			buildConfiguration,
			buildDirectory,
			sourceContext,
			packagingFlags
		);
		const ConstNativeStringView platformName = Platform::GetName(platform);

		IO::Path binaryDirectory =
			IO::Path::Combine(sourceContext.GetProject()->GetDirectory(), sourceContext.GetProject()->GetRelativeBinaryDirectory(), platformName);
		if (!binaryDirectory.Exists())
		{
			binaryDirectory =
				IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), sourceContext.GetEngine()->GetRelativeBinaryDirectory(), platformName);
		}
		const IO::Path configurationBinaryDirectory = IO::Path::Combine(binaryDirectory, buildConfiguration);
		if (buildDirectory != configurationBinaryDirectory)
		{
			LogMessage("Copying executable binaries");

			const IO::Path launcherBinaryPath =
				IO::Path::Combine(configurationBinaryDirectory, IO::Path::Merge(MAKE_PATH("ProjectLauncher"), IO::Library::ApplicationPostfix));
			const IO::Path targetBinaryDirectory =
				IO::Path::Combine(buildDirectory, sourceContext.GetProject()->GetRelativeBinaryDirectory(), platformName, buildConfiguration);

			failedAnyTasksOut |= !CopyExecutableBinaries(targetBinaryDirectory, launcherBinaryPath, configurationBinaryDirectory);
		}
	}

	void PackageProjectEditor(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		const ConstNativeStringView buildConfiguration,
		Asset::Context&& sourceContext,
		IO::Path& buildDirectory,
		EnumFlags<ProjectSystem::PackagingFlags> packagingFlags
	)
	{
		static constexpr Asset::Guid EditorCoreGuid = "1B7A9E37-7221-4F7E-A266-DF5CAD4CEF1E"_asset;

		ProjectInfo project(*sourceContext.GetProject());
		project.AddPlugin(EditorCoreGuid);
		sourceContext.SetProject(Move(project));

		PackageProject(
			assetCompiler,
			currentThread,
			failedAnyTasksOut,
			platform,
			buildConfiguration,
			buildDirectory,
			sourceContext,
			packagingFlags
		);
		const ConstNativeStringView platformName = Platform::GetName(platform);

		IO::Path binaryDirectory =
			IO::Path::Combine(sourceContext.GetProject()->GetDirectory(), sourceContext.GetProject()->GetRelativeBinaryDirectory(), platformName);
		if (!binaryDirectory.Exists())
		{
			binaryDirectory =
				IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), sourceContext.GetEngine()->GetRelativeBinaryDirectory(), platformName);
		}

		// Copy the launcher binary and dependent libraries
		LogMessage("Compiling executable binaries");
		{
			const IO::Path configurationBinaryDirectory = IO::Path::Combine(binaryDirectory, buildConfiguration);
			const IO::Path launcherBinaryPath =
				IO::Path::Combine(configurationBinaryDirectory, IO::Path::Merge(MAKE_PATH("Editor"), IO::Library::ApplicationPostfix));
			const IO::Path targetBinaryDirectory =
				IO::Path::Combine(buildDirectory, sourceContext.GetProject()->GetRelativeBinaryDirectory(), platformName, buildConfiguration);

			failedAnyTasksOut |= !CopyExecutableBinaries(targetBinaryDirectory, launcherBinaryPath, configurationBinaryDirectory);
		}

		// Copy project system and asset compiler
		if constexpr (PLATFORM_DESKTOP && !PLATFORM_APPLE_MACOS)
		{
			const IO::Path projectSystemBinaryPath =
				IO::Path::Combine(binaryDirectory, IO::Path::Merge(MAKE_PATH("ProjectSystem"), IO::Library::ApplicationPostfix));
			const IO::Path targetBinaryDirectory =
				IO::Path::Combine(buildDirectory, sourceContext.GetProject()->GetRelativeBinaryDirectory(), platformName);

			failedAnyTasksOut |= !CopyExecutableBinaries(targetBinaryDirectory, projectSystemBinaryPath, binaryDirectory);
		}
	}

	void PackageStandaloneEditor(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		const ConstNativeStringView buildConfiguration,
		Asset::Context&& sourceContext,
		IO::Path& buildDirectory,
		EnumFlags<ProjectSystem::PackagingFlags> packagingFlags
	)
	{
		const ConstNativeStringView platformName = Platform::GetName(platform);
		if (buildDirectory.IsEmpty())
		{
			buildDirectory = IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), MAKE_NATIVE_LITERAL("PackagedBuilds"), platformName);
		}

		if (packagingFlags.IsSet(PackagingFlags::CleanBuild))
		{
			if (buildDirectory.Exists())
			{
				LogMessage("Clearing build directory");
				buildDirectory.EmptyDirectoryRecursively();
			}
		}

		buildDirectory.CreateDirectories();

		EnginePluginDatabase availablePluginDatabase(
			IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), EnginePluginDatabase::FileName)
		);
		EnginePluginDatabase newAvailablePluginDatabase(IO::Path::Combine(buildDirectory, EnginePluginDatabase::FileName));
		newAvailablePluginDatabase.SetGuid(availablePluginDatabase.GetGuid());

		PackagedBundle packagedBundle;
		packagedBundle.GetEngineInfo() = EngineInfo{*sourceContext.GetEngine()};
		packagedBundle.GetPluginDatabase().SetFilePath(IO::Path::Combine(buildDirectory, EnginePluginDatabase::FileName));

		FixedCapacityVector<NewDatabaseInfo> assetDatabases(Memory::Reserve, 1 + availablePluginDatabase.GetCount());

		// Start by copying all metadata
		{
			LogMessage("Copying engine metadata");
			assetDatabases.EmplaceBack(CopyEngineMetaData(*sourceContext.GetEngine(), buildDirectory, packagedBundle, failedAnyTasksOut));
			const EngineInfo& targetEngine = *assetDatabases.GetLastElement().m_context.GetEngine();

			LogMessage("Copying plug-in metadata");
			availablePluginDatabase.IteratePlugins(
				[buildDirectory, &newAvailablePluginDatabase, &packagedBundle, &assetDatabases, &targetEngine, &sourceContext, &failedAnyTasksOut](
					const Asset::Guid,
					const IO::Path& path
				)
				{
					PluginInfo plugin = PluginInfo(IO::Path(path));
					Assert(plugin.IsValid())
					if (plugin.IsValid())
					{
						Asset::Context pluginSourceContext = sourceContext;
						pluginSourceContext.SetPlugin(Move(plugin));

						// TODO: Context should include plugin dependencies
						LogMessage("Copying {} metadata", plugin.GetName());
						assetDatabases.EmplaceBack(CopyPluginMetaData(
							targetEngine,
							pluginSourceContext,
							buildDirectory,
							newAvailablePluginDatabase,
							packagedBundle,
							failedAnyTasksOut
						));
					}
				}
			);

			LogMessage("Saving plug-in database");
			if (!newAvailablePluginDatabase.Save(Serialization::SavingFlags{}))
			{
				LogError("Failed to save plugin database");
				failedAnyTasksOut = true;
			}

			if (!Serialization::SerializeToDisk(
						IO::Path::Combine(buildDirectory, PackagedBundle::FileName),
						packagedBundle,
						Serialization::SavingFlags{}
					))
			{
				LogError("Failed to save packaged bundle");
				failedAnyTasksOut = true;
			}
		}

		// Compile the asset compiler plug-in first
		// This is necessary as the asset compiler may have assets that are required in order to compile other asset types
		LogMessage("Compiling asset compiler assets");
		assetDatabases.RemoveFirstOccurrencePredicate(
			[&assetCompiler, &currentThread, &failedAnyTasksOut, platform](NewDatabaseInfo& assetDatabase)
			{
				if (!assetDatabase.m_context.GetPlugin().IsValid() || assetDatabase.m_context.GetPlugin()->GetGuid() != AssetCompiler::Plugin::Guid)
				{
					return ErasePredicateResult::Continue;
				}

				CompileAssetsInDatabase(
					assetDatabase,
					assetCompiler,
					currentThread,
					failedAnyTasksOut,
					platform,
					assetDatabase.m_context,
					assetDatabase.m_sourceContext
				);
				return ErasePredicateResult::Remove;
			}
		);

		// Compile all metadata assets
		LogMessage("Compiling remaining assets");
		for (NewDatabaseInfo& assetDatabase : assetDatabases)
		{
			CompileAssetsInDatabase(
				assetDatabase,
				assetCompiler,
				currentThread,
				failedAnyTasksOut,
				platform,
				assetDatabase.m_context,
				assetDatabase.m_sourceContext
			);
		}

		// Copy binaries
		IO::Path binaryDirectory =
			IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), sourceContext.GetEngine()->GetRelativeBinaryDirectory(), platformName);
		const IO::Path configurationBinaryDirectory = IO::Path::Combine(binaryDirectory, buildConfiguration);
		if (buildDirectory != configurationBinaryDirectory)
		{
			LogMessage("Copying executable binaries");
			{
				const IO::Path launcherBinaryPath =
					IO::Path::Combine(configurationBinaryDirectory, IO::Path::Merge(MAKE_PATH("Editor"), IO::Library::ApplicationPostfix));
				const IO::Path targetBinaryDirectory =
					IO::Path::Combine(buildDirectory, sourceContext.GetEngine()->GetRelativeBinaryDirectory(), platformName, buildConfiguration);
				targetBinaryDirectory.CreateDirectories();

				failedAnyTasksOut |= !CopyExecutableBinaries(targetBinaryDirectory, launcherBinaryPath, configurationBinaryDirectory);
			}

			// Copy project system and asset compiler
			if constexpr (PLATFORM_DESKTOP && !PLATFORM_APPLE_MACOS)
			{
				const IO::Path projectSystemBinaryPath =
					IO::Path::Combine(binaryDirectory, IO::Path::Merge(MAKE_PATH("ProjectSystem"), IO::Library::ApplicationPostfix));
				const IO::Path targetBinaryDirectory =
					IO::Path::Combine(buildDirectory, sourceContext.GetEngine()->GetRelativeBinaryDirectory(), platformName);

				failedAnyTasksOut |= !CopyExecutableBinaries(targetBinaryDirectory, projectSystemBinaryPath, binaryDirectory);
			}
		}

		static constexpr bool copyPluginBinaries = false;
		if constexpr (copyPluginBinaries)
		{
			packagedBundle.GetPluginDatabase().IteratePlugins(
				[&failedAnyTasksOut,
			   platform,
			   buildConfiguration,
			   &availablePluginDatabase,
			   buildDirectory](const Asset::Guid pluginGuid, const IO::Path& configFilePath)
				{
					IO::Path sourcePluginPath = availablePluginDatabase.FindPlugin(pluginGuid);
					const PluginInfo sourcePlugin = PluginInfo(Move(sourcePluginPath));
					const PluginInfo targetPlugin = PluginInfo(IO::Path::Combine(buildDirectory, configFilePath));
					failedAnyTasksOut |= !CopyPluginBinaries(platform, buildConfiguration, sourcePlugin, targetPlugin);
				}
			);
		}
	}

	void EmplacePlugin(
		const IO::Path& path,
		FixedCapacityVector<NewDatabaseInfo>& assetDatabases,
		const EngineInfo& targetEngine,
		const Asset::Context& sourceContext,
		const IO::Path& buildDirectory,
		const EnginePluginDatabase availablePluginDatabase,
		EnginePluginDatabase& newAvailablePluginDatabase,
		PackagedBundle& packagedBundle,
		Threading::Atomic<bool>& failedAnyTasksOut
	)
	{
		PluginInfo plugin = PluginInfo(IO::Path(path));
		Assert(plugin.IsValid())
		if (plugin.IsValid())
		{
			Asset::Context pluginSourceContext = sourceContext;
			pluginSourceContext.SetPlugin(Move(plugin));

			// TODO: Context should include plugin dependencies
			assetDatabases.EmplaceBack(
				CopyPluginMetaData(targetEngine, pluginSourceContext, buildDirectory, newAvailablePluginDatabase, packagedBundle, failedAnyTasksOut)
			);

			for (const Guid dependencyPluginGuid : pluginSourceContext.GetPlugin()->GetDependencies())
			{
				if (!packagedBundle.GetPluginDatabase().HasPlugin(dependencyPluginGuid))
				{
					const IO::Path pluginPath = availablePluginDatabase.FindPlugin(dependencyPluginGuid);
					if (pluginPath.HasElements())
					{
						EmplacePlugin(
							pluginPath,
							assetDatabases,
							targetEngine,
							sourceContext,
							buildDirectory,
							availablePluginDatabase,
							newAvailablePluginDatabase,
							packagedBundle,
							failedAnyTasksOut
						);
					}
					else
					{
						LogError("Failed to find plug-in dependency {}", dependencyPluginGuid);
						failedAnyTasksOut = true;
					}
				}
			}
		}
	}

	void PackageStandaloneLauncher(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		const ConstNativeStringView buildConfiguration,
		Asset::Context&& sourceContext,
		IO::Path& buildDirectory,
		EnumFlags<ProjectSystem::PackagingFlags> packagingFlags
	)
	{
		const ConstNativeStringView platformName = Platform::GetName(platform);
		if (buildDirectory.IsEmpty())
		{
			buildDirectory = IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), MAKE_NATIVE_LITERAL("PackagedBuilds"), platformName);
		}

		if (packagingFlags.IsSet(PackagingFlags::CleanBuild))
		{
			if (buildDirectory.Exists())
			{
				LogMessage("Clearing build directory");
				buildDirectory.EmptyDirectoryRecursively();
			}
		}

		buildDirectory.CreateDirectories();

		EnginePluginDatabase availablePluginDatabase(
			IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), EnginePluginDatabase::FileName)
		);
		EnginePluginDatabase newAvailablePluginDatabase(IO::Path::Combine(buildDirectory, EnginePluginDatabase::FileName));
		newAvailablePluginDatabase.SetGuid(availablePluginDatabase.GetGuid());

		PackagedBundle packagedBundle;
		packagedBundle.GetEngineInfo() = EngineInfo{*sourceContext.GetEngine()};
		packagedBundle.GetPluginDatabase().SetFilePath(IO::Path::Combine(buildDirectory, EnginePluginDatabase::FileName));

		FixedCapacityVector<NewDatabaseInfo> assetDatabases(Memory::Reserve, 1 + availablePluginDatabase.GetCount());

		// Start by copying all metadata
		{
			LogMessage("Copying engine metadata");
			assetDatabases.EmplaceBack(CopyEngineMetaData(*sourceContext.GetEngine(), buildDirectory, packagedBundle, failedAnyTasksOut));
			const EngineInfo& targetEngine = *assetDatabases.GetLastElement().m_context.GetEngine();

			// Copy the app core plug-in, and its dependencies
			constexpr Guid appCorePluginGuid = "5D989100-CAB7-4213-98E6-D4949AFB2E10"_guid;
			const IO::Path appCorePluginPath = availablePluginDatabase.FindPlugin(appCorePluginGuid);

			EmplacePlugin(
				appCorePluginPath,
				assetDatabases,
				targetEngine,
				sourceContext,
				buildDirectory,
				availablePluginDatabase,
				newAvailablePluginDatabase,
				packagedBundle,
				failedAnyTasksOut
			);

			if (!newAvailablePluginDatabase.Save(Serialization::SavingFlags{}))
			{
				LogError("Failed to save plugin database");
				failedAnyTasksOut = true;
			}

			if (!Serialization::SerializeToDisk(
						IO::Path::Combine(buildDirectory, PackagedBundle::FileName),
						packagedBundle,
						Serialization::SavingFlags{}
					))
			{
				LogError("Failed to save packaged bundle");
				failedAnyTasksOut = true;
			}
		}

		// Compile the asset compiler plug-in first
		// This is necessary as the asset compiler may have assets that are required in order to compile other asset types
		LogMessage("Compiling asset compiler assets");
		assetDatabases.RemoveFirstOccurrencePredicate(
			[&assetCompiler, &currentThread, &failedAnyTasksOut, platform](NewDatabaseInfo& assetDatabase)
			{
				if (!assetDatabase.m_context.GetPlugin().IsValid() || assetDatabase.m_context.GetPlugin()->GetGuid() != AssetCompiler::Plugin::Guid)
				{
					return ErasePredicateResult::Continue;
				}

				CompileAssetsInDatabase(
					assetDatabase,
					assetCompiler,
					currentThread,
					failedAnyTasksOut,
					platform,
					assetDatabase.m_context,
					assetDatabase.m_sourceContext
				);
				return ErasePredicateResult::Remove;
			}
		);

		// Compile all metadata assets
		LogMessage("Compiling remaining assets");
		for (NewDatabaseInfo& assetDatabase : assetDatabases)
		{
			CompileAssetsInDatabase(
				assetDatabase,
				assetCompiler,
				currentThread,
				failedAnyTasksOut,
				platform,
				assetDatabase.m_context,
				assetDatabase.m_sourceContext
			);
		}

		// Copy binaries
		IO::Path binaryDirectory =
			IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), sourceContext.GetEngine()->GetRelativeBinaryDirectory(), platformName);
		const IO::Path configurationBinaryDirectory = IO::Path::Combine(binaryDirectory, buildConfiguration);
		if (buildDirectory != configurationBinaryDirectory)
		{
			LogMessage("Copying executable binaries");

			{
				const IO::Path launcherBinaryPath =
					IO::Path::Combine(configurationBinaryDirectory, IO::Path::Merge(MAKE_PATH("ProjectLauncher"), IO::Library::ApplicationPostfix));
				const IO::Path targetBinaryDirectory =
					IO::Path::Combine(buildDirectory, sourceContext.GetEngine()->GetRelativeBinaryDirectory(), platformName, buildConfiguration);
				targetBinaryDirectory.CreateDirectories();

				failedAnyTasksOut |= !CopyExecutableBinaries(targetBinaryDirectory, launcherBinaryPath, configurationBinaryDirectory);
			}
		}

		static constexpr bool copyPluginBinaries = false;
		if constexpr (copyPluginBinaries)
		{
			packagedBundle.GetPluginDatabase().IteratePlugins(
				[&failedAnyTasksOut,
			   platform,
			   buildConfiguration,
			   &availablePluginDatabase,
			   buildDirectory](const Asset::Guid pluginGuid, const IO::Path& configFilePath)
				{
					IO::Path sourcePluginPath = availablePluginDatabase.FindPlugin(pluginGuid);
					const PluginInfo sourcePlugin = PluginInfo(Move(sourcePluginPath));
					const PluginInfo targetPlugin = PluginInfo(IO::Path::Combine(buildDirectory, configFilePath));
					failedAnyTasksOut |= !CopyPluginBinaries(platform, buildConfiguration, sourcePlugin, targetPlugin);
				}
			);
		}
	}

	void PackageStandaloneProjectSystem(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		Asset::Context&& sourceContext,
		IO::Path& buildDirectory,
		EnumFlags<ProjectSystem::PackagingFlags> packagingFlags
	)
	{
		const ConstNativeStringView platformName = Platform::GetName(platform);
		if (buildDirectory.IsEmpty())
		{
			buildDirectory =
				IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), MAKE_NATIVE_LITERAL("PackagedProjectSystem"), platformName);
		}

		if (packagingFlags.IsSet(PackagingFlags::CleanBuild))
		{
			if (buildDirectory.Exists())
			{
				LogMessage("Clearing build directory");
				buildDirectory.EmptyDirectoryRecursively();
			}
		}

		buildDirectory.CreateDirectories();

		EnginePluginDatabase availablePluginDatabase(
			IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), EnginePluginDatabase::FileName)
		);
		EnginePluginDatabase newAvailablePluginDatabase(IO::Path::Combine(buildDirectory, EnginePluginDatabase::FileName));
		newAvailablePluginDatabase.SetGuid(availablePluginDatabase.GetGuid());

		PackagedBundle packagedBundle;
		packagedBundle.GetEngineInfo() = EngineInfo{*sourceContext.GetEngine()};
		packagedBundle.GetPluginDatabase().SetFilePath(IO::Path::Combine(buildDirectory, EnginePluginDatabase::FileName));

		FixedCapacityVector<NewDatabaseInfo> assetDatabases(Memory::Reserve, 1 + availablePluginDatabase.GetCount());

		// Start by copying all metadata
		{
			LogMessage("Copying engine metadata");
			assetDatabases.EmplaceBack(CopyEngineMetaData(*sourceContext.GetEngine(), buildDirectory, packagedBundle, failedAnyTasksOut));
			const EngineInfo& targetEngine = *assetDatabases.GetLastElement().m_context.GetEngine();

			LogMessage("Copying plug-in metadata");
			availablePluginDatabase.IteratePlugins(
				[buildDirectory, &newAvailablePluginDatabase, &packagedBundle, &assetDatabases, &targetEngine, &sourceContext, &failedAnyTasksOut](
					const Asset::Guid,
					const IO::Path& path
				)
				{
					PluginInfo plugin = PluginInfo(IO::Path(path));
					Assert(plugin.IsValid())
					if (plugin.IsValid())
					{
						Asset::Context pluginSourceContext = sourceContext;
						pluginSourceContext.SetPlugin(Move(plugin));

						// TODO: Context should include plugin dependencies
						assetDatabases.EmplaceBack(CopyPluginMetaData(
							targetEngine,
							pluginSourceContext,
							buildDirectory,
							newAvailablePluginDatabase,
							packagedBundle,
							failedAnyTasksOut
						));
					}
				}
			);

			if (!newAvailablePluginDatabase.Save(Serialization::SavingFlags{}))
			{
				LogError("Failed to save plugin database");
				failedAnyTasksOut = true;
			}

			if (!Serialization::SerializeToDisk(
						IO::Path::Combine(buildDirectory, PackagedBundle::FileName),
						packagedBundle,
						Serialization::SavingFlags{}
					))
			{
				LogError("Failed to save packaged bundle");
				failedAnyTasksOut = true;
			}
		}

		// Compile the asset compiler plug-in first
		// This is necessary as the asset compiler may have assets that are required in order to compile other asset types
		LogMessage("Compiling asset compiler assets");
		assetDatabases.RemoveFirstOccurrencePredicate(
			[&assetCompiler, &currentThread, &failedAnyTasksOut, platform](NewDatabaseInfo& assetDatabase)
			{
				if (!assetDatabase.m_context.GetPlugin().IsValid() || assetDatabase.m_context.GetPlugin()->GetGuid() != AssetCompiler::Plugin::Guid)
				{
					return ErasePredicateResult::Continue;
				}

				CompileAssetsInDatabase(
					assetDatabase,
					assetCompiler,
					currentThread,
					failedAnyTasksOut,
					platform,
					assetDatabase.m_context,
					assetDatabase.m_sourceContext
				);
				return ErasePredicateResult::Remove;
			}
		);

		// Compile all metadata assets
		LogMessage("Compiling remaining assets");
		for (NewDatabaseInfo& assetDatabase : assetDatabases)
		{
			CompileAssetsInDatabase(
				assetDatabase,
				assetCompiler,
				currentThread,
				failedAnyTasksOut,
				platform,
				assetDatabase.m_context,
				assetDatabase.m_sourceContext
			);
		}

		// Copy binaries
		IO::Path binaryDirectory =
			IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), sourceContext.GetEngine()->GetRelativeBinaryDirectory(), platformName);
		if (buildDirectory != binaryDirectory)
		{
			LogMessage("Copying executable binaries");
			const IO::Path projectSystemBinaryPath =
				IO::Path::Combine(binaryDirectory, IO::Path::Merge(MAKE_PATH("ProjectSystem"), IO::Library::ApplicationPostfix));
			failedAnyTasksOut |= !CopyExecutableBinaries(buildDirectory, projectSystemBinaryPath, binaryDirectory);
		}
	}

	void PackageStandaloneAssetCompiler(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		Asset::Context&& sourceContext,
		IO::Path& buildDirectory,
		EnumFlags<ProjectSystem::PackagingFlags> packagingFlags
	)
	{
		const ConstNativeStringView platformName = Platform::GetName(platform);
		if (buildDirectory.IsEmpty())
		{
			buildDirectory =
				IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), MAKE_NATIVE_LITERAL("PackagedAssetCompiler"), platformName);
		}

		if (packagingFlags.IsSet(PackagingFlags::CleanBuild))
		{
			if (buildDirectory.Exists())
			{
				LogMessage("Clearing build directory");
				buildDirectory.EmptyDirectoryRecursively();
			}
		}

		buildDirectory.CreateDirectories();

		EnginePluginDatabase availablePluginDatabase(
			IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), EnginePluginDatabase::FileName)
		);
		EnginePluginDatabase newAvailablePluginDatabase(IO::Path::Combine(buildDirectory, EnginePluginDatabase::FileName));
		newAvailablePluginDatabase.SetGuid(availablePluginDatabase.GetGuid());

		PackagedBundle packagedBundle;
		packagedBundle.GetEngineInfo() = EngineInfo{*sourceContext.GetEngine()};
		packagedBundle.GetPluginDatabase().SetFilePath(IO::Path::Combine(buildDirectory, EnginePluginDatabase::FileName));

		FixedCapacityVector<NewDatabaseInfo> assetDatabases(Memory::Reserve, 1 + availablePluginDatabase.GetCount());

		// Start by copying all metadata
		{
			LogMessage("Copying engine metadata");
			assetDatabases.EmplaceBack(CopyEngineMetaData(*sourceContext.GetEngine(), buildDirectory, packagedBundle, failedAnyTasksOut));
			const EngineInfo& targetEngine = *assetDatabases.GetLastElement().m_context.GetEngine();

			LogMessage("Copying plug-in metadata");
			availablePluginDatabase.IteratePlugins(
				[buildDirectory, &newAvailablePluginDatabase, &packagedBundle, &assetDatabases, &targetEngine, &sourceContext, &failedAnyTasksOut](
					const Asset::Guid,
					const IO::Path& path
				)
				{
					PluginInfo plugin = PluginInfo(IO::Path(path));
					Assert(plugin.IsValid())
					if (plugin.IsValid())
					{
						Asset::Context pluginSourceContext = sourceContext;
						pluginSourceContext.SetPlugin(Move(plugin));

						// TODO: Context should include plugin dependencies
						assetDatabases.EmplaceBack(CopyPluginMetaData(
							targetEngine,
							pluginSourceContext,
							buildDirectory,
							newAvailablePluginDatabase,
							packagedBundle,
							failedAnyTasksOut
						));
					}
				}
			);

			if (!newAvailablePluginDatabase.Save(Serialization::SavingFlags{}))
			{
				LogError("Failed to save plugin database");
				failedAnyTasksOut = true;
			}

			if (!Serialization::SerializeToDisk(
						IO::Path::Combine(buildDirectory, PackagedBundle::FileName),
						packagedBundle,
						Serialization::SavingFlags{}
					))
			{
				LogError("Failed to save packaged bundle");
				failedAnyTasksOut = true;
			}
		}

		// Compile the asset compiler plug-in first
		// This is necessary as the asset compiler may have assets that are required in order to compile other asset types
		LogMessage("Compiling asset compiler assets");
		assetDatabases.RemoveFirstOccurrencePredicate(
			[&assetCompiler, &currentThread, &failedAnyTasksOut, platform](NewDatabaseInfo& assetDatabase)
			{
				if (!assetDatabase.m_context.GetPlugin().IsValid() || assetDatabase.m_context.GetPlugin()->GetGuid() != AssetCompiler::Plugin::Guid)
				{
					return ErasePredicateResult::Continue;
				}

				CompileAssetsInDatabase(
					assetDatabase,
					assetCompiler,
					currentThread,
					failedAnyTasksOut,
					platform,
					assetDatabase.m_context,
					assetDatabase.m_sourceContext
				);
				return ErasePredicateResult::Remove;
			}
		);

		// Compile all metadata assets
		LogMessage("Compiling remaining assets");
		for (NewDatabaseInfo& assetDatabase : assetDatabases)
		{
			CompileAssetsInDatabase(
				assetDatabase,
				assetCompiler,
				currentThread,
				failedAnyTasksOut,
				platform,
				assetDatabase.m_context,
				assetDatabase.m_sourceContext
			);
		}

		// Copy binaries
		IO::Path binaryDirectory =
			IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), sourceContext.GetEngine()->GetRelativeBinaryDirectory(), platformName);
		if (buildDirectory != binaryDirectory)
		{
			LogMessage("Copying executable binaries");
			const IO::Path assetCompilerBinaryPath =
				IO::Path::Combine(binaryDirectory, IO::Path::Merge(MAKE_PATH("AssetCompiler"), IO::Library::ApplicationPostfix));
			failedAnyTasksOut |= !CopyExecutableBinaries(buildDirectory, assetCompilerBinaryPath, binaryDirectory);
		}
	}

	void PackageStandaloneTools(
		AssetCompiler::Plugin& assetCompiler,
		Threading::JobRunnerThread& currentThread,
		Threading::Atomic<bool>& failedAnyTasksOut,
		const Platform::Type platform,
		Asset::Context&& sourceContext,
		IO::Path& buildDirectory,
		EnumFlags<ProjectSystem::PackagingFlags> packagingFlags
	)
	{
		const ConstNativeStringView platformName = Platform::GetName(platform);
		if (buildDirectory.IsEmpty())
		{
			buildDirectory = IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), MAKE_NATIVE_LITERAL("PackagedTools"), platformName);
		}

		if (packagingFlags.IsSet(PackagingFlags::CleanBuild))
		{
			if (buildDirectory.Exists())
			{
				LogMessage("Clearing build directory");
				buildDirectory.EmptyDirectoryRecursively();
			}
		}

		buildDirectory.CreateDirectories();

		EnginePluginDatabase availablePluginDatabase(
			IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), EnginePluginDatabase::FileName)
		);
		EnginePluginDatabase newAvailablePluginDatabase(IO::Path::Combine(buildDirectory, EnginePluginDatabase::FileName));
		newAvailablePluginDatabase.SetGuid(availablePluginDatabase.GetGuid());

		PackagedBundle packagedBundle;
		packagedBundle.GetEngineInfo() = EngineInfo{*sourceContext.GetEngine()};
		packagedBundle.GetPluginDatabase().SetFilePath(IO::Path::Combine(buildDirectory, EnginePluginDatabase::FileName));

		FixedCapacityVector<NewDatabaseInfo> assetDatabases(Memory::Reserve, 1 + availablePluginDatabase.GetCount());

		// Start by copying all metadata
		{
			LogMessage("Copying engine metadata");
			assetDatabases.EmplaceBack(CopyEngineMetaData(*sourceContext.GetEngine(), buildDirectory, packagedBundle, failedAnyTasksOut));
			const EngineInfo& targetEngine = *assetDatabases.GetLastElement().m_context.GetEngine();

			LogMessage("Copying plug-in metadata");
			availablePluginDatabase.IteratePlugins(
				[buildDirectory, &newAvailablePluginDatabase, &packagedBundle, &assetDatabases, &targetEngine, &sourceContext, &failedAnyTasksOut](
					const Asset::Guid,
					const IO::Path& path
				)
				{
					PluginInfo plugin = PluginInfo(IO::Path(path));
					Assert(plugin.IsValid())
					if (plugin.IsValid())
					{
						Asset::Context pluginSourceContext = sourceContext;
						pluginSourceContext.SetPlugin(Move(plugin));

						// TODO: Context should include plugin dependencies
						assetDatabases.EmplaceBack(CopyPluginMetaData(
							targetEngine,
							pluginSourceContext,
							buildDirectory,
							newAvailablePluginDatabase,
							packagedBundle,
							failedAnyTasksOut
						));
					}
				}
			);

			if (!newAvailablePluginDatabase.Save(Serialization::SavingFlags{}))
			{
				LogError("Failed to save plugin database");
				failedAnyTasksOut = true;
			}

			if (!Serialization::SerializeToDisk(
						IO::Path::Combine(buildDirectory, PackagedBundle::FileName),
						packagedBundle,
						Serialization::SavingFlags{}
					))
			{
				LogError("Failed to save packaged bundle");
				failedAnyTasksOut = true;
			}
		}

		// Compile the asset compiler plug-in first
		// This is necessary as the asset compiler may have assets that are required in order to compile other asset types
		LogMessage("Compiling asset compiler assets");
		assetDatabases.RemoveFirstOccurrencePredicate(
			[&assetCompiler, &currentThread, &failedAnyTasksOut, platform](NewDatabaseInfo& assetDatabase)
			{
				if (!assetDatabase.m_context.GetPlugin().IsValid() || assetDatabase.m_context.GetPlugin()->GetGuid() != AssetCompiler::Plugin::Guid)
				{
					return ErasePredicateResult::Continue;
				}

				CompileAssetsInDatabase(
					assetDatabase,
					assetCompiler,
					currentThread,
					failedAnyTasksOut,
					platform,
					assetDatabase.m_context,
					assetDatabase.m_sourceContext
				);
				return ErasePredicateResult::Remove;
			}
		);

		// Compile all metadata assets
		LogMessage("Compiling remaining assets");
		for (NewDatabaseInfo& assetDatabase : assetDatabases)
		{
			CompileAssetsInDatabase(
				assetDatabase,
				assetCompiler,
				currentThread,
				failedAnyTasksOut,
				platform,
				assetDatabase.m_context,
				assetDatabase.m_sourceContext
			);
		}

		// Copy binaries
		IO::Path binaryDirectory =
			IO::Path::Combine(sourceContext.GetEngine()->GetDirectory(), sourceContext.GetEngine()->GetRelativeBinaryDirectory(), platformName);
		if (buildDirectory != binaryDirectory)
		{
			LogMessage("Copying asset compiler executable binaries");
			const IO::Path assetCompilerBinaryPath =
				IO::Path::Combine(binaryDirectory, IO::Path::Merge(MAKE_PATH("AssetCompiler"), IO::Library::ApplicationPostfix));
			failedAnyTasksOut |= !CopyExecutableBinaries(buildDirectory, assetCompilerBinaryPath, binaryDirectory);
			LogMessage("Copying project system executable binaries");
			const IO::Path projectSystemBinaryPath =
				IO::Path::Combine(binaryDirectory, IO::Path::Merge(MAKE_PATH("ProjectSystem"), IO::Library::ApplicationPostfix));
			failedAnyTasksOut |= !CopyExecutableBinaries(buildDirectory, projectSystemBinaryPath, binaryDirectory);
		}
	}
}
