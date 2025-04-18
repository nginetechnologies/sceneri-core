#include "EngineSystems.h"

#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Asset/AssetType.inl>

#include <Common/Threading/Jobs/Job.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Asset/Format/Guid.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/Project System/PluginInfo.h>
#include <Common/Project System/PluginAssetFormat.h>
#include <Common/Project System/EngineAssetFormat.h>
#include <Common/Plugin/Plugin.h>
#include <Common/Time/Stopwatch.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/IO/Format/Path.h>
#include <Common/IO/Log.h>
#include <Common/Application/PluginInstance.h>
#include <Common/System/Query.h>
#include <Common/Format/Guid.h>
#include <Common/Asset/Reference.h>

#include <Renderer/Renderer.h>
#include <Renderer/Devices/LogicalDevice.h>

#if HAS_LIVEPP
#include "LPP_API_x64_CPP.h"
#endif

namespace ngine
{
	EngineSystems::EngineQueryHelper::EngineQueryHelper(EngineSystems& engineSystems)
	{
		System::Query& systemQuery = System::Query::GetInstance();
		systemQuery.RegisterSystem(engineSystems.m_engine);
		systemQuery.RegisterSystem(static_cast<Threading::JobManager&>(engineSystems.m_jobManager));
		systemQuery.RegisterSystem(engineSystems.m_filesystem);
		systemQuery.RegisterSystem(engineSystems.m_eventManager);
		systemQuery.RegisterSystem(static_cast<Reflection::Registry&>(engineSystems.m_reflectionRegistry));
		systemQuery.RegisterSystem(engineSystems.m_scriptCache);
		systemQuery.RegisterSystem(engineSystems.m_entityManager);
		systemQuery.RegisterSystem(engineSystems.m_inputManager);
		systemQuery.RegisterSystem(engineSystems.m_renderer);
	}

	EngineSystems::EngineQueryHelper::~EngineQueryHelper()
	{
		System::Query& systemQuery = System::Query::GetInstance();
		systemQuery.DeregisterSystem<Rendering::Renderer>();
		systemQuery.DeregisterSystem<Input::Manager>();
		systemQuery.DeregisterSystem<Entity::Manager>();
		systemQuery.DeregisterSystem<Scripting::ScriptCache>();
		systemQuery.DeregisterSystem<Reflection::Registry>();
		systemQuery.DeregisterSystem<Events::Manager>();
		systemQuery.DeregisterSystem<IO::Filesystem>();
		systemQuery.DeregisterSystem<Threading::JobManager>();
		systemQuery.DeregisterSystem<Engine>();
	}

	EngineSystems::EngineSystems(const CommandLine::InitializationParameters& commandLineArguments)
		: m_engineQueryHelper(*this)
		, m_engine(commandLineArguments)
		, m_scriptCache(m_assetManager)
	{
	}

	PURE_LOCALS_AND_POINTERS EngineSystems& Engine::GetSystems()
	{
		const ptrdiff_t offsetFromOwner = Memory::GetOffsetOf(&EngineSystems::m_engine);
		return *reinterpret_cast<EngineSystems*>(reinterpret_cast<ptrdiff_t>(this) - offsetFromOwner);
	}

	PURE_LOCALS_AND_POINTERS const EngineSystems& Engine::GetSystems() const
	{
		const ptrdiff_t offsetFromOwner = Memory::GetOffsetOf(&EngineSystems::m_engine);
		return *reinterpret_cast<EngineSystems*>(reinterpret_cast<ptrdiff_t>(this) - offsetFromOwner);
	}

#if HAS_LIVEPP
	// TODO: Move to plug-in
	static lpp::LppSynchronizedAgent s_livePlusPlusAgent;
#endif

	Engine::Engine(const CommandLine::InitializationParameters& commandLineArguments)
		: m_mainThreadId(Threading::ThreadId::GetCurrent())
		, m_flags(Flags::IsRunning | Flags::FinishedFrame | Flags::IsStarting)
		, m_commandLineArguments(commandLineArguments)
	{
#if HAS_LIVEPP
		s_livePlusPlusAgent = lpp::LppCreateSynchronizedAgent(
			nullptr,
			IO::Path::Combine(IO::Path::GetExecutableDirectory(), MAKE_PATH("Live++")).GetZeroTerminated()
		);
		const bool wasLivePlusPlusInitialized = lpp::LppIsValidSynchronizedAgent(&s_livePlusPlusAgent);
		if (Ensure(wasLivePlusPlusInitialized))
		{
			s_livePlusPlusAgent.EnableModule(lpp::LppGetCurrentModulePath(), lpp::LPP_MODULES_OPTION_NONE, nullptr, nullptr);
		}
#endif

		m_startTickStage.AddSubsequentStage(m_endTickStage);

		m_flags &= ~Flags::IsStarting;
	}

	Engine::~Engine()
	{
		Quit();

		UnloadPlugins();

#if HAS_LIVEPP
		lpp::LppDestroySynchronizedAgent(&s_livePlusPlusAgent);
#endif
	}

	void Engine::RunMainLoop()
	{
		while (DoTick())
			;

		OnBeforeQuitInternal();
	}

	void Engine::OnBeforeQuitInternal()
	{
		if (m_flags.IsSet(Flags::WaitForTasksBeforeQuit))
		{
			Threading::EngineJobManager& jobManager = GetSystems().m_jobManager;
			const uint8 otherThreadCount = (uint8)(jobManager.GetJobThreads().GetSize() - (uint8)1u);
			while (jobManager.GetNumberOfIdleThreads() < otherThreadCount || Threading::JobRunnerThread::GetCurrent()->HasWork())
			{
				Threading::JobRunnerThread::GetCurrent()->DoRunNextJob();
			}
		}

		OnBeforeQuit();
	}

	void Engine::StartTickStage::OnDependenciesResolved(Threading::JobRunnerThread&)
	{
		// Wake the main thread in case it went idle

		Engine& engine = Memory::GetOwnerFromMember(*this, &Engine::m_startTickStage);
		Threading::EngineJobManager& jobManager = engine.GetSystems().m_jobManager;

		Threading::JobRunnerThread& mainRunnerThread = *jobManager.GetJobThreads()[0];
		mainRunnerThread.Wake();
	}

	bool Engine::DoTick()
	{
		const Time::Durationd updateRate = m_updateRate;
		if (updateRate == 0_seconds)
		{
			return DoTickInternal();
		}

		const Time::Durationd currentTime = Time::Durationd::GetCurrentSystemUptime();
		if (UNLIKELY(m_lastTickTime <= 0_seconds))
		{
			m_lastTickTime = currentTime;
		}

		const Time::Durationd timeSinceLastTick = currentTime - m_lastTickTime;
		m_lastTickTime = currentTime;
		m_tickAccumulator = m_tickAccumulator + timeSinceLastTick;

		// Leaving the option to detect whether we need a tick
		constexpr bool hasTickToPerform = true;
		if (hasTickToPerform)
		{
			constexpr uint8 maximumTicksPerFrame = 16;
			m_tickAccumulator = Math::Min(m_tickAccumulator, updateRate * maximumTicksPerFrame);
			while (m_tickAccumulator >= updateRate)
			{
				DoTickInternal();
				m_tickAccumulator = m_tickAccumulator - updateRate;
			}
		}
		else
		{
			m_tickAccumulator = 0_seconds;
			if (m_flags.IsSet(Flags::RequestedQuit))
			{
				return false;
			}
		}

		return true;
	}

	bool Engine::DoTickInternal()
	{
		Threading::EngineJobRunnerThread& mainRunnerThread = *Threading::EngineJobRunnerThread::GetCurrent();
		while (!m_startTickStage.AreAllDependenciesResolved())
		{
			mainRunnerThread.Tick();
		}

		Threading::EngineJobManager& jobManager = GetSystems().m_jobManager;

		for (Threading::JobRunnerThread& thread : jobManager.GetJobThreads())
		{
			Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
			Rendering::JobRunnerData& jobRunnerData = engineThread.GetRenderData();

			// TODO: Allow multiple frames in flight?
			jobRunnerData.AwaitAnyFramesFinish(Rendering::AllFramesMask);
		}

		if (m_flags.IsSet(Flags::RequestedQuit))
		{
			return false;
		}

#if HAS_LIVEPP
		if (s_livePlusPlusAgent.WantsReload(lpp::LPP_RELOAD_OPTION_SYNCHRONIZE_WITH_RELOAD))
		{
			s_livePlusPlusAgent.Reload(lpp::LPP_RELOAD_BEHAVIOUR_WAIT_UNTIL_CHANGES_ARE_APPLIED);
		}

		if (s_livePlusPlusAgent.WantsRestart())
		{
			s_livePlusPlusAgent.Restart(lpp::LPP_RESTART_BEHAVIOUR_INSTANT_TERMINATION, 0u, nullptr);
		}
#endif

		m_flags |= Flags::IsBeforeFrameStep;
		OnBeforeStartFrame.ExecuteAndClear();
		m_flags &= ~Flags::IsBeforeFrameStep;

		m_flags |= Flags::IsTickActive;
		m_endTickStage.StartFrame();
		Assert(m_startTickStage.AreAllDependenciesResolved());

		m_startTickStage.Execute(mainRunnerThread);

#if STAGE_DEPENDENCY_DEBUG
		uint32 counter = 500000000;
#endif

		while (ShouldContinueTicking())
		{
			// Disable use of Tick as it causes a sleep that can hang
#if 1 // STAGE_DEPENDENCY_DEBUG
			mainRunnerThread.DoRunNextJob();
#else
			mainRunnerThread.Tick();
#endif

#if STAGE_DEPENDENCY_DEBUG
			if (--counter == 0)
			{
				BreakIfDebuggerIsAttached();

				PrintRemainingDependencies();
			}
#endif
		}

		m_flags &= ~Flags::IsTickActive;

		// Tick one more time to ensure some progress on jobs in queue outside of update
		mainRunnerThread.DoRunNextJob();

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		{
			Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
			for (const Optional<Rendering::LogicalDevice*> pLogicalDevice : renderer.GetLogicalDevices())
			{
				if (pLogicalDevice.IsValid())
				{
					pLogicalDevice->ResetPushConstants();
				}
			}
		}
#endif

		m_frameIndex = (m_frameIndex + 1) % Rendering::MaximumConcurrentFrameCount;
		return true;
	}

#if STAGE_DEPENDENCY_DEBUG
	void Engine::PrintRemainingDependencies()
	{
		using FindRemainingDependency =
			void (*)(const Threading::StageBase& stage, Vector<ReferenceWrapper<const Threading::StageBase>>& remainingDependencies);
		static FindRemainingDependency findRemainingDependency =
			[](const Threading::StageBase& stage, Vector<ReferenceWrapper<const Threading::StageBase>>& remainingDependencies)
		{
			const auto stageRemainingDependencies = stage.GetRemainingDependencies();
			if (!stageRemainingDependencies.HasElements())
			{
				if (!remainingDependencies.Contains(stage))
				{
					remainingDependencies.EmplaceBack(stage);
				}
				return;
			}

			for (const Threading::StageBase& dependency : stageRemainingDependencies)
			{
				findRemainingDependency(dependency, remainingDependencies);
			}
		};

		Vector<ReferenceWrapper<const Threading::StageBase>> remainingDependencies;
		findRemainingDependency(m_endTickStage, remainingDependencies);

		for (const Threading::StageBase& remainingDependency : remainingDependencies)
		{
			LogMessage("{}", remainingDependency.GetDebugName());
		}
	}

	void Engine::ValidateTickGraph() const
	{
		/*using RecurseDependencies =
		  void (*)(const Threading::StageBase& stage);
		static RecurseDependencies recurseDependencies =
		  [](const Threading::StageBase& stage)
		{
		  Assert(stage.GetDebugName().HasElements(), "Stage must have a debug name");
		  for (const Threading::StageBase& dependency : stage.GetRemainingDependencies())
		  {
		    recurseDependencies(dependency);
		  }
		};
		recurseDependencies(m_endTickStage);*/
	}
#endif

	void Engine::EndTickStage::OnFinishedExecution(Threading::JobRunnerThread&)
	{
		Engine& engine = Memory::GetOwnerFromMember(*this, &Engine::m_endTickStage);
		[[maybe_unused]] const bool wasSet = engine.m_flags.TrySetFlags(Flags::FinishedFrame);
		Assert(wasSet);

		Threading::EngineJobManager& jobManager = engine.GetSystems().m_jobManager;
		Threading::JobRunnerThread& mainRunnerThread = jobManager.GetJobThreads()[0];
		mainRunnerThread.Wake();
	}

	Engine::ProjectLoadResult Engine::LoadProject(ProjectInfo&& project)
	{
		if (UNLIKELY(!project.IsValid()))
		{
			return ProjectLoadResult{};
		}

		{
			[[maybe_unused]] const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsLoadingProject);
			Assert(previousFlags.AreNoneSet(Flags::IsLoadingProject), "Can't queue multiple project loads at the same time");
		}

		Project& currentProject = System::Get<Project>();
		currentProject.SetInfo(Forward<ProjectInfo>(project));
		ProjectInfo& currentProjectInfo = *currentProject.GetInfo();

		LogMessage("Loading project {}", currentProjectInfo.GetConfigFilePath());

		Asset::Manager& assetManager = System::Get<Asset::Manager>();

		Threading::JobBatch jobBatch;

		if (currentProjectInfo.HasPlugins())
		{
			m_flags |= Flags::IsLoadingPlugin;

			for (const Asset::Guid pluginGuid : currentProjectInfo.GetPluginGuids())
			{
				PluginLoadResult pluginLoadResult = LoadPluginInternal(pluginGuid, Guid{}, true);
				if (UNLIKELY(pluginLoadResult.pPluginInstance.IsInvalid()))
				{
					LogError("Failed to load a plugin! {}", pluginGuid);
					OnProjectLoadFailed();
					return ProjectLoadResult{};
				}

				jobBatch.QueueAfterStartStage(pluginLoadResult.jobBatch);
			}
		}

		{
			const IO::PathView projectDirectory = currentProjectInfo.GetDirectory();
			IO::Path assetsDatabasePath = IO::Path::Combine(
				projectDirectory,
				IO::Path::Merge(currentProjectInfo.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
			);

			assetManager.Import(Asset::LibraryReference{currentProjectInfo.GetAssetDatabaseGuid(), Asset::Database::AssetFormat.assetTypeGuid});

			const Optional<Threading::Job*> pLoadDatabaseJob = assetManager.RequestAsyncLoadAssetMetadata(
				currentProjectInfo.GetAssetDatabaseGuid(),
				Threading::JobPriority::LoadProject,
				[this,
			   assetsDatabasePath = Move(assetsDatabasePath),
			   projectDirectory = IO::Path(projectDirectory),
			   projectGuid = currentProjectInfo.GetGuid()](const ConstByteView data)
				{
					Serialization::RootReader rootReader = Serialization::GetReaderFromBuffer(
						ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
					);
					if (UNLIKELY(!rootReader.GetData().IsValid()))
					{
						LogError("Project load failed: Asset database could not be found");
						OnProjectLoadFailed();
						return;
					}

					Assert(m_flags.IsSet(Flags::IsLoadingProject));
					const Serialization::Reader assetDatabaseReader(rootReader);

					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					const Tag::Identifier projectTagIdentifier = System::Get<Tag::Registry>().FindOrRegister(projectGuid);
					const Tag::Identifier importedTagIdentifier = System::Get<Tag::Registry>().FindOrRegister(Asset::Library::ImportedTagGuid);

					// Import all project assets from the asset library if they weren't already
					{
						Asset::Database assetDatabase(assetDatabaseReader.GetData(), assetsDatabasePath.GetParentPath());
						assetDatabase.IterateAssets(
							[&assetManager](const Guid assetGuid, const Asset::DatabaseEntry& assetEntry)
							{
								assetManager.Import(Asset::LibraryReference{assetGuid, assetEntry.m_assetTypeGuid}, Asset::ImportingFlags::FullHierarchy);
								return Memory::CallbackResult::Continue;
							}
						);
					}

					const Asset::Identifier projectRootFolderAssetIdentifier =
						assetManager.FindOrRegisterFolder(projectDirectory, Asset::Identifier{});
					if (UNLIKELY(!assetManager.Load(
								assetsDatabasePath,
								assetDatabaseReader,
								projectDirectory,
								projectRootFolderAssetIdentifier,
								Array{projectTagIdentifier}.GetDynamicView(),
								Array{projectTagIdentifier, importedTagIdentifier}.GetDynamicView()
							)))
					{
						LogError("Project load failed: Failed to load asset database! {}", assetsDatabasePath);
						OnProjectLoadFailed();
						return;
					}
				}
			);
			if (pLoadDatabaseJob.IsValid())
			{
				jobBatch.QueueAsNewFinishedStage(*pLoadDatabaseJob);
			}
		}

		jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
			[this](Threading::JobRunnerThread&)
			{
				if (m_flags.IsSet(Flags::IsLoadingProject))
				{
					[[maybe_unused]] const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~(Flags::IsLoadingProject | Flags::IsLoadingPlugin));
					Assert(previousFlags.IsSet(Flags::IsLoadingProject));
					LogMessage("Finished project load");
				}
			},
			Threading::JobPriority::LoadPlugin
		));

		return ProjectLoadResult{currentProject, jobBatch};
	}

	void Engine::UnloadProject()
	{
	}

	Threading::JobBatch Engine::LoadDefaultResources()
	{
		Asset::EngineManager& assetManager = static_cast<Asset::EngineManager&>(System::Get<Asset::Manager>());
		return assetManager.LoadDefaultResources();
	}

	Engine::PluginLoadResult Engine::LoadPlugin(const Guid pluginGuid)
	{
		[[maybe_unused]] const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsLoadingPlugin);
		Assert(
			previousFlags.AreNoneSet(Flags::IsLoadingPlugin | Flags::IsLoadingProject),
			"Can't queue multiple project or plug-in loads at the same time"
		);
		PluginLoadResult loadResult = LoadPluginInternal(pluginGuid, {}, false);
		loadResult.jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
			[this](Threading::JobRunnerThread&)
			{
				if (m_flags.IsSet(Flags::IsLoadingPlugin))
				{
					[[maybe_unused]] const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsLoadingPlugin);
					Assert(previousFlags.IsSet(Flags::IsLoadingPlugin));
					LogMessage("Finished plug-in load");
				}
			},
			Threading::JobPriority::LoadPlugin
		));
		return loadResult;
	}

	Engine::PluginLoadResult Engine::LoadPlugin(PluginInfo&& plugin)
	{
		[[maybe_unused]] const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsLoadingPlugin);
		Assert(
			previousFlags.AreNoneSet(Flags::IsLoadingPlugin | Flags::IsLoadingProject),
			"Can't queue multiple project or plug-in loads at the same time"
		);
		PluginLoadResult loadResult = LoadPluginInternal(Forward<PluginInfo>(plugin), {}, false);
		loadResult.jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
			[this](Threading::JobRunnerThread&)
			{
				if (m_flags.IsSet(Flags::IsLoadingPlugin))
				{
					[[maybe_unused]] const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsLoadingPlugin);
					Assert(previousFlags.IsSet(Flags::IsLoadingPlugin));
					LogMessage("Finished plug-in load");
				}
			},
			Threading::JobPriority::LoadPlugin
		));
		return loadResult;
	}

	[[nodiscard]] bool Engine::IsDirectOrIndirectDependency(const PluginInstance& pluginInstance, const Guid requestingPluginGuid)
	{
		Assert(requestingPluginGuid.IsValid());
		if (pluginInstance.GetDependencies().Contains(requestingPluginGuid))
		{
			return true;
		}
		else
		{
			/*for(const Guid dependencyGuid : pluginInstance.GetDependencies())
			{
			    const Optional<PluginInstance*> pExistingPluginInstance = FindPlugin(dependencyGuid);
			    Assert(pExistingPluginInstance.IsValid());
			    if(IsDirectOrIndirectDependency(*pExistingPluginInstance, requestingPluginGuid))
			    {
			        return true;
			    }
			}*/
			return false;
		}
	}

	Engine::PluginLoadResult Engine::LoadPluginInternal(const Guid pluginGuid, const Guid requestingPluginGuid, const bool isFromProject)
	{
		// Check if plug-in was already loaded
		if (const Optional<PluginInstance*> pExistingPluginInstance = FindPlugin(pluginGuid))
		{
			Threading::UniqueLock lock(pExistingPluginInstance->m_onLoadedMutex);
			if (pExistingPluginInstance->IsLoaded() || (requestingPluginGuid.IsValid() && IsDirectOrIndirectDependency(*pExistingPluginInstance, requestingPluginGuid)))
			{
				return PluginLoadResult{pExistingPluginInstance};
			}
			else
			{
				Threading::IntermediateStage& intermediateStage = Threading::CreateIntermediateStage();
				Threading::JobBatch jobBatch{Threading::JobBatch::IntermediateStage};
				intermediateStage.AddSubsequentStage(jobBatch.GetFinishedStage());

				pExistingPluginInstance->m_onLoaded.Add(
					*this,
					[&intermediateStage](Engine&)
					{
						intermediateStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
					}
				);
				return PluginLoadResult{pExistingPluginInstance, jobBatch};
			}
		}

		PluginInstance& storedPluginInstance = StartPluginLoading(pluginGuid);

		Asset::Manager& assetManager = System::Get<Asset::Manager>();
		[[maybe_unused]] Asset::Identifier identifier = assetManager.Import(Asset::LibraryReference{pluginGuid, PluginAssetFormat.assetTypeGuid}
		);
		if (UNLIKELY(identifier.IsInvalid()))
		{
			LogError("Plug-in {} could not be found!", pluginGuid.ToString());
			Assert(identifier.IsValid());
		}

		Threading::IntermediateStage& intermediateStage = Threading::CreateIntermediateStage();
		const Optional<Threading::Job*> pLoadPluginMetadataJob = assetManager.RequestAsyncLoadAssetMetadata(
			pluginGuid,
			Threading::JobPriority::LoadPlugin,
			[this, isFromProject, &intermediateStage, pluginGuid, &storedPluginInstance](const ConstByteView data)
			{
				Serialization::RootReader rootReader = Serialization::GetReaderFromBuffer(
					ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
				);
				if (UNLIKELY(!rootReader.GetData().IsValid()))
				{
					LogError("Plug-in load failed: Plug-in metadata could not be found");
					OnPluginLoadFailed();
					return;
				}

				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				IO::Path pluginPath = assetManager.VisitAssetEntry(
					pluginGuid,
					[](const Optional<const Asset::DatabaseEntry*> pEntry)
					{
						Assert(pEntry.IsValid());
						if (LIKELY(pEntry.IsValid()))
						{
							return pEntry->m_path;
						}
						else
						{
							return IO::Path{};
						}
					}
				);

				const Serialization::Reader assetDatabaseReader(rootReader);
				PluginInfo pluginInfo{UnicodeString{}, Move(pluginPath), Asset::Guid{}};
				pluginInfo.Serialize(assetDatabaseReader);
				storedPluginInstance = Move(pluginInfo);

				const PluginLoadResult loadResult = LoadPluginInternal(storedPluginInstance, isFromProject);
				Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
				if (loadResult.jobBatch.IsValid())
				{
					loadResult.jobBatch.GetFinishedStage().AddSubsequentStage(intermediateStage);
					thread.Queue(loadResult.jobBatch);
				}
				else
				{
					intermediateStage.SignalExecutionFinishedAndDestroying(thread);
				}
			}
		);
		if (pLoadPluginMetadataJob != nullptr)
		{
			Threading::JobBatch jobBatch{*pLoadPluginMetadataJob, Threading::JobBatch::IntermediateStage};
			intermediateStage.AddSubsequentStage(jobBatch.GetFinishedStage());
			return PluginLoadResult{storedPluginInstance, jobBatch};
		}
		else
		{
			return PluginLoadResult{storedPluginInstance, {}};
		}
	}

	Engine::PluginLoadResult Engine::LoadPluginInternal(PluginInfo&& plugin, const Guid requestingPluginGuid, const bool isFromProject)
	{
		Assert(plugin.IsValid());
		if (const Optional<PluginInstance*> pExistingPluginInstance = FindPlugin(plugin.GetGuid()))
		{
			Threading::UniqueLock lock(pExistingPluginInstance->m_onLoadedMutex);
			if (pExistingPluginInstance->IsLoaded() || (requestingPluginGuid.IsValid() && IsDirectOrIndirectDependency(*pExistingPluginInstance, requestingPluginGuid)))
			{
				return PluginLoadResult{pExistingPluginInstance};
			}
			else
			{
				Threading::IntermediateStage& intermediateStage = Threading::CreateIntermediateStage();
				Threading::JobBatch jobBatch{Threading::JobBatch::IntermediateStage};
				intermediateStage.AddSubsequentStage(jobBatch.GetFinishedStage());

				pExistingPluginInstance->m_onLoaded.Add(
					*this,
					[&intermediateStage](Engine&)
					{
						intermediateStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
					}
				);
				return PluginLoadResult{pExistingPluginInstance, jobBatch};
			}
		}

		PluginInstance& storedPluginInstance = StartPluginLoading(Forward<PluginInfo>(plugin));
		return LoadPluginInternal(storedPluginInstance, isFromProject);
	}

	Engine::PluginLoadResult Engine::LoadPluginInternal(PluginInstance& storedPluginInstance, const bool isFromProject)
	{
		Assert(storedPluginInstance.IsValid());

		Threading::JobBatch jobBatch;

		LogMessage("Loading plug-in dependencies for plug-in {0}", storedPluginInstance.GetName());

		for (const Asset::Guid dependencyPluginGuid : storedPluginInstance.GetDependencies())
		{
			PluginLoadResult dependencyLoadResult = LoadPluginInternal(dependencyPluginGuid, storedPluginInstance.GetGuid(), isFromProject);
			if (UNLIKELY_ERROR(dependencyLoadResult.pPluginInstance.IsInvalid()))
			{
				OnPluginLoadFailed();
				storedPluginInstance.OnFinishedLoading();
				return PluginLoadResult{};
			}

			jobBatch.QueueAfterStartStage(dependencyLoadResult.jobBatch);
		}

		LogMessage("Loading plug-in {0}", storedPluginInstance.GetName());

		if (storedPluginInstance.HasAssetDirectory())
		{
			LogMessage("Loading plug-in assets for {}", storedPluginInstance.GetName());

			IO::Path assetsDatabasePath = IO::Path::Combine(
				storedPluginInstance.GetDirectory(),
				IO::Path::Merge(storedPluginInstance.GetRelativeAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
			);

			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			Asset::Guid assetDatabaseGuid = storedPluginInstance.GetAssetDatabaseGuid();
			if (assetDatabaseGuid.IsInvalid())
			{
				// Handle old plug-ins before we introduced storing the asset database guid
				assetDatabaseGuid = assetManager.GetAssetGuid(assetsDatabasePath);
			}
			Assert(assetDatabaseGuid.IsValid());

			Asset::Library& assetLibrary = assetManager.GetAssetLibrary();
			if (!assetLibrary.HasAsset(assetDatabaseGuid))
			{
				assetLibrary.RegisterAsset(
					storedPluginInstance.GetAssetDatabaseGuid(),
					Asset::DatabaseEntry{
						Asset::Database::AssetFormat.assetTypeGuid,
						{},
						IO::Path::Merge(storedPluginInstance.GetAssetDirectory(), Asset::Database::AssetFormat.metadataFileExtension)
					},
					assetLibrary.FindOrRegisterFolder(storedPluginInstance.GetDirectory(), {}),
					{}
				);
			}

			if (!assetLibrary.HasAsset(storedPluginInstance.GetGuid()))
			{
				assetLibrary.RegisterAsset(
					storedPluginInstance.GetGuid(),
					Asset::DatabaseEntry{storedPluginInstance},
					assetLibrary.FindOrRegisterFolder(storedPluginInstance.GetDirectory(), {}),
					{}
				);
			}

			const Asset::Identifier assetDatabaseIdentifier =
				assetManager.Import(Asset::LibraryReference{assetDatabaseGuid, Asset::Database::AssetFormat.assetTypeGuid});

			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
			const Tag::Identifier pluginAssetTagIdentifier = tagRegistry.FindOrRegister(Asset::Library::PluginTagGuid);
			const Tag::Identifier engineAssetTagIdentifier = tagRegistry.FindOrRegister(Asset::EngineManager::EngineAssetsTagGuid);
			FlatVector<Tag::Identifier, 2> tagIdentifiers{pluginAssetTagIdentifier};

			// Ensure we correctly assign the plug-in
			Asset::Identifier pluginRootFolderAssetIdentifier;
			if (assetManager.IsTagSet(engineAssetTagIdentifier, assetDatabaseIdentifier))
			{
				tagIdentifiers.EmplaceBack(engineAssetTagIdentifier);
				pluginRootFolderAssetIdentifier =
					assetManager.FindOrRegisterFolder(System::Get<IO::Filesystem>().GetEnginePath(), Asset::Identifier{});
			}
			else
			{
				pluginRootFolderAssetIdentifier = assetManager.FindOrRegisterFolder(assetsDatabasePath.GetParentPath(), Asset::Identifier{});
			}

			const Optional<Threading::Job*> pLoadDatabaseJob = assetManager.RequestAsyncLoadAssetMetadata(
				assetDatabaseGuid,
				Threading::JobPriority::LoadPlugin,
				[this, assetsDatabasePath = Move(assetsDatabasePath), pluginRootFolderAssetIdentifier, tagIdentifiers = Move(tagIdentifiers)](
					const ConstByteView data
				)
				{
					Serialization::RootReader rootReader = Serialization::GetReaderFromBuffer(
						ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
					);
					if (UNLIKELY(!rootReader.GetData().IsValid()))
					{
						LogError("Plug-in load failed: Asset database could not be found");
						OnPluginLoadFailed();
						return;
					}

					const Serialization::Reader assetDatabaseReader(rootReader);

					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					// Import all plug-in assets from the asset library if they weren't already
					{
						Asset::Database assetDatabase(assetDatabaseReader.GetData(), assetsDatabasePath.GetParentPath());
						assetDatabase.IterateAssets(
							[&assetManager](const Guid assetGuid, const Asset::DatabaseEntry& assetEntry)
							{
								assetManager.Import(Asset::LibraryReference{assetGuid, assetEntry.m_assetTypeGuid}, Asset::ImportingFlags::FullHierarchy);
								return Memory::CallbackResult::Continue;
							}
						);
					}

					if (UNLIKELY(!assetManager.Load(
								assetsDatabasePath,
								assetDatabaseReader,
								assetsDatabasePath.GetParentPath(),
								pluginRootFolderAssetIdentifier,
								tagIdentifiers.GetView()
							)))
					{
						LogError("Plug-in load failed: Failed to load asset database! {}", assetsDatabasePath);
						OnPluginLoadFailed();
						return;
					}
				}
			);
			if (pLoadDatabaseJob.IsValid())
			{
				jobBatch.QueueAsNewFinishedStage(*pLoadDatabaseJob);
			}
		}

		if (storedPluginInstance.HasSourceDirectory())
		{
			jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[this, &storedPluginInstance](Threading::JobRunnerThread&)
				{
					LogMessage("Loading plug-in binary for {}", storedPluginInstance.GetName());
					if (m_flags.IsSet(Flags::IsLoadingPlugin))
					{
						if (UNLIKELY_ERROR(!LoadPluginBinary(storedPluginInstance)))
						{
							OnPluginLoadFailed();
						}
					}
				},
				Threading::JobPriority::LoadPlugin,
				"Load plug-in binary"
			));
		}

		jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
			[&storedPluginInstance](Threading::JobRunnerThread&)
			{
				storedPluginInstance.OnFinishedLoading();
				LogMessage("Finished loading plug-in {}", storedPluginInstance.GetName());
			},
			Threading::JobPriority::LoadPlugin,
			"Finish plug-in loading"
		));

		return PluginLoadResult{storedPluginInstance, jobBatch};
	}

	void Engine::OnProjectLoadFailed()
	{
		Assert(false, "Project load failed!");
		m_flags.Clear(Flags::IsLoadingProject | Flags::IsLoadingPlugin);
	}

	void Engine::OnPluginLoadFailed()
	{
		Assert(false, "Plug-in load failed!");
		m_flags.Clear(Flags::IsLoadingProject | Flags::IsLoadingPlugin);
	}

	void Engine::Quit()
	{
		QuitInternal();

		if (Threading::JobRunnerThread* pThread = Threading::JobRunnerThread::GetCurrent())
		{
			// Run all jobs until we're ready to quit
			while (!IsAboutToQuit())
			{
				pThread->Tick();
			}
		}
		else
		{
			while (!IsAboutToQuit())
				;
		}
	}

	void Engine::QuitAndWaitForTasks()
	{
		m_flags |= Flags::WaitForTasksBeforeQuit;
		QuitInternal();
	}

	void Engine::QuitInternal()
	{
		if (m_flags.TrySetFlags(Flags::RequestedQuit))
		{
#if PLATFORM_WINDOWS || PLATFORM_LINUX || PLATFORM_APPLE_MACOS || PLATFORM_ANDROID
			PUSH_MSVC_WARNINGS
			DISABLE_MSVC_WARNINGS(4702)
			System::Get<Threading::JobManager>().ScheduleAsync(
				5_seconds,
				[](Threading::JobRunnerThread&)
				{
					Assert(false, "Quit timeout exceeded, terminating application");
					volatile bool test = true;
					if (test)
					{
						std::quick_exit(1);
					}
				},
				Threading::JobPriority::Present
			);
			POP_MSVC_WARNINGS
#endif

			// Quit once all the high priority tasks are done.
			m_quitJob.QueueAsNewFinishedStage(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					[[maybe_unused]] const bool wasCleared = m_flags.TryClearFlags(Flags::IsRunning);
					Assert(wasCleared);
				},
				Threading::JobPriority::EndFrame,
				"Finish quitting"
			));

			if (Threading::JobRunnerThread* pThread = Threading::JobRunnerThread::GetCurrent())
			{
				pThread->Queue(m_quitJob);
			}
			else
			{
				System::Get<Threading::JobManager>().Queue(m_quitJob, Threading::JobPriority::EndFrame);
			}
		}
	}

	void Engine::ModifyFrameGraph(Function<void(), 24>&& callback)
	{
		if (CanModifyFrameGraph())
		{
			callback();
		}
		else
		{
			OnBeforeStartFrame.Add(
				*this,
				[callback = Move(callback)](Engine&)
				{
					callback();
				}
			);
		}
	}

	bool Engine::CanModifyFrameGraph() const
	{
		const EnumFlags<Flags> flags = m_flags.GetFlags();
		if ((flags.IsNotSet(Flags::IsTickActive) | flags.AreAnySet(Flags::IsStarting | Flags::IsBeforeFrameStep)) && (GetSystems().m_jobManager.GetJobThreads().IsEmpty() || GetSystems().m_jobManager.GetJobThreads()[0]->IsExecutingOnThread()))
		{
			return true;
		}

		return false;
	}
}
