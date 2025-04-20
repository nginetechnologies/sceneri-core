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

	struct SuperResolutionPipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		SuperResolutionPipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache);
		SuperResolutionPipeline(const SuperResolutionPipeline&) = delete;
		SuperResolutionPipeline& operator=(const SuperResolutionPipeline&) = delete;
		SuperResolutionPipeline(SuperResolutionPipeline&& other) = default;
		SuperResolutionPipeline& operator=(SuperResolutionPipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return ComputePipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		void ComputeEASUConstants(const Math::Vector2ui renderResolution, const Math::Vector2ui displayResolution);
		void Compute(
			Rendering::LogicalDevice& logicalDevice,
			ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
			const ComputeCommandEncoderView computeCommandEncoder,
			const Math::Vector2ui& displayResolution
		) const;
	};
}
