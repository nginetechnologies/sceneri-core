#include <Renderer/Pipelines/GraphicsPipeline.h>
#include <Renderer/Pipelines/ComputePipeline.h>

#include <Renderer/Devices/LogicalDeviceView.h>
#include <Renderer/Descriptors/DescriptorSetView.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>
#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Pipelines/PushConstantRange.h>
#include <Renderer/Index.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>
#include "Metal/ConvertFormatToVertexFormat.h"
#include "Metal/ConvertFormat.h"
#include "Metal/ConvertBlendFactor.h"
#include "Metal/ConvertCompareOperation.h"
#include <Renderer/WebGPU/Includes.h>
#include "WebGPU/ConvertFormatToVertexFormat.h"
#include <Renderer/Window/Window.h>

#include <Common/Assert/Assert.h>
#include <Common/Memory/Containers/Array.h>

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Engine/Threading/JobRunnerThread.h>

#include <Renderer/Assets/Shader/ShaderCache.h>
#include "WebGPU/ConvertCompareOperation.h"
#include "WebGPU/ConvertBlendFactor.h"
#include "WebGPU/ConvertFormat.h"

#include <Renderer/FormatInfo.h>

namespace ngine::Rendering
{
	void GraphicsPipelineView::SetDebugName([[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name)
	{
#if RENDERER_VULKAN
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_PIPELINE,
			reinterpret_cast<uint64_t>(m_pPipeline),
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
		UNUSED(name);
		//[m_pPipeline setLabel:[NSString stringWithUTF8String:name]];
#elif RENDERER_WEBGPU
		if (m_pPipeline != nullptr)
		{
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pPipeline = m_pPipeline, name]()
				{
#if RENDERER_WEBGPU_DAWN
					wgpuRenderPipelineSetLabel(pPipeline, WGPUStringView{name, name.GetSize()});
#else
					wgpuRenderPipelineSetLabel(pPipeline, name);
#endif
				}
			);
		}
#endif
	}

	void ComputePipelineView::SetDebugName([[maybe_unused]] const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name)
	{
#if RENDERER_VULKAN
		const VkDebugUtilsObjectNameInfoEXT debugInfo{
			VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
			nullptr,
			VK_OBJECT_TYPE_PIPELINE,
			reinterpret_cast<uint64_t>(m_pPipeline),
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
		UNUSED(name);
		//[m_pPipeline setLabel:[NSString stringWithUTF8String:name]];
#elif RENDERER_WEBGPU
		if (m_pPipeline != nullptr)
		{
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pPipeline = m_pPipeline, name]()
				{
#if RENDERER_WEBGPU_DAWN
					wgpuComputePipelineSetLabel(pPipeline, WGPUStringView{name, name.GetSize()});
#else
					wgpuComputePipelineSetLabel(pPipeline, name);
#endif
				}
			);
		}
#endif
	}

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
	inline static constexpr Array<const DescriptorSetLayout::Binding, 1> PushConstantDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeStorageBufferDynamic(
			0, ShaderStage::Vertex | ShaderStage::Fragment | ShaderStage::Geometry | ShaderStage::Compute
		)
	};

	[[nodiscard]] FixedCapacityInlineVector<DescriptorSetLayoutView, 8> GetDescriptorSetLayouts(
		const DescriptorSetLayoutView pushConstantDescriptorSetLayout,
		const ArrayView<const DescriptorSetLayoutView, uint8> descriptorSetLayouts
	)
	{
		FixedCapacityInlineVector<DescriptorSetLayoutView, 8> descriptorSetLayoutsResult(Memory::Reserve, descriptorSetLayouts.GetSize() + 1);
		if (pushConstantDescriptorSetLayout.IsValid())
		{
			descriptorSetLayoutsResult.EmplaceBack(pushConstantDescriptorSetLayout);
		}
		descriptorSetLayoutsResult.CopyEmplaceRangeBack(descriptorSetLayouts);
		return descriptorSetLayoutsResult;
	}
#endif

	GraphicsPipelineBase& GraphicsPipelineBase::operator=(GraphicsPipelineBase&& other)
	{
#if RENDERER_SUPPORTS_PUSH_CONSTANTS
		m_pipelineLayout = Move(other.m_pipelineLayout);
#else
		m_pushConstantsDescriptorLayout = Move(m_pushConstantsDescriptorLayout);
		m_pipelineLayout = Move(other.m_pipelineLayout);
		m_pushConstantsDescriptorSet = Move(other.m_pushConstantsDescriptorSet);
		m_pPushConstantsDescriptorSetLoadingThread = other.m_pPushConstantsDescriptorSetLoadingThread;
		other.m_pPushConstantsDescriptorSetLoadingThread = nullptr;
#endif
		return *this;
	}

	void GraphicsPipelineBase::Destroy(LogicalDevice& logicalDevice)
	{
#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		Threading::EngineJobRunnerThread* pDescriptorSetLoadingThread = m_pPushConstantsDescriptorSetLoadingThread;
		if (pDescriptorSetLoadingThread != nullptr && m_pPushConstantsDescriptorSetLoadingThread.CompareExchangeStrong(pDescriptorSetLoadingThread, nullptr))
		{
			pDescriptorSetLoadingThread->GetRenderData()
				.DestroyDescriptorSets(logicalDevice.GetIdentifier(), ArrayView<DescriptorSet>{m_pushConstantsDescriptorSet});
		}
#endif

		m_pipelineLayout.Destroy(logicalDevice);

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		m_pushConstantsDescriptorLayout.Destroy(logicalDevice);
#endif
	}

	void GraphicsPipelineBase::Create(
		LogicalDevice& logicalDevice,
		const ArrayView<const DescriptorSetLayoutView, uint8> descriptorSetLayouts,
		const ArrayView<const PushConstantRange, uint8> pushConstantRanges
	)
	{
#if RENDERER_SUPPORTS_PUSH_CONSTANTS
		m_pipelineLayout = PipelineLayout(logicalDevice, descriptorSetLayouts, pushConstantRanges);
#else
		if (pushConstantRanges.HasElements())
		{
			m_pushConstantsDescriptorLayout = DescriptorSetLayout(logicalDevice, PushConstantDescriptorBindings);
#if RENDERER_OBJECT_DEBUG_NAMES
			m_pushConstantsDescriptorLayout.SetDebugName(logicalDevice, "Emulated Push Constants");
#endif

			Threading::EngineJobRunnerThread& engineThread = *Threading::EngineJobRunnerThread::GetCurrent();
			const bool allocatedDescriptorSets = engineThread.GetRenderData()
			                                       .GetDescriptorPool(logicalDevice.GetIdentifier())
			                                       .AllocateDescriptorSets(
																							 logicalDevice,
																							 ArrayView<const DescriptorSetLayoutView>{m_pushConstantsDescriptorLayout},
																							 ArrayView<DescriptorSet>{m_pushConstantsDescriptorSet}
																						 );
			Assert(allocatedDescriptorSets);
			if (LIKELY(allocatedDescriptorSets))
			{
				Threading::EngineJobRunnerThread* pPreviousDescriptorSetLoadingThread = nullptr;
				[[maybe_unused]] const bool wasExchanged =
					m_pPushConstantsDescriptorSetLoadingThread.CompareExchangeStrong(pPreviousDescriptorSetLoadingThread, &engineThread);
				Assert(wasExchanged);

				{
					const Array descriptorBufferInfo{
						DescriptorSet::BufferInfo{
							logicalDevice.GetPushConstantsBuffer(),
							0,
							LogicalDevice::MaximumPushConstantInstanceDataSize,
						},
					};
					const Array descriptorUpdates{
						DescriptorSet::UpdateInfo{m_pushConstantsDescriptorSet, 0, 0, DescriptorType::StorageBufferDynamic, descriptorBufferInfo}
					};
					DescriptorSet::Update(logicalDevice, descriptorUpdates);
				}
			}
		}

		m_pipelineLayout =
			PipelineLayout(logicalDevice, GetDescriptorSetLayouts(m_pushConstantsDescriptorLayout, descriptorSetLayouts).GetView(), {});
#endif
	}

	GraphicsPipeline& GraphicsPipeline::operator=(GraphicsPipeline&& other)
	{
		GraphicsPipelineBase::operator=(Forward<GraphicsPipelineBase>(other));
		Assert(!m_pipeline.IsValid(), "Destroy must have been called!");
		m_pipeline = other.m_pipeline;
		other.m_pipeline = {};
		return *this;
	}

	void GraphicsPipeline::Destroy(LogicalDevice& logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroyPipeline(logicalDevice, m_pipeline, nullptr);
		m_pipeline = {};
#elif RENDERER_METAL
		m_pipeline = {};
#elif RENDERER_WEBGPU
		if (m_pipeline.IsValid())
		{
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pipeline = m_pipeline]()
				{
					wgpuRenderPipelineRelease(pipeline);
				}
			);
#else
			wgpuRenderPipelineRelease(m_pipeline);
#endif
			m_pipeline = {};
		}
#endif

		GraphicsPipelineBase::Destroy(logicalDevice);
	}

	void GraphicsPipeline::SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name)
	{
		m_pipeline.SetDebugName(logicalDevice, name);
	}

	void GraphicsPipeline::CreateBase(
		LogicalDevice& logicalDevice,
		const ArrayView<const DescriptorSetLayoutView, uint8> descriptorSetLayouts,
		const ArrayView<const PushConstantRange, uint8> pushConstantRanges
	)
	{
		Assert(
			pushConstantRanges.All(
				[](const PushConstantRange& pushConstantRange)
				{
					return pushConstantRange.m_range.GetMaximum() <= 128;
				}
			),
			"Exceeding 128 bytes for push constants"
		);
		GraphicsPipelineBase::Create(logicalDevice, descriptorSetLayouts, pushConstantRanges);
	}

	Threading::JobBatch GraphicsPipeline::CreateAsync(
		LogicalDevice& logicalDevice,
		ShaderCache& shaderCache,
		PipelineLayoutView pipelineLayout,
		const RenderPassView renderPass,
		const VertexStageInfo& vertexStage,
		const PrimitiveInfo& primitiveInfo,
		ArrayView<const Viewport> viewports,
		ArrayView<const Math::Rectangleui> scissors,
		const uint8 subpassIndex,
		Optional<const FragmentStageInfo*> pFragmentStageInfo,
		Optional<const MultisamplingInfo*> pMultisamplingInfo,
		Optional<const DepthStencilInfo*> pDepthStencilInfo,
		Optional<const GeometryStageInfo*> pGeometryStageInfo,
		EnumFlags<DynamicStateFlags> dynamicStateFlags
	)
	{
		if constexpr (ENABLE_ASSERTS)
		{
			for (const VertexInputAttributeDescription& vertexAttributeDescription : vertexStage.m_attributeDescriptions)
			{
				[[maybe_unused]] const uint8 attributeIndex = vertexStage.m_attributeDescriptions.GetIteratorIndex(&vertexAttributeDescription);
				Assert(vertexAttributeDescription.shaderLocation == attributeIndex, "Attributes must be sequential!");

				Assert(
					attributeIndex == 0 || vertexAttributeDescription.binding == vertexStage.m_attributeDescriptions[attributeIndex - 1].binding ||
						vertexAttributeDescription.binding == vertexStage.m_attributeDescriptions[attributeIndex - 1].binding + 1,
					"Bindings must be sequential!"
				);
			}

			for (const VertexInputBindingDescription& vertexBindingDescription : vertexStage.m_bindingDescriptions)
			{
				[[maybe_unused]] const uint32 bindingIndex = vertexStage.m_bindingDescriptions.GetIteratorIndex(&vertexBindingDescription);
				Assert(vertexBindingDescription.binding == bindingIndex, "Bindings must be sequential!");
			}
		}

#if RENDERER_METAL
		Internal::PipelineLayoutData* __restrict pPipelineLayoutData = m_pipelineLayout;
		Assert(pPipelineLayoutData != nullptr);
		if (LIKELY(pPipelineLayoutData != nullptr))
		{
			pPipelineLayoutData->m_vertexBufferCount = vertexStage.m_bindingDescriptions.GetSize();
		}
#endif

		struct Data
		{
			Data(
				LogicalDevice& logicalDevice,
				ShaderCache& shaderCache,
				const PipelineLayoutView pipelineLayout,
				const RenderPassView renderPass,
				const VertexStageInfo vertexStage,
				const PrimitiveInfo primitiveInfo,
				const ArrayView<const Viewport> viewports,
				const ArrayView<const Math::Rectangleui> scissors,
				const uint8 subpassIndex,
				const Optional<const FragmentStageInfo*> pFragmentStageInfo,
				const Optional<const MultisamplingInfo*> pMultisamplingInfo,
				const Optional<const DepthStencilInfo*> pDepthStencilInfo,
				const Optional<const GeometryStageInfo*> pGeometryStageInfo,
				const EnumFlags<DynamicStateFlags> dynamicStateFlags
			)
				: m_bindingDescriptions(vertexStage.m_bindingDescriptions)
				, m_attributeDescriptions(vertexStage.m_attributeDescriptions)
				, m_colorTargets(pFragmentStageInfo.IsValid() ? pFragmentStageInfo->m_colorTargets : ArrayView<const ColorTargetInfo, uint8>{})
				, m_logicalDevice(logicalDevice)
				, m_shaderCache(shaderCache)
				, m_pipelineLayout(pipelineLayout)
				, m_renderPass(renderPass)
				, m_vertexStage{ShaderStageInfo{vertexStage}, m_bindingDescriptions.GetView(), m_attributeDescriptions.GetView()}
				, m_primitiveInfo(primitiveInfo)
				, m_viewports(viewports)
				, m_scissors(scissors)
				, m_subpassIndex(subpassIndex)
				, m_pFragmentStageInfo(
						pFragmentStageInfo.IsValid() ? FragmentStageInfo{ShaderStageInfo{*pFragmentStageInfo}, m_colorTargets.GetView()}
																				 : Optional<FragmentStageInfo>{}
					)
				, m_pMultisamplingInfo(pMultisamplingInfo.IsValid() ? *pMultisamplingInfo : Optional<MultisamplingInfo>{})
				, m_pDepthStencilInfo(pDepthStencilInfo.IsValid() ? *pDepthStencilInfo : Optional<DepthStencilInfo>{})
				, m_pGeometryStageInfo(pGeometryStageInfo.IsValid() ? *pGeometryStageInfo : Optional<GeometryStageInfo>{})
				, m_dynamicStateFlags(dynamicStateFlags)
			{
			}

			InlineVector<VertexInputBindingDescription, 6> m_bindingDescriptions;
			InlineVector<VertexInputAttributeDescription, 6> m_attributeDescriptions;
			InlineVector<ColorTargetInfo, 8> m_colorTargets;

			LogicalDevice& m_logicalDevice;
			ShaderCache& m_shaderCache;
			const PipelineLayoutView m_pipelineLayout;
			const RenderPassView m_renderPass;
			const VertexStageInfo m_vertexStage;
			const PrimitiveInfo m_primitiveInfo;
			const InlineVector<Viewport, 4> m_viewports;
			const InlineVector<Math::Rectangleui, 4> m_scissors;
			const uint8 m_subpassIndex;
			const Optional<FragmentStageInfo> m_pFragmentStageInfo;
			const Optional<MultisamplingInfo> m_pMultisamplingInfo;
			const Optional<DepthStencilInfo> m_pDepthStencilInfo;
			const Optional<GeometryStageInfo> m_pGeometryStageInfo;
			const EnumFlags<DynamicStateFlags> m_dynamicStateFlags;
		};
		UniquePtr<Data> pData{
			Memory::ConstructInPlace,
			logicalDevice,
			shaderCache,
			pipelineLayout,
			renderPass,
			vertexStage,
			primitiveInfo,
			viewports,
			scissors,
			subpassIndex,
			pFragmentStageInfo,
			pMultisamplingInfo,
			pDepthStencilInfo,
			pGeometryStageInfo,
			dynamicStateFlags
		};

		Threading::JobBatch jobBatch{Threading::JobBatch::IntermediateStage};
		if (pGeometryStageInfo.IsValid())
		{
			Threading::IntermediateStage& loadedStage = Threading::CreateIntermediateStage();
			loadedStage.AddSubsequentStage(jobBatch.GetFinishedStage());

			const Optional<Threading::Job*> pGeometryShaderLoadJob = shaderCache.FindOrLoad(
				pGeometryStageInfo->m_assetGuid,
				ShaderStage::Geometry,
				pGeometryStageInfo->m_entryPointName,
				m_pipelineLayout,
				ShaderCache::StageInfo{},
				ShaderCache::ShaderRequesters::ListenerData{
					this,
					[&loadedStage](GraphicsPipeline&, const ShaderView)
					{
						loadedStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
						return EventCallbackResult::Remove;
					}
				}
			);
			if (pGeometryShaderLoadJob.IsValid())
			{
				jobBatch.QueueAfterStartStage(*pGeometryShaderLoadJob);
			}
		}

		{
			Threading::IntermediateStage& loadedStage = Threading::CreateIntermediateStage();
			loadedStage.AddSubsequentStage(jobBatch.GetFinishedStage());

			const Optional<Threading::Job*> pVertexShaderLoadJob = shaderCache.FindOrLoad(
				vertexStage.m_assetGuid,
				ShaderStage::Vertex,
				vertexStage.m_entryPointName,
				m_pipelineLayout,
				ShaderCache::StageInfo{ShaderCache::VertexInfo{pData->m_bindingDescriptions.GetView(), pData->m_attributeDescriptions.GetView()}},
				ShaderCache::ShaderRequesters::ListenerData{
					this,
					[&loadedStage](GraphicsPipeline&, const ShaderView)
					{
						loadedStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
						return EventCallbackResult::Remove;
					}
				}
			);
			if (pVertexShaderLoadJob.IsValid())
			{
				jobBatch.QueueAfterStartStage(*pVertexShaderLoadJob);
			}
		}

		if (pFragmentStageInfo.IsValid())
		{
			Threading::IntermediateStage& loadedStage = Threading::CreateIntermediateStage();
			loadedStage.AddSubsequentStage(jobBatch.GetFinishedStage());

			const Optional<Threading::Job*> pFragmentShaderLoadJob = shaderCache.FindOrLoad(
				pFragmentStageInfo->m_assetGuid,
				ShaderStage::Fragment,
				pFragmentStageInfo->m_entryPointName,
				m_pipelineLayout,
				ShaderCache::StageInfo{ShaderCache::FragmentInfo{renderPass, subpassIndex}},
				ShaderCache::ShaderRequesters::ListenerData{
					this,
					[&loadedStage](GraphicsPipeline&, const ShaderView)
					{
						loadedStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
						return EventCallbackResult::Remove;
					}
				}
			);
			if (pFragmentShaderLoadJob.IsValid())
			{
				jobBatch.QueueAfterStartStage(*pFragmentShaderLoadJob);
			}
		}

		jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
			[this, pData = Move(pData)](Threading::JobRunnerThread&)
			{
				Data& __restrict data = *pData;
				CreateInternal(
					data.m_logicalDevice,
					data.m_shaderCache,
					data.m_pipelineLayout,
					data.m_renderPass,
					data.m_vertexStage,
					data.m_primitiveInfo,
					data.m_viewports,
					data.m_scissors,
					data.m_subpassIndex,
					data.m_pFragmentStageInfo.IsValid() ? Optional<const FragmentStageInfo*>{&data.m_pFragmentStageInfo.GetUnsafe()}
																							: Optional<const FragmentStageInfo*>{},
					data.m_pMultisamplingInfo.IsValid() ? Optional<const MultisamplingInfo*>{&data.m_pMultisamplingInfo.GetUnsafe()}
																							: Optional<const MultisamplingInfo*>{},
					data.m_pDepthStencilInfo.IsValid() ? Optional<const DepthStencilInfo*>{&data.m_pDepthStencilInfo.GetUnsafe()}
																						 : Optional<const DepthStencilInfo*>{},
					data.m_pGeometryStageInfo.IsValid() ? Optional<const GeometryStageInfo*>{&data.m_pGeometryStageInfo.GetUnsafe()}
																							: Optional<const GeometryStageInfo*>{},
					data.m_dynamicStateFlags
				);
			},
			Threading::JobPriority::LoadGraphicsPipeline
		));
		return jobBatch;
	}

	void GraphicsPipeline::CreateInternal(
		LogicalDevice& logicalDevice,
		ShaderCache& shaderCache,
		[[maybe_unused]] PipelineLayoutView pipelineLayout,
		const RenderPassView renderPass,
		const VertexStageInfo& vertexStage,
		const PrimitiveInfo& primitiveInfo,
		ArrayView<const Viewport> viewports,
		ArrayView<const Math::Rectangleui> scissors,
		const uint8 subpassIndex,
		Optional<const FragmentStageInfo*> pFragmentStageInfo,
		Optional<const MultisamplingInfo*> pMultisamplingInfo,
		Optional<const DepthStencilInfo*> pDepthStencilInfo,
		Optional<const GeometryStageInfo*> pGeometryStageInfo,
		EnumFlags<DynamicStateFlags> dynamicStateFlags
	)
	{
		const ShaderView vertexShader = shaderCache.GetAssetData(shaderCache.FindIdentifier(vertexStage.m_assetGuid));
		Assert(vertexShader.IsValid());
		const ShaderView fragmentShader = pFragmentStageInfo.IsValid()
		                                    ? shaderCache.GetAssetData(shaderCache.FindIdentifier(pFragmentStageInfo->m_assetGuid))
		                                    : ShaderView{};
		Assert(fragmentShader.IsValid() || pFragmentStageInfo.IsInvalid());
		[[maybe_unused]] const ShaderView geometryShader =
			pGeometryStageInfo.IsValid() ? shaderCache.GetAssetData(shaderCache.FindIdentifier(pGeometryStageInfo->m_assetGuid)) : ShaderView{};
		Assert(geometryShader.IsValid() || pGeometryStageInfo.IsInvalid());

		if (LIKELY(
					vertexShader.IsValid() & (fragmentShader.IsValid() || pFragmentStageInfo.IsInvalid()) &
					(geometryShader.IsValid() || pGeometryStageInfo.IsInvalid())
				))
		{
#if RENDERER_VULKAN
			const uint8 shaderStageCount = 1 + pFragmentStageInfo.IsValid() + pGeometryStageInfo.IsValid();
			FlatVector<VkPipelineShaderStageCreateInfo, 3> shaderStages(Memory::Reserve, shaderStageCount);
			{

				const ShaderStageInfo vertexShaderInfo = vertexStage;
				shaderStages.EmplaceBack(VkPipelineShaderStageCreateInfo{
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					nullptr,
					VkPipelineShaderStageCreateFlags{0},
					VK_SHADER_STAGE_VERTEX_BIT,
					vertexShader,
					vertexShaderInfo.m_entryPointName,
					nullptr
				});
			}

			if (pFragmentStageInfo.IsValid())
			{
				const ShaderStageInfo fragmentShaderInfo = *pFragmentStageInfo;
				shaderStages.EmplaceBack(VkPipelineShaderStageCreateInfo{
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					nullptr,
					VkPipelineShaderStageCreateFlags{0},
					VK_SHADER_STAGE_FRAGMENT_BIT,
					fragmentShader,
					fragmentShaderInfo.m_entryPointName,
					nullptr
				});
			}

			if (pGeometryStageInfo.IsValid())
			{
				const ShaderStageInfo geometryShaderInfo = *pGeometryStageInfo;
				shaderStages.EmplaceBack(VkPipelineShaderStageCreateInfo{
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					nullptr,
					VkPipelineShaderStageCreateFlags{0},
					VK_SHADER_STAGE_GEOMETRY_BIT,
					geometryShader,
					geometryShaderInfo.m_entryPointName,
					nullptr
				});
			}

			static_assert(sizeof(VertexInputBindingDescription) == sizeof(VkVertexInputBindingDescription));
			static_assert(alignof(VertexInputBindingDescription) == alignof(VkVertexInputBindingDescription));
			static_assert(sizeof(VertexInputAttributeDescription) == sizeof(VkVertexInputAttributeDescription));
			static_assert(alignof(VertexInputAttributeDescription) == alignof(VkVertexInputAttributeDescription));

			InlineVector<VkVertexInputBindingDescription, 8> vertexInputBindingDescriptions(
				Memory::Reserve,
				vertexStage.m_bindingDescriptions.GetSize()
			);
			for (const VertexInputBindingDescription& inputBindingDescription : vertexStage.m_bindingDescriptions)
			{
				vertexInputBindingDescriptions.EmplaceBack(VkVertexInputBindingDescription{
					inputBindingDescription.binding,
					inputBindingDescription.stride,
					static_cast<VkVertexInputRate>(inputBindingDescription.inputRate)
				});
			}

			InlineVector<VkVertexInputAttributeDescription, 8> vertexInputAttributeDescriptions(
				Memory::Reserve,
				vertexStage.m_attributeDescriptions.GetSize()
			);
			for (const VertexInputAttributeDescription& inputAttributeDescription : vertexStage.m_attributeDescriptions)
			{
				vertexInputAttributeDescriptions.EmplaceBack(VkVertexInputAttributeDescription{
					inputAttributeDescription.shaderLocation,
					inputAttributeDescription.binding,
					static_cast<VkFormat>(inputAttributeDescription.format),
					inputAttributeDescription.offset
				});
			}

			const VkPipelineVertexInputStateCreateInfo vertexInputState{
				VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO,
				nullptr,
				VkPipelineVertexInputStateCreateFlags{0},
				vertexInputBindingDescriptions.GetSize(),
				vertexInputBindingDescriptions.GetData(),
				vertexInputAttributeDescriptions.GetSize(),
				vertexInputAttributeDescriptions.GetData()
			};

			static_assert(sizeof(PrimitiveTopology) == sizeof(VkPrimitiveTopology));
			static_assert(alignof(PrimitiveTopology) == alignof(VkPrimitiveTopology));
			static_assert((uint32)PrimitiveTopology::TriangleList == VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);

			const VkPipelineInputAssemblyStateCreateInfo inputAssemblyState{
				VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO,
				nullptr,
				VkPipelineInputAssemblyStateCreateFlags{0},
				static_cast<VkPrimitiveTopology>(primitiveInfo.topology),
				VK_FALSE
			};

			const VkPipelineTessellationStateCreateInfo
				tessellationState{VK_STRUCTURE_TYPE_PIPELINE_TESSELLATION_STATE_CREATE_INFO, nullptr, VkPipelineTessellationStateCreateFlags{0}, 0};

			InlineVector<VkViewport, 4> vulkanViewports(Memory::Reserve, viewports.GetSize());
			for (const Viewport viewport : viewports)
			{
				vulkanViewports.EmplaceBack(VkViewport{
					viewport.m_area.GetPosition().x,
					viewport.m_area.GetPosition().y,
					viewport.m_area.GetSize().x,
					viewport.m_area.GetSize().y,
					viewport.minDepth,
					viewport.maxDepth
				});
			}

			InlineVector<VkRect2D, 4> vulkanScissors(Memory::Reserve, scissors.GetSize());
			for (const Math::Rectangleui scissor : scissors)
			{
				const Math::Vector2i position = (Math::Vector2i)scissor.GetPosition();
				vulkanScissors.EmplaceBack(VkRect2D{
					VkOffset2D{
						position.x,
						position.y,
					},
					VkExtent2D{
						scissor.GetSize().x,
						scissor.GetSize().y,
					}
				});
			}

			const VkPipelineViewportStateCreateInfo viewportState{
				VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO,
				nullptr,
				VkPipelineViewportStateCreateFlags{0},
				vulkanViewports.GetSize(),
				vulkanViewports.GetData(),
				vulkanScissors.GetSize(),
				vulkanScissors.GetData()
			};

			static_assert((uint32)PolygonMode::Fill == VK_POLYGON_MODE_FILL);
			static_assert((uint32)PolygonMode::Line == VK_POLYGON_MODE_LINE);
			static_assert((uint32)PolygonMode::Point == VK_POLYGON_MODE_POINT);

			static_assert((uint32)CullMode::Front == VK_CULL_MODE_FRONT_BIT);
			static_assert((uint32)CullMode::Back == VK_CULL_MODE_BACK_BIT);

			static_assert((uint32)WindingOrder::Clockwise == VK_FRONT_FACE_CLOCKWISE);
			static_assert((uint32)WindingOrder::CounterClockwise == VK_FRONT_FACE_COUNTER_CLOCKWISE);

			const VkPipelineRasterizationStateCreateInfo rasterizationState{
				VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO,
				nullptr,
				VkPipelineRasterizationStateCreateFlags{0},
				primitiveInfo.flags.IsSet(PrimitiveFlags::DepthClamp),
				primitiveInfo.flags.IsSet(PrimitiveFlags::RasterizerDiscard),
				static_cast<VkPolygonMode>(primitiveInfo.polygonMode),
				static_cast<VkCullModeFlags>(primitiveInfo.cullMode.GetFlags()),
				static_cast<VkFrontFace>(primitiveInfo.windingOrder),
				primitiveInfo.flags.IsSet(PrimitiveFlags::DepthBias),
				(float)primitiveInfo.depthBiasConstantFactor,
				primitiveInfo.depthBiasClamp,
				primitiveInfo.depthBiasSlopeFactor,
				1.f // lineWidth
			};

			const VkPipelineMultisampleStateCreateInfo multisamplingState{
				VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO,
				nullptr,
				VkPipelineMultisampleStateCreateFlags{0},
				static_cast<VkSampleCountFlagBits>(pMultisamplingInfo.IsValid() ? pMultisamplingInfo->rasterizationSamples : SampleCount::One),
				VK_FALSE, // sampleShadingEnable
				1.f,      // minSampleShading
				nullptr,  // pSampleMask
				VK_FALSE, // alphaToCoverageEnable
				VK_FALSE  // alphaToOneEnable
			};

			VkPipelineDepthStencilStateCreateInfo depthStencilState;
			if (pDepthStencilInfo.IsValid())
			{
				static_assert(sizeof(StencilOperationState) == sizeof(VkStencilOpState));
				static_assert(alignof(StencilOperationState) == alignof(VkStencilOpState));

				const DepthStencilInfo depthStencilInfo = *pDepthStencilInfo;
				depthStencilState = VkPipelineDepthStencilStateCreateInfo{
					VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO,
					nullptr,
					VkPipelineDepthStencilStateCreateFlags{0},
					depthStencilInfo.m_flags.IsSet(DepthStencilFlags::DepthTest),
					depthStencilInfo.m_flags.IsSet(DepthStencilFlags::DepthWrite),
					static_cast<VkCompareOp>(depthStencilInfo.m_depthCompareOperation),
					depthStencilInfo.m_flags.IsSet(DepthStencilFlags::DepthBoundsTest),
					depthStencilInfo.m_flags.IsSet(DepthStencilFlags::StencilTest),
					reinterpret_cast<const VkStencilOpState&>(depthStencilInfo.m_front),
					reinterpret_cast<const VkStencilOpState&>(depthStencilInfo.m_back),
					0.f, // minDepthBounds
					1.f  // maxDepthBounds
				};
			}

			VkPipelineColorBlendStateCreateInfo colorBlendState;

			FlatVector<VkPipelineColorBlendAttachmentState, 16, uint32> colorBlendAttachmentStates;

			if (pFragmentStageInfo.IsValid())
			{
				const ArrayView<const ColorTargetInfo> colorTargets = pFragmentStageInfo->m_colorTargets;

				for (const ColorTargetInfo colorTargetInfo : colorTargets)
				{
					colorBlendAttachmentStates.EmplaceBack(VkPipelineColorBlendAttachmentState{
						static_cast<VkBool32>(colorTargetInfo.colorBlendState.IsEnabled() | colorTargetInfo.alphaBlendState.IsEnabled()),
						static_cast<VkBlendFactor>(colorTargetInfo.colorBlendState.sourceBlendFactor),
						static_cast<VkBlendFactor>(colorTargetInfo.colorBlendState.targetBlendFactor),
						static_cast<VkBlendOp>(colorTargetInfo.colorBlendState.blendOperation),
						static_cast<VkBlendFactor>(colorTargetInfo.alphaBlendState.sourceBlendFactor),
						static_cast<VkBlendFactor>(colorTargetInfo.alphaBlendState.targetBlendFactor),
						static_cast<VkBlendOp>(colorTargetInfo.alphaBlendState.blendOperation),
						static_cast<VkColorComponentFlags>(colorTargetInfo.colorWriteMask.GetFlags())
					});
				}

				colorBlendState = VkPipelineColorBlendStateCreateInfo{
					VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO,
					nullptr,
					VkPipelineColorBlendStateCreateFlags{0},
					VK_FALSE, // logicOpEnable
					VK_LOGIC_OP_CLEAR,
					colorBlendAttachmentStates.GetSize(),
					colorBlendAttachmentStates.GetData(),
					{0.f, 0.f, 0.f, 0.f} // blendConstants
				};
			}

			InlineVector<VkDynamicState, (uint8)DynamicStateFlags::Count> dynamicStates(Memory::Reserve, dynamicStateFlags.GetNumberOfSetFlags());
			if (dynamicStateFlags.IsSet(DynamicStateFlags::Viewport))
			{
				dynamicStates.EmplaceBack(VK_DYNAMIC_STATE_VIEWPORT);
			}
			if (dynamicStateFlags.IsSet(DynamicStateFlags::LineWidth))
			{
				dynamicStates.EmplaceBack(VK_DYNAMIC_STATE_LINE_WIDTH);
			}
			if (dynamicStateFlags.IsSet(DynamicStateFlags::DepthBias))
			{
				dynamicStates.EmplaceBack(VK_DYNAMIC_STATE_DEPTH_BIAS);
			}
			if (dynamicStateFlags.IsSet(DynamicStateFlags::BlendConstants))
			{
				dynamicStates.EmplaceBack(VK_DYNAMIC_STATE_BLEND_CONSTANTS);
			}
			if (dynamicStateFlags.IsSet(DynamicStateFlags::DepthBounds))
			{
				dynamicStates.EmplaceBack(VK_DYNAMIC_STATE_DEPTH_BOUNDS);
			}
			if (dynamicStateFlags.IsSet(DynamicStateFlags::StencilCompareMask))
			{
				dynamicStates.EmplaceBack(VK_DYNAMIC_STATE_STENCIL_COMPARE_MASK);
			}
			if (dynamicStateFlags.IsSet(DynamicStateFlags::StencilWriteMask))
			{
				dynamicStates.EmplaceBack(VK_DYNAMIC_STATE_STENCIL_WRITE_MASK);
			}
			if (dynamicStateFlags.IsSet(DynamicStateFlags::StencilReference))
			{
				dynamicStates.EmplaceBack(VK_DYNAMIC_STATE_STENCIL_REFERENCE);
			}
			if (dynamicStateFlags.IsSet(DynamicStateFlags::CullMode))
			{
				dynamicStates.EmplaceBack(VK_DYNAMIC_STATE_CULL_MODE_EXT);
			}

			const VkPipelineDynamicStateCreateInfo& dynamicState{
				VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO,
				nullptr,
				VkPipelineDynamicStateCreateFlags{0},
				dynamicStates.GetSize(),
				dynamicStates.GetData()
			};

			const VkGraphicsPipelineCreateInfo creationInfo{
				VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO,
				nullptr,
				VkPipelineStageFlags{0},
				shaderStages.GetSize(),
				shaderStages.GetData(),
				&vertexInputState,
				&inputAssemblyState,
				&tessellationState,
				&viewportState,
				&rasterizationState,
				&multisamplingState,
				pDepthStencilInfo.IsValid() ? &depthStencilState : nullptr,
				pFragmentStageInfo.IsValid() ? &colorBlendState : nullptr,
				&dynamicState,
				pipelineLayout,
				renderPass,
				subpassIndex,
				nullptr,
				-1
			};

			VkPipeline pipeline;
			[[maybe_unused]] const VkResult graphicsPipelineCreationResult =
				vkCreateGraphicsPipelines(logicalDevice, shaderCache.GetPipelineCache(), 1, &creationInfo, nullptr, &pipeline);
			Assert(graphicsPipelineCreationResult == VK_SUCCESS);
			m_pipeline = pipeline;
#elif RENDERER_METAL
			MTLRenderPipelineDescriptor* pipelineStateDescriptor = [[MTLRenderPipelineDescriptor alloc] init];

			{
				[pipelineStateDescriptor setVertexFunction:vertexShader];
			}

			MTLVertexDescriptor* vertexDescriptor = [[MTLVertexDescriptor alloc] init];
			Internal::PipelineLayoutData* __restrict pPipelineLayoutData = m_pipelineLayout;
			Assert(pPipelineLayoutData != nullptr);
			if (UNLIKELY_ERROR(pPipelineLayoutData == nullptr))
			{
				return;
			}

			for (const VertexInputBindingDescription& __restrict vertexInputBindingDescription : vertexStage.m_bindingDescriptions)
			{
				if (vertexInputBindingDescription.inputRate == VertexInputRate::Vertex)
				{
					pPipelineLayoutData->m_vertexBufferCount =
						Math::Max(pPipelineLayoutData->m_vertexBufferCount, uint8(vertexInputBindingDescription.binding + 1));
				}

				MTLVertexBufferLayoutDescriptor* layoutDescriptor = [[MTLVertexBufferLayoutDescriptor alloc] init];
				layoutDescriptor.stride = vertexInputBindingDescription.stride;
				switch (vertexInputBindingDescription.inputRate)
				{
					case VertexInputRate::Vertex:
						layoutDescriptor.stepFunction = MTLVertexStepFunctionPerVertex;
						break;
					case VertexInputRate::Instance:
						layoutDescriptor.stepFunction = MTLVertexStepFunctionPerInstance;
						break;
				}
				layoutDescriptor.stepRate = 1;

				vertexDescriptor.layouts[vertexInputBindingDescription.binding] = layoutDescriptor;
			}

			const uint8 bufferOffset = 0;

			for (const VertexInputAttributeDescription& __restrict vertexInputAttributeDescription : vertexStage.m_attributeDescriptions)
			{
				MTLVertexAttributeDescriptor* attributeDescriptor = [[MTLVertexAttributeDescriptor alloc] init];

				attributeDescriptor.format = ConvertFormatToVertexFormat(vertexInputAttributeDescription.format);
				attributeDescriptor.offset = vertexInputAttributeDescription.offset;
				attributeDescriptor.bufferIndex = bufferOffset + vertexInputAttributeDescription.binding;

				vertexDescriptor.attributes[vertexInputAttributeDescription.shaderLocation] = attributeDescriptor;
			}

			pipelineStateDescriptor.vertexDescriptor = vertexDescriptor;

			if (pMultisamplingInfo.IsValid())
			{
				pipelineStateDescriptor.rasterSampleCount = static_cast<uint32>(pMultisamplingInfo->rasterizationSamples);

				// TODO: Expose these (also available for Vulkan)
				// alphaToCoverageEnabled
				// alphaToOneEnabled
			}
			else
			{
				pipelineStateDescriptor.rasterSampleCount = 1;
			}

			const bool isRasterizationDisabled = primitiveInfo.flags.IsSet(PrimitiveFlags::RasterizerDiscard) ||
			                                     (primitiveInfo.cullMode.AreAllSet(CullMode::FrontAndBack) &&
			                                      primitiveInfo.topology == PrimitiveTopology::TriangleList);
			pipelineStateDescriptor.rasterizationEnabled = !isRasterizationDisabled;

			switch (primitiveInfo.topology)
			{
				case PrimitiveTopology::TriangleList:
					pipelineStateDescriptor.inputPrimitiveTopology = MTLPrimitiveTopologyClassTriangle;
					break;
			}

			m_cullMode = primitiveInfo.cullMode;
			m_windingOrder = primitiveInfo.windingOrder;
			m_polygonMode = primitiveInfo.polygonMode;
			m_depthClamp = primitiveInfo.flags.IsSet(PrimitiveFlags::DepthClamp);

			m_depthBiasConstantFactor = primitiveInfo.depthBiasConstantFactor;
			m_depthBiasSlopeFactor = primitiveInfo.depthBiasSlopeFactor;
			m_depthBiasClamp = primitiveInfo.depthBiasClamp;

			UNUSED(viewports);
			UNUSED(scissors);
			UNUSED(scissors);
			UNUSED(dynamicStateFlags);
			UNUSED(subpassIndex);

			const Internal::RenderPassData* __restrict pRenderPassData{renderPass};

			if (pFragmentStageInfo.IsValid())
			{
				[pipelineStateDescriptor setFragmentFunction:fragmentShader];

				// const ArrayView<const uint8> subpassInputAttachmentIndices =
				// pRenderPassData->m_subpassInputAttachmentIndices[(uint8)subpassIndex];
				const ArrayView<const uint8> subpassColorAttachmentIndices = pRenderPassData->m_subpassColorAttachmentIndices[(uint8)subpassIndex];
				// const uint8 subpassDepthAttachmentIndex = pRenderPassData->m_subpassDepthAttachmentIndices[(uint8)subpassIndex];

				// Bind all input attachments
				/*for(const uint8 inputAttachmentIndex : subpassInputAttachmentIndices)
				{
				    MTLRenderPipelineColorAttachmentDescriptor* colorAttachmentDescriptor = [[MTLRenderPipelineColorAttachmentDescriptor alloc]
				init];

				    const AttachmentDescription& __restrict attachmentDescription = pRenderPassData->m_attachmentDescriptions[inputAttachmentIndex];

				    const FormatInfo& __restrict formatInfo = GetFormatInfo(attachmentDescription.m_format);
				    if(formatInfo.m_flags.AreNoneSet(FormatFlags::DepthStencil))
				    {
				        colorAttachmentDescriptor.pixelFormat = ConvertFormat(attachmentDescription.m_format);

				        colorAttachmentDescriptor.blendingEnabled = false;

				        colorAttachmentDescriptor.sourceRGBBlendFactor = MTLBlendFactorOne;
				        colorAttachmentDescriptor.destinationRGBBlendFactor = MTLBlendFactorZero;
				        colorAttachmentDescriptor.rgbBlendOperation = MTLBlendOperationAdd;

				        colorAttachmentDescriptor.sourceAlphaBlendFactor = MTLBlendFactorOne;
				        colorAttachmentDescriptor.destinationAlphaBlendFactor = MTLBlendFactorZero;
				        colorAttachmentDescriptor.alphaBlendOperation = MTLBlendOperationAdd;

				        colorAttachmentDescriptor.writeMask = MTLColorWriteMaskNone;

				        pipelineStateDescriptor.colorAttachments[inputAttachmentIndex] = colorAttachmentDescriptor;
				    }
				}*/

				Assert(pFragmentStageInfo->m_colorTargets.GetSize() == subpassColorAttachmentIndices.GetSize());
				for (const ColorTargetInfo& __restrict colorTarget : pFragmentStageInfo->m_colorTargets)
				{
					const uint8 colorTargetIndex = pFragmentStageInfo->m_colorTargets.GetIteratorIndex(&colorTarget);
					const uint8 colorAttachmentIndex = (uint8)subpassColorAttachmentIndices[colorTargetIndex];

					MTLRenderPipelineColorAttachmentDescriptor* colorAttachmentDescriptor = [[MTLRenderPipelineColorAttachmentDescriptor alloc] init];

					colorAttachmentDescriptor.pixelFormat = ConvertFormat(pRenderPassData->m_attachmentDescriptions[colorAttachmentIndex].m_format);

					colorAttachmentDescriptor.blendingEnabled = colorTarget.colorBlendState.IsEnabled() | colorTarget.alphaBlendState.IsEnabled();

					colorAttachmentDescriptor.sourceRGBBlendFactor = ConvertBlendFactor(colorTarget.colorBlendState.sourceBlendFactor);
					colorAttachmentDescriptor.destinationRGBBlendFactor = ConvertBlendFactor(colorTarget.colorBlendState.targetBlendFactor);
					colorAttachmentDescriptor.rgbBlendOperation = static_cast<MTLBlendOperation>(colorTarget.colorBlendState.blendOperation);

					colorAttachmentDescriptor.sourceAlphaBlendFactor = ConvertBlendFactor(colorTarget.alphaBlendState.sourceBlendFactor);
					colorAttachmentDescriptor.destinationAlphaBlendFactor = ConvertBlendFactor(colorTarget.alphaBlendState.targetBlendFactor);
					colorAttachmentDescriptor.alphaBlendOperation = static_cast<MTLBlendOperation>(colorTarget.alphaBlendState.blendOperation);

					colorAttachmentDescriptor.writeMask = colorTarget.colorWriteMask.GetUnderlyingValue();

					pipelineStateDescriptor.colorAttachments[colorTargetIndex] = colorAttachmentDescriptor;
				}

				// Bind all unbound attachment descriptions
				/*for(const AttachmentDescription& attachmentDescription : pRenderPassData->m_attachmentDescriptions)
				{
				    const uint8 attachmentIndex = pRenderPassData->m_attachmentDescriptions.GetIteratorIndex(&attachmentDescription);

				    const bool isColorAttachment = subpassColorAttachmentIndices.ContainsIf([attachmentIndex](const uint32
				subpassColorAttachmentIndex)
				    {
				        return subpassColorAttachmentIndex == attachmentIndex;
				    });
				    const bool isInputAttachment = subpassInputAttachmentIndices.ContainsIf([attachmentIndex](const uint32
				subpassInputAttachmentIndex)
				    {
				        return subpassInputAttachmentIndex == attachmentIndex;
				    });
				    const bool isDepthAttachment = attachmentIndex == subpassDepthAttachmentIndex;
				    const bool isAttachmentUnused = !isColorAttachment && !isInputAttachment && !isDepthAttachment;

				    const FormatInfo& __restrict formatInfo = GetFormatInfo(attachmentDescription.m_format);
				    if(formatInfo.m_flags.AreNoneSet(FormatFlags::DepthStencil) && isAttachmentUnused)
				    {
				        MTLRenderPipelineColorAttachmentDescriptor* colorAttachmentDescriptor = [[MTLRenderPipelineColorAttachmentDescriptor alloc]
				init];

				        colorAttachmentDescriptor.pixelFormat = ConvertFormat(attachmentDescription.m_format);

				        colorAttachmentDescriptor.blendingEnabled = false;

				        colorAttachmentDescriptor.sourceRGBBlendFactor = MTLBlendFactorOne;
				        colorAttachmentDescriptor.destinationRGBBlendFactor = MTLBlendFactorZero;
				        colorAttachmentDescriptor.rgbBlendOperation = MTLBlendOperationAdd;

				        colorAttachmentDescriptor.sourceAlphaBlendFactor = MTLBlendFactorOne;
				        colorAttachmentDescriptor.destinationAlphaBlendFactor = MTLBlendFactorZero;
				        colorAttachmentDescriptor.alphaBlendOperation = MTLBlendOperationAdd;

				        colorAttachmentDescriptor.writeMask = MTLColorWriteMaskNone;

				        pipelineStateDescriptor.colorAttachments[attachmentIndex] = colorAttachmentDescriptor;
				    }
				}*/
			}

			if (pDepthStencilInfo.IsValid())
			{
				const uint8 subpassDepthAttachmentIndex = pRenderPassData->m_subpassDepthAttachmentIndices[(uint8)subpassIndex];
				Assert(subpassDepthAttachmentIndex != Internal::RenderPassData::InvalidAttachmentIndex);
				const AttachmentDescription& __restrict depthStencilAttachmentDescription =
					pRenderPassData->m_attachmentDescriptions[subpassDepthAttachmentIndex];

				const Rendering::FormatInfo formatInfo = Rendering::GetFormatInfo(depthStencilAttachmentDescription.m_format);
				Assert(formatInfo.m_flags.AreAnySet(Rendering::FormatFlags::DepthStencil));
				if (formatInfo.m_flags.IsSet(Rendering::FormatFlags::Depth))
				{
					pipelineStateDescriptor.depthAttachmentPixelFormat = ConvertFormat(depthStencilAttachmentDescription.m_format);
				}
				else
				{
					pipelineStateDescriptor.depthAttachmentPixelFormat = MTLPixelFormatInvalid;
				}
				if (formatInfo.m_flags.IsSet(Rendering::FormatFlags::Stencil))
				{
					pipelineStateDescriptor.stencilAttachmentPixelFormat = ConvertFormat(depthStencilAttachmentDescription.m_format);
				}
				else
				{
					pipelineStateDescriptor.stencilAttachmentPixelFormat = MTLPixelFormatInvalid;
				}

				MTLDepthStencilDescriptor* depthStencilDescriptor = [[MTLDepthStencilDescriptor alloc] init];
				depthStencilDescriptor.depthCompareFunction = pDepthStencilInfo->m_flags.IsSet(DepthStencilFlags::DepthTest)
				                                                ? ConvertCompareOperation(pDepthStencilInfo->m_depthCompareOperation)
				                                                : MTLCompareFunctionAlways;
				depthStencilDescriptor.depthWriteEnabled = pDepthStencilInfo->m_flags.IsSet(DepthStencilFlags::DepthWrite);

				if (pDepthStencilInfo->m_flags.IsSet(DepthStencilFlags::StencilTest))
				{
					{
						MTLStencilDescriptor* frontFaceStencil = [[MTLStencilDescriptor alloc] init];
						frontFaceStencil.stencilCompareFunction = ConvertCompareOperation(pDepthStencilInfo->m_front.compareOp);
						frontFaceStencil.stencilFailureOperation = static_cast<MTLStencilOperation>(pDepthStencilInfo->m_front.failOp);
						frontFaceStencil.depthFailureOperation = static_cast<MTLStencilOperation>(pDepthStencilInfo->m_front.depthFailOp);
						frontFaceStencil.depthStencilPassOperation = static_cast<MTLStencilOperation>(pDepthStencilInfo->m_front.passOp);
						frontFaceStencil.readMask = pDepthStencilInfo->m_front.compareMask;
						frontFaceStencil.writeMask = pDepthStencilInfo->m_front.writeMask;
						depthStencilDescriptor.frontFaceStencil = frontFaceStencil;
						m_stencilFrontReference = pDepthStencilInfo->m_front.reference;
					}

					{
						MTLStencilDescriptor* backFaceStencil = [[MTLStencilDescriptor alloc] init];
						backFaceStencil.stencilCompareFunction = ConvertCompareOperation(pDepthStencilInfo->m_back.compareOp);
						backFaceStencil.stencilFailureOperation = static_cast<MTLStencilOperation>(pDepthStencilInfo->m_back.failOp);
						backFaceStencil.depthFailureOperation = static_cast<MTLStencilOperation>(pDepthStencilInfo->m_back.depthFailOp);
						backFaceStencil.depthStencilPassOperation = static_cast<MTLStencilOperation>(pDepthStencilInfo->m_back.passOp);
						backFaceStencil.readMask = pDepthStencilInfo->m_back.compareMask;
						backFaceStencil.writeMask = pDepthStencilInfo->m_back.writeMask;
						depthStencilDescriptor.backFaceStencil = backFaceStencil;
						m_stencilBackReference = pDepthStencilInfo->m_back.reference;
					}
				}

				m_depthStencilState = [(id<MTLDevice>)logicalDevice newDepthStencilStateWithDescriptor:depthStencilDescriptor];
			}
			else
			{
				Assert(
					pRenderPassData->m_subpassDepthAttachmentIndices[(uint8)subpassIndex] == Internal::RenderPassData::InvalidAttachmentIndex,
					"Provided unused depth attachment"
				);
			}

			Assert(pGeometryStageInfo.IsInvalid(), "Not supported!");

			// TODO: pipeline cache
			// binaryArchives

			// TODO: Use the async callback
			NSError* error = nil;
			m_pipeline = [(id<MTLDevice>)logicalDevice newRenderPipelineStateWithDescriptor:pipelineStateDescriptor
																																							options:MTLPipelineOptionNone
																																					 reflection:nil
																																								error:&error];
			Assert(m_pipeline.IsValid());
#elif RENDERER_WEBGPU
			struct State
			{
				WGPUVertexState vertexState;
				WGPUDepthStencilState depthStencilState;
				WGPUFragmentState fragmentState;
				InlineVector<WGPUColorTargetState, 16> colorTargetStates;
				InlineVector<WGPUBlendState, 16> colorTargetBlendStates;

				InlineVector<WGPUVertexBufferLayout, 16> vertexBufferLayouts;
				InlineVector<WGPUVertexAttribute, 16> vertexAttributes;

#if !RENDERER_WEBGPU_DAWN
				WGPUPrimitiveDepthClipControl depthClipControl;
#endif

				WGPURenderPipelineDescriptor descriptor;
			};
			UniquePtr<State> pState = UniquePtr<State>::Make();
			State& state = *pState;
			state.vertexBufferLayouts.Reserve(vertexStage.m_bindingDescriptions.GetSize());
			state.vertexAttributes.Reserve(vertexStage.m_attributeDescriptions.GetSize());

			for (const VertexInputAttributeDescription& vertexAttributeDescription : vertexStage.m_attributeDescriptions)
			{
				state.vertexAttributes.EmplaceBack(WGPUVertexAttribute{
					ConvertFormatToVertexFormat(vertexAttributeDescription.format),
					vertexAttributeDescription.offset,
					vertexAttributeDescription.shaderLocation
				});
			}

			ArrayView<const VertexInputAttributeDescription> remainingVertexAttributeDescriptions = vertexStage.m_attributeDescriptions;

			for (const VertexInputBindingDescription& vertexBindingDescription : vertexStage.m_bindingDescriptions)
			{
				const uint32 bindingIndex = vertexStage.m_bindingDescriptions.GetIteratorIndex(&vertexBindingDescription);

				const ArrayView<const VertexInputAttributeDescription> bindingAttributeDescriptions =
					remainingVertexAttributeDescriptions.FindFirstContiguousRange(
						[bindingIndex](const VertexInputAttributeDescription& vertexAttributeDescription)
						{
							return vertexAttributeDescription.binding == bindingIndex;
						}
					);

				const uint32 firstAttributeIndex = vertexStage.m_attributeDescriptions.GetIteratorIndex(bindingAttributeDescriptions.GetData());

				const ArrayView<const WGPUVertexAttribute> bindingVertexAttibutes =
					state.vertexAttributes.GetSubView(firstAttributeIndex, bindingAttributeDescriptions.GetSize());

				remainingVertexAttributeDescriptions += bindingAttributeDescriptions.GetSize();

				auto getVertexStepMode = [](const VertexInputRate inputRate) -> WGPUVertexStepMode
				{
					switch (inputRate)
					{
						case VertexInputRate::Vertex:
							return WGPUVertexStepMode_Vertex;
						case VertexInputRate::Instance:
							return WGPUVertexStepMode_Instance;
					}
					ExpectUnreachable();
				};

				state.vertexBufferLayouts.EmplaceBack(WGPUVertexBufferLayout{
					vertexBindingDescription.stride,
					getVertexStepMode(vertexBindingDescription.inputRate),
					bindingVertexAttibutes.GetSize(),
					bindingVertexAttibutes.GetData()
				});
			}

			UNUSED(dynamicStateFlags);
			UNUSED(viewports);
			UNUSED(scissors);

			UNUSED(subpassIndex); // TODO: Not supported by WebGPU yet

			const Internal::RenderPassData* __restrict pRenderPassData{renderPass};

			state.vertexState = WGPUVertexState
			{
				nullptr, vertexShader,
#if RENDERER_WEBGPU_DAWN
					WGPUStringView{vertexStage.m_entryPointName, vertexStage.m_entryPointName.GetSize()},
#else
					vertexStage.m_entryPointName,
#endif
					0, nullptr, state.vertexBufferLayouts.GetSize(), state.vertexBufferLayouts.GetData()
			};

			if (pDepthStencilInfo.IsValid())
			{
				const DepthStencilInfo depthStencilInfo = *pDepthStencilInfo;
				if (depthStencilInfo.m_flags.IsSet(DepthStencilFlags::StencilTest))
				{
					Assert(depthStencilInfo.m_front.reference == depthStencilInfo.m_back.reference, "Per-face stencil reference is not supported!");
					m_stencilTest = true;
					m_stencilReference = depthStencilInfo.m_front.reference;
				}

				const auto convertStencilOperation = [](const StencilOperation operation)
				{
					switch (operation)
					{
						case StencilOperation::Keep:
							return WGPUStencilOperation_Keep;
						case StencilOperation::Zero:
							return WGPUStencilOperation_Zero;
						case StencilOperation::Replace:
							return WGPUStencilOperation_Replace;
					}
					ExpectUnreachable();
				};

				state.depthStencilState = WGPUDepthStencilState
				{
					nullptr, ConvertFormat(pRenderPassData->m_attachmentDescriptions.GetLastElement().m_format),
#if RENDERER_WEBGPU_DAWN
						depthStencilInfo.m_flags.IsSet(DepthStencilFlags::DepthWrite) ? WGPUOptionalBool_True : WGPUOptionalBool_False,
#else
						depthStencilInfo.m_flags.IsSet(DepthStencilFlags::DepthWrite),
#endif
						ConvertCompareOperation(depthStencilInfo.m_depthCompareOperation),
						WGPUStencilFaceState{
							ConvertCompareOperation(depthStencilInfo.m_front.compareOp),
							convertStencilOperation(depthStencilInfo.m_front.failOp),
							convertStencilOperation(depthStencilInfo.m_front.depthFailOp),
							convertStencilOperation(depthStencilInfo.m_front.passOp)
						},
						WGPUStencilFaceState{
							ConvertCompareOperation(depthStencilInfo.m_back.compareOp),
							convertStencilOperation(depthStencilInfo.m_back.failOp),
							convertStencilOperation(depthStencilInfo.m_back.depthFailOp),
							convertStencilOperation(depthStencilInfo.m_back.passOp)
						},
						depthStencilInfo.m_front.compareMask | depthStencilInfo.m_back.compareMask,
						depthStencilInfo.m_front.writeMask | depthStencilInfo.m_back.writeMask, primitiveInfo.depthBiasConstantFactor,
						primitiveInfo.depthBiasSlopeFactor, primitiveInfo.depthBiasClamp
				};
			}

			if (pFragmentStageInfo.IsValid())
			{
				const FragmentStageInfo fragmentStageInfo = *pFragmentStageInfo;

				const Internal::RenderPassData::SubpassAttachmentIndices& __restrict subpassColorAttachmentIndices =
					pRenderPassData->m_subpassColorAttachmentIndices[(uint8)subpassIndex];

				state.colorTargetStates.Reserve(fragmentStageInfo.m_colorTargets.GetSize());
				state.colorTargetBlendStates.Reserve(fragmentStageInfo.m_colorTargets.GetSize());
				for (const ColorTargetInfo& __restrict colorTargetInfo : fragmentStageInfo.m_colorTargets)
				{
					Optional<WGPUBlendState*> pBlendState = nullptr;
					if (colorTargetInfo.colorBlendState.IsEnabled() | colorTargetInfo.alphaBlendState.IsEnabled())
					{
						auto convertBlendOperation = [](const BlendOperation blendOperation)
						{
							switch (blendOperation)
							{
								case BlendOperation::Add:
									return WGPUBlendOperation_Add;
								case BlendOperation::Subtract:
									return WGPUBlendOperation_Subtract;
								case BlendOperation::ReverseSubtract:
									return WGPUBlendOperation_ReverseSubtract;
								case BlendOperation::Minimum:
									return WGPUBlendOperation_Min;
								case BlendOperation::Maximum:
									return WGPUBlendOperation_Max;
							}
							ExpectUnreachable();
						};

						pBlendState = state.colorTargetBlendStates.EmplaceBack(WGPUBlendState{
							WGPUBlendComponent{
								convertBlendOperation(colorTargetInfo.colorBlendState.blendOperation),
								ConvertBlendFactor(colorTargetInfo.colorBlendState.sourceBlendFactor),
								ConvertBlendFactor(colorTargetInfo.colorBlendState.targetBlendFactor)
							},
							WGPUBlendComponent{
								convertBlendOperation(colorTargetInfo.alphaBlendState.blendOperation),
								ConvertBlendFactor(colorTargetInfo.alphaBlendState.sourceBlendFactor),
								ConvertBlendFactor(colorTargetInfo.alphaBlendState.targetBlendFactor)
							}
						});
					}

					const uint8 colorTargetIndex = fragmentStageInfo.m_colorTargets.GetIteratorIndex(&colorTargetInfo);
					const uint8 colorAttachmentIndex = (uint8)subpassColorAttachmentIndices[colorTargetIndex];

					state.colorTargetStates.EmplaceBack(WGPUColorTargetState{
						nullptr,
						ConvertFormat(pRenderPassData->m_attachmentDescriptions[colorAttachmentIndex].m_format),
						pBlendState,
						static_cast<WGPUColorWriteMask>(colorTargetInfo.colorWriteMask.GetFlags())
					});
				}

				state.fragmentState = WGPUFragmentState
				{
					nullptr, fragmentShader,
#if RENDERER_WEBGPU_DAWN
						WGPUStringView{fragmentStageInfo.m_entryPointName, fragmentStageInfo.m_entryPointName.GetSize()},
#else
						fragmentStageInfo.m_entryPointName,
#endif
						0, nullptr, state.colorTargetStates.GetSize(), state.colorTargetStates.GetData()
				};
			}

			UNUSED(pMultisamplingInfo);
			Assert(pMultisamplingInfo.IsInvalid() || pMultisamplingInfo->rasterizationSamples == SampleCount::One);

			UNUSED(pGeometryStageInfo);
			Assert(pGeometryStageInfo.IsInvalid(), "Geometry shader not supported!");

			Assert(primitiveInfo.flags.IsNotSet(PrimitiveFlags::RasterizerDiscard), "Not supported!");

			const auto getTopology = [](const PrimitiveTopology topology)
			{
				switch (topology)
				{
					case PrimitiveTopology::TriangleList:
						return WGPUPrimitiveTopology_TriangleList;
				}
				ExpectUnreachable();
			};

			const auto getCullMode = [](const EnumFlags<CullMode> cullModeFlags)
			{
				Assert(cullModeFlags.GetNumberOfSetFlags() <= 1);
				switch (cullModeFlags.GetFlags())
				{
					case CullMode::None:
						return WGPUCullMode_None;
					case CullMode::Front:
						return WGPUCullMode_Front;
					case CullMode::Back:
						return WGPUCullMode_Back;
					case CullMode::FrontAndBack:
						Assert(false, "Not supported by WebGPU!");
						return WGPUCullMode_Front;
				}
				ExpectUnreachable();
			};

#if !RENDERER_WEBGPU_DAWN
			state.depthClipControl = WGPUPrimitiveDepthClipControl{
				WGPUChainedStruct{
					nullptr,
					WGPUSType_PrimitiveDepthClipControl,
				},
				primitiveInfo.flags.IsSet(PrimitiveFlags::DepthClamp)
			};
#endif

			state.descriptor = WGPURenderPipelineDescriptor
			{
				nullptr,
#if RENDERER_WEBGPU_DAWN
					WGPUStringView{nullptr, 0},
#else
					nullptr,
#endif
					pipelineLayout, state.vertexState, WGPUPrimitiveState
				{
#if RENDERER_WEBGPU_DAWN
					nullptr,
#else
					&state.depthClipControl.chain,
#endif
						getTopology(primitiveInfo.topology),
						WGPUIndexFormat_Undefined, // Math::Select(TypeTraits::IsSame<Index, uint32>, WGPUIndexFormat_Uint32, WGPUIndexFormat_Uint16),
						primitiveInfo.windingOrder == WindingOrder::CounterClockwise ? WGPUFrontFace_CCW : WGPUFrontFace_CW,
						getCullMode(primitiveInfo.cullMode.GetFlags()),
#if RENDERER_WEBGPU_DAWN
						primitiveInfo.flags.IsNotSet(PrimitiveFlags::DepthClamp)
#endif
				}
				, pDepthStencilInfo.IsValid() ? &state.depthStencilState : nullptr, WGPUMultisampleState{nullptr, 1, ~0u, false},
					pFragmentStageInfo.IsValid() ? &state.fragmentState : nullptr
			};

#if 1 //! RENDERER_WEBGPU_DAWN // wgpu-native doesn't support async yet.
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[this, &logicalDevice, pipelineLayout, pState = Move(pState)]()
				{
					pState->descriptor.layout = pipelineLayout;
					m_pipeline = wgpuDeviceCreateRenderPipeline(logicalDevice, &pState->descriptor);
#if RENDERER_WEBGPU_DAWN
					wgpuRenderPipelineAddRef(m_pipeline);
#else
					wgpuRenderPipelineReference(m_pipeline);
#endif
				}
			);
#else
			m_pipeline = wgpuDeviceCreateRenderPipeline(logicalDevice, &state.descriptor);
			wgpuRenderPipelineReference(m_pipeline);
#endif
#else
#if RENDERER_WEBGPU_DAWN
			auto callback = [](
												const WGPUCreatePipelineAsyncStatus status,
												WGPURenderPipeline pipeline,
												[[maybe_unused]] const WGPUStringView message,
												void* pUserData
											)
#else
			auto callback =
				[](const WGPUCreatePipelineAsyncStatus status, WGPURenderPipeline pipeline, [[maybe_unused]] const char* message, void* pUserData)
#endif
			{
				Assert(status == WGPUCreatePipelineAsyncStatus_Success);
				if (LIKELY(status == WGPUCreatePipelineAsyncStatus_Success))
				{
					GraphicsPipeline& graphicsPipeline = *reinterpret_cast<GraphicsPipeline*>(pUserData);
#if RENDERER_WEBGPU_DAWN
					wgpuRenderPipelineAddRef(pipeline);
#else
					wgpuRenderPipelineReference(pipeline);
#endif
					graphicsPipeline.m_pipeline = pipeline;
				}
			};

			wgpuDeviceCreateRenderPipelineAsync(logicalDevice, &state.descriptor, callback, this);
#endif
#endif
		}
	}

	ComputePipeline& ComputePipeline::operator=(ComputePipeline&& other)
	{
		GraphicsPipelineBase::operator=(Forward<GraphicsPipelineBase>(other));
		Assert(!m_pipeline.IsValid(), "Destroy must have been called!");
		m_pipeline = other.m_pipeline;
		other.m_pipeline = {};
		return *this;
	}

	void ComputePipeline::Destroy(LogicalDevice& logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroyPipeline(logicalDevice, m_pipeline, nullptr);
		m_pipeline = {};
#elif RENDERER_METAL
		m_pipeline = {};
#elif RENDERER_WEBGPU
		if (m_pipeline.IsValid())
		{
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pipeline = m_pipeline]()
				{
					wgpuComputePipelineRelease(pipeline);
				}
			);
#else
			wgpuComputePipelineRelease(m_pipeline);
#endif
			m_pipeline = {};
		}
#endif

		GraphicsPipelineBase::Destroy(logicalDevice);
	}

	void ComputePipeline::SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name)
	{
		m_pipeline.SetDebugName(logicalDevice, name);
	}

	void ComputePipeline::CreateBase(
		LogicalDevice& logicalDevice,
		const ArrayView<const DescriptorSetLayoutView, uint8> descriptorSetLayouts,
		const ArrayView<const PushConstantRange, uint8> pushConstantRanges
	)
	{
		GraphicsPipelineBase::Create(logicalDevice, descriptorSetLayouts, pushConstantRanges);
	}

	Threading::JobBatch ComputePipeline::CreateAsync(
		const LogicalDeviceView logicalDevice,
		ShaderCache& shaderCache,
		const ShaderStageInfo computeShaderInfo,
		const PipelineLayoutView pipelineLayout
	)
	{
		struct Data
		{
			const LogicalDeviceView logicalDevice;
			ShaderCache& shaderCache;
			const ShaderStageInfo computeShaderInfo;
			const PipelineLayoutView pipelineLayout;
		};
		UniquePtr<Data> pData{Memory::ConstructInPlace, logicalDevice, shaderCache, computeShaderInfo, pipelineLayout};

		Threading::JobBatch jobBatch;
		{
			const Optional<Threading::Job*> pComputeShaderLoadJob = shaderCache.FindOrLoad(
				computeShaderInfo.m_assetGuid,
				ShaderStage::Compute,
				computeShaderInfo.m_entryPointName,
				m_pipelineLayout,
				ShaderCache::StageInfo{},
				ShaderCache::ShaderRequesters::ListenerData{
					this,
					[](ComputePipeline&, const ShaderView)
					{
						return EventCallbackResult::Remove;
					}
				}
			);
			if (pComputeShaderLoadJob.IsValid())
			{
				Assert(jobBatch.IsInvalid());
				jobBatch = {*pComputeShaderLoadJob, Threading::JobBatch::IntermediateStage};
			}
		}

		jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
			[this, pData = Move(pData)](Threading::JobRunnerThread&)
			{
				Data& __restrict data = *pData;
				CreateInternal(data.logicalDevice, data.shaderCache, data.computeShaderInfo, data.pipelineLayout);
			},
			Threading::JobPriority::LoadGraphicsPipeline
		));
		return jobBatch;
	}

	void ComputePipeline::Create(
		const LogicalDeviceView logicalDevice,
		ShaderCache& shaderCache,
		const ShaderStageInfo computeShaderInfo,
		const PipelineLayoutView pipelineLayout
	)
	{
		Threading::JobBatch jobBatch = CreateAsync(logicalDevice, shaderCache, computeShaderInfo, pipelineLayout);
		Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
		thread.Queue(jobBatch);
	}

	void ComputePipeline::CreateInternal(
		const LogicalDeviceView logicalDevice,
		ShaderCache& shaderCache,
		const ShaderStageInfo computeShaderInfo,
		[[maybe_unused]] const PipelineLayoutView layout
	)
	{
		const ShaderView computeShader = shaderCache.GetAssetData(shaderCache.FindIdentifier(computeShaderInfo.m_assetGuid));
		Assert(computeShader.IsValid());
		if (LIKELY(computeShader.IsValid()))
		{
#if RENDERER_VULKAN
			const VkComputePipelineCreateInfo creationInfo{
				VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
				nullptr,
				VkPipelineCreateFlags{0},
				VkPipelineShaderStageCreateInfo{
					VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
					nullptr,
					VkPipelineShaderStageCreateFlags{0},
					VK_SHADER_STAGE_COMPUTE_BIT,
					computeShader,
					computeShaderInfo.m_entryPointName,
					nullptr
				},
				layout,
				nullptr,
				-1
			};

			VkPipeline pipeline;
			[[maybe_unused]] const VkResult graphicsPipelineCreationResult =
				vkCreateComputePipelines(logicalDevice, shaderCache.GetPipelineCache(), 1, &creationInfo, nullptr, &pipeline);
			Assert(graphicsPipelineCreationResult == VK_SUCCESS);
			m_pipeline = pipeline;
#elif RENDERER_METAL
			MTLComputePipelineDescriptor* pipelineStateDescriptor = [[MTLComputePipelineDescriptor alloc] init];

			pipelineStateDescriptor.computeFunction = computeShader;
			// TODO
			pipelineStateDescriptor.threadGroupSizeIsMultipleOfThreadExecutionWidth = false;

			NSError* error = nil;
			m_pipeline = [(id<MTLDevice>)logicalDevice newComputePipelineStateWithDescriptor:pipelineStateDescriptor
																																							 options:MTLPipelineOptionNone
																																						reflection:nil
																																								 error:&error];
#elif RENDERER_WEBGPU
#if RENDERER_WEBGPU_DAWN
			const WGPUComputePipelineDescriptor descriptor{
				nullptr,
				WGPUStringView{nullptr, 0},
				layout,
				WGPUComputeState{
					nullptr,
					computeShader,
					WGPUStringView{computeShaderInfo.m_entryPointName, computeShaderInfo.m_entryPointName.GetSize()},
					0,
					nullptr
				}
			};
#else
			const WGPUComputePipelineDescriptor descriptor{
				nullptr,
				nullptr,
				layout,
				WGPUProgrammableStageDescriptor{nullptr, computeShader, computeShaderInfo.m_entryPointName, 0, nullptr}
			};
#endif

#if 1 //! RENDERER_WEBGPU_DAWN // wgpu-native doesn't support async yet.
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[this, logicalDevice, descriptor]()
				{
					m_pipeline = wgpuDeviceCreateComputePipeline(logicalDevice, &descriptor);
#if RENDERER_WEBGPU_DAWN
					wgpuComputePipelineAddRef(m_pipeline);
#else
					wgpuComputePipelineReference(m_pipeline);
#endif
				}
			);
#else
			m_pipeline = wgpuDeviceCreateComputePipeline(logicalDevice, &descriptor);
			wgpuComputePipelineReference(m_pipeline);
#endif
#else

#if RENDERER_WEBGPU_DAWN
			auto callback = [](
												const WGPUCreatePipelineAsyncStatus status,
												WGPUComputePipeline pipeline,
												[[maybe_unused]] const WGPUStringView message,
												void* pUserData
											)
#else
			auto callback =
				[](const WGPUCreatePipelineAsyncStatus status, WGPUComputePipeline pipeline, [[maybe_unused]] const char* message, void* pUserData)
#endif
			{
				Assert(status == WGPUCreatePipelineAsyncStatus_Success);
				if (LIKELY(status == WGPUCreatePipelineAsyncStatus_Success))
				{
					ComputePipeline& computePipeline = *reinterpret_cast<ComputePipeline*>(pUserData);
#if RENDERER_WEBGPU_DAWN
					wgpuComputePipelineAddRef(pipeline);
#else
					wgpuComputePipelineReference(pipeline);
#endif
					computePipeline.m_pipeline = pipeline;
				}
			};
			wgpuDeviceCreateComputePipelineAsync(logicalDevice, &descriptor, callback, this);
#endif
#endif
		}
	}

	void GraphicsPipeline::PrepareForResize([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroyPipeline(logicalDevice, m_pipeline, nullptr);
		m_pipeline = {};
#elif RENDERER_METAL
		m_pipeline = {};
#elif RENDERER_WEBGPU
		if (m_pipeline.IsValid())
		{
#if WEBGPU_SINGLE_THREADED
			Rendering::Window::QueueOnWindowThreadOrExecuteImmediately(
				[pipeline = m_pipeline]()
				{
					wgpuRenderPipelineRelease(pipeline);
				}
			);
#else
			wgpuRenderPipelineRelease(m_pipeline);
#endif
			m_pipeline = {};
		}
#endif
	}

	void GraphicsPipeline::PushConstants(
		[[maybe_unused]] LogicalDevice& logicalDevice,
		[[maybe_unused]] const RenderCommandEncoderView renderCommandEncoder,
		[[maybe_unused]] const ArrayView<const PushConstantRange, uint8> pushConstantRanges,
		const ConstByteView data
	) const
	{
		if constexpr (ENABLE_ASSERTS)
		{
			for ([[maybe_unused]] const PushConstantRange pushConstantRange : pushConstantRanges)
			{
				Assert(data.GetSubViewFrom(pushConstantRange.m_range.GetMinimum()).GetDataSize() >= pushConstantRange.m_range.GetSize());
			}
		}

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		const uint32 pushConstantsInstanceOffset = logicalDevice.AcquirePushConstantsInstance(data);
		renderCommandEncoder.BindDynamicDescriptorSets(
			*this,
			ArrayView<const DescriptorSetView>{m_pushConstantsDescriptorSet},
			ArrayView<const uint32>{pushConstantsInstanceOffset}
		);

#elif RENDERER_METAL
		const Internal::PipelineLayoutData* __restrict pPipelineLayoutData = m_pipelineLayout;

		for (const PushConstantRange pushConstantRange : pushConstantRanges)
		{
			for (const ShaderStage shaderStage : pushConstantRange.m_shaderStages)
			{
				const uint8 shaderStageIndex = GetShaderStageIndex(shaderStage);
				uint8 pushConstantsBufferIndex = pPipelineLayoutData->m_pushConstantOffsets[shaderStageIndex];

				const Math::Range<uint16> pushConstantDataRange = pPipelineLayoutData->m_stagePushConstantDataRanges[shaderStageIndex];

				Assert(data.GetSubViewFrom(pushConstantDataRange.GetMinimum()).GetDataSize() >= pushConstantDataRange.GetSize());
				switch (shaderStage)
				{
					case ShaderStage::Vertex:
						pushConstantsBufferIndex += pPipelineLayoutData->m_vertexBufferCount;
						[(id<MTLRenderCommandEncoder>)renderCommandEncoder setVertexBytes:data.GetData() + pushConstantDataRange.GetMinimum()
																																			 length:pushConstantDataRange.GetSize()
																																			atIndex:pushConstantsBufferIndex];
						break;
					case ShaderStage::Fragment:
						[(id<MTLRenderCommandEncoder>)renderCommandEncoder setFragmentBytes:data.GetData() + pushConstantDataRange.GetMinimum()
																																				 length:pushConstantDataRange.GetSize()
																																				atIndex:pushConstantsBufferIndex];
						break;
					case ShaderStage::Compute:
						[(id<MTLRenderCommandEncoder>)renderCommandEncoder setTileBytes:data.GetData() + pushConstantDataRange.GetMinimum()
																																		 length:pushConstantDataRange.GetSize()
																																		atIndex:pushConstantsBufferIndex];
						break;

					case Rendering::ShaderStage::TessellationControl:
					case Rendering::ShaderStage::TessellationEvaluation:
						Assert(false, "Unsupported");
						break;

					case Rendering::ShaderStage::RayGeneration:
					case Rendering::ShaderStage::RayAnyHit:
					case Rendering::ShaderStage::RayClosestHit:
					case Rendering::ShaderStage::RayIntersection:
					case Rendering::ShaderStage::RayMiss:
					case Rendering::ShaderStage::RayCallable:
						Assert(false, "Unsupported");
						break;

					case ShaderStage::Geometry:
					case ShaderStage::Invalid:
					case ShaderStage::Count:
					case ShaderStage::All:
						ExpectUnreachable();
				}
			}
		}
#elif RENDERER_VULKAN
		Assert(m_pipelineLayout != 0);
		for (const PushConstantRange pushConstantRange : pushConstantRanges)
		{
			vkCmdPushConstants(
				renderCommandEncoder,
				m_pipelineLayout,
				static_cast<VkShaderStageFlags>(pushConstantRange.m_shaderStages.GetUnderlyingValue()),
				pushConstantRange.m_range.GetMinimum(),
				pushConstantRange.m_range.GetSize(),
				data.GetData() + pushConstantRange.m_range.GetMinimum()
			);
		}
#endif
	}

	void ComputePipeline::PushConstants(
		[[maybe_unused]] LogicalDevice& logicalDevice,
		[[maybe_unused]] const ComputeCommandEncoderView computeCommandEncoder,
		[[maybe_unused]] const ArrayView<const PushConstantRange, uint8> pushConstantRanges,
		const ConstByteView data
	) const
	{
		Assert(pushConstantRanges.All(
			[](const PushConstantRange pushConstantRange)
			{
				return pushConstantRange.m_shaderStages == ShaderStage::Compute;
			}
		));

		if constexpr (ENABLE_ASSERTS)
		{
			for ([[maybe_unused]] const PushConstantRange pushConstantRange : pushConstantRanges)
			{
				Assert(data.GetSubViewFrom(pushConstantRange.m_range.GetMinimum()).GetDataSize() >= pushConstantRange.m_range.GetSize());
			}
		}

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		const uint32 pushConstantsInstanceOffset = logicalDevice.AcquirePushConstantsInstance(data);
		computeCommandEncoder.BindDynamicDescriptorSets(
			*this,
			ArrayView<const DescriptorSetView>{m_pushConstantsDescriptorSet},
			ArrayView<const uint32>{pushConstantsInstanceOffset}
		);

#elif RENDERER_METAL
		const Internal::PipelineLayoutData* __restrict pPipelineLayoutData = m_pipelineLayout;
		for (const PushConstantRange pushConstantRange : pushConstantRanges)
		{
			constexpr uint8 shaderStageIndex = (uint8)Math::Log2((uint32)ShaderStage::Compute);

			const uint8 pushConstantsBufferIndex = pPipelineLayoutData->m_pushConstantOffsets[shaderStageIndex];
			const Math::Range<uint16> pushConstantDataRange = pPipelineLayoutData->m_stagePushConstantDataRanges[shaderStageIndex];

			Assert(data.GetSubViewFrom(pushConstantDataRange.GetMinimum()).GetDataSize() >= pushConstantDataRange.GetSize());
			[(id<MTLComputeCommandEncoder>)computeCommandEncoder setBytes:data.GetData() + pushConstantDataRange.GetMinimum()
																														 length:pushConstantDataRange.GetSize()
																														atIndex:pushConstantsBufferIndex];
		}
#elif RENDERER_VULKAN
		Assert(m_pipelineLayout != 0);
		for (const PushConstantRange pushConstantRange : pushConstantRanges)
		{
			vkCmdPushConstants(
				computeCommandEncoder,
				m_pipelineLayout,
				VK_SHADER_STAGE_COMPUTE_BIT,
				pushConstantRange.m_range.GetMinimum(),
				pushConstantRange.m_range.GetSize(),
				data.GetData() + pushConstantRange.m_range.GetMinimum()
			);
		}
#endif
	}
}
