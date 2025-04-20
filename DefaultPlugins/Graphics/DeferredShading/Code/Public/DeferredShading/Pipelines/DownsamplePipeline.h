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
	struct ComputeCommandEncoderView;
	struct ShaderCache;
	struct RenderPassView;
	struct RenderMeshView;
	struct TransformBufferView;
	struct ViewMatrices;

	struct InstanceGroupPerLogicalDeviceData;

	struct DownsamplePipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		DownsamplePipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache);
		DownsamplePipeline(const DownsamplePipeline&) = delete;
		DownsamplePipeline& operator=(const DownsamplePipeline&) = delete;
		DownsamplePipeline(DownsamplePipeline&& other) = default;
		DownsamplePipeline& operator=(DownsamplePipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return ComputePipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		void Compute(
			ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
			const ComputeCommandEncoderView computeCommandEncoder,
			const Math::Vector2ui resolution
		) const;
	};
}
