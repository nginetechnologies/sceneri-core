#include "Pipelines/SharpenPipeline.h"

#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/Vector4.h>
#include <Common/Memory/OffsetOf.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Assets/StaticMesh/RenderMeshView.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Devices/LogicalDevice.h>

#define A_CPU
#include <FSR/ffx_a.h>
#include <FSR/ffx_fsr1.h>

#include <DeferredShading/FSR/fsr_settings.h>

namespace ngine::Rendering
{
	struct RCASConstants
	{
		// 5 members * 4 components each * 4 bytes per component = 80 bytes < 128
		Math::TVector4<uint32> Const0;
		Math::TVector4<uint32> Const1;
		Math::TVector4<uint32> Const2;
		Math::TVector4<uint32> Const3;
		Math::TVector4<uint32> Sample; // Only uses x component as a bool
	};

	RCASConstants rcasConstants;

	inline static constexpr Array<const PushConstantRange, 1> SharpenPushConstantRanges = {
		PushConstantRange{ShaderStage::Compute, 0, sizeof(RCASConstants)}
	};

	inline static constexpr Array<const DescriptorSetLayout::Binding, 3> SharpenDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(
			0, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		), // Upscaled image
		DescriptorSetLayout::Binding::MakeStorageImage(
			1, ShaderStage::Compute, Format::R16G16B16A16_SFLOAT, StorageTextureAccess::WriteOnly
		), // Swapchain image output
		DescriptorSetLayout::Binding::MakeSampler(2, ShaderStage::Compute, SamplerBindingType::Filtering)
	}; // Sampler for gather4 (textureGather)

	SharpenPipeline::SharpenPipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache)
		: DescriptorSetLayout(logicalDevice, SharpenDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Sharpen");
#endif
		CreateBase(logicalDevice, Array<const DescriptorSetLayoutView, 1>{*this}, SharpenPushConstantRanges);
		Create(logicalDevice, shaderCache, ShaderStageInfo{"d27a817a-890e-4e60-aaca-26890dca42a6"_asset}, m_pipelineLayout);
	}

	void SharpenPipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void SharpenPipeline::ComputeRCASConstants()
	{
		float rcasAttenuation = 0.25f; // Adjustable parameter, hardcoded for now, seems to be a sharpness factor
		FsrRcasCon(reinterpret_cast<AU1*>(&rcasConstants.Const0), rcasAttenuation);
		bool hdr = false;

		// Nothing for now
		rcasConstants.Sample.x = (hdr) ? 1 : 0; // IDK what this is for yet
	}

	void SharpenPipeline::Compute(
		Rendering::LogicalDevice& logicalDevice,
		ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		const Math::Vector2ui& displayResolution
	) const
	{
		computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, imageDescriptorSets, GetFirstDescriptorSetIndex());

		PushConstants(logicalDevice, computeCommandEncoder, SharpenPushConstantRanges, rcasConstants);

		const Math::Vector3ui threadGroupSize{16, 16, 1}; // From FSR_Filter.cpp example

		computeCommandEncoder.Dispatch(GetNumberOfThreadGroups(displayResolution, threadGroupSize), Math::Vector3ui{64, 1, 1});
	}
}
