#include "FontAtlas.h"
#include "Font.h"
#include "FontPipeline.h"

#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>
#include <Common/System/Query.h>

#include <Renderer/Renderer.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Commands/CommandBuffer.h>
#include <Renderer/Commands/CommandEncoder.h>
#include <Renderer/Commands/EncodedCommandBuffer.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/Descriptors/DescriptorSetLayoutView.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Assets/Texture/TextureCache.h>
#include <Renderer/FormatInfo.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>

#include <Common/Math/PowerOfTwo.h>
#include <Common/Math/Vector2/Max.h>
#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include "ft2build.h"
#include FT_FREETYPE_H

namespace ngine::Font
{
	Atlas::Atlas(Rendering::TextureCache& textureCache)
		: m_textureIdentifier(textureCache.RegisterProceduralAsset(
				[this](const Rendering::TextureIdentifier, const Asset::Guid)
				{
					return Rendering::TextureInfo{Rendering::TextureLoadingCallback(*this, &Atlas::LoadRenderTexture)};
				},
				Guid::Generate()
			))
	{
	}

	void Atlas::LoadGlyphs(
		Font&& font,
		const Point size,
		const float devicePixelRatio,
		const EnumFlags<Modifier> fontModifiers,
		const Weight fontWeight,
		const ConstUnicodeStringView characters
	)
	{
		m_font = Forward<Font>(font);
		m_characters = characters;
		Font& assignedFont = m_font;

		m_glyphInfoMap.Reserve(characters.GetSize());
		FT_Face pFace = assignedFont.GetFace();

		assignedFont.SwitchVariation(fontWeight, fontModifiers);

		FT_Size_RequestRec sizeRequest;
		sizeRequest.type = FT_SIZE_REQUEST_TYPE_REAL_DIM;
		sizeRequest.width = 0;
		sizeRequest.height = (uint32)(size.GetPixels() * devicePixelRatio * 64.f);
		sizeRequest.horiResolution = 0;
		sizeRequest.vertResolution = 0;
		FT_Request_Size(pFace, &sizeRequest);

		Math::Vector2ui maximumGlyphSize = Math::Zero;
		for (const UnicodeCharType glyph : characters)
		{
			const uint32 glyphIndex = FT_Get_Char_Index(assignedFont.GetFace(), glyph);
			FT_Load_Glyph(pFace, glyphIndex, FT_LOAD_BITMAP_METRICS_ONLY);

			const Math::Vector2ui glyphSize = {pFace->glyph->bitmap.width, pFace->glyph->bitmap.rows};

			m_glyphInfoMap.EmplaceOrAssign(
				UnicodeCharType(glyph),
				GlyphInfo{
					glyphSize,
					Math::Vector2i{pFace->glyph->bitmap_left, pFace->glyph->bitmap_top},
					Math::Vector2ui{(uint32)(pFace->glyph->advance.x >> 6), (uint32)(pFace->glyph->advance.y >> 6)},
				}
			);

			maximumGlyphSize =
				Math::Max(maximumGlyphSize, Math::Vector2ui{uint32(pFace->glyph->metrics.width >> 6), uint32(pFace->glyph->metrics.height >> 6)});
		}

		m_maximumGlyphSize = maximumGlyphSize;
	}

	Optional<Threading::Job*> Atlas::LoadRenderTexture(const Rendering::TextureIdentifier identifier, Rendering::LogicalDevice& logicalDevice)
	{
		struct LoadAtlasTextureJob final : public Threading::Job
		{
			LoadAtlasTextureJob(const Rendering::TextureIdentifier identifier, Rendering::LogicalDevice& logicalDevice, Atlas& atlas)
				: Threading::Job(Threading::JobPriority::LoadFont)
				, m_identifier(identifier)
				, m_logicalDevice(logicalDevice)
				, m_atlas(atlas)
			{
			}
			virtual ~LoadAtlasTextureJob() = default;

			virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override
			{
				switch (m_status)
				{
					case LoadStatus::AwaitingLoad:
					case LoadStatus::AwaitingStagingBufferMapping:
					case LoadStatus::AwaitingRenderTextureCreation:
						break;

					case LoadStatus::AwaitingSubmissionStart:
					{
						if (LIKELY(m_encodedCommandBuffer.IsValid()))
						{
							m_status = LoadStatus::AwaitingTransferCompletion;

							Rendering::QueueSubmissionParameters submissionParameters;
							submissionParameters.m_finishedCallback = [this]()
							{
								Queue(System::Get<Threading::JobManager>());
							};

							m_logicalDevice.GetQueueSubmissionJob(Rendering::QueueFamily::Graphics)
								.Queue(
									GetPriority(),
									ArrayView<const Rendering::EncodedCommandBufferView, uint16>(m_encodedCommandBuffer),
									Move(submissionParameters)
								);
						}
						else
						{
							m_status = LoadStatus::Done;
							m_logicalDevice.GetRenderer().GetTextureCache().OnTextureLoadFailed(m_logicalDevice.GetIdentifier(), m_identifier);
							SignalExecutionFinishedAndDestroying(thread);
							delete this;
						}
					}
					break;
					case LoadStatus::AwaitingTransferCompletion:
					case LoadStatus::Done:
						ExpectUnreachable();
				}
			}

			virtual Result OnExecute(Threading::JobRunnerThread& thread) override
			{
				constexpr Rendering::Format format = Rendering::Format::R8_UNORM;

				Assert(m_atlas.HasLoadedGlyphs());
				if (LIKELY(m_atlas.HasLoadedGlyphs()))
				{
					switch (m_status)
					{
						case LoadStatus::AwaitingLoad:
						{
							Font& font = m_atlas.m_font;

							FT_Face pFace = font.GetFace();
							const ConstUnicodeStringView characters = m_atlas.m_characters;

							Math::Vector2ui largestGlyphSize = {0u, 0u};

							for (const UnicodeCharType glyph : characters)
							{
								const uint32 glyphIndex = FT_Get_Char_Index(font.GetFace(), glyph);
								const FT_Error error = FT_Load_Glyph(pFace, glyphIndex, FT_LOAD_BITMAP_METRICS_ONLY);
								Assert(error == 0);
								if (LIKELY(error == 0))
								{
									largestGlyphSize = Math::Max(largestGlyphSize, Math::Vector2ui{pFace->glyph->bitmap.width, pFace->glyph->bitmap.rows});
								}
							}

							Assert(largestGlyphSize.GetLengthSquared() != 0);
							if (LIKELY(largestGlyphSize.GetLengthSquared() != 0))
							{
								const uint32 requiredArea = largestGlyphSize.x * largestGlyphSize.y * characters.GetSize();
								const uint32 atlasUnitSize = Math::NearestPowerOfTwo((uint32)Math::Ceil(Math::Sqrt((float)requiredArea)));
								m_atlasUnitSize = atlasUnitSize;

								const Rendering::FormatInfo formatInfo = Rendering::GetFormatInfo(format);

								uint32 bytesPerLayer = formatInfo.GetBytesPerLayer(Math::Vector2ui{atlasUnitSize});
								if constexpr (RENDERER_WEBGPU)
								{
									bytesPerLayer = Memory::Align(bytesPerLayer, 256);
								}

								m_textureStagingBuffer = Rendering::StagingBuffer(
									m_logicalDevice,
									m_logicalDevice.GetPhysicalDevice(),
									m_logicalDevice.GetDeviceMemoryPool(),
									bytesPerLayer,
									Rendering::StagingBuffer::Flags::TransferSource
								);

								m_status = LoadStatus::AwaitingStagingBufferMapping;
								const bool executedAsynchronously = m_textureStagingBuffer.MapToHostMemoryAsync(
									m_logicalDevice,
									Math::Range<size>::Make(0, m_textureStagingBuffer.GetSize()),
									Rendering::Buffer::MapMemoryFlags::Write,
									[this, atlasUnitSize, largestGlyphSize, characters, &font, pFace, &glyphInfoMap = m_atlas.m_glyphInfoMap](
										const Rendering::Buffer::MapMemoryStatus status,
										ByteView data,
										const bool executedAsynchronously
									)
									{
										Assert(status == Rendering::Buffer::MapMemoryStatus::Success);
										if (LIKELY(status == Rendering::Buffer::MapMemoryStatus::Success))
										{
											const uint32 numCharactersPerRow = atlasUnitSize / largestGlyphSize.x;

											for (auto it = characters.begin(), end = characters.end(); it != end; ++it)
											{
												const uint32 glyphIndex = FT_Get_Char_Index(font.GetFace(), *it);

												const FT_Error loadResult = FT_Load_Glyph(pFace, glyphIndex, FT_LOAD_RENDER);
												if (UNLIKELY(loadResult != 0))
												{
													continue;
												}
												const FT_Error renderResult = FT_Render_Glyph(pFace->glyph, FT_RENDER_MODE_NORMAL);
												if (UNLIKELY(renderResult != 0))
												{
													continue;
												}

												const uint32 characterIndex = characters.GetIteratorIndex(it);
												const uint32 atlasColumnIndex = characterIndex % numCharactersPerRow;
												const uint32 atlasRowIndex = (characterIndex - atlasColumnIndex) / numCharactersPerRow;

												ByteView atlasMemoryView =
													data + size(atlasColumnIndex * largestGlyphSize.x + atlasRowIndex * atlasUnitSize * largestGlyphSize.y);

												const Math::Vector2ui glyphSize = {pFace->glyph->bitmap.width, pFace->glyph->bitmap.rows};

												for (uint8 *pGlyphMemory = pFace->glyph->bitmap.buffer, *pGlyphEnd = pGlyphMemory + glyphSize.x * glyphSize.y;
											       pGlyphMemory != pGlyphEnd;
											       pGlyphMemory += glyphSize.x)
												{
													atlasMemoryView.CopyFrom(ConstByteView{pGlyphMemory, glyphSize.x});

													atlasMemoryView += atlasUnitSize;
												}

												GlyphInfo& glyphInfo = glyphInfoMap.Find(*it)->second;
												glyphInfo.m_atlasCoordinates = Math::Vector2f{
													(float)(atlasColumnIndex * largestGlyphSize.x) / (float)atlasUnitSize,
													(float)(atlasRowIndex * largestGlyphSize.y) / (float)atlasUnitSize
												};
												glyphInfo.m_atlasScale = (Math::Vector2f)glyphSize / (float)atlasUnitSize;
											}
										}

										m_status = LoadStatus::AwaitingRenderTextureCreation;
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
							}
							else
							{
								return Result::AwaitExternalFinish;
							}
						}
							[[fallthrough]];
						case LoadStatus::AwaitingRenderTextureCreation:
						{
							Threading::EngineJobRunnerThread& engineThreadRunner = static_cast<Threading::EngineJobRunnerThread&>(thread);

							const uint32 atlasUnitSize = m_atlasUnitSize;
							m_renderTexture = Rendering::RenderTexture(
								m_logicalDevice,
								m_logicalDevice.GetPhysicalDevice(),
								format,
								Rendering::SampleCount::One,
								Rendering::ImageFlags{},
								Math::Vector3ui{atlasUnitSize, atlasUnitSize, 1},
								Rendering::UsageFlags::TransferDestination | Rendering::UsageFlags::Sampled,
								Rendering::ImageLayout::Undefined,
								Rendering::MipMask::FromSizeToLargest({atlasUnitSize, atlasUnitSize}),
								Rendering::MipMask::FromSizeToLargest({atlasUnitSize, atlasUnitSize}),
								1
							);

							m_pSubmittingThread = &engineThreadRunner;
							m_commandBuffer = Rendering::CommandBuffer(
								m_logicalDevice,
								engineThreadRunner.GetRenderData().GetCommandPool(m_logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics),
								m_logicalDevice.GetCommandQueue(Rendering::QueueFamily::Graphics)
							);
							Rendering::CommandEncoder commandEncoder = m_commandBuffer.BeginEncoding(m_logicalDevice);

							{
								Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

								barrierCommandEncoder.TransitionImageLayout(
									Rendering::PipelineStageFlags::Transfer,
									Rendering::AccessFlags::TransferWrite,
									Rendering::ImageLayout::TransferDestinationOptimal,
									m_renderTexture,
									Rendering::ImageSubresourceRange{
										Rendering::ImageAspectFlags::Color,
										Rendering::MipRange{0, 1},
										Rendering::ArrayRange{0, 1}
									}
								);
							}

							{
								const Rendering::FormatInfo formatInfo = Rendering::GetFormatInfo(format);

								Math::Vector2ui bytesPerDimension = formatInfo.GetBytesPerDimension(Math::Vector2ui{atlasUnitSize});
								if constexpr (RENDERER_WEBGPU)
								{
									bytesPerDimension.x = Memory::Align(bytesPerDimension.x, 256);
								}

								const Array<Rendering::BufferImageCopy, 1> imageCopies = {Rendering::BufferImageCopy{
									0,
									bytesPerDimension,
									formatInfo.GetBlockCount(Math::Vector2ui{Math::Vector2ui{atlasUnitSize, atlasUnitSize}}),
									formatInfo.m_blockExtent,
									Rendering::SubresourceLayers{Rendering::ImageAspectFlags::Color, 0, Rendering::ArrayRange{0u, 1}},
									Math::Vector3i{0, 0, 0},
									Math::Vector3ui{atlasUnitSize, atlasUnitSize, 1}
								}};

								Rendering::BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
								blitCommandEncoder.RecordCopyBufferToImage(
									m_textureStagingBuffer,
									m_renderTexture,
									Rendering::ImageLayout::TransferDestinationOptimal,
									imageCopies.GetView()
								);
							}

							{
								Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

								barrierCommandEncoder.TransitionImageLayout(
									Rendering::PipelineStageFlags::FragmentShader,
									Rendering::AccessFlags::ShaderRead,
									Rendering::ImageLayout::ShaderReadOnlyOptimal,
									m_renderTexture,
									Rendering::ImageSubresourceRange{
										Rendering::ImageAspectFlags::Color,
										Rendering::MipRange{0, 1},
										Rendering::ArrayRange{0, 1}
									}
								);
							}

							m_status = LoadStatus::AwaitingSubmissionStart;

							m_encodedCommandBuffer = commandEncoder.StopEncoding();
							return Result::AwaitExternalFinish;
						}
						case LoadStatus::AwaitingSubmissionStart:
						case LoadStatus::AwaitingStagingBufferMapping:
							ExpectUnreachable();
						case LoadStatus::AwaitingTransferCompletion:
						{
							m_status = LoadStatus::Done;
						}
							[[fallthrough]];
						case LoadStatus::Done:
						{
							thread.QueueCallbackFromThread(
								Threading::JobPriority::DeallocateResourcesMin,
								[stagingBuffer = Move(m_textureStagingBuffer), &logicalDevice = m_logicalDevice](Threading::JobRunnerThread&) mutable
								{
									stagingBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
								}
							);

							Rendering::RenderTexture previousTexture;
							m_logicalDevice.GetRenderer().GetTextureCache().AssignRenderTexture(
								m_logicalDevice.GetIdentifier(),
								m_identifier,
								Move(m_renderTexture),
								previousTexture,
								Rendering::LoadedTextureFlags{}
							);
							m_logicalDevice.GetRenderer().GetTextureCache().OnTextureLoadFinished(m_logicalDevice.GetIdentifier(), m_identifier);

							if (previousTexture.IsValid())
							{
								Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
								engineThread.GetRenderData().DestroyImage(m_logicalDevice.GetIdentifier(), Move(previousTexture));
							}

							Threading::EngineJobRunnerThread& submittingEngineThread = static_cast<Threading::EngineJobRunnerThread&>(*m_pSubmittingThread
							);
							submittingEngineThread.GetRenderData()
								.DestroyCommandBuffer(m_logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics, Move(m_commandBuffer));

							return Result::FinishedAndDelete;
						}
					}

					ExpectUnreachable();
				}
				else
				{
					m_logicalDevice.GetRenderer().GetTextureCache().OnTextureLoadFailed(m_logicalDevice.GetIdentifier(), m_identifier);
					return Result::FinishedAndDelete;
				}
			}
#if STAGE_DEPENDENCY_PROFILING
			[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
			{
				return "Load Font Render Texture";
			}
#endif
		protected:
			enum class LoadStatus : uint8
			{
				AwaitingLoad,
				AwaitingStagingBufferMapping,
				AwaitingRenderTextureCreation,
				AwaitingSubmissionStart,
				AwaitingTransferCompletion,
				Done
			};

			LoadStatus m_status = LoadStatus::AwaitingLoad;
			const Rendering::TextureIdentifier m_identifier;
			Rendering::LogicalDevice& m_logicalDevice;
			Atlas& m_atlas;
			Rendering::CommandBuffer m_commandBuffer;
			Rendering::EncodedCommandBuffer m_encodedCommandBuffer;
			Rendering::StagingBuffer m_textureStagingBuffer;
			Rendering::RenderTexture m_renderTexture;
			Optional<Threading::JobRunnerThread*> m_pSubmittingThread;

			uint32 m_atlasUnitSize;
		};

		return new LoadAtlasTextureJob(identifier, logicalDevice, *this);
	}

	uint32 Atlas::CalculateWidth(const ConstUnicodeStringView text) const
	{
		if (text.IsEmpty())
		{
			return 0;
		}

		uint32 size;
		{
			const Optional<const GlyphInfo*> pFirstGlyphInfo = GetGlyphInfo(text[0]);
			Assert(pFirstGlyphInfo.IsValid());
			if (LIKELY(pFirstGlyphInfo.IsValid()))
			{
				size = pFirstGlyphInfo->m_offset.x;
			}
			else
			{
				size = 0;
			}
		}

		for (const UnicodeCharType glyph : text.GetSubView(0, text.GetSize() - 1u))
		{
			const Optional<const GlyphInfo*> pGlyphInfo = GetGlyphInfo(glyph);
			Assert(pGlyphInfo.IsValid());
			if (LIKELY(pGlyphInfo.IsValid()))
			{
				size += pGlyphInfo->m_advance.x;
			}
		}

		{
			const Optional<const GlyphInfo*> pLastGlyphInfo = GetGlyphInfo(text.GetLastElement());
			Assert(pLastGlyphInfo.IsValid());
			if (LIKELY(pLastGlyphInfo.IsValid()))
			{
				size += pLastGlyphInfo->m_advance.x;
			}
		}

		return size;
	}

	Math::Vector2ui Atlas::CalculateSize(const ConstUnicodeStringView text) const
	{
		if (text.IsEmpty())
		{
			return Math::Zero;
		}

		int32 maxOffset = 0;
		for (const UnicodeCharType glyph : text)
		{
			const Optional<const GlyphInfo*> pGlyphInfo = GetGlyphInfo(glyph);
			Assert(pGlyphInfo.IsValid());
			if (LIKELY(pGlyphInfo.IsValid()))
			{
				maxOffset = Math::Max(maxOffset, (int)pGlyphInfo->m_pixelSize.y - pGlyphInfo->m_offset.y);
			}
		}

		Math::Vector2i size;
		{
			const Optional<const GlyphInfo*> pFirstGlyphInfo = GetGlyphInfo(text[0]);
			Assert(pFirstGlyphInfo.IsValid());
			if (LIKELY(pFirstGlyphInfo.IsValid()))
			{
				size = {pFirstGlyphInfo->m_offset.x, 0};
			}
			else
			{
				size = Math::Zero;
			}
		}

		for (const UnicodeCharType glyph : text.GetSubView(0, text.GetSize() - 1u))
		{
			const Optional<const GlyphInfo*> pGlyphInfo = GetGlyphInfo(glyph);
			Assert(pGlyphInfo.IsValid());
			if (LIKELY(pGlyphInfo.IsValid()))
			{
				size.x += pGlyphInfo->m_advance.x;
				size.y = (int32)Math::Max((uint32)size.y, pGlyphInfo->m_pixelSize.y);
			}
		}

		{
			const Optional<const GlyphInfo*> pLastGlyphInfo = GetGlyphInfo(text.GetLastElement());
			Assert(pLastGlyphInfo.IsValid());
			if (LIKELY(pLastGlyphInfo.IsValid()))
			{
				size.x += pLastGlyphInfo->m_advance.x;
				size.y = (int32)Math::Max((uint32)size.y, pLastGlyphInfo->m_pixelSize.y) + maxOffset;
			}
		}

		return (Math::Vector2ui)size;
	}

	UnicodeStringView Atlas::TrimStringToFit(UnicodeStringView string, const uint32 maximumWidth) const
	{
		constexpr ConstStringView shortenedIndicator = "...";

		for (uint8 i = 0; i < shortenedIndicator.GetSize() && string.GetSize() > i; ++i)
		{
			string[string.GetSize() - 1 - i] = shortenedIndicator[i];
		}

		while (string.GetSize() > 3)
		{
			const uint32 newWidth = CalculateSize(string).x;
			if (newWidth <= maximumWidth)
			{
				break;
			}

			string--;
			string[string.GetSize() - 3] = '.';
		};

		return string;
	}
}
