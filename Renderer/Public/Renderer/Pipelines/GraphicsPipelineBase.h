#pragma once

#include <Renderer/Wrappers/PipelineLayout.h>
#include <Renderer/Assets/Shader/ShaderView.h>
#include <Renderer/Constants.h>

#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
#include <Renderer/Descriptors/DescriptorSetLayout.h>
#include <Renderer/Descriptors/DescriptorSet.h>

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
}

#include <Common/Threading/AtomicPtr.h>
#endif

#include <Common/Asset/Guid.h>
#include <Common/Platform/ForceInline.h>
#include <Common/Memory/Containers/ZeroTerminatedStringView.h>
#include <Common/Memory/Containers/ByteView.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct PhysicalDevice;
	struct DeviceMemoryPool;
	struct DescriptorSetView;
	struct DescriptorSetLayoutView;
	struct ShaderCache;

	struct PushConstantRange;

	struct ShaderStageInfo
	{
		Asset::Guid m_assetGuid;
		ConstZeroTerminatedStringView m_entryPointName{"main0"};
	};

	struct GraphicsPipelineBase
	{
		GraphicsPipelineBase() = default;
		GraphicsPipelineBase(const GraphicsPipelineBase&) = delete;
		GraphicsPipelineBase& operator=(const GraphicsPipelineBase&) = delete;
		GraphicsPipelineBase(GraphicsPipelineBase&& other)
#if RENDERER_SUPPORTS_PUSH_CONSTANTS
			: m_pipelineLayout(Move(other.m_pipelineLayout))
#else
			: m_pushConstantsDescriptorLayout(Move(other.m_pushConstantsDescriptorLayout))
			, m_pipelineLayout(Move(other.m_pipelineLayout))
			, m_pushConstantsDescriptorSet(Move(other.m_pushConstantsDescriptorSet))
			, m_pPushConstantsDescriptorSetLoadingThread(other.m_pPushConstantsDescriptorSetLoadingThread)
#endif
		{
		}
		GraphicsPipelineBase& operator=(GraphicsPipelineBase&& other);

		[[nodiscard]] operator PipelineLayoutView() const
		{
			return m_pipelineLayout;
		}

		[[nodiscard]] uint32 GetFirstDescriptorSetIndex() const
		{
#if RENDERER_SUPPORTS_PUSH_CONSTANTS
			return 0;
#else
			return m_pushConstantsDescriptorSet.IsValid() ? 1 : 0;
#endif
		}

		void Create(
			LogicalDevice& logicalDevice,
			const ArrayView<const DescriptorSetLayoutView, uint8> descriptorSetLayouts,
			const ArrayView<const PushConstantRange, uint8> pushConstantRanges
		);

		void Destroy(LogicalDevice& logicalDevice);
	protected:
#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		DescriptorSetLayout m_pushConstantsDescriptorLayout;
#endif
		PipelineLayout m_pipelineLayout;
#if !RENDERER_SUPPORTS_PUSH_CONSTANTS
		DescriptorSet m_pushConstantsDescriptorSet;
		Threading::Atomic<Threading::EngineJobRunnerThread*> m_pPushConstantsDescriptorSetLoadingThread = nullptr;
#endif
	};
}
