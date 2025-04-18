#pragma once

#include "PipelineLayoutView.h"

#include <Renderer/Constants.h>
#include <Renderer/ShaderStage.h>
#include <Renderer/Descriptors/DescriptorSetLayoutView.h>

#include <Common/Math/Range.h>
#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/EnumFlags.h>
#include <Common/Memory/Containers/ForwardDeclarations/ZeroTerminatedStringView.h>

namespace ngine::Rendering
{
#if RENDERER_METAL
	namespace Internal
	{
		struct PipelineLayoutData
		{
			uint8 m_vertexBufferCount{0};
			InlineVector<DescriptorSetLayoutView, 2, uint8> m_descriptorSetLayouts;
			Array<uint8, (uint8)ShaderStage::Count, uint8> m_pushConstantOffsets{Memory::Zeroed};

			EnumFlags<ShaderStage> m_pushConstantsStages;
			Array<Math::Range<uint16>, (uint8)ShaderStage::Count> m_stagePushConstantDataRanges;
		};
	}
#endif

	struct LogicalDevice;
	struct LogicalDeviceView;
	struct DescriptorSetLayoutView;
	struct PushConstantRange;

	struct PipelineLayout : public PipelineLayoutView
	{
		PipelineLayout() = default;
		PipelineLayout(
			const LogicalDeviceView logicalDevice,
			const ArrayView<const DescriptorSetLayoutView, uint8> descriptorSetLayouts = {},
			const ArrayView<const PushConstantRange, uint8> pushConstantRanges = {}
		);
		PipelineLayout(const PipelineLayout&) = delete;
		PipelineLayout& operator=(const PipelineLayout&) = delete;
		PipelineLayout(PipelineLayout&& other)
			: PipelineLayoutView(other)
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			other.m_pPipelineLayout = 0;
#endif
		}
		PipelineLayout& operator=(PipelineLayout&& other);
		~PipelineLayout();

		void Destroy(const LogicalDeviceView logicalDevice);

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name);
	};
}
