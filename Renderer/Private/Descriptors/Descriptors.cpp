#include <Renderer/Descriptors/DescriptorPool.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>
#include <Renderer/Descriptors/DescriptorSet.h>

#include <Renderer/Devices/LogicalDevice.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Window/Window.h>

#include <Renderer/Metal/Includes.h>
#include <Renderer/Metal/DescriptorSetData.h>
#include "Metal/ConvertShaderStages.h"

#include <Renderer/WebGPU/Includes.h>
#include "WebGPU/ConvertShaderStage.h"
#include "WebGPU/ConvertFormat.h"
#include "WebGPU/ConvertImageMappingType.h"

#include <Common/Assert/Assert.h>
#include <Common/Memory/Containers/Array.h>

#include <Common/Threading/Atomics/Exchange.h>
#include <Common/Threading/Atomics/Load.h>
#include <Common/IO/Log.h>

namespace ngine::Rendering
{
#if RENDERER_VULKAN
	static_assert(sizeof(DescriptorPool::Size) == sizeof(VkDescriptorPoolSize));
	static_assert(alignof(DescriptorPool::Size) == alignof(VkDescriptorPoolSize));
#endif

	DescriptorPool::DescriptorPool(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		[[maybe_unused]] const uint32 maximumDescriptorSetCount,
		[[maybe_unused]] const ArrayView<const Size, uint8> poolSizes,
		[[maybe_unused]] const CreationFlags flags
	)
	{
#if RENDERER_VULKAN
		const VkDescriptorPoolCreateInfo poolInfo{
			VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
			nullptr,
			static_cast<VkDescriptorPoolCreateFlags>(flags),
			maximumDescriptorSetCount,
			poolSizes.GetSize(),
			reinterpret_cast<const VkDescriptorPoolSize*>(poolSizes.GetData())
		};

		[[maybe_unused]] const VkResult descriptorPoolCreationResult = vkCreateDescriptorPool(logicalDevice, &poolInfo, nullptr, &m_pPool);
		Assert(descriptorPoolCreationResult == VK_SUCCESS);
#endif
	}

	DescriptorPool& DescriptorPool::operator=([[maybe_unused]] DescriptorPool&& other)
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN
		m_pPool = other.m_pPool;
		other.m_pPool = 0;
#endif
		return *this;
	}

	DescriptorPool::~DescriptorPool()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void DescriptorPool::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroyDescriptorPool(logicalDevice, m_pPool, nullptr);
		m_pPool = 0;
#endif
	}

#if RENDERER_METAL || RENDERER_WEBGPU
	[[nodiscard]] uint8 GetBindingCount(const DescriptorType type)
	{
		switch (type)
		{
			case DescriptorType::Sampler:
			case DescriptorType::SampledImage:
			case DescriptorType::InputAttachment:
			case DescriptorType::UniformTexelBuffer:
			case DescriptorType::UniformBuffer:
			case DescriptorType::UniformBufferDynamic:
			case DescriptorType::StorageBuffer:
			case DescriptorType::StorageBufferDynamic:
			case DescriptorType::AccelerationStructure:
			case DescriptorType::StorageImage:
			case DescriptorType::StorageTexelBuffer:
				return 1;
			case DescriptorType::CombinedImageSampler:
				return 2;
		}
		ExpectUnreachable();
	};
#endif

	bool DescriptorPoolView::AllocateDescriptorSets(
		LogicalDevice& logicalDevice,
		const ArrayView<const DescriptorSetLayoutView, uint8> layouts,
		const ArrayView<DescriptorSet, uint8> descriptorSetsOut,
		[[maybe_unused]] const ArrayView<const uint32, uint8> maximumVariableDescriptorCount
	) const
	{
		Assert(layouts.GetSize() == descriptorSetsOut.GetSize());
		Assert(descriptorSetsOut.All(
			[](const DescriptorSetView descriptorSet)
			{
				return !descriptorSet.IsValid();
			}
		));

#if RENDERER_VULKAN
		Assert(maximumVariableDescriptorCount.IsEmpty() || maximumVariableDescriptorCount.GetSize() == descriptorSetsOut.GetSize());
		VkDescriptorSetVariableDescriptorCountAllocateInfo variableCountInfo{
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_VARIABLE_DESCRIPTOR_COUNT_ALLOCATE_INFO,
			nullptr,
			maximumVariableDescriptorCount.GetSize(),
			maximumVariableDescriptorCount.GetData()
		};

		const VkDescriptorSetAllocateInfo allocateDescriptorSets = {
			VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
			maximumVariableDescriptorCount.HasElements() ? &variableCountInfo : nullptr,
			m_pPool,
			layouts.GetSize(),
			reinterpret_cast<const VkDescriptorSetLayout*>(layouts.GetData())
		};

		const VkResult result =
			vkAllocateDescriptorSets(logicalDevice, &allocateDescriptorSets, reinterpret_cast<VkDescriptorSet*>(descriptorSetsOut.GetData()));
		Assert(result == VK_SUCCESS);
		return result == VK_SUCCESS;
#elif RENDERER_METAL
		for (DescriptorSet& descriptorSetOut : descriptorSetsOut)
		{
			const uint8 index = descriptorSetsOut.GetIteratorIndex(&descriptorSetOut);
			const Internal::DescriptorSetLayoutData* __restrict pLayoutData = layouts[index];

			Internal::DescriptorSetData* __restrict pDescriptorSetData = new Internal::DescriptorSetData{layouts[index]};

			const size argumentBufferSize = [pLayoutData->m_encoder encodedLength];

			pDescriptorSetData->m_argumentBuffer = Buffer(
				logicalDevice,
				logicalDevice.GetPhysicalDevice(),
				logicalDevice.GetDeviceMemoryPool(),
				argumentBufferSize,
				Buffer::UsageFlags::VertexBuffer,
				MemoryFlags::HostCoherent | MemoryFlags::HostVisible
			);

			pDescriptorSetData->m_resourceBatches.Resize(pLayoutData->m_resourceBatches.GetSize());
			for (uint8 resourceBatchIndex = 0, count = pLayoutData->m_resourceBatches.GetSize(); resourceBatchIndex < count; ++resourceBatchIndex)
			{
				const uint32 resourceCount = pLayoutData->m_resourceBatches[resourceBatchIndex].m_resourceCount;
				pDescriptorSetData->m_resourceBatches[resourceBatchIndex].m_resources.Reserve(resourceCount);
				pDescriptorSetData->m_resourceBatches[resourceBatchIndex].m_resourceIndices.Resize(resourceCount, Memory::Zeroed);
			}

			uint32 objectCount = 0;
			for (const Internal::DescriptorSetLayoutData::Binding& __restrict binding : pLayoutData->m_bindings)
			{
				objectCount += GetBindingCount(binding.m_type) * binding.m_arraySize;
			}

			pDescriptorSetData->m_bufferOffsets.Resize(objectCount);
			pDescriptorSetData->m_objects.Resize(objectCount);

			descriptorSetOut = DescriptorSet{pDescriptorSetData};
		}
		return true;
#elif RENDERER_WEBGPU
		UNUSED(logicalDevice);
		for (DescriptorSet& descriptorSetOut : descriptorSetsOut)
		{
			const uint8 index = descriptorSetsOut.GetIteratorIndex(&descriptorSetOut);

			const DescriptorSetLayoutView descriptorSetLayout = layouts[index];
			const Internal::DescriptorSetLayoutData* __restrict pLayoutData = descriptorSetLayout;

			Internal::DescriptorSetData* __restrict pDescriptorSetData = new Internal::DescriptorSetData{descriptorSetLayout};
			pDescriptorSetData->m_bindGroupEntries.Resize(pLayoutData->m_bindEntryCount, Memory::Zeroed);
			descriptorSetOut = DescriptorSet{pDescriptorSetData};
		}
		return true;
#else
		UNUSED(logicalDevice);
		return false;
#endif
	}

	void
	DescriptorPoolView::FreeDescriptorSets([[maybe_unused]] LogicalDevice& logicalDevice, const ArrayView<DescriptorSet> descriptorSets) const
	{
#if RENDERER_VULKAN
		vkFreeDescriptorSets(
			logicalDevice,
			m_pPool,
			descriptorSets.GetSize(),
			reinterpret_cast<const VkDescriptorSet*>(descriptorSets.GetData())
		);
#elif RENDERER_METAL
		for (const DescriptorSetView descriptorSet : descriptorSets)
		{
			Internal::DescriptorSetData* pDescriptorSetData{descriptorSet};
			pDescriptorSetData->m_argumentBuffer.Destroy(logicalDevice, logicalDevice.GetDeviceMemoryPool());
			delete pDescriptorSetData;
		}
#elif RENDERER_WEBGPU
		for (const DescriptorSetView descriptorSet : descriptorSets)
		{
			Internal::DescriptorSetData* pDescriptorSetData{descriptorSet};
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pDescriptorSetData]()
				{
					if (pDescriptorSetData->m_bindGroup != nullptr)
					{
						wgpuBindGroupRelease(pDescriptorSetData->m_bindGroup);
					}
					delete pDescriptorSetData;
				}
			);
		}
#endif

		for (DescriptorSet& descriptorSet : descriptorSets)
		{
			descriptorSet.OnFreed();
		}
	}

#if PLATFORM_APPLE_VISIONOS
	using MetalBindingAccess = MTLBindingAccess;
	inline static constexpr MetalBindingAccess MetalBindingAccessReadOnly = MTLBindingAccessReadOnly;
	// inline static constexpr MetalBindingAccess MetalBindingAccessWriteOnly = MTLBindingAccessWriteOnly;
	inline static constexpr MetalBindingAccess MetalBindingAccessReadWrite = MTLBindingAccessReadWrite;
#elif RENDERER_METAL
	using MetalBindingAccess = MTLArgumentAccess;
	inline static constexpr MetalBindingAccess MetalBindingAccessReadOnly = MTLArgumentAccessReadOnly;
	// inline static constexpr MetalBindingAccess MetalBindingAccessWriteOnly = MTLArgumentAccessWriteOnly;
	inline static constexpr MetalBindingAccess MetalBindingAccessReadWrite = MTLArgumentAccessReadWrite;
#endif

	// DescriptorSetLayout
#if RENDERER_VULKAN
	static_assert(sizeof(DescriptorSetLayout::Binding) == sizeof(VkDescriptorSetLayoutBinding));
	static_assert(alignof(DescriptorSetLayout::Binding) == alignof(VkDescriptorSetLayoutBinding));
#endif

	DescriptorSetLayout::DescriptorSetLayout(
		const LogicalDeviceView logicalDevice,
		const ArrayView<const Binding, uint8> bindings,
		const EnumFlags<Flags> flags,
		[[maybe_unused]] const ArrayView<const EnumFlags<Binding::Flags>, uint8> bindingsFlags
	)
	{
		Assert(bindingsFlags.IsEmpty() || bindings.GetSize() == bindingsFlags.GetSize(), "Must specify flags for all elements");

#if RENDERER_VULKAN
		if (bindings.HasElements())
		{
			VkDescriptorSetLayoutBindingFlagsCreateInfo bindingFlagsCreateInfo{
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_BINDING_FLAGS_CREATE_INFO,
				nullptr,
				bindingsFlags.GetSize(),
				reinterpret_cast<const VkDescriptorBindingFlags*>(bindingsFlags.GetData())
			};

			const VkDescriptorSetLayoutCreateInfo layoutInfo = {
				VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
				bindingsFlags.HasElements() ? &bindingFlagsCreateInfo : nullptr,
				static_cast<VkDescriptorSetLayoutCreateFlags>(flags.GetUnderlyingValue()),
				bindings.GetSize(),
				reinterpret_cast<const VkDescriptorSetLayoutBinding*>(bindings.GetData())
			};

			[[maybe_unused]] const VkResult result = vkCreateDescriptorSetLayout(logicalDevice, &layoutInfo, nullptr, &m_pLayout);
			Assert(result == VK_SUCCESS);
		}
#elif RENDERER_METAL
		UNUSED(flags);

		NSMutableArray<MTLArgumentDescriptor*>* argumentDescriptors;
		{
			const uint32 bindingCounts = bindings.Count(
				[](const Binding& __restrict binding)
				{
					return GetBindingCount((DescriptorType)binding.m_type);
				}
			);
			argumentDescriptors = [NSMutableArray<MTLArgumentDescriptor*> arrayWithCapacity:bindingCounts];
		}

		auto addArgumentDescriptor =
			[&argumentDescriptors](const uint32 index, const MTLDataType dataType, const MetalBindingAccess access, const uint32 arrayLength)
		{
			MTLArgumentDescriptor* argumentDescriptor = [MTLArgumentDescriptor argumentDescriptor];
			argumentDescriptor.index = index;
			argumentDescriptor.dataType = dataType;
			argumentDescriptor.access = access;
			argumentDescriptor.arrayLength = arrayLength;

			if (dataType == MTLDataTypeTexture)
			{
				// TODO: Allow specifying this?
				argumentDescriptor.textureType = MTLTextureType2D;
			}

			[argumentDescriptors addObject:argumentDescriptor];
		};

		EnumFlags<ShaderStage> shaderStages;
		for (const Binding& __restrict binding : bindings)
		{
			shaderStages |= binding.m_shaderStages;
		}

		Internal::DescriptorSetLayoutData* pLayoutData = new Internal::DescriptorSetLayoutData{shaderStages};
		pLayoutData->m_bindings.Resize(bindings.GetSize(), Memory::Uninitialized);
		pLayoutData->m_resourceBatches.Reserve(bindings.GetSize());
		pLayoutData->m_argumentBuffers = [logicalDevice supportsFamily:MTLGPUFamilyApple6];

		const auto getResourceCount = [](const DescriptorType type) -> uint8
		{
			switch (type)
			{
				case DescriptorType::Sampler:
					return 0;
				case DescriptorType::SampledImage:
				case DescriptorType::InputAttachment:
				case DescriptorType::UniformTexelBuffer:
				case DescriptorType::UniformBuffer:
				case DescriptorType::UniformBufferDynamic:
				case DescriptorType::StorageBuffer:
				case DescriptorType::StorageBufferDynamic:
				case DescriptorType::CombinedImageSampler:
				case DescriptorType::StorageImage:
				case DescriptorType::StorageTexelBuffer:
				case DescriptorType::AccelerationStructure:
					return 1;
			}
		};

		for (const Binding& __restrict binding : bindings)
		{
			Internal::DescriptorSetLayoutData::Binding& layoutBinding = pLayoutData->m_bindings[(uint8)binding.m_index];
			layoutBinding.m_type = binding.m_type;
			layoutBinding.m_stages = binding.m_shaderStages;
			layoutBinding.m_arraySize = binding.m_count;

			const uint32 resourceCount = getResourceCount((DescriptorType)binding.m_type) * binding.m_count;
			if (resourceCount > 0)
			{
				Internal::DescriptorSetLayoutData::ResourceBatch requestedResourceBatch = {
					[](const DescriptorType descriptorType) -> MTLResourceUsage
					{
						switch (descriptorType)
						{
							case DescriptorType::SampledImage:
							case DescriptorType::InputAttachment:
							case DescriptorType::UniformTexelBuffer:
							case DescriptorType::UniformBuffer:
							case DescriptorType::UniformBufferDynamic:
							case DescriptorType::CombinedImageSampler:
							case DescriptorType::AccelerationStructure:
								return MTLResourceUsageRead;
							case DescriptorType::StorageBuffer:
							case DescriptorType::StorageBufferDynamic:
							case DescriptorType::StorageImage:
							case DescriptorType::StorageTexelBuffer:
								return MTLResourceUsageRead | MTLResourceUsageWrite;
							case DescriptorType::Sampler:
								ExpectUnreachable();
						}
					}((DescriptorType)binding.m_type),
					ConvertShaderStages(binding.m_shaderStages),
					1
				};
				const OptionalIterator<Internal::DescriptorSetLayoutData::ResourceBatch> foundResourceBatch =
					pLayoutData->m_resourceBatches.Find(requestedResourceBatch);

				if (foundResourceBatch.IsValid())
				{
					layoutBinding.m_batchIndex = pLayoutData->m_resourceBatches.GetIteratorIndex(foundResourceBatch);
					layoutBinding.m_baseBatchResourceIndex = foundResourceBatch->m_resourceCount;
					foundResourceBatch->m_resourceCount += resourceCount;
				}
				else
				{
					layoutBinding.m_batchIndex = pLayoutData->m_resourceBatches.GetNextAvailableIndex();
					layoutBinding.m_baseBatchResourceIndex = 0;
					requestedResourceBatch.m_resourceCount = resourceCount;
					pLayoutData->m_resourceBatches.EmplaceBack(requestedResourceBatch);
				}
			}
		}

		uint16 bindingIndex = 0;
		for (const Binding& __restrict binding : bindings)
		{
			pLayoutData->m_bindings[(uint8)binding.m_index].m_baseIndex = bindingIndex;

			switch ((DescriptorType)binding.m_type)
			{
				case DescriptorType::Sampler:
					addArgumentDescriptor(bindingIndex, MTLDataTypeSampler, MetalBindingAccessReadOnly, binding.m_count);
					bindingIndex += binding.m_count;
					break;
				case DescriptorType::SampledImage:
				case DescriptorType::InputAttachment:
				case DescriptorType::UniformTexelBuffer:
					addArgumentDescriptor(bindingIndex, MTLDataTypeTexture, MetalBindingAccessReadOnly, binding.m_count);
					bindingIndex += binding.m_count;
					break;
				case DescriptorType::UniformBuffer:
				case DescriptorType::UniformBufferDynamic:
					addArgumentDescriptor(bindingIndex, MTLDataTypePointer, MetalBindingAccessReadOnly, binding.m_count);
					bindingIndex += binding.m_count;
					break;
				case DescriptorType::StorageBuffer:
				case DescriptorType::StorageBufferDynamic:
					addArgumentDescriptor(bindingIndex, MTLDataTypePointer, MetalBindingAccessReadWrite, binding.m_count);
					bindingIndex += binding.m_count;
					break;
				case DescriptorType::CombinedImageSampler:
					addArgumentDescriptor(bindingIndex, MTLDataTypeTexture, MetalBindingAccessReadOnly, binding.m_count);
					addArgumentDescriptor(bindingIndex, MTLDataTypeSampler, MetalBindingAccessReadOnly, binding.m_count);
					bindingIndex += binding.m_count * 2;
					break;
				case DescriptorType::StorageImage:
				case DescriptorType::StorageTexelBuffer:
					addArgumentDescriptor(bindingIndex, MTLDataTypeTexture, MetalBindingAccessReadWrite, binding.m_count);
					bindingIndex += binding.m_count;
					break;
				case DescriptorType::AccelerationStructure:
					addArgumentDescriptor(bindingIndex, MTLDataTypeInstanceAccelerationStructure, MetalBindingAccessReadOnly, binding.m_count);
					bindingIndex += binding.m_count;
					break;
			}
		}

		id<MTLDevice> device{logicalDevice};
		pLayoutData->m_encoder = [device newArgumentEncoderWithArguments:argumentDescriptors];

		m_pLayout = pLayoutData;
#elif RENDERER_WEBGPU
		UNUSED(flags);

		const uint8 bindingCounts = bindings.Count(
			[](const Binding& __restrict binding)
			{
				return GetBindingCount((DescriptorType)binding.m_type);
			}
		);

		FixedCapacityInlineVector<WGPUBindGroupLayoutEntry, 8, DescriptorSetView::BindingIndexType> bindingLayoutEntries(
			Memory::Reserve,
			bindingCounts
		);
		InlineVector<Internal::DescriptorSetLayoutData::Binding, 6, uint16> computedBindings(Memory::Reserve, bindings.GetSize());

		for (const Binding& __restrict binding : bindings)
		{
			const uint8 bindingIndex = bindings.GetIteratorIndex(&binding);
			const EnumFlags<Binding::Flags> bindingFlags = bindingsFlags.HasElements() ? bindingsFlags[bindingIndex]
			                                                                           : EnumFlags<Binding::Flags>{};

			const Internal::DescriptorSetLayoutData::Binding& __restrict computedBinding = computedBindings.EmplaceBack(
				Internal::DescriptorSetLayoutData::Binding{bindingLayoutEntries.GetNextAvailableIndex(), binding.m_type}
			);

			switch (binding.m_type)
			{
				case DescriptorType::Sampler:
				{
					bindingLayoutEntries.EmplaceBack(WGPUBindGroupLayoutEntry{
						nullptr,
						computedBinding.m_baseIndex,
						ConvertShaderStages(binding.m_shaderStages),
						WGPUBufferBindingLayout{nullptr, WGPUBufferBindingType_BindingNotUsed},
						WGPUSamplerBindingLayout{nullptr, static_cast<WGPUSamplerBindingType>(binding.m_samplerBindingType)},
						WGPUTextureBindingLayout{nullptr, WGPUTextureSampleType_BindingNotUsed},
						WGPUStorageTextureBindingLayout{nullptr, WGPUStorageTextureAccess_BindingNotUsed}
					});
				}
				break;
				case DescriptorType::CombinedImageSampler:
				{
					// First specify the image binding
					const bool isMultisampled{false};
					bindingLayoutEntries.EmplaceBack(WGPUBindGroupLayoutEntry{
						nullptr,
						computedBinding.m_baseIndex,
						ConvertShaderStages(binding.m_shaderStages),
						WGPUBufferBindingLayout{nullptr, WGPUBufferBindingType_BindingNotUsed},
						WGPUSamplerBindingLayout{nullptr, WGPUSamplerBindingType_BindingNotUsed},
						// TODO: Specify sample type and dimension
						WGPUTextureBindingLayout{nullptr, WGPUTextureSampleType_Float, WGPUTextureViewDimension_2D, isMultisampled},
						WGPUStorageTextureBindingLayout{nullptr, WGPUStorageTextureAccess_BindingNotUsed}
					});

					// Now specify the sampler binding
					bindingLayoutEntries.EmplaceBack(WGPUBindGroupLayoutEntry{
						nullptr,
						computedBinding.m_baseIndex + 1u,
						ConvertShaderStages(binding.m_shaderStages),
						WGPUBufferBindingLayout{nullptr, WGPUBufferBindingType_BindingNotUsed},
						// TODO: Allow specifying WGPUSamplerBindingType_NonFiltering and WGPUSamplerBindingType_Comparison
						WGPUSamplerBindingLayout{nullptr, WGPUSamplerBindingType_Filtering},
						WGPUTextureBindingLayout{nullptr, WGPUTextureSampleType_BindingNotUsed},
						WGPUStorageTextureBindingLayout{nullptr, WGPUStorageTextureAccess_BindingNotUsed}
					});
				}
				break;
				case DescriptorType::SampledImage:
				case DescriptorType::UniformTexelBuffer:
				case DescriptorType::InputAttachment:
				{
					const bool isMultisampled{false};
					bindingLayoutEntries.EmplaceBack(WGPUBindGroupLayoutEntry{
						nullptr,
						computedBinding.m_baseIndex,
						ConvertShaderStages(binding.m_shaderStages),
						WGPUBufferBindingLayout{nullptr, WGPUBufferBindingType_BindingNotUsed},
						WGPUSamplerBindingLayout{nullptr, WGPUSamplerBindingType_BindingNotUsed},
						WGPUTextureBindingLayout{
							nullptr,
							static_cast<WGPUTextureSampleType>(binding.m_sampledImageType),
							ConvertImageMappingType(binding.m_imageMappingType),
							isMultisampled
						},
						WGPUStorageTextureBindingLayout{nullptr, WGPUStorageTextureAccess_BindingNotUsed}
					});
				}
				break;
				case DescriptorType::StorageImage:
				case DescriptorType::StorageTexelBuffer:
				{
					bindingLayoutEntries.EmplaceBack(WGPUBindGroupLayoutEntry{
						nullptr,
						computedBinding.m_baseIndex,
						ConvertShaderStages(binding.m_shaderStages),
						WGPUBufferBindingLayout{nullptr, WGPUBufferBindingType_BindingNotUsed},
						WGPUSamplerBindingLayout{nullptr, WGPUSamplerBindingType_BindingNotUsed},
						WGPUTextureBindingLayout{nullptr, WGPUTextureSampleType_BindingNotUsed},
						WGPUStorageTextureBindingLayout{
							nullptr,
							static_cast<WGPUStorageTextureAccess>(binding.m_storageTextureAccess),
							ConvertFormat(binding.m_format),
							WGPUTextureViewDimension_2D
						}
					});
				}
				break;
				case DescriptorType::UniformBuffer:
				case DescriptorType::StorageBuffer:
				case DescriptorType::UniformBufferDynamic:
				case DescriptorType::StorageBufferDynamic:
				{
					const WGPUBufferBindingType bufferBindingType =
						[](const DescriptorType type, const EnumFlags<Binding::Flags> bindingFlags) -> WGPUBufferBindingType
					{
						switch (type)
						{
							case DescriptorType::UniformBuffer:
							case DescriptorType::UniformBufferDynamic:
								return WGPUBufferBindingType_Uniform;
							case DescriptorType::StorageBuffer:
							case DescriptorType::StorageBufferDynamic:
								return bindingFlags.IsSet(Binding::Flags::ShaderWrite) ? WGPUBufferBindingType_Storage
								                                                       : WGPUBufferBindingType_ReadOnlyStorage;
							default:
								return WGPUBufferBindingType_BindingNotUsed;
						}
					}(binding.m_type, bindingFlags);
					const bool isDynamicBuffer = [](const DescriptorType type) -> bool
					{
						switch (type)
						{
							case DescriptorType::UniformBuffer:
							case DescriptorType::StorageBuffer:
								return false;
							case DescriptorType::UniformBufferDynamic:
							case DescriptorType::StorageBufferDynamic:
								return true;
							default:
								return false;
						}
					}(binding.m_type);

					bindingLayoutEntries.EmplaceBack(WGPUBindGroupLayoutEntry{
						nullptr,
						computedBinding.m_baseIndex,
						ConvertShaderStages(binding.m_shaderStages),
						WGPUBufferBindingLayout{nullptr, bufferBindingType, isDynamicBuffer, 0},
						WGPUSamplerBindingLayout{nullptr, WGPUSamplerBindingType_BindingNotUsed},
						WGPUTextureBindingLayout{nullptr, WGPUTextureSampleType_BindingNotUsed},
						WGPUStorageTextureBindingLayout{nullptr, WGPUStorageTextureAccess_BindingNotUsed}
					});
				}
				break;
				case DescriptorType::AccelerationStructure:
				{
					Assert(false, "WebGPU does not support acceleration structures!");
				}
				break;
			}
		}

		Internal::DescriptorSetLayoutData* pLayoutData =
			new Internal::DescriptorSetLayoutData{nullptr, bindingLayoutEntries.GetSize(), Move(computedBindings)};
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[logicalDevice, bindingLayoutEntries = Move(bindingLayoutEntries), pLayoutData]()
			{
				const WGPUBindGroupLayoutDescriptor descriptor
				{
					nullptr,
#if RENDERER_WEBGPU_DAWN
						WGPUStringView{nullptr, 0},
#else
						nullptr,
#endif
						bindingLayoutEntries.GetSize(), bindingLayoutEntries.GetData()
				};
				WGPUBindGroupLayout pLayout = wgpuDeviceCreateBindGroupLayout(logicalDevice, &descriptor);
				if (pLayout != nullptr)
				{
#if RENDERER_WEBGPU_DAWN
					wgpuBindGroupLayoutAddRef(pLayout);
#else
					wgpuBindGroupLayoutReference(pLayout);
#endif
					pLayoutData->m_bindGroupLayout = pLayout;
				}
			}
		);
#else
		const WGPUBindGroupLayoutDescriptor descriptor{nullptr, nullptr, bindingLayoutEntries.GetSize(), bindingLayoutEntries.GetData()};
		WGPUBindGroupLayout pLayout = wgpuDeviceCreateBindGroupLayout(logicalDevice, &descriptor);
		if (pLayout != nullptr)
		{
#if RENDERER_WEBGPU_DAWN
			wgpuBindGroupLayoutAddRef(pLayout);
#else
			wgpuBindGroupLayoutReference(pLayout);
#endif
			pLayoutData->m_bindGroupLayout = pLayout;
		}
#endif
		m_pLayout = pLayoutData;

#else
		UNUSED(logicalDevice);
		UNUSED(bindings);
#endif
	}

	DescriptorSetLayout& DescriptorSetLayout::operator=([[maybe_unused]] DescriptorSetLayout&& other)
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		m_pLayout = other.m_pLayout;
		other.m_pLayout = 0;
#endif
		return *this;
	}

	DescriptorSetLayout::~DescriptorSetLayout()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void DescriptorSetLayout::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroyDescriptorSetLayout(logicalDevice, m_pLayout, nullptr);
		m_pLayout = 0;
#elif RENDERER_METAL
		delete m_pLayout;
		m_pLayout = nullptr;
#elif RENDERER_WEBGPU
		if (m_pLayout != nullptr)
		{
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pLayout = m_pLayout]()
				{
					wgpuBindGroupLayoutRelease(pLayout->m_bindGroupLayout);
					delete pLayout;
				}
			);
			m_pLayout = nullptr;
		}
#endif
	}

	void DescriptorSetLayoutView::SetDebugName([[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name)
	{
#if RENDERER_VULKAN
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_DESCRIPTOR_SET_LAYOUT,
			reinterpret_cast<uint64_t>(m_pLayout),
			name
		};

#if PLATFORM_APPLE
		vkSetDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
#elif ENABLE_VULKAN_DEVICE_DEBUG_UTILS
		const PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT =
			reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(logicalDevice.GetSetDebugUtilsObjectNameEXT());
		if (setDebugUtilsObjectNameEXT != nullptr)
		{
			setDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
		}
#endif

#elif RENDERER_METAL
		[m_pLayout->m_encoder setLabel:[NSString stringWithUTF8String:name]];
#elif RENDERER_WEBGPU
		if (m_pLayout != nullptr)
		{
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pLayout = m_pLayout, name]()
				{
#if RENDERER_WEBGPU_DAWN
					wgpuBindGroupLayoutSetLabel(pLayout->m_bindGroupLayout, WGPUStringView{name, name.GetSize()});
#else
					wgpuBindGroupLayoutSetLabel(pLayout->m_bindGroupLayout, name);
#endif
				}
			);
		}
#endif
	}

	// DescriptorSet
	DescriptorSet& DescriptorSet::operator=([[maybe_unused]] DescriptorSet&& other)
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		m_pDescriptorSet = other.m_pDescriptorSet;
		other.m_pDescriptorSet = 0;
#endif
		return *this;
	}

	DescriptorSet::~DescriptorSet()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void DescriptorSet::Destroy(LogicalDevice& logicalDevice, DescriptorPoolView pool)
	{
		pool.FreeDescriptorSets(logicalDevice, ArrayView<DescriptorSet, uint8>(this));
	}

	void DescriptorSet::AtomicSwap([[maybe_unused]] DescriptorSet& other)
	{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		other.m_pDescriptorSet = Threading::Atomics::Exchange(m_pDescriptorSet, other.m_pDescriptorSet);
#endif
	}

	DescriptorSetView DescriptorSet::AtomicLoad() const
	{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		return Threading::Atomics::Load(m_pDescriptorSet);
#else
		return {};
#endif
	}

#if RENDERER_WEBGPU

#endif

	/* static */ bool DescriptorSetView::ValidateUpdate(const ArrayView<const UpdateInfo, uint8> descriptorUpdates)
	{
		return descriptorUpdates.All(
			[](const UpdateInfo& updateInfo)
			{
				switch ((DescriptorType)updateInfo.m_type)
				{
					case DescriptorType::Sampler:
					{
						if (updateInfo.m_pImageInfo.IsInvalid())
						{
							LogError("Descriptor update of type sampler at binding {} did not specify image info", updateInfo.m_bindingIndex);
							return false;
						}

						if (!updateInfo.m_pImageInfo->m_sampler.IsValid())
						{
							LogError("Descriptor update of type sampler at binding {} did not specify image sampler", updateInfo.m_bindingIndex);
							return false;
						}
					}
					break;
					case DescriptorType::SampledImage:
					case DescriptorType::UniformTexelBuffer:
					case DescriptorType::StorageImage:
					case DescriptorType::StorageTexelBuffer:
					{
						if (updateInfo.m_pImageInfo.IsInvalid())
						{
							LogError(
								"Descriptor update of type {} at binding {} did not specify image info",
								(uint32)updateInfo.m_type,
								updateInfo.m_bindingIndex
							);
							return false;
						}

						if (updateInfo.m_pImageInfo->m_imageLayout == ImageLayout::Undefined || updateInfo.m_pImageInfo->m_imageLayout == ImageLayout::Preinitialized)
						{
							LogError(
								"Descriptor update of type {} at binding {} specified an invalid image layout",
								(uint32)updateInfo.m_type,
								updateInfo.m_bindingIndex
							);
							return false;
						}

						if (!updateInfo.m_pImageInfo->m_imageView.IsValid())
						{
							LogError(
								"Descriptor update of type {} at binding {} did not specify image view",
								(uint32)updateInfo.m_type,
								updateInfo.m_bindingIndex
							);
							return false;
						}
					}
					break;
					case DescriptorType::CombinedImageSampler:
					{
						if (updateInfo.m_pImageInfo.IsInvalid())
						{
							LogError(
								"Descriptor update of type combined image sampler at binding {} did not specify image info",
								updateInfo.m_bindingIndex
							);
							return false;
						}

						if (updateInfo.m_pImageInfo->m_imageLayout == ImageLayout::Undefined || updateInfo.m_pImageInfo->m_imageLayout == ImageLayout::Preinitialized)
						{
							LogError(
								"Descriptor update of type combined image sampler at binding {} specified an invalid image layout",
								updateInfo.m_bindingIndex
							);
							return false;
						}

						if (!updateInfo.m_pImageInfo->m_imageView.IsValid())
						{
							LogError(
								"Descriptor update of type combined image sampler at binding {} did not specify image view",
								updateInfo.m_bindingIndex
							);
							return false;
						}

						if (!updateInfo.m_pImageInfo->m_sampler.IsValid())
						{
							LogError(
								"Descriptor update of type combined image sampler at binding {} did not specify image sampler",
								updateInfo.m_bindingIndex
							);
							return false;
						}
					}
					break;
					case DescriptorType::InputAttachment:
					{
						if (updateInfo.m_pImageInfo.IsInvalid())
						{
							LogError("Descriptor update of type input attachment at binding {} did not specify image info", updateInfo.m_bindingIndex);
							return false;
						}

						if (updateInfo.m_pImageInfo->m_imageLayout != ImageLayout::ShaderReadOnlyOptimal && updateInfo.m_pImageInfo->m_imageLayout != ImageLayout::DepthStencilReadOnlyOptimal)
						{
							LogError(
								"Descriptor update of type input attachment at binding {} specified an invalid image layout",
								updateInfo.m_bindingIndex
							);
							return false;
						}

						if (!updateInfo.m_pImageInfo->m_imageView.IsValid())
						{
							LogError("Descriptor update of type input attachment at binding {} specified an image view", updateInfo.m_bindingIndex);
							return false;
						}
					}
					break;
					case DescriptorType::UniformBuffer:
					case DescriptorType::UniformBufferDynamic:
					case DescriptorType::StorageBuffer:
					case DescriptorType::StorageBufferDynamic:
					{
						if (updateInfo.m_pBufferInfo.IsInvalid())
						{
							LogError("Descriptor update of type buffer at binding {} specified invalid buffer info", updateInfo.m_bindingIndex);
							return false;
						}

						if (!updateInfo.m_pBufferInfo->m_buffer.IsValid())
						{
							LogError("Descriptor update of type buffer at binding {} specified invalid buffer", updateInfo.m_bindingIndex);
							return false;
						}
					}
					break;
					case DescriptorType::AccelerationStructure:
					{
						// TODO
					}
					break;
				}

				if (!updateInfo.m_set.IsValid())
				{
					LogError("Descriptor update at binding {} specified invalid set", updateInfo.m_bindingIndex);
					return false;
				}

				return true;
			}
		);
	}

	/* static */ void DescriptorSetView::UpdateAndCopy(
		const LogicalDeviceView logicalDevice,
		const ArrayView<const UpdateInfo, uint8> descriptorUpdates,
		const ArrayView<const CopyInfo, uint8> descriptorCopies
	)
	{
		Assert(descriptorUpdates.HasElements());
		Assert(descriptorCopies.HasElements());
		Assert(ValidateUpdate(descriptorUpdates));

#if RENDERER_VULKAN
		static_assert(sizeof(VkWriteDescriptorSet) == sizeof(UpdateInfo));
		static_assert(alignof(VkWriteDescriptorSet) == alignof(UpdateInfo));
		static_assert(sizeof(VkCopyDescriptorSet) == sizeof(CopyInfo));
		static_assert(alignof(VkCopyDescriptorSet) == alignof(CopyInfo));

		vkUpdateDescriptorSets(
			logicalDevice,
			descriptorUpdates.GetSize(),
			reinterpret_cast<const VkWriteDescriptorSet*>(descriptorUpdates.GetData()),
			descriptorCopies.GetSize(),
			reinterpret_cast<const VkCopyDescriptorSet*>(descriptorCopies.GetData())
		);
#else
		Update(logicalDevice, descriptorUpdates);
		Copy(logicalDevice, descriptorCopies);
#endif
	}

	/* static */ void
	DescriptorSetView::Update(const LogicalDeviceView logicalDevice, const ArrayView<const UpdateInfo, uint8> descriptorUpdates)
	{
		Assert(descriptorUpdates.HasElements());
		Assert(ValidateUpdate(descriptorUpdates));

#if RENDERER_VULKAN
		static_assert(sizeof(VkWriteDescriptorSet) == sizeof(UpdateInfo));
		static_assert(alignof(VkWriteDescriptorSet) == alignof(UpdateInfo));

		vkUpdateDescriptorSets(
			logicalDevice,
			descriptorUpdates.GetSize(),
			reinterpret_cast<const VkWriteDescriptorSet*>(descriptorUpdates.GetData()),
			0,
			nullptr
		);
#elif RENDERER_METAL
		UNUSED(logicalDevice);

		for (const UpdateInfo& __restrict updateInfo : descriptorUpdates)
		{
			Internal::DescriptorSetData* __restrict pSetData = updateInfo.m_set;
			Assert(pSetData != nullptr, "Can't update invalid descriptor set");
			if (UNLIKELY_ERROR(pSetData == nullptr))
			{
				continue;
			}

			Internal::DescriptorSetLayoutData* __restrict pSetLayoutData = pSetData->m_layout;

			[pSetLayoutData->m_encoder setArgumentBuffer:pSetData->m_argumentBuffer offset:0];

			const Internal::DescriptorSetLayoutData::Binding binding = pSetLayoutData->m_bindings[(uint8)updateInfo.m_bindingIndex];

			Assert(pSetData->m_resourceBatches.IsValidIndex(binding.m_batchIndex), "Can't update invalid descriptor set batch");
			if (UNLIKELY_ERROR(!pSetData->m_resourceBatches.IsValidIndex(binding.m_batchIndex)))
			{
				continue;
			}
			Internal::DescriptorSetData::ResourceBatch& resourceBatch = pSetData->m_resourceBatches[binding.m_batchIndex];

			switch (DescriptorType(updateInfo.m_type))
			{
				case DescriptorType::Sampler:
				{
					id<MTLSamplerState> sampler = updateInfo.m_pImageInfo->m_sampler;
					Assert(sampler != nil);
					for (uint32 index = 0, end = updateInfo.m_count; index < end; ++index)
					{
						const uint32 baseIndex = index + binding.m_baseIndex + updateInfo.m_arrayIndex;
						[pSetLayoutData->m_encoder setSamplerState:sampler atIndex:baseIndex];
						pSetData->m_objects[baseIndex] = sampler;
					}
				}
				break;
				case DescriptorType::SampledImage:
				case DescriptorType::InputAttachment:
				case DescriptorType::UniformTexelBuffer:
				case DescriptorType::StorageImage:
				case DescriptorType::StorageTexelBuffer:
				{
					id<MTLTexture> texture = updateInfo.m_pImageInfo->m_imageView;
					Assert(texture != nil);
					for (uint32 index = 0, end = updateInfo.m_count; index < end; ++index)
					{
						const uint32 baseIndex = index + binding.m_baseIndex + updateInfo.m_arrayIndex;
						pSetData->m_objects[baseIndex] = texture;
						[pSetLayoutData->m_encoder setTexture:texture atIndex:baseIndex];

						const uint32 resourceIndex = binding.m_baseBatchResourceIndex + updateInfo.m_arrayIndex + index;
						resourceBatch.EmplaceResource(resourceIndex, texture);
					}
				}
				break;
				case DescriptorType::UniformBuffer:
				case DescriptorType::UniformBufferDynamic:
				case DescriptorType::StorageBuffer:
				case DescriptorType::StorageBufferDynamic:
				{
					id<MTLBuffer> buffer = updateInfo.m_pBufferInfo->m_buffer;
					const size bufferOffset = updateInfo.m_pBufferInfo->m_offset;
					for (uint32 index = 0, end = updateInfo.m_count; index < end; ++index)
					{
						const uint32 baseIndex = index + binding.m_baseIndex + updateInfo.m_arrayIndex;
						[pSetLayoutData->m_encoder setBuffer:buffer offset:bufferOffset atIndex:baseIndex];
						pSetData->m_objects[baseIndex] = buffer;
						pSetData->m_bufferOffsets[baseIndex] = bufferOffset;

						const uint32 resourceIndex = binding.m_baseBatchResourceIndex + updateInfo.m_arrayIndex + index;
						resourceBatch.EmplaceResource(resourceIndex, buffer);
					}
				}
				break;
				case DescriptorType::AccelerationStructure:
				{
					id<MTLAccelerationStructure> accelerationStructure = updateInfo.m_accelerationStructure;
					for (uint32 index = 0, end = updateInfo.m_count; index < end; ++index)
					{
						const uint32 baseIndex = index + binding.m_baseIndex + updateInfo.m_arrayIndex;
						[pSetLayoutData->m_encoder setAccelerationStructure:accelerationStructure atIndex:baseIndex];
						pSetData->m_objects[baseIndex] = accelerationStructure;

						const uint32 resourceIndex = binding.m_baseBatchResourceIndex + updateInfo.m_arrayIndex + index;
						resourceBatch.EmplaceResource(resourceIndex, accelerationStructure);
					}
				}
				break;
				case DescriptorType::CombinedImageSampler:
				{
					Assert(updateInfo.m_arrayIndex == 0);
					id<MTLTexture> texture = updateInfo.m_pImageInfo->m_imageView;
					id<MTLSamplerState> sampler = updateInfo.m_pImageInfo->m_sampler;
					Assert(texture != nil);
					for (uint32 index = 0, end = updateInfo.m_count; index < end; ++index)
					{
						const uint32 baseIndex = index + binding.m_baseIndex + updateInfo.m_arrayIndex;
						[pSetLayoutData->m_encoder setTexture:texture atIndex:baseIndex];
						[pSetLayoutData->m_encoder setSamplerState:sampler atIndex:baseIndex + 1];
						pSetData->m_objects[baseIndex] = texture;
						pSetData->m_objects[baseIndex + 1] = sampler;

						const uint32 resourceIndex = binding.m_baseBatchResourceIndex + updateInfo.m_arrayIndex + index;
						resourceBatch.EmplaceResource(resourceIndex, texture);
					}
				}
				break;
			}
		}
#elif RENDERER_WEBGPU
		InlineVector<DescriptorSetView, 1> descriptorSets(Memory::Reserve, descriptorUpdates.GetSize());

		for (const UpdateInfo& __restrict updateInfo : descriptorUpdates)
		{
			descriptorSets.EmplaceBackUnique(DescriptorSetView(updateInfo.m_set));
		}

		static auto updateDescriptorSets = [](
																				 const LogicalDeviceView logicalDevice,
																				 const ArrayView<const UpdateInfo, uint8> descriptorUpdates,
																				 const ArrayView<const DescriptorSetView> descriptorSets
																			 )
		{
			for (const UpdateInfo& __restrict updateInfo : descriptorUpdates)
			{
				Assert(updateInfo.m_arrayIndex == 0, "TODO");

				Internal::DescriptorSetData* __restrict pSetData = updateInfo.m_set;
				const Internal::DescriptorSetLayoutData* __restrict pDescriptorSetLayoutData = pSetData->m_layout;
				const Internal::DescriptorSetLayoutData::Binding& __restrict layoutBinding =
					pDescriptorSetLayoutData->m_bindings[updateInfo.m_bindingIndex];

				Assert(updateInfo.m_count == 1);

				switch (DescriptorType(updateInfo.m_type))
				{
					case DescriptorType::Sampler:
					{
						Assert((WGPUSampler)updateInfo.m_pImageInfo->m_sampler != nullptr);
						pSetData->m_bindGroupEntries[layoutBinding.m_baseIndex] =
							WGPUBindGroupEntry{nullptr, layoutBinding.m_baseIndex, nullptr, 0, 0, updateInfo.m_pImageInfo->m_sampler, nullptr};
					}
					break;
					case DescriptorType::SampledImage:
					case DescriptorType::InputAttachment:
					case DescriptorType::UniformTexelBuffer:
					{
						Assert((WGPUTextureView)updateInfo.m_pImageInfo->m_imageView != nullptr);
						pSetData->m_bindGroupEntries[layoutBinding.m_baseIndex] =
							WGPUBindGroupEntry{nullptr, layoutBinding.m_baseIndex, nullptr, 0, 0, nullptr, updateInfo.m_pImageInfo->m_imageView};
					}
					break;
					case DescriptorType::UniformBuffer:
					case DescriptorType::UniformBufferDynamic:
					{
						Assert((WGPUBuffer)updateInfo.m_pBufferInfo->m_buffer != nullptr);
						pSetData->m_bindGroupEntries[layoutBinding.m_baseIndex] = WGPUBindGroupEntry{
							nullptr,
							layoutBinding.m_baseIndex,
							updateInfo.m_pBufferInfo->m_buffer,
							updateInfo.m_pBufferInfo->m_offset,
							updateInfo.m_pBufferInfo->m_range,
							nullptr,
							nullptr
						};
					}
					break;
					case DescriptorType::StorageBuffer:
					case DescriptorType::StorageBufferDynamic:
					{
						Assert((WGPUBuffer)updateInfo.m_pBufferInfo->m_buffer != nullptr);
						Assert(Memory::IsAligned(updateInfo.m_pBufferInfo->m_range, 4u));
						pSetData->m_bindGroupEntries[layoutBinding.m_baseIndex] = WGPUBindGroupEntry{
							nullptr,
							layoutBinding.m_baseIndex,
							updateInfo.m_pBufferInfo->m_buffer,
							updateInfo.m_pBufferInfo->m_offset,
							updateInfo.m_pBufferInfo->m_range,
							nullptr,
							nullptr
						};
					}
					break;
					case DescriptorType::AccelerationStructure:
					{
						NotSupported("Unsupported on WebGPU");
					}
					break;
					case DescriptorType::CombinedImageSampler:
					{
						Assert((WGPUSampler)updateInfo.m_pImageInfo->m_sampler != nullptr);
						Assert((WGPUTextureView)updateInfo.m_pImageInfo->m_imageView != nullptr);

						// First update the image
						pSetData->m_bindGroupEntries[layoutBinding.m_baseIndex] =
							WGPUBindGroupEntry{nullptr, layoutBinding.m_baseIndex, nullptr, 0, 0, nullptr, updateInfo.m_pImageInfo->m_imageView};
						// Now update the sampler
						pSetData->m_bindGroupEntries[layoutBinding.m_baseIndex + 1] =
							WGPUBindGroupEntry{nullptr, layoutBinding.m_baseIndex + 1u, nullptr, 0, 0, updateInfo.m_pImageInfo->m_sampler, nullptr};
					}
					break;
					case DescriptorType::StorageImage:
					case DescriptorType::StorageTexelBuffer:
					{
						Assert((WGPUTextureView)updateInfo.m_pImageInfo->m_imageView != nullptr);

						pSetData->m_bindGroupEntries[layoutBinding.m_baseIndex] =
							WGPUBindGroupEntry{nullptr, layoutBinding.m_baseIndex, nullptr, 0, 0, nullptr, updateInfo.m_pImageInfo->m_imageView};
					}
					break;
				}
			}

			for (const DescriptorSetView descriptorSet : descriptorSets)
			{
				Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
				Internal::DescriptorSetLayoutData* __restrict pDescriptorSetLayoutData = pDescriptorSetData->m_layout;

				const bool isValid = pDescriptorSetData->m_bindGroupEntries.GetView().All(
					[](const WGPUBindGroupEntry& __restrict bindGroupEntry)
					{
						return bindGroupEntry.buffer != nullptr || bindGroupEntry.sampler != nullptr || bindGroupEntry.textureView != nullptr;
					}
				);
				if (isValid)
				{
					Assert(pDescriptorSetLayoutData->m_bindGroupLayout != nullptr);
					const WGPUBindGroupDescriptor descriptor
					{
						nullptr,
#if RENDERER_WEBGPU_DAWN
							WGPUStringView{nullptr, 0},
#else
							nullptr,
#endif
							pDescriptorSetLayoutData->m_bindGroupLayout, pDescriptorSetData->m_bindGroupEntries.GetSize(),
							pDescriptorSetData->m_bindGroupEntries.GetData()
					};
					WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(logicalDevice, &descriptor);
#if RENDERER_WEBGPU_DAWN
					wgpuBindGroupAddRef(bindGroup);
#else
					wgpuBindGroupReference(bindGroup);
#endif
					if (pDescriptorSetData->m_bindGroup != nullptr)
					{
						wgpuBindGroupRelease(pDescriptorSetData->m_bindGroup);
					}
					pDescriptorSetData->m_bindGroup = bindGroup;
				}
			}
		};

#if WEBGPU_SINGLE_THREADED
		FixedCapacityInlineVector<UpdateInfo, 1> updateInfos(Memory::Reserve, descriptorUpdates.GetSize());

		FixedCapacityVector<ImageInfo> imageInfos(
			Memory::Reserve,
			descriptorUpdates.Count(
				[](const UpdateInfo& __restrict updateInfo) -> uint32
				{
					return updateInfo.m_pImageInfo.IsValid();
				}
			)
		);
		FixedCapacityVector<BufferInfo> bufferInfos(
			Memory::Reserve,
			descriptorUpdates.Count(
				[](const UpdateInfo& __restrict updateInfo) -> uint32
				{
					return updateInfo.m_pBufferInfo.IsValid();
				}
			)
		);

		for (const UpdateInfo& __restrict updateInfo : descriptorUpdates)
		{
			if (updateInfo.m_pImageInfo.IsValid())
			{
				updateInfos.EmplaceBack(UpdateInfo{
					updateInfo.m_set,
					updateInfo.m_bindingIndex,
					updateInfo.m_arrayIndex,
					updateInfo.m_type,
					ArrayView<const ImageInfo>{imageInfos.EmplaceBack(*updateInfo.m_pImageInfo)}
				});
			}
			else if (updateInfo.m_pBufferInfo.IsValid())
			{
				updateInfos.EmplaceBack(UpdateInfo{
					updateInfo.m_set,
					updateInfo.m_bindingIndex,
					updateInfo.m_arrayIndex,
					updateInfo.m_type,
					ArrayView<const BufferInfo>{bufferInfos.EmplaceBack(*updateInfo.m_pBufferInfo)}
				});
			}
			else
			{
				[[maybe_unused]] const UpdateInfo& emplacedUpdateInfo = updateInfos.EmplaceBack(updateInfo);
				Assert(emplacedUpdateInfo.m_pImageInfo.IsInvalid());
				Assert(emplacedUpdateInfo.m_pBufferInfo.IsInvalid());
				Assert(emplacedUpdateInfo.m_pTexelBufferView == nullptr);
			}
		}

		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[logicalDevice,
		   descriptorSets = Move(descriptorSets),
		   imageInfos = Move(imageInfos),
		   bufferInfos = Move(bufferInfos),
		   descriptorUpdates = Move(updateInfos)]()
			{
				updateDescriptorSets(logicalDevice, descriptorUpdates.GetView(), descriptorSets.GetView());
			}
		);
#else
		updateDescriptorSets(logicalDevice, descriptorUpdates, descriptorSets);
#endif
#else
		Assert(false);
		UNUSED(logicalDevice);
		UNUSED(descriptorUpdates);
#endif
	}

	void DescriptorSetView::BindIndirectAccelerationStructures(
		[[maybe_unused]] const BindingIndexType bindingIndex,
		[[maybe_unused]] const ArrayView<const PrimitiveAccelerationStructureView> primitiveAccelerationStructures
	) const
	{
#if RENDERER_SUPPORTS_RAYTRACING && RENDERER_METAL
		Internal::DescriptorSetData* __restrict pSetData = m_pDescriptorSet;
		Assert(pSetData != nullptr);
		if (UNLIKELY_ERROR(pSetData == nullptr))
		{
			return;
		}

		Internal::DescriptorSetLayoutData* __restrict pSetLayoutData = pSetData->m_layout;

		const Internal::DescriptorSetLayoutData::Binding binding = pSetLayoutData->m_bindings[bindingIndex];

		Internal::DescriptorSetData::ResourceBatch& resourceBatch = pSetData->m_resourceBatches[binding.m_batchIndex];

		for (const PrimitiveAccelerationStructureView primitiveAccelerationStructure : primitiveAccelerationStructures)
		{
			if (primitiveAccelerationStructure.IsValid())
			{
				resourceBatch.EmplaceIndirectResource(primitiveAccelerationStructure);
			}
		}
#endif
	}

	/* static */ void DescriptorSetView::Copy(const LogicalDeviceView logicalDevice, const ArrayView<const CopyInfo, uint8> descriptorCopies)
	{
		Assert(descriptorCopies.HasElements());

#if RENDERER_VULKAN
		static_assert(sizeof(VkCopyDescriptorSet) == sizeof(CopyInfo));
		static_assert(alignof(VkCopyDescriptorSet) == alignof(CopyInfo));

		vkUpdateDescriptorSets(
			logicalDevice,
			0,
			nullptr,
			descriptorCopies.GetSize(),
			reinterpret_cast<const VkCopyDescriptorSet*>(descriptorCopies.GetData())
		);
#elif RENDERER_METAL
		UNUSED(logicalDevice);

		for (const CopyInfo& copyInfo : descriptorCopies)
		{
			Assert(copyInfo.m_source.m_arrayIndex == 0, "TODO");
			Assert(copyInfo.m_target.m_arrayIndex == 0, "TODO");

			const Internal::DescriptorSetData* __restrict pSourceSetData = copyInfo.m_source.m_set;
			Internal::DescriptorSetData* __restrict pTargetSetData = copyInfo.m_target.m_set;

			Assert(pSourceSetData->m_layout == pTargetSetData->m_layout, "Descriptors are not compatible");

			const Internal::DescriptorSetLayoutData* __restrict pSetLayoutData = pSourceSetData->m_layout;

			[pSetLayoutData->m_encoder setArgumentBuffer:pTargetSetData->m_argumentBuffer offset:0];

			for (uint8 baseBindingIndex = 0; baseBindingIndex < (uint8)copyInfo.m_count; ++baseBindingIndex)
			{
				const Internal::DescriptorSetLayoutData::Binding sourceBinding =
					pSetLayoutData->m_bindings[(uint8)copyInfo.m_source.m_bindingIndex + baseBindingIndex];
				const Internal::DescriptorSetLayoutData::Binding targetBinding =
					pSetLayoutData->m_bindings[(uint8)copyInfo.m_target.m_bindingIndex + baseBindingIndex];

				Assert(pTargetSetData->m_resourceBatches.IsValidIndex(targetBinding.m_batchIndex));
				if (!pTargetSetData->m_resourceBatches.IsValidIndex(targetBinding.m_batchIndex))
				{
					continue;
				}

				Internal::DescriptorSetData::ResourceBatch& targetResourceBatch = pTargetSetData->m_resourceBatches[targetBinding.m_batchIndex];
				const Internal::DescriptorSetData::ResourceBatch& sourceResourceBatch =
					pSourceSetData->m_resourceBatches[sourceBinding.m_batchIndex];

				Assert(targetBinding.m_type == sourceBinding.m_type);

				pTargetSetData->m_objects[targetBinding.m_baseIndex] = pSourceSetData->m_objects[sourceBinding.m_baseIndex];
				pTargetSetData->m_bufferOffsets[targetBinding.m_baseIndex] = pSourceSetData->m_bufferOffsets[sourceBinding.m_baseIndex];

				switch (DescriptorType(targetBinding.m_type))
				{
					case DescriptorType::Sampler:
					{
						id<MTLSamplerState> sampler = (id<MTLSamplerState>)pSourceSetData->m_objects[sourceBinding.m_baseIndex];
						Assert(sampler != nil);
						[pSetLayoutData->m_encoder setSamplerState:sampler atIndex:targetBinding.m_baseIndex];
					}
					break;
					case DescriptorType::SampledImage:
					case DescriptorType::InputAttachment:
					case DescriptorType::UniformTexelBuffer:
					{
						id<MTLTexture> texture = (id<MTLTexture>)pSourceSetData->m_objects[sourceBinding.m_baseIndex];
						Assert(texture != nil);
						[pSetLayoutData->m_encoder setTexture:texture atIndex:targetBinding.m_baseIndex];
						targetResourceBatch
							.CopyResourceFrom(targetBinding.m_baseBatchResourceIndex, sourceBinding.m_baseBatchResourceIndex, sourceResourceBatch);
					}
					break;
					case DescriptorType::UniformBuffer:
					case DescriptorType::UniformBufferDynamic:
					case DescriptorType::StorageBuffer:
					case DescriptorType::StorageBufferDynamic:
					{
						id<MTLBuffer> buffer = (id<MTLBuffer>)pSourceSetData->m_objects[sourceBinding.m_baseIndex];
						Assert(buffer != nil);
						[pSetLayoutData->m_encoder setBuffer:buffer
																					offset:pSourceSetData->m_bufferOffsets[sourceBinding.m_baseIndex]
																				 atIndex:targetBinding.m_baseIndex];
						targetResourceBatch
							.CopyResourceFrom(targetBinding.m_baseBatchResourceIndex, sourceBinding.m_baseBatchResourceIndex, sourceResourceBatch);
					}
					break;
					case DescriptorType::AccelerationStructure:
					{
						id<MTLAccelerationStructure> accelerationStructure = (id<MTLAccelerationStructure>)
						                                                       pSourceSetData->m_objects[sourceBinding.m_baseIndex];
						Assert(accelerationStructure != nil);
						[pSetLayoutData->m_encoder setAccelerationStructure:accelerationStructure atIndex:targetBinding.m_baseIndex];
						targetResourceBatch
							.CopyResourceFrom(targetBinding.m_baseBatchResourceIndex, sourceBinding.m_baseBatchResourceIndex, sourceResourceBatch);
					}
					break;
					case DescriptorType::CombinedImageSampler:
					{
						id<MTLTexture> texture = (id<MTLTexture>)pSourceSetData->m_objects[sourceBinding.m_baseIndex];
						Assert(texture != nil);
						id<MTLSamplerState> sampler = (id<MTLSamplerState>)pSourceSetData->m_objects[sourceBinding.m_baseIndex + 1];
						Assert(sampler != nil);
						[pSetLayoutData->m_encoder setTexture:texture atIndex:targetBinding.m_baseIndex];
						[pSetLayoutData->m_encoder setSamplerState:sampler atIndex:targetBinding.m_baseIndex + 1];
						targetResourceBatch
							.CopyResourceFrom(targetBinding.m_baseBatchResourceIndex, sourceBinding.m_baseBatchResourceIndex, sourceResourceBatch);

						pTargetSetData->m_objects[targetBinding.m_baseIndex + 1] = pSourceSetData->m_objects[sourceBinding.m_baseIndex + 1];
						pTargetSetData->m_bufferOffsets[targetBinding.m_baseIndex + 1] = pSourceSetData->m_bufferOffsets[sourceBinding.m_baseIndex + 1];
					}
					break;
					case DescriptorType::StorageImage:
					case DescriptorType::StorageTexelBuffer:
					{
						id<MTLTexture> texture = (id<MTLTexture>)pSourceSetData->m_objects[sourceBinding.m_baseIndex];
						Assert(texture != nil);
						[pSetLayoutData->m_encoder setTexture:texture atIndex:targetBinding.m_baseIndex];
						targetResourceBatch
							.CopyResourceFrom(targetBinding.m_baseBatchResourceIndex, sourceBinding.m_baseBatchResourceIndex, sourceResourceBatch);
					}
					break;
				}
			}
		}
#elif RENDERER_WEBGPU
		FixedCapacityInlineVector<DescriptorSetView, 1> descriptorSets(Memory::Reserve, descriptorCopies.GetSize());
		for (const CopyInfo& copyInfo : descriptorCopies)
		{
			descriptorSets.EmplaceBackUnique(DescriptorSetView(copyInfo.m_target.m_set));
		}

		static auto copyDescriptorSets = [](
																			 const LogicalDeviceView logicalDevice,
																			 const ArrayView<const CopyInfo, uint8> descriptorCopies,
																			 const ArrayView<const DescriptorSetView> descriptorSets
																		 )
		{
			for (const CopyInfo& copyInfo : descriptorCopies)
			{
				Assert(copyInfo.m_source.m_arrayIndex == 0, "TODO");
				Assert(copyInfo.m_target.m_arrayIndex == 0, "TODO");

				const Internal::DescriptorSetData* __restrict pSourceSetData = copyInfo.m_source.m_set;
				Internal::DescriptorSetData* __restrict pTargetSetData = copyInfo.m_target.m_set;

				Assert(pSourceSetData->m_layout == pTargetSetData->m_layout, "Descriptors are not compatible");

				const Internal::DescriptorSetLayoutData* __restrict pDescriptorSetLayoutData = pTargetSetData->m_layout;

				for (BindingIndexType baseBindingIndex = 0; baseBindingIndex < copyInfo.m_count; ++baseBindingIndex)
				{
					const BindingIndexType sourceBindingIndex = baseBindingIndex + copyInfo.m_source.m_bindingIndex;
					const BindingIndexType targetBindingIndex = baseBindingIndex + copyInfo.m_target.m_bindingIndex;

					const Internal::DescriptorSetLayoutData::Binding& __restrict layoutSourceBinding =
						pDescriptorSetLayoutData->m_bindings[sourceBindingIndex];
					const Internal::DescriptorSetLayoutData::Binding& __restrict layoutTargetBinding =
						pDescriptorSetLayoutData->m_bindings[targetBindingIndex];

					Assert(layoutSourceBinding.m_type == layoutTargetBinding.m_type, "Descriptor bindings are not compatible");
					switch (layoutSourceBinding.m_type)
					{
						case DescriptorType::Sampler:
						case DescriptorType::SampledImage:
						case DescriptorType::InputAttachment:
						case DescriptorType::UniformTexelBuffer:
						case DescriptorType::UniformBuffer:
						case DescriptorType::UniformBufferDynamic:
						case DescriptorType::StorageBuffer:
						case DescriptorType::StorageBufferDynamic:
						case DescriptorType::AccelerationStructure:
						case DescriptorType::StorageImage:
						case DescriptorType::StorageTexelBuffer:
						{
							const WGPUBindGroupEntry& __restrict sourceBindGroupEntry =
								pSourceSetData->m_bindGroupEntries[layoutSourceBinding.m_baseIndex];
							pTargetSetData->m_bindGroupEntries[layoutTargetBinding.m_baseIndex] = WGPUBindGroupEntry{
								nullptr,
								layoutTargetBinding.m_baseIndex,
								sourceBindGroupEntry.buffer,
								sourceBindGroupEntry.offset,
								sourceBindGroupEntry.size,
								sourceBindGroupEntry.sampler,
								sourceBindGroupEntry.textureView
							};
						}
						break;
						case DescriptorType::CombinedImageSampler:
						{
							// First copy the image
							{
								const WGPUBindGroupEntry& __restrict sourceBindGroupEntry =
									pSourceSetData->m_bindGroupEntries[layoutSourceBinding.m_baseIndex];
								WGPUBindGroupEntry& __restrict targetBindGroupEntry = pTargetSetData->m_bindGroupEntries[layoutTargetBinding.m_baseIndex];
								targetBindGroupEntry = WGPUBindGroupEntry{
									nullptr,
									layoutTargetBinding.m_baseIndex,
									sourceBindGroupEntry.buffer,
									sourceBindGroupEntry.offset,
									sourceBindGroupEntry.size,
									sourceBindGroupEntry.sampler,
									sourceBindGroupEntry.textureView
								};
							}
							// Now copy the sampler
							{
								const WGPUBindGroupEntry& __restrict sourceBindGroupEntry =
									pSourceSetData->m_bindGroupEntries[layoutSourceBinding.m_baseIndex + 1u];
								WGPUBindGroupEntry& __restrict targetBindGroupEntry =
									pTargetSetData->m_bindGroupEntries[layoutTargetBinding.m_baseIndex + 1u];
								targetBindGroupEntry = WGPUBindGroupEntry{
									nullptr,
									layoutTargetBinding.m_baseIndex + 1u,
									sourceBindGroupEntry.buffer,
									sourceBindGroupEntry.offset,
									sourceBindGroupEntry.size,
									sourceBindGroupEntry.sampler,
									sourceBindGroupEntry.textureView
								};
							}
						}
						break;
					}
				}
			}

			for (const DescriptorSetView descriptorSet : descriptorSets)
			{
				Internal::DescriptorSetData* __restrict pDescriptorSetData = descriptorSet;
				Internal::DescriptorSetLayoutData* __restrict pDescriptorSetLayoutData = pDescriptorSetData->m_layout;

				const bool isValid = pDescriptorSetData->m_bindGroupEntries.GetView().All(
					[](const WGPUBindGroupEntry& __restrict bindGroupEntry)
					{
						return bindGroupEntry.buffer != nullptr || bindGroupEntry.sampler != nullptr || bindGroupEntry.textureView != nullptr;
					}
				);
				if (isValid)
				{
					const WGPUBindGroupDescriptor descriptor
					{
						nullptr,
#if RENDERER_WEBGPU_DAWN
							WGPUStringView{nullptr, 0},
#else
							nullptr,
#endif
							pDescriptorSetLayoutData->m_bindGroupLayout, pDescriptorSetData->m_bindGroupEntries.GetSize(),
							pDescriptorSetData->m_bindGroupEntries.GetData()
					};
					WGPUBindGroup bindGroup = wgpuDeviceCreateBindGroup(logicalDevice, &descriptor);
#if RENDERER_WEBGPU_DAWN
					wgpuBindGroupAddRef(bindGroup);
#else
					wgpuBindGroupReference(bindGroup);
#endif
					if (pDescriptorSetData->m_bindGroup != nullptr)
					{
						wgpuBindGroupRelease(pDescriptorSetData->m_bindGroup);
					}
					pDescriptorSetData->m_bindGroup = bindGroup;
				}
			}
		};

#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[logicalDevice, descriptorCopies = InlineVector<CopyInfo, 1>{descriptorCopies}, descriptorSets = Move(descriptorSets)]()
			{
				copyDescriptorSets(logicalDevice, descriptorCopies.GetView(), descriptorSets.GetView());
			}
		);
#else
		copyDescriptorSets(logicalDevice, descriptorCopies, descriptorSets);
#endif
#else
		Assert(false);
		UNUSED(logicalDevice);
		UNUSED(descriptorCopies);
#endif
	}
}
