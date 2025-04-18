#pragma once

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/Mutexes/ConditionVariable.h>
#include <Common/Memory/Containers/Array.h>
#include <Renderer/Threading/SemaphoreView.h>

namespace ngine::Rendering
{
	struct StageBase;
	struct Stage;
	struct RenderOutput;
	struct StartFrameStage;
	struct LogicalDevice;
	struct LogicalDeviceView;

	struct PresentStage final : public Threading::Job
	{
		PresentStage(LogicalDevice& logicalDevice, RenderOutput& renderOutput, StartFrameStage& startFrameStage);
		virtual ~PresentStage();

		using Threading::Job::AddSubsequentStage;
		using Threading::Job::RemoveSubsequentStage;
		void AddSubsequentStage(Rendering::StageBase& stage) = delete;
		void RemoveSubsequentStage(Rendering::StageBase& stage, Threading::JobRunnerThread& thread) = delete;

		[[nodiscard]] ArrayView<const ReferenceWrapper<Rendering::Stage>, uint16> GetDependencies() const
		{
			return m_parentStages.GetView();
		}

		//! Whether the present requires one or more semaphores, otherwise a fence.
		[[nodiscard]] bool SupportsSemaphores() const;
	protected:
		friend Stage;
		void AddGpuDependency(Rendering::Stage& stage);
		void RemoveGpuDependency(Rendering::Stage& stage);

		[[nodiscard]] virtual Result OnExecute(Threading::JobRunnerThread& thread) override;
		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override;
#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Present Stage";
		}
#endif
	protected:
		LogicalDevice& m_logicalDevice;
		RenderOutput& m_renderOutput;
		StartFrameStage& m_startFrameStage;
		Vector<ReferenceWrapper<Rendering::Stage>, uint16> m_parentStages;
		Vector<SemaphoreView, uint16> m_waitSemaphores;
	};
}
