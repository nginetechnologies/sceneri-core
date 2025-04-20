#pragma once

#include <DeferredShading/Features.h>

#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS

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
	struct ViewMatrices;

	struct SDSMPhase1Pipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		SDSMPhase1Pipeline(
			Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache, const DescriptorSetLayoutView viewInfoDescriptorSetLayout
		);
		SDSMPhase1Pipeline(const SDSMPhase1Pipeline&) = delete;
		SDSMPhase1Pipeline& operator=(const SDSMPhase1Pipeline&) = delete;
		SDSMPhase1Pipeline(SDSMPhase1Pipeline&& other) = default;
		SDSMPhase1Pipeline& operator=(SDSMPhase1Pipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return ComputePipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		void Compute(
			Rendering::LogicalDevice& logicalDevice,
			ArrayView<const DescriptorSetView, uint8> descriptorSets,
			const ComputeCommandEncoderView computeCommandEncoder,
			const uint32 directionalLightCount,
			const Math::Vector2ui smallestDepthMinMaxResolution
		) const;
	};

	struct SDSMPhase2Pipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		SDSMPhase2Pipeline(
			Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache, const DescriptorSetLayoutView viewInfoDescriptorSetLayout
		);
		SDSMPhase2Pipeline(const SDSMPhase2Pipeline&) = delete;
		SDSMPhase2Pipeline& operator=(const SDSMPhase2Pipeline&) = delete;
		SDSMPhase2Pipeline(SDSMPhase2Pipeline&& other) = default;
		SDSMPhase2Pipeline& operator=(SDSMPhase2Pipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return ComputePipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		void Compute(
			Rendering::LogicalDevice& logicalDevice,
			ArrayView<const DescriptorSetView, uint8> descriptorSets,
			const ComputeCommandEncoderView computeCommandEncoder,
			uint8 directionalLightIndex,
			Math::Vector2ui depthBufferResolution
		) const;
	};

	struct SDSMPhase3Pipeline final : public DescriptorSetLayout, public ComputePipeline
	{
		SDSMPhase3Pipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache);
		SDSMPhase3Pipeline(const SDSMPhase3Pipeline&) = delete;
		SDSMPhase3Pipeline& operator=(const SDSMPhase3Pipeline&) = delete;
		SDSMPhase3Pipeline(SDSMPhase3Pipeline&& other) = default;
		SDSMPhase3Pipeline& operator=(SDSMPhase3Pipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return ComputePipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);
		void Compute(
			ArrayView<const DescriptorSetView, uint8> descriptorSets,
			const ComputeCommandEncoderView computeCommandEncoder,
			uint32 directionalLightCount
		) const;
	};

}
#endif // ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
