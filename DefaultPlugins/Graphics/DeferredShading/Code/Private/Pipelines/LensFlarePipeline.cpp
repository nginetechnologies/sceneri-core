#include "Pipelines/LensFlarePipeline.h"

#include <Renderer/Vulkan/Includes.h>

#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Math/Vector2.h>
#include <Common/Math/Vector3.h>

namespace ngine::Rendering
{
	struct LensFlareConstants
	{
		int m_numGhosts;
		float m_bloomIntensity, m_haloIntensity, m_ghostIntensity, m_ghostDispersal, m_haloWidth, m_chromaticAberation;
	};

	inline static constexpr LensFlareConstants LensFlareConstantValues{5, 0.003f, 0.0002f, 0.00002f, 0.3f, 0.5f, 6.f};

	inline static constexpr Array<const PushConstantRange, 1> LensFlarePushConstantRanges = {
		PushConstantRange{ShaderStage::Compute, 0, sizeof(LensFlareConstants)}
	};

	inline static constexpr Array<const DescriptorSetLayout::Binding, 3> LensFlareDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(0, ShaderStage::Compute, SampledImageType::Float, ImageMappingType::TwoDimensional),
		DescriptorSetLayout::Binding::MakeStorageImage(1, ShaderStage::Compute, Format::R16G16B16A16_SFLOAT, StorageTextureAccess::WriteOnly),
		DescriptorSetLayout::Binding::MakeSampler(2, ShaderStage::Compute, SamplerBindingType::Filtering)
	};

	LensFlarePipeline::LensFlarePipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache)
		: DescriptorSetLayout(logicalDevice, LensFlareDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Lens Flare");
#endif
		CreateBase(logicalDevice, ArrayView<const DescriptorSetLayoutView>{*this}, LensFlarePushConstantRanges);
		Create(logicalDevice, shaderCache, ShaderStageInfo{"E610AE8D-BC71-4655-98B5-40FAB2D84889"_asset}, m_pipelineLayout);
	}

	void LensFlarePipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void LensFlarePipeline::Compute(
		Rendering::LogicalDevice& logicalDevice,
		ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		const Math::Vector2ui resolution
	) const
	{
		PushConstants(logicalDevice, computeCommandEncoder, LensFlarePushConstantRanges, LensFlareConstantValues);

		computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, imageDescriptorSets, GetFirstDescriptorSetIndex());

		const Math::Vector3ui threadGroupSize{8, 8, 1};
		computeCommandEncoder.Dispatch(GetNumberOfThreadGroups(resolution, threadGroupSize), Math::Vector3ui{8, 8, 1});
	}
}
