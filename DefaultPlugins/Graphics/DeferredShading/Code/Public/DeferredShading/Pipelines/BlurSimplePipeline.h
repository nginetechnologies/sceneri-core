#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>
#include <Common/Math/ForwardDeclarations/Matrix4x4.h>
#include <Common/Math/ForwardDeclarations/Matrix3x3.h>

#include <Renderer/Pipelines/ComputePipeline.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct ShaderCache;
	struct RenderPassView;
	struct RenderMeshView;
	struct TransformBufferView;
	struct ViewMatrices;

	struct InstanceGroupPerLogicalDeviceData;

	struct BlurSimplePipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		BlurSimplePipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache);
		BlurSimplePipeline(const BlurSimplePipeline&) = delete;
		BlurSimplePipeline& operator=(const BlurSimplePipeline&) = delete;
		BlurSimplePipeline(BlurSimplePipeline&& other) = default;
		BlurSimplePipeline& operator=(BlurSimplePipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return ComputePipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		void Compute(
			Rendering::LogicalDevice& logicalDevice,
			ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
			const ComputeCommandEncoderView computeCommandEncoder,
			const Math::Vector2ui resolution,
			const Math::Vector2i direction
		) const;
	};
}
