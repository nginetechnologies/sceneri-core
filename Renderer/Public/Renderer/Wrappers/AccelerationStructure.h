#pragma once

#include "AccelerationStructureView.h"

#include <Renderer/Format.h>
#include <Renderer/Index.h>
#include <Renderer/Vulkan/Includes.h>

#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Math/Matrix3x4.h>

namespace ngine::Rendering
{
	struct BufferView;
	struct DeviceMemoryView;
	struct AccelerationStructureCommandEncoderView;

	PUSH_CLANG_WARNINGS
	DISABLE_CLANG_WARNING("-Wunguarded-availability-new")

	struct PrimitiveAccelerationStructureDescriptor;

	struct PrimitiveAccelerationStructure : public PrimitiveAccelerationStructureView
	{
		struct Descriptor;

		struct TriangleGeometryDescriptor
		{
			TriangleGeometryDescriptor(
				const LogicalDevice& logicalDevice,
				const Format vertexFormat,
				const BufferView vertexBuffer,
				const size vertexStride,
				const Index vertexCount,
				const Index indexCount,
				const BufferView indexBuffer
			);
		private:
			friend PrimitiveAccelerationStructureDescriptor;
			friend AccelerationStructureCommandEncoderView;

#if RENDERER_VULKAN
			VkAccelerationStructureGeometryKHR m_geometry;
#elif RENDERER_METAL
			MTLAccelerationStructureTriangleGeometryDescriptor* m_descriptor;
#endif
		};

		PrimitiveAccelerationStructure() = default;
		PrimitiveAccelerationStructure(
			const LogicalDevice& logicalDevice,
			const BufferView buffer,
			const DeviceMemoryView bufferMemoryView,
			const size bufferOffset,
			const size size
		);
		PrimitiveAccelerationStructure(const PrimitiveAccelerationStructure&) = delete;
		PrimitiveAccelerationStructure& operator=(const PrimitiveAccelerationStructure&) = delete;
		PrimitiveAccelerationStructure([[maybe_unused]] PrimitiveAccelerationStructure&& other) noexcept
		{
#if RENDERER_VULKAN || RENDERER_METAL
			m_accelerationStructure = other.m_accelerationStructure;
			other.m_accelerationStructure = nullptr;
#endif
		}
		PrimitiveAccelerationStructure& operator=(PrimitiveAccelerationStructure&& other) noexcept;
		~PrimitiveAccelerationStructure();

		void Destroy(const LogicalDevice& logicalDevice);
	};

	struct PrimitiveAccelerationStructureBuildRangeInfo
	{
		uint32 primitiveCount;
		uint32 primitiveOffset;
		uint32 firstVertex;
		uint32 transformOffset;
	};

	struct PrimitiveAccelerationStructureDescriptor
	{
		PrimitiveAccelerationStructureDescriptor(
			const ArrayView<const PrimitiveAccelerationStructure::TriangleGeometryDescriptor> geometryDescriptors
		);

		[[nodiscard]] AccelerationStructureView::BuildSizesInfo
		CalculateBuildSizes(const LogicalDevice& logicalDevice, const ArrayView<const uint32> triangleCounts);
	private:
		friend AccelerationStructureCommandEncoderView;

#if RENDERER_VULKAN
		VkAccelerationStructureBuildGeometryInfoKHR m_geometryInfo;
#elif RENDERER_METAL
		MTLPrimitiveAccelerationStructureDescriptor* m_descriptor;
#endif
	};

	struct InstanceAccelerationStructureDescriptor
	{
		InstanceAccelerationStructureDescriptor(
			const LogicalDevice& logicalDevice,
			const BufferView instancesBuffer,
			const uint32 maximumInstanceCount,
			const BufferView instanceCountBuffer
		);

		[[nodiscard]] AccelerationStructureView::BuildSizesInfo
		CalculateBuildSizes(const LogicalDevice& logicalDevice, const uint32 instanceCount);
	private:
		friend AccelerationStructureCommandEncoderView;

#if RENDERER_VULKAN
		VkAccelerationStructureGeometryKHR m_geometry;
		VkAccelerationStructureBuildGeometryInfoKHR m_instanceInfo;
#elif RENDERER_METAL
		MTLIndirectInstanceAccelerationStructureDescriptor* m_descriptor;
#endif
	};

	struct AccelerationStructureInstance;

	struct InstanceAccelerationStructure : public InstanceAccelerationStructureView
	{
		using Instance = AccelerationStructureInstance;

		InstanceAccelerationStructure() = default;
		InstanceAccelerationStructure(
			const LogicalDevice& logicalDevice,
			const BufferView buffer,
			const DeviceMemoryView bufferMemoryView,
			const size bufferOffset,
			const size size
		);
		InstanceAccelerationStructure(const InstanceAccelerationStructure&) = delete;
		InstanceAccelerationStructure& operator=(const InstanceAccelerationStructure&) = delete;
		InstanceAccelerationStructure([[maybe_unused]] InstanceAccelerationStructure&& other) noexcept
		{
#if RENDERER_VULKAN || RENDERER_METAL
			m_accelerationStructure = other.m_accelerationStructure;
			other.m_accelerationStructure = nullptr;
#endif
		}
		InstanceAccelerationStructure& operator=(InstanceAccelerationStructure&& other) noexcept;
		~InstanceAccelerationStructure();

		void Destroy(const LogicalDevice& logicalDevice);
	};

	POP_CLANG_WARNINGS
}
