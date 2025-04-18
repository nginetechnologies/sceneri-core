#include "Assets/Texture/TextureCache.h"
#include "Assets/Texture/TextureAsset.h"
#include "Assets/Texture/TextureAssetType.h"

#include "Devices/LogicalDevice.h"

#include <Engine/Engine.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Asset/AssetType.inl>

#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Assets/Texture/RenderTargetAsset.h>
#include <Renderer/Assets/Texture/MipMask.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <Renderer/Threading/Semaphore.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Commands/UnifiedCommandBuffer.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/FormatInfo.h>
#include <Renderer/Renderer.h>
#include <Renderer/Window/Window.h>

#include <Renderer/WebGPU/Includes.h>
#include "WebGPU/ConvertImageAspectFlags.h"

#include <Common/System/Query.h>
#include <Common/Math/Random.h>
#include <Common/Math/Vector2/Mod.h>
#include <Common/Math/Format/Vector2.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Memory/OffsetOf.h>
#include <Common/Memory/AddressOf.h>
#include <Common/Asset/Format/Guid.h>
#include <Common/Asset/Reference.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/IO/Log.h>
#include <Common/Serialization/Deserialize.h>

namespace ngine::Rendering
{
	[[nodiscard]] PURE_LOCALS_AND_POINTERS Renderer& TextureCache::GetRenderer()
	{
		return Memory::GetOwnerFromMember(*this, &Renderer::m_textureCache);
	}

	[[nodiscard]] PURE_LOCALS_AND_POINTERS const Renderer& TextureCache::GetRenderer() const
	{
		return Memory::GetConstOwnerFromMember(*this, &Renderer::m_textureCache);
	}

	static Optional<Threading::Job*> InvalidTextureLoadingFunction(const TextureIdentifier, LogicalDevice&)
	{
		return nullptr;
	}

	// Async job that loads a texture from disk, mip by mip
	// Starts with the lowest resolution mip.
	struct LoadTextureFromDiskJob : public Threading::Job
	{
		enum class LoadStatus : uint8
		{
			AwaitingInitialLoad,
			AwaitingMetadataParsing,
			AwaitingRenderTextureCreation,
			AwaitingInitialStagingBufferMapping,
			AwaitingMipMapLoad,
			AwaitingMipMapStagingBufferMapping,
			AwaitingAsyncFileLoadStart,
			AwaitingAsyncFileLoad,
			AwaitingMipTransfer,
			AwaitingExecutionFinish,
			AwaitingTransferCompletion,
			FinishedTransfer,
			LoadFailed
		};

#define HAS_STAGING_BUFFER (!RENDERER_WEBGPU)

		LoadTextureFromDiskJob(const TextureIdentifier identifier, LogicalDevice& logicalDevice, const Threading::JobPriority priority)
			: Threading::Job(priority)
			, m_identifier(identifier)
			, m_logicalDevice(logicalDevice)
		{
			Assert(logicalDevice.GetRenderer().GetTextureCache().IsRenderTextureLoading(logicalDevice.GetIdentifier(), identifier));
		}
		LoadTextureFromDiskJob(const LoadTextureFromDiskJob&) = delete;
		LoadTextureFromDiskJob& operator=(const LoadTextureFromDiskJob&) = delete;
		LoadTextureFromDiskJob(LoadTextureFromDiskJob&&) = delete;
		LoadTextureFromDiskJob& operator=(LoadTextureFromDiskJob&&) = delete;
		virtual ~LoadTextureFromDiskJob()
		{
			switch (m_status)
			{
				case LoadStatus::LoadFailed:
				{
					m_transferSignalSemaphore.Destroy(m_logicalDevice);
#if HAS_STAGING_BUFFER
					m_stagingBuffer.Destroy(m_logicalDevice, m_logicalDevice.GetDeviceMemoryPool());
#endif
					m_logicalDevice.GetRenderer().GetTextureCache().OnTextureLoadFailed(m_logicalDevice.GetIdentifier(), m_identifier);
					m_ownedRenderTexture.Destroy(m_logicalDevice, m_logicalDevice.GetDeviceMemoryPool());
				}
				break;
				default:
					break;
			}
		}

		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override final
		{
			switch (m_status)
			{
				case LoadStatus::AwaitingExecutionFinish:
				{
					m_status = LoadStatus::AwaitingTransferCompletion;

					{
						// Queue the transfer job on the submission job
						// Specify semaphores to correctly wait for termination
						QueueSubmissionParameters parameters;
						parameters.m_signalSemaphores = m_graphicsCommandBuffer.IsValid() ? ArrayView<const SemaphoreView>{m_transferSignalSemaphore}
						                                                                  : ArrayView<const SemaphoreView>{};
						if (m_graphicsCommandBuffer.IsValid())
						{
							parameters.m_submittedCallback = [this]()
							{
								QueueSubmissionParameters parameters;
								parameters.m_waitSemaphores = ArrayView<const SemaphoreView>{m_transferSignalSemaphore};
								static constexpr EnumFlags<PipelineStageFlags> waitStageMask = PipelineStageFlags::Transfer;
								parameters.m_waitStagesMasks = ArrayView<const EnumFlags<PipelineStageFlags>>{waitStageMask};
								parameters.m_finishedCallback = [this]
								{
									m_status = LoadStatus::FinishedTransfer;
									Queue(System::Get<Threading::JobManager>());
								};
								m_logicalDevice.GetQueueSubmissionJob(QueueFamily::Graphics)
									.Queue(GetPriority(), ArrayView<const EncodedCommandBufferView, uint16>(m_graphicsCommandBuffer), Move(parameters));
							};
						}
						else
						{
							parameters.m_finishedCallback = [this]
							{
								m_status = LoadStatus::FinishedTransfer;
								Queue(System::Get<Threading::JobManager>());
							};
						}
						m_logicalDevice.GetQueueSubmissionJob(QueueFamily::Transfer)
							.Queue(GetPriority(), ArrayView<const EncodedCommandBufferView, uint16>(m_transferCommandBuffer), Move(parameters));
					}
				}
				break;
				case LoadStatus::AwaitingAsyncFileLoad:
				{
					Threading::Job* pJob = m_pAsyncLoadingJob;
					m_pAsyncLoadingJob = nullptr;
					pJob->Queue(thread);
				}
				break;
				case LoadStatus::AwaitingRenderTextureCreation:
				{
					if constexpr (PLATFORM_EMSCRIPTEN)
					{
						// JavaScript proxies texture creation to the window thread
						// We don't want that blocking, so we preemptively queue instead.
						Rendering::Window::QueueOnWindowThread(
							[this]()
							{
								if (LIKELY(CreateRenderTexture(m_allMipsMask, m_currentMipMask)))
								{
									Queue(System::Get<Threading::JobManager>());
								}
								else
								{
									System::Get<Threading::JobManager>().QueueCallback(
										[this](Threading::JobRunnerThread& thread)
										{
											SignalExecutionFinishedAndDestroying(thread);
											delete this;
										},
										GetPriority()
									);
								}
							}
						);
					}
				}
				break;
				default:
					break;
			}
		}

		virtual Result OnExecute(Threading::JobRunnerThread& thread) override final
		{
			switch (m_status)
			{
				case LoadStatus::AwaitingInitialLoad:
				{
					// Query the asset entry of the texture identifier
					const Asset::Guid assetGuid = m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier);
					Asset::Manager& assetManager = System::Get<Asset::Manager>();
					IO::Path assetPath = assetManager.GetAssetPath(assetGuid);
					if (assetPath.IsEmpty())
					{
						// Check if the asset exists in the library, so that we can render thumbnails
						// TODO: This logic should really be moved out of engine into the editor
						const Guid assetTypeGuid = assetManager.GetAssetLibrary().GetAssetTypeGuid(assetGuid);
						if (assetTypeGuid.IsValid())
						{
							const Asset::Identifier importedAssetIdentifier =
								assetManager.Import(Asset::LibraryReference{assetGuid, assetTypeGuid}, Asset::ImportingFlags{});
							if (importedAssetIdentifier.IsValid())
							{
								assetPath = assetManager.GetAssetPath(assetGuid);
							}
						}

						if (assetPath.IsEmpty())
						{
							return COLD_ERROR_LOGIC(
								[this]()
								{
									LogWarning(
										"Texture load failed: Asset {0} with guid {1} not found in database",
										m_identifier.GetIndex(),
										m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
									);
									m_status = LoadStatus::LoadFailed;
									return Result::FinishedAndDelete;
								}
							);
						}
					}

					m_textureAsset.SetMetaDataFilePath(assetPath);

					m_status = LoadStatus::AwaitingAsyncFileLoadStart;
					m_pAsyncLoadingJob = assetManager.RequestAsyncLoadAssetMetadata(
						assetGuid,
						GetPriority(),
						[this](const ConstByteView readView) mutable
						{
							if (UNLIKELY(readView.IsEmpty()))
							{
								return COLD_ERROR_LOGIC(
									[this]()
									{
										LogWarning(
											"Texture load failed: Asset {0} with guid {1} metadata load failure",
											m_identifier.GetIndex(),
											m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
										);
										if (m_status == LoadStatus::AwaitingAsyncFileLoad)
										{
											m_status = LoadStatus::LoadFailed;
											delete this;
										}
										else
										{
											Assert(m_status == LoadStatus::AwaitingAsyncFileLoadStart);
											m_status = LoadStatus::LoadFailed;
										}
										return;
									}
								);
							}

							if (UNLIKELY(!Serialization::DeserializeFromBuffer(
										ConstStringView{
											reinterpret_cast<const char*>(readView.GetData()),
											static_cast<uint32>(readView.GetDataSize() / sizeof(char))
										},
										m_textureAsset
									)))
							{
								return COLD_ERROR_LOGIC(
									[this]()
									{
										LogWarning(
											"Texture load failed: Asset {0} with guid {1} deserialization failure",
											m_identifier.GetIndex(),
											m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
										);
										if (m_status == LoadStatus::AwaitingAsyncFileLoad)
										{
											m_status = LoadStatus::LoadFailed;
											delete this;
										}
										else
										{
											Assert(m_status == LoadStatus::AwaitingAsyncFileLoadStart);
											m_status = LoadStatus::LoadFailed;
										}
										return;
									}
								);
							}

							m_status = LoadStatus::AwaitingMetadataParsing;
							TryQueue(*Threading::JobRunnerThread::GetCurrent());
						}
					);
					if (m_pAsyncLoadingJob != nullptr)
					{
						Assert(m_status != LoadStatus::LoadFailed);
						m_status = LoadStatus::AwaitingAsyncFileLoad;
						return Result::AwaitExternalFinish;
					}
					else if (m_status != LoadStatus::LoadFailed)
					{
						return Result::TryRequeue;
					}
					else
					{
						return Result::FinishedAndDelete;
					}
				}
				case LoadStatus::AwaitingMetadataParsing:
				{
					const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
					const Rendering::TextureAsset::BinaryType desiredBinaryType =
						supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR) ? Rendering::TextureAsset::BinaryType::ASTC
																																															: Rendering::TextureAsset::BinaryType::BC;
					const TextureAsset::BinaryInfo& __restrict textureBinaryInfo = m_textureAsset.GetBinaryAssetInfo(desiredBinaryType);
					TextureAsset::BinaryInfo::MipInfoView mipInfoView = textureBinaryInfo.GetMipInfoView();
					if (UNLIKELY(mipInfoView.IsEmpty()))
					{
						return COLD_ERROR_LOGIC(
							[this]()
							{
								LogWarning(
									"Texture load failed: Asset {0} with guid {1} had no mips",
									m_identifier.GetIndex(),
									m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
								);
								m_status = LoadStatus::LoadFailed;
								return Result::FinishedAndDelete;
							}
						);
					}

					const Math::Vector2ui resolution = m_textureAsset.GetResolution();

					const Rendering::Format format = textureBinaryInfo.GetFormat();
					const FormatInfo& __restrict formatInfo = GetFormatInfo(format);

					{
						const Math::TBoolVector2<uint32> isDivisibleByBlockExtent =
							Math::Mod(resolution, Math::Vector2ui{formatInfo.m_blockExtent.x, formatInfo.m_blockExtent.y}) == Math::Zero;
						if (UNLIKELY(!isDivisibleByBlockExtent.AreAllSet()))
						{
							return COLD_ERROR_LOGIC(
								[this]()
								{
									LogWarning(
										"Texture load failed: Resolution was not divisible by block extent in asset {} with guid {}",
										m_identifier.GetIndex(),
										m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
									);
									m_status = LoadStatus::LoadFailed;
									return Result::FinishedAndDelete;
								}
							);
						}
					}

					const uint16 maximumMipCount = Rendering::MipMask::FromSizeAllToLargest(resolution).GetSize();
					m_maximumMipCount = maximumMipCount;

					// Start by validating that the mips can be loaded
					MipMask validMipsMask;

					const uint8 blockDataSize = formatInfo.m_blockDataSize;
					for (const TextureAsset::MipInfo& __restrict mipInfo : mipInfoView)
					{
						Math::Vector2ui mipSize = resolution >> mipInfo.level;
						// TODO: Enable the below for all platforms
						// Requires patching the texture compiler to correct this case.
						const Math::TBoolVector2<uint32> isDivisibleByBlockExtent =
							Math::Mod(mipSize, Math::Vector2ui{formatInfo.m_blockExtent.x, formatInfo.m_blockExtent.y}) == Math::Zero;
						if (UNLIKELY(!isDivisibleByBlockExtent.AreAllSet()))
						{
							if (validMipsMask.GetSize() > 0)
							{
								// Quietly ignore final mips that aren't divisible by the block extent
								break;
							}
							else
							{
								// Error out for other invalid mips higher in the chain
								return COLD_ERROR_LOGIC(
									[this]()
									{
										LogWarning(
											"Encountered invalid mip not divisible by block extent in texture asset {0} with guid {1}",
											m_identifier.GetIndex(),
											m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
										);
										m_status = LoadStatus::LoadFailed;
										return Result::FinishedAndDelete;
									}
								);
							}
						}
						mipSize = Math::Max(mipSize, Math::Vector2ui{formatInfo.m_blockExtent.x, formatInfo.m_blockExtent.y});

						const Math::Vector2ui mipBlockCount = formatInfo.GetBlockCount(mipSize);
						const size expectedMipDataSize = mipBlockCount.GetComponentLength() * blockDataSize;
						if (LIKELY(mipInfo.m_size >= expectedMipDataSize))
						{
							const uint16 relativeLevel = maximumMipCount - mipInfo.level - 1;
							validMipsMask |= MipMask::FromIndex(relativeLevel);
						}
						else
						{
							return COLD_ERROR_LOGIC(
								[this]()
								{
									LogWarning(
										"Encountered invalid mip in texture asset {0} with guid {1}",
										m_identifier.GetIndex(),
										m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
									);
									m_status = LoadStatus::LoadFailed;
									return Result::FinishedAndDelete;
								}
							);
						}
					}

					const uint16 totalValidMipCount = validMipsMask.GetSize();
					if (UNLIKELY_ERROR(totalValidMipCount == 0))
					{
						return COLD_ERROR_LOGIC(
							[this]()
							{
								LogWarning(
									"Texture load failed: Asset {0} with guid {1} had no valid mips",
									m_identifier.GetIndex(),
									m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
								);
								m_status = LoadStatus::LoadFailed;
								return Result::FinishedAndDelete;
							}
						);
					}

					m_allMipsMask = validMipsMask;

					Rendering::TextureCache& textureCache = m_logicalDevice.GetRenderer().GetTextureCache();
					const MipMask initiallyRequestedMips = textureCache.GetRequestedTextureMips(m_logicalDevice.GetIdentifier(), m_identifier);
					MipMask requestedMips = initiallyRequestedMips & validMipsMask;

					// Ensure that we load all mips in the range
					MipMask::StoredType firstRequestedMipIndex = *requestedMips.GetFirstIndex();
					MipMask::StoredType lastRequestedMipIndex = *requestedMips.GetLastIndex();
					requestedMips = MipMask::FromRange(MipRange(firstRequestedMipIndex, lastRequestedMipIndex - firstRequestedMipIndex + 1));

					if (const Optional<RenderTexture*> pExistingTexture = textureCache.GetRenderTexture(m_logicalDevice.GetIdentifier(), m_identifier))
					{
						m_textureView = *pExistingTexture;
						m_pRenderTexture = pExistingTexture;

						const MipMask loadedMips = pExistingTexture->GetLoadedMipMask();

						// Ensure that we load all mips in the range between our request and currently loaded ones
						firstRequestedMipIndex = Math::Min(firstRequestedMipIndex, *loadedMips.GetFirstIndex());
						lastRequestedMipIndex = Math::Max(lastRequestedMipIndex, *loadedMips.GetLastIndex());

						requestedMips = MipMask::FromRange(MipRange(firstRequestedMipIndex, lastRequestedMipIndex - firstRequestedMipIndex + 1));

						// Don't reload mips we already have
						Assert(requestedMips.AreAnySet());
						requestedMips &= ~loadedMips;
						if (!requestedMips.AreAnySet())
						{
							textureCache.OnTextureLoadFinished(m_logicalDevice.GetIdentifier(), m_identifier);
							m_status = LoadStatus::FinishedTransfer;
							return Result::FinishedAndDelete;
						}
					}

					if (!requestedMips.AreAnySet())
					{
						// Requested mip wasn't available, pick the highest resolution one
						const uint16 highestMipMaskIndex = *validMipsMask.GetLastIndex();
						requestedMips = MipMask::FromIndex(highestMipMaskIndex);
					}

					const uint16 totalRequestedMipCount = requestedMips.GetSize();
					m_totalRequestedMipCount = totalRequestedMipCount;

					const MipMask currentMipMask = MipMask::FromIndex(*requestedMips.GetFirstIndex());
					m_currentMipMask = currentMipMask;
					m_remainingMips = requestedMips & ~currentMipMask;

					m_status = LoadStatus::AwaitingRenderTextureCreation;
					if (!m_textureView.IsValid())
					{
						if constexpr (PLATFORM_EMSCRIPTEN)
						{
							return Result::AwaitExternalFinish;
						}
						else if (UNLIKELY_ERROR(!CreateRenderTexture(validMipsMask, currentMipMask)))
						{
							return Result::FinishedAndDelete;
						}
					}
				}
					[[fallthrough]];
				case LoadStatus::AwaitingRenderTextureCreation:
				{
#if HAS_STAGING_BUFFER
					const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
					const Rendering::TextureAsset::BinaryType desiredBinaryType =
						supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR) ? Rendering::TextureAsset::BinaryType::ASTC
																																															: Rendering::TextureAsset::BinaryType::BC;
					const TextureAsset::BinaryInfo& __restrict textureBinaryInfo = m_textureAsset.GetBinaryAssetInfo(desiredBinaryType);

					const Rendering::Format format = textureBinaryInfo.GetFormat();
					const FormatInfo& __restrict formatInfo = GetFormatInfo(format);

					const Math::Vector2ui resolution = m_textureAsset.GetResolution();

					const Math::Vector2ui mipSize =
						Math::Max(resolution >> GetCurrentRelativeMipIndex(), Math::Vector2ui{formatInfo.m_blockExtent.x, formatInfo.m_blockExtent.y});

					const uint32 bytesPerLayer = formatInfo.GetBytesPerLayer(mipSize);
					const uint32 arraySize = m_textureAsset.GetArraySize();

					// TODO: Replace this with a global streaming buffer? (max 256MB)
					// That way we can keep it mapped
					m_stagingBuffer = StagingBuffer(
						m_logicalDevice,
						m_logicalDevice.GetPhysicalDevice(),
						m_logicalDevice.GetDeviceMemoryPool(),
						bytesPerLayer * arraySize,
						StagingBuffer::Flags::TransferSource
					);
					if (UNLIKELY(!m_stagingBuffer.IsValid()))
					{
						return COLD_ERROR_LOGIC(
							[this]()
							{
								LogWarning(
									"Texture load failed: Asset {0} with guid {1} staging buffer creation failure",
									m_identifier.GetIndex(),
									m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
								);
								m_status = LoadStatus::LoadFailed;
								return Result::FinishedAndDelete;
							}
						);
					}

					m_status = LoadStatus::AwaitingInitialStagingBufferMapping;
					const bool executedAsynchronously = m_stagingBuffer.MapToHostMemoryAsync(
						m_logicalDevice,
						Math::Range<size>::Make(0, m_stagingBuffer.GetSize()),
						Buffer::MapMemoryFlags::Write | Buffer::MapMemoryFlags::KeepMapped,
						[this]([[maybe_unused]] const Buffer::MapMemoryStatus status, const ByteView data, const bool executedAsynchronously)
						{
							Assert(status == Buffer::MapMemoryStatus::Success);
							m_stagingBufferMappedData = data;
							if (executedAsynchronously)
							{
								Queue(System::Get<Threading::JobManager>());
							}
						}
					);
					if (executedAsynchronously)
					{
						return Result::AwaitExternalFinish;
					}
#endif
				}
					[[fallthrough]];
				case LoadStatus::AwaitingInitialStagingBufferMapping:
				{
#if HAS_STAGING_BUFFER
					if (UNLIKELY(m_stagingBufferMappedData.IsEmpty()))
					{
						return COLD_ERROR_LOGIC(
							[this]()
							{
								LogWarning(
									"Texture load failed: Asset {0} with guid {1} staging buffer mapping failure",
									m_identifier.GetIndex(),
									m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
								);
								m_status = LoadStatus::LoadFailed;
								return Result::FinishedAndDelete;
							}
						);
					}
#endif

					const Asset::Manager& assetDatabase = System::Get<Asset::Manager>();
					const Asset::Guid assetGuid = m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier);

					const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
					const Rendering::TextureAsset::BinaryType desiredBinaryType =
						supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR) ? Rendering::TextureAsset::BinaryType::ASTC
																																															: Rendering::TextureAsset::BinaryType::BC;
					const TextureAsset::BinaryInfo& __restrict textureBinaryInfo = m_textureAsset.GetBinaryAssetInfo(desiredBinaryType);
					TextureAsset::BinaryInfo::MipInfoView mipInfoView = textureBinaryInfo.GetMipInfoView();

					const TextureAsset::MipInfo& __restrict loadingMip = mipInfoView[GetCurrentRelativeMipIndex()];

#if !HAS_STAGING_BUFFER
					m_stagingVector.Resize(loadingMip.m_size, Memory::Uninitialized);
#endif

					m_status = LoadStatus::AwaitingAsyncFileLoadStart;
					m_pAsyncLoadingJob = assetDatabase.RequestAsyncLoadAssetPath(
						assetGuid,
						m_textureAsset.GetBinaryFilePath(desiredBinaryType),
						GetPriority(),
						[this, expectedSize = loadingMip.m_size](const ConstByteView readView) mutable
						{
							LogErrorIf(
								readView.GetDataSize() > expectedSize,
								"Binary size mismatch when reading texture data from asset {0}",
								m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
							);

#if HAS_STAGING_BUFFER
							m_stagingBuffer.UnmapFromHostMemory(m_logicalDevice);
#endif
							if (UNLIKELY(readView.GetDataSize() != expectedSize))
							{
								return COLD_ERROR_LOGIC(
									[this]()
									{
										LogWarning(
											"Texture load failed: Asset {0} with guid {1} first mip binary data load failure",
											m_identifier.GetIndex(),
											m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
										);
										if (m_status == LoadStatus::AwaitingAsyncFileLoad)
										{
											m_status = LoadStatus::LoadFailed;
											delete this;
										}
										else
										{
											Assert(m_status == LoadStatus::AwaitingAsyncFileLoadStart);
											m_status = LoadStatus::LoadFailed;
										}
									}
								);
							}

							m_status = LoadStatus::AwaitingMipTransfer;
							TryQueue(*Threading::JobRunnerThread::GetCurrent());
						},
#if HAS_STAGING_BUFFER
						m_stagingBufferMappedData,
#else
						m_stagingVector.GetView(),
#endif
						Math::Range<size>::MakeStartToEnd(loadingMip.m_offset, loadingMip.m_offset + loadingMip.m_size - 1)
					);
					if (m_pAsyncLoadingJob != nullptr)
					{
						Assert(m_status != LoadStatus::LoadFailed);
						m_status = LoadStatus::AwaitingAsyncFileLoad;
						return Result::AwaitExternalFinish;
					}
					else if (m_status != LoadStatus::LoadFailed)
					{
						return Result::TryRequeue;
					}
					else
					{
						return Result::FinishedAndDelete;
					}
				}
				case LoadStatus::AwaitingMipTransfer:
				{
					const uint16 currentMipIndex = GetCurrentRelativeMipIndex();

					const Math::Vector2ui mipSize = m_textureAsset.GetResolution() >> currentMipIndex;

					const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
					const Rendering::TextureAsset::BinaryType desiredBinaryType =
						supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR) ? Rendering::TextureAsset::BinaryType::ASTC
																																															: Rendering::TextureAsset::BinaryType::BC;
					const TextureAsset::BinaryInfo& __restrict textureBinaryInfo = m_textureAsset.GetBinaryAssetInfo(desiredBinaryType);

					const Rendering::Format format = textureBinaryInfo.GetFormat();
					const Rendering::FormatInfo& __restrict formatInfo = Rendering::GetFormatInfo(format);

					const Math::Vector2ui bytesPerDimension = formatInfo.GetBytesPerDimension(mipSize);

					// Copy the mip from the staging buffer to the image
					Array<BufferImageCopy, 1> copyRegions = {BufferImageCopy{
						0,
						bytesPerDimension,
						formatInfo.GetBlockCount(mipSize),
						formatInfo.m_blockExtent,
						SubresourceLayers{ImageAspectFlags::Color, currentMipIndex, ArrayRange{0, m_textureAsset.GetArraySize()}},
						Math::Vector3i{0, 0, 0},
						Math::Vector3ui{mipSize.x, mipSize.y, 1}
					}};

#if HAS_STAGING_BUFFER
					Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
					m_transferCommandBuffer = UnifiedCommandBuffer(
						m_logicalDevice,
						engineThread.GetRenderData().GetCommandPool(m_logicalDevice.GetIdentifier(), Rendering::QueueFamily::Transfer),
						m_logicalDevice.GetCommandQueue(Rendering::QueueFamily::Transfer)
					);

					const CommandEncoderView transferCommandEncoder = m_transferCommandBuffer.BeginEncoding(m_logicalDevice);

					{
						// Transition the image from creation to transfer destination
						BarrierCommandEncoder barrierCommandEncoder = transferCommandEncoder.BeginBarrier();
						barrierCommandEncoder.TransitionImageLayout(
							PipelineStageFlags::Transfer,
							AccessFlags::TransferWrite,
							ImageLayout::TransferDestinationOptimal,
							*m_pRenderTexture,
							Rendering::ImageSubresourceRange{
								ImageAspectFlags::Color,
								MipRange{currentMipIndex, 1},
								ArrayRange {
									0,
									m_textureAsset.GetArraySize()
								}
							}
						);
					}

					// Record the copy of the mip to the image
					{
						BlitCommandEncoder blitCommandEncoder = transferCommandEncoder.BeginBlit();
						blitCommandEncoder
							.RecordCopyBufferToImage(m_stagingBuffer, *m_pRenderTexture, ImageLayout::TransferDestinationOptimal, copyRegions.GetView());
					}

					m_pCommandBufferThread = &thread;
					// Handle difference between a unified graphics and transfer queue
					// Separate queues will entail needing ownership transfer via a pipeline barrier
					const bool isUnifiedGraphicsAndTransferQueue = m_logicalDevice.GetCommandQueue(QueueFamily::Graphics) ==
					                                               m_logicalDevice.GetCommandQueue(QueueFamily::Transfer);
					if (isUnifiedGraphicsAndTransferQueue)
					{
						BarrierCommandEncoder barrierCommandEncoder = transferCommandEncoder.BeginBarrier();
						barrierCommandEncoder.TransitionImageLayout(
							PipelineStageFlags::FragmentShader,
							AccessFlags::ShaderRead,
							ImageLayout::ShaderReadOnlyOptimal,
							*m_pRenderTexture,
							Rendering::ImageSubresourceRange{
								ImageAspectFlags::Color,
								MipRange{currentMipIndex, 1},
								ArrayRange {
									0,
									m_textureAsset.GetArraySize()
								}
							}
						);
					}
					else
					{
						{
							BarrierCommandEncoder barrierCommandEncoder = transferCommandEncoder.BeginBarrier();
							barrierCommandEncoder.TransitionImageLayout(
								PipelineStageFlags::BottomOfPipe,
								AccessFlags::None,
								ImageLayout::ShaderReadOnlyOptimal,
								m_logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics),
								*m_pRenderTexture,
								Rendering::ImageSubresourceRange{
									ImageAspectFlags::Color,
									MipRange{currentMipIndex, 1},
									ArrayRange {
										0,
										m_textureAsset.GetArraySize()
									}
								}
							);
						}

						// Create a graphics command buffer, and record ownership transfer of the resource from the transfer to graphics queue
						m_graphicsCommandBuffer = UnifiedCommandBuffer(
							m_logicalDevice,
							engineThread.GetRenderData().GetCommandPool(m_logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics),
							m_logicalDevice.GetCommandQueue(Rendering::QueueFamily::Graphics)
						);
						CommandEncoderView graphicsCommandEncoder = m_graphicsCommandBuffer.BeginEncoding(m_logicalDevice);

						{
							BarrierCommandEncoder barrierCommandEncoder = graphicsCommandEncoder.BeginBarrier();
							barrierCommandEncoder.TransitionImageLayout(
								PipelineStageFlags::FragmentShader,
								AccessFlags::ShaderRead,
								ImageLayout::ShaderReadOnlyOptimal,
								m_logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics),
								m_ownedRenderTexture,
								Rendering::ImageSubresourceRange{
									ImageAspectFlags::Color,
									MipRange{currentMipIndex, 1},
									ArrayRange {
										0,
										m_textureAsset.GetArraySize()
									}
								}
							);
						}
					}

					m_transferCommandBuffer.StopEncoding();
					if (m_graphicsCommandBuffer.IsValid())
					{
						m_graphicsCommandBuffer.StopEncoding();
					}

					m_status = LoadStatus::AwaitingExecutionFinish;
					return Result::AwaitExternalFinish;
#else
					const BufferImageCopy& __restrict region = copyRegions[0];
					WGPUImageCopyTexture webGpuImageCopyTexture
					{
#if !RENDERER_WEBGPU_DAWN
						nullptr,
#endif
							nullptr, region.m_imageSubresource.mipLevel,
							WGPUOrigin3D{
								(uint32)region.m_imageOffset.x,
								(uint32)region.m_imageOffset.y,
								(uint32)region.m_imageOffset.z + region.m_imageSubresource.arrayLayerRanges.GetIndex()
							},
							ConvertImageAspectFlags(region.m_imageSubresource.aspectMask),
					};
					const WGPUTextureDataLayout
						webGpuTextureDataLayout{nullptr, region.m_bufferOffset, region.m_bufferBytesPerDimension.x, region.m_blockCountPerDimension.y};
					const WGPUExtent3D webGpuExtent = WGPUExtent3D{
						region.m_imageExtent.x,
						region.m_imageExtent.y,
						Math::Max(region.m_imageExtent.z, region.m_imageSubresource.arrayLayerRanges.GetCount())
					};

					m_status = LoadStatus::FinishedTransfer;
					m_logicalDevice.GetQueueSubmissionJob(QueueFamily::Graphics)
						.QueueCallback(
							[this, webGpuImageCopyTexture, webGpuTextureDataLayout, webGpuExtent]() mutable
							{
								webGpuImageCopyTexture.texture = m_textureView;
								const CommandQueueView commandQueue = m_logicalDevice.GetCommandQueue(QueueFamily::Transfer);
								wgpuQueueWriteTexture(
									commandQueue,
									&webGpuImageCopyTexture,
									m_stagingVector.GetData(),
									m_stagingVector.GetDataSize(),
									&webGpuTextureDataLayout,
									&webGpuExtent
								);

								wgpuQueueOnSubmittedWorkDone(
									commandQueue,
									[](const WGPUQueueWorkDoneStatus, void* pUserData)
									{
										LoadTextureFromDiskJob& job = *static_cast<LoadTextureFromDiskJob*>(pUserData);
										job.Queue(System::Get<Threading::JobManager>());
									},
									this
								);
							}
						);
					return Result::AwaitExternalFinish;
#endif
				}
				case LoadStatus::AwaitingMipMapLoad:
				{
#if HAS_STAGING_BUFFER
					const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
					const Rendering::TextureAsset::BinaryType desiredBinaryType =
						supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR) ? Rendering::TextureAsset::BinaryType::ASTC
																																															: Rendering::TextureAsset::BinaryType::BC;
					const TextureAsset::BinaryInfo& __restrict textureBinaryInfo = m_textureAsset.GetBinaryAssetInfo(desiredBinaryType);

					const Rendering::Format format = textureBinaryInfo.GetFormat();
					const uint16 currentMipIndex = GetCurrentRelativeMipIndex();

					const Rendering::FormatInfo& __restrict formatInfo = Rendering::GetFormatInfo(format);
					const Math::Vector2ui mipSize = Math::Max(
						m_textureAsset.GetResolution() >> currentMipIndex,
						Math::Vector2ui{formatInfo.m_blockExtent.x, formatInfo.m_blockExtent.y}
					);

					const uint32 bytesPerLayer = formatInfo.GetBytesPerLayer(mipSize);
					const uint32 arraySize = m_textureAsset.GetArraySize();

					m_stagingBuffer = StagingBuffer(
						m_logicalDevice,
						m_logicalDevice.GetPhysicalDevice(),
						m_logicalDevice.GetDeviceMemoryPool(),
						bytesPerLayer * arraySize,
						StagingBuffer::Flags::TransferSource
					);

					m_status = LoadStatus::AwaitingMipMapStagingBufferMapping;
					const bool executedAsynchronously = m_stagingBuffer.MapToHostMemoryAsync(
						m_logicalDevice,
						Math::Range<size>::Make(0, m_stagingBuffer.GetSize()),
						Buffer::MapMemoryFlags::Write | Buffer::MapMemoryFlags::KeepMapped,
						[this]([[maybe_unused]] const Buffer::MapMemoryStatus status, const ByteView data, const bool executedAsynchronously)
						{
							Assert(status == Buffer::MapMemoryStatus::Success);
							m_stagingBufferMappedData = data;
							if (executedAsynchronously)
							{
								Queue(System::Get<Threading::JobManager>());
							}
						}
					);
					if (executedAsynchronously)
					{
						return Result::AwaitExternalFinish;
					}
#endif
				}
					[[fallthrough]];
				case LoadStatus::AwaitingMipMapStagingBufferMapping:
				{
#if HAS_STAGING_BUFFER
					if (UNLIKELY(m_stagingBufferMappedData.IsEmpty()))
					{
						return COLD_ERROR_LOGIC(
							[this]()
							{
								LogWarning(
									"Texture load failed: Asset {0} with guid {1} staging buffer mapping failure",
									m_identifier.GetIndex(),
									m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
								);
								m_status = LoadStatus::LoadFailed;
								return Result::FinishedAndDelete;
							}
						);
					}
#endif

					const Asset::Guid assetGuid = m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier);
					const Asset::Manager& assetDatabase = System::Get<Asset::Manager>();

					const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
					const Rendering::TextureAsset::BinaryType desiredBinaryType =
						supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR) ? Rendering::TextureAsset::BinaryType::ASTC
																																															: Rendering::TextureAsset::BinaryType::BC;
					const TextureAsset::BinaryInfo& __restrict textureBinaryInfo = m_textureAsset.GetBinaryAssetInfo(desiredBinaryType);
					TextureAsset::BinaryInfo::MipInfoView mipInfoView = textureBinaryInfo.GetMipInfoView();

					const uint16 currentMipIndex = GetCurrentRelativeMipIndex();
					const TextureAsset::MipInfo& __restrict loadingMip = mipInfoView[currentMipIndex];

#if !HAS_STAGING_BUFFER
					m_stagingVector.Resize(loadingMip.m_size, Memory::Uninitialized);
#endif

					m_status = LoadStatus::AwaitingAsyncFileLoadStart;
					m_pAsyncLoadingJob = assetDatabase.RequestAsyncLoadAssetPath(
						assetGuid,
						m_textureAsset.GetBinaryFilePath(desiredBinaryType),
						GetPriority(),
						[this, expectedSize = loadingMip.m_size](const ConstByteView readView) mutable
						{
							LogErrorIf(
								readView.GetDataSize() > expectedSize,
								"Binary size mismatch when reading texture data from asset {0}",
								m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
							);

#if HAS_STAGING_BUFFER
							m_stagingBuffer.UnmapFromHostMemory(m_logicalDevice);
#endif

							if (UNLIKELY(readView.GetDataSize() != expectedSize))
							{
								return COLD_ERROR_LOGIC(
									[this]()
									{
										LogWarning(
											"Texture load failed: Asset {0} with guid {1} secondary mip binary data load failure",
											m_identifier.GetIndex(),
											m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
										);
										if (m_status == LoadStatus::AwaitingAsyncFileLoad)
										{
											m_status = LoadStatus::LoadFailed;
											delete this;
										}
										else
										{
											Assert(m_status == LoadStatus::AwaitingAsyncFileLoadStart);
											m_status = LoadStatus::LoadFailed;
										}
									}
								);
							}

							m_status = LoadStatus::AwaitingMipTransfer;
							TryQueue(*Threading::JobRunnerThread::GetCurrent());
						},
#if HAS_STAGING_BUFFER
						m_stagingBufferMappedData,
#else
						m_stagingVector.GetView(),
#endif
						Math::Range<size>::MakeStartToEnd(loadingMip.m_offset, loadingMip.m_offset + loadingMip.m_size - 1)
					);
					if (m_pAsyncLoadingJob != nullptr)
					{
						Assert(m_status != LoadStatus::LoadFailed);
						m_status = LoadStatus::AwaitingAsyncFileLoad;
						return Result::AwaitExternalFinish;
					}
					else if (m_status != LoadStatus::LoadFailed)
					{
						return Result::TryRequeue;
					}
					else
					{
						return Result::FinishedAndDelete;
					}
				}
				case LoadStatus::FinishedTransfer:
				{
					if (m_ownedRenderTexture.IsValid())
					{
						// First mip was loaded, assign the initial render texture
						RenderTexture previousTexture;
						TextureCache& textureCache = m_logicalDevice.GetRenderer().GetTextureCache();
						textureCache.AssignRenderTexture(
							m_logicalDevice.GetIdentifier(),
							m_identifier,
							Move(m_ownedRenderTexture),
							previousTexture,
							LoadedTextureFlags{}
						);
						m_pRenderTexture = textureCache.GetRenderTexture(m_logicalDevice.GetIdentifier(), m_identifier);
						if (previousTexture.IsValid())
						{
							Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
							engineThread.GetRenderData().DestroyImage(m_logicalDevice.GetIdentifier(), Move(previousTexture));
						}
					}
					else
					{
						// Texture had already been created but a new mip was loaded
						m_logicalDevice.GetRenderer()
							.GetTextureCache()
							.ChangeRenderTextureAvailableMips(m_logicalDevice.GetIdentifier(), m_identifier, m_currentMipMask, LoadedTextureFlags{});
					}

#if HAS_STAGING_BUFFER
					thread.QueueCallbackFromThread(
						Threading::JobPriority::DeallocateResourcesMin,
						[stagingBuffer = Move(m_stagingBuffer), &logicalDevice = m_logicalDevice](Threading::JobRunnerThread&) mutable
						{
							stagingBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
						}
					);
#endif

					if (m_pCommandBufferThread != nullptr)
					{
						m_pCommandBufferThread->QueueExclusiveCallbackFromAnyThread(
							Threading::JobPriority::DeallocateResourcesMin,
							[transferCommandBuffer = Move(m_transferCommandBuffer),
						   graphicsCommandBuffer = Move(m_graphicsCommandBuffer),
						   &logicalDevice = m_logicalDevice](Threading::JobRunnerThread& thread) mutable
							{
								Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
								transferCommandBuffer.Destroy(
									logicalDevice,
									engineThread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Transfer)
								);
								graphicsCommandBuffer.Destroy(
									logicalDevice,
									engineThread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics)
								);
							}
						);
					}

					if (m_remainingMips.IsEmpty())
					{
						m_logicalDevice.GetRenderer().GetTextureCache().OnTextureLoadFinished(m_logicalDevice.GetIdentifier(), m_identifier);

						// All mips loaded, destroy resources and delete this job
						m_transferSignalSemaphore.Destroy(m_logicalDevice);
						return Result::FinishedAndDelete;
					}
					else
					{
						const MipMask currentMipMask = MipMask::FromIndex(*m_remainingMips.GetFirstIndex());
						m_currentMipMask = currentMipMask;
						m_remainingMips &= ~currentMipMask;

						SetPriority(1.f - ((float)m_remainingMips.GetSize() / float(m_totalRequestedMipCount)));
						m_status = LoadStatus::AwaitingMipMapLoad;
						return Result::TryRequeue;
					}
				}
				case LoadStatus::AwaitingAsyncFileLoadStart:
				case LoadStatus::AwaitingAsyncFileLoad:
				case LoadStatus::AwaitingExecutionFinish:
				case LoadStatus::AwaitingTransferCompletion:
				case LoadStatus::LoadFailed:
					ExpectUnreachable();
			}
			ExpectUnreachable();
		}
	protected:
		[[nodiscard]] bool CreateRenderTexture(const MipMask allMipsMask, const MipMask loadingMipMask)
		{
			const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
			const Rendering::TextureAsset::BinaryType desiredBinaryType =
				supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR) ? Rendering::TextureAsset::BinaryType::ASTC
																																													: Rendering::TextureAsset::BinaryType::BC;
			const TextureAsset::BinaryInfo& __restrict textureBinaryInfo = m_textureAsset.GetBinaryAssetInfo(desiredBinaryType);

			const Rendering::Format format = textureBinaryInfo.GetFormat();

			const Math::Vector2ui resolution = m_textureAsset.GetResolution();

			// Create the render texture
			m_ownedRenderTexture = RenderTexture(
				m_logicalDevice,
				m_logicalDevice.GetPhysicalDevice(),
				format,
				SampleCount::One,
				m_textureAsset.GetFlags(),
				Math::Vector3ui{resolution.x, resolution.y, 1u},
				m_textureAsset.GetUsageFlags(),
				ImageLayout::Undefined,
				allMipsMask,
				loadingMipMask,
				m_textureAsset.GetArraySize()
			);
			m_pRenderTexture = m_ownedRenderTexture;
			if (UNLIKELY(!m_ownedRenderTexture.IsValid()))
			{
				return COLD_ERROR_LOGIC(
					[this]()
					{
						LogWarning(
							"Texture load failed: Asset {0} with guid {1} render texture creation failure",
							m_identifier.GetIndex(),
							m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
						);
						m_status = LoadStatus::LoadFailed;
						return false;
					}
				);
			}

#if RENDERER_OBJECT_DEBUG_NAMES
			const Asset::Manager& assetManager = System::Get<Asset::Manager>();
			String assetName = assetManager.VisitAssetEntry(
				m_textureAsset.GetGuid(),
				[](const Optional<const Asset::DatabaseEntry*> pAssetEntry) -> String
				{
					Assert(pAssetEntry.IsValid());
					if (LIKELY(pAssetEntry.IsValid()))
					{
						return pAssetEntry->GetName();
					}
					else
					{
						return {};
					}
				}
			);
			m_ownedRenderTexture.SetDebugName(m_logicalDevice, Move(assetName));
#endif

			m_textureView = m_ownedRenderTexture;
			return true;
		}

		void SetPriority(const float ratio)
		{
			// Dynamically set job system priority depending on how many mips have been loaded
			// This gives all textures a fair chance of loading in the same mips before one completes.
			Job::SetPriority(
				Threading::GetJobPriorityRange(ratio, Threading::JobPriority::LoadTextureFirstMip, Threading::JobPriority::LoadTextureLastMip)
			);
		}

		//! Returns the current mip index relative to resolution, where the first bit is the full resolution
		[[nodiscard]] uint16 GetCurrentRelativeMipIndex() const
		{
			return (uint16)m_currentMipMask.GetRange(m_maximumMipCount).GetIndex();
		}
	protected:
		LoadStatus m_status = LoadStatus::AwaitingInitialLoad;
		UnifiedCommandBuffer m_graphicsCommandBuffer;
		UnifiedCommandBuffer m_transferCommandBuffer;
		Threading::JobRunnerThread* m_pCommandBufferThread = nullptr;
#if HAS_STAGING_BUFFER
		StagingBuffer m_stagingBuffer;
		ByteView m_stagingBufferMappedData;
#else
		Vector<ByteType, size> m_stagingVector;
#endif
		MipMask m_remainingMips{0};
		MipMask m_allMipsMask;
		//! Mip mask containing the currently loaded mip index
		MipMask m_currentMipMask;
		uint16 m_maximumMipCount = 0u;
		uint16 m_totalRequestedMipCount = 0u;

		const TextureIdentifier m_identifier;
		TextureAsset m_textureAsset;
		LogicalDevice& m_logicalDevice;
		Semaphore m_transferSignalSemaphore = Semaphore(m_logicalDevice);
		RenderTexture m_ownedRenderTexture;
		Optional<RenderTexture*> m_pRenderTexture;
		ImageView m_textureView;

		Threading::Job* m_pAsyncLoadingJob = nullptr;
	};

	TextureCache::TextureCache()
	{
		RegisterAssetModifiedCallback(System::Get<Asset::Manager>());

		System::Get<Engine>().OnRendererInitialized.Add(
			this,
			[](TextureCache& cache)
			{
				cache.GetRenderer().OnLogicalDeviceCreated.Add(cache, &TextureCache::OnLogicalDeviceCreated);
			}
		);
	}

	TextureCache::~TextureCache() = default;

	[[nodiscard]] inline UniquePtr<RenderTexture>
	CreateDummyTexture(LogicalDevice& logicalDevice, const uint8 arraySize, const ImageFlags flags)
	{
		Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
		const CommandPoolView commandPool =
			thread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics);
		UnifiedCommandBuffer commandBuffer(logicalDevice, commandPool, logicalDevice.GetCommandQueue(Rendering::QueueFamily::Graphics));
		CommandEncoderView commandEncoder = commandBuffer.BeginEncoding(logicalDevice);

		StagingBuffer stagingBuffer;
		UniquePtr<RenderTexture> result = UniquePtr<RenderTexture>::Make(
			RenderTexture::Dummy,
			logicalDevice,
			commandEncoder,
			stagingBuffer,
			flags,
			UsageFlags::TransferDestination | UsageFlags::Sampled,
			arraySize,
			"#FFFFFF00"_color
		);

		EncodedCommandBufferView encodedCommandBuffer = commandBuffer.StopEncoding();

		Threading::Job& destroyBufferJob = Threading::CreateCallback(
			[stagingBuffer = Move(stagingBuffer), commandBuffer = Move(commandBuffer), commandPool, &logicalDevice, &commandPoolThread = thread](
				Threading::JobRunnerThread& thread
			) mutable
			{
				if (stagingBuffer.IsValid())
				{
					stagingBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
				}

				if (&thread != &commandPoolThread)
				{
					commandPoolThread.QueueExclusiveCallbackFromAnyThread(
						Threading::JobPriority::DeallocateResourcesMin,
						[commandBuffer = Move(commandBuffer), commandPool, &logicalDevice](Threading::JobRunnerThread&) mutable
						{
							commandBuffer.Destroy(logicalDevice, commandPool);
						}
					);
				}
				else
				{
					commandBuffer.Destroy(logicalDevice, commandPool);
				}
			},
			Threading::JobPriority::DeallocateResourcesMin
		);

		QueueSubmissionParameters parameters;
		parameters.m_finishedCallback = [&destroyBufferJob]
		{
			destroyBufferJob.Queue(System::Get<Threading::JobManager>());
		};

		logicalDevice.GetQueueSubmissionJob(QueueFamily::Graphics)
			.Queue(
				Threading::JobPriority::CoreRenderStageResources,
				ArrayView<const EncodedCommandBufferView, uint16>(encodedCommandBuffer),
				Move(parameters)
			);

		return result;
	}

	void TextureCache::OnLogicalDeviceCreated(LogicalDevice& logicalDevice)
	{
		UniquePtr<PerLogicalDeviceData>& pDeviceData = m_perLogicalDeviceData[logicalDevice.GetIdentifier()];
		pDeviceData.CreateInPlace();

		PerLogicalDeviceData& deviceData = *pDeviceData;
		deviceData.m_pDummyTexture = CreateDummyTexture(logicalDevice, 1, ImageFlags{});
		deviceData.m_pDummyTextureCube = CreateDummyTexture(logicalDevice, 6, ImageFlags::Cubemap);

		const EnumFlags<Rendering::PhysicalDeviceFeatures> physicalDeviceFeatures = logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
		const bool supportsDynamicTextureSampling = physicalDeviceFeatures.AreAllSet(
			Rendering::PhysicalDeviceFeatures::PartiallyBoundDescriptorBindings |
			Rendering::PhysicalDeviceFeatures::UpdateDescriptorSampleImageAfterBind |
			Rendering::PhysicalDeviceFeatures::NonUniformImageArrayIndexing | Rendering::PhysicalDeviceFeatures::RuntimeDescriptorArrays |
			Rendering::PhysicalDeviceFeatures::BufferDeviceAddress | Rendering::PhysicalDeviceFeatures::AccelerationStructure
		);

		if (supportsDynamicTextureSampling)
		{
			deviceData.m_texturesDescriptorSetLayout = DescriptorSetLayout(
				logicalDevice,
				Array{
					DescriptorSetLayout::Binding::MakeSampledImage(
						0,
						ShaderStage::All,
						SampledImageType::Float,
						ImageMappingType::TwoDimensional,
						TextureIdentifier::MaximumCount
					),
					DescriptorSetLayout::Binding::MakeSampler(1, ShaderStage::All, SamplerBindingType::Filtering, TextureIdentifier::MaximumCount)
				},
				DescriptorSetLayout::Flags::UpdateAfterBind,
				Array{
					EnumFlags<DescriptorSetLayout::Binding::Flags>{
						DescriptorSetLayout::Binding::Flags::UpdateAfterBind | DescriptorSetLayout::Binding::Flags::PartiallyBound
					},
					EnumFlags<DescriptorSetLayout::Binding::Flags>{
						DescriptorSetLayout::Binding::Flags::UpdateAfterBind | DescriptorSetLayout::Binding::Flags::PartiallyBound
					}
				}
			);
#if RENDERER_OBJECT_DEBUG_NAMES
			deviceData.m_texturesDescriptorSetLayout.SetDebugName(logicalDevice, "Textures Indirect");
#endif

			deviceData.m_tempTextureSampler = Sampler(logicalDevice); // TEMP

			Threading::EngineJobRunnerThread& currentJobRunner = *Threading::EngineJobRunnerThread::GetCurrent();

			const DescriptorPoolView descriptorPool = currentJobRunner.GetRenderData().GetDescriptorPool(logicalDevice.GetIdentifier());
			[[maybe_unused]] const bool allocatedTexturesDescriptorSet = descriptorPool.AllocateDescriptorSets(
				logicalDevice,
				ArrayView<const DescriptorSetLayoutView>{deviceData.m_texturesDescriptorSetLayout},
				ArrayView<DescriptorSet>{deviceData.m_texturesDescriptorSet},
				Array{TextureIdentifier::MaximumCount}.GetDynamicView()
			);
			Assert(allocatedTexturesDescriptorSet);
			deviceData.m_pTexturesDescriptorPoolLoadingThread = &currentJobRunner;
		}

		logicalDevice.OnDestroyed.Add(
			*this,
			[&logicalDevice](TextureCache& textureCache, const LogicalDeviceView, const LogicalDeviceIdentifier deviceIdentifier)
			{
				PerLogicalDeviceData& deviceData = *textureCache.m_perLogicalDeviceData[deviceIdentifier];

				if (deviceData.m_pTexturesDescriptorPoolLoadingThread != nullptr)
				{
					Rendering::JobRunnerData& jobRunnerData = deviceData.m_pTexturesDescriptorPoolLoadingThread->GetRenderData();
					jobRunnerData.DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(deviceData.m_texturesDescriptorSet));
					jobRunnerData.DestroyDescriptorSetLayout(logicalDevice.GetIdentifier(), Move(deviceData.m_texturesDescriptorSetLayout));
				}

				if (deviceData.m_tempTextureSampler.IsValid())
				{
					deviceData.m_tempTextureSampler.Destroy(logicalDevice);
				}

				textureCache.IterateElements(
					deviceData.m_textures.GetView(),
					[&logicalDevice](UniquePtr<RenderTexture>& pTexture)
					{
						if (pTexture != nullptr)
						{
							pTexture->Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
						}
					}
				);

				textureCache.IterateElements(
					deviceData.m_textureData.GetView(),
					[](PerDeviceTextureData* pTextureData)
					{
						if (pTextureData != nullptr)
						{
							delete pTextureData;
						}
					}
				);

				deviceData.m_pDummyTexture->Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
				deviceData.m_pDummyTextureCube->Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
				textureCache.m_perLogicalDeviceData[deviceIdentifier].DestroyElement();
			}
		);
	}

	/* static */ Optional<Threading::Job*> LoadDefaultTextureFromDisk(const TextureIdentifier identifier, LogicalDevice& logicalDevice)
	{
		return new LoadTextureFromDiskJob(identifier, logicalDevice, Threading::JobPriority::LoadTextureFirstMip);
	}

	TextureIdentifier TextureCache::RegisterAsset(const Asset::Guid guid)
	{
		return BaseType::RegisterAsset(
			guid,
			[](const TextureIdentifier, const Asset::Guid)
			{
				return TextureInfo{LoadDefaultTextureFromDisk};
			}
		);
	}

	TextureIdentifier TextureCache::FindOrRegisterAsset(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[](const StaticMeshIdentifier, const Asset::Guid)
			{
				return TextureInfo{LoadDefaultTextureFromDisk};
			}
		);
	}

	struct CreateRenderTargetJob : public Threading::Job
	{
		enum class Status : uint8
		{
			AwaitingCreation,
			AwaitingAsyncFileLoadStart,
			AwaitingAsyncFileLoad,
			AwaitingMetadataParsing,
			AwaitingRenderTextureCreation,
			CreationFailed
		};

		CreateRenderTargetJob(
			const TextureIdentifier identifier,
			const RenderTargetTemplateIdentifier templateIdentifier,
			LogicalDevice& logicalDevice,
			const SampleCount sampleCount,
			const Math::Vector2ui viewRenderResolution,
			const MipMask totalMipMask,
			const uint8 numArrayLayers
		)
			: Threading::Job(Threading::JobPriority::CreateRenderTarget)
			, m_identifier(identifier)
			, m_templateIdentifier(templateIdentifier)
			, m_logicalDevice(logicalDevice)
			, m_sampleCount(sampleCount)
			, m_viewRenderResolution(viewRenderResolution)
			, m_totalMipMask(totalMipMask)
			, m_numArrayLayers(numArrayLayers)
		{
			Assert(totalMipMask.GetSize() > 0);
			Assert(numArrayLayers > 0);
		}
		CreateRenderTargetJob(const CreateRenderTargetJob&) = delete;
		CreateRenderTargetJob& operator=(const CreateRenderTargetJob&) = delete;
		CreateRenderTargetJob(CreateRenderTargetJob&&) = delete;
		CreateRenderTargetJob& operator=(CreateRenderTargetJob&&) = delete;
		virtual ~CreateRenderTargetJob() = default;

		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) final
		{
			switch (m_status)
			{
				case Status::AwaitingAsyncFileLoad:
				{
					Threading::Job* pJob = m_pAsyncLoadingJob;
					m_pAsyncLoadingJob = nullptr;
					pJob->Queue(thread);
				}
				break;
				case Status::AwaitingRenderTextureCreation:
				{
					if constexpr (PLATFORM_EMSCRIPTEN)
					{
						// JavaScript proxies texture creation to the window thread
						// We don't want that blocking, so we preemptively queue instead.
						Rendering::Window::QueueOnWindowThread(
							[this]()
							{
								CreateRenderTexture();
								Queue(System::Get<Threading::JobManager>());
							}
						);
					}
				}
				break;
				default:
					break;
			}
		}

		virtual Result OnExecute(Threading::JobRunnerThread&) final
		{
			switch (m_status)
			{
				case Status::AwaitingCreation:
				{
					const Asset::Guid templateAssetGuid =
						m_logicalDevice.GetRenderer().GetTextureCache().GetRenderTargetAssetGuid(m_templateIdentifier);

					m_status = Status::AwaitingAsyncFileLoadStart;
					const Asset::Manager& assetManager = System::Get<Asset::Manager>();
					m_pAsyncLoadingJob = assetManager.RequestAsyncLoadAssetMetadata(
						templateAssetGuid,
						GetPriority(),
						[this, templateAssetGuid](const ConstByteView readView) mutable
						{
							if (UNLIKELY(readView.IsEmpty()))
							{
								return COLD_ERROR_LOGIC(
									[this]()
									{
										LogWarning(
											"Render target creation failed: Asset {0} with guid {1} metadata load failure",
											m_identifier.GetIndex(),
											m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
										);
										if (m_status == Status::AwaitingAsyncFileLoad)
										{
											m_status = Status::CreationFailed;
											delete this;
										}
										else
										{
											Assert(m_status == Status::AwaitingAsyncFileLoadStart);
											m_status = Status::CreationFailed;
										}
										return;
									}
								);
							}

							UNUSED(templateAssetGuid);
							Assert(
								System::Get<Asset::Manager>().GetAssetTypeGuid(templateAssetGuid) == RenderTargetAsset::AssetFormat.assetTypeGuid,
								"Attempting to create a render target from a texture asset, you likely want GetOrLoadRenderTexture"
							);

							if (UNLIKELY(!Serialization::DeserializeFromBuffer(
										ConstStringView{
											reinterpret_cast<const char*>(readView.GetData()),
											static_cast<uint32>(readView.GetDataSize() / sizeof(char))
										},
										m_renderTargetAsset
									)))
							{
								return COLD_ERROR_LOGIC(
									[this]()
									{
										LogWarning(
											"Texture load failed: Asset {0} with guid {1} deserialization failure",
											m_identifier.GetIndex(),
											m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
										);
										if (m_status == Status::AwaitingAsyncFileLoad)
										{
											m_status = Status::CreationFailed;
											delete this;
										}
										else
										{
											Assert(m_status == Status::AwaitingAsyncFileLoadStart);
											m_status = Status::CreationFailed;
										}
										return;
									}
								);
							}

							m_status = Status::AwaitingMetadataParsing;
							TryQueue(*Threading::JobRunnerThread::GetCurrent());
						}
					);
					if (m_pAsyncLoadingJob != nullptr)
					{
						Assert(m_status != Status::CreationFailed);
						m_status = Status::AwaitingAsyncFileLoad;
						return Result::AwaitExternalFinish;
					}
					else if (m_status != Status::CreationFailed)
					{
						return Result::TryRequeue;
					}
					else
					{
						return Result::FinishedAndDelete;
					}
				}
				case Status::AwaitingAsyncFileLoadStart:
				case Status::AwaitingAsyncFileLoad:
				case Status::CreationFailed:
					ExpectUnreachable();
				case Status::AwaitingMetadataParsing:
				{
					const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
					const TextureAsset::BinaryType desiredBinaryType =
						supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR) ? Rendering::TextureAsset::BinaryType::ASTC
																																															: Rendering::TextureAsset::BinaryType::BC;
					const Format desiredFormat = m_renderTargetAsset.GetBinaryAssetInfo(desiredBinaryType).GetFormat();
					const FormatInfo formatInfo = GetFormatInfo(desiredFormat);

					m_loadedFlags |= LoadedTextureFlags::HasDepth * formatInfo.m_flags.IsSet(FormatFlags::Depth);
					m_loadedFlags |= LoadedTextureFlags::HasStencil * formatInfo.m_flags.IsSet(FormatFlags::Stencil);

					m_status = Status::AwaitingRenderTextureCreation;
					if constexpr (PLATFORM_EMSCRIPTEN)
					{
						return Result::AwaitExternalFinish;
					}
					else
					{
						CreateRenderTexture();
					}
				}
					[[fallthrough]];
				case Status::AwaitingRenderTextureCreation:

				{
					if (LIKELY(m_renderTarget.IsValid()))
					{
#if RENDERER_OBJECT_DEBUG_NAMES
						const Asset::Manager& assetManager = System::Get<Asset::Manager>();
						String assetName = assetManager.VisitAssetEntry(
							m_renderTargetAsset.GetGuid(),
							[](const Optional<const Asset::DatabaseEntry*> pAssetEntry) -> String
							{
								Assert(pAssetEntry.IsValid());
								if (LIKELY(pAssetEntry.IsValid()))
								{
									return pAssetEntry->GetName();
								}
								else
								{
									return {};
								}
							}
						);
						m_renderTarget.SetDebugName(m_logicalDevice, Move(assetName));
#endif

						RenderTexture previousTexture;
						m_logicalDevice.GetRenderer()
							.GetTextureCache()
							.AssignRenderTexture(m_logicalDevice.GetIdentifier(), m_identifier, Move(m_renderTarget), previousTexture, m_loadedFlags);
						m_logicalDevice.GetRenderer().GetTextureCache().OnTextureLoadFinished(m_logicalDevice.GetIdentifier(), m_identifier);
						Assert(!previousTexture.IsValid());

						return Result::FinishedAndDelete;
					}
					else
					{
						m_logicalDevice.GetRenderer().GetTextureCache().OnTextureLoadFailed(m_logicalDevice.GetIdentifier(), m_identifier);
						m_renderTarget.Destroy(m_logicalDevice, m_logicalDevice.GetDeviceMemoryPool());

						return Result::FinishedAndDelete;
					}
				}
			}
			ExpectUnreachable();
		}

		void CreateRenderTexture()
		{
			const Math::Vector2ui textureSize = !m_renderTargetAsset.GetResolution().IsZero() ? m_renderTargetAsset.GetResolution()
			                                                                                  : m_viewRenderResolution;

			const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
			const TextureAsset::BinaryType desiredBinaryType = supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR)
			                                                     ? Rendering::TextureAsset::BinaryType::ASTC
			                                                     : Rendering::TextureAsset::BinaryType::BC;
			const Format desiredFormat = m_renderTargetAsset.GetBinaryAssetInfo(desiredBinaryType).GetFormat();
			const FormatInfo formatInfo = GetFormatInfo(desiredFormat);

			// Start by validating the usable mips
			MipMask validMipsMask;

			const MipMask::StoredType totalMipCount = m_totalMipMask.GetSize();

			for (MipMask::StoredType mipIndex : Memory::GetSetBitsIterator(m_totalMipMask.GetValue()))
			{
				const uint16 relativeLevel = totalMipCount - mipIndex - 1;
				Math::Vector2ui mipSize = textureSize >> relativeLevel;
				const Math::TBoolVector2<uint32> isDivisibleByBlockExtent =
					Math::Mod(mipSize, Math::Vector2ui{formatInfo.m_blockExtent.x, formatInfo.m_blockExtent.y}) == Math::Zero;
				if (UNLIKELY(!isDivisibleByBlockExtent.AreAllSet()))
				{
					continue;
				}
				validMipsMask |= MipMask::FromIndex(mipIndex);
			}

			const uint8 numArrayLayers = m_numArrayLayers;
			m_renderTarget = RenderTexture(
				RenderTexture::RenderTarget,
				m_logicalDevice,
				textureSize,
				desiredFormat,
				m_sampleCount,
				m_renderTargetAsset.GetImageFlags(),
				m_renderTargetAsset.GetUsageFlags(),
				validMipsMask,
				numArrayLayers
			);
		}
	protected:
		Status m_status = Status::AwaitingCreation;
		const TextureIdentifier m_identifier;
		const RenderTargetTemplateIdentifier m_templateIdentifier;
		LogicalDevice& m_logicalDevice;
		const SampleCount m_sampleCount;
		const Math::Vector2ui m_viewRenderResolution;
		const MipMask m_totalMipMask;
		const uint8 m_numArrayLayers;
		EnumFlags<LoadedTextureFlags> m_loadedFlags;
		RenderTexture m_renderTarget;

		RenderTargetAsset m_renderTargetAsset;
		Threading::Job* m_pAsyncLoadingJob = nullptr;
	};

	/* static */ Optional<Threading::Job*> CreateDefaultRenderTarget(
		const TextureIdentifier identifier,
		const RenderTargetTemplateIdentifier templateIdentifier,
		LogicalDevice& logicalDevice,
		const SampleCount sampleCount,
		const Math::Vector2ui viewRenderResolution,
		const MipMask totalMipMask,
		const uint8 numArrayLayers

	)
	{
		return new CreateRenderTargetJob(
			identifier,
			templateIdentifier,
			logicalDevice,
			sampleCount,
			viewRenderResolution,
			totalMipMask,
			numArrayLayers
		);
	}

	struct ResizeRenderTargetJob : public Threading::Job
	{
		enum class Status : uint8
		{
			AwaitingCreation,
			AwaitingAsyncFileLoadStart,
			AwaitingAsyncFileLoad,
			AwaitingMetadataParsing,
			AwaitingRenderTextureCreation,
			CreationFailed
		};

		ResizeRenderTargetJob(
			const TextureIdentifier identifier,
			const RenderTargetTemplateIdentifier templateIdentifier,
			LogicalDevice& logicalDevice,
			const SampleCount sampleCount,
			const Math::Vector2ui viewRenderResolution,
			const MipMask totalMipMask,
			const uint8 numArrayLayers
		)
			: Threading::Job(Threading::JobPriority::CreateRenderTarget)
			, m_identifier(identifier)
			, m_templateIdentifier(templateIdentifier)
			, m_logicalDevice(logicalDevice)
			, m_sampleCount(sampleCount)
			, m_viewRenderResolution(viewRenderResolution)
			, m_totalMipMask(totalMipMask)
			, m_numArrayLayers(numArrayLayers)
		{
			Assert(totalMipMask.GetSize() > 0);
			Assert(numArrayLayers > 0);
		}
		ResizeRenderTargetJob(const ResizeRenderTargetJob&) = delete;
		ResizeRenderTargetJob& operator=(const ResizeRenderTargetJob&) = delete;
		ResizeRenderTargetJob(ResizeRenderTargetJob&&) = delete;
		ResizeRenderTargetJob& operator=(ResizeRenderTargetJob&&) = delete;
		virtual ~ResizeRenderTargetJob() = default;

		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override final
		{
			switch (m_status)
			{
				case Status::AwaitingAsyncFileLoad:
				{
					Threading::Job* pJob = m_pAsyncLoadingJob;
					m_pAsyncLoadingJob = nullptr;
					pJob->Queue(thread);
				}
				break;
				case Status::AwaitingRenderTextureCreation:
				{
					if constexpr (PLATFORM_EMSCRIPTEN)
					{
						// JavaScript proxies texture creation to the window thread
						// We don't want that blocking, so we preemptively queue instead.
						Rendering::Window::QueueOnWindowThread(
							[this]()
							{
								CreateRenderTexture();
								Queue(System::Get<Threading::JobManager>());
							}
						);
					}
				}
				break;
				default:
					break;
			}
		}

		virtual Result OnExecute(Threading::JobRunnerThread&) final
		{
			switch (m_status)
			{
				case Status::AwaitingCreation:
				{
					const Asset::Guid templateAssetGuid =
						m_logicalDevice.GetRenderer().GetTextureCache().GetRenderTargetAssetGuid(m_templateIdentifier);

					m_status = Status::AwaitingAsyncFileLoadStart;
					const Asset::Manager& assetManager = System::Get<Asset::Manager>();
					m_pAsyncLoadingJob = assetManager.RequestAsyncLoadAssetMetadata(
						templateAssetGuid,
						GetPriority(),
						[this, templateAssetGuid](const ConstByteView readView) mutable
						{
							if (UNLIKELY(readView.IsEmpty()))
							{
								return COLD_ERROR_LOGIC(
									[this]()
									{
										LogWarning(
											"Render target creation failed: Asset {0} with guid {1} metadata load failure",
											m_identifier.GetIndex(),
											m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
										);
										if (m_status == Status::AwaitingAsyncFileLoad)
										{
											m_status = Status::CreationFailed;
											delete this;
										}
										else
										{
											Assert(m_status == Status::AwaitingAsyncFileLoadStart);
											m_status = Status::CreationFailed;
										}
										return;
									}
								);
							}

							UNUSED(templateAssetGuid);
							Assert(
								System::Get<Asset::Manager>().GetAssetTypeGuid(templateAssetGuid) == RenderTargetAsset::AssetFormat.assetTypeGuid,
								"Attempting to create a render target from a texture asset, you likely want GetOrLoadRenderTexture"
							);

							if (UNLIKELY(!Serialization::DeserializeFromBuffer(
										ConstStringView{
											reinterpret_cast<const char*>(readView.GetData()),
											static_cast<uint32>(readView.GetDataSize() / sizeof(char))
										},
										m_renderTargetAsset
									)))
							{
								return COLD_ERROR_LOGIC(
									[this]()
									{
										LogWarning(
											"Texture load failed: Asset {0} with guid {1} deserialization failure",
											m_identifier.GetIndex(),
											m_logicalDevice.GetRenderer().GetTextureCache().GetAssetGuid(m_identifier)
										);
										if (m_status == Status::AwaitingAsyncFileLoad)
										{
											m_status = Status::CreationFailed;
											delete this;
										}
										else
										{
											Assert(m_status == Status::AwaitingAsyncFileLoadStart);
											m_status = Status::CreationFailed;
										}
										return;
									}
								);
							}

							m_status = Status::AwaitingMetadataParsing;
							TryQueue(*Threading::JobRunnerThread::GetCurrent());
						}
					);
					if (m_pAsyncLoadingJob != nullptr)
					{
						Assert(m_status != Status::CreationFailed);
						m_status = Status::AwaitingAsyncFileLoad;
						return Result::AwaitExternalFinish;
					}
					else if (m_status != Status::CreationFailed)
					{
						return Result::TryRequeue;
					}
					else
					{
						return Result::FinishedAndDelete;
					}
				}
				case Status::AwaitingAsyncFileLoadStart:
				case Status::AwaitingAsyncFileLoad:
				case Status::CreationFailed:
					ExpectUnreachable();
				case Status::AwaitingMetadataParsing:
				{
					const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
					const TextureAsset::BinaryType desiredBinaryType =
						supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR) ? Rendering::TextureAsset::BinaryType::ASTC
																																															: Rendering::TextureAsset::BinaryType::BC;
					const Format desiredFormat = m_renderTargetAsset.GetBinaryAssetInfo(desiredBinaryType).GetFormat();
					const FormatInfo formatInfo = GetFormatInfo(desiredFormat);

					m_loadedFlags |= LoadedTextureFlags::HasDepth * formatInfo.m_flags.IsSet(FormatFlags::Depth);
					m_loadedFlags |= LoadedTextureFlags::HasStencil * formatInfo.m_flags.IsSet(FormatFlags::Stencil);

					m_status = Status::AwaitingRenderTextureCreation;
					if constexpr (PLATFORM_EMSCRIPTEN)
					{
						return Result::AwaitExternalFinish;
					}
					else
					{
						CreateRenderTexture();
					}
				}
					[[fallthrough]];
				case Status::AwaitingRenderTextureCreation:
				{
					if (LIKELY(m_renderTarget.IsValid()))
					{
#if RENDERER_OBJECT_DEBUG_NAMES
						const Asset::Manager& assetManager = System::Get<Asset::Manager>();
						String assetName = assetManager.VisitAssetEntry(
							m_renderTargetAsset.GetGuid(),
							[](const Optional<const Asset::DatabaseEntry*> pAssetEntry) -> String
							{
								Assert(pAssetEntry.IsValid());
								if (LIKELY(pAssetEntry.IsValid()))
								{
									return pAssetEntry->GetName();
								}
								else
								{
									return {};
								}
							}
						);
						m_renderTarget.SetDebugName(m_logicalDevice, Move(assetName));
#endif

						RenderTexture previousTexture;
						m_logicalDevice.GetRenderer()
							.GetTextureCache()
							.AssignRenderTexture(m_logicalDevice.GetIdentifier(), m_identifier, Move(m_renderTarget), previousTexture, m_loadedFlags);
						m_logicalDevice.GetRenderer().GetTextureCache().OnTextureLoadFinished(m_logicalDevice.GetIdentifier(), m_identifier);
						Assert(!previousTexture.IsValid());

						return Result::FinishedAndDelete;
					}
					else
					{
						m_logicalDevice.GetRenderer().GetTextureCache().OnTextureLoadFailed(m_logicalDevice.GetIdentifier(), m_identifier);
						m_renderTarget.Destroy(m_logicalDevice, m_logicalDevice.GetDeviceMemoryPool());

						return Result::FinishedAndDelete;
					}
				}
			}

			ExpectUnreachable();
		}

		void CreateRenderTexture()
		{
			const Math::Vector2ui textureSize = !m_renderTargetAsset.GetResolution().IsZero() ? m_renderTargetAsset.GetResolution()
			                                                                                  : m_viewRenderResolution;

			const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = m_logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
			const TextureAsset::BinaryType desiredBinaryType = supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR)
			                                                     ? Rendering::TextureAsset::BinaryType::ASTC
			                                                     : Rendering::TextureAsset::BinaryType::BC;
			const Format desiredFormat = m_renderTargetAsset.GetBinaryAssetInfo(desiredBinaryType).GetFormat();
			const MipMask totalMipMask = m_totalMipMask;
			const uint8 arrayLayerCount = m_numArrayLayers;

			m_renderTarget = RenderTexture(
				RenderTexture::RenderTarget,
				m_logicalDevice,
				textureSize,
				desiredFormat,
				m_sampleCount,
				m_renderTargetAsset.GetImageFlags(),
				m_renderTargetAsset.GetUsageFlags(),
				totalMipMask,
				arrayLayerCount
			);
		}
	protected:
		Status m_status = Status::AwaitingCreation;
		const TextureIdentifier m_identifier;
		const RenderTargetTemplateIdentifier m_templateIdentifier;
		LogicalDevice& m_logicalDevice;
		const SampleCount m_sampleCount;
		const Math::Vector2ui m_viewRenderResolution;
		const MipMask m_totalMipMask;
		const uint8 m_numArrayLayers;
		EnumFlags<LoadedTextureFlags> m_loadedFlags = LoadedTextureFlags::WasResized;
		RenderTexture m_renderTarget;

		RenderTargetAsset m_renderTargetAsset;
		Threading::Job* m_pAsyncLoadingJob = nullptr;
	};

	/* static */ Optional<Threading::Job*> ResizeDefaultRenderTarget(
		const TextureIdentifier identifier,
		const RenderTargetTemplateIdentifier templateIdentifier,
		LogicalDevice& logicalDevice,
		const SampleCount sampleCount,
		const Math::Vector2ui viewRenderResolution,
		const MipMask totalMipMask,
		const uint8 numArrayLayers
	)
	{
		return new ResizeRenderTargetJob(
			identifier,
			templateIdentifier,
			logicalDevice,
			sampleCount,
			viewRenderResolution,
			totalMipMask,
			numArrayLayers
		);
	}

	RenderTargetTemplateIdentifier TextureCache::RegisterRenderTargetTemplate(const Asset::Guid guid)
	{
		return m_renderTargetAssetType.RegisterAsset(
			guid,
			[](const RenderTargetTemplateIdentifier, const Asset::Guid)
			{
				return RenderTargetInfo{CreateDefaultRenderTarget, ResizeDefaultRenderTarget};
			}
		);
	}

	RenderTargetTemplateIdentifier TextureCache::RegisterProceduralRenderTargetTemplate(
		const Asset::Guid guid, RenderTargetCreationCallback&& creationCallback, RenderTargetResizingCallback&& resizingCallback
	)
	{
#if PLATFORM_APPLE_VISIONOS
		{
			const RenderTargetTemplateIdentifier existingIdentifier = m_renderTargetAssetType.FindIdentifier(guid);
			Assert(!existingIdentifier.IsValid());
			if (existingIdentifier.IsValid())
			{
				m_renderTargetAssetType.GetAssetData(existingIdentifier).m_creationCallback = Forward<RenderTargetCreationCallback>(creationCallback
				);
				m_renderTargetAssetType.GetAssetData(existingIdentifier).m_resizingCallback = Forward<RenderTargetResizingCallback>(resizingCallback
				);
				return existingIdentifier;
			}
		}
#endif

		return m_renderTargetAssetType.RegisterAsset(
			guid,
			[creationCallback = Forward<RenderTargetCreationCallback>(creationCallback),
		   resizingCallback = Forward<RenderTargetResizingCallback>(resizingCallback
		   )](const RenderTargetTemplateIdentifier, const Asset::Guid) mutable
			{
				return RenderTargetInfo{Move(creationCallback), Move(resizingCallback)};
			}
		);
	}

	RenderTargetTemplateIdentifier TextureCache::FindOrRegisterRenderTargetTemplate(const Asset::Guid guid)
	{
		return m_renderTargetAssetType.FindOrRegisterAsset(
			guid,
			[](const RenderTargetTemplateIdentifier, const Asset::Guid)
			{
				return RenderTargetInfo{CreateDefaultRenderTarget, ResizeDefaultRenderTarget};
			}
		);
	}

	RenderTargetTemplateIdentifier TextureCache::FindRenderTargetTemplateIdentifier(const Asset::Guid guid)
	{
		return m_renderTargetAssetType.FindIdentifier(guid);
	}

	TextureIdentifier TextureCache::RegisterProceduralRenderTargetAsset()
	{
		return BaseType::RegisterProceduralAsset(
			[](const TextureIdentifier, const Asset::Guid)
			{
				return TextureInfo{InvalidTextureLoadingFunction};
			}
		);
	}

	void TextureCache::Remove(const TextureIdentifier identifier)
	{
		BaseType::DeregisterAsset(identifier);
	}

	RenderTexture& TextureCache::GetDummyTexture(const LogicalDeviceIdentifier deviceIdentifier, const ImageMappingType type) const
	{
		switch (type)
		{
			case ImageMappingType::TwoDimensional:
				return *m_perLogicalDeviceData[deviceIdentifier]->m_pDummyTexture;
			case ImageMappingType::Cube:
				return *m_perLogicalDeviceData[deviceIdentifier]->m_pDummyTextureCube;
			default:
				ExpectUnreachable();
		}
	}

	TextureCache::PerDeviceTextureData&
	TextureCache::GetOrCreatePerDeviceTextureData(PerLogicalDeviceData& perDeviceData, const TextureIdentifier identifier)
	{
		Threading::Atomic<PerDeviceTextureData*>& textureData = perDeviceData.m_textureData[identifier];

		if (textureData.Load() != nullptr)
		{
			return *textureData;
		}
		else
		{
			PerDeviceTextureData* pExpected = nullptr;
			PerDeviceTextureData* pNewValue = new PerDeviceTextureData();
			if (textureData.CompareExchangeStrong(pExpected, pNewValue))
			{
				return *pNewValue;
			}
			else
			{
				delete pNewValue;
				return *pExpected;
			}
		}
	}

	Optional<Threading::Job*> TextureCache::GetOrLoadRenderTexture(
		const LogicalDeviceIdentifier deviceIdentifier,
		const TextureIdentifier identifier,
		const ImageMappingType type,
		MipMask requestedMips,
		const EnumFlags<TextureLoadFlags> flags,
		TextureLoadListenerData&& newListenerData
	)
	{
		Assert(requestedMips.AreAnySet());
		PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];

		PerDeviceTextureData& perDeviceTextureData = GetOrCreatePerDeviceTextureData(perDeviceData, identifier);
		perDeviceTextureData.m_onLoadedCallback.Emplace(Forward<TextureLoadListenerData>(newListenerData));

		LogicalDevice& logicalDevice = *GetRenderer().GetLogicalDevice(deviceIdentifier);

		if (perDeviceData.m_textures[identifier].IsValid())
		{
			RenderTexture& texture = *perDeviceData.m_textures[identifier];
			const MipMask loadedMipMask = texture.GetLoadedMipMask();
			const MipMask totalMipMask = texture.GetTotalMipMask();

			// Exclude mips that don't exist
			requestedMips &= totalMipMask;

			if (!requestedMips.AreAnySet())
			{
				// Fall back to highest mip
				requestedMips = MipMask::FromIndex(*totalMipMask.GetLastIndex());
			}

			if ((loadedMipMask & requestedMips).AreAnySet())
			{
				const MipMask requestedLoadedMips = requestedMips & loadedMipMask;

				// Some mips had already loaded, notify the callback
				[[maybe_unused]] const bool wasExecuted = perDeviceTextureData.m_onLoadedCallback.Execute(
					newListenerData.m_identifier,
					logicalDevice,
					identifier,
					texture,
					requestedLoadedMips,
					perDeviceData.m_textureData[identifier].Load()->m_flags
				);
				Assert(wasExecuted);

				if (requestedLoadedMips == requestedMips)
				{
					return nullptr;
				}

				requestedMips &= ~loadedMipMask;
			}

			if (requestedMips.AreAnySet())
			{
				// Request any additional mips that haven't been loaded
				const MipMask newRequestedMips = MipMask{perDeviceTextureData.m_requestedMips.FetchOr(requestedMips.GetValue())};
				if (newRequestedMips.AreAnySet())
				{
					if (perDeviceData.m_loadingTextures.Set(identifier))
					{
						const TextureLoadingCallback& loadingCallback = logicalDevice.GetRenderer().GetTextureCache().GetLoadingCallback(identifier);
						if (Optional<Threading::Job*> pLoadingJob = loadingCallback(identifier, logicalDevice))
						{
							return pLoadingJob;
						}
						else
						{
							Assert(!perDeviceData.m_loadingTextures.IsSet(identifier));
						}
					}
				}
			}
		}
		else
		{
			perDeviceTextureData.m_requestedMips |= requestedMips.GetValue();
			if (perDeviceData.m_loadingTextures.Set(identifier))
			{
				if (!perDeviceData.m_textures[identifier].IsValid())
				{
					if (flags.IsSet(TextureLoadFlags::LoadDummy))
					{
						RenderTexture& dummyTexture = GetDummyTexture(deviceIdentifier, type);
						[[maybe_unused]] const bool wasExecuted = perDeviceTextureData.m_onLoadedCallback.Execute(
							newListenerData.m_identifier,
							logicalDevice,
							identifier,
							dummyTexture,
							dummyTexture.GetLoadedMipMask(),
							LoadedTextureFlags::IsDummy
						);
						Assert(wasExecuted);
					}

					const TextureLoadingCallback& loadingCallback = logicalDevice.GetRenderer().GetTextureCache().GetLoadingCallback(identifier);
					if (Optional<Threading::Job*> pLoadingJob = loadingCallback(identifier, logicalDevice))
					{
						return pLoadingJob;
					}
					else
					{
						Assert(!perDeviceData.m_loadingTextures.IsSet(identifier));
					}
				}
				else
				{
					Assert(false);
					[[maybe_unused]] const bool cleared = perDeviceData.m_loadingTextures.Clear(identifier);
					Assert(cleared);

					Assert(
						perDeviceData.m_textureData[identifier].Load() == nullptr ||
						(perDeviceData.m_textures[identifier]->GetLoadedMipMask() &
					   MipMask{perDeviceData.m_textureData[identifier]->m_requestedMips.Load()}) ==
							MipMask{perDeviceData.m_textureData[identifier]->m_requestedMips.Load()}
					);

					RenderTexture& texture = *perDeviceData.m_textures[identifier];
					[[maybe_unused]] const bool wasExecuted = perDeviceTextureData.m_onLoadedCallback.Execute(
						newListenerData.m_identifier,
						logicalDevice,
						identifier,
						texture,
						texture.GetLoadedMipMask(),
						perDeviceData.m_textureData[identifier].Load()->m_flags
					);
					Assert(wasExecuted);
				}
			}
			else if (flags.IsSet(TextureLoadFlags::LoadDummy))
			{
				RenderTexture& texture = GetDummyTexture(deviceIdentifier, type);
				[[maybe_unused]] const bool wasExecuted = perDeviceTextureData.m_onLoadedCallback.Execute(
					newListenerData.m_identifier,
					logicalDevice,
					identifier,
					texture,
					texture.GetLoadedMipMask(),
					LoadedTextureFlags::IsDummy
				);
				Assert(wasExecuted);
			}
		}

		return nullptr;
	}

	Optional<Threading::Job*>
	TextureCache::ReloadRenderTexture(const LogicalDeviceIdentifier deviceIdentifier, const TextureIdentifier identifier)
	{
		if (IsRenderTextureLoaded(deviceIdentifier, identifier))
		{
			PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];
			// Try to atomically set the texture as loading
			if (perDeviceData.m_loadingTextures.Set(identifier))
			{
				LogicalDevice& logicalDevice = *GetRenderer().GetLogicalDevice(deviceIdentifier);
				// Queue the texture for reload
				return GetLoadingCallback(identifier)(identifier, logicalDevice);
			}
		}
		return nullptr;
	}

	Threading::JobBatch TextureCache::ReloadRenderTexture(const TextureIdentifier identifier)
	{
		Threading::JobBatch renderTexturesJobBatch;
		for (const Optional<LogicalDevice*> pLogicalDevice : GetRenderer().GetLogicalDevices())
		{
			if (pLogicalDevice.IsValid())
			{
				Threading::JobBatch deviceJobBatch = ReloadRenderTexture(pLogicalDevice->GetIdentifier(), identifier);
				renderTexturesJobBatch.QueueAfterStartStage(deviceJobBatch);
			}
		}
		return renderTexturesJobBatch;
	}

	Optional<Threading::Job*> TextureCache::GetOrLoadRenderTarget(
		LogicalDevice& logicalDevice,
		const TextureIdentifier identifier,
		const RenderTargetTemplateIdentifier templateIdentifier,
		const SampleCount sampleCount,
		const Math::Vector2ui viewRenderResolution,
		const MipMask totalMipMask,
		const ArrayRange arrayLayerRange,
		TextureLoadListenerData&& newListenerData
	)
	{
		PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[logicalDevice.GetIdentifier()];

		PerDeviceTextureData& textureRequesters = GetOrCreatePerDeviceTextureData(perDeviceData, identifier);
		textureRequesters.m_onLoadedCallback.Emplace(Forward<TextureLoadListenerData>(newListenerData));

		UniquePtr<RenderTexture>& pTexture = perDeviceData.m_textures[identifier];
		if (pTexture.IsValid())
		{
			[[maybe_unused]] const bool wasExecuted = textureRequesters.m_onLoadedCallback.Execute(
				newListenerData.m_identifier,
				logicalDevice,
				identifier,
				*pTexture,
				pTexture->GetLoadedMipMask(),
				perDeviceData.m_textureData[identifier].Load()->m_flags
			);
			Assert(wasExecuted);
		}
		else
		{
			if (perDeviceData.m_loadingTextures.Set(identifier))
			{
				if (pTexture.IsValid())
				{
					Assert(false);
					[[maybe_unused]] const bool cleared = perDeviceData.m_loadingTextures.Clear(identifier);
					Assert(cleared);

					Assert(
						perDeviceData.m_textureData[identifier].Load() == nullptr ||
						(perDeviceData.m_textures[identifier]->GetLoadedMipMask() &
					   MipMask{perDeviceData.m_textureData[identifier]->m_requestedMips.Load()}) ==
							MipMask{perDeviceData.m_textureData[identifier]->m_requestedMips.Load()}
					);

					[[maybe_unused]] const bool wasExecuted = textureRequesters.m_onLoadedCallback.Execute(
						newListenerData.m_identifier,
						logicalDevice,
						identifier,
						*pTexture,
						pTexture->GetLoadedMipMask(),
						perDeviceData.m_textureData[identifier].Load()->m_flags
					);
					Assert(wasExecuted);
				}
				else
				{
					return m_renderTargetAssetType.GetAssetData(templateIdentifier)
					  .m_creationCallback(
							identifier,
							templateIdentifier,
							logicalDevice,
							sampleCount,
							viewRenderResolution,
							totalMipMask,
							(uint8)arrayLayerRange.GetCount()
						);
				}
			}
			else if (pTexture.IsValid())
			{
				[[maybe_unused]] const bool wasExecuted = textureRequesters.m_onLoadedCallback.Execute(
					newListenerData.m_identifier,
					logicalDevice,
					identifier,
					*pTexture,
					pTexture->GetLoadedMipMask(),
					perDeviceData.m_textureData[identifier].Load()->m_flags
				);
				Assert(wasExecuted);
			}
		}

		return nullptr;
	}

	Optional<Threading::Job*> TextureCache::ResizeRenderTarget(
		LogicalDevice& logicalDevice,
		const TextureIdentifier identifier,
		const RenderTargetTemplateIdentifier templateIdentifier,
		const SampleCount sampleCount,
		const Math::Vector2ui viewRenderResolution,
		const MipMask totalMipMask,
		const ArrayRange arrayLayerRange
	)
	{
		if (logicalDevice.GetRenderer().GetTextureCache().m_perLogicalDeviceData[logicalDevice.GetIdentifier()]->m_loadingTextures.Set(
					identifier
				))
		{
			return m_renderTargetAssetType.GetAssetData(templateIdentifier)
			  .m_resizingCallback(
					identifier,
					templateIdentifier,
					logicalDevice,
					sampleCount,
					viewRenderResolution,
					totalMipMask,
					(uint8)arrayLayerRange.GetCount()
				);
		}
		return nullptr;
	}

	void TextureCache::DestroyRenderTarget(LogicalDevice& logicalDevice, const TextureIdentifier identifier)
	{
		PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[logicalDevice.GetIdentifier()];

		UniquePtr<RenderTexture>& pTexture = perDeviceData.m_textures[identifier];
		if (pTexture.IsValid())
		{
			pTexture->Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
			pTexture.DestroyElement();
		}
	}

	bool TextureCache::RemoveRenderTextureListener(
		const LogicalDeviceIdentifier deviceIdentifier,
		const TextureIdentifier identifier,
		const TextureLoadListenerIdentifier listenerIdentifier
	)
	{
		PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];
		PerDeviceTextureData* pTextureData = perDeviceData.m_textureData[identifier];
		if (LIKELY(pTextureData != nullptr))
		{
			return pTextureData->m_onLoadedCallback.Remove(listenerIdentifier);
		}
		return false;
	}

	void TextureCache::AssignRenderTexture(
		const LogicalDeviceIdentifier deviceIdentifier,
		const Rendering::TextureIdentifier identifier,
		RenderTexture&& texture,
		RenderTexture& previousTexture,
		const EnumFlags<LoadedTextureFlags> flags
	)
	{
		PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];

		Assert(perDeviceData.m_loadingTextures.IsSet(identifier));

		if (perDeviceData.m_textures[identifier].IsValid())
		{
			previousTexture = Move(*perDeviceData.m_textures[identifier]);
		}

		perDeviceData.m_textures[identifier].CreateInPlace(Move(texture));

		PerDeviceTextureData& textureData = *perDeviceData.m_textureData[identifier];
		LogicalDevice& logicalDevice = *GetRenderer().GetLogicalDevice(deviceIdentifier);
		RenderTexture& storedTexture = *perDeviceData.m_textures[identifier];

		if (storedTexture.GetUsageFlags().IsSet(UsageFlags::Sampled) && perDeviceData.m_texturesDescriptorSet.IsValid())
		{
			const ImageMappingType mappingType = [](const ArrayRange::UnitType arrayCount)
			{
				switch (arrayCount)
				{
					case 1:
						return ImageMappingType::TwoDimensional;
					case 6:
						return ImageMappingType::Cube;
					case 0:
						ExpectUnreachable();
					default:
						return ImageMappingType::TwoDimensionalArray;
				}
			}(storedTexture.GetTotalArrayCount());
			const FormatInfo& __restrict formatInfo = GetFormatInfo(storedTexture.GetFormat());
			EnumFlags<Rendering::ImageAspectFlags> imageAspectFlags;
			imageAspectFlags |= Rendering::ImageAspectFlags::Color * formatInfo.m_flags.AreNoneSet(FormatFlags::DepthStencil);
			imageAspectFlags |= Rendering::ImageAspectFlags::Depth * formatInfo.m_flags.IsSet(FormatFlags::Depth);
			// imageAspectFlags |= Rendering::ImageAspectFlags::Stencil * formatInfo.m_flags.IsSet(FormatFlags::Stencil);
			ImageMapping newMapping(
				logicalDevice,
				storedTexture,
				mappingType,
				storedTexture.GetFormat(),
				imageAspectFlags,
				storedTexture.GetAvailableMipRange(),
				ArrayRange(0, storedTexture.GetTotalArrayCount())
			);

			Array imageInfos{
				DescriptorSet::ImageInfo{{}, newMapping, ImageLayout::ShaderReadOnlyOptimal},
				DescriptorSet::ImageInfo{perDeviceData.m_tempTextureSampler, {}, ImageLayout::ShaderReadOnlyOptimal}
			};

			DescriptorSet::Update(
				logicalDevice,
				Array{
					DescriptorSet::UpdateInfo{
						perDeviceData.m_texturesDescriptorSet,
						0,
						identifier.GetFirstValidIndex(),
						DescriptorType::SampledImage,
						imageInfos.GetSubView(0, 1)
					},
					DescriptorSet::UpdateInfo{
						perDeviceData.m_texturesDescriptorSet,
						1,
						identifier.GetFirstValidIndex(),
						DescriptorType::Sampler,
						imageInfos.GetSubView(1, 1)
					}
				}
			);

			textureData.m_mapping.AtomicSwap(newMapping);

			if (newMapping.IsValid())
			{
				Threading::EngineJobRunnerThread& engineThread = *Threading::EngineJobRunnerThread::GetCurrent();
				engineThread.GetRenderData().DestroyImageMapping(deviceIdentifier, Move(newMapping));
			}
		}

		textureData.m_flags = flags;
		textureData.m_onLoadedCallback(logicalDevice, identifier, storedTexture, storedTexture.GetLoadedMipMask(), flags);
	}

	void TextureCache::ChangeRenderTextureAvailableMips(
		const LogicalDeviceIdentifier deviceIdentifier,
		const Rendering::TextureIdentifier identifier,
		const MipMask loadedMipValues,
		const EnumFlags<LoadedTextureFlags> flags
	)
	{
		PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];
		RenderTexture& texture = *perDeviceData.m_textures[identifier];
		texture.OnMipsLoaded(loadedMipValues);

		PerDeviceTextureData& textureData = *perDeviceData.m_textureData[identifier];
		LogicalDevice& logicalDevice = *GetRenderer().GetLogicalDevice(deviceIdentifier);
		Assert(texture.IsValid());

		if (texture.GetUsageFlags().IsSet(UsageFlags::Sampled) && perDeviceData.m_texturesDescriptorSet.IsValid())
		{
			const ImageMappingType mappingType = texture.GetTotalArrayCount() == 6 ? ImageMappingType::Cube : ImageMappingType::TwoDimensional;
			const FormatInfo& __restrict formatInfo = GetFormatInfo(texture.GetFormat());
			EnumFlags<Rendering::ImageAspectFlags> imageAspectFlags;
			imageAspectFlags |= Rendering::ImageAspectFlags::Color * formatInfo.m_flags.AreNoneSet(FormatFlags::DepthStencil);
			imageAspectFlags |= Rendering::ImageAspectFlags::Depth * formatInfo.m_flags.IsSet(FormatFlags::Depth);
			// imageAspectFlags |= Rendering::ImageAspectFlags::Stencil * formatInfo.m_flags.IsSet(FormatFlags::Stencil);

			ImageMapping newMapping(
				logicalDevice,
				texture,
				mappingType,
				texture.GetFormat(),
				imageAspectFlags,
				texture.GetAvailableMipRange(),
				ArrayRange(0, texture.GetTotalArrayCount())
			);

			Array imageInfos{DescriptorSet::ImageInfo{perDeviceData.m_tempTextureSampler, newMapping, ImageLayout::ShaderReadOnlyOptimal}};

			DescriptorSet::Update(
				logicalDevice,
				Array{DescriptorSet::UpdateInfo{
					perDeviceData.m_texturesDescriptorSet,
					0,
					identifier.GetFirstValidIndex(),
					DescriptorType::SampledImage,
					imageInfos.GetDynamicView()
				}}
			);

			textureData.m_mapping.AtomicSwap(newMapping);

			Threading::EngineJobRunnerThread& engineThread = *Threading::EngineJobRunnerThread::GetCurrent();
			engineThread.GetRenderData().DestroyImageMapping(deviceIdentifier, Move(newMapping));
		}

		textureData.m_flags = flags;
		textureData.m_onLoadedCallback(logicalDevice, identifier, texture, loadedMipValues, flags);
	}

	void TextureCache::OnTextureLoadFailed(const LogicalDeviceIdentifier deviceIdentifier, const Rendering::TextureIdentifier identifier)
	{
		PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];

		// TODO: Replace texture with a 'failed to load' dummy
		// Should have some visual representation making it clear that this texture is missing

		[[maybe_unused]] const bool wasCleared = perDeviceData.m_loadingTextures.Clear(identifier);
		Assert(wasCleared);
	}

	void TextureCache::OnTextureLoadFinished(const LogicalDeviceIdentifier deviceIdentifier, const Rendering::TextureIdentifier identifier)
	{
		PerLogicalDeviceData& perDeviceData = *m_perLogicalDeviceData[deviceIdentifier];

		[[maybe_unused]] const bool wasCleared = perDeviceData.m_loadingTextures.Clear(identifier);
		Assert(wasCleared);

		if constexpr (DEBUG_BUILD)
		{
			RenderTexture& texture = *perDeviceData.m_textures[identifier];
			[[maybe_unused]] MipMask requestedMips = MipMask{perDeviceData.m_textureData[identifier]->m_requestedMips.Load()};
			[[maybe_unused]] const MipMask totalMips = texture.GetTotalMipMask();
			requestedMips &= totalMips;

			[[maybe_unused]] const MipMask loadedMips = texture.GetLoadedMipMask();
		}
	}

	// Handle runtime reloading of textures
	void TextureCache::OnAssetModified(
		[[maybe_unused]] const Asset::Guid assetGuid, const IdentifierType identifier, [[maybe_unused]] const IO::PathView filePath
	)
	{
		Threading::JobBatch reloadJobBatch = ReloadRenderTexture(identifier);
		if (reloadJobBatch.IsValid())
		{
			reloadJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[this, identifier](Threading::JobRunnerThread&)
				{
					[[maybe_unused]] const bool wasCleared = m_reloadingAssets.Clear(identifier);
					Assert(wasCleared);
				},
				Threading::JobPriority::FileChangeDetection
			));
			Threading::JobRunnerThread::GetCurrent()->Queue(reloadJobBatch);
		}
	}
}
