#include "Wrappers/PipelineLayout.h"

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/WebGPU/Includes.h>

#include <Renderer/Devices/LogicalDevice.h>

#include <Renderer/Pipelines/PushConstantRange.h>
#include <Renderer/Descriptors/DescriptorSetLayoutView.h>
#include <Metal/DescriptorSetData.h>
#include <Renderer/Window/Window.h>

#include <Common/Memory/Containers/ZeroTerminatedStringView.h>
#include <Common/Memory/Align.h>

namespace ngine::Rendering
{
	PipelineLayout::PipelineLayout(
		[[maybe_unused]] const LogicalDeviceView logicalDevice,
		const ArrayView<const DescriptorSetLayoutView, uint8> descriptorSetLayouts,
		const ArrayView<const PushConstantRange, uint8> pushConstantRanges
	)
	{
#if RENDERER_VULKAN
		static_assert(sizeof(PushConstantRange) == sizeof(VkPushConstantRange));
		static_assert(alignof(PushConstantRange) == alignof(VkPushConstantRange));

		const VkPipelineLayoutCreateInfo pipelineLayoutInfo = {
			VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
			nullptr,
			0,
			descriptorSetLayouts.GetSize(),
			reinterpret_cast<const VkDescriptorSetLayout*>(descriptorSetLayouts.GetData()),
			pushConstantRanges.GetSize(),
			reinterpret_cast<const VkPushConstantRange*>(pushConstantRanges.GetData())
		};

		[[maybe_unused]] const VkResult pipelineLayoutCreationResult =
			vkCreatePipelineLayout(logicalDevice, &pipelineLayoutInfo, nullptr, &m_pPipelineLayout);
		Assert(pipelineLayoutCreationResult == VK_SUCCESS);
#elif RENDERER_METAL
		Internal::PipelineLayoutData* __restrict pPipelineLayoutData = new Internal::PipelineLayoutData{};

		if (descriptorSetLayouts.HasElements())
		{
			pPipelineLayoutData->m_descriptorSetLayouts.Reserve(descriptorSetLayouts.GetSize());

			for (const DescriptorSetLayoutView descriptorSetLayout : descriptorSetLayouts)
			{
				const Internal::DescriptorSetLayoutData* __restrict pDescriptorSetLayoutData = descriptorSetLayout;
				if ([logicalDevice supportsFamily:MTLGPUFamilyApple6])
				{
					for (const ShaderStage shaderStage : pDescriptorSetLayoutData->m_stages)
					{
						const uint8 shaderStageIndex = GetShaderStageIndex(shaderStage);
						pPipelineLayoutData->m_pushConstantOffsets[shaderStageIndex]++;
					}
				}
				else
				{
					for (const Internal::DescriptorSetLayoutData::Binding& __restrict descriptorBinding : pDescriptorSetLayoutData->m_bindings)
					{
						for (const ShaderStage shaderStage : descriptorBinding.m_stages)
						{
							const uint8 shaderStageIndex = GetShaderStageIndex(shaderStage);
							switch (descriptorBinding.m_type)
							{
								case DescriptorType::Sampler:
								case DescriptorType::SampledImage:
								case DescriptorType::UniformTexelBuffer:
								case DescriptorType::UniformBuffer:
								case DescriptorType::StorageBuffer:
								case DescriptorType::UniformBufferDynamic:
								case DescriptorType::StorageBufferDynamic:
								case DescriptorType::InputAttachment:
								case DescriptorType::AccelerationStructure:
								case DescriptorType::StorageImage:
								case DescriptorType::StorageTexelBuffer:
									pPipelineLayoutData->m_pushConstantOffsets[shaderStageIndex]++;
									break;
								case DescriptorType::CombinedImageSampler:
									pPipelineLayoutData->m_pushConstantOffsets[shaderStageIndex] += 2;
									break;
							}
						}
					}
				}

				pPipelineLayoutData->m_descriptorSetLayouts.EmplaceBack(descriptorSetLayout);
			}
		}

		if (pushConstantRanges.HasElements())
		{
			{
				const PushConstantRange& __restrict firstPushConstantRange = pushConstantRanges[0];
				for (const ShaderStage shaderStage : firstPushConstantRange.m_shaderStages)
				{
					const uint8 shaderStageIndex = GetShaderStageIndex(shaderStage);
					pPipelineLayoutData->m_pushConstantsStages |= shaderStage;

					pPipelineLayoutData->m_stagePushConstantDataRanges[shaderStageIndex] = Math::Range<uint16>::Make(
						(uint16)firstPushConstantRange.m_range.GetMinimum(),
						(uint16)firstPushConstantRange.m_range.GetSize()
					);
				}
			}

			for (const PushConstantRange& __restrict pushConstantRange : (pushConstantRanges + 1))
			{
				for (const ShaderStage shaderStage : pushConstantRange.m_shaderStages)
				{
					const uint8 shaderStageIndex = GetShaderStageIndex(shaderStage);
					pPipelineLayoutData->m_pushConstantsStages |= shaderStage;

					Math::Range<uint16>& existingPushConstantRange = pPipelineLayoutData->m_stagePushConstantDataRanges[shaderStageIndex];
					const Math::Range<uint16> checkedPushConstantRange =
						Math::Range<uint16>::Make((uint16)pushConstantRange.m_range.GetMinimum(), (uint16)pushConstantRange.m_range.GetSize());
					if (existingPushConstantRange.GetSize() > 0)
					{

						const Math::Range<uint16> newPushConstantRange = Math::Range<uint16>::MakeStartToEnd(
							Math::Min(existingPushConstantRange.GetMinimum(), checkedPushConstantRange.GetMinimum()),
							Math::Max(existingPushConstantRange.GetMaximum(), checkedPushConstantRange.GetMaximum())
						);

						existingPushConstantRange = newPushConstantRange;
					}
					else
					{
						existingPushConstantRange = checkedPushConstantRange;
					}
				}
			}

			for (const PushConstantRange& __restrict pushConstantRange : pushConstantRanges)
			{
				for (const ShaderStage shaderStage : pushConstantRange.m_shaderStages)
				{
					const uint8 shaderStageIndex = GetShaderStageIndex(shaderStage);
					Math::Range<uint16>& existingPushConstantRange = pPipelineLayoutData->m_stagePushConstantDataRanges[shaderStageIndex];
					existingPushConstantRange =
						Math::Range<uint16>::Make(0, (uint16)Memory::Align(existingPushConstantRange.GetMaximum() + (uint16)1u, (uint16)16u));
				}
			}
		}

		m_pPipelineLayout = pPipelineLayoutData;
#elif RENDERER_WEBGPU
		UNUSED(pushConstantRanges);
		Assert(pushConstantRanges.IsEmpty(), "Push constants aren't supported on WebGPU");

#if WEBGPU_SINGLE_THREADED
		WGPUPipelineLayout* pPipelineLayout = new WGPUPipelineLayout{nullptr};
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[logicalDevice, pPipelineLayout, descriptorSetLayouts = InlineVector<DescriptorSetLayoutView, 4>(descriptorSetLayouts)]()
			{
				InlineVector<WGPUBindGroupLayout, 1> bindGroupLayouts(Memory::Reserve, descriptorSetLayouts.GetSize());
				for (const DescriptorSetLayoutView descriptorSetLayout : descriptorSetLayouts)
				{
					const Internal::DescriptorSetLayoutData* pDescriptorSetLayoutData = descriptorSetLayout;
					bindGroupLayouts.EmplaceBack(pDescriptorSetLayoutData->m_bindGroupLayout);
				}

				const WGPUPipelineLayoutDescriptor descriptor
				{
					nullptr,
#if RENDERER_WEBGPU_DAWN
						WGPUStringView{nullptr, 0},
#else
						nullptr,
#endif
						bindGroupLayouts.GetSize(), bindGroupLayouts.GetData()
				};
				WGPUPipelineLayout pWGPUPipelineLayout = wgpuDeviceCreatePipelineLayout(logicalDevice, &descriptor);
#if RENDERER_WEBGPU_DAWN
				wgpuPipelineLayoutAddRef(pWGPUPipelineLayout);
#else
				wgpuPipelineLayoutReference(pWGPUPipelineLayout);
#endif
				*pPipelineLayout = pWGPUPipelineLayout;
			}
		);
		m_pPipelineLayout = pPipelineLayout;
#else
		InlineVector<WGPUBindGroupLayout, 1> bindGroupLayouts(Memory::Reserve, descriptorSetLayouts.GetSize());
		for (const DescriptorSetLayoutView descriptorSetLayout : descriptorSetLayouts)
		{
			const Internal::DescriptorSetLayoutData* pDescriptorSetLayoutData = descriptorSetLayout;
			bindGroupLayouts.EmplaceBack(pDescriptorSetLayoutData->m_bindGroupLayout);
		}

		const WGPUPipelineLayoutDescriptor descriptor{nullptr, nullptr, bindGroupLayouts.GetSize(), bindGroupLayouts.GetData()};
		WGPUPipelineLayout pPipelineLayout = wgpuDeviceCreatePipelineLayout(logicalDevice, &descriptor);
#if RENDERER_WEBGPU_DAWN
		wgpuPipelineLayoutAddRef(pPipelineLayout);
#else
		wgpuPipelineLayoutReference(pPipelineLayout);
#endif
		m_pPipelineLayout = pPipelineLayout;
#endif

#endif
	}

	PipelineLayout::~PipelineLayout()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	PipelineLayout& PipelineLayout::operator=([[maybe_unused]] PipelineLayout&& other)
	{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
		Assert(m_pPipelineLayout == 0, "Destroy must have been called!");
		m_pPipelineLayout = other.m_pPipelineLayout;
		other.m_pPipelineLayout = 0;
#endif
		return *this;
	}

	void PipelineLayout::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroyPipelineLayout(logicalDevice, m_pPipelineLayout, nullptr);
		m_pPipelineLayout = 0;
#elif RENDERER_METAL
		delete m_pPipelineLayout;
		m_pPipelineLayout = nullptr;
#elif RENDERER_WEBGPU
		if (m_pPipelineLayout != nullptr)
		{
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pPipelineLayout = m_pPipelineLayout]()
				{
					WGPUPipelineLayout pWGPUPipelineLayout = *pPipelineLayout;
					if (pWGPUPipelineLayout != nullptr)
					{
						wgpuPipelineLayoutRelease(pWGPUPipelineLayout);
					}
					delete pPipelineLayout;
				}
			);
#else
			wgpuPipelineLayoutRelease(m_pPipelineLayout);
#endif
		}
		m_pPipelineLayout = nullptr;
#endif
	}

	void PipelineLayout::SetDebugName([[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name)
	{
#if RENDERER_VULKAN
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_PIPELINE_LAYOUT,
			reinterpret_cast<uint64_t>(m_pPipelineLayout),
			name
		};

#if PLATFORM_APPLE
		vkSetDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
#else
		const PFN_vkSetDebugUtilsObjectNameEXT setDebugUtilsObjectNameEXT =
			reinterpret_cast<PFN_vkSetDebugUtilsObjectNameEXT>(logicalDevice.GetSetDebugUtilsObjectNameEXT());
		if (setDebugUtilsObjectNameEXT != nullptr)
		{
			setDebugUtilsObjectNameEXT(logicalDevice, &debugInfo);
		}
#endif

#elif RENDERER_WEBGPU
#if WEBGPU_SINGLE_THREADED
		Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
			[pPipelineLayout = m_pPipelineLayout, name]()
			{
				WGPUPipelineLayout pWGPUPipelineLayout = *pPipelineLayout;
				if (pWGPUPipelineLayout != nullptr)
				{
#if RENDERER_WEBGPU_DAWN
					wgpuPipelineLayoutSetLabel(pWGPUPipelineLayout, WGPUStringView{name, name.GetSize()});
#else
					wgpuPipelineLayoutSetLabel(pWGPUPipelineLayout, name);
#endif
				}
			}
		);
#else
		wgpuPipelineLayoutSetLabel(m_pPipelineLayout, name);
#endif
#else
		Assert(false, "TODO");
		UNUSED(logicalDevice);
		UNUSED(name);
#endif
	}
}
