#include "Assets/Texture/RenderTexture.h"

#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/FormatInfo.h>

#include <Common/Math/Color.h>
#include <Common/Math/Vector2.h>
#include <Common/Memory/Align.h>

namespace ngine::Rendering
{
	[[nodiscard]] PURE_STATICS EnumFlags<ImageAspectFlags> GetFormatAspectFlags(const EnumFlags<FormatFlags> formatFlags)
	{
		EnumFlags<ImageAspectFlags> aspectFlags;
		aspectFlags |= ImageAspectFlags::Depth * formatFlags.IsSet(FormatFlags::Depth);
		aspectFlags |= ImageAspectFlags::Stencil * formatFlags.IsSet(FormatFlags::Stencil);
		aspectFlags |= ImageAspectFlags::Color * aspectFlags.AreNoneSet();
		return aspectFlags;
	}
	[[nodiscard]] PURE_STATICS EnumFlags<ImageAspectFlags> GetFormatAspectFlags(const Format format)
	{
		return GetFormatAspectFlags(GetFormatInfo(format).m_flags);
	}

	[[nodiscard]] PURE_STATICS ImageSubresourceRange
	CalculateTotalSubresourceRange(const Format format, const MipMask totalMipMask, const ArrayRange::UnitType numArrayLayers)
	{
		const MipMask::StoredType totalMipCount = totalMipMask.GetSize();
		const MipRange mipRange = totalMipMask.GetRange(totalMipCount);
		return ImageSubresourceRange{GetFormatAspectFlags(format), MipRange{mipRange.GetIndex(), totalMipCount}, ArrayRange{0, numArrayLayers}};
	}

	RenderTexture::RenderTexture(
		const RenderTargetType,
		LogicalDevice& logicalDevice,
		const Math::Vector2ui resolution,
		const Format format,
		const SampleCount sampleCount,
		const EnumFlags<Flags> flags,
		const EnumFlags<UsageFlags> usageFlags,
		const MipMask totalMipMask,
		const ArrayRange::UnitType numArrayLayers
	)
		: Image(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				format,
				sampleCount,
				flags,
				{resolution.x, resolution.y, 1},
				usageFlags,
				ImageLayout::Undefined,
				totalMipMask.GetSize(),
				numArrayLayers
			)
		, m_format(format)
		, m_usageFlags(usageFlags)
		, m_totalMipsMask(totalMipMask.GetValue())
		, m_loadedMipsMask(totalMipMask.GetValue())
		, m_totalArrayCount(numArrayLayers)
	{
		const uint8 numSubresourceBuckets = 1;

		const ImageSubresourceRange subresourceRange = CalculateTotalSubresourceRange(format, totalMipMask, numArrayLayers);
		m_subresourceStates.Initialize(subresourceRange, numSubresourceBuckets);

		Assert(totalMipMask.GetSize() <= MipMask::FromSizeAllToLargest(resolution).GetSize());
	}

	RenderTexture::RenderTexture(
		const DummyType,
		LogicalDevice& logicalDevice,
		CommandEncoderView commandEncoder,
		StagingBuffer& stagingBufferOut,
		const EnumFlags<Flags> flags,
		const EnumFlags<UsageFlags> usageFlags,
		const ArrayRange::UnitType arraySize,
		const Math::Color color
	)
		: RenderTexture(
				Dummy,
				logicalDevice,
				{1, 1},
				Format::R8G8B8A8_UNORM_PACK8,
				commandEncoder,
				stagingBufferOut,
				flags,
				usageFlags,
				arraySize,
				ConstByteView::Make(color)
			)
	{
	}

	RenderTexture::RenderTexture(
		const DummyType,
		LogicalDevice& logicalDevice,
		const Math::Vector2ui resolution,
		const Format format,
		CommandEncoderView commandEncoder,
		StagingBuffer& stagingBufferOut,
		const EnumFlags<Flags> flags,
		const EnumFlags<UsageFlags> usageFlags,
		const ArrayRange::UnitType arraySize,
		const ConstByteView colorData
	)
		: Image(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				format,
				SampleCount::One,
				flags,
				{resolution.x, resolution.y, 1},
				usageFlags,
				ImageLayout::Undefined,
				1u,
				arraySize
			)
		, m_format(format)
		, m_usageFlags(usageFlags)
		, m_totalMipsMask(MipMask::FromSizeToLargest({1, 1}).GetValue())
		, m_loadedMipsMask(m_totalMipsMask)
		, m_totalArrayCount(arraySize)
	{
		const uint8 numSubresourceBuckets = 1;
		const ImageSubresourceRange subresourceRange{ImageAspectFlags::Color, MipRange{0, 1}, ArrayRange{0, arraySize}};
		m_subresourceStates.Initialize(subresourceRange, numSubresourceBuckets);

		const Rendering::FormatInfo formatInfo = Rendering::GetFormatInfo(m_format);
		uint32 bytesPerLayer = formatInfo.GetBytesPerLayer(resolution);
		Math::Vector2ui bytesPerDimension = formatInfo.GetBytesPerDimension(resolution);
		if constexpr (RENDERER_WEBGPU)
		{
			bytesPerLayer = Memory::Align(bytesPerLayer, 256);
			bytesPerDimension.x = Memory::Align(bytesPerDimension.x, 256);
		}

		const Math::Vector2ui blockCount = formatInfo.GetBlockCount(resolution);

		stagingBufferOut = StagingBuffer(
			logicalDevice,
			logicalDevice.GetPhysicalDevice(),
			logicalDevice.GetDeviceMemoryPool(),
			bytesPerLayer * arraySize,
			StagingBuffer::Flags::TransferSource
		);

		const size bufferAlignment = RENDERER_WEBGPU ? 256 : colorData.GetAlignment();
		InlineVector<ByteType, 256> colorBytes(Memory::ConstructWithSize, Memory::Uninitialized, (uint32)stagingBufferOut.GetSize());

		ByteView colorBytesView{colorBytes.GetView()};
		for (ArrayRange::UnitType arrayIndex = 0; arrayIndex < arraySize; ++arrayIndex)
		{
			colorBytesView.GetSubView(size(0), sizeof(Math::Color)).CopyFrom(colorData);
			colorBytesView += Memory::Align(sizeof(Math::Color), bufferAlignment);
		}

		stagingBufferOut.MapToHostMemory(
			logicalDevice,
			Math::Range<size>::Make(0, stagingBufferOut.GetSize()),
			Buffer::MapMemoryFlags::Write,
			[colors = ConstByteView(colorBytes.GetSubView(0, (uint32)stagingBufferOut.GetSize())
		   )]([[maybe_unused]] const Buffer::MapMemoryStatus status, const ByteView data, [[maybe_unused]] const bool executedAsynchronously)
			{
				Assert(status == Buffer::MapMemoryStatus::Success);
				Assert(!executedAsynchronously);
				if (LIKELY(status == Buffer::MapMemoryStatus::Success))
				{
					data.CopyFrom(colors);
				}
			}
		);

		{
			BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();
			barrierCommandEncoder.TransitionImageLayout(
				PipelineStageFlags::Transfer,
				AccessFlags::TransferWrite,
				ImageLayout::TransferDestinationOptimal,
				*this,
				subresourceRange
			);
			m_subresourceStates.SetSubresourceState(
				subresourceRange,
				SubresourceState{
					ImageLayout::TransferDestinationOptimal,
					PassAttachmentReference{},
					PipelineStageFlags::Transfer,
					AccessFlags::TransferWrite,
					logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Transfer)
				},
				subresourceRange,
				0
			);
		}

		const Array<BufferImageCopy, 1> imageCopies = {
			BufferImageCopy{
				0,
				bytesPerDimension,
				blockCount,
				formatInfo.m_blockExtent,
				SubresourceLayers{
					ImageAspectFlags::Color,
					0,
					ArrayRange{0, arraySize},
				},
				Math::Vector3i{0, 0, 0},
				Math::Vector3ui{resolution.x, resolution.y, 1},
			},
		};

		{
			BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
			blitCommandEncoder.RecordCopyBufferToImage(stagingBufferOut, *this, ImageLayout::TransferDestinationOptimal, imageCopies.GetView());
		}

		{
			BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();
			barrierCommandEncoder.TransitionImageLayout(
				PipelineStageFlags::FragmentShader,
				AccessFlags::ShaderRead,
				ImageLayout::ShaderReadOnlyOptimal,
				*this,
				subresourceRange
			);
			m_subresourceStates.SetSubresourceState(
				subresourceRange,
				SubresourceState{
					ImageLayout::ShaderReadOnlyOptimal,
					PassAttachmentReference{},
					PipelineStageFlags::FragmentShader,
					AccessFlags::ShaderRead,
					logicalDevice.GetPhysicalDevice().GetQueueFamily(QueueFamily::Graphics)
				},
				subresourceRange,
				0
			);
		}
	}

	RenderTexture::RenderTexture(
		LogicalDevice& logicalDevice,
		const PhysicalDevice& physicalDevice,
		const Format format,
		const SampleCount sampleCount,
		const EnumFlags<Flags> flags,
		const Math::Vector3ui resolution,
		const EnumFlags<UsageFlags> usageFlags,
		const ImageLayout initialLayout,
		const MipMask totalMipMask,
		const MipMask availableMipMask,
		const ArrayRange::UnitType numArrayLayers
	)
		: Image(
				logicalDevice,
				physicalDevice,
				logicalDevice.GetDeviceMemoryPool(),
				format,
				sampleCount,
				flags,
				resolution,
				usageFlags,
				initialLayout,
				(uint16)totalMipMask.GetRange(MipMask::FromSizeAllToLargest({resolution.x, resolution.y}).GetSize()).GetCount() + 1,
				numArrayLayers
			)
		, m_format(format)
		, m_usageFlags(usageFlags)
		, m_totalMipsMask(totalMipMask.GetValue())
		, m_loadedMipsMask(availableMipMask.GetValue())
		, m_totalArrayCount(numArrayLayers)
	{
		const uint8 numSubresourceBuckets = 1;
		const ImageSubresourceRange subresourceRange = CalculateTotalSubresourceRange(format, totalMipMask, numArrayLayers);
		m_subresourceStates.Initialize(subresourceRange, numSubresourceBuckets);

		if (initialLayout != ImageLayout::Undefined)
		{
			m_subresourceStates.SetSubresourceState(
				subresourceRange,
				SubresourceState{
					initialLayout,
					PassAttachmentReference{},
					GetSupportedPipelineStageFlags(initialLayout),
					GetSupportedAccessFlags(initialLayout),
					(QueueFamilyIndex)~0
				},
				subresourceRange,
				0
			);
		}

		Assert(
			(totalMipMask & MipMask::FromSizeToLargest({resolution.x, resolution.y})).AreAnySet() ||
				(totalMipMask & MipMask::FromSizeToSmallest({resolution.x, resolution.y})).AreAnySet(),
			"Top mip must always be present, otherwise change the resolution"
		);
		Assert(
			(totalMipMask.GetValue() & availableMipMask.GetValue()) == availableMipMask.GetValue(),
			"Available mips must all be part of the total mip mask"
		);

		Assert(totalMipMask.GetSize() <= MipMask::FromSizeAllToLargest({resolution.x, resolution.y}).GetSize());

		Assert(availableMipMask.GetSize() <= totalMipMask.GetSize());
		Assert(*availableMipMask.GetFirstIndex() >= *totalMipMask.GetFirstIndex());
		Assert(*availableMipMask.GetLastIndex() <= *totalMipMask.GetLastIndex());
		Assert((GetLoadedMipMask() & GetTotalMipMask()) == GetLoadedMipMask());
	}

	ImageSubresourceRange RenderTexture::GetTotalSubresourceRange() const
	{
		return CalculateTotalSubresourceRange(m_format, GetTotalMipMask(), m_totalArrayCount);
	}
}
