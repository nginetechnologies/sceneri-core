#pragma once

#include <Common/Threading/Jobs/Job.h>
#include <Renderer/Threading/FenceView.h>
#include <Renderer/Threading/SemaphoreView.h>
#include <Renderer/PipelineStageFlags.h>

namespace ngine::Rendering
{
	struct Stage;

	struct StageBase : public Threading::Job
	{
		using Job::Job;

		[[nodiscard]] virtual SemaphoreView GetSubmissionFinishedSemaphore(const Threading::Job& stage) const = 0;
		[[nodiscard]] virtual bool IsSubmissionFinishedSemaphoreUsable() const = 0;
		[[nodiscard]] virtual ArrayView<const ReferenceWrapper<StageBase>, uint8> GetParentStages() const
		{
			return {};
		}
		[[nodiscard]] virtual FenceView GetSubmissionFinishedFence() const
		{
			return {};
		}
		[[nodiscard]] virtual bool IsSubmissionFinishedFenceUsable() const
		{
			return false;
		}

		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const
		{
			return {};
		}

		struct UsedStage
		{
			ReferenceWrapper<const StageBase> stage;
			ReferenceWrapper<const Threading::Job> nextStage;
		};

		template<typename Callback>
		[[nodiscard]] static void
		IterateUsedStages(ReferenceWrapper<const StageBase> parent, ReferenceWrapper<const Threading::Job> child, const Callback& callback)
		{
			while (!parent->IsSubmissionFinishedSemaphoreUsable())
			{
				child = parent;

				if (parent->GetParentStages().IsEmpty())
				{
					return;
				}
				if (parent->GetParentStages().GetSize() > 1)
				{
					for (const StageBase& multiParent : parent->GetParentStages())
					{
						IterateUsedStages(multiParent, child, callback);
					}
					return;
				}

				parent = parent->GetParentStages()[0];
			}

			callback(UsedStage{parent, child});
		}

		void AddSubsequentStage(Rendering::StageBase& stage) = delete;
		void RemoveSubsequentStage(
			Rendering::StageBase& stage,
			const Optional<Threading::JobRunnerThread*> pThread,
			const Threading::StageBase::RemovalFlags flags = Threading::StageBase::RemovalFlags::Default
		) = delete;

		void AddSubsequentCpuStage(Threading::StageBase& stage)
		{
			Threading::StageBase::AddSubsequentStage(stage);
		}

		void RemoveSubsequentCpuStage(
			Threading::StageBase& stage,
			const Optional<Threading::JobRunnerThread*> pThread,
			const Threading::StageBase::RemovalFlags flags = Threading::StageBase::RemovalFlags::Default
		)
		{
			Threading::StageBase::RemoveSubsequentStage(stage, pThread, flags);
		}

		void AddSubsequentGpuStage(Rendering::Stage& stage);
		void RemoveSubsequentGpuStage(Rendering::Stage& stage);

		void AddSubsequentCpuGpuStage(Rendering::Stage& stage);

		void RemoveSubsequentCpuGpuStage(
			Rendering::Stage& stage,
			const Optional<Threading::JobRunnerThread*> pThread,
			const Threading::StageBase::RemovalFlags flags = Threading::StageBase::RemovalFlags::Default
		);
	protected:
		virtual void OnAddedSubsequentGpuStage(Stage&)
		{
		}
		virtual void OnRemovedSubsequentGpuStage(Stage&)
		{
		}
	};
}
