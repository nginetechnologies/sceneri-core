#pragma once

#include <Common/Application/Application.h>
#include <Common/Project System/EngineInfo.h>
#include <Common/CommandLine/CommandLineArguments.h>
#include <Common/Memory/ReferenceWrapper.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/Platform/NoUniqueAddress.h>

#include <Common/Threading/ThreadId.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/IntermediateStage.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Time/Duration.h>

namespace ngine::Asset
{
	struct EngineManager;
}

namespace ngine
{
	struct ProjectInfo;

	struct EngineSystems;

	struct Engine final : public Application
	{
		inline static constexpr System::Type SystemType = System::Type::Engine;

		enum class Flags : uint16
		{
			IsRunning = 1 << 0,
			WaitForTasksBeforeQuit = 1 << 1,
			IsTickActive = 1 << 2,
			IsBeforeFrameStep = 1 << 3,
			FinishedFrame = 1 << 4,
			IsStarting = 1 << 5,
			IsLoadingProject = 1 << 6,
			IsLoadingPlugin = 1 << 7,
			RequestedQuit = 1 << 8
		};

		Engine(const CommandLine::InitializationParameters& commandLineArguments);
		Engine(const Engine&) = delete;
		Engine& operator=(const Engine&) = delete;
		Engine(Engine&&) = delete;
		Engine& operator=(Engine&&) = delete;
		~Engine();

		void RunMainLoop();
		bool DoTick();

		void SetTickRate(const Time::Durationd updateRate)
		{
			m_updateRate = updateRate;
		}
		[[nodiscard]] Time::Durationd GetTickRate() const
		{
			return m_updateRate;
		}

		Event<void(void*), 24> OnRendererInitialized;
		ThreadSafe::Event<void(void*), 24, false> OnBeforeStartFrame;
		ThreadSafe::Event<void(void*), 24, false> OnBeforeQuit;

		[[nodiscard]] Guid GetSessionGuid() const
		{
			return m_sessionGuid;
		}

		[[nodiscard]] const CommandLine::Arguments& GetCommandLineArguments() const
		{
			return m_commandLineArguments;
		}

		[[nodiscard]] const EngineInfo& GetInfo() const
		{
			return m_engineInfo;
		}

		[[nodiscard]] Threading::ThreadId GetMainThreadId() const
		{
			return m_mainThreadId;
		}

		[[nodiscard]] uint8 GetCurrentFrameIndex() const
		{
			return m_frameIndex;
		}

		[[nodiscard]] Threading::JobBatch& GetQuitJobBatch()
		{
			return m_quitJob;
		}

		void Quit();
		void QuitAndWaitForTasks();

		[[nodiscard]] bool IsAboutToQuit() const
		{
			return !m_flags.IsSet(Flags::IsRunning);
		}
		[[nodiscard]] bool IsAwaitingQuit() const
		{
			return !m_flags.IsSet(Flags::RequestedQuit);
		}

		[[nodiscard]] Threading::ManualIntermediateStage& GetStartTickStage()
		{
			return m_startTickStage;
		}
		[[nodiscard]] Threading::IntermediateStage& GetEndTickStage()
		{
			return m_endTickStage;
		}
		[[nodiscard]] bool IsTicking() const
		{
			return m_flags.IsSet(Flags::IsTickActive);
		}
		[[nodiscard]] bool CanModifyFrameGraph() const;
		void ModifyFrameGraph(Function<void(), 24>&& callback);

		[[nodiscard]] Threading::JobBatch LoadDefaultResources();

		struct ProjectLoadResult
		{
			Optional<Project*> pProject;
			Threading::JobBatch jobBatch;
		};
		[[nodiscard]] ProjectLoadResult LoadProject(ProjectInfo&& project);
		void UnloadProject();

		bool LoadPlugin(PluginInfo&& plugin, const Asset::Database& assetDatabase, Asset::Database& targetAssetDatabase) = delete;
		bool LoadPlugin(const Guid pluginGuid, const Asset::Database& assetDatabase, Asset::Database& targetAssetDatabase) = delete;

		struct PluginLoadResult
		{
			Optional<PluginInstance*> pPluginInstance;
			Threading::JobBatch jobBatch;
		};
		[[nodiscard]] PluginLoadResult LoadPlugin(const Guid pluginGuid);
		[[nodiscard]] PluginLoadResult LoadPlugin(PluginInfo&& plugin);

		[[nodiscard]] PURE_LOCALS_AND_POINTERS EngineSystems& GetSystems();
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const EngineSystems& GetSystems() const;

#if STAGE_DEPENDENCY_DEBUG
		void PrintRemainingDependencies();
#endif
	protected:
		[[nodiscard]] bool IsDirectOrIndirectDependency(const PluginInstance& pluginInstance, const Guid requestingPluginGuid);
		[[nodiscard]] PluginLoadResult LoadPluginInternal(const Guid pluginGuid, const Guid requestingPluginGuid, const bool isFromProject);
		[[nodiscard]] PluginLoadResult LoadPluginInternal(PluginInfo&& plugin, const Guid requestingPluginGuid, const bool isFromProject);
		[[nodiscard]] PluginLoadResult LoadPluginInternal(PluginInstance& storedPluginInstance, const bool isFromProject);

		void OnProjectLoadFailed();
		void OnPluginLoadFailed();
		void QuitInternal();
		void OnBeforeQuitInternal();

		bool DoTickInternal();

		[[nodiscard]] bool ShouldContinueTicking() const
		{
			const EnumFlags<Flags>& nonAtomicFlags = reinterpret_cast<const EnumFlags<Flags>&>(m_flags);
			const EnumFlags<Flags> flags = nonAtomicFlags;
			return flags.IsNotSet(Flags::FinishedFrame);
		}

#if STAGE_DEPENDENCY_DEBUG
		void ValidateTickGraph() const;
#endif
	protected:
		Threading::ThreadId m_mainThreadId;
		AtomicEnumFlags<Flags> m_flags;
		//! Unique identifier for this engine session
		const Guid m_sessionGuid = Guid::Generate();

		const CommandLine::Arguments m_commandLineArguments;

		struct StartTickStage final : public Threading::ManualIntermediateStage
		{
			virtual ~StartTickStage() = default;

			virtual void OnDependenciesResolved(Threading::JobRunnerThread&) override final;

#if STAGE_DEPENDENCY_PROFILING
			[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
			{
				return "Engine Start Tick";
			}
#endif

#if STAGE_DEPENDENCY_DEBUG
			virtual void OnDependencyChanged() const override
			{
				Threading::StageBase::OnDependencyChanged();

				[[maybe_unused]] const Engine& engine = Memory::GetConstOwnerFromMember(*this, &Engine::m_startTickStage);
				Assert(engine.CanModifyFrameGraph());
				engine.ValidateTickGraph();
			}

			virtual void OnNextStageChanged() const override
			{
				Threading::StageBase::OnNextStageChanged();

				[[maybe_unused]] const Engine& engine = Memory::GetConstOwnerFromMember(*this, &Engine::m_startTickStage);
				Assert(engine.CanModifyFrameGraph());
				engine.ValidateTickGraph();
			}
#endif
		};

		StartTickStage m_startTickStage;

		struct EndTickStage final : public Threading::IntermediateStage
		{
			void StartFrame()
			{
				Engine& engine = Memory::GetOwnerFromMember(*this, &Engine::m_endTickStage);

				[[maybe_unused]] const bool wasCleared = engine.m_flags.TryClearFlags(Flags::FinishedFrame);
				Assert(wasCleared);
			}

#if STAGE_DEPENDENCY_DEBUG
			virtual void OnDependencyChanged() const override
			{
				Threading::StageBase::OnDependencyChanged();

				[[maybe_unused]] const Engine& engine = Memory::GetConstOwnerFromMember(*this, &Engine::m_endTickStage);
				Assert(engine.CanModifyFrameGraph());
				engine.ValidateTickGraph();
			}
#endif

#if STAGE_DEPENDENCY_DEBUG
			virtual void OnNextStageChanged() const override
			{
				Threading::StageBase::OnNextStageChanged();

				[[maybe_unused]] const Engine& engine = Memory::GetConstOwnerFromMember(*this, &Engine::m_endTickStage);
				Assert(engine.CanModifyFrameGraph());
				engine.ValidateTickGraph();
			}
#endif

#if STAGE_DEPENDENCY_PROFILING
			[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
			{
				return "Engine End Tick";
			}
#endif
		protected:
			virtual void OnFinishedExecution(Threading::JobRunnerThread&) override final;
		};

		EndTickStage m_endTickStage;

		friend Asset::EngineManager;
		EngineInfo m_engineInfo;

		uint8 m_frameIndex = 0u;

		//! The engine tick rate
		//! 0 = unlimited
		Time::Durationd m_updateRate{0_seconds};
		Time::Durationd m_lastTickTime = 0_seconds;
		Time::Durationd m_tickAccumulator = 0_seconds;

		Threading::JobBatch m_quitJob = Threading::JobBatch(Threading::JobBatch::IntermediateStage);
	};

	ENUM_FLAG_OPERATORS(Engine::Flags);
}
