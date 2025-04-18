#pragma once

#include "PresentedCallback.h"

#include <Renderer/PipelineStageFlags.h>
#include <Renderer/Threading/SemaphoreView.h>
#include <Renderer/Threading/FenceView.h>
#include <Renderer/Commands/CommandQueueView.h>
#include <Renderer/Commands/EncodedCommandBufferView.h>
#include <Renderer/FrameImageId.h>
#include <Renderer/RenderOutput/SwapchainView.h>

#if RENDERER_VULKAN
#include <Renderer/Threading/Fence.h>
#endif

#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/Thread.h>
#include <Common/Threading/Mutexes/ConditionVariable.h>
#include <Common/Threading/Jobs/Job.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Function/Function.h>

namespace ngine::Rendering
{
	struct Renderer;
	struct LogicalDevice;

	using CommandBuffersSubmittedCallback = Function<void(), 64>;
	using CommandBuffersFinishedExecutionCallback = Function<void(), 64>;

	struct QueueSubmissionParameters
	{
		CommandBuffersSubmittedCallback m_submittedCallback;
		CommandBuffersFinishedExecutionCallback m_finishedCallback;
		ArrayView<const SemaphoreView, uint8> m_waitSemaphores;
		ArrayView<const EnumFlags<PipelineStageFlags>, uint8> m_waitStagesMasks;
		ArrayView<const SemaphoreView, uint8> m_signalSemaphores;
		FenceView m_fence;
	};

#define IMMEDIATE_SUBMISSION 0

	struct QueueSubmissionJob final : public Threading::Job
	{
		enum class Flags : uint8
		{
			IsProcessingDoubleBufferedData0 = 1 << 0,
			IsProcessingDoubleBufferedData1 = 1 << 1,
			IsPaused = 1 << 2,
#if WEBGPU_SINGLE_THREADED
			IsQueued = 1 << 3,
#endif
		};

		QueueSubmissionJob(LogicalDevice& logicalDevice, const CommandQueueView queue);
		QueueSubmissionJob(const QueueSubmissionJob&) = delete;
		QueueSubmissionJob& operator=(const QueueSubmissionJob&) = delete;
		QueueSubmissionJob(QueueSubmissionJob&&) = delete;
		QueueSubmissionJob& operator=(QueueSubmissionJob&&) = delete;
		~QueueSubmissionJob();

		void Queue(
			const Threading::JobPriority priority,
			const ArrayView<const EncodedCommandBufferView, uint16> encodedCommandBuffers,
			QueueSubmissionParameters&& parameters = QueueSubmissionParameters()
		);
		void QueueAwaitFence(const FenceView fence, CommandBuffersFinishedExecutionCallback&& callback);

		void QueuePresent(
			const Threading::JobPriority priority,
			const SwapchainView swapchain,
			const FrameImageId imageIndex,
			const ArrayView<const SemaphoreView, uint8> waitSemaphores,
			PresentedCallback&& finishedCallback =
				[]()
			{
			}
		);

		using Callback = Function<void(), 24>;
		void QueueCallback(Callback&&);

		void Tick();
		void TickDevice();

		[[nodiscard]] CommandQueueView GetQueue() const
		{
			return m_queue;
		}

		void Pause();
		void Resume();

#if RENDERER_VULKAN
		struct AwaitFenceThread final : public Threading::ThreadWithRunMember<AwaitFenceThread>
		{
			enum class Flags : uint8
			{
				IsRunning = 1 << 0,
				IsIdle = 1 << 1,
				IsQuitting = 1 << 2,
				Default = IsRunning | IsIdle
			};

			AwaitFenceThread(QueueSubmissionJob& job)
				: m_job(job)
			{
			}

			void Run();

			QueueSubmissionJob& m_job;

			AtomicEnumFlags<Flags> m_flags{Flags::Default};
			Threading::Mutex m_mutex;
			Threading::ConditionVariable m_conditionVariable;

			Fence m_ownedFence;
			InlineVector<FenceView, 1> m_fenceViews;
			LogicalDevice* m_pLogicalDevice = nullptr;
			CommandBuffersFinishedExecutionCallback m_callback;
		};
#endif
	protected:
		virtual Result OnExecute(Threading::JobRunnerThread& thread) override;

		[[nodiscard]] bool HasQueuedDoubleBufferedData() const;
		[[nodiscard]] bool HasWork() const
		{
			return HasQueuedDoubleBufferedData();
		}

		struct QueuedSubmit final : public QueueSubmissionParameters
		{
			QueuedSubmit(QueueSubmissionParameters&& parameters, const ArrayView<const EncodedCommandBufferView, uint16> encodedCommandBuffers)
				: QueueSubmissionParameters(Forward<QueueSubmissionParameters>(parameters))
				, m_encodedCommandBuffers(encodedCommandBuffers)
			{
			}

			FlatVector<EncodedCommandBufferView, 32> m_encodedCommandBuffers;
		};

		struct QueuedPresent
		{
			SwapchainView m_swapchain;
			FrameImageId m_imageIndex;
			ArrayView<const SemaphoreView, uint8> m_waitSemaphores;
			PresentedCallback m_finishedCallback;
		};

		struct DoubleBufferedData
		{
			using QueuedSubmitContainer = Vector<QueuedSubmit>;
			using QueuedPresentContainer = Vector<QueuedPresent>;
			using QueuedCallbackContainer = Vector<Callback>;

			[[nodiscard]] bool HasWorkQueued() const
			{
				return m_queue.HasElements() || m_presentQueue.HasElements() || m_callbackQueue.HasElements();
			}

			mutable Threading::SharedMutex m_mutex;
			QueuedSubmitContainer m_queue;
			QueuedPresentContainer m_presentQueue;
			QueuedCallbackContainer m_callbackQueue;

			Threading::JobPriority m_priority = Threading::JobPriority::LastBackground;
		};

		void Submit(QueuedSubmit&& queuedSubmit);
		void Present(QueuedPresent&& queuedPresent);
		void ProcessQueuedDoubleBufferedData(DoubleBufferedData& data);
		void TryQueueForExecution();
	protected:
		LogicalDevice& m_logicalDevice;
		CommandQueueView m_queue;
		AtomicEnumFlags<Flags> m_flags;

		Array<DoubleBufferedData, 2> m_doubleBufferedData;
		Threading::JobRunnerThread* m_pRunner = nullptr;

#if RENDERER_VULKAN
		Threading::Mutex m_availableFencesMutex;
		Vector<Fence> m_availableFences;
		Threading::SharedMutex m_idleAwaitFenceThreadMutex;
		Vector<UniquePtr<AwaitFenceThread>> m_idleAwaitFenceThreads;
#endif

#if IMMEDIATE_SUBMISSION
		Threading::Mutex m_immediateSubmissionMutex;
		Optional<Threading::JobRunnerThread*> m_pLockingThread;
#endif
	};

	ENUM_FLAG_OPERATORS(QueueSubmissionJob::Flags);
#if RENDERER_VULKAN
	ENUM_FLAG_OPERATORS(QueueSubmissionJob::AwaitFenceThread::Flags);
#endif
}
