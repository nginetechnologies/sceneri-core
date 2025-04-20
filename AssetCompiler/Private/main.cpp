#include <Common/Platform/GetName.h>
#include <Common/Platform/Console.h>
#include <Common/CommandLine/CommandLineArguments.h>
#include <Common/CommandLine/CommandLineInitializationParameters.h>
#include <Common/CommandLine/CommandLineArgumentsView.h>
#include <Common/CommandLine/CommandLineArguments.inl>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/AddressOf.h>
#include <Common/Asset/Asset.h>
#include <Common/Asset/AssetDatabase.h>
#include <Common/Asset/AssetOwners.h>
#include <Common/IO/FileIterator.h>
#include <Common/Project System/PluginInfo.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Serialization/SerializedData.h>
#include <Common/IO/File.h>
#include <Common/Project System/EngineInfo.h>
#include <Common/Project System/ProjectInfo.h>
#include <Common/Project System/EngineDatabase.h>
#include <Common/Project System/PluginDatabase.h>
#include <Common/Project System/FindEngine.h>
#include <Common/Serialization/Serialize.h>
#include <Common/Reflection/Registry.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Format/Guid.h>
#include <Common/Threading/Jobs/JobManager.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Platform/Windows.h>
#include <Common/Platform/GetProcessorCoreTypes.h>
#include <Common/Application/Application.h>
#include <Common/System/Query.h>
#include <Common/IO/Format/Path.h>
#include <Common/IO/Log.h>

#include <Engine/EngineSystems.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/DataSource/DataSourceCache.h>
#include <Engine/Tag/TagRegistry.h>

#include <AssetCompilerCore/Plugin.h>

#include <iostream>

#if PLATFORM_APPLE_MACOS
#import <Cocoa/Cocoa.h>
#endif

ngine::UniquePtr<ngine::EngineSystems> CreateEngine(const ngine::CommandLine::InitializationParameters&)
{
	return {};
}

namespace ngine
{
	[[nodiscard]] bool Run(CommandLine::Arguments&& arguments);
}

#if PLATFORM_APPLE_MACOS
@interface AssetCompilerApplication : NSApplication
@end

@implementation AssetCompilerApplication
@end

@interface AssetCompilerAppDelegate : NSResponder <NSApplicationDelegate>
{
	NSArray<NSURL*>* requestedURLs;
}
@end

@implementation AssetCompilerAppDelegate
- (void)application:(NSApplication* _Nonnull)application openURLs:(NSArray<NSURL*>* _Nonnull)urls API_AVAILABLE(macos(10.13))
{
	requestedURLs = urls;
}

- (void)__attribute__((noreturn))applicationDidFinishLaunching:(NSNotification* _Nonnull)notification
{
	using namespace ngine;

	CommandLine::Arguments commandLineArgs(CommandLine::InitializationParameters::GetGlobalParameters());
	if (requestedURLs.count > 0)
	{
		for (NSURL* url : requestedURLs)
		{
			if ([url isFileURL])
			{
				[url startAccessingSecurityScopedResource];
				const char* filePath = url.fileSystemRepresentation;

				commandLineArgs.EmplaceArgument(NativeString(filePath, (uint32)strlen(filePath)), String(), CommandLine::Prefix::None);
			}
		}
	}
	const bool success = Run(Move(commandLineArgs));
	exit(!success);
}
@end
#endif

#if PLATFORM_WINDOWS
int __cdecl wmain(int argumentCount, wchar_t* pArguments[])
#elif PLATFORM_APPLE_MACOS
int main(int argumentCount, const char* _Nonnull pArguments[_Nonnull])
#else
int main(int argumentCount, char* pArguments[])
#endif
{
	using namespace ngine;

#if PLATFORM_WINDOWS
	// Argument 0 is always executable path on Windows
	Vector<ConstNativeStringView, uint16> arguments(Memory::Reserve, static_cast<uint16>(argumentCount));
	for (uint16 argumentIndex = 1; argumentIndex < argumentCount; ++argumentIndex)
	{
		arguments.EmplaceBack(pArguments[argumentIndex], (uint16)wcslen(pArguments[argumentIndex]));
	}
	const bool success = Run(CommandLine::Arguments(arguments.GetView()));
	return !success;
#elif PLATFORM_APPLE_MACOS
	Vector<ConstNativeStringView, uint16> arguments(Memory::Reserve, static_cast<uint16>(argumentCount));
	uint16 argumentIndex = 0;
	if (argumentCount > 0)
	{
		const IO::Path executablePath = IO::Path::GetExecutablePath();
		const ConstNativeStringView firstArgument{pArguments[argumentIndex], (uint16)strlen(pArguments[argumentIndex])};
		if (executablePath.GetView().GetStringView().EndsWith(firstArgument))
		{
			argumentIndex++;
		}
	}

	for (; argumentIndex < argumentCount; ++argumentIndex)
	{
		arguments.EmplaceBack(pArguments[argumentIndex], (uint16)strlen(pArguments[argumentIndex]));
	}

	ngine::CommandLine::InitializationParameters::GetGlobalParameters() = CommandLine::InitializationParameters(arguments.GetView());

	@autoreleasepool
	{
		AssetCompilerApplication* app = [AssetCompilerApplication sharedApplication];
		AssetCompilerAppDelegate* delegate = [[AssetCompilerAppDelegate alloc] init];
		app.delegate = delegate;
		return NSApplicationMain(argumentCount, pArguments);
	}
#else
	FixedCapacityVector<ConstNativeStringView, uint16> arguments(Memory::Reserve, static_cast<uint16>(argumentCount));
	for (int i = 1; i < argumentCount; ++i)
	{
		arguments.EmplaceBack(pArguments[i], (uint16)strlen(pArguments[i]));
	}

	const bool success = Run(CommandLine::Arguments(arguments.GetView()));
	return !success;
#endif
}

namespace ngine
{
	struct AssetCompilerJobManager : public Threading::JobManager
	{
		AssetCompilerJobManager()
		{
			const uint16 logicalPerformanceCoreCount = Math::Max(Platform::GetLogicalPerformanceCoreCount(), (uint16)1u);
			const uint16 logicalEfficiencyCoreCount = Math::Max(Platform::GetLogicalEfficiencyCoreCount(), (uint16)1u);

			const uint16 totalWorkerCount = (uint16)(logicalPerformanceCoreCount + logicalEfficiencyCoreCount);
			const Math::Range<uint16> workerRange = Math::Range<uint16>::Make(0, uint16(totalWorkerCount));

			// Allow all jobs run on all threads for command line tools such as the project system
			StartRunners(totalWorkerCount, workerRange, workerRange, workerRange);
		}
	};

	struct AssetCompilerApp
	{
		AssetCompilerApp()
		{
			System::Query::GetInstance().RegisterSystem(m_jobManager);
			System::Query::GetInstance().RegisterSystem(m_reflectionRegistry);
			System::Get<Log>().Open(MAKE_PATH("AssetCompiler"));
		}
		~AssetCompilerApp()
		{
			m_application.UnloadPlugins();
		}

		Application m_application;
		AssetCompilerJobManager m_jobManager;
		DataSource::Cache m_dataSourceCache;
		Tag::Registry m_tagRegistry;
		Asset::Manager m_assetManager;
		Reflection::Registry m_reflectionRegistry{Reflection::Registry::Initializer::Initialize};
	};

	bool Run(CommandLine::Arguments&& commandLineArgs)
	{
		UniquePtr<AssetCompilerApp> pApplication = UniquePtr<AssetCompilerApp>::Make();
		AssetCompilerApp& application = *pApplication;
		Threading::JobBatch loadPluginDataJobBatch = application.m_assetManager.GetAssetLibrary().LoadPluginDatabase();
		Threading::Atomic<bool> loadedPluginDatabase{false};
		loadPluginDataJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
			[&loadedPluginDatabase](Threading::JobRunnerThread&)
			{
				loadedPluginDatabase = true;
			},
			Threading::JobPriority::LoadProject
		));
		Threading::JobRunnerThread::GetCurrent()->Queue(loadPluginDataJobBatch);
		while (!loadedPluginDatabase)
		{
			Threading::JobRunnerThread::GetCurrent()->DoRunNextJob();
		}

		const IO::Path engineDirectory = ProjectSystem::FindEngineDirectoryFromExecutableFolder(IO::Path::GetExecutableDirectory());
		EnginePluginDatabase pluginDatabase(IO::Path::Combine(engineDirectory, EnginePluginDatabase::FileName));

		Threading::Atomic<bool> failedAnyTasks = false;

		{
			if (!pluginDatabase.HasPlugin(AssetCompiler::Plugin::Guid))
			{
				LogMessage("Plug-in database was missing asset compiler, registering all plug-ins");
				pluginDatabase.RegisterAllDefaultPlugins(IO::Path::Combine(engineDirectory, MAKE_PATH("Code")));
				if (!pluginDatabase.Save(Serialization::SavingFlags::HumanReadable))
				{
					LogError("Failed to save plug-in database");
					return false;
				}

				pluginDatabase.IteratePlugins(
					[&assetLibrary = pApplication->m_assetManager.GetAssetLibrary(),
				   engineDirectory](const Asset::Guid assetGuid, const IO::Path& assetPath)
					{
						const Asset::Identifier rootFolderAssetIdentifier =
							assetLibrary.FindOrRegisterFolder(assetPath.GetParentPath(), Asset::Identifier{});
						assetLibrary.RegisterAsset(
							assetGuid,
							Asset::DatabaseEntry{PluginAssetFormat.assetTypeGuid, {}, IO::Path(assetPath)},
							rootFolderAssetIdentifier
						);
					}
				);

				// Generate all plug-in asset databases
				pluginDatabase.IteratePlugins(
					[engineDirectory, &failedAnyTasks](const Asset::Guid, const IO::Path& pluginPath)
					{
						IO::Path absolutePluginPath = pluginPath.IsRelative() ? IO::Path::Combine(engineDirectory, pluginPath) : pluginPath;
						PluginInfo plugin = PluginInfo(Move(absolutePluginPath));
						if (plugin.IsValid())
						{
							Asset::Database database;
							const bool generatedPlugin = database.LoadAndGenerate(plugin);
							if (UNLIKELY_ERROR(!generatedPlugin))
							{
								LogError("Failed to load and generate plug-in asset database {}", plugin.GetConfigFilePath());
							}
							failedAnyTasks |= !generatedPlugin;

							IO::Path assetsDatabasePath = IO::Path::Merge(plugin.GetAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension);
							LogMessage("Generating plug-in database {}", assetsDatabasePath);
							const bool savedDatabase = database.Save(assetsDatabasePath, Serialization::SavingFlags::HumanReadable);
							if (UNLIKELY_ERROR(!savedDatabase))
							{
								LogError("Failed to load and generate plug-in asset database {}", plugin.GetConfigFilePath());
							}
							failedAnyTasks |= !savedDatabase;
						}
						else
						{
							LogWarning("Failed to parse plug-in {}", plugin.GetConfigFilePath());
						}
					}
				);

				if (!pluginDatabase.HasPlugin(AssetCompiler::Plugin::Guid))
				{
					LogError("Failed to find asset compiler plug-in in asset database");
					return false;
				}
			}
		}

		auto getAssetCompiler = [&application = application.m_application, &assetManager = application.m_assetManager]()
		{
			static Optional<AssetCompiler::Plugin*> pAssetCompiler = [&application, &assetManager]() -> Optional<AssetCompiler::Plugin*>
			{
				if (UNLIKELY(
							!application.LoadPlugin(AssetCompiler::Plugin::Guid, assetManager.GetAssetLibrary().GetDatabase(), assetManager.GetDatabase())
						))
				{
					LogError("Failed to find asset compiler plug-in");
					return Invalid;
				}
				return application.GetPluginInstance<AssetCompiler::Plugin>();
			}();
			return pAssetCompiler;
		};

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

		{
			Asset::Context context = ProjectSystem::GetInitialAssetContext(commandLineArgs);

			AssetCompiler::Plugin& assetCompiler = *getAssetCompiler();

			assetCompiler.RunCommandLineOptions(commandLineArgs, context, application.m_jobManager, {}, failedAnyTasks);

			for (const CommandLine::Argument& command : commandLineArgs.GetView())
			{
				switch (command.prefix)
				{
					case CommandLine::Prefix::None:
					{
						IO::Path assetMetaFilePath(command.key.GetView());
						const Asset::Owners assetOwners(assetMetaFilePath, Asset::Context{context});

						if(Threading::Job* pJob = assetCompiler.CompileAnyAsset(
							   AssetCompiler::CompileFlags::WasDirectlyRequested | AssetCompiler::CompileFlags::SaveHumanReadable,
								   [&failedAnyTasks](
									const EnumFlags<AssetCompiler::CompileFlags> flags,
									ArrayView<Asset::Asset> assets,
									ArrayView<const Serialization::Data> assetsData
								)
								{
									if (flags.IsSet(AssetCompiler::CompileFlags::Compiled))
									{
										for (Asset::Asset& asset : assets)
										{
											const uint32 assetIndex = assets.GetIteratorIndex(Memory::GetAddressOf(asset));
											const Serialization::Data& assetData = assetsData[assetIndex];

											failedAnyTasks |= !assetData.SaveToFile(
												asset.GetMetaDataFilePath(),
												Serialization::SavingFlags::HumanReadable * flags.IsSet(AssetCompiler::CompileFlags::SaveHumanReadable)
											);
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
										failedAnyTasks = true;
									}
								},
							   *Threading::JobRunnerThread::GetCurrent(),
							   platforms,
							   Move(assetMetaFilePath),
							   assetOwners.m_context,
							   assetOwners.m_context,
							   targetDirectory
						   ))
						{
							pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
						}
					}
					break;
					default:
						break;
				}
			}
		}

		const uint8 otherThreadCount = (uint8)(application.m_jobManager.GetJobThreads().GetSize() - (uint8)1u);
		while (application.m_jobManager.GetNumberOfIdleThreads() < otherThreadCount || Threading::JobRunnerThread::GetCurrent()->HasWork() ||
		       application.m_jobManager.IsRunningExternalTasks())
		{
			Threading::JobRunnerThread::GetCurrent()->DoRunNextJob();
		}

		LogWarningIf(failedAnyTasks, "Failed a task while running AssetCompiler");
		return !failedAnyTasks;
	}
}
