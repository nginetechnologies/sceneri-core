#include "Scene/SceneBase.h"

#include <Engine/Engine.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Entity/Scene/SceneRegistry.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/System/Query.h>

namespace ngine
{
	SceneBase::SceneBase(Entity::SceneRegistry& sceneRegistry, const Guid guid, const EnumFlags<Flags> flags)
		: m_sceneRegistry(sceneRegistry)
		, m_guid(guid)
		, m_flags(flags)
		, m_pStartFrameJob(UniquePtr<Threading::Job>::FromRaw(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					OnStartFrame();
					return Threading::Job::Result::Finished;
				},
				Threading::JobPriority::StartFrame,
				"Scene Start Frame"
			)))
		, m_pDestroyComponentsJob(UniquePtr<Threading::Job>::FromRaw(Threading::CreateCallback(
				[this](Threading::JobRunnerThread&)
				{
					ProcessDestroyedComponentsQueue();
					return Threading::CallbackResult::Finished;
				},
				Threading::JobPriority::QueuedComponentDestructions,
				"Destroy Components"
			)))
	{
		Threading::StageBase& startFrameJob = *m_pStartFrameJob;
		Threading::StageBase& endFrameJob = m_endFrameStage;

		startFrameJob.AddSubsequentStage(endFrameJob);

		Threading::StageBase& destroyComponentsStage = *m_pDestroyComponentsJob;

		startFrameJob.AddSubsequentStage(destroyComponentsStage);
		destroyComponentsStage.AddSubsequentStage(endFrameJob);
		m_polledForInputStage.AddSubsequentStage(*m_pDestroyComponentsJob);

		if (flags.IsNotSet(Flags::IsDisabled))
		{
			Engine& engine = System::Get<Engine>();
			Input::Manager& inputManager = System::Get<Input::Manager>();

			Threading::StageBase& startFrameStage = GetStartFrameStage();
			Threading::StageBase& endFrameStage = GetEndFrameStage();
			Threading::StageBase& polledForInputStage = GetPolledForInputStage();

			engine.GetStartTickStage().AddSubsequentStage(startFrameStage);
			endFrameStage.AddSubsequentStage(engine.GetEndTickStage());
			inputManager.GetPolledForInputStage().AddSubsequentStage(polledForInputStage);

			m_sceneRegistry.GetDynamicRenderUpdatesFinishedStage().AddSubsequentStage(destroyComponentsStage);
			m_sceneRegistry.GetDynamicLateUpdatesFinishedStage().AddSubsequentStage(destroyComponentsStage);
			m_sceneRegistry.GetPhysicsSimulationFinishedStage().AddSubsequentStage(destroyComponentsStage);

			m_sceneRegistry.Enable(startFrameStage, endFrameStage);
		}
	}

	SceneBase::~SceneBase()
	{
		Assert(!System::Find<Engine>().IsValid() || !System::Get<Engine>().GetStartTickStage().IsDirectlyFollowedBy(*m_pStartFrameJob));
	}

	void SceneBase::OnBeforeUnload()
	{
		Assert(!m_pStartFrameJob->IsQueuedOrExecuting());
		if (System::Get<Engine>().GetStartTickStage().IsDirectlyFollowedBy(*m_pStartFrameJob))
		{
			Disable();
		}
	}

	void SceneBase::OnStartFrame()
	{
		m_frameCounter += 1;
		if (m_stopwatch.IsRunning())
		{
			m_frameTime = FrameTime(m_stopwatch.GetElapsedTimeAndRestart());
			m_time += m_frameTime;
		}
		else
		{
			m_stopwatch.Start();
			m_frameTime = FrameTime(0_seconds);
		}
	}

	void SceneBase::ModifyFrameGraph(Function<void(), 24>&& callback)
	{
		System::Get<Engine>().ModifyFrameGraph(
			[this, callback = Move(callback)]()
			{
				m_flags |= Flags::IsModifyingFrameGraph;
				m_sceneRegistry.EnableSceneFrameGraphModification();
				callback();
				m_sceneRegistry.DisableSceneFrameGraphModification();
				m_flags &= ~Flags::IsModifyingFrameGraph;
			}
		);
	}

	void SceneBase::Enable()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::IsDisabled);
		if (previousFlags.IsSet(Flags::IsDisabled))
		{
			System::Get<Engine>().ModifyFrameGraph(
				[this]()
				{
					if (m_flags.IsNotSet(Flags::IsDisabled))
					{
						m_flags |= Flags::IsModifyingFrameGraph;
						m_sceneRegistry.EnableSceneFrameGraphModification();
						OnEnabledInternal();
						m_sceneRegistry.DisableSceneFrameGraphModification();
						m_flags &= ~Flags::IsModifyingFrameGraph;
					}
				}
			);
		}
	}

	void SceneBase::OnEnabledInternal()
	{
		Engine& engine = System::Get<Engine>();
		Assert(engine.CanModifyFrameGraph());
		Input::Manager& inputManager = System::Get<Input::Manager>();

		Threading::StageBase& startFrameStage = GetStartFrameStage();
		Threading::StageBase& endFrameStage = GetEndFrameStage();
		Threading::StageBase& polledForInputStage = GetPolledForInputStage();

		if (!engine.GetStartTickStage().IsDirectlyFollowedBy(startFrameStage))
		{
			engine.GetStartTickStage().AddSubsequentStage(startFrameStage);
			endFrameStage.AddSubsequentStage(engine.GetEndTickStage());
			inputManager.GetPolledForInputStage().AddSubsequentStage(polledForInputStage);
			Threading::StageBase& destroyComponentsStage = *m_pDestroyComponentsJob;

			m_sceneRegistry.GetDynamicRenderUpdatesFinishedStage().AddSubsequentStage(destroyComponentsStage);
			m_sceneRegistry.GetDynamicLateUpdatesFinishedStage().AddSubsequentStage(destroyComponentsStage);
			m_sceneRegistry.GetPhysicsSimulationFinishedStage().AddSubsequentStage(destroyComponentsStage);

			m_sceneRegistry.Enable(startFrameStage, endFrameStage);
		}

		OnEnabledUpdate();
		Assert(engine.CanModifyFrameGraph());
	}

	void SceneBase::Disable()
	{
		const EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::IsDisabled);
		if (!previousFlags.IsSet(Flags::IsDisabled))
		{
			if (m_flags.IsSet(Flags::IsModifyingFrameGraph))
			{
				OnDisabledInternal();
			}
			else
			{
				System::Get<Engine>().ModifyFrameGraph(
					[this]()
					{
						Assert(m_flags.IsSet(Flags::IsDisabled));
						m_flags |= Flags::IsModifyingFrameGraph;
						m_sceneRegistry.EnableSceneFrameGraphModification();
						OnDisabledInternal();
						m_sceneRegistry.DisableSceneFrameGraphModification();
						m_flags &= ~Flags::IsModifyingFrameGraph;
					}
				);
			}
		}
	}

	void SceneBase::OnDisabledInternal()
	{
		Engine& engine = System::Get<Engine>();
		Assert(engine.CanModifyFrameGraph());
		Input::Manager& inputManager = System::Get<Input::Manager>();

		Threading::StageBase& startFrameStage = GetStartFrameStage();
		Threading::StageBase& endFrameStage = GetEndFrameStage();
		Threading::StageBase& polledForInputStage = GetPolledForInputStage();

		if (engine.GetStartTickStage().IsDirectlyFollowedBy(startFrameStage))
		{
			engine.GetStartTickStage().RemoveSubsequentStage(startFrameStage, Invalid, Threading::StageBase::RemovalFlags{});
			endFrameStage.RemoveSubsequentStage(engine.GetEndTickStage(), Invalid, Threading::StageBase::RemovalFlags{});
			inputManager.GetPolledForInputStage().RemoveSubsequentStage(polledForInputStage, Invalid, Threading::StageBase::RemovalFlags{});

			Threading::StageBase& destroyComponentsStage = *m_pDestroyComponentsJob;

			m_sceneRegistry.GetDynamicRenderUpdatesFinishedStage()
				.RemoveSubsequentStage(destroyComponentsStage, Invalid, Threading::StageBase::RemovalFlags{});
			m_sceneRegistry.GetDynamicLateUpdatesFinishedStage()
				.RemoveSubsequentStage(destroyComponentsStage, Invalid, Threading::StageBase::RemovalFlags{});
			m_sceneRegistry.GetPhysicsSimulationFinishedStage()
				.RemoveSubsequentStage(destroyComponentsStage, Invalid, Threading::StageBase::RemovalFlags{});

			m_sceneRegistry.Disable(startFrameStage, endFrameStage);
		}

		OnDisabledUpdate();
		Assert(engine.CanModifyFrameGraph());
	}

	void SceneBase::ProcessDestroyedComponentsQueue()
	{
#if ENABLE_ASSERTS
		{
			Threading::UniqueLock lock(Threading::TryLock, m_queuedComponentDestructionsMutex);
			Assert(lock.IsLocked(), "Component destruction queue must not run simultaneously as any possible scene changes");
		}
#endif
		{
			Threading::UniqueLock lock(m_queuedComponentDestructionsMutex);
			QueuedComponentDestructionsContainer queuedComponentDestructions = Move(m_queuedComponentDestructions);
			m_queuedComponentDestructions = Move(m_queuedComponentDestructionsCopy);
			m_queuedComponentDestructionsCopy = Move(queuedComponentDestructions);
		}

		ProcessDestroyedComponentsQueueInternal(m_queuedComponentDestructionsCopy.GetView());
		m_queuedComponentDestructionsCopy.Clear();
	}

	void SceneBase::ProcessFullDestroyedComponentsQueue()
	{
		do
		{
			ProcessDestroyedComponentsQueue();
		} while (m_queuedComponentDestructions.HasElements() || m_queuedComponentDestructionsCopy.HasElements());
	}
}
