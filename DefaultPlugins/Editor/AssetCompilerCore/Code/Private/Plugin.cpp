#include "AssetCompilerCore/Plugin.h"

#include "AssetCompilerCore/AssetCompilers/SceneObjectCompiler.h"
#include "AssetCompilerCore/AssetCompilers/Textures/GenericTextureCompiler.h"
#include "AssetCompilerCore/AssetCompilers/ShaderCompiler.h"
#include "AssetCompilerCore/AssetCompilers/AudioCompiler.h"
#include "AssetCompilerCore/AssetCompilers/ScriptCompiler.h"

#include "AssetCompilerCore/Packaging.h"
#include "AssetCompilerCore/SolutionGenerator.h"

#include <Common/Platform/Type.h>
#include <Common/Platform/GetName.h>
#include <Common/Asset/Asset.h>
#include <Common/Asset/AssetOwners.h>
#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/LocalAssetDatabase.h>
#include <Common/Project System/ProjectInfo.h>
#include <Common/Project System/EngineInfo.h>
#include <Common/Project System/EngineDatabase.h>
#include <Common/Project System/PluginDatabase.h>
#include <Common/Project System/ProjectDatabase.h>
#include <Common/Project System/FindEngine.h>
#include <Common/Project System/PackagedBundle.h>
#include <Common/Serialization/Serialize.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/Serialization/Guid.h>
#include <Common/Reflection/DynamicTypeDefinition.h>
#include <Common/Reflection/Registry.h>
#include <Common/IO/FileIterator.h>
#include <Common/Time/Timestamp.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/CommandLine/CommandLineArguments.h>
#include <Common/CommandLine/CommandLineArgumentsView.h>
#include <Common/CommandLine/CommandLineArguments.inl>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/IO/Format/Path.h>
#include <Common/IO/Log.h>
#include <Common/Format/Guid.h>

#include <Engine/Engine.h>
#include <Engine/Project/Project.h>
#include <Engine/Scene/Scene3DAssetType.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Tag/TagRegistry.h>

#include <Renderer/Assets/Material/MaterialAsset.h>
#include <Renderer/Assets/Material/MaterialAssetType.h>
#include <Renderer/Assets/Material/MaterialInstanceAsset.h>
#include <Renderer/Assets/Material/MaterialInstanceAssetType.h>
#include <Renderer/Assets/Texture/TextureAsset.h>
#include <Renderer/Assets/Texture/TextureAssetType.h>
#include <Renderer/Assets/Texture/RenderTargetAsset.h>
#include <Renderer/Assets/Texture/GetBestPresetFormat.h>
#include <Renderer/Assets/Shader/ShaderAsset.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>

#include <Widgets/WidgetAssetType.h>
#include <Widgets/Style/ComputedStylesheet.h>

#include <AudioCore/AudioAssetType.h>

#include <Common/System/Query.h>
#include <Common/Threading/AtomicBool.h>
#include <Engine/Scripting/ScriptAssetType.h>

namespace ngine::AssetCompiler
{
	void Plugin::OnLoaded(Application&)
	{
		if (const Optional<Asset::Manager*> pAssetManager = System::Find<Asset::Manager>())
		{
			pAssetManager->OnAssetsImported.Add(*this, &Plugin::OnAssetsImported);
		}
	}

	void Plugin::OnAssetsImported(const Asset::Mask& assets, const EnumFlags<Asset::ImportingFlags> flags)
	{
		if (flags.IsSet(Asset::ImportingFlags::SaveToDisk))
		{
			Engine& engine = System::Get<Engine>();
			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			for (const Asset::Identifier::IndexType assetIndex : assets.GetSetBitsIterator(0, assetManager.GetMaximumUsedElementCount()))
			{
				const Asset::Identifier assetIdentifier = Asset::Identifier::MakeFromValidIndex(assetIndex);
				const IO::Path assetPath = assetManager.GetAssetPath(assetIdentifier);
				Assert(assetPath.HasElements());
				if (LIKELY(assetPath.HasElements()))
				{
					Project& currentProject = System::Get<Project>();
					const Asset::Guid assetGuid = assetManager.GetAssetGuid(assetIdentifier);

					Asset::Owners assetOwners;
					if (currentProject.IsValid())
					{
						assetOwners = Asset::Owners(assetPath, Asset::Context(ProjectInfo{*currentProject.GetInfo()}, EngineInfo{engine.GetInfo()}));
					}
					else
					{
						assetOwners = Asset::Owners(assetPath, Asset::Context(ProjectInfo{}, EngineInfo{engine.GetInfo()}));
					}
					[[maybe_unused]] const bool wasAddedToProjectDatabase =
						AddImportedAssetToDatabase(assetGuid, assetOwners, Serialization::SavingFlags{});
					Assert(wasAddedToProjectDatabase);
				}
			}
		}
	}

	void ReconcilePlugin(
		const ngine::Guid pluginGuid,
		Vector<ngine::Guid>& reconciledPlugins,
		const EnginePluginDatabase& pluginDatabase,
		Threading::Atomic<bool>& failedAnyTasks,
		const EngineInfo& engineInfo,
		const EnumFlags<Platform::Type> platforms,
		AssetCompiler::Plugin& assetCompiler,
		CompileCallback& compileCallback,
		Threading::JobManager& jobManager
	)
	{
		if (reconciledPlugins.Contains(pluginGuid))
		{
			return;
		}
		reconciledPlugins.EmplaceBack(pluginGuid);

		IO::Path pluginPath = pluginDatabase.FindPlugin(pluginGuid);
		pluginPath = pluginPath.IsRelative() ? IO::Path::Combine(engineInfo.GetDirectory(), pluginPath) : pluginPath;

		const PluginInfo plugin = PluginInfo(Move(pluginPath));

		for (const ngine::Guid pluginDependencyGuid : plugin.GetDependencies())
		{
			ReconcilePlugin(
				pluginDependencyGuid,
				reconciledPlugins,
				pluginDatabase,
				failedAnyTasks,
				engineInfo,
				platforms,
				assetCompiler,
				compileCallback,
				jobManager
			);
		}
	};

	void Plugin::RunCommandLineOptions(
		const CommandLine::Arguments& commandLineArgs,
		const Asset::Context& context,
		Threading::JobManager& jobManager,
		CompileCallback&& compileCallback,
		Threading::Atomic<bool>& failedAnyTasks
	)
	{
		// Default -platform switch to all platforms if not explicitly specified
		const bool explicitlySpecifiedPlatform = commandLineArgs.HasArgument(MAKE_NATIVE_LITERAL("platform"), CommandLine::Prefix::Minus);
		EnumFlags<Platform::Type> platforms;
		if (explicitlySpecifiedPlatform)
		{
			commandLineArgs.IterateOptions(
				[&platforms](const CommandLine::ArgumentView command)
				{
					if (command.key == MAKE_NATIVE_LITERAL("platform"))
					{
						platforms |= Platform::GetFromName(command.value);
					}
				}
			);
		}
		else
		{
			platforms = Platform::Type::All;
		}

		IO::Path targetDirectory;
		if (OptionalIterator<const CommandLine::Argument> pTargetDirectory = commandLineArgs.FindArgument(MAKE_NATIVE_LITERAL("target_directory"), CommandLine::Prefix::Minus))
		{
			targetDirectory = IO::Path(pTargetDirectory->value.GetView());
		}

		EnumFlags<CompileFlags> compileFlags = AssetCompiler::CompileFlags::WasDirectlyRequested |
		                                       AssetCompiler::CompileFlags::SaveHumanReadable;
		compileFlags |= AssetCompiler::CompileFlags::ForceRecompile *
		                commandLineArgs.HasArgument(MAKE_NATIVE_LITERAL("force"), CommandLine::Prefix::Minus);

		commandLineArgs.IterateCommands(
			[&failedAnyTasks, this, compileFlags, &compileCallback, &jobManager, &context, platforms, &commandLineArgs](
				const CommandLine::ArgumentView command,
				const CommandLine::View commandArguments
			)
			{
				if (command.key == MAKE_NATIVE_LITERAL("compile_from_meta_asset"))
				{
					IO::Path assetMetaFilePath(command.value);
					const Asset::Owners assetOwners(assetMetaFilePath, Asset::Context{context});
					if(Threading::Job* pJob = CompileMetaDataAsset(
					   compileFlags,
					   CompileCallback(compileCallback),
					   *Threading::JobRunnerThread::GetCurrent(),
					   platforms,
					   Move(assetMetaFilePath),
					   assetOwners.m_context,
					   assetOwners.m_context
				   ))
					{
						pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("compile_source"))
				{
					const IO::Path sourcePath = IO::Path(command.value);

					IO::Path targetDirectory;
					if (const OptionalIterator<const CommandLine::Argument> targetDirectoryArgument = commandArguments.FindArgument(MAKE_NATIVE_LITERAL("target_directory"), CommandLine::Prefix::Minus))
					{
						targetDirectory = IO::Path(targetDirectoryArgument->value.GetView());
					}
					else
					{
						targetDirectory = IO::Path(sourcePath.GetParentPath());
					}

					if (Threading::Job* pJob = CompileAssetSourceFile(compileFlags, CompileCallback(compileCallback), *Threading::JobRunnerThread::GetCurrent(), platforms, IO::Path(sourcePath), targetDirectory, context, context))
					{
						pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("export_asset"))
				{
					IO::Path assetMetaFilePath(command.value);
					const Asset::Owners assetOwners(assetMetaFilePath, Asset::Context{context});
					Serialization::Data assetData(assetMetaFilePath);
					IO::Path targetFilePath{assetMetaFilePath.GetWithoutExtensions()};
					Asset::Asset asset(assetData, Move(assetMetaFilePath));
					if(Threading::Job* pJob = ExportAsset(
					   [targetFilePath = Move(targetFilePath), &failedAnyTasks](const ConstByteView data, const IO::PathView fileExtension)
                        {
                            if (data.HasElements())
                            {
                                const IO::Path finalTargetFilePath = IO::Path::Merge(targetFilePath, fileExtension);
                                IO::File targetFile(finalTargetFilePath, IO::AccessModeFlags::WriteBinary);
                                if (targetFile.IsValid())
                                {
                                    failedAnyTasks |= targetFile.Write(data) != data.GetDataSize();
                                }
                                else
                                {
                                    failedAnyTasks = true;
                                }
                            }
                            else
                            {
                                failedAnyTasks = true;
                            }
                        },
					   platforms,
                        Move(assetData),
                        Move(asset),
					   assetOwners.m_context
				   ))
					{
						pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("compile_asset_thumbnail"))
				{
					IO::Path assetMetaFilePath = IO::Path(command.value);
					const OptionalIterator<const CommandLine::Argument> thumbnailSourceFileArgument =
						commandArguments.FindArgument(MAKE_NATIVE_LITERAL("thumbnail_source_file"), CommandLine::Prefix::Minus);
					if (!thumbnailSourceFileArgument.IsValid())
					{
						failedAnyTasks = true;
						LogError("Missing -thumbnail_source_file switch");
						return;
					}

					const IO::Path sourceFilePath = IO::Path(thumbnailSourceFileArgument->value.GetView());
					IO::Path targetDirectory;
					if (const OptionalIterator<const CommandLine::Argument> targetDirectoryArgument = commandArguments.FindArgument(MAKE_NATIVE_LITERAL("target_directory"), CommandLine::Prefix::Minus))
					{
						targetDirectory = IO::Path(targetDirectoryArgument->value.GetView());
					}
					else
					{
						targetDirectory = IO::Path(sourceFilePath.GetParentPath());
					}

					auto callback = [compileCallback, &context, assetMetaFilePath = IO::Path(command.value)](
														const EnumFlags<AssetCompiler::CompileFlags> compileFlags,
														ArrayView<Asset::Asset> thumbnailAssets,
														ArrayView<const Serialization::Data> thumbnailAssetsData
													)
					{
						compileCallback(compileFlags, thumbnailAssets, thumbnailAssetsData);

						if (compileFlags.IsSet(AssetCompiler::CompileFlags::Compiled))
						{
							Serialization::Data assetData = Serialization::Data(IO::Path(assetMetaFilePath));
							Asset::Asset asset(assetData, IO::Path(assetMetaFilePath));
							asset.SetThumbnail(thumbnailAssets[0].GetGuid());
							Serialization::Serialize(assetData, asset);

							const EnumFlags<Serialization::SavingFlags> savingFlags = Serialization::SavingFlags::HumanReadable *
						                                                            compileFlags.IsSet(AssetCompiler::CompileFlags::SaveHumanReadable);
							[[maybe_unused]] const bool wasSaved = assetData.SaveToFile(asset.GetMetaDataFilePath(), savingFlags);
							Assert(wasSaved);

							const Asset::Owners assetOwners(assetMetaFilePath, Asset::Context{context});
							const IO::Path assetDatabasePath = assetOwners.GetDatabasePath();
							Serialization::Data assetDatabaseData(assetDatabasePath);
							Asset::Database assetDatabase(assetDatabaseData, assetDatabasePath.GetParentPath());
							assetDatabase.GetAssetEntry(asset.GetGuid())->m_thumbnailGuid = thumbnailAssets[0].GetGuid();
							[[maybe_unused]] const bool wasDatabaseSaved = assetDatabase.Save(assetDatabasePath, savingFlags);
							Assert(wasDatabaseSaved);
						}
					};

					const Asset::Owners assetOwners(assetMetaFilePath, Asset::Context{context});
					if(Threading::Job* pJob = CompileAssetSourceFile(
					   compileFlags,
					   callback,
					   *Threading::JobRunnerThread::GetCurrent(),
					   platforms,
					   IO::Path(sourceFilePath),
					   targetDirectory,
					   assetOwners.m_context,
					   assetOwners.m_context
				   ))
					{
						pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("compile_project_thumbnail"))
				{
					IO::Path assetMetaFilePath = IO::Path(command.value);
					const OptionalIterator<const CommandLine::Argument> thumbnailSourceFileArgument =
						commandArguments.FindArgument(MAKE_NATIVE_LITERAL("thumbnail_source_file"), CommandLine::Prefix::Minus);
					if (!thumbnailSourceFileArgument.IsValid())
					{
						failedAnyTasks = true;
						LogError("Missing -thumbnail_source_file switch");
						return;
					}

					const IO::Path sourceFilePath = IO::Path(thumbnailSourceFileArgument->value.GetView());
					IO::Path targetDirectory;
					if (const OptionalIterator<const CommandLine::Argument> targetDirectoryArgument = commandArguments.FindArgument(MAKE_NATIVE_LITERAL("target_directory"), CommandLine::Prefix::Minus))
					{
						targetDirectory = IO::Path(targetDirectoryArgument->value.GetView());
					}
					else
					{
						targetDirectory = IO::Path(sourceFilePath.GetParentPath());
					}

					auto callback = [compileCallback, projectMetaFilePath = IO::PathView(command.value)](
														const EnumFlags<AssetCompiler::CompileFlags> compileFlags,
														ArrayView<Asset::Asset> thumbnailAssets,
														ArrayView<const Serialization::Data> thumbnailAssetsData
													)
					{
						compileCallback(compileFlags, thumbnailAssets, thumbnailAssetsData);

						if (compileFlags.IsSet(AssetCompiler::CompileFlags::Compiled))
						{
							ProjectInfo projectInfo = ProjectInfo(IO::Path(projectMetaFilePath));
							projectInfo.SetThumbnail(thumbnailAssets[0].GetGuid());

							const EnumFlags<Serialization::SavingFlags> savingFlags = Serialization::SavingFlags::HumanReadable *
						                                                            compileFlags.IsSet(AssetCompiler::CompileFlags::SaveHumanReadable);
							Reflection::Registry reflectionRegistry(Reflection::Registry::Initializer::Initialize);
							[[maybe_unused]] const bool wasSaved =
								Serialization::SerializeToDisk(projectInfo.GetConfigFilePath(), projectInfo, savingFlags);
							Assert(wasSaved);

							const IO::Path assetDatabasePath = IO::Path::Combine(
								projectInfo.GetDirectory(),
								IO::Path::Merge(projectInfo.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
							);
							Serialization::Data assetDatabaseData(assetDatabasePath);
							Asset::Database assetDatabase(assetDatabaseData, assetDatabasePath.GetParentPath());
							assetDatabase.GetAssetEntry(projectInfo.GetGuid())->m_thumbnailGuid = thumbnailAssets[0].GetGuid();
							[[maybe_unused]] const bool wasDatabaseSaved = assetDatabase.Save(assetDatabasePath, savingFlags);
							Assert(wasDatabaseSaved);
						}
					};

					const Asset::Owners assetOwners(assetMetaFilePath, Asset::Context{context});
					if(Threading::Job* pJob = CompileAssetSourceFile(
					   compileFlags,
					   callback,
					   *Threading::JobRunnerThread::GetCurrent(),
					   platforms,
					   IO::Path(sourceFilePath),
					   targetDirectory,
					   assetOwners.m_context,
					   assetOwners.m_context
				   ))
					{
						pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("register_asset"))
				{
					Asset::LocalDatabase localAssetDatabase;
					IO::Path assetMetaFilePath = IO::Path(command.value);
					if (localAssetDatabase.RegisterAsset(assetMetaFilePath))
					{
						failedAnyTasks |= !localAssetDatabase.Save(Serialization::SavingFlags::HumanReadable);
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to register asset {}", assetMetaFilePath);
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("register_plugin"))
				{
					const PluginInfo plugin = PluginInfo(IO::Path(command.value));
					if (plugin.IsValid())
					{
						Asset::LocalDatabase localAssetDatabase;
						if (localAssetDatabase.RegisterPlugin(plugin))
						{
							failedAnyTasks |= !localAssetDatabase.Save(Serialization::SavingFlags::HumanReadable);
						}
						else
						{
							failedAnyTasks = true;
							LogError("Failed to register plug-in {}", plugin.GetConfigFilePath());
						}
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open plug-in {}", plugin.GetConfigFilePath());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("register_assets"))
				{
					Asset::LocalDatabase localAssetDatabase;

					const PluginInfo plugin = PluginInfo(IO::Path(command.value));
					if (plugin.IsValid())
					{
						if (localAssetDatabase.RegisterPluginAssets(plugin))
						{
							const IO::Path assetCacheDirectory = Asset::LocalDatabase::GetDirectoryPath();
							if (!assetCacheDirectory.Exists())
							{
								assetCacheDirectory.CreateDirectories();
							}

							failedAnyTasks |= !localAssetDatabase.Save(Serialization::SavingFlags::HumanReadable);
						}
						else
						{
							failedAnyTasks = true;
						}
					}
					else
					{
						failedAnyTasks = true;
					}
				}
				// Replaces all assets in a project's asset database that are found in the local asset database
				else if (command.key == MAKE_NATIVE_LITERAL("replace_project_local_assets_with_imports"))
				{
					const ProjectInfo project = ProjectInfo(IO::Path(command.value));
					if (LIKELY(project.IsValid()))
					{
						const IO::Path assetsDatabasePath = IO::Path::Combine(
							project.GetDirectory(),
							IO::Path::Merge(project.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
						);
						Asset::Database database(assetsDatabasePath, assetsDatabasePath.GetParentPath());
						Asset::LocalDatabase localAssetDatabase;

						database.ReplaceDuplicateAssetsWithImports(localAssetDatabase);

						failedAnyTasks |= !database.Save(assetsDatabasePath, Serialization::SavingFlags::HumanReadable);
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open project {}", project.GetConfigFilePath());
					}
				}
				// Replaces all assets in a project's asset database that are found in a plugin's asset database
				else if (command.key == MAKE_NATIVE_LITERAL("replace_project_local_assets_with_plugin_imports"))
				{
					const ProjectInfo project = ProjectInfo(IO::Path(command.value));
					if (LIKELY(project.IsValid()))
					{
						const IO::Path assetsDatabasePath = IO::Path::Combine(
							project.GetDirectory(),
							IO::Path::Merge(project.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
						);
						Asset::Database database(assetsDatabasePath, assetsDatabasePath.GetParentPath());
						Asset::LocalDatabase localAssetDatabase;

						database.ReplaceDuplicateAssetsWithImports(localAssetDatabase);

						failedAnyTasks |= !database.Save(assetsDatabasePath, Serialization::SavingFlags::HumanReadable);
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open plug-in {}", project.GetConfigFilePath());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("duplicate_asset"))
				{
					IO::Path existingAssetMetaFilePath = IO::Path(command.value);

					Asset::Owners assetOwners(existingAssetMetaFilePath, Asset::Context(context));

					IO::Path newAssetMetaFilePath = existingAssetMetaFilePath.GetDuplicated();

					failedAnyTasks |= !DuplicateAsset(
															 existingAssetMetaFilePath,
															 newAssetMetaFilePath,
															 assetOwners.m_context,
															 Serialization::SavingFlags::HumanReadable,
															 [](Serialization::Data&, Asset::Asset&)
															 {
															 }
					).IsValid();
				}
				else if (command.key == MAKE_NATIVE_LITERAL("generate_engine_asset_database"))
				{
					const EngineInfo engineInfo(IO::Path(command.value));
					if (LIKELY(engineInfo.IsValid()))
					{
						Asset::Database database;
						failedAnyTasks |= !database.LoadAndGenerate(engineInfo);

						const IO::Path assetsDatabasePath = IO::Path::Combine(
							engineInfo.GetDirectory(),
							IO::Path::Merge(EngineInfo::EngineAssetsPath, Asset::Database::AssetFormat.metadataFileExtension)
						);
						failedAnyTasks |= !database.Save(assetsDatabasePath, Serialization::SavingFlags::HumanReadable);
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open engine {}", engineInfo.GetConfigFilePath());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("generate_project_asset_database"))
				{
					const ProjectInfo projectInfo = ProjectInfo(IO::Path(command.value));
					if (LIKELY(projectInfo.IsValid()))
					{
						Asset::Database database;
						failedAnyTasks |= !database.LoadAndGenerate(projectInfo);

						const IO::Path assetsDatabasePath = IO::Path::Combine(
							projectInfo.GetDirectory(),
							IO::Path::Merge(projectInfo.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
						);
						failedAnyTasks |= !database.Save(assetsDatabasePath, Serialization::SavingFlags::HumanReadable);
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open project {}", projectInfo.GetConfigFilePath());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("generate_plugin_asset_database"))
				{
					const PluginInfo plugin = PluginInfo(IO::Path(command.value));
					if (LIKELY(plugin.IsValid()))
					{
						Asset::Database database;
						failedAnyTasks |= !database.LoadAndGenerate(plugin);

						const IO::Path assetsDatabasePath = IO::Path::Combine(
							plugin.GetDirectory(),
							IO::Path::Merge(plugin.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
						);
						failedAnyTasks |= !database.Save(assetsDatabasePath, Serialization::SavingFlags::HumanReadable);
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open plug-in {}", plugin.GetConfigFilePath());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("compile_all_engine_assets"))
				{
					EngineInfo engineInfo(IO::Path(command.value));
					if (LIKELY(engineInfo.IsValid()))
					{
						Asset::Context engineContext(Move(engineInfo));
						Asset::Database assetDatabase;
						assetDatabase.Load(*engineContext.GetEngine());

						assetDatabase.IterateAssets(
							[platforms, this, compileFlags, &compileCallback, &currentThread = *Threading::JobRunnerThread::GetCurrent(), &engineContext](
								[[maybe_unused]] const Asset::Guid assetGuid,
								const Asset::DatabaseEntry& assetEntry
							) -> Memory::CallbackResult
							{
								if(Threading::Job* pJob = CompileAnyAsset(
								   compileFlags,
								   CompileCallback(compileCallback),
								   currentThread,
								   platforms,
								   IO::Path(assetEntry.m_path),
								   engineContext,
								   engineContext
							   ))
								{
									pJob->Queue(currentThread);
								}

								return Memory::CallbackResult::Continue;
							}
						);
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open engine {}", engineInfo.GetConfigFilePath());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("compile_all_project_assets"))
				{
					ProjectInfo project = ProjectInfo(IO::Path(command.value));
					if (LIKELY(project.IsValid()))
					{
						Asset::Database assetDatabase;
						assetDatabase.Load(project);

						EngineInfo engineInfo = ProjectSystem::FindEngineFromProject(project, commandLineArgs);
						Asset::Context projectContext(Move(project), Move(engineInfo));

						assetDatabase.IterateAssets(
							[platforms,
					     this,
					     compileFlags,
					     &compileCallback,
					     &currentThread = *Threading::JobRunnerThread::GetCurrent(),
					     &projectContext]([[maybe_unused]] const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry)
							{
								if(Threading::Job* pJob = CompileAnyAsset(
								   compileFlags,
								   CompileCallback(compileCallback),
								   currentThread,
								   platforms,
								   IO::Path(assetEntry.m_path),
								   projectContext,
								   projectContext
							   ))
								{
									pJob->Queue(currentThread);
								}
								return Memory::CallbackResult::Continue;
							}
						);
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open project {}", project.GetConfigFilePath());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("compile_all_plugin_assets"))
				{
					PluginInfo plugin = PluginInfo(IO::Path(command.value));
					if (LIKELY(plugin.IsValid()))
					{
						Asset::Database assetDatabase;
						assetDatabase.Load(plugin);

						EngineInfo engineInfo = ProjectSystem::FindEngineFromPlugin(plugin, commandLineArgs);
						Asset::Context pluginContext(Move(plugin), Move(engineInfo));

						assetDatabase.IterateAssets(
							[platforms, this, compileFlags, &compileCallback, &currentThread = *Threading::JobRunnerThread::GetCurrent(), &pluginContext](
								[[maybe_unused]] const Asset::Guid assetGuid,
								const Asset::DatabaseEntry& assetEntry
							)
							{
								if(Threading::Job* pJob = CompileAnyAsset(
								   compileFlags,
								   CompileCallback(compileCallback),
								   currentThread,
								   platforms,
								   IO::Path(assetEntry.m_path),
								   pluginContext,
								   pluginContext
							   ))
								{
									pJob->Queue(currentThread);
								}
								return Memory::CallbackResult::Continue;
							}
						);
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open plug-in {}", plugin.GetConfigFilePath());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("reconcile_engine_dependencies"))
				{
					const EngineInfo engineInfo(IO::Path(command.value));
					if (LIKELY(engineInfo.IsValid()))
					{
						// Start with generating engine database
						{
							Asset::Database database;
							const bool loadedEngineInfo = database.LoadAndGenerate(engineInfo);
							if (UNLIKELY_ERROR(!loadedEngineInfo))
							{
								LogError("Failed to load and generator engine metadata at {}", command.value);
							}
							failedAnyTasks |= !loadedEngineInfo;

							const IO::Path assetsDatabasePath = IO::Path::Combine(
								engineInfo.GetDirectory(),
								IO::Path::Merge(EngineInfo::EngineAssetsPath, Asset::Database::AssetFormat.metadataFileExtension)
							);
							const bool savedDatabase = database.Save(assetsDatabasePath, Serialization::SavingFlags::HumanReadable);
							if (UNLIKELY_ERROR(!savedDatabase))
							{
								LogError("Failed to save engine asset database at {}", assetsDatabasePath);
							}
							failedAnyTasks |= !savedDatabase;

							Asset::Context engineContext = Asset::Context(EngineInfo(engineInfo));

							database.IterateAssets(
								[rootDirectory = assetsDatabasePath.GetParentPath(),
						     platforms,
						     this,
						     compileFlags,
						     &compileCallback,
						     &currentThread = *Threading::JobRunnerThread::GetCurrent(),
						     &engineContext]([[maybe_unused]] const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry)
									-> Memory::CallbackResult
								{
									if(Threading::Job* pJob = CompileAnyAsset(
									   compileFlags,
									   CompileCallback(compileCallback),
									   currentThread,
									   platforms,
									   IO::Path::Combine(rootDirectory, assetEntry.m_path),
									   engineContext,
									   engineContext
								   ))
									{
										pJob->Queue(currentThread);
									}

									return Memory::CallbackResult::Continue;
								}
							);
						}

						// Register all default engine plug-ins
						EnginePluginDatabase availablePluginDatabase(IO::Path::Combine(engineInfo.GetDirectory(), EnginePluginDatabase::FileName));
						availablePluginDatabase.RegisterAllDefaultPlugins(engineInfo.GetSourceDirectory());
						availablePluginDatabase.RegisterAllDefaultPlugins(engineInfo.GetCoreDirectory());
						const bool savedDefaultPlugins = availablePluginDatabase.Save(Serialization::SavingFlags::HumanReadable);
						if (UNLIKELY_ERROR(!savedDefaultPlugins))
						{
							LogError("Failed to save default plug-in database ");
						}
						failedAnyTasks |= !savedDefaultPlugins;

						struct PluginData
						{
							PluginInfo m_pluginInfo;
							Asset::Database m_assetDatabase;
							IO::Path m_assetDatabasePath;
						};

						FixedCapacityVector<PluginData> plugins(Memory::Reserve, availablePluginDatabase.GetCount());

						availablePluginDatabase.IteratePlugins(
							[&failedAnyTasks, &engineInfo, &plugins](const Asset::Guid, const IO::Path& pluginPath)
							{
								IO::Path absolutePluginPath = pluginPath.IsRelative() ? IO::Path::Combine(engineInfo.GetDirectory(), pluginPath)
						                                                          : pluginPath;
								PluginInfo plugin = PluginInfo(Move(absolutePluginPath));
								if (plugin.IsValid())
								{
									Asset::Database database;
									const bool generatedDatabase = database.LoadAndGenerate(plugin);
									if (UNLIKELY_ERROR(!generatedDatabase))
									{
										LogError("Failed to generate asset database for plug-in {}", plugin.GetConfigFilePath());
									}
									failedAnyTasks |= !generatedDatabase;

									IO::Path assetsDatabasePath =
										IO::Path::Merge(plugin.GetAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension);
									const bool savedDatabase = database.Save(assetsDatabasePath, Serialization::SavingFlags::HumanReadable);
									if (UNLIKELY_ERROR(!savedDatabase))
									{
										LogError("Failed to save asset database for plug-in {}", plugin.GetConfigFilePath());
									}
									failedAnyTasks |= !savedDatabase;

									plugins.EmplaceBack(PluginData{Move(plugin), Move(database), Move(assetsDatabasePath)});
								}
								else
								{
									failedAnyTasks = true;
									LogMessage("Encountered invalid plug-in at path {}", plugin.GetConfigFilePath());
								}
							}
						);

						Threading::JobBatch assetsJobBatch;
						// Compile the asset compiler plug-in first
					  // This is necessary as the asset compiler may have assets that are required in order to compile other asset types
						plugins.RemoveFirstOccurrencePredicate(
							[&engineInfo, platforms, this, compileFlags, &compileCallback, &assetsJobBatch](PluginData& pluginData)
							{
								if (pluginData.m_pluginInfo.GetGuid() != AssetCompiler::Plugin::Guid)
								{
									return ErasePredicateResult::Continue;
								}

								Asset::Context context = Asset::Context(Move(pluginData.m_pluginInfo), EngineInfo(engineInfo));

								pluginData.m_assetDatabase.IterateAssets(
									[rootDirectory = pluginData.m_assetDatabasePath.GetParentPath(),
						       platforms,
						       this,
						       compileFlags,
						       &compileCallback,
						       &currentThread = *Threading::JobRunnerThread::GetCurrent(),
						       &context,
						       &assetsJobBatch]([[maybe_unused]] const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry) mutable
									-> Memory::CallbackResult
									{
										if(Threading::Job* pJob = CompileAnyAsset(
										   compileFlags,
										   CompileCallback(compileCallback),
										   currentThread,
										   platforms,
										   IO::Path::Combine(rootDirectory, assetEntry.m_path),
										   context,
										   context
									   ))
										{
											assetsJobBatch.QueueAfterStartStage(*pJob);
										}

										return Memory::CallbackResult::Continue;
									}
								);
								return ErasePredicateResult::Remove;
							}
						);

						Threading::JobBatch pluginAssetsJobBatch;
						for (PluginData& pluginData : plugins)
						{
							Asset::Context pluginContext = Asset::Context(Move(pluginData.m_pluginInfo), EngineInfo(engineInfo));

							pluginData.m_assetDatabase.IterateAssets(
								[rootDirectory = pluginData.m_assetDatabasePath.GetParentPath(),
						     platforms,
						     this,
						     compileFlags,
						     &compileCallback,
						     &currentThread = *Threading::JobRunnerThread::GetCurrent(),
						     &pluginContext,
						     &pluginAssetsJobBatch]([[maybe_unused]] const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry)
									-> Memory::CallbackResult
								{
									if(Threading::Job* pJob = CompileAnyAsset(
									   compileFlags,
									   CompileCallback(compileCallback),
									   currentThread,
									   platforms,
									   IO::Path::Combine(rootDirectory, assetEntry.m_path),
									   pluginContext,
									   pluginContext
								   ))
									{
										pluginAssetsJobBatch.QueueAfterStartStage(*pJob);
									}

									return Memory::CallbackResult::Continue;
								}
							);
						}

						if (assetsJobBatch.IsValid())
						{
							assetsJobBatch.QueueAsNewFinishedStage(pluginAssetsJobBatch);
							Threading::JobRunnerThread::GetCurrent()->Queue(assetsJobBatch);
						}
						else
						{
							Threading::JobRunnerThread::GetCurrent()->Queue(pluginAssetsJobBatch);
						}
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open engine {}", engineInfo.GetConfigFilePath());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("reconcile_packaged_engine_dependencies"))
				{
					const EngineInfo engineInfo(IO::Path(command.value));
					if (LIKELY(engineInfo.IsValid()))
					{
						PackagedBundle packagedBundle;
						packagedBundle.GetEngineInfo() = EngineInfo{engineInfo};
						packagedBundle.GetPluginDatabase().SetFilePath(IO::Path::Combine(engineInfo.GetDirectory(), EnginePluginDatabase::FileName));

						// Start with generating engine database
						{
							Asset::Database database;
							const bool loadedEngineInfo = database.LoadAndGenerate(engineInfo);
							if (UNLIKELY_ERROR(!loadedEngineInfo))
							{
								LogError("Failed to load and generator engine metadata at {}", command.value);
							}
							failedAnyTasks |= !loadedEngineInfo;

							const IO::Path assetsDatabasePath = IO::Path::Combine(
								engineInfo.GetDirectory(),
								IO::Path::Merge(EngineInfo::EngineAssetsPath, Asset::Database::AssetFormat.metadataFileExtension)
							);
							const bool savedDatabase = database.Save(assetsDatabasePath, Serialization::SavingFlags::HumanReadable);
							if (UNLIKELY_ERROR(!savedDatabase))
							{
								LogError("Failed to save engine asset database at {}", assetsDatabasePath);
							}
							failedAnyTasks |= !savedDatabase;

							packagedBundle.AddAssetDatabase(
								IO::Path::Merge(EngineInfo::EngineAssetsPath, Asset::Database::AssetFormat.metadataFileExtension),
								engineInfo.GetDirectory(),
								Asset::Database{database}
							);

							Asset::Context engineContext = Asset::Context(EngineInfo(engineInfo));

							database.IterateAssets(
								[rootDirectory = assetsDatabasePath.GetParentPath(),
						     platforms,
						     this,
						     compileFlags,
						     &compileCallback,
						     &currentThread = *Threading::JobRunnerThread::GetCurrent(),
						     &engineContext]([[maybe_unused]] const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry)
									-> Memory::CallbackResult
								{
									if(Threading::Job* pJob = CompileAnyAsset(
									   compileFlags,
									   CompileCallback(compileCallback),
									   currentThread,
									   platforms,
									   IO::Path::Combine(rootDirectory, assetEntry.m_path),
									   engineContext,
									   engineContext
								   ))
									{
										pJob->Queue(currentThread);
									}

									return Memory::CallbackResult::Continue;
								}
							);
						}

						// Register all default engine plug-ins
						EnginePluginDatabase availablePluginDatabase(IO::Path::Combine(engineInfo.GetDirectory(), EnginePluginDatabase::FileName));
						availablePluginDatabase.RegisterAllDefaultPlugins(engineInfo.GetSourceDirectory());
						availablePluginDatabase.RegisterAllDefaultPlugins(engineInfo.GetCoreDirectory());
						packagedBundle.GetPluginDatabase() = PluginDatabase{availablePluginDatabase};
						const bool savedDefaultPlugins = availablePluginDatabase.Save(Serialization::SavingFlags::HumanReadable);
						if (UNLIKELY_ERROR(!savedDefaultPlugins))
						{
							LogError("Failed to save default plug-in database ");
						}
						failedAnyTasks |= !savedDefaultPlugins;

						struct PluginData
						{
							PluginInfo m_pluginInfo;
							Asset::Database m_assetDatabase;
							IO::Path m_assetDatabasePath;
						};

						FixedCapacityVector<PluginData> plugins(Memory::Reserve, availablePluginDatabase.GetCount());

						availablePluginDatabase.IteratePlugins(
							[&failedAnyTasks, &engineInfo, &plugins, &packagedBundle](const Asset::Guid, const IO::Path& pluginPath)
							{
								IO::Path absolutePluginPath = pluginPath.IsRelative() ? IO::Path::Combine(engineInfo.GetDirectory(), pluginPath)
						                                                          : pluginPath;
								PluginInfo plugin = PluginInfo(Move(absolutePluginPath));
								if (plugin.IsValid())
								{
									Asset::Database database;
									const bool generatedDatabase = database.LoadAndGenerate(plugin);
									if (UNLIKELY_ERROR(!generatedDatabase))
									{
										LogError("Failed to generate asset database for plug-in {}", plugin.GetConfigFilePath());
									}
									failedAnyTasks |= !generatedDatabase;

									IO::Path assetsDatabasePath =
										IO::Path::Merge(plugin.GetAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension);
									const bool savedDatabase = database.Save(assetsDatabasePath, Serialization::SavingFlags::HumanReadable);
									if (UNLIKELY_ERROR(!savedDatabase))
									{
										LogError("Failed to save asset database for plug-in {}", plugin.GetConfigFilePath());
									}
									failedAnyTasks |= !savedDatabase;

									packagedBundle.AddAssetDatabase(
										plugin.GetDirectory().GetRelativeToParent(engineInfo.GetDirectory()),
										plugin.GetDirectory(),
										Asset::Database{database}
									);

									PluginData& pluginData = plugins.EmplaceBack(PluginData{Move(plugin), Move(database), Move(assetsDatabasePath)});

									packagedBundle.AddPlugin(PluginInfo{pluginData.m_pluginInfo});
								}
								else
								{
									failedAnyTasks = true;
									LogMessage("Encountered invalid plug-in at path {}", plugin.GetConfigFilePath());
								}
							}
						);

						Threading::JobBatch assetsJobBatch;
						// Compile the asset compiler plug-in first
					  // This is necessary as the asset compiler may have assets that are required in order to compile other asset types
						plugins.RemoveFirstOccurrencePredicate(
							[&engineInfo, platforms, this, compileFlags, &compileCallback, &assetsJobBatch](PluginData& pluginData)
							{
								if (pluginData.m_pluginInfo.GetGuid() != AssetCompiler::Plugin::Guid)
								{
									return ErasePredicateResult::Continue;
								}

								Asset::Context context = Asset::Context(Move(pluginData.m_pluginInfo), EngineInfo(engineInfo));

								pluginData.m_assetDatabase.IterateAssets(
									[rootDirectory = pluginData.m_assetDatabasePath.GetParentPath(),
						       platforms,
						       this,
						       compileFlags,
						       &compileCallback,
						       &currentThread = *Threading::JobRunnerThread::GetCurrent(),
						       &context,
						       &assetsJobBatch]([[maybe_unused]] const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry) mutable
									-> Memory::CallbackResult
									{
										if(Threading::Job* pJob = CompileAnyAsset(
										   compileFlags,
										   CompileCallback(compileCallback),
										   currentThread,
										   platforms,
										   IO::Path::Combine(rootDirectory, assetEntry.m_path),
										   context,
										   context
									   ))
										{
											assetsJobBatch.QueueAfterStartStage(*pJob);
										}

										return Memory::CallbackResult::Continue;
									}
								);
								return ErasePredicateResult::Remove;
							}
						);

						Threading::JobBatch pluginAssetsJobBatch;
						for (PluginData& pluginData : plugins)
						{
							Asset::Context pluginContext = Asset::Context(Move(pluginData.m_pluginInfo), EngineInfo(engineInfo));

							pluginData.m_assetDatabase.IterateAssets(
								[rootDirectory = pluginData.m_assetDatabasePath.GetParentPath(),
						     platforms,
						     this,
						     compileFlags,
						     &compileCallback,
						     &currentThread = *Threading::JobRunnerThread::GetCurrent(),
						     &pluginContext,
						     &pluginAssetsJobBatch]([[maybe_unused]] const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry)
									-> Memory::CallbackResult
								{
									if(Threading::Job* pJob = CompileAnyAsset(
									   compileFlags,
									   CompileCallback(compileCallback),
									   currentThread,
									   platforms,
									   IO::Path::Combine(rootDirectory, assetEntry.m_path),
									   pluginContext,
									   pluginContext
								   ))
									{
										pluginAssetsJobBatch.QueueAfterStartStage(*pJob);
									}

									return Memory::CallbackResult::Continue;
								}
							);
						}

						if (assetsJobBatch.IsValid())
						{
							assetsJobBatch.QueueAsNewFinishedStage(pluginAssetsJobBatch);
							Threading::JobRunnerThread::GetCurrent()->Queue(assetsJobBatch);
						}
						else
						{
							Threading::JobRunnerThread::GetCurrent()->Queue(pluginAssetsJobBatch);
						}

						if (!Serialization::SerializeToDisk(
									IO::Path::Combine(engineInfo.GetDirectory(), PackagedBundle::FileName),
									packagedBundle,
									Serialization::SavingFlags{}
								))
						{
							LogError("Failed to save packaged bundle");
							failedAnyTasks = true;
						}
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open engine {}", engineInfo.GetConfigFilePath());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("reconcile_project_dependencies"))
				{
					const ProjectInfo projectInfo = ProjectInfo(IO::Path(command.value));
					if (LIKELY(projectInfo.IsValid()))
					{
						// Start with generating project asset database
						Asset::Database projectDatabase;
						failedAnyTasks |= !projectDatabase.LoadAndGenerate(projectInfo);

						const IO::Path projectAssetsDatabasePath = IO::Path::Combine(
							projectInfo.GetDirectory(),
							IO::Path::Merge(projectInfo.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
						);
						failedAnyTasks |= !projectDatabase.Save(projectAssetsDatabasePath, Serialization::SavingFlags::HumanReadable);

						EngineInfo engineInfo = ProjectSystem::FindEngineFromProject(projectInfo, commandLineArgs);
						if (engineInfo.IsValid())
						{
							// Compile project assets
							Asset::Context projectContext = Asset::Context(ProjectInfo(projectInfo), EngineInfo(engineInfo));
							projectDatabase.IterateAssets(
								[rootDirectory = projectAssetsDatabasePath.GetParentPath(),
						     platforms,
						     this,
						     compileFlags,
						     &compileCallback,
						     &currentThread = *Threading::JobRunnerThread::GetCurrent(),
						     &projectContext]([[maybe_unused]] const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry)
									-> Memory::CallbackResult
								{
									if(Threading::Job* pJob = CompileAnyAsset(
									   compileFlags,
									   CompileCallback(compileCallback),
									   currentThread,
									   platforms,
									   IO::Path::Combine(rootDirectory, assetEntry.m_path),
									   projectContext,
									   projectContext
								   ))
									{
										pJob->Queue(currentThread);
									}

									return Memory::CallbackResult::Continue;
								}
							);

							// Generating engine database
							{
								Asset::Database database;
								failedAnyTasks |= !database.LoadAndGenerate(engineInfo);

								const IO::Path assetsDatabasePath = IO::Path::Combine(
									engineInfo.GetDirectory(),
									IO::Path::Merge(EngineInfo::EngineAssetsPath, Asset::Database::AssetFormat.metadataFileExtension)
								);
								failedAnyTasks |= !database.Save(assetsDatabasePath, Serialization::SavingFlags::HumanReadable);

								// Compile assets
								Asset::Context engineContext = Asset::Context(EngineInfo(engineInfo));
								database.IterateAssets(
									[rootDirectory = assetsDatabasePath.GetParentPath(),
							     platforms,
							     this,
							     compileFlags,
							     &compileCallback,
							     &currentThread = *Threading::JobRunnerThread::GetCurrent(),
							     &engineContext]([[maybe_unused]] const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry)
										-> Memory::CallbackResult
									{
										if(Threading::Job* pJob = CompileAnyAsset(
										   compileFlags,
										   CompileCallback(compileCallback),
										   currentThread,
										   platforms,
										   IO::Path::Combine(rootDirectory, assetEntry.m_path),
										   engineContext,
										   engineContext
									   ))
										{
											pJob->Queue(currentThread);
										}

										return Memory::CallbackResult::Continue;
									}
								);
							}

							// Register all engine plug-ins
							EnginePluginDatabase availablePluginDatabase(IO::Path::Combine(engineInfo.GetDirectory(), EnginePluginDatabase::FileName));
							availablePluginDatabase.RegisterAllDefaultPlugins(engineInfo.GetSourceDirectory());
							availablePluginDatabase.RegisterAllDefaultPlugins(engineInfo.GetCoreDirectory());
							failedAnyTasks |= !availablePluginDatabase.Save(Serialization::SavingFlags::HumanReadable);

							Vector<ngine::Guid> reconciledPlugins;
							for (const ngine::Guid pluginGuid : projectInfo.GetPluginGuids())
							{
								ReconcilePlugin(
									pluginGuid,
									reconciledPlugins,
									availablePluginDatabase,
									failedAnyTasks,
									engineInfo,
									platforms,
									*this,
									compileCallback,
									jobManager
								);
							}
						}
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open project {}", projectInfo.GetConfigFilePath());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("reconcile_plugin_dependencies"))
				{
					const PluginInfo pluginInfo = PluginInfo(IO::Path(command.value));
					if (LIKELY(pluginInfo.IsValid()))
					{
						// Start with generating project asset database
						Asset::Database pluginDatabase;
						failedAnyTasks |= !pluginDatabase.LoadAndGenerate(pluginInfo);

						const IO::Path pluginAssetsDatabasePath = IO::Path::Combine(
							pluginInfo.GetDirectory(),
							IO::Path::Merge(pluginInfo.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
						);
						failedAnyTasks |= !pluginDatabase.Save(pluginAssetsDatabasePath, Serialization::SavingFlags::HumanReadable);

						EngineInfo engineInfo = ProjectSystem::FindEngineFromPlugin(pluginInfo, commandLineArgs);
						if (engineInfo.IsValid())
						{
							// Compile project assets
							Asset::Context pluginContext = Asset::Context(PluginInfo(pluginInfo), EngineInfo(engineInfo));
							pluginDatabase.IterateAssets(
								[rootDirectory = pluginAssetsDatabasePath.GetParentPath(),
						     platforms,
						     this,
						     compileFlags,
						     &compileCallback,
						     &currentThread = *Threading::JobRunnerThread::GetCurrent(),
						     &pluginContext]([[maybe_unused]] const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry)
									-> Memory::CallbackResult
								{
									if(Threading::Job* pJob = CompileAnyAsset(
									   compileFlags,
									   CompileCallback(compileCallback),
									   currentThread,
									   platforms,
									   IO::Path::Combine(rootDirectory, assetEntry.m_path),
									   pluginContext,
									   pluginContext
								   ))
									{
										pJob->Queue(currentThread);
									}

									return Memory::CallbackResult::Continue;
								}
							);

							// Generating engine database
							{
								Asset::Database database;
								failedAnyTasks |= !database.LoadAndGenerate(engineInfo);

								const IO::Path assetsDatabasePath = IO::Path::Combine(
									engineInfo.GetDirectory(),
									IO::Path::Merge(EngineInfo::EngineAssetsPath, Asset::Database::AssetFormat.metadataFileExtension)
								);
								failedAnyTasks |= !database.Save(assetsDatabasePath, Serialization::SavingFlags::HumanReadable);

								// Compile assets
								Asset::Context engineContext = Asset::Context(EngineInfo(engineInfo));
								database.IterateAssets(
									[rootDirectory = assetsDatabasePath.GetParentPath(),
							     platforms,
							     this,
							     compileFlags,
							     &compileCallback,
							     &currentThread = *Threading::JobRunnerThread::GetCurrent(),
							     &engineContext]([[maybe_unused]] const Asset::Guid assetGuid, const Asset::DatabaseEntry& assetEntry)
										-> Memory::CallbackResult
									{
										if(Threading::Job* pJob = CompileAnyAsset(
										   compileFlags,
										   CompileCallback(compileCallback),
										   currentThread,
										   platforms,
										   IO::Path::Combine(rootDirectory, assetEntry.m_path),
										   engineContext,
										   engineContext
									   ))
										{
											pJob->Queue(currentThread);
										}

										return Memory::CallbackResult::Continue;
									}
								);
							}

							// Register all engine plug-ins
							EnginePluginDatabase availablePluginDatabase(IO::Path::Combine(engineInfo.GetDirectory(), EnginePluginDatabase::FileName));
							availablePluginDatabase.RegisterAllDefaultPlugins(engineInfo.GetSourceDirectory());
							availablePluginDatabase.RegisterAllDefaultPlugins(engineInfo.GetCoreDirectory());
							failedAnyTasks |= !availablePluginDatabase.Save(Serialization::SavingFlags::HumanReadable);

							Vector<ngine::Guid> reconciledPlugins;

							for (const ngine::Guid pluginGuid : pluginInfo.GetDependencies())
							{
								ReconcilePlugin(
									pluginGuid,
									reconciledPlugins,
									availablePluginDatabase,
									failedAnyTasks,
									engineInfo,
									platforms,
									*this,
									compileCallback,
									jobManager
								);
							}
						}
					}
					else
					{
						failedAnyTasks = true;
						LogError("Failed to open plug-in {}", pluginInfo.GetConfigFilePath());
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("register_engine"))
				{
					IO::Path nativePath = IO::Path(command.value);
					nativePath.MakeNativeSlashes();

					EngineDatabase engineDatabase;
					if (engineDatabase.RegisterEngine(IO::Path(nativePath)))
					{
						failedAnyTasks |= !engineDatabase.Save(Serialization::SavingFlags::HumanReadable);
					}
					else
					{
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("register_project"))
				{
					IO::Path nativePath = IO::Path(command.value);
					nativePath.MakeNativeSlashes();

					EditableProjectDatabase projectDatabase;

					ProjectInfo projectInfo = ProjectInfo(IO::Path(nativePath));
					if (projectInfo.IsValid())
					{
						projectDatabase.RegisterProject(IO::Path(projectInfo.GetConfigFilePath()), projectInfo.GetGuid());
						projectDatabase.Save(Serialization::SavingFlags::HumanReadable);
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("create_project"))
				{
					IO::Path nativePath = IO::Path(command.value);
					nativePath.MakeNativeSlashes();

					const EngineInfo engineInfo = EngineInfo(IO::Path(nativePath));
					if (LIKELY(engineInfo.IsValid()))
					{
						IO::Path newProjectPath = IO::Path::Combine(
							engineInfo.GetDirectory(),
							MAKE_PATH("Projects"),
							IO::Path::Merge(MAKE_PATH("MyNewProject"), ProjectAssetFormat.metadataFileExtension),
							IO::Path::Merge(MAKE_PATH("Project"), ProjectAssetFormat.metadataFileExtension)
						);
						failedAnyTasks |= !CreateProject(
							MAKE_UNICODE_LITERAL("My New Project"),
							Move(newProjectPath),
							engineInfo.GetGuid(),
							Serialization::SavingFlags::HumanReadable
						);
					}
					else
					{
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("duplicate_project"))
				{
					IO::Path nativePath = IO::Path(command.value);
					nativePath.MakeNativeSlashes();

					const ProjectInfo projectInfo = ProjectInfo(IO::Path(nativePath));
					if (LIKELY(projectInfo.IsValid()))
					{
						const IO::Path assetDatabasePath = IO::Path::Combine(
							projectInfo.GetDirectory(),
							IO::Path::Merge(projectInfo.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
						);
						Serialization::Data assetDatabaseData(assetDatabasePath);
						Asset::Database assetDatabase(assetDatabaseData, assetDatabasePath.GetParentPath());
						if (UNLIKELY(!assetDatabaseData.IsValid()))
						{
							failedAnyTasks = true;
							return;
						}

						failedAnyTasks |= !CopyProject(
							projectInfo,
							Move(assetDatabase),
							UnicodeString(projectInfo.GetName()),
							IO::Path(projectInfo.GetConfigFilePath()),
							projectInfo.GetEngineGuid(),
							Serialization::SavingFlags::HumanReadable
						);
					}
					else
					{
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("create_asset_plugin"))
				{
					IO::Path nativePath = IO::Path(command.value);
					nativePath.MakeNativeSlashes();

					const EngineInfo engineInfo = EngineInfo(IO::Path(nativePath));
					if (LIKELY(engineInfo.IsValid()))
					{
						IO::Path newPluginPath = IO::Path::Combine(
							engineInfo.GetDirectory(),
							MAKE_PATH("CustomPlugins"),
							MAKE_PATH("MyNewPlugin"),
							IO::Path::Merge(MAKE_PATH("MyNewPlugin"), PluginAssetFormat.metadataFileExtension)
						);
						failedAnyTasks |= !CreateAssetPlugin(
																	 MAKE_UNICODE_LITERAL("My New Plugin"),
																	 Move(newPluginPath),
																	 engineInfo.GetGuid(),
																	 Serialization::SavingFlags::HumanReadable
																 )
											 .IsValid();
					}
					else
					{
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("create_code_plugin"))
				{
#if SUPPORT_GENERATE_CODE_PLUGIN
					IO::Path nativePath = IO::Path(command.value);
					nativePath.MakeNativeSlashes();

					const EngineInfo engineInfo = EngineInfo(IO::Path(nativePath));
					if (LIKELY(engineInfo.IsValid()))
					{
						IO::Path newPluginPath = IO::Path::Combine(
							engineInfo.GetDirectory(),
							MAKE_PATH("CustomPlugins"),
							MAKE_PATH("MyNewPlugin"),
							IO::Path::Merge(MAKE_PATH("MyNewPlugin"), PluginAssetFormat.metadataFileExtension)
						);
						failedAnyTasks |= !CreateCodePlugin(
																	 MAKE_UNICODE_LITERAL("My New Plugin"),
																	 Move(newPluginPath),
																	 engineInfo,
																	 Serialization::SavingFlags::HumanReadable
																 )
											 .IsValid();
					}
					else
#endif
					{
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("create_project_asset_plugin"))
				{
#if SUPPORT_GENERATE_CODE_PLUGIN
					IO::Path nativePath = IO::Path(command.value);
					nativePath.MakeNativeSlashes();

					ProjectInfo projectInfo = ProjectInfo(IO::Path(nativePath));
					if (LIKELY(projectInfo.IsValid()))
					{
						IO::Path newPluginPath = IO::Path::Combine(
							projectInfo.GetDirectory(),
							MAKE_PATH("Plugins"),
							MAKE_PATH("MyNewPlugin"),
							IO::Path::Merge(MAKE_PATH("MyNewPlugin"), PluginAssetFormat.metadataFileExtension)
						);

						failedAnyTasks |= !CreateAssetPlugin(
																	 MAKE_UNICODE_LITERAL("My New Plugin"),
																	 Move(newPluginPath),
																	 projectInfo,
																	 Serialization::SavingFlags::HumanReadable
																 )
											 .IsValid();
					}
					else
#endif
					{
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("create_project_code_plugin"))
				{
					IO::Path nativePath = IO::Path(command.value);
					nativePath.MakeNativeSlashes();

					ProjectInfo projectInfo = ProjectInfo(IO::Path(nativePath));
					if (LIKELY(projectInfo.IsValid()))
					{

						IO::Path newPluginPath = IO::Path::Combine(
							projectInfo.GetDirectory(),
							MAKE_PATH("Plugins"),
							MAKE_PATH("MyNewPlugin"),
							IO::Path::Merge(MAKE_PATH("MyNewPlugin"), PluginAssetFormat.metadataFileExtension)
						);
						failedAnyTasks |= !CreateCodePlugin(
																	 MAKE_UNICODE_LITERAL("My New Plugin"),
																	 Move(newPluginPath),
																	 projectInfo,
																	 Serialization::SavingFlags::HumanReadable
																 )
											 .IsValid();
					}
					else
					{
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("package_project"))
				{
					IO::Path nativePath(command.value);
					nativePath.MakeNativeSlashes();

					IO::Path buildDirectory;
					if (commandArguments.HasArgument(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus))
					{
						buildDirectory =
							IO::Path(commandArguments.GetArgumentValue(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus));
					}
					buildDirectory.MakeNativeSlashes();
					LogMessage("Preparing to package project to {}", buildDirectory);

					const ConstNativeStringView buildConfiguration =
						commandArguments.GetArgumentValue(MAKE_NATIVE_LITERAL("config"), CommandLine::Prefix::Minus);

					Asset::Context projectContext{context};
					projectContext.SetProject(ProjectInfo(IO::Path(nativePath)));
					projectContext.SetEngine(ProjectSystem::FindEngineFromProject(*projectContext.GetProject(), commandLineArgs));

					if (LIKELY(projectContext.GetProject().IsValid() & projectContext.GetEngine().IsValid()))
					{
						Assert(platforms.GetNumberOfSetFlags() == 1);

						EnumFlags<ProjectSystem::PackagingFlags> flags;
						// Whether to remove the package directory prior to packaging, to create a fully clean build
						flags |= ProjectSystem::PackagingFlags::CleanBuild *
								 commandArguments.HasArgument(MAKE_NATIVE_LITERAL("clean"), CommandLine::Prefix::Minus);
						PackageProjectLauncher(
							*this,
							*Threading::JobRunnerThread::GetCurrent(),
							failedAnyTasks,
							*platforms.GetFirstSetFlag(),
							buildConfiguration,
							projectContext,
							buildDirectory,
							flags
						);
					}
					else
					{
						LogError("Failed to find project");
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("package_project_editor"))
				{
					IO::Path nativePath(command.value);
					nativePath.MakeNativeSlashes();

					IO::Path buildDirectory;
					if (commandArguments.HasArgument(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus))
					{
						buildDirectory =
							IO::Path(commandArguments.GetArgumentValue(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus));
					}
					buildDirectory.MakeNativeSlashes();
					LogMessage("Preparing to package project editor to {}", buildDirectory);

					const ConstNativeStringView buildConfiguration =
						commandArguments.GetArgumentValue(MAKE_NATIVE_LITERAL("config"), CommandLine::Prefix::Minus);

					Asset::Context projectContext{context};
					projectContext.SetProject(ProjectInfo(IO::Path(nativePath)));
					projectContext.SetEngine(ProjectSystem::FindEngineFromProject(*projectContext.GetProject(), commandLineArgs));

					if (LIKELY(projectContext.GetProject().IsValid() & projectContext.GetEngine().IsValid()))
					{
						Assert(platforms.GetNumberOfSetFlags() == 1);

						EnumFlags<ProjectSystem::PackagingFlags> flags;
						// Whether to remove the package directory prior to packaging, to create a fully clean build
						flags |= ProjectSystem::PackagingFlags::CleanBuild *
								 commandArguments.HasArgument(MAKE_NATIVE_LITERAL("clean"), CommandLine::Prefix::Minus);
						PackageProjectEditor(
							*this,
							*Threading::JobRunnerThread::GetCurrent(),
							failedAnyTasks,
							*platforms.GetFirstSetFlag(),
							buildConfiguration,
							Move(projectContext),
							buildDirectory,
							flags
						);
					}
					else
					{
						LogError("Failed to find project");
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("package_editor"))
				{
					IO::Path nativePath(command.value);
					nativePath.MakeNativeSlashes();

					IO::Path buildDirectory;
					if (commandArguments.HasArgument(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus))
					{
						buildDirectory =
							IO::Path(commandArguments.GetArgumentValue(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus));
					}
					buildDirectory.MakeNativeSlashes();
					LogMessage("Preparing to package standalone editor to {}", buildDirectory);

					const ConstNativeStringView buildConfiguration =
						commandArguments.GetArgumentValue(MAKE_NATIVE_LITERAL("config"), CommandLine::Prefix::Minus);

					Asset::Context packagingContext{context};
					packagingContext.SetEngine(EngineInfo(IO::Path(nativePath)));

					if (LIKELY(packagingContext.GetEngine().IsValid()))
					{
						Assert(platforms.GetNumberOfSetFlags() == 1);

						EnumFlags<ProjectSystem::PackagingFlags> flags;
						// Whether to remove the package directory prior to packaging, to create a fully clean build
						flags |= ProjectSystem::PackagingFlags::CleanBuild *
								 commandArguments.HasArgument(MAKE_NATIVE_LITERAL("clean"), CommandLine::Prefix::Minus);
						PackageStandaloneEditor(
							*this,
							*Threading::JobRunnerThread::GetCurrent(),
							failedAnyTasks,
							*platforms.GetFirstSetFlag(),
							buildConfiguration,
							Move(packagingContext),
							buildDirectory,
							flags
						);
					}
					else
					{
						LogError("Failed to find engine");
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("package_launcher"))
				{
					IO::Path nativePath(command.value);
					nativePath.MakeNativeSlashes();

					IO::Path buildDirectory;
					if (commandArguments.HasArgument(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus))
					{
						buildDirectory =
							IO::Path(commandArguments.GetArgumentValue(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus));
					}
					buildDirectory.MakeNativeSlashes();
					LogMessage("Preparing to package standalone launcher to {}", buildDirectory);

					const ConstNativeStringView buildConfiguration =
						commandArguments.GetArgumentValue(MAKE_NATIVE_LITERAL("config"), CommandLine::Prefix::Minus);

					Asset::Context packagingContext{context};
					packagingContext.SetEngine(EngineInfo(IO::Path(nativePath)));

					if (LIKELY(packagingContext.GetEngine().IsValid()))
					{
						Assert(platforms.GetNumberOfSetFlags() == 1);

						EnumFlags<ProjectSystem::PackagingFlags> flags;
						// Whether to remove the package directory prior to packaging, to create a fully clean build
						flags |= ProjectSystem::PackagingFlags::CleanBuild *
								 commandArguments.HasArgument(MAKE_NATIVE_LITERAL("clean"), CommandLine::Prefix::Minus);
						PackageStandaloneLauncher(
							*this,
							*Threading::JobRunnerThread::GetCurrent(),
							failedAnyTasks,
							*platforms.GetFirstSetFlag(),
							buildConfiguration,
							Move(packagingContext),
							buildDirectory,
							flags
						);
					}
					else
					{
						LogError("Failed to find engine");
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("package_project_system"))
				{
					IO::Path nativePath(command.value);
					nativePath.MakeNativeSlashes();

					IO::Path buildDirectory;
					if (commandArguments.HasArgument(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus))
					{
						buildDirectory =
							IO::Path(commandArguments.GetArgumentValue(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus));
					}
					buildDirectory.MakeNativeSlashes();
					LogMessage("Preparing to package standalone project system to {}", buildDirectory);

					Asset::Context packagingContext{context};
					packagingContext.SetEngine(EngineInfo(IO::Path(nativePath)));

					if (LIKELY(packagingContext.GetEngine().IsValid()))
					{
						Assert(platforms.GetNumberOfSetFlags() == 1);

						EnumFlags<ProjectSystem::PackagingFlags> flags;
						// Whether to remove the package directory prior to packaging, to create a fully clean build
						flags |= ProjectSystem::PackagingFlags::CleanBuild *
								 commandArguments.HasArgument(MAKE_NATIVE_LITERAL("clean"), CommandLine::Prefix::Minus);
						PackageStandaloneProjectSystem(
							*this,
							*Threading::JobRunnerThread::GetCurrent(),
							failedAnyTasks,
							*platforms.GetFirstSetFlag(),
							Move(packagingContext),
							buildDirectory,
							flags
						);
					}
					else
					{
						LogError("Failed to find engine");
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("package_asset_compiler"))
				{
					IO::Path nativePath(command.value);
					nativePath.MakeNativeSlashes();

					IO::Path buildDirectory;
					if (commandArguments.HasArgument(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus))
					{
						buildDirectory =
							IO::Path(commandArguments.GetArgumentValue(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus));
					}
					buildDirectory.MakeNativeSlashes();
					LogMessage("Preparing to package standalone asset compiler to {}", buildDirectory);

					Asset::Context packagingContext{context};
					packagingContext.SetEngine(EngineInfo(IO::Path(nativePath)));

					if (LIKELY(packagingContext.GetEngine().IsValid()))
					{
						Assert(platforms.GetNumberOfSetFlags() == 1);

						EnumFlags<ProjectSystem::PackagingFlags> flags;
						// Whether to remove the package directory prior to packaging, to create a fully clean build
						flags |= ProjectSystem::PackagingFlags::CleanBuild *
								 commandArguments.HasArgument(MAKE_NATIVE_LITERAL("clean"), CommandLine::Prefix::Minus);
						PackageStandaloneAssetCompiler(
							*this,
							*Threading::JobRunnerThread::GetCurrent(),
							failedAnyTasks,
							*platforms.GetFirstSetFlag(),
							Move(packagingContext),
							buildDirectory,
							flags
						);
					}
					else
					{
						LogError("Failed to find engine");
						failedAnyTasks = true;
					}
				}
				else if (command.key == MAKE_NATIVE_LITERAL("package_tools"))
				{
					IO::Path nativePath(command.value);
					nativePath.MakeNativeSlashes();

					IO::Path buildDirectory;
					if (commandArguments.HasArgument(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus))
					{
						buildDirectory =
							IO::Path(commandArguments.GetArgumentValue(MAKE_NATIVE_LITERAL("package_directory"), CommandLine::Prefix::Minus));
					}
					buildDirectory.MakeNativeSlashes();
					LogMessage("Preparing to package standalone tools to {}", buildDirectory);

					Asset::Context packagingContext{context};
					packagingContext.SetEngine(EngineInfo(IO::Path(nativePath)));

					if (LIKELY(packagingContext.GetEngine().IsValid()))
					{
						Assert(platforms.GetNumberOfSetFlags() == 1);

						EnumFlags<ProjectSystem::PackagingFlags> flags;
						// Whether to remove the package directory prior to packaging, to create a fully clean build
						flags |= ProjectSystem::PackagingFlags::CleanBuild *
								 commandArguments.HasArgument(MAKE_NATIVE_LITERAL("clean"), CommandLine::Prefix::Minus);
						PackageStandaloneTools(
							*this,
							*Threading::JobRunnerThread::GetCurrent(),
							failedAnyTasks,
							*platforms.GetFirstSetFlag(),
							Move(packagingContext),
							buildDirectory,
							flags
						);
					}
					else
					{
						LogError("Failed to find engine");
						failedAnyTasks = true;
					}
				}
				else
				{
					failedAnyTasks = true;
					LogError("Encountered invalid command {}", command.key);
				}
			}
		);
	}

	namespace MetadataAsset
	{
		bool IsUpToDateInternal(const Asset::Asset& asset, const IO::Path& sourceFilePath)
		{
			return asset.GetMetaDataFilePath().Exists() &&
			       sourceFilePath.GetLastModifiedTime() <= asset.GetMetaDataFilePath().GetLastModifiedTime() &&
			       (IO::File(asset.GetMetaDataFilePath(), IO::AccessModeFlags::ReadBinary, IO::SharingFlags::DisallowWrite).GetSize() > 0);
		}

		bool IsUpToDate(
			[[maybe_unused]] const Platform::Type platform,
			[[maybe_unused]] const Serialization::Data&,
			const Asset::Asset& asset,
			[[maybe_unused]] const IO::Path& sourceFilePath,
			const Asset::Context& context
		);

		Threading::Job* Compile(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			[[maybe_unused]] Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& assetCompiler,
			[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
			[[maybe_unused]] Serialization::Data&& assetData,
			[[maybe_unused]] Asset::Asset&& asset,
			[[maybe_unused]] const IO::Path& sourceFilePath,
			const Asset::Context&,
			[[maybe_unused]] const Asset::Context& sourceContext
		);
	}

	namespace ProjectAsset
	{
		using MetadataAsset::Compile;
	}

	namespace EngineAsset
	{
		using MetadataAsset::Compile;
	}

	namespace PluginAsset
	{
		using MetadataAsset::Compile;
	}

	namespace DatabaseAsset
	{
		using MetadataAsset::Compile;
	}

	namespace WidgetAsset
	{
		Threading::Job* Compile(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			[[maybe_unused]] Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin&,
			const EnumFlags<Platform::Type>,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			[[maybe_unused]] const IO::Path& sourceFilePath,
			const Asset::Context&,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			asset.SetTypeGuid(WidgetAssetType::AssetFormat.assetTypeGuid);
			Serialization::Serialize(assetData, asset);
			callback(flags | (CompileFlags::Compiled), ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
			return nullptr;
		}
	}

	namespace MaterialAsset
	{
		Threading::Job* Compile(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			[[maybe_unused]] Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin&,
			const EnumFlags<Platform::Type>,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			[[maybe_unused]] const IO::Path& sourceFilePath,
			const Asset::Context&,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			Rendering::MaterialAsset materialAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));
			const bool result = materialAsset.Compile();
			Serialization::Serialize(assetData, materialAsset);
			callback(
				flags | (CompileFlags::Compiled * result),
				ArrayView<Asset::Asset>{materialAsset},
				ArrayView<const Serialization::Data>{assetData}
			);
			return nullptr;
		}
	}

	namespace MaterialInstanceAsset
	{
		Threading::Job* Compile(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			[[maybe_unused]] Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin&,
			const EnumFlags<Platform::Type>,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context&,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			UNUSED(sourceFilePath);

			Rendering::MaterialInstanceAsset materialInstanceAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));
			Serialization::Serialize(assetData, materialInstanceAsset);
			callback(
				flags | (CompileFlags::Compiled),
				ArrayView<Asset::Asset>{materialInstanceAsset},
				ArrayView<const Serialization::Data>{assetData}
			);
			return nullptr;
		}
	}

	namespace RenderTargetAsset
	{
		bool IsUpToDate(
			const Platform::Type platform,
			const Serialization::Data& assetData,
			const Asset::Asset& asset,
			const IO::Path& sourceFilePath,
			[[maybe_unused]] const Asset::Context& context
		)
		{
			const bool isUpToDate = MetadataAsset::IsUpToDateInternal(asset, sourceFilePath);

			Rendering::RenderTargetAsset renderTargetAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));
			if (renderTargetAsset.GetPreset() == Rendering::TexturePreset::Explicit)
			{
				return isUpToDate;
			}
			else
			{
				const EnumFlags<Rendering::TextureAsset::BinaryType> supportedBinaryTypes = Rendering::GetSupportedTextureBinaryTypes(platform);
				for (const Rendering::TextureAsset::BinaryType supportedBinaryType : supportedBinaryTypes)
				{
					const Rendering::Format preferredFormat = Rendering::GetBestPresetFormat(
						supportedBinaryType,
						renderTargetAsset.GetPreset(),
						// Indicate that the render target is uniform
						Math::Vector2ui{8u}
					);
					if (renderTargetAsset.GetBinaryAssetInfo(supportedBinaryType).GetFormat() != preferredFormat)
					{
						return false;
					}
				}

				return isUpToDate;
			}
		}

		Threading::Job* Compile(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			[[maybe_unused]] Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin&,
			const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			[[maybe_unused]] const IO::Path& sourceFilePath,
			const Asset::Context&,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			Assert(platforms.GetNumberOfSetFlags() == 1, "TODO: Support compiling multiple render target platforms at a time");
			const Platform::Type platform = *platforms.GetFirstSetFlag();

			Rendering::RenderTargetAsset renderTargetAsset(assetData, IO::Path(asset.GetMetaDataFilePath()));
			if (renderTargetAsset.GetPreset() != Rendering::TexturePreset::Explicit)
			{
				const EnumFlags<Rendering::TextureAsset::BinaryType> supportedBinaryTypes = Rendering::GetSupportedTextureBinaryTypes(platform);
				for (const Rendering::TextureAsset::BinaryType supportedBinaryType : supportedBinaryTypes)
				{
					const Rendering::Format preferredFormat = Rendering::GetBestPresetFormat(
						supportedBinaryType,
						renderTargetAsset.GetPreset(),
						// Indicate that the render target is uniform
						Math::Vector2ui{8u}
					);
					Rendering::RenderTargetAsset::BinaryInfo binaryInfo;
					binaryInfo.SetFormat(preferredFormat);
					renderTargetAsset.SetBinaryAssetInfo(supportedBinaryType, Move(binaryInfo));
				}
			}
			else
			{
				const EnumFlags<Rendering::TextureAsset::BinaryType> supportedBinaryTypes = Rendering::GetSupportedTextureBinaryTypes(platform);
				for (const Rendering::TextureAsset::BinaryType supportedBinaryType : supportedBinaryTypes)
				{
					if (UNLIKELY_ERROR(renderTargetAsset.GetBinaryAssetInfo(supportedBinaryType).GetFormat() == Rendering::Format::Invalid))
					{
						LogError("Render target had invalid format for binary type {}!", (uint32)supportedBinaryType);
						callback(flags, ArrayView<Asset::Asset>{renderTargetAsset}, ArrayView<const Serialization::Data>{assetData});
						return nullptr;
					}
				}
			}

			Serialization::Serialize(assetData, renderTargetAsset);

			callback(
				flags | (CompileFlags::Compiled),
				ArrayView<Asset::Asset>{renderTargetAsset},
				ArrayView<const Serialization::Data>{assetData}
			);
			return nullptr;
		}
	}

	namespace DynamicTypeDefinitionAsset
	{
		Threading::Job* Compile(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			[[maybe_unused]] Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin&,
			const EnumFlags<Platform::Type>,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context&,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			UNUSED(sourceFilePath);

			Reflection::DynamicTypeDefinition typeDefinition;

			if (Serialization::Deserialize(assetData, typeDefinition))
			{
				if (Serialization::Serialize(assetData, typeDefinition))
				{
					callback(flags | (CompileFlags::Compiled), ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
				}
				else
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
				}
			}
			else
			{
				callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
			}

			return nullptr;
		}
	}

	namespace ExistingBinaryAsset
	{
		Threading::Job* Compile(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			[[maybe_unused]] Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin&,
			[[maybe_unused]] const EnumFlags<Platform::Type> platforms,
			Serialization::Data&& assetData,
			Asset::Asset&& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context&,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			const IO::PathView targetFilePath = IO::PathView(
				asset.GetMetaDataFilePath().GetZeroTerminated(),
				asset.GetMetaDataFilePath().GetSize() - Asset::Asset::FileExtension.GetSize()
			);
			{
				const IO::Path directory{targetFilePath.GetParentPath()};
				if (!directory.Exists())
				{
					directory.CreateDirectories();
				}
			}

			if (sourceFilePath == targetFilePath)
			{
				if (asset.GetMetaDataFilePath().Exists())
				{
					callback(flags | CompileFlags::UpToDate, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
				}
				else
				{
					callback(flags | CompileFlags::Compiled, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
				}
				return nullptr;
			}

			const bool success = sourceFilePath.CopyFileTo(IO::Path(targetFilePath));
			callback(flags | (CompileFlags::Compiled * success), ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
			return nullptr;
		}

		bool IsUpToDate(
			const Platform::Type,
			const Serialization::Data&,
			const Asset::Asset& asset,
			const IO::Path& sourceFilePath,
			[[maybe_unused]] const Asset::Context& context
		)
		{
			if (!asset.GetMetaDataFilePath().Exists())
			{
				return false;
			}

			const IO::Path binaryTargetPath(
				asset.GetMetaDataFilePath().GetZeroTerminated(),
				asset.GetMetaDataFilePath().GetSize() - Asset::Asset::FileExtension.GetSize()
			);
			if (sourceFilePath == binaryTargetPath)
			{
				return true;
			}

			IO::File targetFile(binaryTargetPath, IO::AccessModeFlags::ReadBinary, IO::SharingFlags::DisallowWrite);
			if (!targetFile.IsValid())
			{
				return false;
			}

			IO::File sourceFile(sourceFilePath, IO::AccessModeFlags::ReadBinary, IO::SharingFlags::DisallowWrite);
			const IO::FileView::SizeType fileSize = sourceFile.GetSize();
			if (targetFile.GetSize() != fileSize)
			{
				return false;
			}

			Array<char, 256, IO::FileView::SizeType, IO::FileView::SizeType> sourceBuffer;
			Array<char, 256, IO::FileView::SizeType, IO::FileView::SizeType> targetBuffer;
			do
			{
				const IO::FileView::SizeType remainingSize = fileSize - sourceFile.Tell();
				[[maybe_unused]] const bool wasSourceRead = sourceFile.ReadIntoView(sourceBuffer.GetSubView(0, remainingSize));
				Assert(wasSourceRead);
				[[maybe_unused]] const bool wasTargetRead = targetFile.ReadIntoView(targetBuffer.GetSubView(0, remainingSize));
				Assert(wasTargetRead);
				if (sourceBuffer.GetDynamicView() != targetBuffer.GetDynamicView())
				{
					return false;
				}
			} while (!sourceFile.ReachedEnd());

			return true;
		}
	}

	// Temp until we can register asset formats dynamically
	inline static constexpr Asset::Format FontFileFormat = {
		"{C1CAE915-9D72-418C-9E61-9D60E71A5D96}"_guid,
		MAKE_PATH(".ttf.nasset"),
		MAKE_PATH(".ttf"),
	};
	inline static constexpr Asset::Format NetworkCertificateFileFormat = {
		"{6592DBC5-58BB-4059-BB71-06342E557CAC}"_guid,
		MAKE_PATH(".pem.nasset"),
		MAKE_PATH(".pem"),
	};

	inline static constexpr Array CompiledFileFormats = {
		SourceFileFormat{ProjectAssetFormat.metadataFileExtension, ProjectAssetFormat, ProjectAsset::Compile, MetadataAsset::IsUpToDate},
		SourceFileFormat{EngineAssetFormat.metadataFileExtension, EngineAssetFormat, EngineAsset::Compile, MetadataAsset::IsUpToDate},
		SourceFileFormat{PluginAssetFormat.metadataFileExtension, PluginAssetFormat, PluginAsset::Compile, MetadataAsset::IsUpToDate},
		SourceFileFormat{
			Asset::Database::AssetFormat.metadataFileExtension, Asset::Database::AssetFormat, DatabaseAsset::Compile, MetadataAsset::IsUpToDate
		},
		SourceFileFormat{
			WidgetAssetType::AssetFormat.metadataFileExtension, WidgetAssetType::AssetFormat, WidgetAsset::Compile, MetadataAsset::IsUpToDate
		},
		SourceFileFormat{
			MaterialAssetType::AssetFormat.metadataFileExtension,
			MaterialAssetType::AssetFormat,
			MaterialAsset::Compile,
			MetadataAsset::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".brdf.tex.nasset"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileBRDF,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			TextureAssetType::AssetFormat.metadataFileExtension,
			TextureAssetType::AssetFormat,
			MetadataAsset::Compile,
			Compilers::GenericTextureCompiler::IsUpToDate,
			// Temporarily using PNG since we don't support TIFF compilation on all platforms
			MAKE_PATH(".png"),                            // MAKE_PATH(".tiff"),
			Compilers::GenericTextureCompiler::ExportPNG, // Compilers::GenericTextureCompiler::ExportTIFF
		},
		SourceFileFormat{
			MaterialInstanceAssetType::AssetFormat.metadataFileExtension,
			MaterialInstanceAssetType::AssetFormat,
			MaterialInstanceAsset::Compile,
			MetadataAsset::IsUpToDate
		},
		SourceFileFormat{
			Rendering::RenderTargetAsset::AssetFormat.metadataFileExtension,
			Rendering::RenderTargetAsset::AssetFormat,
			RenderTargetAsset::Compile,
			RenderTargetAsset::IsUpToDate
		},
		SourceFileFormat{
			Reflection::DynamicTypeDefinition::AssetFormat.metadataFileExtension,
			Reflection::DynamicTypeDefinition::AssetFormat,
			DynamicTypeDefinitionAsset::Compile,
			MetadataAsset::IsUpToDate
		},
		SourceFileFormat{
			MeshPartAssetType::AssetFormat.metadataFileExtension,
			MeshPartAssetType::AssetFormat,
			MetadataAsset::Compile,
			MetadataAsset::IsUpToDate,
			MAKE_PATH(".fbx"),
			Compilers::MeshCompiler::Export
		},
		SourceFileFormat{
			MeshSceneAssetType::AssetFormat.metadataFileExtension,
			MeshSceneAssetType::AssetFormat,
			MetadataAsset::Compile,
			MetadataAsset::IsUpToDate,
			MAKE_PATH(".fbx"),
			Compilers::SceneObjectCompiler::Export
		},
		SourceFileFormat{
			Scene3DAssetType::AssetFormat.metadataFileExtension,
			Scene3DAssetType::AssetFormat,
			MetadataAsset::Compile,
			MetadataAsset::IsUpToDate,
			MAKE_PATH(".fbx"),
			Compilers::SceneObjectCompiler::Export
		},
		SourceFileFormat{Asset::Asset::FileExtension, Asset::AssetFormat, MetadataAsset::Compile, MetadataAsset::IsUpToDate},
		SourceFileFormat{
			MAKE_PATH(".3D"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".3DS"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".AC"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".AC3D"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".ACC"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".AMJ"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".ASE"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".ASK"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".B3D"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".BLEND"),
			Scene3DAssetType::AssetFormat,
			Compilers::SceneObjectCompiler::Compile,
			Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".BVH"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".CMS"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".COB"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".DAE"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".DXF"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".enff"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".fbx"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".gltf"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".glb"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".hmb"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".ifc-step"),
			Scene3DAssetType::AssetFormat,
			Compilers::SceneObjectCompiler::Compile,
			Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".irr"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".irrmesh"),
			Scene3DAssetType::AssetFormat,
			Compilers::SceneObjectCompiler::Compile,
			Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".lwo"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".lws"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".lxo"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".M3D"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".MD2"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".MD3"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".MD5"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".MDC"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".MDL"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".mesh"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".mesh.xml"),
			Scene3DAssetType::AssetFormat,
			Compilers::SceneObjectCompiler::Compile,
			Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".mot"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".ms3d"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".ndo"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".nff"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".obj"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".off"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".ogex"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".ply"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".pmx"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".prj"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".q30"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".q3s"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".raw"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".scn"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".sib"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".smd"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".stp"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".stl"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".ter"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".uc"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".vta"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".x"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".x3d"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".xgl"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".zgl"), Scene3DAssetType::AssetFormat, Compilers::SceneObjectCompiler::Compile, Compilers::SceneObjectCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".jpg"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileJPG,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".png"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompilePNG,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".psd"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompilePSD,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".tga"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileTGA,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".bmp"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileBMP,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".gif"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileGIF,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".hdr"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileHDR,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".exr"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileEXR,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".pic"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompilePIC,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".pnm"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompilePNM,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".tif"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileTIFF,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".tiff"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileTIFF,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".dds"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileDDS,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".ktx"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileKTX,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".ktx2"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileKTX2,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".svg"),
			TextureAssetType::AssetFormat,
			Compilers::GenericTextureCompiler::CompileSVG,
			Compilers::GenericTextureCompiler::IsUpToDate
		},
		SourceFileFormat{MAKE_PATH(".ogg"), Audio::AssetFormat, Compilers::AudioCompiler::CompileAudio, Compilers::AudioCompiler::IsUpToDate},
		SourceFileFormat{MAKE_PATH(".mp3"), Audio::AssetFormat, Compilers::AudioCompiler::CompileAudio, Compilers::AudioCompiler::IsUpToDate},
		SourceFileFormat{MAKE_PATH(".wav"), Audio::AssetFormat, Compilers::AudioCompiler::CompileAudio, Compilers::AudioCompiler::IsUpToDate},
		SourceFileFormat{MAKE_PATH(".flac"), Audio::AssetFormat, Compilers::AudioCompiler::CompileAudio, Compilers::AudioCompiler::IsUpToDate},
		SourceFileFormat{MAKE_PATH(".mpc"), Audio::AssetFormat, Compilers::AudioCompiler::CompileAudio, Compilers::AudioCompiler::IsUpToDate},
		SourceFileFormat{MAKE_PATH(".wv"), Audio::AssetFormat, Compilers::AudioCompiler::CompileAudio, Compilers::AudioCompiler::IsUpToDate},
		SourceFileFormat{
			Rendering::ShaderAsset::FragmentSourceFileExtension,
			Rendering::ShaderAsset::FragmentAssetFormat,
			Compilers::ShaderCompiler::CompileFragmentShader,
			Compilers::ShaderCompiler::IsUpToDate
		},
		SourceFileFormat{
			Rendering::ShaderAsset::VertexSourceFileExtension,
			Rendering::ShaderAsset::VertexAssetFormat,
			Compilers::ShaderCompiler::CompileVertexShader,
			Compilers::ShaderCompiler::IsUpToDate
		},
		SourceFileFormat{
			Rendering::ShaderAsset::GeometrySourceFileExtension,
			Rendering::ShaderAsset::GeometryAssetFormat,
			Compilers::ShaderCompiler::CompileGeometryShader,
			Compilers::ShaderCompiler::IsUpToDate
		},
		SourceFileFormat{
			Rendering::ShaderAsset::ComputeSourceFileExtension,
			Rendering::ShaderAsset::ComputeAssetFormat,
			Compilers::ShaderCompiler::CompileComputeShader,
			Compilers::ShaderCompiler::IsUpToDate
		},
		SourceFileFormat{
			Rendering::ShaderAsset::TessellationControlSourceFileExtension,
			Rendering::ShaderAsset::TessellationControlAssetFormat,
			Compilers::ShaderCompiler::CompileTessellationControlShader,
			Compilers::ShaderCompiler::IsUpToDate
		},
		SourceFileFormat{
			Rendering::ShaderAsset::TessellationEvaluationSourceFileExtension,
			Rendering::ShaderAsset::TessellationEvaluationAssetFormat,
			Compilers::ShaderCompiler::CompileTessellationEvaluationShader,
			Compilers::ShaderCompiler::IsUpToDate
		},
		SourceFileFormat{
			Rendering::ShaderAsset::RaytracingGenerationSourceFileExtension,
			Rendering::ShaderAsset::RaytracingGenerationAssetFormat,
			Compilers::ShaderCompiler::CompileRaytracingGenerationShader,
			Compilers::ShaderCompiler::IsUpToDate
		},
		SourceFileFormat{
			Rendering::ShaderAsset::RaytracingIntersectionSourceFileExtension,
			Rendering::ShaderAsset::RaytracingIntersectionAssetFormat,
			Compilers::ShaderCompiler::CompileRaytracingIntersectionShader,
			Compilers::ShaderCompiler::IsUpToDate
		},
		SourceFileFormat{
			Rendering::ShaderAsset::RaytracingAnyHitSourceFileExtension,
			Rendering::ShaderAsset::RaytracingAnyHitAssetFormat,
			Compilers::ShaderCompiler::CompileRaytracingAnyHitShader,
			Compilers::ShaderCompiler::IsUpToDate
		},
		SourceFileFormat{
			Rendering::ShaderAsset::RaytracingClosestHitSourceFileExtension,
			Rendering::ShaderAsset::RaytracingClosestHitAssetFormat,
			Compilers::ShaderCompiler::CompileRaytracingClosestHitShader,
			Compilers::ShaderCompiler::IsUpToDate
		},
		SourceFileFormat{
			Rendering::ShaderAsset::RaytracingMissSourceFileExtension,
			Rendering::ShaderAsset::RaytracingMissAssetFormat,
			Compilers::ShaderCompiler::CompileRaytracingMissShader,
			Compilers::ShaderCompiler::IsUpToDate
		},
		SourceFileFormat{
			Rendering::ShaderAsset::RaytracingCallableSourceFileExtension,
			Rendering::ShaderAsset::RaytracingCallableAssetFormat,
			Compilers::ShaderCompiler::CompileRaytracingCallableShader,
			Compilers::ShaderCompiler::IsUpToDate
		},
		SourceFileFormat{FontFileFormat.binaryFileExtension, FontFileFormat, ExistingBinaryAsset::Compile, ExistingBinaryAsset::IsUpToDate},
		SourceFileFormat{
			NetworkCertificateFileFormat.binaryFileExtension,
			NetworkCertificateFileFormat,
			ExistingBinaryAsset::Compile,
			ExistingBinaryAsset::IsUpToDate
		},
		SourceFileFormat{
			Widgets::Style::ComputedStylesheet::CSSAssetFormat.binaryFileExtension,
			Widgets::Style::ComputedStylesheet::CSSAssetFormat,
			ExistingBinaryAsset::Compile,
			ExistingBinaryAsset::IsUpToDate
		},
		SourceFileFormat{
			MAKE_PATH(".lua"),
			Scripting::ScriptAssetType::AssetFormat,
			Compilers::ScriptCompiler::CompileFromLua,
			Compilers::ScriptCompiler::IsUpToDate
		},
		SourceFileFormat{
			Scripting::ScriptAssetType::AssetFormat.metadataFileExtension,
			Scripting::ScriptAssetType::AssetFormat,
			Compilers::ScriptCompiler::CompileFromAST,
			Compilers::ScriptCompiler::IsUpToDate
		}
	};

	namespace MetadataAsset
	{
		bool IsUpToDate(
			const Platform::Type platform,
			const Serialization::Data& assetData,
			const Asset::Asset& asset,
			const IO::Path& sourceFilePath,
			const Asset::Context& context
		)
		{
			if (const Optional<Asset::Guid> sourceAssetGuid = asset.GetSourceAssetGuid())
			{
				Asset::Owners assetOwners(asset.GetMetaDataFilePath(), Asset::Context(context));
				const Asset::Database assetDatabase = assetOwners.m_context.ComputeFullDependencyDatabase();

				if (const Optional<const Asset::DatabaseEntry*> sourceAssetEntry = assetDatabase.GetAssetEntry(*sourceAssetGuid))
				{
					const Asset::Owners sourceAssetOwners(sourceAssetEntry->m_path, Asset::Context{context});
					Serialization::Data sourceAssetData(sourceAssetEntry->m_path);
					Asset::Asset sourceAsset(sourceAssetData, IO::Path(sourceAssetEntry->m_path));

					for (const SourceFileFormat& sourceFileFormat : CompiledFileFormats)
					{
						if (sourceAssetEntry->m_path.EndsWithExtensions(sourceFileFormat.sourceFileExtension))
						{
							return sourceFileFormat
							  .isUpToDateFunction(platform, sourceAssetData, sourceAsset, sourceAssetEntry->m_path, sourceAssetOwners.m_context);
						}
					}
				}
			}
			else if (const Optional<IO::PathView> sourceAssetPath = asset.GetSourceFilePath())
			{
				const IO::Path absoluteSourceFilePath(*asset.ComputeAbsoluteSourceFilePath());

				for (const SourceFileFormat& sourceFileFormat : CompiledFileFormats)
				{
					if (sourceAssetPath->EndsWithExtensions(sourceFileFormat.sourceFileExtension) && sourceFileFormat.isUpToDateFunction != &MetadataAsset::IsUpToDate)
					{
						return sourceFileFormat.isUpToDateFunction(platform, assetData, asset, absoluteSourceFilePath, context);
					}
				}
			}

			return IsUpToDateInternal(asset, sourceFilePath);
		}

		Threading::Job* Compile(
			const EnumFlags<CompileFlags> flags,
			CompileCallback&& callback,
			[[maybe_unused]] Threading::JobRunnerThread& currentThread,
			const AssetCompiler::Plugin& assetCompiler,
			[[maybe_unused]] EnumFlags<Platform::Type> platforms,
			[[maybe_unused]] Serialization::Data&& assetData,
			[[maybe_unused]] Asset::Asset&& asset,
			[[maybe_unused]] const IO::Path& sourceFilePath,
			const Asset::Context& context,
			[[maybe_unused]] const Asset::Context& sourceContext
		)
		{
			if (const Optional<Asset::Guid> sourceAssetGuid = asset.GetSourceAssetGuid())
			{
				Asset::Owners assetOwners(asset.GetMetaDataFilePath(), Asset::Context(context));
				const Asset::Database assetDatabase = assetOwners.m_context.ComputeFullDependencyDatabase();

				if (const Optional<const Asset::DatabaseEntry*> sourceAssetEntry = assetDatabase.GetAssetEntry(*sourceAssetGuid))
				{
					return assetCompiler.CompileMetaDataAsset(
						flags,
						Forward<CompileCallback>(callback),
						currentThread,
						platforms,
						IO::Path(sourceAssetEntry->m_path),
						context,
						sourceContext
					);
				}
				else
				{
					if (callback.IsValid())
					{
						callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
					}
					return nullptr;
				}
			}
			else if (const Optional<IO::PathView> sourceAssetPath = asset.GetSourceFilePath())
			{
				const IO::Path absoluteSourceFilePath(*asset.ComputeAbsoluteSourceFilePath());

				for (const SourceFileFormat& sourceFileFormat : CompiledFileFormats)
				{
					if (absoluteSourceFilePath.EndsWithExtensions(sourceFileFormat.sourceFileExtension))
					{
						if (flags.IsNotSet(CompileFlags::ForceRecompile))
						{
							for (const Platform::Type platform : platforms)
							{
								if (sourceFileFormat.isUpToDateFunction(platform, assetData, asset, absoluteSourceFilePath, context))
								{
									platforms &= ~platform;
								}
							}
						}

						if (!platforms.AreAnySet())
						{
							if (callback.IsValid())
							{
								callback(CompileFlags::UpToDate, ArrayView<Asset::Asset>{asset}, ArrayView<Serialization::Data>{assetData});
							}
							return nullptr;
						}

						if (sourceFileFormat.compileFunction != &MetadataAsset::Compile)
						{
							return sourceFileFormat.compileFunction(
								flags,
								Forward<CompileCallback>(callback),
								currentThread,
								assetCompiler,
								platforms,
								Forward<Serialization::Data>(assetData),
								Forward<Asset::Asset>(asset),
								absoluteSourceFilePath,
								context,
								sourceContext
							);
						}
						else
						{
							callback(flags | (CompileFlags::Compiled), ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
							return nullptr;
						}
					}
				}

				AssertMessage(false, "Requested asset compilation of unknown source file format {}", sourceAssetPath->GetAllExtensions());
				if (callback.IsValid())
				{
					callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
				}
				return nullptr;
			}

			callback(flags | (CompileFlags::Compiled), ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
			return nullptr;
		}
	}

	Threading::Job* Plugin::CompileAsset(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		Threading::JobRunnerThread& currentThread,
		EnumFlags<Platform::Type> platforms,
		Serialization::Data&& assetData,
		Asset::Asset&& asset,
		const SourceFileFormat& sourceFileFormat,
		const IO::Path& sourceFilePath,
		const Asset::Context& context,
		const Asset::Context& sourceContext
	) const
	{
		Assert(sourceFilePath.IsAbsolute());

		if (flags.IsNotSet(CompileFlags::ForceRecompile))
		{
			for (const Platform::Type platform : platforms)
			{
				if (sourceFileFormat.isUpToDateFunction(platform, assetData, asset, sourceFilePath, context))
				{
					platforms &= ~platform;
				}
			}
		}

		if (!platforms.AreAnySet())
		{
			if (callback.IsValid())
			{
				callback(CompileFlags::UpToDate, ArrayView<Asset::Asset>{asset}, ArrayView<Serialization::Data>{assetData});
			}
			return nullptr;
		}

		{
			Threading::UniqueLock lock(m_currentlyCompilingAssetsLock);
			if (m_currentlyCompilingAssets.Contains(asset.GetGuid()))
			{
				return nullptr;
			}
			m_currentlyCompilingAssets.Emplace(Asset::Guid(asset.GetGuid()));
		}

		const IO::Path mainAssetDirectory{asset.GetMetaDataFilePath().GetParentPath()};
		return sourceFileFormat.compileFunction(
			flags,
			[callback = Forward<CompileCallback>(callback), mainAssetDirectory](
				EnumFlags<CompileFlags> compileFlags,
				const ArrayView<Asset::Asset> assets,
				const ArrayView<const Serialization::Data> assetsData
			)
			{
				Plugin& assetCompiler = *System::FindPlugin<Plugin>();
				if (compileFlags.IsSet(CompileFlags::Compiled))
				{
					for (Asset::Asset& asset : assets)
					{
						const uint32 assetIndex = assets.GetIteratorIndex(Memory::GetAddressOf(asset));
						const Serialization::Data& assetData = assetsData[assetIndex];
						const EnumFlags<Serialization::SavingFlags> savingFlags = Serialization::SavingFlags::HumanReadable *
					                                                            compileFlags.IsSet(CompileFlags::SaveHumanReadable);
						if (assetData.SaveToFile(asset.GetMetaDataFilePath(), savingFlags))
						{
							const Asset::Owners assetOwners(asset.GetMetaDataFilePath(), Asset::Context());
							const bool isNewAsset = !asset.GetMetaDataFilePath().Exists() ||
						                          !assetCompiler.HasAssetInDatabase(asset.GetGuid(), assetOwners);
							if (isNewAsset)
							{
								if (assetOwners.HasOwner())
								{
									const bool addedToDatabase =
										assetCompiler.AddNewAssetToDatabase(Asset::DatabaseEntry{asset}, asset.GetGuid(), assetOwners, savingFlags);
									if (UNLIKELY_ERROR(!addedToDatabase))
									{
										compileFlags &= ~CompileFlags::Compiled;
										LogError(
											"Failed to add asset {} ({}) at {} to database",
											asset.GetName(),
											ngine::Guid(asset.GetGuid()),
											asset.GetMetaDataFilePath()
										);
										continue;
									}
								}

								assetCompiler.RegisterRuntimeAsset(asset, assetOwners);
							}
						}
						else
						{
							LogError("Failed to save asset {} ({}) to {}", asset.GetName(), ngine::Guid(asset.GetGuid()), asset.GetMetaDataFilePath());
							compileFlags &= ~CompileFlags::Compiled;
						}
					}
				}
				else if (compileFlags.IsSet(CompileFlags::IsCollection))
				{
					for (Asset::Asset& asset : assets)
					{
						if (asset.GetGuid().IsValid())
						{
							const Asset::Owners assetOwners(asset.GetMetaDataFilePath(), Asset::Context());
							assetCompiler.RegisterRuntimeAsset(asset, assetOwners);
						}
					}
				}
				else if (compileFlags.IsSet(CompileFlags::UpToDate))
					;
				else if (compileFlags.IsSet(CompileFlags::UnsupportedOnPlatform))
					;
				else if (assets.HasElements())
				{
					for (Asset::Asset& asset : assets)
					{
						LogError("Failed to compile asset {} ({})", asset.GetName(), ngine::Guid(asset.GetGuid()));
					}
					compileFlags &= ~CompileFlags::Compiled;
				}
				else
				{
					LogError("Failed to compile unknown asset");
					compileFlags &= ~CompileFlags::Compiled;
				}

				for (Asset::Asset& asset : assets)
				{
					assetCompiler.OnAssetFinishedCompilingInternal(asset.GetGuid());
				}

				if (callback.IsValid())
				{
					callback(compileFlags, assets, assetsData);
				}
			},
			currentThread,
			*this,
			platforms,
			Move(assetData),
			Move(asset),
			sourceFilePath,
			context,
			sourceContext
		);
	}

	void Plugin::OnAssetFinishedCompilingInternal(const Asset::Guid assetGuid)
	{
		Threading::UniqueLock lock(m_currentlyCompilingAssetsLock);
		auto it = m_currentlyCompilingAssets.Find(assetGuid);
		if (it != m_currentlyCompilingAssets.end())
		{
			m_currentlyCompilingAssets.Remove(it);
		}
	}

	Threading::Job* Plugin::CompileAsset(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		Threading::JobRunnerThread& currentThread,
		const EnumFlags<Platform::Type> platforms,
		Serialization::Data&& assetData,
		Asset::Asset&& asset,
		const Asset::Context& context,
		const Asset::Context& sourceContext
	) const
	{
		for (const SourceFileFormat& sourceFileFormat : CompiledFileFormats)
		{
			if (asset.GetMetaDataFilePath().EndsWithExtensions(sourceFileFormat.sourceFileExtension))
			{
				return CompileAsset(
					flags,
					Forward<CompileCallback>(callback),
					currentThread,
					platforms,
					Move(assetData),
					Move(asset),
					sourceFileFormat,
					asset.GetMetaDataFilePath(),
					context,
					sourceContext
				);
			}
		}

		AssertMessage(false, "Failed to compile asset {}", asset.GetMetaDataFilePath());
		if (callback.IsValid())
		{
			callback(flags, ArrayView<Asset::Asset>{asset}, ArrayView<const Serialization::Data>{assetData});
		}
		return nullptr;
	}

	Threading::Job* Plugin::CompileMetaDataAsset(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		Threading::JobRunnerThread& currentThread,
		const EnumFlags<Platform::Type> platforms,
		IO::Path&& filePath,
		const Asset::Context& context,
		const Asset::Context& sourceContext
	) const
	{
		Serialization::Data assetData(filePath);
		Asset::Asset asset(assetData, Move(filePath));
		const Asset::Owners owners(asset.GetMetaDataFilePath(), Asset::Context(context));

		return CompileAsset(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			platforms,
			Move(assetData),
			Move(asset),
			owners.m_context,
			sourceContext
		);
	}

	Threading::Job* Plugin::CompileAssetSourceFile(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		Threading::JobRunnerThread& currentThread,
		const EnumFlags<Platform::Type> platforms,
		IO::Path&& filePath,
		const IO::PathView targetDirectory,
		const Asset::Context& context,
		const Asset::Context& sourceContext
	) const
	{
		const IO::PathView fileExtension = filePath.GetRightMostExtension();
		Assert(fileExtension != Asset::Asset::FileExtension);

		for (const SourceFileFormat& sourceFileFormat : CompiledFileFormats)
		{
			if (sourceFileFormat.sourceFileExtension == fileExtension)
			{
				IO::PathView fileName = filePath.GetFileName();
				fileName = fileName.GetSubView(0, fileName.GetSize() - fileExtension.GetSize());

				IO::Path metaDataPath =
					IO::Path::Combine(targetDirectory, IO::Path::Merge(fileName, sourceFileFormat.m_assetFormat.metadataFileExtension));
				if (sourceFileFormat.m_assetFormat.flags.IsSet(Asset::Format::Flags::IsCollection))
				{
					metaDataPath = IO::Path::Combine(
						metaDataPath,
						IO::Path::Merge(filePath.GetFileNameWithoutExtensions(), sourceFileFormat.m_assetFormat.metadataFileExtension)
					);
					if (!metaDataPath.Exists())
					{
						metaDataPath = IO::Path::Combine(
							metaDataPath.GetParentPath(),
							IO::Path::Merge(MAKE_PATH("Main"), sourceFileFormat.m_assetFormat.metadataFileExtension)
						);
					}
				}
				Serialization::Data assetData(metaDataPath);
				Asset::Asset asset(assetData, Move(metaDataPath));

				// If this assertion hits then we should SetSourceAsset
				Assert(filePath.GetRightMostExtension() != Asset::Asset::FileExtension);
				asset.SetSourceFilePath(Move(filePath), context);
				Serialization::Serialize(assetData, asset);

				const IO::Path assetDirectory(asset.GetDirectory());
				if (!assetDirectory.Exists())
				{
					assetDirectory.CreateDirectories();
				}

				IO::Path absoluteSourceFilePath(*asset.ComputeAbsoluteSourceFilePath());
				return CompileAsset(
					flags,
					Forward<CompileCallback>(callback),
					currentThread,
					platforms,
					Move(assetData),
					Move(asset),
					sourceFileFormat,
					absoluteSourceFilePath,
					context,
					sourceContext
				);
			}
		}

		if (callback.IsValid())
		{
			callback(flags, ArrayView<Asset::Asset>{}, ArrayView<const Serialization::Data>{});
		}
		return nullptr;
	}

	Threading::Job* Plugin::CompileAnyAsset(
		const EnumFlags<CompileFlags> flags,
		CompileCallback&& callback,
		Threading::JobRunnerThread& currentThread,
		const EnumFlags<Platform::Type> platforms,
		IO::Path&& filePath,
		const Asset::Context& context,
		const Asset::Context& sourceContext,
		const IO::PathView targetDirectory
	) const
	{
		if (filePath.GetRightMostExtension() == Asset::Asset::FileExtension)
		{
			return CompileMetaDataAsset(
				flags,
				Forward<CompileCallback>(callback),
				currentThread,
				platforms,
				Move(filePath),
				context,
				sourceContext
			);
		}
		else if (filePath.GetRightMostExtension() == ProjectAssetFormat.metadataFileExtension)
		{
		}

		IO::Path metaDataPath = IO::Path::Merge(filePath, Asset::Asset::FileExtension);
		if (metaDataPath.Exists())
		{
			return CompileMetaDataAsset(
				flags,
				Forward<CompileCallback>(callback),
				currentThread,
				platforms,
				Move(metaDataPath),
				context,
				sourceContext
			);
		}

		// Treat the asset as a source file
		return CompileAssetSourceFile(
			flags,
			Forward<CompileCallback>(callback),
			currentThread,
			platforms,
			Move(filePath),
			targetDirectory.IsEmpty() ? filePath.GetParentPath() : targetDirectory,
			context,
			sourceContext
		);
	}

	Threading::Job* Plugin::ExportAsset(
		ExportedCallback&& callback,
		const EnumFlags<Platform::Type> platforms,
		Serialization::Data&& assetData,
		Asset::Asset&& asset,
		const Asset::Context& context
	) const
	{
		for (const SourceFileFormat& sourceFileFormat : CompiledFileFormats)
		{
			if (asset.GetMetaDataFilePath().EndsWithExtensions(sourceFileFormat.sourceFileExtension))
			{
				if (sourceFileFormat.exportFunction.IsValid())
				{
					return sourceFileFormat.exportFunction(
						Forward<ExportedCallback>(callback),
						platforms,
						Forward<Serialization::Data>(assetData),
						Forward<Asset::Asset>(asset),
						sourceFileFormat.m_exportedFileExtension,
						context
					);
				}
				else
				{
					continue;
				}
			}
		}

		AssertMessage(false, "Failed to export asset {}", asset.GetMetaDataFilePath());
		if (callback.IsValid())
		{
			callback({}, {});
		}
		return nullptr;
	}

	bool Plugin::AddNewAssetToDatabase(
		Asset::DatabaseEntry&& entry,
		const Asset::Guid assetGuid,
		const Asset::Owners& assetOwners,
		const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		if (!assetOwners.HasOwner())
		{
			return false;
		}

		const IO::Path ownerAssetDatabasePath = assetOwners.GetDatabasePath();
		const IO::PathView databaseRootDirectory = ownerAssetDatabasePath.GetParentPath();

		DatabaseLock* pFoundLock;
		{
			Threading::UniqueLock locksLock(m_databaseLocksLock);
			OptionalIterator<DatabaseLock> pLock = m_databaseLocks.FindIf(
				[ownerAssetDatabasePath = ownerAssetDatabasePath.GetView()](const DatabaseLock& lock)
				{
					return lock.path == ownerAssetDatabasePath;
				}
			);

			if (pLock)
			{
				pFoundLock = pLock;
			}
			else
			{
				pFoundLock = &m_databaseLocks.EmplaceBack(ownerAssetDatabasePath.GetView());
			}
		}

		Threading::UniqueLock lock(pFoundLock->lock);
		Serialization::Data databaseData(ownerAssetDatabasePath);

		Asset::Database database(databaseData, databaseRootDirectory);
		database.RegisterAsset(assetGuid, Forward<Asset::DatabaseEntry>(entry), databaseRootDirectory);
		return database.Save(ownerAssetDatabasePath, savingFlags);
	}

	bool Plugin::HasAssetInDatabase(const Asset::Guid assetGuid, const Asset::Owners& assetOwners)
	{
		if (!assetOwners.HasOwner())
		{
			return false;
		}

		const IO::Path ownerAssetDatabasePath = assetOwners.GetDatabasePath();
		const IO::PathView databaseRootDirectory = ownerAssetDatabasePath.GetParentPath();

		DatabaseLock* pFoundLock;
		{
			Threading::UniqueLock locksLock(m_databaseLocksLock);
			OptionalIterator<DatabaseLock> pLock = m_databaseLocks.FindIf(
				[ownerAssetDatabasePath = ownerAssetDatabasePath.GetView()](const DatabaseLock& lock)
				{
					return lock.path == ownerAssetDatabasePath;
				}
			);

			if (pLock)
			{
				pFoundLock = pLock;
			}
			else
			{
				pFoundLock = &m_databaseLocks.EmplaceBack(ownerAssetDatabasePath.GetView());
			}
		}

		Threading::UniqueLock lock(pFoundLock->lock);
		Serialization::Data databaseData(ownerAssetDatabasePath);
		if (databaseData.IsValid())
		{
			Asset::Database database(databaseData, databaseRootDirectory);
			return database.HasAsset(assetGuid);
		}
		else
		{
			return false;
		}
	}

	Guid Plugin::CreateAsset(
		const IO::Path& newAssetFilePath,
		const ngine::Guid assetTypeGuid,
		const Asset::Context context,
		const EnumFlags<Serialization::SavingFlags> flags,
		Function<void(Serialization::Data& assetData, Asset::Asset&), 64>&& callback
	)
	{
		if (!IO::Path(newAssetFilePath.GetParentPath()).Exists())
		{
			IO::Path(newAssetFilePath.GetParentPath()).CreateDirectories();
		}

		if (newAssetFilePath.Exists())
		{
			return {};
		}

		const IO::Path ownerAssetDatabasePath = context.GetDatabasePath();
		const IO::PathView databaseRootDirectory = ownerAssetDatabasePath.GetParentPath();
		Assert(newAssetFilePath.IsRelativeTo(databaseRootDirectory));

		DatabaseLock* pFoundLock;
		{
			Threading::UniqueLock locksLock(m_databaseLocksLock);
			OptionalIterator<DatabaseLock> pLock = m_databaseLocks.FindIf(
				[ownerAssetDatabasePath = ownerAssetDatabasePath.GetView()](const DatabaseLock& lock)
				{
					return lock.path == ownerAssetDatabasePath;
				}
			);

			if (pLock)
			{
				pFoundLock = pLock;
			}
			else
			{
				pFoundLock = &m_databaseLocks.EmplaceBack(ownerAssetDatabasePath.GetView());
			}
		}

		Threading::UniqueLock lock(pFoundLock->lock);
		Serialization::Data databaseData(ownerAssetDatabasePath);

		Asset::Database database(databaseData, databaseRootDirectory);

		Serialization::Data assetData(rapidjson::Type::kObjectType);
		assetData.SetContextFlags(Serialization::ContextFlags::ToDisk);

		Asset::Asset asset(assetData, IO::Path(newAssetFilePath));
		asset.SetTypeGuid(assetTypeGuid);
		Serialization::Serialize(assetData, asset);

		callback(assetData, asset);
		Asset::DatabaseEntry& databaseEntry = database.RegisterAsset(asset.GetGuid(), Asset::DatabaseEntry{asset}, databaseRootDirectory);

		Asset::Owners assetOwners(databaseEntry.m_path, Asset::Context(context));
		RegisterRuntimeAsset(asset, assetOwners);

		const bool saveResult = assetData.SaveToFile(newAssetFilePath, flags);
		Assert(saveResult);
		if (saveResult)
		{
			const bool wasDatabaseSaved = database.Save(ownerAssetDatabasePath, flags);
			Assert(wasDatabaseSaved);
			if (wasDatabaseSaved)
			{
				return asset.GetGuid();
			}
		}
		return {};
	}

	bool Plugin::ModifyAsset(
		const Asset::Guid assetGuid,
		const Asset::Context context,
		const EnumFlags<Serialization::SavingFlags> flags,
		Function<void(Serialization::Data& assetData, Asset::DatabaseEntry& entry), 64>&& callback
	)
	{
		const IO::Path ownerAssetDatabasePath = context.GetDatabasePath();
		const IO::PathView databaseRootDirectory = ownerAssetDatabasePath.GetParentPath();

		DatabaseLock* pFoundLock;
		{
			Threading::UniqueLock locksLock(m_databaseLocksLock);
			OptionalIterator<DatabaseLock> pLock = m_databaseLocks.FindIf(
				[ownerAssetDatabasePath = ownerAssetDatabasePath.GetView()](const DatabaseLock& lock)
				{
					return lock.path == ownerAssetDatabasePath;
				}
			);

			if (pLock)
			{
				pFoundLock = pLock;
			}
			else
			{
				pFoundLock = &m_databaseLocks.EmplaceBack(ownerAssetDatabasePath.GetView());
			}
		}

		Threading::UniqueLock lock(pFoundLock->lock);
		Serialization::Data databaseData(ownerAssetDatabasePath);

		Asset::Database database(databaseData, databaseRootDirectory);
		Optional<Asset::DatabaseEntry*> pEntry = database.GetAssetEntry(assetGuid);
		if (pEntry.IsValid())
		{
			Serialization::Data assetData(pEntry->m_path);
			assetData.SetContextFlags(Serialization::ContextFlags::ToDisk);
			callback(assetData, *pEntry);

			Asset::Asset asset(assetData, IO::Path(pEntry->m_path));
			*pEntry = Asset::DatabaseEntry(asset);
			Asset::Owners assetOwners(pEntry->m_path, Asset::Context(context));

			RegisterRuntimeAsset(asset, assetOwners);

			const bool saveResult = assetData.SaveToFile(pEntry->m_path, flags);
			Assert(saveResult);
			if (saveResult)
			{
				return database.Save(ownerAssetDatabasePath, flags);
			}
		}
		return false;
	}

	bool Plugin::AddImportedAssetToDatabase(
		const Asset::Guid assetGuid, const Asset::Owners& assetOwners, const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		const IO::Path ownerAssetDatabasePath = assetOwners.GetDatabasePath();
		const IO::PathView databaseRootDirectory = ownerAssetDatabasePath.GetParentPath();

		DatabaseLock* pFoundLock;
		{
			Threading::UniqueLock locksLock(m_databaseLocksLock);
			OptionalIterator<DatabaseLock> pLock = m_databaseLocks.FindIf(
				[ownerAssetDatabasePath = ownerAssetDatabasePath.GetView()](const DatabaseLock& lock)
				{
					return lock.path == ownerAssetDatabasePath;
				}
			);

			if (pLock)
			{
				pFoundLock = pLock;
			}
			else
			{
				pFoundLock = &m_databaseLocks.EmplaceBack(ownerAssetDatabasePath.GetView());
			}
		}

		Threading::UniqueLock lock(pFoundLock->lock);
		Serialization::Data databaseData(ownerAssetDatabasePath);

		Asset::Database database(databaseData, databaseRootDirectory);
		database.ImportAsset(assetGuid);
		return database.Save(ownerAssetDatabasePath, savingFlags);
	}

	bool Plugin::RemoveAssetFromDatabase(
		const Asset::Guid assetGuid, const Asset::Owners& assetOwners, const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		const IO::Path ownerAssetDatabasePath = assetOwners.GetDatabasePath();
		const IO::PathView databaseRootDirectory = ownerAssetDatabasePath.GetParentPath();

		DatabaseLock* pFoundLock;
		{
			Threading::UniqueLock locksLock(m_databaseLocksLock);
			OptionalIterator<DatabaseLock> pLock = m_databaseLocks.FindIf(
				[ownerAssetDatabasePath = ownerAssetDatabasePath.GetView()](const DatabaseLock& lock)
				{
					return lock.path == ownerAssetDatabasePath;
				}
			);

			if (pLock)
			{
				pFoundLock = pLock;
			}
			else
			{
				pFoundLock = &m_databaseLocks.EmplaceBack(ownerAssetDatabasePath.GetView());
			}
		}

		Threading::UniqueLock lock(pFoundLock->lock);
		Serialization::Data databaseData(ownerAssetDatabasePath);

		Asset::Database database(databaseData, databaseRootDirectory);
		Optional<Asset::DatabaseEntry> entry = database.RemoveAsset(assetGuid);
		if (entry.IsInvalid())
		{
			return false;
		}

		return database.Save(ownerAssetDatabasePath, savingFlags);
	}

	bool Plugin::CopySingleAssetInternal(
		const IO::Path& existingMetaDataPath,
		const Asset::Context& sourceContext,
		const IO::Path& newMetaDataPath,
		Asset::Owners& newAssetOwners,
		const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		Assert(existingMetaDataPath.IsFile());

		if (existingMetaDataPath == newMetaDataPath)
		{
			return false;
		}

		Serialization::Data assetData(existingMetaDataPath);
		if (!assetData.IsValid())
		{
			return false;
		}

		Asset::Asset asset(assetData, IO::Path(existingMetaDataPath));

		const Asset::Database& sourceAssetDatabase = *sourceContext.FindOrLoadDatabase();
		Asset::Database& targetAssetDatabase = *newAssetOwners.m_context.FindOrLoadDatabase();
		for (const Asset::Guid dependencyAssetGuid : asset.GetDependencies())
		{
			if (!targetAssetDatabase.HasAsset(dependencyAssetGuid))
			{
				if (const Optional<const Asset::DatabaseEntry*> pSourceAssetDependencyEntry = sourceAssetDatabase.GetAssetEntry(dependencyAssetGuid))
				{
					if (!CopyAssetInternal(
								pSourceAssetDependencyEntry->m_path,
								sourceContext,
								IO::Path::Combine(newMetaDataPath.GetParentPath(), pSourceAssetDependencyEntry->m_path.GetFileName()),
								newAssetOwners,
								savingFlags
							))
					{
						return false;
					}
				}
			}
		}

		IO::Path(newMetaDataPath.GetParentPath()).CreateDirectories();

		// This asset could have multiple (binary) files, e.g. '.tex.bc' and '.tex.astc'
		const IO::Path sourceBinaryFilePath = asset.GetBinaryFilePath();
		if (sourceBinaryFilePath.HasElements())
		{
			bool failedAny{false};
			const IO::Path sourceDirectory = IO::Path(existingMetaDataPath.GetParentPath());
			const IO::Path targetDirectory = IO::Path(newMetaDataPath.GetParentPath());
			IO::FileIterator::TraverseDirectory(
				sourceDirectory,
				[sourceBinaryFilePath,
			   sourceDirectory,
			   sourceMetadataFilePath = asset.GetMetaDataFilePath(),
			   newFileName = newMetaDataPath.GetFileNameWithoutExtensions(),
			   targetDirectory,
			   &failedAny](IO::Path&& sourceFilePath) -> IO::FileIterator::TraversalResult
				{
					if (sourceFilePath.GetView().StartsWith(sourceBinaryFilePath) && sourceFilePath != sourceMetadataFilePath)
					{
						const IO::PathView relativeBinaryPath = sourceFilePath.GetRelativeToParent(sourceDirectory);
						const IO::Path targetBinaryPath = IO::Path::Combine(
							targetDirectory,
							IO::Path::Merge(relativeBinaryPath.GetParentPath(), newFileName, relativeBinaryPath.GetAllExtensions())
						);
						if (LIKELY(!targetBinaryPath.Exists()))
						{
							failedAny |= !sourceFilePath.CopyFileTo(targetBinaryPath);
						}
					}
					return IO::FileIterator::TraversalResult::Continue;
				}
			);
			if (UNLIKELY_ERROR(failedAny))
			{
				return false;
			}
		}

		if (asset.IsSourceFilePathRelativeToAsset())
		{
			IO::Path sourceFilePath = *asset.ComputeAbsoluteSourceFilePath();
			asset.SetMetaDataFilePath(newMetaDataPath);

			sourceFilePath.MakeRelativeTo(newMetaDataPath.GetParentPath());

			// If this assertion hits then we should SetSourceAsset
			Assert(sourceFilePath.GetRightMostExtension() != Asset::Asset::FileExtension);
			asset.SetSourceFilePath(Move(sourceFilePath), newAssetOwners.m_context);
		}
		else
		{
			asset.SetMetaDataFilePath(newMetaDataPath);
		}

		if (!Serialization::Serialize(assetData, asset))
		{
			return false;
		}

		const bool saved = assetData.SaveToFile(newMetaDataPath, savingFlags);
		if (!saved)
		{
			return false;
		}

		if (!AddNewAssetToDatabase(Asset::DatabaseEntry{asset}, asset.GetGuid(), newAssetOwners, savingFlags))
		{
			return false;
		}

		targetAssetDatabase.RegisterAsset(asset.GetGuid(), Asset::DatabaseEntry{asset}, newAssetOwners.GetDatabaseRootDirectory());

		RegisterRuntimeAsset(asset, newAssetOwners);

		return true;
	}

	bool Plugin::CopyAssetInternal(
		const IO::Path& existingPath,
		const Asset::Context& sourceContext,
		const IO::Path& targetPath,
		Asset::Owners& newAssetOwners,
		const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		if (targetPath.Exists())
		{
			return false;
		}

		if (!IO::Path(targetPath.GetParentPath()).Exists())
		{
			IO::Path(targetPath.GetParentPath()).CreateDirectories();
		}

		if (existingPath.IsFile())
		{
			return CopySingleAssetInternal(existingPath, sourceContext, targetPath, newAssetOwners, savingFlags);
		}
		else
		{
			bool failedAny = false;
			IO::FileIterator::TraverseDirectoryRecursive(
				existingPath,
				[this, existingPath, &sourceContext, targetPath, &newAssetOwners, savingFlags, &failedAny](IO::Path&& existingFilePath
			  ) mutable -> IO::FileIterator::TraversalResult
				{
					if (existingFilePath.GetRightMostExtension() == Asset::Asset::FileExtension && existingFilePath.IsFile())
					{
						const IO::Path newPath = IO::Path::Combine(targetPath, existingFilePath.GetRelativeToParent(existingPath));
						failedAny |= !CopySingleAssetInternal(existingFilePath, sourceContext, newPath, newAssetOwners, savingFlags);
					}

					return IO::FileIterator::TraversalResult::Continue;
				}
			);
			return !failedAny;
		}
	}

	bool Plugin::CopyAsset(
		const IO::Path& existingPath,
		const Asset::Context& sourceContext,
		const IO::Path& targetPath,
		Asset::Context&& targetContext,
		const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		Asset::Owners assetOwners(existingPath, Forward<Asset::Context>(targetContext));
		return CopyAssetInternal(existingPath, sourceContext, targetPath, assetOwners, savingFlags);
	}

	bool Plugin::MoveAssetInternal(
		const IO::Path& existingMetaDataPath,
		const Asset::Owners& existingAssetOwners,
		const IO::Path& newMetaDataPath,
		const Asset::Owners& newAssetOwners,
		const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		Assert(existingMetaDataPath.IsFile());

		if (newMetaDataPath.Exists())
		{
			return false;
		}

		if (!IO::Path(newMetaDataPath.GetParentPath()).Exists())
		{
			IO::Path(newMetaDataPath.GetParentPath()).CreateDirectories();
		}

		if (existingMetaDataPath == newMetaDataPath)
		{
			return false;
		}

		Serialization::Data assetData(existingMetaDataPath);
		if (!assetData.IsValid())
		{
			return false;
		}

		Asset::Asset asset(assetData, IO::Path(existingMetaDataPath));

		IO::Path(newMetaDataPath.GetParentPath()).CreateDirectories();

		// This asset could have multiple (binary) files, e.g. '.tex.bc' and '.tex.astc'
		const IO::Path sourceBinaryFilePath = asset.GetBinaryFilePath();
		if (sourceBinaryFilePath.HasElements())
		{
			bool failedAny{false};
			const IO::Path sourceDirectory = IO::Path(existingMetaDataPath.GetParentPath());
			const IO::Path targetDirectory = IO::Path(newMetaDataPath.GetParentPath());
			IO::FileIterator::TraverseDirectory(
				sourceDirectory,
				[sourceBinaryFilePath,
			   sourceDirectory,
			   sourceMetadataFilePath = asset.GetMetaDataFilePath(),
			   newFileName = newMetaDataPath.GetFileNameWithoutExtensions(),
			   targetDirectory,
			   &failedAny](IO::Path&& sourceFilePath) -> IO::FileIterator::TraversalResult
				{
					if (sourceFilePath.GetView().StartsWith(sourceBinaryFilePath) && sourceFilePath != sourceMetadataFilePath)
					{
						const IO::PathView relativeBinaryPath = sourceFilePath.GetRelativeToParent(sourceDirectory);
						const IO::Path targetBinaryPath = IO::Path::Combine(
							targetDirectory,
							IO::Path::Merge(relativeBinaryPath.GetParentPath(), newFileName, relativeBinaryPath.GetAllExtensions())
						);
						if (LIKELY(!targetBinaryPath.Exists()))
						{
							failedAny |= !sourceFilePath.CopyFileTo(targetBinaryPath);
						}
					}
					return IO::FileIterator::TraversalResult::Continue;
				}
			);
			if (UNLIKELY_ERROR(failedAny))
			{
				return false;
			}
		}

		if (asset.IsSourceFilePathRelativeToAsset())
		{
			IO::Path sourceFilePath = *asset.ComputeAbsoluteSourceFilePath();
			asset.SetMetaDataFilePath(newMetaDataPath);

			sourceFilePath.MakeRelativeTo(newMetaDataPath.GetParentPath());

			// If this assertion hits then we should SetSourceAsset
			Assert(sourceFilePath.GetRightMostExtension() != Asset::Asset::FileExtension);
			asset.SetSourceFilePath(Move(sourceFilePath), newAssetOwners.m_context);
		}
		else
		{
			asset.SetMetaDataFilePath(newMetaDataPath);
		}

		if (!Serialization::Serialize(assetData, asset))
		{
			return false;
		}

		const bool saved = assetData.SaveToFile(newMetaDataPath, savingFlags);
		if (!saved)
		{
			return false;
		}

		if (!AddNewAssetToDatabase(Asset::DatabaseEntry{asset}, asset.GetGuid(), newAssetOwners, savingFlags))
		{
			return false;
		}

		RegisterRuntimeAsset(asset, newAssetOwners);

		if (existingAssetOwners.GetDatabasePath() != newAssetOwners.GetDatabasePath())
		{
			if (!RemoveAssetFromDatabase(asset.GetGuid(), existingAssetOwners, savingFlags))
			{
				return false;
			}
		}

		if (!existingMetaDataPath.RemoveFile())
		{
			return false;
		}

		// Finally remove binaries from the original asset
		const IO::Path targetBinaryFilePath = asset.GetBinaryFilePath();
		if (targetBinaryFilePath.HasElements())
		{
			bool failedAny{false};
			const IO::Path sourceDirectory = IO::Path(existingMetaDataPath.GetParentPath());
			const IO::Path targetDirectory = IO::Path(newMetaDataPath.GetParentPath());
			IO::FileIterator::TraverseDirectory(
				sourceDirectory,
				[targetBinaryFilePath,
			   sourceDirectory,
			   sourceMetadataFilePath = asset.GetMetaDataFilePath(),
			   newFileName = newMetaDataPath.GetFileNameWithoutExtensions(),
			   targetDirectory,
			   &failedAny](IO::Path&& targetFilePath) -> IO::FileIterator::TraversalResult
				{
					if (targetFilePath.GetView().StartsWith(targetBinaryFilePath) && targetDirectory != targetBinaryFilePath)
					{
						const IO::PathView relativeBinaryPath = targetFilePath.GetRelativeToParent(targetDirectory);
						const IO::Path sourceBinaryPath = IO::Path::Combine(
							sourceDirectory,
							IO::Path::Merge(relativeBinaryPath.GetParentPath(), newFileName, relativeBinaryPath.GetAllExtensions())
						);
						failedAny |= !sourceBinaryPath.RemoveFile();
					}
					return IO::FileIterator::TraversalResult::Continue;
				}
			);
			if (UNLIKELY_ERROR(failedAny))
			{
				return false;
			}
		}

		return true;
	}

	bool Plugin::MoveAsset(
		const IO::Path& existingPath,
		const IO::Path& targetPath,
		const Asset::Context& context,
		const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		if (targetPath.Exists())
		{
			return false;
		}

		if (!IO::Path(targetPath.GetParentPath()).Exists())
		{
			IO::Path(targetPath.GetParentPath()).CreateDirectories();
		}

		if (existingPath.IsFile())
		{
			Asset::Owners existingAssetOwners(existingPath, Asset::Context(context));
			Asset::Owners newAssetOwners(targetPath, Asset::Context(context));

			return MoveAssetInternal(existingPath, existingAssetOwners, targetPath, newAssetOwners, savingFlags);
		}
		else
		{
			bool failedAny = false;
			IO::FileIterator::TraverseDirectoryRecursive(
				existingPath,
				[this, existingPath, targetPath, &context, savingFlags, &failedAny](IO::Path&& existingFilePath
			  ) mutable -> IO::FileIterator::TraversalResult
				{
					if (existingFilePath.GetRightMostExtension() == Asset::Asset::FileExtension)
					{
						const IO::Path newPath = IO::Path::Combine(targetPath, existingFilePath.GetRelativeToParent(existingPath));
						Asset::Owners existingAssetOwners(existingFilePath, Asset::Context(context));
						Asset::Owners newAssetOwners(newPath, Asset::Context(context));

						failedAny |= !MoveAssetInternal(existingFilePath, existingAssetOwners, newPath, newAssetOwners, savingFlags);
					}

					return IO::FileIterator::TraversalResult::Continue;
				}
			);
			return !failedAny;
		}
	}

	ngine::Guid Plugin::DuplicateAsset(
		const IO::Path& existingMetaDataPath,
		const IO::Path& newMetaDataPath,
		const Asset::Context& newContext,
		const EnumFlags<Serialization::SavingFlags> savingFlags,
		Function<void(Serialization::Data& assetData, Asset::Asset&), 64>&& callback
	)
	{
		// TODO: Support directory assets
		Assert(existingMetaDataPath.IsFile());

		if (newMetaDataPath.Exists())
		{
			return {};
		}

		if (existingMetaDataPath == newMetaDataPath)
		{
			return {};
		}

		Serialization::Data assetData(existingMetaDataPath);
		if (!assetData.IsValid())
		{
			return {};
		}

		Asset::Asset asset(assetData, IO::Path(existingMetaDataPath));
		asset.RegenerateGuid();

		IO::Path(newMetaDataPath.GetParentPath()).CreateDirectories();
		if (!existingMetaDataPath.Exists())
		{
			return {};
		}

		Asset::Owners newAssetOwners(newMetaDataPath, Asset::Context(newContext));
		if (asset.IsSourceFilePathRelativeToAsset())
		{
			IO::Path sourceFilePath = *asset.ComputeAbsoluteSourceFilePath();
			asset.SetMetaDataFilePath(newMetaDataPath);

			sourceFilePath.MakeRelativeTo(newMetaDataPath.GetParentPath());

			// If this assertion hits then we should SetSourceAsset
			Assert(sourceFilePath.GetRightMostExtension() != Asset::Asset::FileExtension);
			asset.SetSourceFilePath(Move(sourceFilePath), newAssetOwners.m_context);
		}
		else
		{
			asset.SetMetaDataFilePath(newMetaDataPath);
		}

		callback(assetData, asset);

		if (!AddNewAssetToDatabase(Asset::DatabaseEntry{asset}, asset.GetGuid(), newAssetOwners, savingFlags))
		{
			return {};
		}

		if (!Serialization::Serialize(assetData, asset))
		{
			return {};
		}

		const bool saved = assetData.SaveToFile(newMetaDataPath, savingFlags);
		if (!saved)
		{
			return {};
		}

		// This asset could have multiple (binary) files, e.g. '.tex.bc' and '.tex.astc'
		const IO::Path sourceBinaryFilePath = asset.GetBinaryFilePath();
		if (sourceBinaryFilePath.HasElements())
		{
			bool failedAny{false};
			const IO::Path sourceDirectory = IO::Path(existingMetaDataPath.GetParentPath());
			const IO::Path targetDirectory = IO::Path(newMetaDataPath.GetParentPath());
			IO::FileIterator::TraverseDirectory(
				sourceDirectory,
				[sourceBinaryFilePath,
			   sourceDirectory,
			   sourceMetadataFilePath = asset.GetMetaDataFilePath(),
			   newFileName = newMetaDataPath.GetFileNameWithoutExtensions(),
			   targetDirectory,
			   &failedAny](IO::Path&& sourceFilePath) -> IO::FileIterator::TraversalResult
				{
					if (sourceFilePath.GetView().StartsWith(sourceBinaryFilePath) && sourceFilePath != sourceMetadataFilePath)
					{
						const IO::PathView relativeBinaryPath = sourceFilePath.GetRelativeToParent(sourceDirectory);
						const IO::Path targetBinaryPath = IO::Path::Combine(
							targetDirectory,
							IO::Path::Merge(relativeBinaryPath.GetParentPath(), newFileName, relativeBinaryPath.GetAllExtensions())
						);
						if (LIKELY(!targetBinaryPath.Exists()))
						{
							failedAny |= !sourceFilePath.CopyFileTo(targetBinaryPath);
						}
					}
					return IO::FileIterator::TraversalResult::Continue;
				}
			);
			if (UNLIKELY_ERROR(failedAny))
			{
				return {};
			}
		}

		RegisterRuntimeAsset(asset, newAssetOwners);

		return asset.GetGuid();
	}

	void Plugin::RegisterRuntimeAsset(const Asset::Asset& asset, const Asset::Owners& assetOwners)
	{
		if (const Optional<Asset::Manager*> pAssetManager = System::Find<Asset::Manager>())
		{
			Asset::Library& assetLibrary = pAssetManager->GetAssetLibrary();
			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

			if (assetOwners.GetDatabasePath() == Asset::LocalDatabase::GetConfigFilePath())
			{
				const Tag::Identifier importedAssetTagIdentifier = tagRegistry.FindOrRegister(Asset::Library::LocalAssetDatabaseTagGuid);

				const IO::Path assetCacheDirectory = Asset::LocalDatabase::GetDirectoryPath();
				const Asset::Identifier rootFolderAssetIdentifier = assetLibrary.FindOrRegisterFolder(assetCacheDirectory, Asset::Identifier{});
				assetLibrary.RegisterAsset(asset.GetGuid(), Asset::Asset{asset}, rootFolderAssetIdentifier, Array{importedAssetTagIdentifier});
			}

			const Asset::Context& context = assetOwners.m_context;
			if (context.HasEngine() || context.HasPlugin() || context.HasProject())
			{
				const Asset::Identifier rootFolderAssetIdentifier =
					pAssetManager->FindOrRegisterFolder(assetOwners.GetDatabaseRootDirectory(), Asset::Identifier{});
				FlatVector<Tag::Identifier, 2> tagIdentifiers;
				if (context.GetProject().IsValid())
				{
					tagIdentifiers.EmplaceBack(tagRegistry.FindOrRegister(context.GetProject()->GetGuid()));
				}
				else if (context.GetPlugin().IsValid())
				{
					const Tag::Identifier pluginAssetTagIdentifier = tagRegistry.FindOrRegister(Asset::Library::PluginTagGuid);
					tagIdentifiers.EmplaceBack(pluginAssetTagIdentifier);
					if (context.GetEngine().IsValid() && context.GetPlugin()->GetDirectory().IsRelativeTo(context.GetEngine()->GetDirectory()))
					{
						const Tag::Identifier engineAssetTagIdentifier = tagRegistry.FindOrRegister(Asset::EngineManager::EngineAssetsTagGuid);
						tagIdentifiers.EmplaceBack(engineAssetTagIdentifier);
					}
				}
				else if (context.GetEngine().IsValid())
				{
					const Tag::Identifier engineAssetTagIdentifier = tagRegistry.FindOrRegister(Asset::EngineManager::EngineAssetsTagGuid);
					tagIdentifiers.EmplaceBack(engineAssetTagIdentifier);
				}
				pAssetManager->RegisterAsset(asset.GetGuid(), Asset::DatabaseEntry{asset}, rootFolderAssetIdentifier, tagIdentifiers.GetView());
			}
		}
	}
	
	Optional<ProjectInfo> Plugin::CreateProject(
		UnicodeString&& name,
		IO::Path&& targetConfigFilePath,
		const ngine::Guid engineGuid,
		const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		if (IO::Path(targetConfigFilePath.GetParentPath()).Exists())
		{
			const IO::PathView fileName = targetConfigFilePath.GetFileName();
			targetConfigFilePath = IO::Path::Combine(IO::Path(targetConfigFilePath.GetParentPath()).GetDuplicated(), fileName);
		}

		ProjectInfo projectInfo(Forward<UnicodeString>(name), Forward<IO::Path>(targetConfigFilePath), engineGuid);
		IO::Path(projectInfo.GetDirectory()).CreateDirectories();

		if (!Serialization::SerializeToDisk(projectInfo.GetConfigFilePath(), projectInfo, savingFlags))
		{
			return Invalid;
		}

		const IO::Path assetsDirectory = IO::Path::Combine(projectInfo.GetDirectory(), projectInfo.GetRelativeAssetDirectory());
		assetsDirectory.CreateDirectory();

		const IO::Path assetDatabasePath = IO::Path::Merge(assetsDirectory, Asset::Database::AssetFormat.metadataFileExtension);
		Asset::Database assetDatabase(assetDatabasePath, projectInfo.GetDirectory());
		if (!assetDatabase.Generate(projectInfo))
		{
			return Invalid;
		}

		projectInfo.SetAssetDatabaseGuid(assetDatabase.GetGuid());
		if (!Serialization::SerializeToDisk(projectInfo.GetConfigFilePath(), projectInfo, savingFlags))
		{
			return Invalid;
		}

		assetDatabase.RegisterAsset(
			assetDatabase.GetGuid(),
			Asset::DatabaseEntry{Asset::Database::AssetFormat.assetTypeGuid, ngine::Guid{}, IO::Path(assetDatabasePath)},
			projectInfo.GetDirectory()
		);

		if (!assetDatabase.Save(assetDatabasePath, savingFlags))
		{
			return Invalid;
		}

		EditableProjectDatabase projectDatabase;
		projectDatabase.RegisterProject(IO::Path(projectInfo.GetConfigFilePath()), projectInfo.GetGuid());
		if (!projectDatabase.Save(savingFlags))
		{
			return Invalid;
		}

		if (Optional<Asset::Manager*> pAssetManager = System::Find<Asset::Manager>())
		{
			const Asset::Identifier projectRootFolderAssetIdentifier =
				pAssetManager->FindOrRegisterFolder(projectInfo.GetDirectory(), Asset::Identifier{});
			const Asset::Identifier projectAssetIdentifier = pAssetManager->RegisterAsset(
				projectInfo.GetGuid(),
				Asset::DatabaseEntry{*assetDatabase.GetAssetEntry(projectInfo.GetGuid())},
				projectRootFolderAssetIdentifier
			);

			assetDatabase.IterateAssets(
				[&assetManager = *pAssetManager, projectRootFolderAssetIdentifier](const Asset::Guid assetGuid, const Asset::DatabaseEntry& entry)
				{
					assetManager.RegisterAsset(assetGuid, Asset::DatabaseEntry{entry}, projectRootFolderAssetIdentifier);
					return Memory::CallbackResult::Continue;
				}
			);

			constexpr ngine::Guid EditableLocalProjectTagGuid = "99aca0e9-ef55-405d-8988-94165b912a08"_tag;
			constexpr ngine::Guid LocalPlayerTagGuid = "44be35b5-5c1c-49b9-a203-784b5b5ea4c6"_guid;

			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
			pAssetManager->SetTagAsset(tagRegistry.FindOrRegister(EditableLocalProjectTagGuid), projectAssetIdentifier);
			pAssetManager->SetTagAsset(tagRegistry.FindOrRegister(LocalPlayerTagGuid), projectAssetIdentifier);
		}

		return Move(projectInfo);
	}

	enum class CopyResult : uint8
	{
		Failed,
		Copied,
		Skipped
	};

	CopyResult
	CopyPackagingAsset(const IO::PathView sourceMetadataFilePath, const IO::Path& targetMetadataFilePath, const Asset::DatabaseEntry& assetEntry)
	{
		Assert(assetEntry.m_path.IsAbsolute());
		CopyResult result{CopyResult::Skipped};
		if (assetEntry.m_path.Exists() && !targetMetadataFilePath.Exists())
		{
			if (assetEntry.m_path.CopyFileTo(targetMetadataFilePath))
			{
				result = CopyResult::Copied;
			}
			else
			{
				result = CopyResult::Failed;
			}
		}
		// This asset could have multiple (binary) files, e.g. '.tex.bc' and '.tex.astc'
		const IO::PathView sourceBinaryFilePath = assetEntry.GetBinaryFilePath();
		if (sourceBinaryFilePath.HasElements() && result != CopyResult::Skipped)
		{
			const IO::Path sourceDirectory = IO::Path(sourceMetadataFilePath.GetParentPath());
			const IO::Path targetDirectory = IO::Path(targetMetadataFilePath.GetParentPath());
			IO::FileIterator::TraverseDirectory(
				IO::Path(sourceBinaryFilePath.GetParentPath()),
				[sourceBinaryFilePath, sourceDirectory, targetDirectory, &result](IO::Path&& sourceFilePath) -> IO::FileIterator::TraversalResult
				{
					if (sourceFilePath.GetView().StartsWith(sourceBinaryFilePath))
					{
						const IO::PathView relativeBinaryPath = sourceFilePath.GetRelativeToParent(sourceDirectory);
						const IO::Path targetBinaryPath = IO::Path::Combine(targetDirectory, relativeBinaryPath);
						if (LIKELY(!targetBinaryPath.Exists()))
						{
							Assert(result != CopyResult::Skipped);
							if (!sourceFilePath.CopyFileTo(targetBinaryPath))
							{
								result = CopyResult::Failed;
							}
						}
					}
					return IO::FileIterator::TraversalResult::Continue;
				}
			);
		}

		return result;
	}

	CopyResult CopyPackagingAsset(
		const ProjectInfo& sourceProject, const ProjectInfo& targetProject, const Asset::DatabaseEntry& assetEntry, IO::Path& newAssetPathOut
	)
	{
		Assert(assetEntry.m_path.IsAbsolute());
		const IO::PathView sourceProjectDirectory = sourceProject.GetDirectory();
		IO::PathView relativePath;
		if (assetEntry.m_path.IsRelativeTo(sourceProjectDirectory))
		{
			relativePath = assetEntry.m_path.GetRelativeToParent(sourceProjectDirectory);
		}
		else
		{
			relativePath = assetEntry.m_path.GetFileName();
		}
		const IO::Path targetMetadataFilePath = IO::Path::Combine(targetProject.GetDirectory(), relativePath);
		IO::Path(targetMetadataFilePath.GetParentPath()).CreateDirectories();

		newAssetPathOut = targetMetadataFilePath;
		return CopyPackagingAsset(assetEntry.m_path, targetMetadataFilePath, assetEntry);
	}

	Optional<ProjectInfo> Plugin::CopyProject(
		const ProjectInfo& sourceProject,
		Asset::Database&& sourceAssetDatabase,
		UnicodeString&& name,
		IO::Path&& targetConfigFilePath,
		const ngine::Guid engineGuid,
		const EnumFlags<Serialization::SavingFlags> savingFlags,
		const EnumFlags<CopyProjectFlags> flags
	)
	{
		if (targetConfigFilePath.Exists())
		{
			const IO::PathView fileName = targetConfigFilePath.GetFileName();
			targetConfigFilePath = IO::Path::Combine(IO::Path(targetConfigFilePath.GetParentPath()).GetDuplicated(), fileName);
		}

		ProjectInfo projectInfo(sourceProject);
		projectInfo.SetName(Forward<UnicodeString>(name));
		projectInfo.SetMetadataFilePath(Forward<IO::Path>(targetConfigFilePath));
		projectInfo.SetEngine(engineGuid);

		IO::Path(projectInfo.GetDirectory()).CreateDirectories();

		const IO::Path assetsDirectory = IO::Path::Combine(projectInfo.GetDirectory(), projectInfo.GetRelativeAssetDirectory());
		assetsDirectory.CreateDirectory();

		Optional<Asset::Manager*> pAssetManager = System::Find<Asset::Manager>();

		Asset::Database assetDatabase;
		if (flags.IsSet(CopyProjectFlags::Clone))
		{
			assetDatabase.ReserveImported(sourceAssetDatabase.GetAssetCount() + sourceAssetDatabase.GetImportedAssetCount());

			// Import all assets from the other project
			sourceAssetDatabase.IterateAssets(
				[&assetDatabase](const Asset::Guid assetGuid, Asset::DatabaseEntry&) -> Memory::CallbackResult
				{
					assetDatabase.ImportAsset(assetGuid);
					return Memory::CallbackResult::Continue;
				}
			);

			// Carry over all imports
			sourceAssetDatabase.IterateImportedAssets(
				[&assetDatabase](const Asset::Guid assetGuid) -> Memory::CallbackResult
				{
					assetDatabase.ImportAsset(assetGuid);
					return Memory::CallbackResult::Continue;
				}
			);

			// We always clone the project info and asset database
			assetDatabase.RemoveImportedAsset(projectInfo.GetGuid());
			assetDatabase.RemoveImportedAsset(projectInfo.GetAssetDatabaseGuid());

			projectInfo.SetGuid(Guid::Generate());
			projectInfo.SetAssetDatabaseGuid(assetDatabase.GetGuid());
		}
		else
		{
			assetDatabase = sourceAssetDatabase;
			assetDatabase.RemoveAsset(sourceProject.GetGuid());
			assetDatabase.RemoveAsset(sourceProject.GetAssetDatabaseGuid());
		}

		Asset::DatabaseEntry& projectEntry = assetDatabase.RegisterAsset(projectInfo.GetGuid(), Asset::DatabaseEntry{projectInfo}, {});

		Asset::Identifier projectRootFolderAssetIdentifier;

		if (pAssetManager != nullptr && flags.IsSet(CopyProjectFlags::AddToProjectsDatabase))
		{
			projectRootFolderAssetIdentifier = pAssetManager->FindOrRegisterFolder(projectInfo.GetDirectory(), Asset::Identifier{});
			const Asset::Identifier projectAssetIdentifier =
				pAssetManager->RegisterAsset(projectInfo.GetGuid(), Asset::DatabaseEntry{projectEntry}, projectRootFolderAssetIdentifier);

			constexpr ngine::Guid EditableLocalProjectTagGuid = "99aca0e9-ef55-405d-8988-94165b912a08"_tag;
			constexpr ngine::Guid LocalPlayerTagGuid = "44be35b5-5c1c-49b9-a203-784b5b5ea4c6"_guid;

			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
			pAssetManager->SetTagAsset(tagRegistry.FindOrRegister(EditableLocalProjectTagGuid), projectAssetIdentifier);
			pAssetManager->SetTagAsset(tagRegistry.FindOrRegister(LocalPlayerTagGuid), projectAssetIdentifier);
		}

		if (sourceProject.GetAssetDatabaseGuid().IsValid())
		{
			Asset::DatabaseEntry& newEntry = assetDatabase.RegisterAsset(
				projectInfo.GetAssetDatabaseGuid(),
				Asset::DatabaseEntry{
					Asset::Database::AssetFormat.assetTypeGuid,
					{},
					IO::Path::Combine(
						projectInfo.GetDirectory(),
						IO::Path::Merge(projectInfo.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
					)
				},
				{}
			);

			if (pAssetManager != nullptr && flags.IsSet(CopyProjectFlags::AddToProjectsDatabase))
			{
				Assert(projectRootFolderAssetIdentifier.IsValid());
				pAssetManager->RegisterAsset(projectInfo.GetAssetDatabaseGuid(), Asset::DatabaseEntry{newEntry}, projectRootFolderAssetIdentifier);
			}
		}

		if (flags.IsSet(CopyProjectFlags::Clone))
		{
			const Asset::Guid previousThumbnailGuid = sourceProject.GetThumbnailGuid();
			if (projectInfo.GetThumbnailGuid().IsValid() && !sourceAssetDatabase.IsAssetImported(previousThumbnailGuid))
			{
				projectInfo.SetThumbnail(Asset::Guid::Generate());

				const Optional<const Asset::DatabaseEntry*> pPreviousThumbnailEntry = sourceAssetDatabase.GetAssetEntry(previousThumbnailGuid);
				if (pPreviousThumbnailEntry.IsValid())
				{
					IO::Path newAssetPath;
					const bool couldCopyAsset = CopyPackagingAsset(sourceProject, projectInfo, *pPreviousThumbnailEntry, newAssetPath) == CopyResult::Copied;

					Asset::DatabaseEntry thumbnailEntry = *pPreviousThumbnailEntry;
					thumbnailEntry.m_path = newAssetPath;

					assetDatabase.RemoveAsset(previousThumbnailGuid);

					const bool copiedFile = couldCopyAsset && newAssetPath.Exists();
					if (copiedFile)
					{
						Serialization::Data thumbnailAssetData(newAssetPath);
						Assert(thumbnailAssetData.IsValid() && thumbnailAssetData.GetDocument().IsObject());
						if (LIKELY(thumbnailAssetData.IsValid() && thumbnailAssetData.GetDocument().IsObject()))
						{
							Serialization::Writer writer(thumbnailAssetData);
							writer.Serialize("guid", projectInfo.GetThumbnailGuid());
							[[maybe_unused]] const bool wasSaved = thumbnailAssetData.SaveToFile(newAssetPath, savingFlags);
							if (!wasSaved)
							{
								return Invalid;
							}
						}
					}

					Asset::DatabaseEntry& emplacedEntry = assetDatabase.RegisterAsset(projectInfo.GetThumbnailGuid(), Move(thumbnailEntry), {});

					if (pAssetManager != nullptr && flags.IsSet(CopyProjectFlags::AddToProjectsDatabase))
					{
						Assert(projectRootFolderAssetIdentifier.IsValid());
						pAssetManager
							->RegisterAsset(projectInfo.GetThumbnailGuid(), Asset::DatabaseEntry{emplacedEntry}, projectRootFolderAssetIdentifier);
						if (!copiedFile)
						{
							pAssetManager->RegisterCopyFromSourceAsyncLoadCallback(previousThumbnailGuid, projectInfo.GetThumbnailGuid());
						}
					}
				}
			}

			const Asset::Guid previousSceneGuid = sourceProject.GetDefaultSceneGuid();
			if (projectInfo.GetDefaultSceneGuid().IsValid() && !sourceAssetDatabase.IsAssetImported(previousSceneGuid))
			{
				projectInfo.SetDefaultSceneGuid(Asset::Guid::Generate());

				const Optional<const Asset::DatabaseEntry*> pPreviousSceneEntry = sourceAssetDatabase.GetAssetEntry(previousSceneGuid);
				Assert(pPreviousSceneEntry.IsValid());
				if (LIKELY(pPreviousSceneEntry.IsValid()))
				{
					IO::Path newAssetPath;
					const bool couldCopyAsset = CopyPackagingAsset(sourceProject, projectInfo, *pPreviousSceneEntry, newAssetPath) == CopyResult::Copied;

					Asset::DatabaseEntry sceneEntry = *pPreviousSceneEntry;
					sceneEntry.m_path = newAssetPath;

					assetDatabase.RemoveAsset(previousSceneGuid);

					const bool copiedFile = couldCopyAsset && newAssetPath.Exists();
					if (copiedFile)
					{
						Serialization::Data sceneAssetData(newAssetPath);
						if (sceneAssetData.IsValid())
						{
							Serialization::Writer writer(sceneAssetData);
							writer.Serialize("guid", projectInfo.GetDefaultSceneGuid());
							[[maybe_unused]] const bool wasSaved = sceneAssetData.SaveToFile(newAssetPath, savingFlags);
							if (!wasSaved)
							{
								return Invalid;
							}
						}
					}

					Asset::DatabaseEntry& emplacedEntry = assetDatabase.RegisterAsset(projectInfo.GetDefaultSceneGuid(), Move(sceneEntry), {});

					if (pAssetManager != nullptr && flags.IsSet(CopyProjectFlags::AddToProjectsDatabase))
					{
						Assert(projectRootFolderAssetIdentifier.IsValid());
						pAssetManager
							->RegisterAsset(projectInfo.GetDefaultSceneGuid(), Asset::DatabaseEntry{emplacedEntry}, projectRootFolderAssetIdentifier);
						if (!copiedFile)
						{
							pAssetManager->RegisterCopyFromSourceAsyncLoadCallback(previousSceneGuid, projectInfo.GetDefaultSceneGuid());
						}
					}
				}
			}
		}

		if (!Serialization::SerializeToDisk(projectInfo.GetConfigFilePath(), projectInfo, savingFlags))
		{
			return Invalid;
		}

		const IO::Path assetDatabasePath = IO::Path::Merge(assetsDirectory, Asset::Database::AssetFormat.metadataFileExtension);
		if (!assetDatabase.Save(assetDatabasePath, savingFlags))
		{
			return Invalid;
		}

		if (flags.IsSet(CopyProjectFlags::AddToProjectsDatabase))
		{
			ProjectDatabase projectDatabase = flags.IsSet(CopyProjectFlags::IsEditable) ? (ProjectDatabase)EditableProjectDatabase()
			                                                                            : (ProjectDatabase)PlayableProjectDatabase();
			projectDatabase.RegisterProject(IO::Path(projectInfo.GetConfigFilePath()), projectInfo.GetGuid());
			if (!projectDatabase.Save(Serialization::SavingFlags{}))
			{
				return Invalid;
			}
		}

		return Move(projectInfo);
	}

	Optional<PluginInfo> Plugin::CreateAssetPlugin(
		UnicodeString&& name,
		IO::Path&& targetConfigFilePath,
		const ngine::Guid engineGuid,
		const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		PluginInfo plugin(Forward<UnicodeString>(name), Forward<IO::Path>(targetConfigFilePath), engineGuid);
		IO::Path(plugin.GetDirectory()).CreateDirectories();

		const IO::Path assetsDirectory = IO::Path::Combine(plugin.GetDirectory(), plugin.GetRelativeAssetDirectory());
		assetsDirectory.CreateDirectory();

		const IO::Path assetDatabasePath = IO::Path::Merge(assetsDirectory, Asset::Database::AssetFormat.metadataFileExtension);
		Asset::Database assetDatabase(assetDatabasePath, plugin.GetDirectory());
		if (!assetDatabase.Generate(plugin))
		{
			return Invalid;
		}

		plugin.SetAssetDatabaseGuid(assetDatabase.GetGuid());
		if (!plugin.Save(savingFlags))
		{
			return Invalid;
		}

		if (!assetDatabase.Save(assetDatabasePath, savingFlags))
		{
			return Invalid;
		}

		return plugin;
	}

	Optional<PluginInfo> Plugin::CreateAssetPlugin(
		UnicodeString&& name, IO::Path&& targetConfigFilePath, ProjectInfo& projectInfo, const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		Optional<PluginInfo> plugin =
			CreateAssetPlugin(Forward<UnicodeString>(name), Forward<IO::Path>(targetConfigFilePath), projectInfo.GetEngineGuid(), savingFlags);
		if (plugin.IsInvalid())
		{
			return Invalid;
		}

		projectInfo.AddPlugin(plugin->GetGuid());

		if (!Serialization::SerializeToDisk(projectInfo.GetConfigFilePath(), projectInfo, savingFlags))
		{
			return Invalid;
		}

		return plugin;
	}

#if SUPPORT_GENERATE_CODE_PLUGIN
	Optional<PluginInfo> Plugin::CreateCodePlugin(
		UnicodeString&& name, IO::Path&& targetConfigFilePath, ProjectInfo& projectInfo, const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		Optional<PluginInfo> plugin =
			CreateAssetPlugin(Forward<UnicodeString>(name), Forward<IO::Path>(targetConfigFilePath), projectInfo, savingFlags);
		if (plugin.IsInvalid())
		{
			return Invalid;
		}

		plugin->SetRelativeSourceDirectory(IO::Path(MAKE_PATH("Code")));
		if (!plugin->Save(savingFlags))
		{
			return Invalid;
		}

		return plugin;
	}

	Optional<PluginInfo> Plugin::CreateCodePlugin(
		UnicodeString&& name,
		IO::Path&& targetConfigFilePath,
		const EngineInfo& engineInfo,
		const EnumFlags<Serialization::SavingFlags> savingFlags
	)
	{
		Optional<PluginInfo> plugin =
			CreateAssetPlugin(Forward<UnicodeString>(name), Forward<IO::Path>(targetConfigFilePath), engineInfo.GetGuid(), savingFlags);
		if (plugin.IsInvalid())
		{
			return Invalid;
		}

		plugin->SetRelativeSourceDirectory(IO::Path(MAKE_PATH("Code")));
		if (!plugin->Save(savingFlags))
		{
			return Invalid;
		}

		const IO::Path sourceDirectory = plugin->GetSourceDirectory();
		if (!sourceDirectory.Exists())
		{
			sourceDirectory.CreateDirectory();
		}

		if (!GeneratePluginCMakeLists(*plugin))
		{
			return Invalid;
		}

		return plugin;
	}
#endif
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::AssetCompiler::Plugin>();
#else
extern "C" EDITORCORE_EXPORT_API ngine::Plugin* InitializePlugin(Optional<ReferenceWrapper<Engine>> engine)
{
	return new ngine::AssetCompiler::Plugin(application);
}
#endif
