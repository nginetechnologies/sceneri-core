#include "RenderHelper.h"

#include <Renderer/Devices/LogicalDeviceView.h>
#include <Renderer/Threading/Fence.h>
#include <Renderer/Commands/CommandBufferView.h>
#include <Renderer/Commands/CommandEncoder.h>
#include <Renderer/Commands/EncodedCommandBuffer.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Commands/CommandBuffer.h>
#include <Renderer/Buffers/StagingBuffer.h>
#include <Renderer/Renderer.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <Renderer/FormatInfo.h>

#include <Common/IO/File.h>
#include <Common/IO/Path.h>
#include <Common/EnumFlags.h>
#include <Common/Memory/Containers/ArrayView.h>
#include <Common/Math/Vector3.h>
#include <Common/System/Query.h>
#include <Common/Version.h>
#include <Common/Threading/Mutexes/ConditionVariable.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>

#include <cstring>
#include <iostream>

namespace ngine::AssetCompiler
{
	using namespace ngine;
	RenderHelper::RenderHelper(const bool enableValidation)
		: m_instance(
				"Asset Compiler",
				Version(1, 0, 0),
				"Asset Compiler",
				Version(1, 0, 0),
				Rendering::Instance::CreationFlags::Validation * enableValidation
			)
		, m_pPhysicalDevices(m_instance.IsValid() ? UniquePtr<Rendering::PhysicalDevices>::Make(m_instance) : nullptr)
		, m_pPhysicalDevice(
				m_pPhysicalDevices != nullptr && m_pPhysicalDevices->GetView().HasElements() ? &m_pPhysicalDevices->GetView().GetLastElement()
																																										 : nullptr
			)
		, m_pLogicalDevice(
				m_pPhysicalDevice != nullptr ? UniquePtr<Rendering::LogicalDevice>::Make(
																				 m_instance,
																				 *m_pPhysicalDevice,
																				 Invalid,
																				 Rendering::LogicalDeviceIdentifier{},
																				 Rendering::LogicalDevice::CreateDevice(*m_pPhysicalDevice, Invalid, {})
																			 )
																		 : nullptr
			)
		, m_debugOutputEnabled(enableValidation)
	{
		Assert(m_pLogicalDevice.IsValid());
		if (UNLIKELY(m_pLogicalDevice.IsInvalid()))
		{
			return;
		}

		m_commandPool = Rendering::CommandPool(
			*m_pLogicalDevice,
			m_pPhysicalDevice->GetQueueFamily(Rendering::QueueFamily::Graphics),
			Rendering::CommandPool::Flags::SupportIndividualCommandBufferReset
		);
		if (!m_commandPool.IsValid())
		{
			return;
		}

		// Create descriptor pool
		constexpr uint32_t descCount = 8u;
		constexpr uint32_t setCount = 8u;

		// TODO: remove unneeded descriptor types
		const Array sizes = {
			Rendering::DescriptorPool::Size{Rendering::DescriptorType::Sampler, descCount},
			Rendering::DescriptorPool::Size{Rendering::DescriptorType::CombinedImageSampler, descCount},
			Rendering::DescriptorPool::Size{Rendering::DescriptorType::SampledImage, descCount},
			Rendering::DescriptorPool::Size{Rendering::DescriptorType::StorageImage, descCount},
			Rendering::DescriptorPool::Size{Rendering::DescriptorType::UniformTexelBuffer, descCount},
			Rendering::DescriptorPool::Size{Rendering::DescriptorType::UniformTexelBuffer, descCount},
			Rendering::DescriptorPool::Size{Rendering::DescriptorType::UniformBuffer, descCount},
			Rendering::DescriptorPool::Size{Rendering::DescriptorType::StorageBuffer, descCount},
			Rendering::DescriptorPool::Size{Rendering::DescriptorType::UniformBufferDynamic, descCount},
			Rendering::DescriptorPool::Size{Rendering::DescriptorType::StorageBufferDynamic, descCount}
		};

		m_descriptorPool =
			Rendering::DescriptorPool(*m_pLogicalDevice, setCount, sizes, Rendering::DescriptorPool::CreationFlags::SupportIndividualFree);
		if (!m_descriptorPool.IsValid())
		{
			return;
		}
	}

	RenderHelper::~RenderHelper()
	{
		if (m_pLogicalDevice.IsValid())
		{
			if (m_descriptorPool.IsValid())
			{
				m_descriptorPool.Destroy(*m_pLogicalDevice);
			}

			if (m_commandPool.IsValid())
			{
				m_commandPool.Destroy(*m_pLogicalDevice);
			}

			m_pLogicalDevice.DestroyElement();
		}
	}

	Rendering::Image RenderHelper::uploadImage(const ConstByteView inputData, const uint32_t width, const uint32_t height)
	{
#if RENDERER_HAS_COMMAND_POOL
		Rendering::CommandBuffer commandBuffer;
		m_commandPool.AllocateCommandBuffers(*m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
		Rendering::CommandBuffer
			commandBuffer(*m_pLogicalDevice, m_commandPool, m_pLogicalDevice->GetCommandQueue(Rendering::QueueFamily::Graphics));
#endif

		if (!commandBuffer.IsValid())
		{
			return {};
		}

		Rendering::StagingBuffer stagingBuffer(
			*m_pLogicalDevice,
			m_pLogicalDevice->GetPhysicalDevice(),
			m_pLogicalDevice->GetDeviceMemoryPool(),
			inputData.GetDataSize(),
			Rendering::StagingBuffer::Flags::TransferSource
		);
		if (!stagingBuffer.IsValid())
		{
			return {};
		}

		// transfer data to the host coherent staging buffer
		const Array<Rendering::DataToBuffer, 1> copies{Rendering::DataToBuffer{0, inputData}};
		stagingBuffer
			.MapAndCopyFrom(*m_pLogicalDevice, Rendering::QueueFamily::Graphics, copies, Math::Range<size>::Make(0, inputData.GetDataSize()));

		// create the destination image we want to sample in the shader
		Rendering::RenderTexture image(
			*m_pLogicalDevice,
			*m_pPhysicalDevice,
			Rendering::Format::R32G32B32A32_SFLOAT,
			Rendering::SampleCount::One,
			Rendering::Image::Flags{},
			Math::Vector3ui{width, height, 1u},
			Rendering::UsageFlags::TransferDestination | Rendering::UsageFlags::Sampled | Rendering::UsageFlags::TransferSource,
			Rendering::ImageLayout::Undefined,
			Rendering::MipMask::FromRange(Rendering::MipRange{0, 1}),
			Rendering::MipMask::FromRange(Rendering::MipRange{0, 1}),
			1
		);
		if (!image.IsValid())
		{
			stagingBuffer.Destroy(*m_pLogicalDevice, m_pLogicalDevice->GetDeviceMemoryPool());
			return {};
		}

		Rendering::CommandEncoder commandEncoder =
			commandBuffer.BeginEncoding(*m_pLogicalDevice, Rendering::CommandBuffer::Flags::OneTimeSubmit);
		if (!commandEncoder.IsValid())
		{
			stagingBuffer.Destroy(*m_pLogicalDevice, m_pLogicalDevice->GetDeviceMemoryPool());
			image.Destroy(*m_pLogicalDevice, m_pLogicalDevice->GetDeviceMemoryPool());
			return {};
		}

		{
			Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

			barrierCommandEncoder.TransitionImageLayout(
				Rendering::PipelineStageFlags::Transfer,
				Rendering::AccessFlags::TransferWrite,
				Rendering::ImageLayout::TransferDestinationOptimal,
				image,
				Rendering::ImageSubresourceRange{Rendering::ImageAspectFlags::Color}
			);
		}

		{
			const Rendering::FormatInfo& formatInfo = Rendering::GetFormatInfo(Rendering::Format::R32G32B32A32_SFLOAT);

			Rendering::BlitCommandEncoder blitCommandEncoder = commandEncoder.BeginBlit();
			blitCommandEncoder.RecordCopyBufferToImage(
				stagingBuffer,
				image,
				Rendering::ImageLayout::TransferDestinationOptimal,
				Array<Rendering::BufferImageCopy, 1>{
					Rendering::BufferImageCopy{
						0,
						formatInfo.GetBytesPerDimension(Math::Vector2ui{width, height}),
						formatInfo.GetBlockCount(Math::Vector2ui{Math::Vector2ui{width, height}}),
						formatInfo.m_blockExtent,
						Rendering::SubresourceLayers{Rendering::ImageAspectFlags::Color, 0u, Rendering::ArrayRange{0u, 1}},
						{0, 0, 0},
						{width, height, 1}
					}
				}.GetView()
			);
		}

		{
			Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

			barrierCommandEncoder.TransitionImageLayout(
				Rendering::PipelineStageFlags::FragmentShader,
				Rendering::AccessFlags::ShaderRead,
				Rendering::ImageLayout::ShaderReadOnlyOptimal,
				image,
				Rendering::ImageSubresourceRange{Rendering::ImageAspectFlags::Color}
			);
		}

		const Rendering::EncodedCommandBuffer encodedCommandBuffer = commandEncoder.StopEncoding();
		if (!encodedCommandBuffer.IsValid())
		{
			stagingBuffer.Destroy(*m_pLogicalDevice, m_pLogicalDevice->GetDeviceMemoryPool());
			image.Destroy(*m_pLogicalDevice, m_pLogicalDevice->GetDeviceMemoryPool());
			return {};
		}

		if (!executeCommandBuffers(ArrayView<const Rendering::EncodedCommandBufferView>{encodedCommandBuffer}))
		{
			stagingBuffer.Destroy(*m_pLogicalDevice, m_pLogicalDevice->GetDeviceMemoryPool());
			image.Destroy(*m_pLogicalDevice, m_pLogicalDevice->GetDeviceMemoryPool());
			return {};
		}

		stagingBuffer.Destroy(*m_pLogicalDevice, m_pLogicalDevice->GetDeviceMemoryPool());
#if RENDERER_HAS_COMMAND_POOL
		m_commandPool.FreeCommandBuffers(*m_pLogicalDevice, ArrayView<Rendering::CommandBuffer>{commandBuffer});
#else
		commandBuffer.Destroy(*m_pLogicalDevice, m_commandPool);
#endif

		return Move(image);
	}

	bool RenderHelper::executeCommandBuffers(const ArrayView<const Rendering::EncodedCommandBufferView> encodedCommandBuffers) const
	{
		Rendering::Fence fence(*m_pLogicalDevice, Rendering::FenceView::Status::Unsignaled);
		if (!fence.IsValid())
		{
			return false;
		}

		struct State
		{
			Threading::Atomic<bool> finishedExecution{false};
		};
		State state;
		m_pLogicalDevice->GetQueueSubmissionJob(Rendering::QueueFamily::Graphics)
			.Queue(
				Threading::JobPriority::AssetCompilation,
				encodedCommandBuffers,
				Rendering::QueueSubmissionParameters{
					[]()
					{
					},         // on submitted
					[&state]() // on finished
					{
						{
							state.finishedExecution = true;
						}
					}
				}
			);

		while (!state.finishedExecution)
		{
			Threading::JobRunnerThread::GetCurrent()->DoRunNextJob();
		}

		fence.Destroy(*m_pLogicalDevice);
		return true;
	}
}
