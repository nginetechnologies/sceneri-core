#include "Threading/JobRunnerData.h"

#include <Engine/Engine.h>
#include <Engine/Threading/JobRunnerThread.h>

#include <Renderer/Renderer.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Wrappers/Image.h>
#include <Renderer/Wrappers/AccelerationStructure.h>
#include <Renderer/Buffers/Buffer.h>
#include <Renderer/Buffers/Buffer.h>
#include <Renderer/Threading/Semaphore.h>
#include <Renderer/Threading/Fence.h>
#include <Renderer/Window/Window.h>

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/System/Query.h>
#include <Common/Memory/OffsetOf.h>

namespace ngine::Rendering
{
	void JobRunnerData::PerFrameData::AtomicState::StartCpuWork()
	{
		uint64 previousValue = m_value.Load();
		State state;
		do
		{
			state.m_value = previousValue;
			Assert(state.m_data.m_stateFlags.IsSet(FrameStateFlags::AwaitingFrameFinish) || state.m_data.m_stateFlags.AreNoneSet());

			Assert(state.m_data.m_stateFlags.IsNotSet(FrameStateFlags::AwaitingCpuFinish) || state.m_data.m_ongoingCpuWorkCounter > 0);
			Assert(state.m_data.m_stateFlags.IsSet(FrameStateFlags::AwaitingCpuFinish) || state.m_data.m_ongoingCpuWorkCounter == 0);

			Assert(
				state.m_data.m_stateFlags.IsNotSet(FrameStateFlags::AwaitingCpuFinish) ||
				state.m_data.m_stateFlags.IsSet(FrameStateFlags::AwaitingCpuFinish)
			);

			if (state.m_data.m_stateFlags.AreNoneSet())
			{
				Assert(state.m_data.m_ongoingGpuWorkCounter == 0);
				// First work of a frame
				state.m_data.m_stateFlags = FrameStateFlags::AwaitingFrameFinish;
			}

			state.m_data.m_stateFlags |= FrameStateFlags::AwaitingCpuFinish;

			state.m_data.m_ongoingCpuWorkCounter++;
		} while (!m_value.CompareExchangeWeak(previousValue, state.m_value));
	}

	bool JobRunnerData::PerFrameData::AtomicState::FinishCpuWork()
	{
		uint64 previousValue = m_value.Load();
		State state;
		bool result{false};
		do
		{
			result = false;

			state.m_value = previousValue;

			Assert(state.m_data.m_stateFlags.AreAnySet(), "Can't finish frame that hasn't started!");
			if (UNLIKELY(state.m_data.m_stateFlags.AreNoneSet()))
			{
				return false;
			}
			Assert(state.m_data.m_stateFlags.IsSet(FrameStateFlags::AwaitingCpuFinish), "Can't finish cpu frame twice!");
			if (UNLIKELY(state.m_data.m_stateFlags.IsNotSet(FrameStateFlags::AwaitingCpuFinish)))
			{
				return false;
			}

			const uint16 previousWorkCount = state.m_data.m_ongoingCpuWorkCounter;
			Assert(previousWorkCount > 0);
			state.m_data.m_ongoingCpuWorkCounter--;
			if (previousWorkCount == 1)
			{
				const EnumFlags<FrameStateFlags> removedFlags = FrameStateFlags::AwaitingCpuFinish;
				const EnumFlags<FrameStateFlags> previousFlags = state.m_data.m_stateFlags;
				state.m_data.m_stateFlags &= ~removedFlags;

				if ((previousFlags & ~removedFlags).AreNoneSet(FrameStateFlags::AwaitingCpuFinish | FrameStateFlags::AwaitingGpuFinish))
				{
					Assert(state.m_data.m_stateFlags.AreNoneSet(FrameStateFlags::AwaitingCpuFinish | FrameStateFlags::AwaitingGpuFinish));
					Assert(state.m_data.m_ongoingCpuWorkCounter == 0 && state.m_data.m_ongoingGpuWorkCounter == 0);

					result = true;
				}
			}
		} while (!m_value.CompareExchangeWeak(previousValue, state.m_value));

		return result;
	}

	void JobRunnerData::PerFrameData::AtomicState::StartGpuWork()
	{
		uint64 previousValue = m_value.Load();
		State state;
		do
		{
			state.m_value = previousValue;

			Assert(state.m_data.m_stateFlags.IsNotSet(FrameStateFlags::AwaitingGpuFinish) || state.m_data.m_ongoingGpuWorkCounter > 0);
			Assert(state.m_data.m_stateFlags.IsSet(FrameStateFlags::AwaitingGpuFinish) || state.m_data.m_ongoingGpuWorkCounter == 0);
			state.m_data.m_ongoingGpuWorkCounter++;

			state.m_data.m_stateFlags |= FrameStateFlags::AwaitingGpuFinish;

			Assert(state.m_data.m_stateFlags.AreAllSet(FrameStateFlags::AwaitingCpuFinish | FrameStateFlags::AwaitingFrameFinish));
		} while (!m_value.CompareExchangeWeak(previousValue, state.m_value));
	}

	bool JobRunnerData::PerFrameData::AtomicState::FinishGpuWork()
	{
		uint64 previousValue = m_value.Load();
		State state;
		bool result{false};
		do
		{
			result = false;

			state.m_value = previousValue;

			Assert(state.m_data.m_stateFlags.IsSet(FrameStateFlags::AwaitingGpuFinish), "Can't finish GPU work on frame that hasn't started!");
			if (UNLIKELY(state.m_data.m_stateFlags.IsNotSet(FrameStateFlags::AwaitingGpuFinish)))
			{
				return false;
			}
			Assert(state.m_data.m_stateFlags.IsSet(FrameStateFlags::AwaitingFrameFinish));
			if (UNLIKELY(state.m_data.m_stateFlags.IsNotSet(FrameStateFlags::AwaitingFrameFinish)))
			{
				return false;
			}

			const uint16 previousWorkCount = state.m_data.m_ongoingGpuWorkCounter;
			state.m_data.m_ongoingGpuWorkCounter--;
			Assert(previousWorkCount > 0);
			if (previousWorkCount == 1)
			{
				[[maybe_unused]] const EnumFlags<FrameStateFlags> previousFlags = state.m_data.m_stateFlags;
				state.m_data.m_stateFlags &= ~FrameStateFlags::AwaitingGpuFinish;
				Assert(state.m_data.m_ongoingGpuWorkCounter == 0);

				if (state.m_data.m_stateFlags.IsNotSet(FrameStateFlags::AwaitingCpuFinish))
				{
					Assert(state.m_data.m_stateFlags.AreNoneSet(FrameStateFlags::AwaitingCpuFinish | FrameStateFlags::AwaitingGpuFinish));

					result = true;
				}
			}
		} while (!m_value.CompareExchangeWeak(previousValue, state.m_value));

		return result;
	}

	bool JobRunnerData::PerFrameData::AtomicState::IsProcessingFrame() const
	{
		State state;
		state.m_value = m_value.Load();
		const bool isProcessingFrame =
			state.m_data.m_stateFlags.AreAnySet(FrameStateFlags::AwaitingCpuFinish | FrameStateFlags::AwaitingGpuFinish);
		Assert(isProcessingFrame || (state.m_data.m_ongoingGpuWorkCounter == 0u && state.m_data.m_ongoingCpuWorkCounter == 0u));

		return isProcessingFrame;
	}

	bool JobRunnerData::PerFrameData::AtomicState::IsProcessingFrameOrAwaitingReset() const
	{
		State state;
		state.m_value = m_value.Load();
		const bool isProcessingFrame = state.m_data.m_stateFlags.AreAnySet(
			FrameStateFlags::AwaitingCpuFinish | FrameStateFlags::AwaitingGpuFinish | FrameStateFlags::AwaitingFrameFinish
		);
		Assert(isProcessingFrame || (state.m_data.m_ongoingGpuWorkCounter == 0u && state.m_data.m_ongoingCpuWorkCounter == 0u));

		return isProcessingFrame;
	}

	void JobRunnerData::PerFrameData::AtomicState::FinishFrame()
	{
		uint64 previousValue = m_value.Load();
		State state;
		do
		{
			state.m_value = previousValue;
			Assert(state.m_data.m_stateFlags == FrameStateFlags::AwaitingFrameFinish);
			state.m_data.m_stateFlags = {};
		} while (!m_value.CompareExchangeWeak(previousValue, state.m_value));
	}

	PURE_LOCALS_AND_POINTERS Threading::EngineJobRunnerThread& JobRunnerData::GetRunnerThread()
	{
		const ptrdiff_t offsetFromOwner = Memory::GetOffsetOf(&Threading::EngineJobRunnerThread::m_renderData);

		return *reinterpret_cast<Threading::EngineJobRunnerThread*>(reinterpret_cast<ptrdiff_t>(this) - offsetFromOwner);
	}

	PURE_LOCALS_AND_POINTERS const Threading::EngineJobRunnerThread& JobRunnerData::GetRunnerThread() const
	{
		const ptrdiff_t offsetFromOwner = Memory::GetOffsetOf(&Threading::EngineJobRunnerThread::m_renderData);

		return *reinterpret_cast<const Threading::EngineJobRunnerThread*>(reinterpret_cast<ptrdiff_t>(this) - offsetFromOwner);
	}

	JobRunnerData::JobRunnerData()
	{
		System::Get<Engine>().OnRendererInitialized.Add(
			this,
			[](JobRunnerData& jobRunnerData)
			{
				Renderer& renderer = System::Get<Rendering::Renderer>();

				renderer.OnLogicalDeviceCreated.Add(
					jobRunnerData,
					[](JobRunnerData& jobRunnerData, const LogicalDevice& logicalDevice)
					{
						UniquePtr<LogicalDeviceData>& pLogicalDeviceData = jobRunnerData.m_logicalDeviceData[logicalDevice.GetIdentifier()];
						pLogicalDeviceData.CreateInPlace();
						LogicalDeviceData& logicalDeviceData = *pLogicalDeviceData;

						for (uint8 queueFamilyIndex = 0; queueFamilyIndex < (uint8)QueueFamily::Count; ++queueFamilyIndex)
						{
							const QueueFamilyIndex physicalDeviceQueueFamilyIndex =
								logicalDevice.GetPhysicalDevice().GetQueueFamily((QueueFamily)(1 << queueFamilyIndex));
							uint8 duplicatedQueueFamilyIndex = queueFamilyIndex;
							for (uint8 otherQueueFamilyIndex = 0; otherQueueFamilyIndex < queueFamilyIndex; ++otherQueueFamilyIndex)
							{
								const QueueFamilyIndex otherPhysicalDeviceQueueFamilyIndex =
									logicalDevice.GetPhysicalDevice().GetQueueFamily((QueueFamily)(1 << otherQueueFamilyIndex));
								if (physicalDeviceQueueFamilyIndex == otherPhysicalDeviceQueueFamilyIndex)
								{
									duplicatedQueueFamilyIndex = otherQueueFamilyIndex;
									break;
								}
							}
							if (duplicatedQueueFamilyIndex != queueFamilyIndex)
							{
								// We are a shared queue family, use the previously created pools
								logicalDeviceData.m_commandPoolViews[queueFamilyIndex] = logicalDeviceData.m_commandPools[duplicatedQueueFamilyIndex];

								for (uint8 frameIndex = 0; frameIndex < MaximumConcurrentFrameCount; frameIndex++)
								{
									PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];
									PerFramePoolData& perFramePool = perFrameData.m_commandPoolStorage[(QueueFamily)duplicatedQueueFamilyIndex];
									perFrameData.m_commandPools[(QueueFamily)queueFamilyIndex] = &perFramePool;
								}
							}
							else
							{
								logicalDeviceData.m_commandPools[queueFamilyIndex] = CommandPool(logicalDevice, physicalDeviceQueueFamilyIndex);
								logicalDeviceData.m_commandPoolViews[queueFamilyIndex] = logicalDeviceData.m_commandPools[queueFamilyIndex];

								for (uint8 frameIndex = 0; frameIndex < MaximumConcurrentFrameCount; frameIndex++)
								{
									PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];
									PerFramePoolData& perFramePool = perFrameData.m_commandPoolStorage[(QueueFamily)queueFamilyIndex];
									perFramePool.m_commandPool = CommandPool(logicalDevice, physicalDeviceQueueFamilyIndex);
									perFrameData.m_commandPools[(QueueFamily)queueFamilyIndex] = &perFramePool;
								}
							}
						}

						const EnumFlags<Rendering::PhysicalDeviceFeatures> physicalDeviceFeatures =
							logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
						const bool supportsDynamicTextureSampling = physicalDeviceFeatures.AreAllSet(
							Rendering::PhysicalDeviceFeatures::PartiallyBoundDescriptorBindings |
							Rendering::PhysicalDeviceFeatures::UpdateDescriptorSampleImageAfterBind |
							Rendering::PhysicalDeviceFeatures::NonUniformImageArrayIndexing | Rendering::PhysicalDeviceFeatures::RuntimeDescriptorArrays |
							Rendering::PhysicalDeviceFeatures::AccelerationStructure
						);

						enum class LimitType : uint8
						{
							LowPriority,
							HighPriority,
							Count
						};

						struct DescriptorTypeLimits
						{
							constexpr DescriptorTypeLimits(const uint32 lowPriorityLimit, const uint32 highPriorityLimit)
								: m_maximumCount{lowPriorityLimit, highPriorityLimit}
							{
							}

							[[nodiscard]] constexpr uint32 operator[](const LimitType limitType) const
							{
								return m_maximumCount[limitType];
							}

							Array<uint32, (uint8)LimitType::Count, LimitType> m_maximumCount;
						};

						const auto getDescriptorTypeLimits = [supportsDynamicTextureSampling](const DescriptorType descriptorType
				                                         ) -> DescriptorTypeLimits
						{
							switch (descriptorType)
							{
								case DescriptorType::Sampler:
									return DescriptorTypeLimits{8192, 8192};
								case DescriptorType::CombinedImageSampler:
									// TODO: Change texture max count over to SampledImage when we don't have a sampler per texture.
									return supportsDynamicTextureSampling
							             ? DescriptorTypeLimits{Rendering::TextureIdentifier::MaximumCount, Rendering::TextureIdentifier::MaximumCount}
							             : DescriptorTypeLimits{2, 2};
								case DescriptorType::SampledImage:
									return DescriptorTypeLimits{32768, 32768};
								case DescriptorType::StorageImage:
									return DescriptorTypeLimits{1024, 1024};
								case DescriptorType::UniformTexelBuffer:
								case DescriptorType::StorageTexelBuffer:
								case DescriptorType::UniformBuffer:
									return DescriptorTypeLimits{1024, 1024};
								case DescriptorType::StorageBuffer:
									return DescriptorTypeLimits{1024, 1024};
								case DescriptorType::UniformBufferDynamic:
									return DescriptorTypeLimits{1024, 1024};
								case DescriptorType::StorageBufferDynamic:
									return DescriptorTypeLimits{1024, 1024};
								case DescriptorType::InputAttachment:
									return DescriptorTypeLimits{1024, 1024};
								case DescriptorType::AccelerationStructure:
									return DescriptorTypeLimits{1024, 1024};
							}
							ExpectUnreachable();
						};

						Threading::JobRunnerThread& thread = jobRunnerData.GetRunnerThread();
						const LimitType limitType = thread.CanRunLowPriorityPerformanceJobs() ? LimitType::LowPriority : LimitType::HighPriority;

						FlatVector<DescriptorPool::Size, 10> descriptorPoolSizes{
							DescriptorPool::Size{
								DescriptorType::CombinedImageSampler,
								getDescriptorTypeLimits(DescriptorType::CombinedImageSampler)[limitType]
							},
							DescriptorPool::Size{DescriptorType::SampledImage, getDescriptorTypeLimits(DescriptorType::SampledImage)[limitType]},
							DescriptorPool::Size{DescriptorType::Sampler, getDescriptorTypeLimits(DescriptorType::Sampler)[limitType]},
							DescriptorPool::Size{DescriptorType::UniformBuffer, getDescriptorTypeLimits(DescriptorType::UniformBuffer)[limitType]},
							DescriptorPool::Size{
								DescriptorType::UniformBufferDynamic,
								getDescriptorTypeLimits(DescriptorType::UniformBufferDynamic)[limitType]
							},
							DescriptorPool::Size{
								DescriptorType::StorageBufferDynamic,
								getDescriptorTypeLimits(DescriptorType::StorageBufferDynamic)[limitType]
							},
							DescriptorPool::Size{DescriptorType::StorageBuffer, getDescriptorTypeLimits(DescriptorType::StorageBuffer)[limitType]},
							DescriptorPool::Size{DescriptorType::StorageImage, getDescriptorTypeLimits(DescriptorType::StorageImage)[limitType]},
							DescriptorPool::Size{DescriptorType::InputAttachment, getDescriptorTypeLimits(DescriptorType::InputAttachment)[limitType]},
						};
						if (physicalDeviceFeatures.IsSet(Rendering::PhysicalDeviceFeatures::AccelerationStructure))
						{
							descriptorPoolSizes.EmplaceBack(DescriptorPool::Size{
								DescriptorType::AccelerationStructure,
								getDescriptorTypeLimits(DescriptorType::AccelerationStructure)[limitType]
							});
						}

						const uint32 maximumDescriptorCount = limitType == LimitType::LowPriority ? 7168 : 2048;
						logicalDeviceData.m_descriptorPool = DescriptorPool(
							logicalDevice,
							maximumDescriptorCount,
							descriptorPoolSizes,
							DescriptorPool::CreationFlags::SupportIndividualFree | DescriptorPool::CreationFlags::UpdateAfterBind
						);
					}
				);
			}
		);
	}

	JobRunnerData::~JobRunnerData()
	{
	}

	void JobRunnerData::Destroy()
	{
		for (const Optional<LogicalDevice*> pLogicalDevice : System::Get<Rendering::Renderer>().GetLogicalDevices())
		{
			if (pLogicalDevice.IsValid())
			{
				LogicalDeviceData& logicalDeviceData = *m_logicalDeviceData[pLogicalDevice->GetIdentifier()];
				for (uint8 frameIndex = 0; frameIndex < MaximumConcurrentFrameCount; frameIndex++)
				{
					PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

					for (PerFramePoolData& perFramePool : perFrameData.m_commandPoolStorage)
					{
#if RENDERER_SUPPORTS_REUSABLE_COMMAND_BUFFERS
						for (CommandBuffer& commandBuffer : perFramePool.m_availableCommandBuffers)
						{
							commandBuffer.Destroy(*pLogicalDevice, perFramePool.m_commandPool);
						}

						for (CommandBuffer& commandBuffer : perFramePool.m_usedCommandBuffers)
						{
							commandBuffer.Destroy(*pLogicalDevice, perFramePool.m_commandPool);
						}
#endif

						perFramePool.m_commandPool.Destroy(*pLogicalDevice);
					}
				}

				for (CommandPool& commandPool : logicalDeviceData.m_commandPools)
				{
					commandPool.Destroy(*pLogicalDevice);
				}

				logicalDeviceData.m_descriptorPool.Destroy(*pLogicalDevice);
			}
		}
	}

	void JobRunnerData::OnStartFrameCpuWork(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex)
	{
		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		perFrameData.m_state.StartCpuWork();
	}

	void JobRunnerData::OnFinishFrameCpuWork(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex)
	{
		const Optional<LogicalDeviceData*> pLogicalDeviceData = m_logicalDeviceData[logicalDeviceIdentifier].Get();
		if (LIKELY(pLogicalDeviceData.IsValid()))
		{
			PerFrameData& __restrict perFrameData = pLogicalDeviceData->m_perFrameData[frameIndex];

			if (perFrameData.m_state.FinishCpuWork())
			{
				QueueFinishFrameWorkInternal(logicalDeviceIdentifier, frameIndex);
			}
		}
	}

	void JobRunnerData::OnStartFrameGpuWork(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex)
	{
		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];
		perFrameData.m_state.StartGpuWork();
	}

	void JobRunnerData::OnFinishFrameGpuWork(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex)
	{
		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		if (perFrameData.m_state.FinishGpuWork())
		{
			QueueFinishFrameWorkInternal(logicalDeviceIdentifier, frameIndex);
		}
	}

	CommandBufferView JobRunnerData::GetPerFrameCommandBuffer(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, const QueueFamily queueFamily, const uint8 frameIndex
	)
	{
		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];
		perFrameData.m_state.StartGpuWork();

		PerFramePoolData& __restrict perFramePool = *perFrameData.m_commandPools[queueFamily >> 1];

#if RENDERER_SUPPORTS_REUSABLE_COMMAND_BUFFERS
		if (perFramePool.m_availableCommandBuffers.HasElements())
		{
			perFramePool.m_usedCommandBuffers.EmplaceBack(Move(perFramePool.m_availableCommandBuffers.GetLastElement()));
			perFramePool.m_availableCommandBuffers.Remove(perFramePool.m_availableCommandBuffers.end() - 1);
			return perFramePool.m_usedCommandBuffers.GetLastElement();
		}
#endif

		CommandPoolView perFrameCommandPool = perFramePool.m_commandPool;
		const LogicalDevice& logicalDevice = *System::Get<Rendering::Renderer>().GetLogicalDevice(logicalDeviceIdentifier);

		CommandBuffer commandBuffer(logicalDevice, perFrameCommandPool, logicalDevice.GetCommandQueue(queueFamily));
#if RENDERER_SUPPORTS_REUSABLE_COMMAND_BUFFERS
		perFramePool.m_usedCommandBuffers.EmplaceBack(Move(commandBuffer));
		return perFramePool.m_usedCommandBuffers.GetLastElement();
#else
		const CommandBufferView commandBufferView{commandBuffer};
		commandBuffer.OnBufferFreed();
		return commandBufferView;
#endif
	}

	void
	JobRunnerData::OnPerFrameCommandBufferFinishedExecution(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex)
	{
		OnFinishFrameGpuWork(logicalDeviceIdentifier, frameIndex);
	}

	void JobRunnerData::AwaitFrameFinish(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex)
	{
		const Optional<LogicalDeviceData*> pLogicalDeviceData = m_logicalDeviceData[logicalDeviceIdentifier].Get();
		if (pLogicalDeviceData.IsValid())
		{
			PerFrameData& __restrict perFrameData = pLogicalDeviceData->m_perFrameData[frameIndex];

			if (const Optional<Threading::JobRunnerThread*> pCurrentThread = Threading::JobRunnerThread::GetCurrent())
			{
				while (perFrameData.m_state.IsProcessingFrameOrAwaitingReset())
				{
					pCurrentThread->DoRunNextJob();
					// TODO: condition variable here?
				}
			}
			else
			{
				while (perFrameData.m_state.IsProcessingFrameOrAwaitingReset())
					;
			}
		}
	}

	void JobRunnerData::AwaitFrameFinish(const uint8 frameIndex)
	{
		Renderer& renderer = System::Get<Renderer>();
		for (const Optional<LogicalDevice*> pLogicalDevice : renderer.GetLogicalDevices())
		{
			if (pLogicalDevice.IsValid())
			{
				AwaitFrameFinish(pLogicalDevice->GetIdentifier(), frameIndex);
			}
		}
	}

	void JobRunnerData::AwaitAnyFramesFinish(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameMask)
	{
		const Optional<LogicalDeviceData*> pLogicalDeviceData = m_logicalDeviceData[logicalDeviceIdentifier].Get();
		if (pLogicalDeviceData.IsValid())
		{
			if (const Optional<Threading::JobRunnerThread*> pCurrentThread = Threading::JobRunnerThread::GetCurrent())
			{
				for (const uint8 frameIndex : Memory::GetSetBitsIterator(frameMask))
				{
					PerFrameData& __restrict perFrameData = pLogicalDeviceData->m_perFrameData[frameIndex];
					while (perFrameData.m_state.IsProcessingFrameOrAwaitingReset())
					{
						pCurrentThread->DoRunNextJob();
						// TODO: condition variable here?
					}
				}
			}
			else
			{
				for (const uint8 frameIndex : Memory::GetSetBitsIterator(frameMask))
				{
					PerFrameData& __restrict perFrameData = pLogicalDeviceData->m_perFrameData[frameIndex];
					while (perFrameData.m_state.IsProcessingFrameOrAwaitingReset())
						;
				}
			}
		}
	}

	void JobRunnerData::AwaitAnyFramesFinish(const uint8 frameMask)
	{
		Renderer& renderer = System::Get<Renderer>();
		for (const Optional<LogicalDevice*> pLogicalDevice : renderer.GetLogicalDevices())
		{
			if (pLogicalDevice.IsValid())
			{
				AwaitAnyFramesFinish(pLogicalDevice->GetIdentifier(), frameMask);
			}
		}
	}

	bool JobRunnerData::IsProcessingFrame(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex) const
	{
		const Optional<LogicalDeviceData*> pLogicalDeviceData = m_logicalDeviceData[logicalDeviceIdentifier].Get();
		if (pLogicalDeviceData.IsValid())
		{
			PerFrameData& __restrict perFrameData = pLogicalDeviceData->m_perFrameData[frameIndex];
			return perFrameData.m_state.IsProcessingFrame();
		}
		else
		{
			return false;
		}
	}

	bool JobRunnerData::IsProcessingFrame(const uint8 frameIndex) const
	{
		Renderer& renderer = System::Get<Renderer>();
		for (const Optional<LogicalDevice*> pLogicalDevice : renderer.GetLogicalDevices())
		{
			if (pLogicalDevice.IsValid())
			{
				if (IsProcessingFrame(pLogicalDevice->GetIdentifier(), frameIndex))
				{
					return true;
				}
			}
		}
		return false;
	}

	bool JobRunnerData::IsProcessingAnyFrames(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameMask) const
	{
		const Optional<LogicalDeviceData*> pLogicalDeviceData = m_logicalDeviceData[logicalDeviceIdentifier].Get();
		if (pLogicalDeviceData.IsValid())
		{
			for (const uint8 frameIndex : Memory::GetSetBitsIterator(frameMask))
			{
				PerFrameData& __restrict perFrameData = pLogicalDeviceData->m_perFrameData[frameIndex];
				if (perFrameData.m_state.IsProcessingFrame())
				{
					return true;
				}
			}
			return false;
		}
		else
		{
			return false;
		}
	}

	bool JobRunnerData::IsProcessingAnyFrames(const uint8 frameMask) const
	{
		Renderer& renderer = System::Get<Renderer>();
		for (const Optional<LogicalDevice*> pLogicalDevice : renderer.GetLogicalDevices())
		{
			if (pLogicalDevice.IsValid())
			{
				if (IsProcessingAnyFrames(pLogicalDevice->GetIdentifier(), frameMask))
				{
					return true;
				}
			}
		}
		return false;
	}

	void JobRunnerData::QueueFinishFrameWorkInternal(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex)
	{
		Threading::JobRunnerThread& thread = GetRunnerThread();
		if (thread.IsExecutingOnThread())
		{
			OnFinishFrameWorkInternal(logicalDeviceIdentifier, frameIndex);
		}
		else
		{
			thread.QueueExclusiveCallbackFromAnyThread(
				Threading::JobPriority::EndFrame,
				[this, logicalDeviceIdentifier, frameIndex](Threading::JobRunnerThread&)
				{
					OnFinishFrameWorkInternal(logicalDeviceIdentifier, frameIndex);
				}
			);
		}
	}

	void JobRunnerData::OnFinishFrameWorkInternal(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex)
	{
		Assert(GetRunnerThread().IsExecutingOnThread(), "Can't finish framework on runner from outside thread!");
		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];
		if (perFrameData.m_state.m_data.m_stateFlags.AreAnySet(FrameStateFlags::AwaitingCpuFinish | FrameStateFlags::AwaitingGpuFinish))
		{
			return;
		}

		LogicalDevice& logicalDevice = *System::Get<Rendering::Renderer>().GetLogicalDevices()[logicalDeviceIdentifier];

		for ([[maybe_unused]] PerFramePoolData& perFramePool : perFrameData.m_commandPoolStorage)
		{
#if RENDERER_HAS_COMMAND_POOL
			if (perFramePool.m_commandPool.IsValid())
			{
				[[maybe_unused]] const bool wasReset =
					perFramePool.m_commandPool.Reset(logicalDevice, CommandPoolView::ResetFlags::ReturnCommandBufferMemoryToPool);
				Assert(wasReset);
			}
#endif

#if RENDERER_SUPPORTS_REUSABLE_COMMAND_BUFFERS
			perFramePool.m_availableCommandBuffers.MoveFrom(perFramePool.m_availableCommandBuffers.end(), perFramePool.m_usedCommandBuffers);
#endif
		}

#if 0 // PLATFORM_EMSCRIPTEN
		// TODO: Have the callback return the queue so we don't need to reallocate
		for (PerFrameQueue::Type queueType = PerFrameQueue::Type::First; queueType != PerFrameQueue::Type::End; ++queueType)
		{
			perFrameData.m_queueMutexes[(uint8)queueType].LockExclusive();
		}
		Rendering::Window::QueueOnWindowThread(
			[&perFrameData, queue = Move(perFrameData.m_queue), &logicalDeviceData, &logicalDevice]() mutable
			{
				for (PerFrameQueue::Type queueType = PerFrameQueue::Type::First; queueType != PerFrameQueue::Type::End; ++queueType)
				{
					if (queue.HasElements(queueType))
					{
						queue.Reset(logicalDeviceData, logicalDevice, queueType);
					}
				}

				for (PerFrameQueue::Type queueType = PerFrameQueue::Type::First; queueType != PerFrameQueue::Type::End; ++queueType)
				{
					perFrameData.m_queueMutexes[(uint8)queueType].UnlockExclusive();
				}

				perFrameData.m_state.FinishFrame();
			}
		);
#else
		for (PerFrameQueue::Type queueType = PerFrameQueue::Type::First; queueType != PerFrameQueue::Type::End; ++queueType)
		{
			if (perFrameData.m_queue.HasElements(queueType))
			{
				Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)queueType]);
				perFrameData.m_queue.Reset(logicalDeviceData, logicalDevice, queueType);
			}
		}

		perFrameData.m_state.FinishFrame();
#endif
	}

	bool JobRunnerData::PerFrameQueue::HasElements(const Type type) const
	{
		switch (type)
		{
			case Type::DescriptorSet:
				return m_queuedDescriptorReleases.HasElements();
			case Type::DescriptorSetLayout:
				return m_queuedDescriptorLayoutReleases.HasElements();
			case Type::ImageMapping:
				return m_queuedImageMappingReleases.HasElements();
			case Type::Image:
				return m_queuedImageReleases.HasElements();
			case Type::Buffer:
				return m_queuedBufferReleases.HasElements();
			case Type::CommandBuffer:
			{
				for (QueueFamily queueFamily = QueueFamily::First; queueFamily != QueueFamily::End; queueFamily = queueFamily << 1)
				{
					const uint8 queueFamilyIndex = (uint8)Math::Log2((uint8)queueFamily);
					if (m_queuedCommandBufferReleases[queueFamilyIndex].HasElements())
					{
						return true;
					}
				}
				return false;
			}
			case Type::Semaphore:
				return m_queuedSemaphoreReleases.HasElements();
			case Type::Fence:
				return m_queuedFenceReleases.HasElements();
			case Type::AccelerationStructure:
				return m_queuedPrimitiveAccelerationStructureReleases.HasElements() || m_queuedInstanceAccelerationStructureReleases.HasElements();
			case Type::Count:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	void JobRunnerData::PerFrameQueue::Reset(LogicalDeviceData& __restrict logicalDeviceData, LogicalDevice& logicalDevice, const Type type)
	{
		switch (type)
		{
			case Type::DescriptorSet:
			{
				logicalDeviceData.m_descriptorPool.FreeDescriptorSets(logicalDevice, m_queuedDescriptorReleases.GetView());
				m_queuedDescriptorReleases.Clear();
			}
			break;
			case Type::DescriptorSetLayout:
			{
				for (DescriptorSetLayout& descriptorSetLayout : m_queuedDescriptorLayoutReleases)
				{
					descriptorSetLayout.Destroy(logicalDevice);
				}
				m_queuedDescriptorLayoutReleases.Clear();
			}
			break;
			case Type::ImageMapping:
			{
				for (ImageMapping& imageMapping : m_queuedImageMappingReleases)
				{
					imageMapping.Destroy(logicalDevice);
				}
				m_queuedImageMappingReleases.Clear();
			}
			break;
			case Type::Image:
			{
				for (Image& image : m_queuedImageReleases)
				{
					image.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
				}
				m_queuedImageReleases.Clear();
			}
			break;
			case Type::Buffer:
			{
				for (Buffer& buffer : m_queuedBufferReleases)
				{
					buffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
				}
				m_queuedBufferReleases.Clear();
			}
			break;
			case Type::CommandBuffer:
			{
				for (QueueFamily queueFamily = QueueFamily::First; queueFamily != QueueFamily::End; queueFamily = queueFamily << 1)
				{
					const uint8 queueFamilyIndex = (uint8)Math::Log2((uint8)queueFamily);
					if (m_queuedCommandBufferReleases[queueFamilyIndex].HasElements())
					{
						const CommandPoolView commandPool = logicalDeviceData.m_commandPoolViews[queueFamilyIndex];

#if RENDERER_HAS_COMMAND_POOL
						commandPool.FreeCommandBuffers(logicalDevice, m_queuedCommandBufferReleases[queueFamilyIndex].GetView());
#else
						for (CommandBuffer& commandBuffer : m_queuedCommandBufferReleases[queueFamilyIndex])
						{
							commandBuffer.Destroy(logicalDevice, commandPool);
						}
#endif

						m_queuedCommandBufferReleases[queueFamilyIndex].Clear();
					}
				}
			}
			break;
			case Type::Semaphore:
			{
				for (Semaphore& semaphore : m_queuedSemaphoreReleases)
				{
					semaphore.Destroy(logicalDevice);
				}
				m_queuedSemaphoreReleases.Clear();
			}
			break;
			case Type::Fence:
			{
				for (Fence& fence : m_queuedFenceReleases)
				{
					fence.Destroy(logicalDevice);
				}
				m_queuedFenceReleases.Clear();
			}
			break;
			case Type::AccelerationStructure:
			{
				for (PrimitiveAccelerationStructure& primitiveAccelerationStructure : m_queuedPrimitiveAccelerationStructureReleases)
				{
					primitiveAccelerationStructure.Destroy(logicalDevice);
				}
				m_queuedPrimitiveAccelerationStructureReleases.Clear();
				for (InstanceAccelerationStructure& instanceAccelerationStructure : m_queuedInstanceAccelerationStructureReleases)
				{
					instanceAccelerationStructure.Destroy(logicalDevice);
				}
				m_queuedInstanceAccelerationStructureReleases.Clear();
			}
			break;
			case Type::Count:
				ExpectUnreachable();
		}
	}

	void JobRunnerData::DestroyDescriptorSet(const LogicalDeviceIdentifier logicalDeviceIdentifier, DescriptorSet&& descriptorSet)
	{
		DestroyDescriptorSet(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			Forward<DescriptorSet>(descriptorSet)
		);
	}

	void JobRunnerData::DestroyDescriptorSets(const LogicalDeviceIdentifier logicalDeviceIdentifier, ArrayView<DescriptorSet> descriptorSets)
	{
		DestroyDescriptorSets(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			descriptorSets
		);
	}

	void JobRunnerData::DestroyDescriptorSet(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, DescriptorSet&& descriptorSet
	)
	{
		Assert(descriptorSet.IsValid());

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::DescriptorSet]);
		perFrameData.m_queue.m_queuedDescriptorReleases.EmplaceBack(Forward<DescriptorSet>(descriptorSet));
	}

	void JobRunnerData::DestroyDescriptorSets(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, ArrayView<DescriptorSet> descriptorSets
	)
	{
		Assert(descriptorSets.All(
			[](const DescriptorSetView descriptorSet)
			{
				return descriptorSet.IsValid();
			}
		));

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::DescriptorSet]);
		perFrameData.m_queue.m_queuedDescriptorReleases.MoveEmplaceRangeBack(descriptorSets);
	}

	void JobRunnerData::DestroyDescriptorSetLayout(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, DescriptorSetLayout&& descriptorSetLayout
	)
	{
		DestroyDescriptorSetLayout(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			Forward<DescriptorSetLayout>(descriptorSetLayout)
		);
	}

	void JobRunnerData::DestroyDescriptorSetLayouts(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, ArrayView<DescriptorSetLayout> descriptorSetLayouts
	)
	{
		DestroyDescriptorSetLayouts(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			descriptorSetLayouts
		);
	}

	void JobRunnerData::DestroyDescriptorSetLayout(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, DescriptorSetLayout&& descriptorSetLayout
	)
	{
		Assert(descriptorSetLayout.IsValid());

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::DescriptorSetLayout]);
		perFrameData.m_queue.m_queuedDescriptorLayoutReleases.EmplaceBack(Forward<DescriptorSetLayout>(descriptorSetLayout));
	}

	void JobRunnerData::DestroyDescriptorSetLayouts(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, ArrayView<DescriptorSetLayout> descriptorSetLayouts
	)
	{
		Assert(descriptorSetLayouts.All(
			[](const DescriptorSetLayoutView descriptorSetLayout)
			{
				return descriptorSetLayout.IsValid();
			}
		));

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::DescriptorSetLayout]);
		perFrameData.m_queue.m_queuedDescriptorLayoutReleases.MoveEmplaceRangeBack(descriptorSetLayouts);
	}

	void JobRunnerData::DestroyImageMapping(const LogicalDeviceIdentifier logicalDeviceIdentifier, ImageMapping&& imageMapping)
	{
		DestroyImageMapping(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			Forward<ImageMapping>(imageMapping)
		);
	}

	void JobRunnerData::DestroyImageMappings(const LogicalDeviceIdentifier logicalDeviceIdentifier, ArrayView<ImageMapping> imageMappings)
	{
		DestroyImageMappings(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			imageMappings
		);
	}

	void JobRunnerData::DestroyImageMapping(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, ImageMapping&& imageMapping
	)
	{
		Assert(imageMapping.IsValid());

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::ImageMapping]);
		perFrameData.m_queue.m_queuedImageMappingReleases.EmplaceBack(Forward<ImageMapping>(imageMapping));
	}

	void JobRunnerData::DestroyImageMappings(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, ArrayView<ImageMapping> imageMappings
	)
	{
		Assert(imageMappings.All(
			[](const ImageMappingView imageMapping)
			{
				return imageMapping.IsValid();
			}
		));

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::ImageMapping]);
		perFrameData.m_queue.m_queuedImageMappingReleases.MoveEmplaceRangeBack(imageMappings);
	}

	void JobRunnerData::DestroyImage(const LogicalDeviceIdentifier logicalDeviceIdentifier, Image&& image)
	{
		DestroyImage(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			Forward<Image>(image)
		);
	}

	void JobRunnerData::DestroyImages(const LogicalDeviceIdentifier logicalDeviceIdentifier, ArrayView<Image> images)
	{
		DestroyImages(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			images
		);
	}

	void JobRunnerData::DestroyImage(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, Image&& image)
	{
		Assert(image.IsValid());

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::Image]);
		perFrameData.m_queue.m_queuedImageReleases.EmplaceBack(Forward<Image>(image));
	}

	void JobRunnerData::DestroyImages(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, ArrayView<Image> images)
	{
		Assert(images.All(
			[](const ImageView image)
			{
				return image.IsValid();
			}
		));

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::Image]);
		perFrameData.m_queue.m_queuedImageReleases.MoveEmplaceRangeBack(images);
	}

	void JobRunnerData::DestroyBuffer(const LogicalDeviceIdentifier logicalDeviceIdentifier, Buffer&& buffer)
	{
		DestroyBuffer(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			Forward<Buffer>(buffer)
		);
	}

	void JobRunnerData::DestroyBuffers(const LogicalDeviceIdentifier logicalDeviceIdentifier, ArrayView<Buffer> buffers)
	{
		DestroyBuffers(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			buffers
		);
	}

	void JobRunnerData::DestroyBuffer(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, Buffer&& buffer)
	{
		Assert(buffer.IsValid());

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::Buffer]);
		perFrameData.m_queue.m_queuedBufferReleases.EmplaceBack(Forward<Buffer>(buffer));
	}

	void
	JobRunnerData::DestroyBuffers(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, ArrayView<Buffer> buffers)
	{
		Assert(buffers.All(
			[](const BufferView buffer)
			{
				return buffer.IsValid();
			}
		));

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::Buffer]);
		perFrameData.m_queue.m_queuedBufferReleases.MoveEmplaceRangeBack(buffers);
	}

	void JobRunnerData::DestroyCommandBuffer(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, const QueueFamily queueFamily, CommandBuffer&& commandBuffer
	)
	{
		DestroyCommandBuffer(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			queueFamily,
			Forward<CommandBuffer>(commandBuffer)
		);
	}

	void JobRunnerData::DestroyCommandBuffers(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, const QueueFamily queueFamily, ArrayView<CommandBuffer> commandBuffers
	)
	{
		DestroyCommandBuffers(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			queueFamily,
			commandBuffers
		);
	}

	void JobRunnerData::DestroyCommandBuffer(
		const LogicalDeviceIdentifier logicalDeviceIdentifier,
		const uint8 frameIndex,
		const QueueFamily queueFamily,
		CommandBuffer&& commandBuffer
	)
	{
		Assert(commandBuffer.IsValid());

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		const uint8 queueFamilyIndex = (uint8)Math::Log2((uint8)queueFamily);

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::CommandBuffer]);
		perFrameData.m_queue.m_queuedCommandBufferReleases[queueFamilyIndex].EmplaceBack(Forward<CommandBuffer>(commandBuffer));
	}

	void JobRunnerData::DestroyCommandBuffers(
		const LogicalDeviceIdentifier logicalDeviceIdentifier,
		const uint8 frameIndex,
		const QueueFamily queueFamily,
		ArrayView<CommandBuffer> commandBuffers
	)
	{
		Assert(commandBuffers.All(
			[](const CommandBufferView commandBuffer)
			{
				return commandBuffer.IsValid();
			}
		));

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		const uint8 queueFamilyIndex = (uint8)Math::Log2((uint8)queueFamily);

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::CommandBuffer]);
		perFrameData.m_queue.m_queuedCommandBufferReleases[queueFamilyIndex].MoveEmplaceRangeBack(commandBuffers);
	}

	void JobRunnerData::DestroySemaphore(const LogicalDeviceIdentifier logicalDeviceIdentifier, Semaphore&& semaphore)
	{
		DestroySemaphore(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			Forward<Semaphore>(semaphore)
		);
	}

	void JobRunnerData::DestroySemaphore(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, Semaphore&& semaphore)
	{
		Assert(semaphore.IsValid());

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::Semaphore]);
		perFrameData.m_queue.m_queuedSemaphoreReleases.EmplaceBack(Forward<Semaphore>(semaphore));
	}

	void JobRunnerData::DestroyFence(const LogicalDeviceIdentifier logicalDeviceIdentifier, Fence&& fence)
	{
		DestroyFence(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			Forward<Fence>(fence)
		);
	}

	void JobRunnerData::DestroyFence(const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, Fence&& fence)
	{
		Assert(fence.IsValid());

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::Fence]);
		perFrameData.m_queue.m_queuedFenceReleases.EmplaceBack(Forward<Fence>(fence));
	}

	void JobRunnerData::DestroyAccelerationStructure(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, PrimitiveAccelerationStructure&& accelerationStructure
	)
	{
		DestroyAccelerationStructure(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			Forward<PrimitiveAccelerationStructure>(accelerationStructure)
		);
	}

	void JobRunnerData::DestroyAccelerationStructure(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, PrimitiveAccelerationStructure&& accelerationStructure
	)
	{
		Assert(accelerationStructure.IsValid());

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::AccelerationStructure]);
		perFrameData.m_queue.m_queuedPrimitiveAccelerationStructureReleases.EmplaceBack(
			Forward<PrimitiveAccelerationStructure>(accelerationStructure)
		);
	}

	void JobRunnerData::DestroyAccelerationStructure(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, InstanceAccelerationStructure&& accelerationStructure
	)
	{
		DestroyAccelerationStructure(
			logicalDeviceIdentifier,
			(System::Get<Engine>().GetCurrentFrameIndex() + 1) % Rendering::MaximumConcurrentFrameCount,
			Forward<InstanceAccelerationStructure>(accelerationStructure)
		);
	}

	void JobRunnerData::DestroyAccelerationStructure(
		const LogicalDeviceIdentifier logicalDeviceIdentifier, const uint8 frameIndex, InstanceAccelerationStructure&& accelerationStructure
	)
	{
		Assert(accelerationStructure.IsValid());

		LogicalDeviceData& __restrict logicalDeviceData = *m_logicalDeviceData[logicalDeviceIdentifier];
		PerFrameData& __restrict perFrameData = logicalDeviceData.m_perFrameData[frameIndex];

		Threading::UniqueLock lock(perFrameData.m_queueMutexes[(uint8)PerFrameQueue::Type::AccelerationStructure]);
		perFrameData.m_queue.m_queuedInstanceAccelerationStructureReleases.EmplaceBack(
			Forward<InstanceAccelerationStructure>(accelerationStructure)
		);
	}
}
