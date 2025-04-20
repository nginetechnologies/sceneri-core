#include "Pipelines/SuperResolutionPipeline.h"

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
	struct FSRConstants
	{
		// 5 members * 4 components each * 4 bytes per component = 80 bytes < 128
		Math::TVector4<uint32> Const0;
		Math::TVector4<uint32> Const1;
		Math::TVector4<uint32> Const2;
		Math::TVector4<uint32> Const3;
		Math::TVector4<uint32> Sample; // Only uses x component as a bool
	};

	FSRConstants fsrConstants;

	inline static constexpr Array<const PushConstantRange, 1> SuperResolutionPushConstantRanges = {
		PushConstantRange{ShaderStage::Compute, 0, sizeof(FSRConstants)}
	};

	inline static constexpr Array<const DescriptorSetLayout::Binding, 3> SuperResolutionDescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(
			0, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		), // TAAIntermediate Image
		DescriptorSetLayout::Binding::MakeStorageImage(
			1, ShaderStage::Compute, Format::R16G16B16A16_SFLOAT, StorageTextureAccess::WriteOnly
		), // Intermediate render target consumed by sharpen substage
		DescriptorSetLayout::Binding::MakeSampler(2, ShaderStage::Compute, SamplerBindingType::Filtering)
	}; // Sampler for gather4 (textureGather)

	SuperResolutionPipeline::SuperResolutionPipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache)
		: DescriptorSetLayout(logicalDevice, SuperResolutionDescriptorBindings)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Super Resolution");
#endif
		CreateBase(logicalDevice, Array<const DescriptorSetLayoutView, 1>{*this}, SuperResolutionPushConstantRanges);
		Create(logicalDevice, shaderCache, ShaderStageInfo{"4aa8c1b5-c5eb-4595-8c3e-b4ff75b9d280"_asset}, m_pipelineLayout);
	}

	void SuperResolutionPipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void SuperResolutionPipeline::ComputeEASUConstants(const Math::Vector2ui renderResolution, const Math::Vector2ui displayResolution)
	{

		FsrEasuCon(
			reinterpret_cast<AU1*>(&fsrConstants.Const0),
			reinterpret_cast<AU1*>(&fsrConstants.Const1),
			reinterpret_cast<AU1*>(&fsrConstants.Const2),
			reinterpret_cast<AU1*>(&fsrConstants.Const3),
			static_cast<AF1>(renderResolution.x),
			static_cast<AF1>(renderResolution.y),
			static_cast<AF1>(renderResolution.x),
			static_cast<AF1>(renderResolution.y),
			(AF1)(displayResolution.x),
			(AF1)(displayResolution.y)
		);

		bool bUseRcas = true; // Hardcoded for now
		bool hdr = false;

		// Nothing for now
		fsrConstants.Sample.x = (hdr && !bUseRcas)
		                          ? 1
		                          : 0; // IDK what this is for yet, something to do with the deringing at the end of the EASU function. But
		                               // since we're using RCAS it seems it doesn't matter since this expression will always be false
	}

	void SuperResolutionPipeline::Compute(
		Rendering::LogicalDevice& logicalDevice,
		ArrayView<const DescriptorSetView, uint8> imageDescriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		const Math::Vector2ui& displayResolution
	) const
	{
		computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, imageDescriptorSets, GetFirstDescriptorSetIndex());

		PushConstants(logicalDevice, computeCommandEncoder, SuperResolutionPushConstantRanges, fsrConstants);

		const Math::Vector3ui threadGroupSize{16, 16, 1}; // From FSR_Filter.cpp example

		computeCommandEncoder.Dispatch(GetNumberOfThreadGroups(displayResolution, threadGroupSize), Math::Vector3ui{64, 1, 1});
	}
}
