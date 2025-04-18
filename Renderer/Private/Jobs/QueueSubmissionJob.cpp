#include "Jobs/QueueSubmissionJob.h"

#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>

#include <Renderer/Commands/CommandBuffer.h>
#include <Renderer/Commands/CommandEncoder.h>
#include <Renderer/Commands/EncodedCommandBuffer.h>

#include <Renderer/Devices/LogicalDeviceView.h>
#include <Renderer/Devices/LogicalDevice.h>

#include <Renderer/Window/Window.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>
#include <Renderer/WebGPU/Includes.h>

#include <Common/System/Query.h>

#include <Common/Assert/Assert.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <Engine/Threading/JobManager.h>

#if PLATFORM_APPLE
#import <QuartzCore/CAMetalLayer.h>
#endif

namespace ngine::Rendering
{
	// TODO: Implement dynamic submit queue priority
	// Needs extra logic in Job queuing to reprioritize in queue when changed
#define DYNAMIC_SUBMIT_QUEUE_PRIORITY 0

	QueueSubmissionJob::QueueSubmissionJob(LogicalDevice& logicalDevice, const CommandQueueView queue)
		: Threading::Job(DYNAMIC_SUBMIT_QUEUE_PRIORITY ? Threading::JobPriority::LastBackground : Threading::JobPriority::Submit)
		, m_logicalDevice(logicalDevice)
		, m_queue(queue)
	{
		// TODO: See if we can detect when it is safe to switch runners
		// Could be done by adding a fence to submissions and if in progress fences = 0 reset the submission thread.
		if (Optional<Threading::JobManager*> pJobManager = System::Find<Threading::JobManager>())
		{
			const Threading::JobRunnerMask highPriorityRunnerMask = pJobManager->GetPerformanceHighPriorityThreadMask();
			m_pRunner = &*pJobManager->GetJobThreads()[(uint8)*Memory::GetLastSetIndex(highPriorityRunnerMask)];
			SetExclusiveToThread(*m_pRunner);

#if RENDERER_WEBGPU && (!WEBGPU_SINGLE_THREADED || !PLATFORM_WEB)
			QueueExclusiveFromAnyThread(*m_pRunner);
#endif
		}

		// TODO: Preallocate some fences
	}

	QueueSubmissionJob::~QueueSubmissionJob()
	{
#if RENDERER_VULKAN
		Threading::UniqueLock lock(m_idleAwaitFenceThreadMutex);
		for (UniquePtr<AwaitFenceThread>& pThread : m_idleAwaitFenceThreads)
		{
			pThread->m_flags |= AwaitFenceThread::Flags::IsQuitting;
			{
				Threading::UniqueLock threadLock(pThread->m_mutex);
				if (pThread->m_fenceViews.HasElements())
				{
					Fence::Reset(*pThread->m_pLogicalDevice, pThread->m_fenceViews.GetView());
				}
			}
			pThread->m_conditionVariable.NotifyAll();
		}

		while (m_idleAwaitFenceThreads.GetView().Any(
			[](const UniquePtr<AwaitFenceThread>& pThread)
			{
				return pThread->m_flags.IsSet(AwaitFenceThread::Flags::IsRunning);
			}
		))
			;

		for (Fence& fence : m_availableFences)
		{
			fence.Destroy(m_logicalDevice);
		}
#endif
	}

#if RENDERER_VULKAN
	void QueueSubmissionJob::AwaitFenceThread::Run()
	{
		while (!m_flags.IsSet(Flags::IsQuitting))
		{
			{
				Threading::UniqueLock lock(m_mutex);
				if (m_fenceViews.IsEmpty())
				{
					m_conditionVariable.Wait(lock);
				}
			}

			if (m_fenceViews.HasElements())
			{
				Assert(!m_ownedFence.IsValid() || m_fenceViews.Contains(m_ownedFence));
				Fence::WaitResult waitResult;
				do
				{
					waitResult = Fence::Wait(*m_pLogicalDevice, m_fenceViews.GetView(), Math::NumericLimits<uint64>::Max);
				}
				while (waitResult == Fence::WaitResult::Timeout);
				Assert(waitResult == Fence::WaitResult::Success, "Failed fence wait");
				if (m_ownedFence.IsValid())
				{
					Threading::UniqueLock lock(m_job.m_availableFencesMutex);
					m_ownedFence.Reset(*m_pLogicalDevice);
					Assert(m_ownedFence.GetStatus(*m_pLogicalDevice) == FenceView::Status::Unsignaled);
					m_job.m_availableFences.EmplaceBack(Move(m_ownedFence));
				}
				m_fenceViews.Clear();
				CommandBuffersFinishedExecutionCallback finishedCallback = Move(m_callback);
				m_flags |= Flags::IsIdle;

				finishedCallback();
			}
		}

		m_flags &= ~Flags::IsRunning;
	}
#endif

	bool QueueSubmissionJob::HasQueuedDoubleBufferedData() const
	{
		for (uint8 i = 0; i < 2; ++i)
		{
			Threading::SharedLock lock(m_doubleBufferedData[i].m_mutex);
			if (m_doubleBufferedData[i].HasWorkQueued())
			{
				return true;
			}
		}

		return false;
	}

	void QueueSubmissionJob::Tick()
	{
		Assert(
			!Threading::JobRunnerThread::GetCurrent().IsValid() ||
			(GetAllowedJobRunnerMask() & (1ull << Threading::JobRunnerThread::GetCurrent()->GetThreadIndex())) != 0 || IMMEDIATE_SUBMISSION
		);
		if (HasQueuedDoubleBufferedData())
		{
			{
				m_flags |= Flags::IsProcessingDoubleBufferedData0;
				Threading::UniqueLock lock(m_doubleBufferedData[0].m_mutex);
				if (m_doubleBufferedData[0].HasWorkQueued())
				{
					ProcessQueuedDoubleBufferedData(m_doubleBufferedData[0]);

					if constexpr (DYNAMIC_SUBMIT_QUEUE_PRIORITY)
					{
						m_doubleBufferedData[0].m_priority = Threading::JobPriority::LastBackground;
						const Threading::JobPriority newPriority = Math::Min(m_doubleBufferedData[0].m_priority, m_doubleBufferedData[1].m_priority);
						SetPriority(newPriority);
					}
				}
				[[maybe_unused]] const bool wasCleared = m_flags.TryClearFlags(Flags::IsProcessingDoubleBufferedData0);
				Assert(wasCleared);
			}

			{
				m_flags |= Flags::IsProcessingDoubleBufferedData1;
				Threading::UniqueLock lock(m_doubleBufferedData[1].m_mutex);
				if (m_doubleBufferedData[1].HasWorkQueued())
				{
					ProcessQueuedDoubleBufferedData(m_doubleBufferedData[1]);

					if constexpr (DYNAMIC_SUBMIT_QUEUE_PRIORITY)
					{
						m_doubleBufferedData[1].m_priority = Threading::JobPriority::LastBackground;
						const Threading::JobPriority newPriority = Math::Min(m_doubleBufferedData[0].m_priority, m_doubleBufferedData[1].m_priority);
						SetPriority(newPriority);
					}
				}
				[[maybe_unused]] const bool wasCleared = m_flags.TryClearFlags(Flags::IsProcessingDoubleBufferedData1);
				Assert(wasCleared);
			}
		}

		TickDevice();
	}

	void QueueSubmissionJob::TickDevice()
	{
#if RENDERER_WEBGPU && !PLATFORM_WEB
#if RENDERER_WEBGPU_WGPU_NATIVE
		wgpuDevicePoll(m_logicalDevice, false, nullptr);
#else
		wgpuDeviceTick(m_logicalDevice);
#endif
#endif
	}

	Threading::Job::Result QueueSubmissionJob::OnExecute(Threading::JobRunnerThread& thread)
	{
		Assert(!RENDERER_WEBGPU || !WEBGPU_SINGLE_THREADED || !PLATFORM_WEB, "All submission work must execute in the render worker for emscripten");

#if IMMEDIATE_SUBMISSION
		Threading::UniqueLock lock(m_immediateSubmissionMutex);
		m_pLockingThread = &thread;
#endif

		if (!m_flags.IsSet(Flags::IsPaused))
		{
			Tick();

			if (HasWork())
			{
				SetExclusiveToThread(thread);
				return Result::TryRequeue;
			}
		}

#if IMMEDIATE_SUBMISSION
		m_pLockingThread = {};
#endif

#if RENDERER_WEBGPU
		return Result::TryRequeue;
#else
		return Result::Finished;
#endif
	}

	void QueueSubmissionJob::ProcessQueuedDoubleBufferedData(DoubleBufferedData& data)
	{
		for (Callback& queuedCallback : data.m_callbackQueue)
		{
			queuedCallback();
		}
		data.m_callbackQueue.Clear();

#if RENDERER_WEBGPU && WEBGPU_SINGLE_THREADED
		for (auto it = data.m_queue.begin(), endIt = data.m_queue.end(); it != endIt;)
		{
			QueuedSubmit& queuedSubmit = *it;
			if (queuedSubmit.m_encodedCommandBuffers.GetView().All(
						[](const EncodedCommandBufferView encodedCommandBuffer)
						{
							return (WGPUCommandBuffer)encodedCommandBuffer != nullptr;
						}
					))
			{
				Submit(Move(queuedSubmit));
				data.m_queue.Remove(it);
				endIt--;
			}
			else
			{
				++it;
			}
		}
#else
		for (QueuedSubmit& queuedSubmit : data.m_queue)
		{
			Submit(Move(queuedSubmit));
		}
		data.m_queue.Clear();
#endif

		for (QueuedPresent& queuedPresent : data.m_presentQueue)
		{
			Present(Move(queuedPresent));
		}
		data.m_presentQueue.Clear();
	}

#if RENDERER_WEBGPU
	struct CommandQueueSubmissionData
	{
		CommandQueueView commandQueue;
		FenceView fence;
		CommandBuffersSubmittedCallback submittedCallback;
		CommandBuffersFinishedExecutionCallback finishedCallback;
		Threading::Atomic<uint16> remainingDependencyCount{0};
		InlineVector<SemaphoreView, 8> signalSemaphores;
		InlineVector<WGPUCommandBuffer, 8> pendingCommandBuffers;

		void Submit()
		{
			Assert(pendingCommandBuffers.HasElements());
			wgpuQueueSubmit(commandQueue, pendingCommandBuffers.GetSize(), pendingCommandBuffers.GetData());
			
			const auto callback = [](const WGPUQueueWorkDoneStatus status, void* pUserData)
			{
				CommandQueueSubmissionData* pSubmissionData = static_cast<CommandQueueSubmissionData*>(pUserData);
				switch (status)
				{
					case WGPUQueueWorkDoneStatus_Success:
						break;
					case WGPUQueueWorkDoneStatus_Error:
#if RENDERER_WEBGPU_DAWN
					case WGPUQueueWorkDoneStatus_InstanceDropped:
#endif
					case WGPUQueueWorkDoneStatus_Unknown:
					case WGPUQueueWorkDoneStatus_DeviceLost:
						Assert(false, "Encountered submission error");
						break;
					case WGPUQueueWorkDoneStatus_Force32:
						ExpectUnreachable();
				}

				if (pSubmissionData->fence.IsValid())
				{
					pSubmissionData->fence.Signal();
				}

				if (pSubmissionData->finishedCallback.IsValid())
				{
#if WEBGPU_SINGLE_THREADED
					System::Get<Threading::JobManager>().QueueCallback(
						[callback = Move(pSubmissionData->finishedCallback)](Threading::JobRunnerThread&)
						{
							callback();
						},
						Threading::JobPriority::FinishSubmitCallback
					);
#else
					pSubmissionData->finishedCallback();
#endif
				}

				delete pSubmissionData;
			};

			wgpuQueueOnSubmittedWorkDone(
				commandQueue,
				callback,
				this
			);

			if (submittedCallback.IsValid())
			{
#if WEBGPU_SINGLE_THREADED
				System::Get<Threading::JobManager>().QueueCallback(
					[callback = Move(submittedCallback)](Threading::JobRunnerThread&)
					{
						callback();
					},
					Threading::JobPriority::Submit
				);
#else
				submittedCallback();
#endif
			}

			// Signal semaphores immediately
			// The WebGPU timeline guarantees they'll execute in order of submission, no need to wait for wgpuQueueOnSubmittedWorkDone
			for (SemaphoreView semaphore : signalSemaphores)
			{
				semaphore.Signal();
				for (CommandQueueSubmissionData& pendingSubmissionData : semaphore.GetPendingSubmissionData())
				{
					pendingSubmissionData.OnDependencyFinishedExecution();
				}
			}
		}

		void OnDependencyFinishedExecution()
		{
			const uint16 previousRemainingDependencyCount = remainingDependencyCount.FetchSubtract(1);
			Assert(previousRemainingDependencyCount > 0);
			if (previousRemainingDependencyCount == 1)
			{
				Submit();
			}
		}
	};
#endif

	void QueueSubmissionJob::Submit(QueuedSubmit&& queuedSubmit)
	{
		const Array<const CommandQueueView::SubmitInfo, 1, uint16, uint16> submits{CommandQueueView::SubmitInfo{
			queuedSubmit.m_waitSemaphores,
			queuedSubmit.m_waitStagesMasks,
			queuedSubmit.m_encodedCommandBuffers.GetView(),
			queuedSubmit.m_signalSemaphores
		}};

#if RENDERER_METAL
		struct SubmissionData
		{
			SubmissionData(const uint32 totalWorkCounter, CommandBuffersFinishedExecutionCallback&& finishedCallback, const FenceView fence)
				: m_remainingWorkCounter(totalWorkCounter)
				, m_finishedCallback(Forward<CommandBuffersFinishedExecutionCallback>(finishedCallback))
				, m_fence(fence)
			{
			}
			SubmissionData(const SubmissionData&) = delete;
			SubmissionData(SubmissionData&&) = delete;
			SubmissionData& operator=(const SubmissionData&) = delete;
			SubmissionData& operator=(SubmissionData&&) = delete;

			Threading::Atomic<uint32> m_remainingWorkCounter{0};
			CommandBuffersFinishedExecutionCallback m_finishedCallback;
			FenceView m_fence;
		};
#endif

#if RENDERER_VULKAN
		FenceView fenceView;
		Fence fence;
		if (queuedSubmit.m_fence.IsValid())
		{
			fenceView = queuedSubmit.m_fence;
		}
		else if (queuedSubmit.m_finishedCallback.IsValid())
		{
			{
				Threading::UniqueLock lock(m_availableFencesMutex);
				fence = m_availableFences.HasElements() ? m_availableFences.PopAndGetBack() : Fence(m_logicalDevice, Fence::Status::Unsignaled);
			}
			fenceView = fence;
		}

		Assert(!fenceView.IsValid() || fenceView.GetStatus(m_logicalDevice) != FenceView::Status::DeviceLost);
		Assert(!fenceView.IsValid() || fenceView.GetStatus(m_logicalDevice) == FenceView::Status::Unsignaled);
		[[maybe_unused]] const VkResult result =
			vkQueueSubmit(m_queue, submits.GetSize(), reinterpret_cast<const VkSubmitInfo*>(submits.GetData()), fenceView);
		Assert(result == VK_SUCCESS);

		if (queuedSubmit.m_finishedCallback.IsValid())
		{
			[this, fenceView, fence = Move(fence), &queuedSubmit]() mutable
			{
				{
					Threading::SharedLock lock(m_idleAwaitFenceThreadMutex);
					for (UniquePtr<AwaitFenceThread>& pThread : m_idleAwaitFenceThreads)
					{
						if (pThread->m_flags.TryClearFlags(AwaitFenceThread::Flags::IsIdle))
						{
							{
								Threading::UniqueLock threadLock(pThread->m_mutex);
								pThread->m_callback = Move(queuedSubmit.m_finishedCallback);
								pThread->m_pLogicalDevice = &m_logicalDevice;
								pThread->m_fenceViews.EmplaceBack(fenceView);
								pThread->m_ownedFence = Move(fence);
							}

							pThread->m_conditionVariable.NotifyAll();
							return;
						}
					}
				}

				AwaitFenceThread* pThread;
				{
					Threading::UniqueLock lock(m_idleAwaitFenceThreadMutex);
					pThread = &*m_idleAwaitFenceThreads.EmplaceBack(UniquePtr<AwaitFenceThread>::Make(*this));
					Threading::UniqueLock threadLock(pThread->m_mutex);
					pThread->m_flags &= ~AwaitFenceThread::Flags::IsIdle;
					pThread->m_callback = Move(queuedSubmit.m_finishedCallback);
					pThread->m_pLogicalDevice = &m_logicalDevice;
					pThread->m_fenceViews.EmplaceBack(fenceView);
					pThread->m_ownedFence = Move(fence);
					pThread->Start(MAKE_NATIVE_LITERAL("Await Fences Thread"));
				}

				pThread->m_conditionVariable.NotifyAll();
			}();
		}

		if (queuedSubmit.m_submittedCallback.IsValid())
		{
			Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
			thread.QueueCallbackFromThread(

				Threading::JobPriority::SubmitCallback,
				[callback = Move(queuedSubmit.m_submittedCallback)](Threading::JobRunnerThread&)
				{
					callback();
				}
			);
		}
#elif RENDERER_METAL
		uint16 encodedCommandBufferCount = 0;

		for (const CommandQueueView::SubmitInfo& __restrict submitInfo : submits)
		{
			for (const SemaphoreView waitSemaphore : submitInfo.GetWaitSemaphores())
			{
				id<MTLEvent> event{waitSemaphore};
				id<MTLCommandBuffer> commandBuffer = submitInfo.GetEncodedCommandBuffers().GetLastElement();
				[commandBuffer encodeWaitForEvent:event value:1];
			}

			for (const SemaphoreView signalSemaphore : submitInfo.GetSignalSemaphores())
			{
				id<MTLEvent> event{signalSemaphore};
				id<MTLCommandBuffer> commandBuffer = submitInfo.GetEncodedCommandBuffers().GetLastElement();
				[commandBuffer encodeSignalEvent:event value:1];
			}

			encodedCommandBufferCount += submitInfo.GetEncodedCommandBuffers().GetSize();
		}

		if (queuedSubmit.m_finishedCallback.IsValid() || queuedSubmit.m_fence.IsValid())
		{
			Assert(!queuedSubmit.m_fence.IsValid() || queuedSubmit.m_fence.GetStatus(m_logicalDevice) == FenceView::Status::Unsignaled);
			SubmissionData* pSubmissionData =
				new SubmissionData{encodedCommandBufferCount, Move(queuedSubmit.m_finishedCallback), queuedSubmit.m_fence};

			if (queuedSubmit.m_fence.IsValid())
			{
				queuedSubmit.m_fence.OnBeforeSubmit();
			}

			for (const CommandQueueView::SubmitInfo& __restrict submitInfo : submits)
			{
				for (const EncodedCommandBufferView commandBuffer : submitInfo.GetEncodedCommandBuffers())
				{
					id<MTLCommandBuffer> pCommandBuffer = commandBuffer;

					[pCommandBuffer addCompletedHandler:^([[maybe_unused]] id<MTLCommandBuffer> _Nonnull completedCommandBuffer) {
						Assert([completedCommandBuffer error] == nil);

						const uint32 previousWorkCount = pSubmissionData->m_remainingWorkCounter.FetchSubtract(1);
						Assert(previousWorkCount > 0);
						if (previousWorkCount == 1)
						{
							if (pSubmissionData->m_fence.IsValid())
							{
								pSubmissionData->m_fence.Signal();
							}

							if (pSubmissionData->m_finishedCallback.IsValid())
							{
								System::Get<Threading::JobManager>().QueueCallback(
									[callback = Move(pSubmissionData->m_finishedCallback)](Threading::JobRunnerThread&)
									{
										callback();
									},
									Threading::JobPriority::Submit
								);
							}

							delete pSubmissionData;
						}
					}];
					[pCommandBuffer commit];
				}
			}
		}
		else
		{
			for (const CommandQueueView::SubmitInfo& __restrict submitInfo : submits)
			{
				for (const EncodedCommandBufferView commandBuffer : submitInfo.GetEncodedCommandBuffers())
				{
					id<MTLCommandBuffer> pCommandBuffer = commandBuffer;
					[pCommandBuffer commit];
				}
			}
		}

		if (queuedSubmit.m_submittedCallback.IsValid())
		{
			Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
			thread.QueueCallbackFromThread(
				Threading::JobPriority::SubmitCallback,
				[callback = Move(queuedSubmit.m_submittedCallback)](Threading::JobRunnerThread&)
				{
					callback();
				}
			);
		}
#elif RENDERER_WEBGPU
		const uint16 waitSemaphoreCount = submits.GetView().Count(
			[](const CommandQueueView::SubmitInfo& __restrict submitInfo) -> uint16
			{
				return submitInfo.GetWaitSemaphores().GetSize();
			}
		);
		const uint16 signalSemaphoreCount = submits.GetView().Count(
			[](const CommandQueueView::SubmitInfo& __restrict submitInfo) -> uint16
			{
				return submitInfo.GetSignalSemaphores().GetSize();
			}
		);

		const uint16 encodedCommandBufferCount = submits.GetView().Count(
			[](const CommandQueueView::SubmitInfo& __restrict submitInfo)
			{
				return submitInfo.GetEncodedCommandBuffers().GetSize();
			}
		);
		InlineVector<WGPUCommandBuffer, 8> commandBuffers(Memory::Reserve, encodedCommandBufferCount);
		for (const CommandQueueView::SubmitInfo& __restrict submitInfo : submits)
		{
			for (const EncodedCommandBufferView encodedCommandBuffer : submitInfo.GetEncodedCommandBuffers())
			{
				commandBuffers.EmplaceBack(encodedCommandBuffer);
			}
		}

		const bool requiresSynchronization = queuedSubmit.m_finishedCallback.IsValid() || queuedSubmit.m_fence.IsValid() ||
		                                     waitSemaphoreCount > 0 || signalSemaphoreCount > 0;
		if (requiresSynchronization)
		{

			Assert(!queuedSubmit.m_fence.IsValid() || queuedSubmit.m_fence.GetStatus(m_logicalDevice) == FenceView::Status::Unsignaled);

			CommandQueueSubmissionData* __restrict pSubmissionData = new CommandQueueSubmissionData{
				m_queue,
				queuedSubmit.m_fence,
				Move(queuedSubmit.m_submittedCallback),
				Move(queuedSubmit.m_finishedCallback),
				waitSemaphoreCount,
				InlineVector<SemaphoreView, 8>(Memory::Reserve, signalSemaphoreCount),
				Move(commandBuffers)
			};

			for (const CommandQueueView::SubmitInfo& __restrict submitInfo : submits)
			{
				for (SemaphoreView signalSemaphore : submitInfo.GetSignalSemaphores())
				{
					pSubmissionData->signalSemaphores.EmplaceBack(signalSemaphore);
					signalSemaphore.EncodeSignal();
				}
			}

			for (const CommandQueueView::SubmitInfo& __restrict submitInfo : submits)
			{
				for (SemaphoreView waitSemaphore : submitInfo.GetWaitSemaphores())
				{
					if (!waitSemaphore.EncodeWait(*pSubmissionData))
					{
						pSubmissionData->OnDependencyFinishedExecution();
					}
				}
			}

			if (waitSemaphoreCount == 0)
			{
				pSubmissionData->Submit();
			}
		}
		else
		{
			for (const CommandQueueView::SubmitInfo& __restrict submitInfo : submits)
			{
				wgpuQueueSubmit(m_queue, submitInfo.GetEncodedCommandBuffers().GetSize(), commandBuffers.GetData());
			}

			if (queuedSubmit.m_submittedCallback.IsValid())
			{
				Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
				thread.QueueCallbackFromThread(
					Threading::JobPriority::SubmitCallback,
					[callback = Move(queuedSubmit.m_submittedCallback)](Threading::JobRunnerThread&)
					{
						callback();
					}
				);
			}
		}
#endif
	}

	void QueueSubmissionJob::Present(QueuedPresent&& queuedPresent)
	{
#if RENDERER_METAL
		Assert(queuedPresent.m_waitSemaphores.IsEmpty(), "Not supported");

		id<CAMetalDrawable> drawable = queuedPresent.m_swapchain;
		[drawable present];

		if (queuedPresent.m_finishedCallback.IsValid())
		{
			Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
			thread.QueueCallbackFromThread(
				Threading::JobPriority::SubmitCallback,
				[callback = Move(queuedPresent.m_finishedCallback)](Threading::JobRunnerThread&)
				{
					callback();
				}
			);
		}
#elif RENDERER_WEBGPU
		if (queuedPresent.m_waitSemaphores.HasElements())
		{
			QueueSubmissionParameters submissionParameters;
			submissionParameters.m_waitSemaphores = queuedPresent.m_waitSemaphores;

			Vector<EnumFlags<PipelineStageFlags>, uint8> waitStagesMasks(
				Memory::ConstructWithSize,
				Memory::InitializeAll,
				submissionParameters.m_waitSemaphores.GetSize(),
				PipelineStageFlags::ColorAttachmentOutput | PipelineStageFlags::ComputeShader
			);
			submissionParameters.m_waitStagesMasks = waitStagesMasks.GetView();

			const QueueFamily queueFamily = QueueFamily::Graphics;

			Threading::EngineJobRunnerThread& thread = static_cast<Threading::EngineJobRunnerThread&>(*m_pRunner);
			const CommandPoolView commandPool = thread.GetRenderData().GetCommandPool(m_logicalDevice.GetIdentifier(), queueFamily);
			const CommandQueueView commandQueue = m_logicalDevice.GetCommandQueue(queueFamily);

			CommandBuffer commandBuffer(m_logicalDevice, commandPool, commandQueue);
			CommandEncoder commandEncoder = commandBuffer.BeginEncoding(m_logicalDevice);
			const EncodedCommandBuffer encodedCommandBuffer = commandEncoder.StopEncoding();

			submissionParameters.m_submittedCallback = [this,
			                                            swapchain = queuedPresent.m_swapchain,
			                                            imageIndex = queuedPresent.m_imageIndex,
			                                            waitStagesMasks = Move(waitStagesMasks),
			                                            commandBuffer = Move(commandBuffer),
			                                            commandPool]() mutable
			{
#if PLATFORM_WEB
				[[maybe_unused]] const bool success = m_queue.Present({}, swapchain, imageIndex);
				commandBuffer.Destroy(m_logicalDevice, commandPool);
#else
				m_pRunner->QueueExclusiveCallbackFromAnyThread(
					Threading::JobPriority::Present,
					[this,
			        swapchain,
			        imageIndex,
			        waitStagesMasks = Move(waitStagesMasks),
			        commandBuffer = Move(commandBuffer),
			        commandPool](Threading::JobRunnerThread&) mutable
					{
						[[maybe_unused]] const bool success = m_queue.Present({}, swapchain, imageIndex);
						commandBuffer.Destroy(m_logicalDevice, commandPool);
					}
				);
#endif

			};
			if (queuedPresent.m_finishedCallback.IsValid())
			{
				submissionParameters.m_finishedCallback = [finishedCallback = Move(queuedPresent.m_finishedCallback)]() mutable
				{
					if (finishedCallback.IsValid())
					{
						finishedCallback();
					}
				};
			}
			Submit(QueuedSubmit{Move(submissionParameters), Array<const EncodedCommandBufferView, 1>{encodedCommandBuffer}});
		}
		else
		{
			[[maybe_unused]] const bool success =
				m_queue.Present(queuedPresent.m_waitSemaphores, queuedPresent.m_swapchain, queuedPresent.m_imageIndex);

			if (queuedPresent.m_finishedCallback.IsValid())
			{
				Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
				thread.QueueCallbackFromThread(
					Threading::JobPriority::SubmitCallback,
					[callback = Move(queuedPresent.m_finishedCallback)](Threading::JobRunnerThread&)
					{
						callback();
					}
				);
			}
		}
#else
		[[maybe_unused]] const bool success =
			m_queue.Present(queuedPresent.m_waitSemaphores, queuedPresent.m_swapchain, queuedPresent.m_imageIndex);

		if (queuedPresent.m_finishedCallback.IsValid())
		{
			Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
			thread.QueueCallbackFromThread(
				Threading::JobPriority::SubmitCallback,
				[callback = Move(queuedPresent.m_finishedCallback)](Threading::JobRunnerThread&)
				{
					callback();
				}
			);
		}
#endif
	}

	void QueueSubmissionJob::Queue(
		const Threading::JobPriority priority,
		const ArrayView<const EncodedCommandBufferView, uint16> encodedCommandBuffers,
		QueueSubmissionParameters&& parameters
	)
	{
		Assert(encodedCommandBuffers.All(
			[](const EncodedCommandBufferView encodedCommandBuffer)
			{
				return encodedCommandBuffer.IsValid();
			}
		));
		Assert(parameters.m_waitSemaphores.All(
			[](const SemaphoreView semaphore)
			{
				return semaphore.IsValid();
			}
		));
		Assert(parameters.m_signalSemaphores.All(
			[](const SemaphoreView semaphore)
			{
				return semaphore.IsValid();
			}
		));
		Assert(!parameters.m_fence.IsValid() || parameters.m_fence.GetStatus(m_logicalDevice) == FenceView::Status::Unsignaled);

#if IMMEDIATE_SUBMISSION
		UNUSED(priority);
		{
			Threading::UniqueLock lock(Threading::TryLock, m_immediateSubmissionMutex);
			if (lock.IsLocked())
			{
				m_pLockingThread = Threading::JobRunnerThread::GetCurrent();
			}
			else if (m_pLockingThread != Threading::JobRunnerThread::GetCurrent())
			{
				lock.Lock();
			}
			Submit(QueuedSubmit{Forward<QueueSubmissionParameters>(parameters), encodedCommandBuffers});
			m_pLockingThread = {};
		}
#else
		const uint8 submissionIndex = m_flags.IsSet(Flags::IsProcessingDoubleBufferedData0);
		{
			DoubleBufferedData& __restrict doubleBufferedData = m_doubleBufferedData[submissionIndex];
			Threading::UniqueLock lock(doubleBufferedData.m_mutex);

			doubleBufferedData.m_queue.EmplaceBack(Forward<QueueSubmissionParameters>(parameters), encodedCommandBuffers);

			if constexpr (DYNAMIC_SUBMIT_QUEUE_PRIORITY)
			{
				if (priority < doubleBufferedData.m_priority)
				{
					doubleBufferedData.m_priority = priority;

					const Threading::JobPriority newPriority = Math::Min(m_doubleBufferedData[0].m_priority, m_doubleBufferedData[1].m_priority);
					SetPriority(newPriority);
				}
			}
		}

		if (!m_flags.IsSet(Flags::IsPaused))
		{
			TryQueueForExecution();
		}
#endif
	}

	void QueueSubmissionJob::QueueAwaitFence(const FenceView fence, CommandBuffersFinishedExecutionCallback&& callback)
	{
		Assert(fence.IsValid());
		Assert(callback.IsValid());

#if RENDERER_VULKAN
		{
			Threading::SharedLock lock(m_idleAwaitFenceThreadMutex);
			for (UniquePtr<AwaitFenceThread>& pThread : m_idleAwaitFenceThreads)
			{
				if (pThread->m_flags.TryClearFlags(AwaitFenceThread::Flags::IsIdle))
				{
					{
						Threading::UniqueLock threadLock(pThread->m_mutex);
						pThread->m_callback = Forward<CommandBuffersFinishedExecutionCallback>(callback);
						pThread->m_pLogicalDevice = &m_logicalDevice;
						pThread->m_fenceViews.EmplaceBack(fence);
					}

					pThread->m_conditionVariable.NotifyAll();
					return;
				}
			}
		}

		AwaitFenceThread* pThread;
		{
			Threading::UniqueLock lock(m_idleAwaitFenceThreadMutex);
			pThread = &*m_idleAwaitFenceThreads.EmplaceBack(UniquePtr<AwaitFenceThread>::Make(*this));
			Threading::UniqueLock threadLock(pThread->m_mutex);
			pThread->m_flags &= ~AwaitFenceThread::Flags::IsIdle;
			pThread->m_callback = Forward<CommandBuffersFinishedExecutionCallback>(callback);
			pThread->m_pLogicalDevice = &m_logicalDevice;
			pThread->m_fenceViews.EmplaceBack(fence);
			pThread->Start(MAKE_NATIVE_LITERAL("Await Fences Thread"));
		}

		pThread->m_conditionVariable.NotifyAll();
#else
		const QueueFamily queueFamily = QueueFamily::Graphics;

		Threading::EngineJobRunnerThread& currentThread = *Threading::EngineJobRunnerThread::GetCurrent();
		const CommandPoolView commandPool = currentThread.GetRenderData().GetCommandPool(m_logicalDevice.GetIdentifier(), queueFamily);
		const CommandQueueView commandQueue = m_logicalDevice.GetCommandQueue(queueFamily);

		CommandBuffer commandBuffer(m_logicalDevice, commandPool, commandQueue);
		CommandEncoder commandEncoder = commandBuffer.BeginEncoding(m_logicalDevice);
		const EncodedCommandBuffer encodedCommandBuffer = commandEncoder.StopEncoding();

		QueueSubmissionParameters submissionParameters;
		Assert(!fence.IsValid() || fence.GetStatus(m_logicalDevice) == FenceView::Status::Unsignaled);
		submissionParameters.m_fence = fence;
		submissionParameters.m_finishedCallback = [this,
		                                           commandBuffer = Move(commandBuffer),
		                                           commandPool,
		                                           callback = Forward<CommandBuffersFinishedExecutionCallback>(callback)]() mutable
		{
			commandBuffer.Destroy(m_logicalDevice, commandPool);
			callback();
		};

		Queue(
			Threading::JobPriority::AwaitFrameFinish,
			ArrayView<const EncodedCommandBufferView>{encodedCommandBuffer},
			Move(submissionParameters)
		);
#endif
	}

	void QueueSubmissionJob::QueuePresent(
		const Threading::JobPriority priority,
		const SwapchainView swapchain,
		const FrameImageId imageIndex,
		const ArrayView<const SemaphoreView, uint8> waitSemaphores,
		PresentedCallback&& finishedCallback
	)
	{
#if IMMEDIATE_SUBMISSION
		{
			Threading::UniqueLock lock(Threading::TryLock, m_immediateSubmissionMutex);
			if (lock.IsLocked())
			{
				m_pLockingThread = Threading::JobRunnerThread::GetCurrent();
			}
			else if (m_pLockingThread != Threading::JobRunnerThread::GetCurrent())
			{
				lock.Lock();
			}
			UNUSED(priority);
			Present(QueuedPresent{swapchain, imageIndex, waitSemaphores, Move(finishedCallback)});
			m_pLockingThread = {};
		}
#else
		const uint8 submissionIndex = m_flags.IsSet(Flags::IsProcessingDoubleBufferedData0);
		{
			Threading::UniqueLock lock(m_doubleBufferedData[submissionIndex].m_mutex);

			DoubleBufferedData& doubleBufferedData = m_doubleBufferedData[submissionIndex];

			DoubleBufferedData::QueuedPresentContainer& queuedPresents = doubleBufferedData.m_presentQueue;
			queuedPresents.EmplaceBack(QueuedPresent{swapchain, imageIndex, waitSemaphores, Move(finishedCallback)});

			if constexpr (DYNAMIC_SUBMIT_QUEUE_PRIORITY)
			{
				if (priority < doubleBufferedData.m_priority)
				{
					doubleBufferedData.m_priority = priority;

					const Threading::JobPriority newPriority = Math::Min(m_doubleBufferedData[0].m_priority, m_doubleBufferedData[1].m_priority);
					SetPriority(newPriority);
				}
			}
		}

		TryQueueForExecution();
#endif
	}

	void QueueSubmissionJob::QueueCallback(Callback&& callback)
	{
#if RENDERER_WEBGPU && WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[this, callback = Forward<Callback>(callback)]()
			{
				callback();
				TryQueueForExecution();
			}
		);
#elif IMMEDIATE_SUBMISSION
		{
			Threading::UniqueLock lock(Threading::TryLock, m_immediateSubmissionMutex);
			if (lock.IsLocked())
			{
				m_pLockingThread = Threading::JobRunnerThread::GetCurrent();
			}
			else if (m_pLockingThread != Threading::JobRunnerThread::GetCurrent())
			{
				lock.Lock();
			}
			callback();
			m_pLockingThread = {};
		}
#else

		const uint8 submissionIndex = m_flags.IsSet(Flags::IsProcessingDoubleBufferedData0);
		{
			Threading::UniqueLock lock(m_doubleBufferedData[submissionIndex].m_mutex);

			DoubleBufferedData& doubleBufferedData = m_doubleBufferedData[submissionIndex];

			DoubleBufferedData::QueuedCallbackContainer& queuedCallbacks = doubleBufferedData.m_callbackQueue;
			queuedCallbacks.EmplaceBack(Forward<Callback>(callback));
		}

		TryQueueForExecution();
#endif
	}

	void QueueSubmissionJob::TryQueueForExecution()
	{
#if PLATFORM_WEB && WEBGPU_SINGLE_THREADED
		if (m_flags.TrySetFlags(Flags::IsQueued))
		{
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[this]()
				{
					m_flags.Clear(Flags::IsQueued);
					Tick();

					if (HasWork())
					{
						TryQueueForExecution();
					}
				}
			);
		}
#else
		Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent();
		if (pThread.IsValid() && (GetAllowedJobRunnerMask() & (1ull << pThread->GetThreadIndex())) != 0)
		{
			TryQueue(*pThread);
		}
		else
		{
			TryQueue(System::Get<Threading::JobManager>());
		}
#endif
	}

	void QueueSubmissionJob::Pause()
	{
		m_flags |= Flags::IsPaused;
	}

	void QueueSubmissionJob::Resume()
	{
		const bool wasResumed = m_flags.TryClearFlags(Flags::IsPaused);
		if (wasResumed && HasWork() && !IsQueuedOrExecuting())
		{
			if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
			{
				TryQueue(*Threading::JobRunnerThread::GetCurrent());
			}
			else
			{
				TryQueue(System::Get<Threading::JobManager>());
			}
		}
	}
}
