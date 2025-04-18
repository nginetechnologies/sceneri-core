#pragma once

#include <Renderer/Pipelines/GraphicsPipeline.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>

namespace ngine::Rendering
{
	struct GraphicsPipeline : protected DescriptorSetLayout, public GraphicsPipeline
	{
		GraphicsPipeline() = default;
		GraphicsPipeline(
			const LogicalDeviceView logicalDevice,
			const ArrayView<const DescriptorSetLayout::Binding, uint8> descriptorBindings,
			const ArrayView<const PushConstantRange, uint8> pushConstantRanges = {}
		);
		GraphicsPipeline(GraphicsPipeline&&) = default;
		GraphicsPipeline& operator=(GraphicsPipeline&&) = default;

		void Destroy(LogicalDevice& logicalDevice);

		[[nodiscard]] DescriptorSetLayoutView GetDescriptorSetLayout() const
		{
			return *this;
		}

		[[nodiscard]] bool IsDescriptorSetLayoutValid() const
		{
			return DescriptorSetLayout::IsValid();
		}

		[[nodiscard]] bool IsValid() const
		{
			return GraphicsPipeline::IsValid();
		}
	};
}
