#pragma once

#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>
#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

#include <Renderer/Constants.h>
#include <Renderer/Devices/QueueFamily.h>
#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#if PROFILE_BUILD
#include <Common/Memory/Containers/ZeroTerminatedStringView.h>
#include <Common/Math/Color.h>
#endif

#include <Common/Math/ForwardDeclarations/Vector3.h>
#include <Common/Math/Color.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct CommandBufferView;
	struct BufferView;
	struct StagingBuffer;

	struct PrimitiveAccelerationStructureView;
	struct PrimitiveAccelerationStructureDescriptor;
	struct PrimitiveAccelerationStructureBuildRangeInfo;
	struct InstanceAccelerationStructureView;
	struct InstanceAccelerationStructureDescriptor;

	struct TRIVIAL_ABI AccelerationStructureCommandEncoderView
	{
		AccelerationStructureCommandEncoderView() = default;
#if RENDERER_VULKAN
		constexpr AccelerationStructureCommandEncoderView(VkCommandBuffer pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}
		[[nodiscard]] operator VkCommandBuffer() const
		{
			return m_pCommandEncoder;
		}
#elif RENDERER_METAL
		constexpr AccelerationStructureCommandEncoderView(id<MTLAccelerationStructureCommandEncoder> commandEncoder)
			: m_pCommandEncoder(commandEncoder)
		{
		}
		[[nodiscard]] operator id<MTLAccelerationStructureCommandEncoder>() const
		{
			return m_pCommandEncoder;
		}
#elif WEBGPU_INDIRECT_HANDLES
		constexpr AccelerationStructureCommandEncoderView(WGPUCommandEncoder* pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}
		[[nodiscard]] operator WGPUCommandEncoder() const
		{
			return m_pCommandEncoder != nullptr ? *m_pCommandEncoder : nullptr;
		}
#elif RENDERER_WEBGPU
		constexpr AccelerationStructureCommandEncoderView(WGPUCommandEncoder pCommandEncoder)
			: m_pCommandEncoder(pCommandEncoder)
		{
		}
		[[nodiscard]] operator WGPUCommandEncoder() const
		{
			return m_pCommandEncoder;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU || RENDERER_METAL
			return m_pCommandEncoder != nullptr;
#else
			return false;
#endif
		}

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name) const;
		void BeginDebugMarker(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView label, const Math::Color color) const;
		void EndDebugMarker(const LogicalDevice& logicalDevice) const;

		void Build(
			const LogicalDevice& logicalDevice,
			const PrimitiveAccelerationStructureView accelerationStructure,
			PrimitiveAccelerationStructureDescriptor& descriptor,
			const BufferView scratchBuffer,
			const size scratchBufferOffset,
			const ArrayView<const PrimitiveAccelerationStructureBuildRangeInfo> perPrimitiveBuildRangeInfo
		) const;
		void Build(
			const LogicalDevice& logicalDevice,
			const InstanceAccelerationStructureView accelerationStructure,
			InstanceAccelerationStructureDescriptor& descriptor,
			const BufferView scratchBuffer,
			const size scratchBufferOffset,
			const uint32 instanceCount
		) const;
	protected:
		friend struct CommandEncoder;

#if RENDERER_VULKAN
		VkCommandBuffer m_pCommandEncoder = nullptr;
#elif RENDERER_METAL
		id<MTLAccelerationStructureCommandEncoder> m_pCommandEncoder;
#elif WEBGPU_INDIRECT_HANDLES
		WGPUCommandEncoder* m_pCommandEncoder = nullptr;
#elif RENDERER_WEBGPU
		WGPUCommandEncoder m_pCommandEncoder = nullptr;
#endif
	};

	struct AccelerationStructureDebugMarker
	{
		AccelerationStructureDebugMarker(
			const AccelerationStructureCommandEncoderView accelerationStructureCommandEncoder,
			const LogicalDevice& logicalDevice,
			const ConstZeroTerminatedStringView label,
			const Math::Color color
		)
			: m_accelerationStructureCommandEncoder(accelerationStructureCommandEncoder)
			, m_logicalDevice(logicalDevice)
		{
			accelerationStructureCommandEncoder.BeginDebugMarker(logicalDevice, label, color);
		}
		~AccelerationStructureDebugMarker()
		{
			m_accelerationStructureCommandEncoder.EndDebugMarker(m_logicalDevice);
		}
	private:
		const AccelerationStructureCommandEncoderView m_accelerationStructureCommandEncoder;
		const LogicalDevice& m_logicalDevice;
	};
}
