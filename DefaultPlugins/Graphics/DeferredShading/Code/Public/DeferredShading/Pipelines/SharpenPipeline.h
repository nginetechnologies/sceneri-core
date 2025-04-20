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
	struct RenderCommandEncoderView;
	struct ShaderCache;
	struct RenderPassView;
	struct RenderMeshView;
	struct TransformBufferView;
	struct ViewMatrices;

	struct InstanceGroupPerLogicalDeviceData;

	struct SharpenPipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		SharpenPipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache);
		SharpenPipeline(const SharpenPipeline&) = delete;
		SharpenPipeline& operator=(const SharpenPipeline&) = delete;
		SharpenPipeline(SharpenPipeline&& other) = default;
		SharpenPipeline& operator=(SharpenPipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return ComputePipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		void ComputeRCASConstants();
		void Compute(
			Rendering::LogicalDevice& logicalDevice,
			ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
			const ComputeCommandEncoderView computeCommandEncoder,
			const Math::Vector2ui& displayResolution
		) const;
	};
}
