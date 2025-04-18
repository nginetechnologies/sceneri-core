#include <Renderer/Buffers/BufferWebGPU.h>

#include <Renderer/Commands/CommandBufferView.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Commands/CommandQueueView.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <Renderer/Window/Window.h>

#include <Renderer/WebGPU/Includes.h>

#if PLATFORM_EMSCRIPTEN
#include <emscripten/emscripten.h>
#include <emscripten/threading.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#elif RENDERER_WEBGPU
#include <Common/Threading/Jobs/JobRunnerThread.h>
#endif

namespace ngine::Rendering
{
#if RENDERER_WEBGPU
	void BufferWebGPU::CreateDeviceBuffer(
		LogicalDevice& logicalDevice,
		[[maybe_unused]] const PhysicalDevice& physicalDevice,
		[[maybe_unused]] DeviceMemoryPool& deviceMemoryPool,
		const EnumFlags<UsageFlags> usageFlags,
		[[maybe_unused]] const EnumFlags<MemoryFlags> memoryFlags
	)
	{
		uint32 wgpuUsageFlags{0};
		wgpuUsageFlags |= WGPUBufferUsage_CopySrc * usageFlags.IsSet(UsageFlags::TransferSource);
		wgpuUsageFlags |= WGPUBufferUsage_CopyDst * usageFlags.IsSet(UsageFlags::TransferDestination);
		wgpuUsageFlags |= WGPUBufferUsage_Index * usageFlags.IsSet(UsageFlags::IndexBuffer);
		wgpuUsageFlags |= WGPUBufferUsage_Vertex * usageFlags.IsSet(UsageFlags::VertexBuffer);
		wgpuUsageFlags |= WGPUBufferUsage_Uniform * usageFlags.IsSet(UsageFlags::UniformBuffer);
		wgpuUsageFlags |= WGPUBufferUsage_Storage * usageFlags.IsSet(UsageFlags::StorageBuffer);
		wgpuUsageFlags |= WGPUBufferUsage_MapRead *
		                  (memoryFlags.IsSet(MemoryFlags::HostVisible) & usageFlags.IsNotSet(UsageFlags::TransferSource));
		wgpuUsageFlags |= WGPUBufferUsage_MapWrite *
		                  (memoryFlags.IsSet(MemoryFlags::HostVisible) & usageFlags.IsNotSet(UsageFlags::TransferDestination));

		const bool mappedAtCreation = false;
		const WGPUBufferDescriptor descriptor
		{
			nullptr,
#if RENDERER_WEBGPU_DAWN
			WGPUStringView { nullptr, 0 },
#else
			nullptr,
#endif
			wgpuUsageFlags,
			m_bufferSize,
			mappedAtCreation
		};

#if WEBGPU_SINGLE_THREADED
		WGPUBuffer* pBuffer = new WGPUBuffer{nullptr};
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[&logicalDevice, descriptor, pBuffer]()
			{
				WGPUBuffer pWGPUBuffer = wgpuDeviceCreateBuffer(logicalDevice, &descriptor);
#if RENDERER_WEBGPU_DAWN
				wgpuBufferAddRef(pWGPUBuffer);
#else
				wgpuBufferReference(pWGPUBuffer);
#endif
				*pBuffer = pWGPUBuffer;
			}
		);
		m_buffer.m_pBuffer = pBuffer;
#else
		WGPUBuffer pBuffer = wgpuDeviceCreateBuffer(logicalDevice, &descriptor);
#if RENDERER_WEBGPU_DAWN
		wgpuBufferAddRef(pBuffer);
#else
		wgpuBufferReference(pBuffer);
#endif
		m_buffer.m_pBuffer = pBuffer;
#endif
	}

	void BufferWebGPU::DestroyDeviceBuffer(
		[[maybe_unused]] const LogicalDeviceView logicalDevice, [[maybe_unused]] DeviceMemoryPool& deviceMemoryPool
	)
	{
		if (m_buffer.m_pBuffer != nullptr)
		{
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pBuffer = m_buffer.m_pBuffer]()
				{
					WGPUBuffer pWGPUBuffer = *pBuffer;
					if (pWGPUBuffer != nullptr)
					{
						wgpuBufferRelease(pWGPUBuffer);
						wgpuBufferDestroy(pWGPUBuffer);
					}
					delete pBuffer;
				}
			);
#else
			wgpuBufferRelease(m_buffer.m_pBuffer);
			wgpuBufferDestroy(m_buffer.m_pBuffer);
#endif
			m_buffer.m_pBuffer = nullptr;
		}
	}

	void BufferWebGPU::MapAndCopyFrom(
		const LogicalDevice& logicalDevice,
		const QueueFamily queueFamily,
		const ArrayView<const DataToBuffer> copies,
		const Math::Range<size> bufferRange
	)
	{
		EnumFlags<Flags> flags = m_flags.GetFlags();
		do
		{
			Assert(flags.IsSet(Flags::IsHostMappable), "Memory must be host mappable!");
			Assert(!flags.IsSet(Flags::IsHostMapping), "Memory mapping cannot overlap!");
			Assert(!flags.IsSet(Flags::IsHostMapped), "Memory cannot be simultaneously mapped!");
			if (UNLIKELY_ERROR(flags.IsNotSet(Flags::IsHostMappable) | flags.AreAnySet(Flags::IsHostMapping | Flags::IsHostMapped)))
			{
				return;
			}
		} while (!m_flags.CompareExchangeWeak(flags, flags | Flags::IsHostMapping));

		struct Data
		{
			BufferWebGPU& buffer;
			ArrayView<const DataToBuffer> copies;
			Math::Range<size> mappedRange;
		};
		Data mapData{*this, copies, bufferRange};

		QueueSubmissionJob& queueSubmissionJob = logicalDevice.GetQueueSubmissionJob(queueFamily);
		queueSubmissionJob.QueueCallback(
			[this, bufferRange, &mapData]()
			{
				const auto callback = [](const WGPUBufferMapAsyncStatus status, void* pUserData)
				{
					Data& __restrict mapData = *reinterpret_cast<Data*>(pUserData);

					switch (status)
					{
						case WGPUBufferMapAsyncStatus_Success:
						{
							[[maybe_unused]] const bool wasFlagSet = mapData.buffer.m_flags.TrySetFlags(Flags::IsHostMapped);
							Assert(wasFlagSet);

							void* pMappedMemory =
								wgpuBufferGetMappedRange(mapData.buffer.m_buffer, mapData.mappedRange.GetMinimum(), mapData.mappedRange.GetSize());
							Assert(pMappedMemory != nullptr);
							if (LIKELY(pMappedMemory != nullptr))
							{
								for (const DataToBuffer& __restrict copyToBufferInfo : mapData.copies)
								{
									const ByteView target{
										reinterpret_cast<ByteType*>(pMappedMemory) + copyToBufferInfo.targetOffset,
										copyToBufferInfo.source.GetDataSize()
									};
									target.CopyFrom(copyToBufferInfo.source);
								}
							}

							wgpuBufferUnmap(mapData.buffer.m_buffer);
							[[maybe_unused]] const EnumFlags<Flags> previousFlags =
								mapData.buffer.m_flags.FetchAnd(~(Flags::IsHostMapping | Flags::IsHostMapped));
							Assert(previousFlags.AreAllSet(Flags::IsHostMapping | Flags::IsHostMapped));
						}
						break;
#if RENDERER_WEBGPU_DAWN
						case WGPUBufferMapAsyncStatus_InstanceDropped:
#endif
						case WGPUBufferMapAsyncStatus_ValidationError:
						case WGPUBufferMapAsyncStatus_OffsetOutOfRange:
						case WGPUBufferMapAsyncStatus_SizeOutOfRange:
						case WGPUBufferMapAsyncStatus_Unknown:
						case WGPUBufferMapAsyncStatus_DeviceLost:
						{
							Assert(false, "CopyFrom failed");
							[[maybe_unused]] const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~Flags::IsHostMapping);
							Assert(previousFlags.IsSet(Flags::IsHostMapping));
						}
						break;
						case WGPUBufferMapAsyncStatus_DestroyedBeforeCallback:
						{
							Assert(false, "Buffer must be unmapped before being destroyed!");
							[[maybe_unused]] const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~Flags::IsHostMapping);
							Assert(previousFlags.IsSet(Flags::IsHostMapping));
						}
						break;
						case WGPUBufferMapAsyncStatus_MappingAlreadyPending:
						{
							Assert(false, "Buffer can't be mapped twice!");
							[[maybe_unused]] const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~Flags::IsHostMapping);
							Assert(previousFlags.IsSet(Flags::IsHostMapping));
						}
						break;
						case WGPUBufferMapAsyncStatus_UnmappedBeforeCallback:
						{
							[[maybe_unused]] const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~Flags::IsHostMapping);
							Assert(previousFlags.IsSet(Flags::IsHostMapping));
						}
						break;
						case WGPUBufferMapAsyncStatus_Force32:
							ExpectUnreachable();
					}
				};

				wgpuBufferMapAsync(
					m_buffer,
					WGPUMapMode_Write,
					bufferRange.GetMinimum(),
					bufferRange.GetSize(),
					callback,
					&mapData
				);
			}
		);
	}

	void BufferWebGPU::MapToHostMemoryDeviceBuffer(
		const LogicalDevice& logicalDevice,
		const Math::Range<size> mappedRange,
		const EnumFlags<MapMemoryFlags> mappedMemoryFlags,
		MapMemoryAsyncCallback&& callback
	)
	{
		struct MappedMemoryData
		{
			BufferWebGPU& buffer;
			Math::Range<size> mappedRange;
			EnumFlags<MapMemoryFlags> flags;
			MapMemoryAsyncCallback callback;
			Threading::Atomic<bool> finished{false};
		};

		MappedMemoryData mapData{*this, mappedRange, mappedMemoryFlags, Forward<MapMemoryAsyncCallback>(callback)};

		auto mapBuffer = [this, mappedMemoryFlags, mappedRange, &mapData]()
		{
			const auto callback = [](const WGPUBufferMapAsyncStatus status, void* pUserData)
			{
				MappedMemoryData& __restrict mapData = *reinterpret_cast<MappedMemoryData*>(pUserData);

				switch (status)
				{
					case WGPUBufferMapAsyncStatus_Success:
					{
						EnumFlags<Flags> flags = mapData.buffer.m_flags.GetFlags();
						do
						{
							Assert(flags.IsSet(Flags::IsHostMappable), "Memory must be host mappable!");
							Assert(flags.IsSet(Flags::IsHostMapping), "Memory mapping must be in progress!");
							Assert(!flags.IsSet(Flags::IsHostMapped), "Memory cannot be simultaneously mapped!");
							if (UNLIKELY_ERROR(!flags.AreAllSet(Flags::IsHostMappable | Flags::IsHostMapping) | flags.IsSet(Flags::IsHostMapped)))
							{
								mapData.callback(MapMemoryStatus::MapFailed, {}, false);
								mapData.finished = true;
								return;
							}
						} while (!mapData.buffer.m_flags.CompareExchangeWeak(flags, flags | Flags::IsHostMapped));

						void* pMappedMemory;
						if (mapData.flags.IsSet(MapMemoryFlags::Write))
						{
							pMappedMemory =
								wgpuBufferGetMappedRange(mapData.buffer.m_buffer, mapData.mappedRange.GetMinimum(), mapData.mappedRange.GetSize());
						}
						else
						{
							pMappedMemory = const_cast<void*>(
								wgpuBufferGetConstMappedRange(mapData.buffer.m_buffer, mapData.mappedRange.GetMinimum(), mapData.mappedRange.GetSize())
							);
						}
						Assert(pMappedMemory != nullptr);
						if (LIKELY(pMappedMemory != nullptr))
						{
							mapData.callback(
								MapMemoryStatus::Success,
								ByteView{reinterpret_cast<ByteType*>(pMappedMemory), mapData.mappedRange.GetSize()},
								false
							);
						}
						else
						{
							mapData.callback(MapMemoryStatus::MapFailed, {}, false);
						}

						if (mapData.flags.IsNotSet(MapMemoryFlags::KeepMapped))
						{
							const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~(Flags::IsHostMapping | Flags::IsHostMapped));
							Assert(previousFlags.IsSet(Flags::IsHostMapped));
							Assert(previousFlags.IsSet(Flags::IsHostMapping));
							if (LIKELY(previousFlags.IsSet(Flags::IsHostMapped)))
							{
								wgpuBufferUnmap(mapData.buffer.m_buffer);
							}
						}
					}
					break;
					case WGPUBufferMapAsyncStatus_ValidationError:
					case WGPUBufferMapAsyncStatus_OffsetOutOfRange:
					case WGPUBufferMapAsyncStatus_SizeOutOfRange:
					case WGPUBufferMapAsyncStatus_Unknown:
					case WGPUBufferMapAsyncStatus_DeviceLost:
#if RENDERER_WEBGPU_DAWN
					case WGPUBufferMapAsyncStatus_InstanceDropped:
#endif
					{
						mapData.callback(MapMemoryStatus::MapFailed, {}, false);
						[[maybe_unused]] const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~Flags::IsHostMapping);
						Assert(previousFlags.IsSet(Flags::IsHostMapping));
					}
					break;
					case WGPUBufferMapAsyncStatus_DestroyedBeforeCallback:
					{
						Assert(false, "Buffer must be unmapped before being destroyed!");
						[[maybe_unused]] const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~Flags::IsHostMapping);
						Assert(previousFlags.IsSet(Flags::IsHostMapping));
					}
					break;
					case WGPUBufferMapAsyncStatus_MappingAlreadyPending:
					{
						Assert(false, "Buffer can't be mapped twice!");
						[[maybe_unused]] const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~Flags::IsHostMapping);
						Assert(previousFlags.IsSet(Flags::IsHostMapping));
					}
					break;
					case WGPUBufferMapAsyncStatus_UnmappedBeforeCallback:
					{
						[[maybe_unused]] const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~Flags::IsHostMapping);
						Assert(previousFlags.IsSet(Flags::IsHostMapping));
					}
					break;
					case WGPUBufferMapAsyncStatus_Force32:
						ExpectUnreachable();
				}

				mapData.finished = true;
			};

			wgpuBufferMapAsync(
				m_buffer,
				static_cast<WGPUMapMode>(mappedMemoryFlags.GetFlags() & MapMemoryFlags::ReadWrite),
				mappedRange.GetMinimum(),
				mappedRange.GetSize(),
				callback,
				&mapData
			);
		};

		QueueSubmissionJob& queueSubmissionJob = logicalDevice.GetQueueSubmissionJob(QueueFamily::Transfer);
		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
#if WEBGPU_SINGLE_THREADED
		queueSubmissionJob.QueueCallback(Move(mapBuffer));

		while (!mapData.finished)
		{
			thread.DoRunNextJob();
		}
#else
		const bool canRunOnThread = (queueSubmissionJob.GetAllowedJobRunnerMask() & (1ull << thread.GetThreadIndex())) != 0;
		if (canRunOnThread)
		{
			if (queueSubmissionJob.IsExecuting())
			{
				mapBuffer();

				while (!mapData.finished)
				{
					queueSubmissionJob.TickDevice();
				}
			}
			else
			{
				queueSubmissionJob.QueueCallback(Move(mapBuffer));

				while (!mapData.finished)
				{
					queueSubmissionJob.Tick();
				}
			}
		}
		else
		{
			queueSubmissionJob.QueueCallback(Move(mapBuffer));

			while (!mapData.finished)
			{
				thread.DoRunNextJob();
			}
		}
#endif
	}

	bool BufferWebGPU::MapToHostMemoryDeviceBufferAsync(
		const LogicalDevice& logicalDevice,
		const Math::Range<size> mappedRange,
		const EnumFlags<MapMemoryFlags> mappedMemoryFlags,
		MapMemoryAsyncCallback&& callback
	)
	{
		QueueSubmissionJob& queueSubmissionJob = logicalDevice.GetQueueSubmissionJob(QueueFamily::Transfer);
		queueSubmissionJob.QueueCallback(
			[this, mappedMemoryFlags, mappedRange, callback = Forward<MapMemoryAsyncCallback>(callback)]() mutable
			{
				struct MappedMemoryData
				{
					BufferWebGPU& buffer;
					Math::Range<size> mappedRange;
					EnumFlags<MapMemoryFlags> flags;
					MapMemoryAsyncCallback callback;
				};
				MappedMemoryData* pMapData = new MappedMemoryData{*this, mappedRange, mappedMemoryFlags, Move(callback)};
				
				const auto gpuCallback = [](const WGPUBufferMapAsyncStatus status, void* pUserData)
				{
					MappedMemoryData& __restrict mapData = *reinterpret_cast<MappedMemoryData*>(pUserData);

					switch (status)
					{
						case WGPUBufferMapAsyncStatus_Success:
						{
							EnumFlags<Flags> flags = mapData.buffer.m_flags.GetFlags();
							do
							{
								Assert(flags.IsSet(Flags::IsHostMappable), "Memory must be host mappable!");
								Assert(flags.IsSet(Flags::IsHostMapping), "Memory mapping must be in progress!");
								Assert(!flags.IsSet(Flags::IsHostMapped), "Memory cannot be simultaneously mapped!");
								if (UNLIKELY_ERROR(!flags.AreAllSet(Flags::IsHostMappable | Flags::IsHostMapping) | flags.IsSet(Flags::IsHostMapped)))
								{
									mapData.callback(MapMemoryStatus::MapFailed, {}, true);
									return;
								}
							} while (!mapData.buffer.m_flags.CompareExchangeWeak(flags, flags | Flags::IsHostMapped));

							void* pMappedMemory;
							if (mapData.flags.IsSet(MapMemoryFlags::Write))
							{
								pMappedMemory =
									wgpuBufferGetMappedRange(mapData.buffer.m_buffer, mapData.mappedRange.GetMinimum(), mapData.mappedRange.GetSize());
							}
							else
							{
								pMappedMemory = const_cast<void*>(
									wgpuBufferGetConstMappedRange(mapData.buffer.m_buffer, mapData.mappedRange.GetMinimum(), mapData.mappedRange.GetSize())
								);
							}
							Assert(pMappedMemory != nullptr);
							if (LIKELY(pMappedMemory != nullptr))
							{
								mapData.callback(
									MapMemoryStatus::Success,
									ByteView{reinterpret_cast<ByteType*>(pMappedMemory), mapData.mappedRange.GetSize()},
									true
								);
							}
							else
							{
								mapData.callback(MapMemoryStatus::MapFailed, {}, true);
							}

							if (mapData.flags.IsNotSet(MapMemoryFlags::KeepMapped))
							{
								const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~(Flags::IsHostMapping | Flags::IsHostMapped));
								Assert(previousFlags.IsSet(Flags::IsHostMapped));
								Assert(previousFlags.IsSet(Flags::IsHostMapping));
								if (LIKELY(previousFlags.IsSet(Flags::IsHostMapped)))
								{
									wgpuBufferUnmap(mapData.buffer.m_buffer);
								}
							}
						}
						break;
						case WGPUBufferMapAsyncStatus_ValidationError:
						case WGPUBufferMapAsyncStatus_OffsetOutOfRange:
						case WGPUBufferMapAsyncStatus_SizeOutOfRange:
						case WGPUBufferMapAsyncStatus_Unknown:
						case WGPUBufferMapAsyncStatus_DeviceLost:
#if RENDERER_WEBGPU_DAWN
						case WGPUBufferMapAsyncStatus_InstanceDropped:
#endif
						{
							mapData.callback(MapMemoryStatus::MapFailed, {}, true);
							[[maybe_unused]] const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~Flags::IsHostMapping);
							Assert(previousFlags.IsSet(Flags::IsHostMapping));
						}
						break;
						case WGPUBufferMapAsyncStatus_DestroyedBeforeCallback:
						{
							Assert(false, "Buffer must be unmapped before being destroyed!");
							[[maybe_unused]] const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~Flags::IsHostMapping);
							Assert(previousFlags.IsSet(Flags::IsHostMapping));
						}
						break;
						case WGPUBufferMapAsyncStatus_MappingAlreadyPending:
						{
							Assert(false, "Buffer can't be mapped twice!");
							[[maybe_unused]] const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~Flags::IsHostMapping);
							Assert(previousFlags.IsSet(Flags::IsHostMapping));
						}
						break;
						case WGPUBufferMapAsyncStatus_UnmappedBeforeCallback:
						{
							[[maybe_unused]] const EnumFlags<Flags> previousFlags = mapData.buffer.m_flags.FetchAnd(~Flags::IsHostMapping);
							Assert(previousFlags.IsSet(Flags::IsHostMapping));
						}
						break;
						case WGPUBufferMapAsyncStatus_Force32:
							ExpectUnreachable();
					}

					delete &mapData;
				};

				wgpuBufferMapAsync(
					m_buffer,
					static_cast<WGPUMapMode>(mappedMemoryFlags.GetFlags() & MapMemoryFlags::ReadWrite),
					mappedRange.GetMinimum(),
					mappedRange.GetSize(),
					gpuCallback,
					pMapData
				);
			}
		);
		// Indicate asynchronous completion
		return true;
	}

	void BufferWebGPU::UnmapFromHostMemoryDeviceBuffer([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
		Rendering::Window::QueueOnWindowThread(
			[buffer = m_buffer]()
			{
				wgpuBufferUnmap(buffer);
			}
		);
	}

	void BufferWebGPU::SetDebugNameDeviceBuffer(
		[[maybe_unused]] const LogicalDevice& logicalDevice, [[maybe_unused]] const ConstZeroTerminatedStringView name
	)
	{
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pBuffer = m_buffer.m_pBuffer, name]()
			{
				WGPUBuffer pWGPUBuffer = *pBuffer;
				if (pWGPUBuffer != nullptr)
				{
#if RENDERER_WEBGPU_DAWN
					wgpuBufferSetLabel(pWGPUBuffer, WGPUStringView { name, name.GetSize() });
#else
					wgpuBufferSetLabel(pWGPUBuffer, name);
#endif
				}
			}
		);
#else
		wgpuBufferSetLabel(m_buffer.m_pBuffer, name);
#endif
	}
#endif
}
