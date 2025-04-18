#include "Stages/StartFrameStage.h"
#include "Stages/PresentStage.h"
#include "RenderOutput/RenderOutput.h"
#include "Stages/Stage.h"

#include <Engine/Engine.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Threading/FenceView.h>

#include <Common/System/Query.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>

namespace ngine::Rendering
{
	StartFrameStage::StartFrameStage(LogicalDevice& logicalDevice, RenderOutput& renderOutput)
		: StageBase(Threading::JobPriority::Draw)
		, m_logicalDevice(logicalDevice)
		, m_renderOutput(renderOutput)
	{
		if (renderOutput.SupportsAcquireImageSemaphore())
		{
			m_imageAcquiringSemaphores.GetView().InitializeAll(logicalDevice);
		}
	}

	StartFrameStage::~StartFrameStage()
	{
		for (Semaphore& semaphore : m_imageAcquiringSemaphores)
		{
			semaphore.Destroy(m_logicalDevice);
		}
	}

	Threading::Job::Result StartFrameStage::OnExecute(Threading::JobRunnerThread& currentThread)
	{
		Engine& engine = System::Get<Engine>();
		const uint8 frameIndex = engine.GetCurrentFrameIndex();

		const SemaphoreView imageAcquiringSemaphore = m_renderOutput.SupportsAcquireImageSemaphore() ? m_imageAcquiringSemaphores[frameIndex]
		                                                                                             : SemaphoreView{};

		constexpr uint64 timeout = 10;
		Optional<FrameImageId> frameImageId;
		do
		{
			frameImageId = m_renderOutput.AcquireNextImage(m_logicalDevice, timeout, imageAcquiringSemaphore, {});

			if (!frameImageId.IsValid())
			{
				currentThread.DoRunNextJob();
			}
		} while (!frameImageId.IsValid());

		m_frameImageId = *frameImageId;
		return Result::Finished;
	}

	SemaphoreView StartFrameStage::GetSubmissionFinishedSemaphore([[maybe_unused]] const Threading::Job& job) const
	{
		Engine& engine = System::Get<Engine>();
		const uint8 frameIndex = engine.GetCurrentFrameIndex();
		return m_imageAcquiringSemaphores[frameIndex];
	}

	bool StartFrameStage::IsSubmissionFinishedSemaphoreUsable() const
	{
		return m_renderOutput.SupportsAcquireImageSemaphore();
	}
}
