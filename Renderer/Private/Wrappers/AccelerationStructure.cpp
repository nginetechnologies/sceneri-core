#include <Renderer/Wrappers/AccelerationStructure.h>
#include <Renderer/Wrappers/AccelerationStructureInstance.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Buffers/BufferView.h>

#include <Renderer/Commands/AccelerationStructureCommandEncoderView.h>

#include <Renderer/Metal/ConvertFormatToAttributeFormat.h>

namespace ngine::Rendering
{
	AccelerationStructureView::ResourceIdentifier
	AccelerationStructureView::GetResourceIdentifier([[maybe_unused]] const LogicalDevice& logicalDevice) const
	{
#if RENDERER_VULKAN
		const VkAccelerationStructureDeviceAddressInfoKHR accelerationDeviceAddressInfo{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_DEVICE_ADDRESS_INFO_KHR,
			nullptr,
			m_accelerationStructure
		};
		const PFN_vkGetAccelerationStructureDeviceAddressKHR getAccelerationStructureDeviceAddress =
			reinterpret_cast<PFN_vkGetAccelerationStructureDeviceAddressKHR>(logicalDevice.GetVkGetAccelerationStructureDeviceAddress());
		return getAccelerationStructureDeviceAddress(logicalDevice, &accelerationDeviceAddressInfo);
#elif RENDERER_METAL
		if (@available(macOS 13.0, iOS 16.0, *))
		{
			return [m_accelerationStructure gpuResourceID];
		}
		else
		{
			Assert(false, "Not supported!");
			return {0};
		}
#else
		return {};
#endif
	}

	PrimitiveAccelerationStructure::TriangleGeometryDescriptor::TriangleGeometryDescriptor(
		[[maybe_unused]] const LogicalDevice& logicalDevice,
		const Format vertexFormat,
		const BufferView vertexBuffer,
		const size vertexStride,
		[[maybe_unused]] const Index vertexCount,
		[[maybe_unused]] const Index indexCount,
		const BufferView indexBuffer
	)
#if RENDERER_VULKAN
		: m_geometry{
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR,
				nullptr,
				VK_GEOMETRY_TYPE_TRIANGLES_KHR,
				VkAccelerationStructureGeometryDataKHR{VkAccelerationStructureGeometryTrianglesDataKHR{
					VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_TRIANGLES_DATA_KHR,
					nullptr,
					static_cast<VkFormat>(vertexFormat),
					vertexBuffer.GetDeviceAddress(logicalDevice),
					vertexStride,
					vertexCount,
					Math::Select(TypeTraits::IsSame<Index, uint32>, VK_INDEX_TYPE_UINT32, VK_INDEX_TYPE_UINT16),
					indexBuffer.GetDeviceAddress(logicalDevice),
					VkDeviceAddress{0}
				}},
				VK_GEOMETRY_OPAQUE_BIT_KHR
			}
#elif RENDERER_METAL
		: m_descriptor([MTLAccelerationStructureTriangleGeometryDescriptor descriptor])
#endif
	{
#if RENDERER_METAL
		m_descriptor.opaque = true;
		m_descriptor.vertexBuffer = vertexBuffer;
		m_descriptor.vertexBufferOffset = 0;
		if (@available(macOS 13.0, iOS 17.0, *))
		{
			m_descriptor.vertexFormat = ConvertFormatToAttributeFormat(vertexFormat);
		}
		else
		{
			Assert(ConvertFormatToAttributeFormat(vertexFormat) == MTLAttributeFormatFloat3);
		}
		m_descriptor.vertexStride = vertexStride;
		m_descriptor.indexBuffer = indexBuffer;
		m_descriptor.indexBufferOffset = 0;
		m_descriptor.indexType = Math::Select(TypeTraits::IsSame<Index, uint32>, MTLIndexTypeUInt32, MTLIndexTypeUInt16);
		m_descriptor.triangleCount = indexCount / 3;
#elif RENDERER_WEBGPU
		UNUSED(vertexFormat);
		UNUSED(vertexBuffer);
		UNUSED(vertexStride);
		UNUSED(vertexCount);
		UNUSED(indexBuffer);
#endif
	}

	PrimitiveAccelerationStructureDescriptor::PrimitiveAccelerationStructureDescriptor(
		const ArrayView<const PrimitiveAccelerationStructure::TriangleGeometryDescriptor> geometryDescriptors
	)
#if RENDERER_VULKAN
		: m_geometryInfo{
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
				nullptr,
				VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
				VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
				VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
				nullptr,
				nullptr,
				geometryDescriptors.GetSize(),
				reinterpret_cast<const VkAccelerationStructureGeometryKHR*>(geometryDescriptors.GetData()),
				nullptr,
				VkDeviceAddress{}
			}
#elif RENDERER_METAL
		: m_descriptor([MTLPrimitiveAccelerationStructureDescriptor descriptor])
#endif
	{
#if RENDERER_VULKAN
		static_assert(sizeof(PrimitiveAccelerationStructure::TriangleGeometryDescriptor) == sizeof(VkAccelerationStructureGeometryKHR));
		static_assert(alignof(PrimitiveAccelerationStructure::TriangleGeometryDescriptor) == alignof(VkAccelerationStructureGeometryKHR));
#elif RENDERER_METAL
		m_descriptor.usage = MTLAccelerationStructureUsageNone;

		NSMutableArray<MTLAccelerationStructureGeometryDescriptor*>* array = [NSMutableArray<MTLAccelerationStructureGeometryDescriptor*>
			arrayWithCapacity:geometryDescriptors.GetSize()];
		for (uint32 i = 0; i < geometryDescriptors.GetSize(); i++)
		{
			[array addObject:geometryDescriptors[i].m_descriptor];
		}
		m_descriptor.geometryDescriptors = array;
#elif RENDERER_WEBGPU
		UNUSED(geometryDescriptors);
#endif
	}

	AccelerationStructureView::BuildSizesInfo PrimitiveAccelerationStructureDescriptor::CalculateBuildSizes(
		[[maybe_unused]] const LogicalDevice& logicalDevice, [[maybe_unused]] const ArrayView<const uint32> meshTriangleCounts
	)
	{
#if RENDERER_VULKAN
		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
			nullptr
		};
		const PFN_vkGetAccelerationStructureBuildSizesKHR getAccelerationStructureBuildSizes =
			reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(logicalDevice.GetVkGetAccelerationStructureBuildSizes());
		Assert(m_geometryInfo.geometryCount == meshTriangleCounts.GetSize());
		getAccelerationStructureBuildSizes(
			logicalDevice,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&m_geometryInfo,
			meshTriangleCounts.GetData(),
			&accelerationStructureBuildSizesInfo
		);
		return AccelerationStructureView::BuildSizesInfo{
			accelerationStructureBuildSizesInfo.accelerationStructureSize,
			accelerationStructureBuildSizesInfo.updateScratchSize,
			accelerationStructureBuildSizesInfo.buildScratchSize
		};
#elif RENDERER_METAL
		const MTLAccelerationStructureSizes sizes = [logicalDevice accelerationStructureSizesWithDescriptor:m_descriptor];
		return AccelerationStructureView::BuildSizesInfo{
			sizes.accelerationStructureSize,
			sizes.refitScratchBufferSize,
			sizes.buildScratchBufferSize
		};
#else
		Assert(false, "Unsupported");
		return {0, 0, 0};
#endif
	}

	PrimitiveAccelerationStructure::PrimitiveAccelerationStructure(
		[[maybe_unused]] const LogicalDevice& logicalDevice,
		const BufferView buffer,
		const DeviceMemoryView bufferMemoryView,
		[[maybe_unused]] const size bufferOffset,
		const size size
	)
	{
#if RENDERER_VULKAN
		UNUSED(bufferMemoryView);
		const VkAccelerationStructureCreateInfoKHR accelerationStructureCreationInfo{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
			nullptr,
			VkAccelerationStructureCreateFlagsKHR{0},
			buffer,
			0,
			size,
			VK_ACCELERATION_STRUCTURE_TYPE_BOTTOM_LEVEL_KHR,
			VkDeviceAddress{0}
		};

		const PFN_vkCreateAccelerationStructureKHR createAccelerationStructure =
			reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(logicalDevice.GetVkCreateAccelerationStructure());
		createAccelerationStructure(logicalDevice, &accelerationStructureCreationInfo, nullptr, &m_accelerationStructure);
#elif RENDERER_METAL
		UNUSED(buffer);
		if (@available(macOS 13.0, iOS 17.0, *))
		{
			m_accelerationStructure = [bufferMemoryView newAccelerationStructureWithSize:size offset:bufferOffset];
		}
		else
		{
			Assert(false, "Raytracing not supported for older versions");
		}
#elif RENDERER_WEBGPU
		UNUSED(buffer);
		UNUSED(bufferMemoryView);
		UNUSED(bufferOffset);
		UNUSED(size);
#endif
	}

	PrimitiveAccelerationStructure& PrimitiveAccelerationStructure::operator=([[maybe_unused]] PrimitiveAccelerationStructure&& other
	) noexcept
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN || RENDERER_METAL
		m_accelerationStructure = other.m_accelerationStructure;
		other.m_accelerationStructure = nullptr;
#endif
		return *this;
	}
	PrimitiveAccelerationStructure::~PrimitiveAccelerationStructure()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void PrimitiveAccelerationStructure::Destroy([[maybe_unused]] const LogicalDevice& logicalDevice)
	{
#if RENDERER_VULKAN
		if (m_accelerationStructure != nullptr)
		{
			const PFN_vkDestroyAccelerationStructureKHR destroyAccelerationStructure =
				reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(logicalDevice.GetVkDestroyAccelerationStructure());
			destroyAccelerationStructure(logicalDevice, m_accelerationStructure, nullptr);
			m_accelerationStructure = nullptr;
		}
#elif RENDERER_METAL
		m_accelerationStructure = nullptr;
#endif
	}

	void AccelerationStructureCommandEncoderView::Build(
		[[maybe_unused]] const LogicalDevice& logicalDevice,
		const PrimitiveAccelerationStructureView accelerationStructure,
		PrimitiveAccelerationStructureDescriptor& descriptor,
		const BufferView scratchBuffer,
		const size scratchBufferOffset,
		[[maybe_unused]] const ArrayView<const PrimitiveAccelerationStructureBuildRangeInfo> perPrimitiveBuildRangeInfo
	) const
	{
#if RENDERER_VULKAN
		VkAccelerationStructureBuildGeometryInfoKHR& geometryInfo = descriptor.m_geometryInfo;
		geometryInfo.dstAccelerationStructure = accelerationStructure;
		geometryInfo.scratchData.deviceAddress = scratchBuffer.GetDeviceAddress(logicalDevice) + scratchBufferOffset;

		static_assert(sizeof(PrimitiveAccelerationStructureBuildRangeInfo) == sizeof(VkAccelerationStructureBuildRangeInfoKHR));
		static_assert(alignof(PrimitiveAccelerationStructureBuildRangeInfo) == alignof(VkAccelerationStructureBuildRangeInfoKHR));

		Array<const VkAccelerationStructureBuildRangeInfoKHR* const, 1> buildRangeInfos{
			reinterpret_cast<const VkAccelerationStructureBuildRangeInfoKHR*>(perPrimitiveBuildRangeInfo.GetData())
		};

		const PFN_vkCmdBuildAccelerationStructuresKHR buildAccelerationStructures =
			reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(logicalDevice.GetVkCmdBuildAccelerationStructuresKHR());
		buildAccelerationStructures(m_pCommandEncoder, 1, &geometryInfo, buildRangeInfos.GetData());
#elif RENDERER_METAL
		[m_pCommandEncoder buildAccelerationStructure:accelerationStructure
																			 descriptor:descriptor.m_descriptor
																		scratchBuffer:scratchBuffer
															scratchBufferOffset:scratchBufferOffset];
#elif RENDERER_WEBGPU
		UNUSED(accelerationStructure);
		UNUSED(descriptor);
		UNUSED(scratchBuffer);
		UNUSED(scratchBufferOffset);
#endif
	}

#if RENDERER_VULKAN
	[[nodiscard]] VkAccelerationStructureGeometryDataKHR
	CreateInstanceStructure(const LogicalDevice& logicalDevice, const BufferView instancesBuffer)
	{
		VkAccelerationStructureGeometryDataKHR data;
		data.instances = VkAccelerationStructureGeometryInstancesDataKHR{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_INSTANCES_DATA_KHR,
			nullptr,
			VK_FALSE,
			VkDeviceAddress{instancesBuffer.GetDeviceAddress(logicalDevice)}
		};
		return data;
	}
#endif

	InstanceAccelerationStructureDescriptor::InstanceAccelerationStructureDescriptor(
		[[maybe_unused]] const LogicalDevice& logicalDevice,
		const BufferView instancesBuffer,
		const uint32 maximumInstanceCount,
		[[maybe_unused]] const BufferView instanceCountBuffer
	)
#if RENDERER_VULKAN
		: m_geometry{VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_GEOMETRY_KHR, nullptr, VK_GEOMETRY_TYPE_INSTANCES_KHR, CreateInstanceStructure(logicalDevice, instancesBuffer), VK_GEOMETRY_OPAQUE_BIT_KHR}
		, m_instanceInfo{
				VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_GEOMETRY_INFO_KHR,
				nullptr,
				VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
				VK_BUILD_ACCELERATION_STRUCTURE_PREFER_FAST_TRACE_BIT_KHR,
				VK_BUILD_ACCELERATION_STRUCTURE_MODE_BUILD_KHR,
				nullptr,
				nullptr,
				1,
				&m_geometry,
				nullptr,
				VkDeviceAddress{}
			}
#endif
	{
#if RENDERER_VULKAN
		UNUSED(maximumInstanceCount);
		static_assert(sizeof(PrimitiveAccelerationStructure::TriangleGeometryDescriptor) == sizeof(VkAccelerationStructureGeometryKHR));
		static_assert(alignof(PrimitiveAccelerationStructure::TriangleGeometryDescriptor) == alignof(VkAccelerationStructureGeometryKHR));
#elif RENDERER_METAL
		if (@available(macOS 14.0, iOS 17.0, *))
		{
			m_descriptor = [MTLIndirectInstanceAccelerationStructureDescriptor descriptor];
			m_descriptor.usage = MTLAccelerationStructureUsageNone;

			m_descriptor.instanceDescriptorBuffer = instancesBuffer;
			m_descriptor.instanceDescriptorBufferOffset = 0;
			m_descriptor.instanceDescriptorStride = sizeof(MTLIndirectAccelerationStructureInstanceDescriptor);

			m_descriptor.maxInstanceCount = maximumInstanceCount;
			m_descriptor.instanceCountBuffer = instanceCountBuffer;
			m_descriptor.instanceCountBufferOffset = 0;

			m_descriptor.instanceDescriptorType = MTLAccelerationStructureInstanceDescriptorTypeIndirect;
			// m_descriptor.instanceTransformationMatrixLayout = MTLMatrixLayoutColumnMajor;
		}
		else
		{
			Assert(false, "Raytracing not supported for platform");
		}
#elif RENDERER_WEBGPU
		UNUSED(instancesBuffer);
		UNUSED(maximumInstanceCount);
#endif
	}

	AccelerationStructureView::BuildSizesInfo InstanceAccelerationStructureDescriptor::CalculateBuildSizes(
		[[maybe_unused]] const LogicalDevice& logicalDevice, [[maybe_unused]] const uint32 instanceCount
	)
	{
#if RENDERER_VULKAN
		VkAccelerationStructureBuildSizesInfoKHR accelerationStructureBuildSizesInfo{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_BUILD_SIZES_INFO_KHR,
			nullptr
		};
		const PFN_vkGetAccelerationStructureBuildSizesKHR getAccelerationStructureBuildSizes =
			reinterpret_cast<PFN_vkGetAccelerationStructureBuildSizesKHR>(logicalDevice.GetVkGetAccelerationStructureBuildSizes());
		getAccelerationStructureBuildSizes(
			logicalDevice,
			VK_ACCELERATION_STRUCTURE_BUILD_TYPE_DEVICE_KHR,
			&m_instanceInfo,
			&instanceCount,
			&accelerationStructureBuildSizesInfo
		);
		return AccelerationStructureView::BuildSizesInfo{
			accelerationStructureBuildSizesInfo.accelerationStructureSize,
			accelerationStructureBuildSizesInfo.updateScratchSize,
			accelerationStructureBuildSizesInfo.buildScratchSize
		};
#elif RENDERER_METAL
		const MTLAccelerationStructureSizes sizes = [logicalDevice accelerationStructureSizesWithDescriptor:m_descriptor];
		return AccelerationStructureView::BuildSizesInfo{
			sizes.accelerationStructureSize,
			sizes.refitScratchBufferSize,
			sizes.buildScratchBufferSize
		};
#else
		Assert(false, "Unsupported");
		return {0, 0, 0};
#endif
	}

	InstanceAccelerationStructure::InstanceAccelerationStructure(
		[[maybe_unused]] const LogicalDevice& logicalDevice,
		const BufferView buffer,
		const DeviceMemoryView bufferMemoryView,
		[[maybe_unused]] const size bufferOffset,
		const size size
	)
	{
#if RENDERER_VULKAN
		UNUSED(bufferMemoryView);
		const VkAccelerationStructureCreateInfoKHR accelerationStructureCreationInfo{
			VK_STRUCTURE_TYPE_ACCELERATION_STRUCTURE_CREATE_INFO_KHR,
			nullptr,
			VkAccelerationStructureCreateFlagsKHR{0},
			buffer,
			0,
			size,
			VK_ACCELERATION_STRUCTURE_TYPE_TOP_LEVEL_KHR,
			VkDeviceAddress{0}
		};

		const PFN_vkCreateAccelerationStructureKHR createAccelerationStructure =
			reinterpret_cast<PFN_vkCreateAccelerationStructureKHR>(logicalDevice.GetVkCreateAccelerationStructure());
		createAccelerationStructure(logicalDevice, &accelerationStructureCreationInfo, nullptr, &m_accelerationStructure);
#elif RENDERER_METAL
		UNUSED(buffer);
		if (@available(macOS 13.0, iOS 17.0, *))
		{
			m_accelerationStructure = [bufferMemoryView newAccelerationStructureWithSize:size offset:bufferOffset];
		}
		else
		{
			Assert(false, "Raytracing not supported for older versions");
		}
#elif RENDERER_WEBGPU
		UNUSED(buffer);
		UNUSED(bufferMemoryView);
		UNUSED(bufferOffset);
		UNUSED(size);
#endif
	}

	InstanceAccelerationStructure& InstanceAccelerationStructure::operator=([[maybe_unused]] InstanceAccelerationStructure&& other) noexcept
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN || RENDERER_METAL
		m_accelerationStructure = other.m_accelerationStructure;
		other.m_accelerationStructure = nullptr;
#endif
		return *this;
	}
	InstanceAccelerationStructure::~InstanceAccelerationStructure()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void InstanceAccelerationStructure::Destroy([[maybe_unused]] const LogicalDevice& logicalDevice)
	{
#if RENDERER_VULKAN
		if (m_accelerationStructure != nullptr)
		{
			const PFN_vkDestroyAccelerationStructureKHR destroyAccelerationStructure =
				reinterpret_cast<PFN_vkDestroyAccelerationStructureKHR>(logicalDevice.GetVkDestroyAccelerationStructure());
			destroyAccelerationStructure(logicalDevice, m_accelerationStructure, nullptr);
			m_accelerationStructure = nullptr;
		}
#elif RENDERER_METAL
		m_accelerationStructure = nullptr;
#endif
	}

	void AccelerationStructureCommandEncoderView::Build(
		[[maybe_unused]] const LogicalDevice& logicalDevice,
		const InstanceAccelerationStructureView accelerationStructure,
		InstanceAccelerationStructureDescriptor& descriptor,
		const BufferView scratchBuffer,
		const size scratchBufferOffset,
		[[maybe_unused]] const uint32 instanceCount
	) const
	{
#if RENDERER_VULKAN
		const VkAccelerationStructureBuildRangeInfoKHR accelerationStructureBuildRangeInfo{
			instanceCount,
			0,
			0,
			0 // uint32(m_instanceBuffer.GetFirstInstanceIndex() * sizeof(InstanceAccelerationStructure::Instance))
		};

		VkAccelerationStructureBuildGeometryInfoKHR& geometryInfo = descriptor.m_instanceInfo;
		geometryInfo.dstAccelerationStructure = accelerationStructure;
		geometryInfo.scratchData.deviceAddress = scratchBuffer.GetDeviceAddress(logicalDevice) + scratchBufferOffset;

		Array<const VkAccelerationStructureBuildRangeInfoKHR* const, 1> buildRangeInfos{&accelerationStructureBuildRangeInfo};

		const PFN_vkCmdBuildAccelerationStructuresKHR buildAccelerationStructures =
			reinterpret_cast<PFN_vkCmdBuildAccelerationStructuresKHR>(logicalDevice.GetVkCmdBuildAccelerationStructuresKHR());
		buildAccelerationStructures(m_pCommandEncoder, 1, &geometryInfo, buildRangeInfos.GetData());
#elif RENDERER_METAL
		[m_pCommandEncoder buildAccelerationStructure:accelerationStructure
																			 descriptor:descriptor.m_descriptor
																		scratchBuffer:scratchBuffer
															scratchBufferOffset:scratchBufferOffset];
#elif RENDERER_WEBGPU
		UNUSED(accelerationStructure);
		UNUSED(descriptor);
		UNUSED(scratchBuffer);
		UNUSED(scratchBufferOffset);
#endif
	}

	AccelerationStructureInstance::AccelerationStructureInstance(
		const Math::Matrix3x4f transform,
		const uint32 instanceCustomIndex,
		const uint8 mask,
		const EnumFlags<Flags> flags,
		const AccelerationStructureView::ResourceIdentifier primitiveAccelerationStructureResourceIdentifier
	)
#if RENDERER_VULKAN
		: m_instance{
				VkTransformMatrixKHR{
					transform.m_right.x,
					transform.m_forward.x,
					transform.m_up.x,
					transform.m_location.x,
					transform.m_right.y,
					transform.m_forward.y,
					transform.m_up.y,
					transform.m_location.y,
					transform.m_right.z,
					transform.m_forward.z,
					transform.m_up.z,
					transform.m_location.z
				},
				instanceCustomIndex,
				mask,
				0,
				static_cast<VkGeometryInstanceFlagsKHR>(
					(VK_GEOMETRY_INSTANCE_TRIANGLE_FACING_CULL_DISABLE_BIT_KHR * flags.IsSet(Flags::DisableTriangleFaceCulling)) |
					VK_GEOMETRY_INSTANCE_TRIANGLE_FRONT_COUNTERCLOCKWISE_BIT_KHR
				),
				primitiveAccelerationStructureResourceIdentifier
			}
#elif RENDERER_METAL
		: m_instance{
				MTLPackedFloat4x3{
					MTLPackedFloat3(transform.m_right.x, transform.m_right.y, transform.m_right.z),
					MTLPackedFloat3(transform.m_forward.x, transform.m_forward.y, transform.m_forward.z),
					MTLPackedFloat3(transform.m_up.x, transform.m_up.y, transform.m_up.z),
					MTLPackedFloat3(transform.m_location.x, transform.m_location.y, transform.m_location.z)
				},
				(MTLAccelerationStructureInstanceOptionDisableTriangleCulling * flags.IsSet(Flags::DisableTriangleFaceCulling)) |
					MTLAccelerationStructureInstanceOptionTriangleFrontFacingWindingCounterClockwise | MTLAccelerationStructureInstanceOptionOpaque,
				mask,
				0,
				instanceCustomIndex,
				primitiveAccelerationStructureResourceIdentifier
			}
#endif
	{
#if RENDERER_WEBGPU
		UNUSED(transform);
		UNUSED(instanceCustomIndex);
		UNUSED(mask);
		UNUSED(flags);
		UNUSED(primitiveAccelerationStructureResourceIdentifier);
#endif
	}
}
