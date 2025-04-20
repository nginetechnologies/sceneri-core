#include "Sampling.h"
#include "RenderHelper.h"
#include <algorithm>

#include "3rdparty/libktx/include/ktx.h"

#include <Renderer/Devices/LogicalDeviceView.h>

PUSH_MSVC_WARNINGS
DISABLE_MSVC_WARNINGS(5219 4619 5266)

PUSH_CLANG_WARNINGS
DISABLE_CLANG_WARNING("-Wnan-infinity-disabled")

#include <EditorCommon/3rdparty/gli/gli.h>

POP_CLANG_WARNINGS
POP_MSVC_WARNINGS

#include <Renderer/Commands/CommandBuffer.h>
#include <Renderer/Commands/CommandEncoder.h>
#include <Renderer/Commands/EncodedCommandBuffer.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/Commands/ClearValue.h>
#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Assets/Texture/RenderTexture.h>

#include <Renderer/Descriptors/DescriptorSetLayout.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Wrappers/PipelineLayout.h>
#include <Renderer/Pipelines/PushConstantRange.h>
#include <Renderer/Pipelines/GraphicsPipeline.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/FormatInfo.h>

#include <Common/IO/File.h>
#include <Common/EnumFlags.h>
#include <Common/IO/Path.h>
#include <Common/Asset/Context.h>
#include <Common/Math/Color.h>
#include <Common/Math/Vector3.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::AssetCompiler
{
	Rendering::RenderTexture BlitImage(
		RenderHelper& renderHelper,
		const Rendering::CommandEncoderView commandEncoder,
		Rendering::RenderTexture& sourceImage,
		const Math::Vector2ui sourceSize,
		const Rendering::MipMask sourceMipMask,
		const uint8 sourceArrayLevelCount,
		const Rendering::Image::Flags flags,
		const Rendering::Format _dstFormat
	)
	{
		Rendering::RenderTexture image(
			*renderHelper.m_pLogicalDevice,
			*renderHelper.m_pPhysicalDevice,
			_dstFormat,
			Rendering::SampleCount::One,
			flags,
			Math::Vector3ui{sourceSize.x, sourceSize.y, 1u},
			Rendering::UsageFlags::TransferSource | Rendering::UsageFlags::TransferDestination,
			Rendering::ImageLayout::Undefined,
			sourceMipMask,
			sourceMipMask,
			sourceArrayLevelCount
		);
		if (!image.IsValid())
		{
			return {};
		}

		const Rendering::ImageSubresourceRange subresourceRange{
			Rendering::ImageAspectFlags::Color,
			sourceMipMask.GetRange(sourceMipMask.GetSize()),
			Rendering::ArrayRange{0, sourceArrayLevelCount}
		};

		{
			Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

			barrierCommandEncoder.TransitionImageLayout(
				Rendering::PipelineStageFlags::Transfer,
				Rendering::AccessFlags::TransferWrite,
				Rendering::ImageLayout::TransferDestinationOptimal,
				image,
				subresourceRange
			);
			barrierCommandEncoder.TransitionImageLayout(
				Rendering::PipelineStageFlags::Transfer,
				Rendering::AccessFlags::TransferRead,
				Rendering::ImageLayout::TransferSourceOptimal,
				sourceImage,
				subresourceRange
			);
		}

		uint32 currentSideWidth = sourceSize.x;
		uint32 currentSideHeight = sourceSize.y;

		FixedSizeVector<Rendering::ImageBlit> imageBlits(Memory::ConstructWithSize, Memory::Zeroed, sourceMipMask.GetSize());
		for (uint32 level = 0, sourceMipCount = sourceMipMask.GetSize(); level < sourceMipCount; level++)
		{
			imageBlits[level] = Rendering::ImageBlit{
				// Source
				Rendering::SubresourceLayers{Rendering::ImageAspectFlags::Color, level, Rendering::ArrayRange{0, sourceArrayLevelCount}},
				Math::Zero,
				Math::Vector3ui{currentSideWidth, currentSideHeight, 1},
				// Target
				Rendering::SubresourceLayers{Rendering::ImageAspectFlags::Color, level, Rendering::ArrayRange{0, sourceArrayLevelCount}},
				Math::Zero,
				Math::Vector3ui{currentSideWidth, currentSideHeight, 1}
			};

			currentSideWidth = currentSideWidth >> 1;
			currentSideHeight = currentSideHeight >> 1;
		}

		{
			Rendering::BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
			blitCommandEncoder.RecordBlitImage(
				image,
				Rendering::ImageLayout::TransferDestinationOptimal,
				sourceImage,
				Rendering::ImageLayout::TransferSourceOptimal,
				imageBlits.GetView()
			);
		}

		return image;
	}

	bool DownloadCubemap(
		RenderHelper& renderHelper,
		Rendering::RenderTexture& sourceImage,
		const Rendering::Format sourceFormat,
		const Math::Vector2ui sourceSize,
		const uint16 sourceMipCount,
		const uint32 cubeMapByteSize,
		gli::texture_cube& cubemapOut
	)
	{
		Rendering::StagingBuffer stagingBuffer;

		size totalImageSize = 0;
		{
			uint32 currentSideWidth = sourceSize.x;
			uint32 currentSideHeight = sourceSize.y;

			for (uint16 level = 0; level < sourceMipCount; level++)
			{
				totalImageSize += currentSideWidth * currentSideHeight * cubeMapByteSize * 6;

				currentSideWidth = currentSideWidth >> 1;
				currentSideHeight = currentSideHeight >> 1;
			}

			stagingBuffer = Rendering::StagingBuffer(
				*renderHelper.m_pLogicalDevice,
				*renderHelper.m_pPhysicalDevice,
				renderHelper.m_pLogicalDevice->GetDeviceMemoryPool(),
				totalImageSize,
				Rendering::StagingBuffer::Flags::TransferDestination
			);
			if (!stagingBuffer.IsValid())
			{
				return false;
			}
		}

#if RENDERER_HAS_COMMAND_POOL
		Rendering::CommandBuffer commandBuffer;
		renderHelper.m_commandPool.AllocateCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
		Rendering::CommandBuffer commandBuffer(
			*renderHelper.m_pLogicalDevice,
			renderHelper.m_commandPool,
			renderHelper.m_pLogicalDevice->GetCommandQueue(Rendering::QueueFamily::Graphics)
		);
#endif

		if (!commandBuffer.IsValid())
		{
			return false;
		}

		Rendering::CommandEncoder commandEncoder =
			commandBuffer.BeginEncoding(*renderHelper.m_pLogicalDevice, Rendering::CommandBuffer::Flags::OneTimeSubmit);
		if (!commandEncoder.IsValid())
		{
			return false;
		}

		// barrier on complete image
		const Rendering::ImageSubresourceRange subresourceRange{
			Rendering::ImageAspectFlags::Color,
			Rendering::MipRange{0, sourceMipCount},
			Rendering::ArrayRange{0, 6u}
		};

		{
			Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

			barrierCommandEncoder.TransitionImageLayout(
				Rendering::PipelineStageFlags::Transfer,
				Rendering::AccessFlags::TransferRead,
				Rendering::ImageLayout::TransferSourceOptimal,
				sourceImage,
				subresourceRange
			);
		}

		// copy all faces & levels into staging buffers
		{
			FixedSizeVector<Rendering::BufferImageCopy> regions(Memory::ConstructWithSize, Memory::Zeroed, sourceMipCount * 6u);
			size stagingBufferOffset = 0;

			const Rendering::FormatInfo& formatInfo = Rendering::GetFormatInfo(sourceFormat);

			for (uint32 face = 0; face < 6u; face++)
			{
				uint32 currentSideWidth = sourceSize.x;
				uint32 currentSideHeight = sourceSize.y;

				for (uint32 level = 0; level < sourceMipCount; level++)
				{
					regions[face * sourceMipCount + level] = Rendering::BufferImageCopy{
						stagingBufferOffset,
						formatInfo.GetBytesPerDimension(Math::Vector2ui{currentSideWidth, currentSideHeight}),
						formatInfo.GetBlockCount(Math::Vector2ui{Math::Vector2ui{currentSideWidth, currentSideHeight}}),
						formatInfo.m_blockExtent,
						Rendering::SubresourceLayers{Rendering::ImageAspectFlags::Color, level, Rendering::ArrayRange{face, 1u}},
						Math::Vector3i{Math::Zero},
						Math::Vector3ui{currentSideWidth, currentSideHeight, 1u}
					};

					stagingBufferOffset += currentSideWidth * currentSideHeight * cubeMapByteSize;

					currentSideWidth = currentSideWidth >> 1;
					currentSideHeight = currentSideHeight >> 1;
				}
			}

			{
				Rendering::BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
				blitCommandEncoder
					.RecordCopyImageToBuffer(sourceImage, Rendering::ImageLayout::TransferSourceOptimal, stagingBuffer, regions.GetView());
			}
		}

		Rendering::EncodedCommandBuffer encodedCommandBuffer = commandEncoder.StopEncoding();
		if (!encodedCommandBuffer.IsValid())
		{
			return false;
		}

		if (!renderHelper.executeCommandBuffers(ArrayView<const Rendering::EncodedCommandBufferView>{encodedCommandBuffer}))
		{
			return false;
		}

#if RENDERER_HAS_COMMAND_POOL
		renderHelper.m_commandPool.FreeCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
		commandBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_commandPool);
#endif

		// Image is copied to buffer
		// Now map buffer and copy to ram
		{
			ByteView cubemapData{reinterpret_cast<ngine::ByteType*>(cubemapOut.data()), totalImageSize};

			stagingBuffer.MapToHostMemory(
				*renderHelper.m_pLogicalDevice,
				Math::Range<size>::Make(0, cubemapData.GetDataSize()),
				Rendering::Buffer::MapMemoryFlags::Read,
				[cubemapData](
					[[maybe_unused]] const Rendering::Buffer::MapMemoryStatus status,
					const ByteView data,
					[[maybe_unused]] const bool executedAsynchronously
				)
				{
					Assert(status == Rendering::Buffer::MapMemoryStatus::Success);
					Assert(!executedAsynchronously);
					cubemapData.CopyFrom(data);
				}
			);

			stagingBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
		}

		return true;
	}

	bool Download2DImage(
		RenderHelper& renderHelper,
		Rendering::RenderTexture& sourceImage,
		const Rendering::Format sourceImageFormat,
		const Math::Vector2ui sourceSize,
		const uint32 cubeMapByteSize,
		gli::texture2d& brdfOut
	)
	{
		const uint32 width = sourceSize.x;
		const uint32 height = sourceSize.y;
		const size imageByteSize = width * height * cubeMapByteSize;

		Rendering::StagingBuffer stagingBuffer(
			*renderHelper.m_pLogicalDevice,
			*renderHelper.m_pPhysicalDevice,
			renderHelper.m_pLogicalDevice->GetDeviceMemoryPool(),
			imageByteSize,
			Rendering::StagingBuffer::Flags::TransferDestination
		);
		if (!stagingBuffer.IsValid())
		{
			return false;
		}

#if RENDERER_HAS_COMMAND_POOL
		Rendering::CommandBuffer commandBuffer;
		renderHelper.m_commandPool.AllocateCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
		Rendering::CommandBuffer commandBuffer(
			*renderHelper.m_pLogicalDevice,
			renderHelper.m_commandPool,
			renderHelper.m_pLogicalDevice->GetCommandQueue(Rendering::QueueFamily::Graphics)
		);
#endif

		if (!commandBuffer.IsValid())
		{
			stagingBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
			return false;
		}

		Rendering::CommandEncoder commandEncoder =
			commandBuffer.BeginEncoding(*renderHelper.m_pLogicalDevice, Rendering::CommandBuffer::Flags::OneTimeSubmit);
		if (!commandEncoder.IsValid())
		{
			stagingBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
			return false;
		}

		// barrier on complete image
		{
			Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

			barrierCommandEncoder.TransitionImageLayout(
				Rendering::PipelineStageFlags::Transfer,
				Rendering::AccessFlags::TransferRead,
				Rendering::ImageLayout::TransferSourceOptimal,
				sourceImage,
				Rendering::ImageSubresourceRange{Rendering::ImageAspectFlags::Color}
			);
		}

		// copy 2D image to buffer
		{
			const Rendering::FormatInfo& formatInfo = Rendering::GetFormatInfo(sourceImageFormat);

			Rendering::BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
			blitCommandEncoder.RecordCopyImageToBuffer(
				sourceImage,
				Rendering::ImageLayout::TransferSourceOptimal,
				stagingBuffer,
				Array<Rendering::BufferImageCopy, 1>{
					Rendering::BufferImageCopy{
						0,
						formatInfo.GetBytesPerDimension(sourceSize),
						formatInfo.GetBlockCount(sourceSize),
						formatInfo.m_blockExtent,
						Rendering::SubresourceLayers{Rendering::ImageAspectFlags::Color, 0u, Rendering::ArrayRange{0u, 1}},
						{0, 0, 0},
						{sourceSize.x, sourceSize.y, 1}
					}
				}.GetView()
			);
		}

		Rendering::EncodedCommandBuffer encodedCommandBuffer = commandEncoder.StopEncoding();
		if (!encodedCommandBuffer.IsValid())
		{
			stagingBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
			return false;
		}

		if (!renderHelper.executeCommandBuffers(ArrayView<const Rendering::EncodedCommandBufferView>{encodedCommandBuffer}))
		{
			stagingBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
			return false;
		}

#if RENDERER_HAS_COMMAND_POOL
		renderHelper.m_commandPool.FreeCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
		commandBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_commandPool);
#endif

		// Image is copied to buffer
		// Now map buffer and copy to ram
		{
			Assert(brdfOut.size() == imageByteSize);

			ByteView brdfData{reinterpret_cast<ByteType*>(brdfOut.data()), imageByteSize};

			stagingBuffer.MapToHostMemory(
				*renderHelper.m_pLogicalDevice,
				Math::Range<size>::Make(0, brdfData.GetDataSize()),
				Rendering::Buffer::MapMemoryFlags::Read,
				[brdfData](
					[[maybe_unused]] const Rendering::Buffer::MapMemoryStatus status,
					const ByteView data,
					[[maybe_unused]] const bool executedAsynchronously
				)
				{
					Assert(status == Rendering::Buffer::MapMemoryStatus::Success);
					Assert(!executedAsynchronously);
					brdfData.CopyFrom(data);
				}
			);

			stagingBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
		}

		return true;
	}

	void GenerateMipmapLevels(
		const Rendering::CommandEncoderView commandEncoder,
		Rendering::RenderTexture& _image,
		const uint16 _maxMipLevels,
		const uint32 _sideWidth,
		const uint32 sideHeight
	)
	{
		{
			Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

			barrierCommandEncoder.TransitionImageLayout(
				Rendering::PipelineStageFlags::Transfer,
				Rendering::AccessFlags::TransferRead,
				Rendering::ImageLayout::TransferSourceOptimal,
				_image,
				Rendering::ImageSubresourceRange{Rendering::ImageAspectFlags::Color, Rendering::MipRange{0, 1}, Rendering::ArrayRange{0, 6u}}
			);

			barrierCommandEncoder.TransitionImageLayout(
				Rendering::PipelineStageFlags::Transfer,
				Rendering::AccessFlags::TransferWrite,
				Rendering::ImageLayout::TransferDestinationOptimal,
				_image,
				Rendering::ImageSubresourceRange{Rendering::ImageAspectFlags::Color, Rendering::MipRange{1, 1}, Rendering::ArrayRange{0, 6u}}
			);

			barrierCommandEncoder.TransitionImageLayout(
				Rendering::PipelineStageFlags::Transfer,
				Rendering::AccessFlags::TransferRead,
				Rendering::ImageLayout::TransferSourceOptimal,
				_image,
				Rendering::ImageSubresourceRange{
					Rendering::ImageAspectFlags::Color,
					Rendering::MipRange{2, _maxMipLevels - 2u},
					Rendering::ArrayRange{0, 6u}
				}
			);
		}

		for (uint16 i = 1; i < _maxMipLevels; i++)
		{
			const Array<const Rendering::ImageBlit, 1> blits{Rendering::ImageBlit{
				// Source
				Rendering::SubresourceLayers{Rendering::ImageAspectFlags::Color, i - 1u, Rendering::ArrayRange{0, 6}},
				Math::Zero,
				Math::Vector3ui{_sideWidth >> (i - 1), sideHeight >> (i - 1), 1},
				// Target
				Rendering::SubresourceLayers{Rendering::ImageAspectFlags::Color, i, Rendering::ArrayRange{0, 6}},
				Math::Zero,
				Math::Vector3ui{_sideWidth >> i, sideHeight >> i, 1}
			}};

			{
				Rendering::BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
				blitCommandEncoder.RecordBlitImage(
					_image,
					Rendering::ImageLayout::TransferDestinationOptimal,
					_image,
					Rendering::ImageLayout::TransferSourceOptimal,
					blits.GetView()
				);
			}

			Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

			barrierCommandEncoder.TransitionImageLayout(
				Rendering::PipelineStageFlags::Transfer,
				Rendering::AccessFlags::TransferRead,
				Rendering::ImageLayout::TransferSourceOptimal,
				_image,
				Rendering::ImageSubresourceRange{Rendering::ImageAspectFlags::Color, Rendering::MipRange{i, 1}, Rendering::ArrayRange{0, 6u}}
			);

			if (i + 1 < _maxMipLevels)
			{
				barrierCommandEncoder.TransitionImageLayout(
					Rendering::PipelineStageFlags::Transfer,
					Rendering::AccessFlags::TransferWrite,
					Rendering::ImageLayout::TransferDestinationOptimal,
					_image,
					Rendering::ImageSubresourceRange{Rendering::ImageAspectFlags::Color, Rendering::MipRange{i + 1u, 1}, Rendering::ArrayRange{0, 6u}}
				);
			}
		}

		{
			Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

			barrierCommandEncoder.TransitionImageLayout(
				Rendering::PipelineStageFlags::FragmentShader,
				Rendering::AccessFlags::ShaderRead,
				Rendering::ImageLayout::ShaderReadOnlyOptimal,
				_image,
				Rendering::ImageSubresourceRange{
					Rendering::ImageAspectFlags::Color,
					Rendering::MipRange{0, _maxMipLevels},
					Rendering::ArrayRange{0, 6u}
				}
			);
		}
	}

	bool Sample(
		RenderHelper& renderHelper,
		const Rendering::ImageView sourceCubemapImage,
		gli::texture_cube& cubemapOut,
		Distribution _distribution,
		const Rendering::MipMask mipMask,
		uint32 _sampleCount,
		ngine::Rendering::Format _targetFormat,
		float _lodBias
	)
	{
		Assert(sourceCubemapImage.IsValid());
		const Rendering::Format cubeMapFormat = Rendering::Format::R32G32B32A32_SFLOAT;
		const Math::Vector2ui cubemapSize = {(uint32)cubemapOut.extent().x, (uint32)cubemapOut.extent().y};

		using namespace ngine;

		constexpr Asset::Guid vertexShaderAssetGuid = "C8B836B2-50C9-49ED-8F4E-08C672B7D510"_asset;

		Rendering::RenderTexture outputCubeMap(
			*renderHelper.m_pLogicalDevice,
			*renderHelper.m_pPhysicalDevice,
			cubeMapFormat,
			Rendering::SampleCount::One,
			Rendering::Image::Flags::Cubemap,
			Math::Vector3ui{cubemapSize.x, cubemapSize.y, 1u},
			Rendering::UsageFlags::ColorAttachment | Rendering::UsageFlags::TransferSource | Rendering::UsageFlags::Sampled,
			Rendering::ImageLayout::Undefined,
			mipMask,
			mipMask,
			6u
		);
		if (!outputCubeMap.IsValid())
		{
			return false;
		}

		const Rendering::MipMask::StoredType _mipmapCount = mipMask.GetSize();
		FixedSizeVector<Rendering::ImageMapping> outputCubeMapViews(Memory::ConstructWithSize, Memory::DefaultConstruct, _mipmapCount * 6);
		for (uint16 mipIndex = 0; mipIndex < _mipmapCount; ++mipIndex)
		{
			for (uint8 faceIndex = 0; faceIndex < 6; faceIndex++)
			{
				outputCubeMapViews[mipIndex * 6 + faceIndex] = Rendering::ImageMapping(
					*renderHelper.m_pLogicalDevice,
					outputCubeMap,
					Rendering::ImageMappingType::TwoDimensional,
					cubeMapFormat,
					Rendering::ImageAspectFlags::Color,
					Rendering::MipRange{mipIndex, 1u},
					Rendering::ArrayRange{faceIndex, 1u}
				);
				if (!outputCubeMapViews[mipIndex * 6 + faceIndex].IsValid())
				{
					return false;
				}
			}
		}

		Rendering::ImageMapping outputCubeMapCompleteView = Rendering::ImageMapping(
			*renderHelper.m_pLogicalDevice,
			outputCubeMap,
			Rendering::ImageMappingType::Cube,
			cubeMapFormat,
			Rendering::ImageAspectFlags::Color,
			Rendering::MipRange{0, _mipmapCount},
			Rendering::ArrayRange{0, 6u}
		);
		if (!outputCubeMapCompleteView.IsValid())
		{
			return false;
		}

		auto makeAttachmentDescription = [format = cubeMapFormat]()
		{
			return Rendering::AttachmentDescription{
				static_cast<Rendering::Format>(format),
				Rendering::SampleCount::One,
				Rendering::AttachmentLoadType::Undefined,
				Rendering::AttachmentStoreType::Store,
				Rendering::AttachmentLoadType::Undefined,
				Rendering::AttachmentStoreType::Undefined,
				Rendering::ImageLayout::Undefined,
				Rendering::ImageLayout::ColorAttachmentOptimal
			};
		};

		Array<Rendering::AttachmentDescription, 6> attachments = {Memory::InitializeAll, makeAttachmentDescription()};
		Array<Rendering::AttachmentReference, 6> attachmentReferences{
			Rendering::AttachmentReference{0, Rendering::ImageLayout::ColorAttachmentOptimal},
			Rendering::AttachmentReference{1, Rendering::ImageLayout::ColorAttachmentOptimal},
			Rendering::AttachmentReference{2, Rendering::ImageLayout::ColorAttachmentOptimal},
			Rendering::AttachmentReference{3, Rendering::ImageLayout::ColorAttachmentOptimal},
			Rendering::AttachmentReference{4, Rendering::ImageLayout::ColorAttachmentOptimal},
			Rendering::AttachmentReference{5, Rendering::ImageLayout::ColorAttachmentOptimal},
		};
		Rendering::RenderPass renderPass(*renderHelper.m_pLogicalDevice, attachments, attachmentReferences, {}, {}, {});
		if (!renderPass.IsValid())
		{
			return false;
		}

		// Push Constants for specular and diffuse filter passes
		struct PushConstant
		{
			float roughness = 0.f;
			uint32 sampleCount = 1u;
			uint32 mipLevel = 1u;
			uint32 width = 1024u;
			uint32 height = 1024u;
			float lodBias = 0.f;
			Distribution distribution = Lambertian;
		};

		const Array<Rendering::PushConstantRange, 1> pushConstantRanges{
			Rendering::PushConstantRange{Rendering::ShaderStage::Fragment, 0u, sizeof(PushConstant)}
		};

		////////////////////////////////////////////////////////////////////////////////////////
		// Filter CubeMap Pipeline
		constexpr Asset::Guid filterFragmentShaderAssetGuid = "5200A133-51ED-4292-AB0D-8BD0EAF8058D"_asset;

		Rendering::ImageMapping inputCubeMapCompleteView = Rendering::ImageMapping(
			*renderHelper.m_pLogicalDevice,
			sourceCubemapImage,
			Rendering::ImageMappingType::Cube,
			_targetFormat,
			Rendering::ImageAspectFlags::Color,
			Rendering::MipRange{0, 1u},
			Rendering::ArrayRange{0, 6u}
		);
		if (!inputCubeMapCompleteView.IsValid())
		{
			return {};
		}

		Rendering::Sampler cubeMipMapSampler = Rendering::Sampler(
			*renderHelper.m_pLogicalDevice,
			Rendering::AddressMode::RepeatMirrored,
			Rendering::FilterMode::Linear,
			Rendering::CompareOperation::AlwaysSucceed,
			Math::Range<int16>::MakeStartToEnd(0u, 2)
		);
		if (!cubeMipMapSampler.IsValid())
		{
			return {};
		}

		Rendering::DescriptorSetLayout filterSetLayout(
			*renderHelper.m_pLogicalDevice,
			Array{
				Rendering::DescriptorSetLayout::Binding::MakeSampledImage(
					0u,
					Rendering::ShaderStage::Fragment,
					Rendering::SampledImageType::Float,
					Rendering::ImageMappingType::TwoDimensional
				),
				Rendering::DescriptorSetLayout::Binding::MakeSampler(
					1u,
					Rendering::ShaderStage::Fragment,
					Rendering::SamplerBindingType::Filtering
				),
			}
		);
		if (!filterSetLayout.IsValid())
		{
			return false;
		}

		Rendering::DescriptorSet filterDescriptorSet;
		if (!renderHelper.m_descriptorPool.AllocateDescriptorSets(
					*renderHelper.m_pLogicalDevice,
					ArrayView<const Rendering::DescriptorSetLayoutView>{filterSetLayout},
					ArrayView<Rendering::DescriptorSet>{filterDescriptorSet}
				))
		{
			return false;
		}

		Rendering::GraphicsPipeline filterGraphicsPipeline;
		filterGraphicsPipeline.CreateBase(
			*renderHelper.m_pLogicalDevice,
			ArrayView<const Rendering::DescriptorSetLayoutView>{filterSetLayout},
			pushConstantRanges.GetDynamicView()
		);
		{
			const uint8 subpassIndex = 0;

			const Rendering::VertexStageInfo vertexStage{Rendering::ShaderStageInfo{vertexShaderAssetGuid}};
			const Math::Rectangleui renderArea{Math::Zero, cubemapSize};
			const Math::Rectangleui outputArea = renderArea;

			const Rendering::PrimitiveInfo primitiveInfo{
				Rendering::PrimitiveTopology::TriangleList,
				Rendering::PolygonMode::Fill,
				Rendering::WindingOrder::CounterClockwise,
				Rendering::CullMode::None
			};

			const Array<Rendering::Viewport, 1> viewports{Rendering::Viewport{outputArea}};
			const Array<Math::Rectangleui, 1> scissors{renderArea};

			const Array<Rendering::ColorTargetInfo, 6> colorBlendAttachments{
				Rendering::ColorTargetInfo{},
				Rendering::ColorTargetInfo{},
				Rendering::ColorTargetInfo{},
				Rendering::ColorTargetInfo{},
				Rendering::ColorTargetInfo{},
				Rendering::ColorTargetInfo{},
			};

			const Rendering::FragmentStageInfo fragmentStage{Rendering::ShaderStageInfo{filterFragmentShaderAssetGuid}, colorBlendAttachments};

			Threading::JobBatch pipelineCreationJobBatch = filterGraphicsPipeline.CreateAsync(
				*renderHelper.m_pLogicalDevice,
				renderHelper.m_pLogicalDevice->GetShaderCache(),
				filterGraphicsPipeline.operator ngine::Rendering::PipelineLayoutView(),
				renderPass,
				vertexStage,
				primitiveInfo,
				viewports,
				scissors,
				subpassIndex,
				fragmentStage,
				Optional<const Rendering::MultisamplingInfo*>{},
				Optional<const Rendering::DepthStencilInfo*>{},
				Optional<const Rendering::GeometryStageInfo*>{},
				Rendering::DynamicStateFlags{}
			);
			Threading::Atomic<bool> finishedCreation{false};
			pipelineCreationJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[&finishedCreation](Threading::JobRunnerThread&)
				{
					finishedCreation = true;
				},
				Threading::JobPriority::LoadGraphicsPipeline
			));
			Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
			thread.Queue(pipelineCreationJobBatch);
			while (!finishedCreation)
			{
				thread.DoRunNextJob();
			}

			if (!filterGraphicsPipeline.IsValid())
			{
				return false;
			}
		}

		{
			{
				const Array<Rendering::DescriptorSetView::ImageInfo, 2> descriptorImageInfo{
					Rendering::DescriptorSetView::ImageInfo{{}, inputCubeMapCompleteView, Rendering::ImageLayout::ShaderReadOnlyOptimal},
					Rendering::DescriptorSetView::ImageInfo{cubeMipMapSampler, {}, Rendering::ImageLayout::ShaderReadOnlyOptimal}
				};

				const Array<Rendering::DescriptorSet::UpdateInfo, 2> descriptorUpdates{
					Rendering::DescriptorSet::UpdateInfo{
						filterDescriptorSet,
						0,
						0,
						Rendering::DescriptorType::SampledImage,
						descriptorImageInfo.GetSubView(0, 1)
					},
					Rendering::DescriptorSet::UpdateInfo{
						filterDescriptorSet,
						1,
						0,
						Rendering::DescriptorType::Sampler,
						descriptorImageInfo.GetSubView(1, 1)
					}
				};

				filterDescriptorSet.Update(*renderHelper.m_pLogicalDevice, descriptorUpdates);
			}
		}

		const Array<Rendering::ClearValue, 6> clearValues(Memory::InitializeAll, Rendering::ClearValue{Math::Color{0.0f, 0.0f, 1.0f, 1.0f}});

#if RENDERER_HAS_COMMAND_POOL
		Rendering::CommandBuffer commandBuffer;
		renderHelper.m_commandPool.AllocateCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
		Rendering::CommandBuffer commandBuffer(
			*renderHelper.m_pLogicalDevice,
			renderHelper.m_commandPool,
			renderHelper.m_pLogicalDevice->GetCommandQueue(Rendering::QueueFamily::Graphics)
		);
#endif

		if (!commandBuffer.IsValid())
		{
			return false;
		}

		Rendering::CommandEncoder commandEncoder =
			commandBuffer.BeginEncoding(*renderHelper.m_pLogicalDevice, Rendering::CommandBuffer::Flags::OneTimeSubmit);
		if (!commandEncoder.IsValid())
		{
			return false;
		}

		// Filter

		FixedCapacityVector<Rendering::Framebuffer> frameBuffers(Memory::Reserve, _mipmapCount);

		// Filter every mip level: from inputCubeMap->currentMipLevel
		// The mip levels are filtered from the smallest mipmap to the largest mipmap,
		// i.e. the last mipmap is filtered last.
		// This has the desirable side effect that the framebuffer size of the last filter pass
		// matches with the LUT size, allowing the LUT to only be written in the last pass
		// without worrying to preserve the LUT's image contents between the previous render passes.
		for (int32 currentMipLevel = _mipmapCount - 1; currentMipLevel != -1; currentMipLevel--)
		{
			uint32 currentFramebufferSideWidth = cubemapSize.x >> currentMipLevel;
			uint32 currentFramebufferSideHeight = cubemapSize.y >> currentMipLevel;
			const ArrayView<const Rendering::ImageMappingView> renderTargetViews(outputCubeMapViews.GetSubView(currentMipLevel * 6, 6));

			Rendering::Framebuffer& filterOutputFramebuffer = frameBuffers.EmplaceBack(
				*renderHelper.m_pLogicalDevice,
				renderPass,
				renderTargetViews,
				Math::Vector2ui{currentFramebufferSideWidth, currentFramebufferSideHeight},
				1u
			);
			if (!filterOutputFramebuffer.IsValid())
			{
				return false;
			}

			{
				Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

				barrierCommandEncoder.TransitionImageLayout(
					Rendering::PipelineStageFlags::ColorAttachmentOutput,
					Rendering::AccessFlags::ColorAttachmentWrite,
					Rendering::ImageLayout::ColorAttachmentOptimal,
					outputCubeMap,
					Rendering::ImageSubresourceRange{
						Rendering::ImageAspectFlags::Color,
						Rendering::MipRange{(uint32)currentMipLevel, 1},
						Rendering::ArrayRange{0, 6u}
					}
				);
			}

			Rendering::RenderCommandEncoder renderCommandEncoder = commandEncoder.BeginRenderPass(
				*renderHelper.m_pLogicalDevice,
				renderPass,
				filterOutputFramebuffer,
				Math::Rectangleui{Math::Zero, Math::Vector2ui{currentFramebufferSideWidth, currentFramebufferSideHeight}},
				clearValues,
				1
			);

			renderCommandEncoder.BindPipeline(filterGraphicsPipeline);

			PushConstant values{};
			values.roughness = static_cast<float>(currentMipLevel) / static_cast<float>(_mipmapCount - 1);
			values.sampleCount = _sampleCount;
			values.mipLevel = currentMipLevel;
			values.width = cubemapSize.x;
			values.height = cubemapSize.y;
			values.lodBias = _lodBias;
			values.distribution = _distribution;

			filterGraphicsPipeline
				.PushConstants(*renderHelper.m_pLogicalDevice, renderCommandEncoder, pushConstantRanges.GetDynamicView(), values);

			renderCommandEncoder.BindDescriptorSets(
				filterGraphicsPipeline.operator ngine::Rendering::PipelineLayoutView(),
				ArrayView<const Rendering::DescriptorSetView>(filterDescriptorSet),
				filterGraphicsPipeline.GetFirstDescriptorSetIndex()
			);

			renderCommandEncoder.Draw(3, 1u);
		}

		////////////////////////////////////////////////////////////////////////////////////////
		// Output

		Rendering::Format targetFormat = _targetFormat;
		Rendering::RenderTexture convertedCubeMap;

		if (targetFormat != cubeMapFormat)
		{
			convertedCubeMap = BlitImage(
				renderHelper,
				commandEncoder,
				outputCubeMap,
				{cubemapSize.x, cubemapSize.y},
				mipMask,
				6u,
				Rendering::Image::Flags::Cubemap,
				targetFormat
			);
			if (!convertedCubeMap.IsValid())
			{
				return false;
			}
		}
		else
		{
			convertedCubeMap = Move(outputCubeMap);
		}

		Rendering::EncodedCommandBuffer encodedCommandBuffer = commandEncoder.StopEncoding();
		if (!encodedCommandBuffer.IsValid())
		{
			return false;
		}

		if (!renderHelper.executeCommandBuffers(ArrayView<const Rendering::EncodedCommandBufferView>{encodedCommandBuffer}))
		{
			return false;
		}

		renderPass.Destroy(*renderHelper.m_pLogicalDevice);

		for (Rendering::Framebuffer& filterOutputFramebuffer : frameBuffers)
		{
			filterOutputFramebuffer.Destroy(*renderHelper.m_pLogicalDevice);
		}

#if RENDERER_HAS_COMMAND_POOL
		renderHelper.m_commandPool.FreeCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
		commandBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_commandPool);
#endif

		if (!DownloadCubemap(
					renderHelper,
					convertedCubeMap,
					targetFormat,
					{cubemapSize.x, cubemapSize.y},
					_mipmapCount,
					Rendering::GetFormatInfo(targetFormat).m_blockDataSize,
					cubemapOut
				))
		{
			return false;
		}

		filterSetLayout.Destroy(*renderHelper.m_pLogicalDevice);
		filterGraphicsPipeline.Destroy(*renderHelper.m_pLogicalDevice);
		convertedCubeMap.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
		filterDescriptorSet.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_descriptorPool);
		cubeMipMapSampler.Destroy(*renderHelper.m_pLogicalDevice);
		inputCubeMapCompleteView.Destroy(*renderHelper.m_pLogicalDevice);
		outputCubeMapCompleteView.Destroy(*renderHelper.m_pLogicalDevice);
		for (Rendering::ImageMapping& mapping : outputCubeMapViews)
		{
			mapping.Destroy(*renderHelper.m_pLogicalDevice);
		}
		return true;
	}

	Rendering::RenderTexture SampleCubemap(
		RenderHelper& renderHelper,
		const Rendering::ImageView sourceImage,
		const Math::Vector2ui cubemapSize,
		const Rendering::MipMask mipMask,
		Rendering::Format _targetFormat
	)
	{
		Assert(sourceImage.IsValid());
		const Rendering::Format cubeMapFormat = Rendering::Format::R32G32B32A32_SFLOAT;

		using namespace ngine;

		constexpr Asset::Guid vertexShaderAssetGuid = "C8B836B2-50C9-49ED-8F4E-08C672B7D510"_asset;

		Rendering::RenderTexture outputCubemap(
			*renderHelper.m_pLogicalDevice,
			*renderHelper.m_pPhysicalDevice,
			cubeMapFormat,
			Rendering::SampleCount::One,
			Rendering::Image::Flags::Cubemap,
			Math::Vector3ui{cubemapSize.x, cubemapSize.y, 1u},
			Rendering::UsageFlags::ColorAttachment | Rendering::UsageFlags::TransferSource | Rendering::UsageFlags::TransferDestination |
				Rendering::UsageFlags::Sampled,
			Rendering::ImageLayout::Undefined,
			mipMask,
			mipMask,
			6u
		);
		if (!outputCubemap.IsValid())
		{
			return {};
		}

#if RENDERER_HAS_COMMAND_POOL
		Rendering::CommandBuffer commandBuffer;
		renderHelper.m_commandPool.AllocateCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
		Rendering::CommandBuffer commandBuffer(
			*renderHelper.m_pLogicalDevice,
			renderHelper.m_commandPool,
			renderHelper.m_pLogicalDevice->GetCommandQueue(Rendering::QueueFamily::Graphics)
		);
#endif

		if (!commandBuffer.IsValid())
		{
			outputCubemap.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
			return {};
		}

		Rendering::CommandEncoder commandEncoder =
			commandBuffer.BeginEncoding(*renderHelper.m_pLogicalDevice, Rendering::CommandBuffer::Flags::OneTimeSubmit);
		if (!commandEncoder.IsValid())
		{
			outputCubemap.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
#if RENDERER_HAS_COMMAND_POOL
			renderHelper.m_commandPool.FreeCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
			commandBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_commandPool);
#endif
			return {};
		}

		// Panorama to cubemap
		constexpr Asset::Guid panoramaToCubemapFragmentShaderAssetGuid = "46C88FE9-DB91-4D51-842F-7ABB3D66AD46"_asset;

		auto makeAttachmentDescription = [sourceFormat = cubeMapFormat]()
		{
			return Rendering::AttachmentDescription{
				sourceFormat,
				Rendering::SampleCount::One,
				Rendering::AttachmentLoadType::Undefined,
				Rendering::AttachmentStoreType::Store,
				Rendering::AttachmentLoadType::Undefined,
				Rendering::AttachmentStoreType::Undefined,
				Rendering::ImageLayout::Undefined,
				Rendering::ImageLayout::ColorAttachmentOptimal
			};
		};

		Array<Rendering::AttachmentDescription, 6> attachments = {Memory::InitializeAll, makeAttachmentDescription()};
		Array<Rendering::AttachmentReference, 6> attachmentReferences{
			Rendering::AttachmentReference{0, Rendering::ImageLayout::ColorAttachmentOptimal},
			Rendering::AttachmentReference{1, Rendering::ImageLayout::ColorAttachmentOptimal},
			Rendering::AttachmentReference{2, Rendering::ImageLayout::ColorAttachmentOptimal},
			Rendering::AttachmentReference{3, Rendering::ImageLayout::ColorAttachmentOptimal},
			Rendering::AttachmentReference{4, Rendering::ImageLayout::ColorAttachmentOptimal},
			Rendering::AttachmentReference{5, Rendering::ImageLayout::ColorAttachmentOptimal},
		};
		Rendering::RenderPass renderPass(*renderHelper.m_pLogicalDevice, attachments, attachmentReferences, {}, {}, {});
		if (!renderPass.IsValid())
		{
			return {};
		}

		Rendering::Sampler panoramaSampler =
			Rendering::Sampler(*renderHelper.m_pLogicalDevice, Rendering::AddressMode::RepeatMirrored, Rendering::FilterMode::Linear);
		if (!panoramaSampler.IsValid())
		{
			return {};
		}

		Rendering::ImageMapping panoramaImageView(
			*renderHelper.m_pLogicalDevice,
			sourceImage,
			Rendering::ImageMappingType::TwoDimensional,
			cubeMapFormat,
			Rendering::ImageAspectFlags::Color
		);
		if (!panoramaImageView.IsValid())
		{
			return {};
		}

		Rendering::DescriptorSetLayout panoramaSetLayout(
			*renderHelper.m_pLogicalDevice,
			Array{
				Rendering::DescriptorSetLayout::Binding::MakeSampledImage(
					0u,
					Rendering::ShaderStage::Fragment,
					Rendering::SampledImageType::Float,
					Rendering::ImageMappingType::TwoDimensional
				),
				Rendering::DescriptorSetLayout::Binding::MakeSampler(1u, Rendering::ShaderStage::Fragment, Rendering::SamplerBindingType::Filtering)
			}
		);
		if (!panoramaSetLayout.IsValid())
		{
			return {};
		}

		Rendering::DescriptorSet panoramaSet;
		if (!renderHelper.m_descriptorPool.AllocateDescriptorSets(
					*renderHelper.m_pLogicalDevice,
					ArrayView<const Rendering::DescriptorSetLayoutView>{panoramaSetLayout},
					ArrayView<Rendering::DescriptorSet>{panoramaSet}
				))
		{
			panoramaSetLayout.Destroy(*renderHelper.m_pLogicalDevice);
			return {};
		}

		{
			Array<Rendering::DescriptorSetView::ImageInfo, 2> descriptorImageInfo{
				Rendering::DescriptorSetView::ImageInfo{{}, panoramaImageView, Rendering::ImageLayout::ShaderReadOnlyOptimal},
				Rendering::DescriptorSetView::ImageInfo{panoramaSampler, {}, Rendering::ImageLayout::ShaderReadOnlyOptimal}
			};

			Array<Rendering::DescriptorSet::UpdateInfo, 2> descriptorUpdates{
				Rendering::DescriptorSet::UpdateInfo{
					panoramaSet,
					0,
					0,
					Rendering::DescriptorType::SampledImage,
					descriptorImageInfo.GetSubView(0, 1)
				},
				Rendering::DescriptorSet::UpdateInfo{panoramaSet, 1, 0, Rendering::DescriptorType::Sampler, descriptorImageInfo.GetSubView(1, 1)}
			};

			panoramaSet.Update(*renderHelper.m_pLogicalDevice, descriptorUpdates);
		}

		Rendering::GraphicsPipeline panoramaToCubePipeline;
		panoramaToCubePipeline
			.CreateBase(*renderHelper.m_pLogicalDevice, ArrayView<const Rendering::DescriptorSetLayoutView>{panoramaSetLayout});
		{
			const Rendering::VertexStageInfo vertexStage{Rendering::ShaderStageInfo{vertexShaderAssetGuid}};

			const uint8 subpassIndex = 0;

			const Math::Rectangleui renderArea{Math::Zero, cubemapSize};
			const Math::Rectangleui outputArea = renderArea;

			const Rendering::PrimitiveInfo primitiveInfo{
				Rendering::PrimitiveTopology::TriangleList,
				Rendering::PolygonMode::Fill,
				Rendering::WindingOrder::CounterClockwise,
				Rendering::CullMode::None
			};

			const Array<Rendering::Viewport, 1> viewports{Rendering::Viewport{outputArea}};
			const Array<Math::Rectangleui, 1> scissors{renderArea};

			const Array<Rendering::ColorTargetInfo, 6> colorBlendAttachments{
				Rendering::ColorTargetInfo{},
				Rendering::ColorTargetInfo{},
				Rendering::ColorTargetInfo{},
				Rendering::ColorTargetInfo{},
				Rendering::ColorTargetInfo{},
				Rendering::ColorTargetInfo{},
			};

			const Rendering::FragmentStageInfo fragmentStage{
				Rendering::ShaderStageInfo{panoramaToCubemapFragmentShaderAssetGuid},
				colorBlendAttachments
			};

			Threading::JobBatch pipelineCreationJobBatch = panoramaToCubePipeline.CreateAsync(
				*renderHelper.m_pLogicalDevice,
				renderHelper.m_pLogicalDevice->GetShaderCache(),
				panoramaToCubePipeline.operator ngine::Rendering::PipelineLayoutView(),
				renderPass,
				vertexStage,
				primitiveInfo,
				viewports,
				scissors,
				subpassIndex,
				fragmentStage,
				Optional<const Rendering::MultisamplingInfo*>{},
				Optional<const Rendering::DepthStencilInfo*>{},
				Optional<const Rendering::GeometryStageInfo*>{},
				Rendering::DynamicStateFlags{}
			);
			Threading::Atomic<bool> finishedCreation{false};
			pipelineCreationJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[&finishedCreation](Threading::JobRunnerThread&)
				{
					finishedCreation = true;
				},
				Threading::JobPriority::LoadGraphicsPipeline
			));
			Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
			thread.Queue(pipelineCreationJobBatch);
			while (!finishedCreation)
			{
				thread.DoRunNextJob();
			}

			if (!panoramaToCubePipeline.IsValid())
			{
				return {};
			}
		}

		/// Render Pass
		Array<Rendering::ImageMapping, 6> outputCubeMapViews;

		for (uint8 i = 0; i < outputCubeMapViews.GetSize(); i++)
		{
			outputCubeMapViews[i] = Rendering::ImageMapping(
				*renderHelper.m_pLogicalDevice,
				outputCubemap,
				Rendering::ImageMappingType::TwoDimensional,
				cubeMapFormat,
				Rendering::ImageAspectFlags::Color,
				Rendering::MipRange{0, 1},
				Rendering::ArrayRange{i, 1}
			);
			if (!outputCubeMapViews[i].IsValid())
			{
				return {};
			}
		}

		Rendering::Framebuffer cubeMapInputFramebuffer(
			*renderHelper.m_pLogicalDevice,
			renderPass,
			outputCubeMapViews.GetView().GetDynamicView(),
			Math::Vector2ui{cubemapSize.x, cubemapSize.y},
			1u
		);
		if (!cubeMapInputFramebuffer.IsValid())
		{
			return {};
		}

		{
			const Array<Rendering::ClearValue, 6> clearValues{Memory::InitializeAll, Math::Color{0.0f, 0.0f, 1.0f, 1.0f}};
			Rendering::RenderCommandEncoder renderCommandEncoder = commandEncoder.BeginRenderPass(
				*renderHelper.m_pLogicalDevice,
				renderPass,
				cubeMapInputFramebuffer,
				Math::Rectangleui{Math::Zero, cubemapSize},
				clearValues,
				0
			);
			renderCommandEncoder.BindPipeline(panoramaToCubePipeline);
			renderCommandEncoder.BindDescriptorSets(
				panoramaToCubePipeline.operator ngine::Rendering::PipelineLayoutView(),
				ArrayView<const Rendering::DescriptorSetView, uint8>{panoramaSet},
				panoramaToCubePipeline.GetFirstDescriptorSetIndex()
			);

			renderCommandEncoder.Draw(3, 1u);
		}

		Rendering::Format targetFormat = _targetFormat;
		Rendering::RenderTexture convertedCubeMap;

		const Rendering::MipMask::StoredType _mipmapCount = mipMask.GetSize();
		if (_mipmapCount > 1)
		{
			GenerateMipmapLevels(commandEncoder, outputCubemap, _mipmapCount, cubemapSize.x, cubemapSize.y);
		}
		else
		{
			Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

			barrierCommandEncoder.TransitionImageLayout(
				Rendering::PipelineStageFlags::FragmentShader,
				Rendering::AccessFlags::ShaderRead,
				Rendering::ImageLayout::ShaderReadOnlyOptimal,
				outputCubemap,
				Rendering::ImageSubresourceRange{Rendering::ImageAspectFlags::Color, Rendering::MipRange{0, 1}, Rendering::ArrayRange{0, 6u}}
			);
		}

		if (targetFormat != cubeMapFormat)
		{
			convertedCubeMap = BlitImage(
				renderHelper,
				commandEncoder,
				outputCubemap,
				{cubemapSize.x, cubemapSize.y},
				mipMask,
				6u,
				Rendering::Image::Flags::Cubemap,
				targetFormat
			);
			outputCubemap.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
			if (!convertedCubeMap.IsValid())
			{
#if RENDERER_HAS_COMMAND_POOL
				renderHelper.m_commandPool.FreeCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
				commandBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_commandPool);
#endif
				return {};
			}
		}
		else
		{
			convertedCubeMap = Move(outputCubemap);
		}

		Rendering::EncodedCommandBuffer encodedCommandBuffer = commandEncoder.StopEncoding();
		if (!encodedCommandBuffer.IsValid())
		{
			convertedCubeMap.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
#if RENDERER_HAS_COMMAND_POOL
			renderHelper.m_commandPool.FreeCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
			commandBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_commandPool);
#endif
			return {};
		}

		if (!renderHelper.executeCommandBuffers(ArrayView<const Rendering::EncodedCommandBufferView>{encodedCommandBuffer}))
		{
			convertedCubeMap.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());
#if RENDERER_HAS_COMMAND_POOL
			renderHelper.m_commandPool.FreeCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
			commandBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_commandPool);
#endif
			return {};
		}

		renderPass.Destroy(*renderHelper.m_pLogicalDevice);
		cubeMapInputFramebuffer.Destroy(*renderHelper.m_pLogicalDevice);

		for (Rendering::ImageMapping& imageView : outputCubeMapViews)
		{
			imageView.Destroy(*renderHelper.m_pLogicalDevice);
		}

		panoramaSetLayout.Destroy(*renderHelper.m_pLogicalDevice);
		panoramaToCubePipeline.Destroy(*renderHelper.m_pLogicalDevice);
		panoramaImageView.Destroy(*renderHelper.m_pLogicalDevice);
		panoramaSampler.Destroy(*renderHelper.m_pLogicalDevice);

#if RENDERER_HAS_COMMAND_POOL
		renderHelper.m_commandPool.FreeCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
		commandBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_commandPool);
#endif

		panoramaSet.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_descriptorPool);

		return convertedCubeMap;
	}

	bool SampleBRDF(RenderHelper& renderHelper, gli::texture2d& brdfOut, Distribution _distribution, uint32 _sampleCount)
	{
		const Rendering::Format LUTFormat = Rendering::Format::R8G8_UNORM;
		const Math::Vector2ui cubemapSize = {(uint32)brdfOut.extent().x, (uint32)brdfOut.extent().y};

		using namespace ngine;
		constexpr Asset::Guid vertexShaderAssetGuid = "C8B836B2-50C9-49ED-8F4E-08C672B7D510"_asset;
		constexpr Asset::Guid brdfFragmentShaderAssetGuid = "ED190A9C-85F4-4EA3-BEDF-2F685D4C4E38"_asset;

		Rendering::RenderTexture outputLUT(
			*renderHelper.m_pLogicalDevice,
			*renderHelper.m_pPhysicalDevice,
			LUTFormat,
			Rendering::SampleCount::One,
			Rendering::Image::Flags{},
			Math::Vector3ui{cubemapSize.x, cubemapSize.y, 1u},
			Rendering::UsageFlags::ColorAttachment | Rendering::UsageFlags::TransferSource,
			Rendering::ImageLayout::Undefined,
			Rendering::MipMask::FromRange(Rendering::MipRange{0, 1}),
			Rendering::MipMask::FromRange(Rendering::MipRange{0, 1}),
			1u
		);
		if (!outputLUT.IsValid())
		{
			return false;
		}

		Rendering::ImageMapping outputLUTView = Rendering::ImageMapping(
			*renderHelper.m_pLogicalDevice,
			outputLUT,
			Rendering::ImageMappingType::TwoDimensional,
			LUTFormat,
			Rendering::ImageAspectFlags::Color,
			Rendering::MipRange{0, 1u},
			Rendering::ArrayRange{0, 1u}
		);
		if (!outputLUTView.IsValid())
		{
			return false;
		}

		auto makeAttachmentDescription = [format = LUTFormat]()
		{
			return Rendering::AttachmentDescription{
				static_cast<Rendering::Format>(format),
				Rendering::SampleCount::One,
				Rendering::AttachmentLoadType::Undefined,
				Rendering::AttachmentStoreType::Store,
				Rendering::AttachmentLoadType::Undefined,
				Rendering::AttachmentStoreType::Undefined,
				Rendering::ImageLayout::Undefined,
				Rendering::ImageLayout::ColorAttachmentOptimal
			};
		};

		Array<Rendering::AttachmentDescription, 1> attachments = {Memory::InitializeAll, makeAttachmentDescription()};
		Array<Rendering::AttachmentReference, 1> attachmentReferences{
			Rendering::AttachmentReference{0, Rendering::ImageLayout::ColorAttachmentOptimal}
		};
		Rendering::RenderPass renderPass(*renderHelper.m_pLogicalDevice, attachments, attachmentReferences, {}, {}, {});
		if (!renderPass.IsValid())
		{
			return false;
		}

		struct PushConstant
		{
			uint32 sampleCount = 1u;
			Distribution distribution = Lambertian;
		};

		const Array<Rendering::PushConstantRange, 1> pushConstantRanges{
			Rendering::PushConstantRange{Rendering::ShaderStage::Fragment, 0u, sizeof(PushConstant)}
		};

		////////////////////////////////////////////////////////////////////////////////////////
		// Filter CubeMap Pipeline
		Rendering::GraphicsPipeline filterGraphicsPipeline;
		filterGraphicsPipeline.CreateBase(
			*renderHelper.m_pLogicalDevice,
			ArrayView<const Rendering::DescriptorSetLayoutView>{},
			pushConstantRanges.GetDynamicView()
		);
		{
			const Rendering::VertexStageInfo vertexStage{Rendering::ShaderStageInfo{vertexShaderAssetGuid}};

			const uint8 subpassIndex = 0;

			const Math::Rectangleui renderArea{Math::Zero, cubemapSize};
			const Math::Rectangleui outputArea = renderArea;

			const Rendering::PrimitiveInfo primitiveInfo{
				Rendering::PrimitiveTopology::TriangleList,
				Rendering::PolygonMode::Fill,
				Rendering::WindingOrder::CounterClockwise,
				Rendering::CullMode::None
			};

			const Array<Rendering::Viewport, 1> viewports{Rendering::Viewport{outputArea}};
			const Array<Math::Rectangleui, 1> scissors{renderArea};

			const Array<Rendering::ColorTargetInfo, 1> colorBlendAttachments{
				Rendering::ColorTargetInfo{},
			};

			const Rendering::FragmentStageInfo fragmentStage{Rendering::ShaderStageInfo{brdfFragmentShaderAssetGuid}, colorBlendAttachments};

			Threading::JobBatch pipelineCreationJobBatch = filterGraphicsPipeline.CreateAsync(
				*renderHelper.m_pLogicalDevice,
				renderHelper.m_pLogicalDevice->GetShaderCache(),
				filterGraphicsPipeline.operator ngine::Rendering::PipelineLayoutView(),
				renderPass,
				vertexStage,
				primitiveInfo,
				viewports,
				scissors,
				subpassIndex,
				fragmentStage,
				Optional<const Rendering::MultisamplingInfo*>{},
				Optional<const Rendering::DepthStencilInfo*>{},
				Optional<const Rendering::GeometryStageInfo*>{},
				Rendering::DynamicStateFlags{}
			);
			Threading::Atomic<bool> finishedCreation{false};
			pipelineCreationJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[&finishedCreation](Threading::JobRunnerThread&)
				{
					finishedCreation = true;
				},
				Threading::JobPriority::LoadGraphicsPipeline
			));
			Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
			thread.Queue(pipelineCreationJobBatch);
			while (!finishedCreation)
			{
				thread.DoRunNextJob();
			}

			if (!filterGraphicsPipeline.IsValid())
			{
				return false;
			}
		}

		const Array<Rendering::ClearValue, 1> clearValues{Math::Color{0.0f, 0.0f, 1.0f, 1.0f}};

#if RENDERER_HAS_COMMAND_POOL
		Rendering::CommandBuffer commandBuffer;
		renderHelper.m_commandPool.AllocateCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
		Rendering::CommandBuffer commandBuffer(
			*renderHelper.m_pLogicalDevice,
			renderHelper.m_commandPool,
			renderHelper.m_pLogicalDevice->GetCommandQueue(Rendering::QueueFamily::Graphics)
		);
#endif

		if (!commandBuffer.IsValid())
		{
			return false;
		}

		Rendering::CommandEncoder commandEncoder =
			commandBuffer.BeginEncoding(*renderHelper.m_pLogicalDevice, Rendering::CommandBuffer::Flags::OneTimeSubmit);
		if (!commandEncoder.IsValid())
		{
			return false;
		}

		const Array<const Rendering::ImageMappingView, 1> renderTargetViews{outputLUTView};
		Rendering::Framebuffer filterOutputFramebuffer(
			*renderHelper.m_pLogicalDevice,
			renderPass,
			renderTargetViews,
			Math::Vector2ui{cubemapSize.x, cubemapSize.y},
			1u
		);
		if (!filterOutputFramebuffer.IsValid())
		{
			return false;
		}

		PushConstant values{};
		values.sampleCount = _sampleCount;
		values.distribution = _distribution;

		{
			Rendering::RenderCommandEncoder renderCommandEncoder = commandEncoder.BeginRenderPass(
				*renderHelper.m_pLogicalDevice,
				renderPass,
				filterOutputFramebuffer,
				Math::Rectangleui{Math::Zero, Math::Vector2ui{cubemapSize.x, cubemapSize.y}},
				clearValues,
				1
			);

			renderCommandEncoder.BindPipeline(filterGraphicsPipeline);
			filterGraphicsPipeline
				.PushConstants(*renderHelper.m_pLogicalDevice, renderCommandEncoder, pushConstantRanges.GetDynamicView(), values);
			renderCommandEncoder.Draw(3, 1u);
		}

		////////////////////////////////////////////////////////////////////////////////////////
		// Output

		Rendering::EncodedCommandBuffer encodedCommandBuffer = commandEncoder.StopEncoding();
		if (!encodedCommandBuffer.IsValid())
		{
			return false;
		}

		if (!renderHelper.executeCommandBuffers(ArrayView<const Rendering::EncodedCommandBufferView>{encodedCommandBuffer}))
		{
			return false;
		}

		renderPass.Destroy(*renderHelper.m_pLogicalDevice);

#if RENDERER_HAS_COMMAND_POOL
		renderHelper.m_commandPool.FreeCommandBuffers(*renderHelper.m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
		commandBuffer.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_commandPool);
#endif

		if (!Download2DImage(
					renderHelper,
					outputLUT,
					LUTFormat,
					Math::Vector2ui{cubemapSize.x, cubemapSize.y},
					Rendering::GetFormatInfo(LUTFormat).m_blockDataSize,
					brdfOut
				))
		{
			return false;
		}

		filterGraphicsPipeline.Destroy(*renderHelper.m_pLogicalDevice);
		outputLUT.Destroy(*renderHelper.m_pLogicalDevice, renderHelper.m_pLogicalDevice->GetDeviceMemoryPool());

		filterOutputFramebuffer.Destroy(*renderHelper.m_pLogicalDevice);
		outputLUTView.Destroy(*renderHelper.m_pLogicalDevice);

		return true;
	}
}
