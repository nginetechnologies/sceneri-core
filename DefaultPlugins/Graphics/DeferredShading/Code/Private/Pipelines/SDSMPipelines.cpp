#include "Pipelines/SDSMPipelines.h"

#if ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS

#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/Vector2/Mod.h>
#include <Common/Math/Vector2/Sign.h>
#include <Common/Memory/OffsetOf.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Descriptors/DescriptorSetView.h>
#include <Renderer/Assets/StaticMesh/RenderMeshView.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Devices/LogicalDevice.h>

namespace ngine::Rendering
{
	struct SDSMPhase1Constants
	{
		uint32 m_directionalLightCount;
		Math::Vector2ui m_smallestMinMaxResolution;
	};

	inline static constexpr Array<const PushConstantRange, 1> SDSMPhase1PushConstantRanges = {
		PushConstantRange{ShaderStage::Compute, 0, sizeof(SDSMPhase1Constants)}
	};

	inline static constexpr Array<const DescriptorSetLayout::Binding, 2> SDSMPhase1DescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(
			0, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeStorageBuffer(1, ShaderStage::Compute)
	};

	inline static constexpr Array<const EnumFlags<DescriptorSetLayout::Binding::Flags>, 2> SDSMPhase1DescriptorBindingFlags = {
		DescriptorSetLayout::Binding::Flags{}, DescriptorSetLayout::Binding::Flags::ShaderWrite
	};

	SDSMPhase1Pipeline::SDSMPhase1Pipeline(
		Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache, const DescriptorSetLayoutView viewInfoDescriptorSetLayout
	)
		: DescriptorSetLayout(logicalDevice, SDSMPhase1DescriptorBindings, DescriptorSetLayout::Flags{}, SDSMPhase1DescriptorBindingFlags)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "SDSM Phase 1");
#endif
		CreateBase(logicalDevice, Array<const DescriptorSetLayoutView, 2>{viewInfoDescriptorSetLayout, *this}, SDSMPhase1PushConstantRanges);
		Create(logicalDevice, shaderCache, ShaderStageInfo{"CE6361E8-5EBE-46BC-AAAB-2ACC997A7C47"_asset}, m_pipelineLayout);
	}

	void SDSMPhase1Pipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void SDSMPhase1Pipeline::Compute(
		Rendering::LogicalDevice& logicalDevice,
		ArrayView<const DescriptorSetView, uint8> descriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		const uint32 directionalLightCount,
		const Math::Vector2ui smallestDepthMinMaxResolution
	) const
	{
		const SDSMPhase1Constants constants = {directionalLightCount, smallestDepthMinMaxResolution};

		PushConstants(logicalDevice, computeCommandEncoder, SDSMPhase1PushConstantRanges, constants);
		computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, descriptorSets, GetFirstDescriptorSetIndex());
		computeCommandEncoder.Dispatch(Math::Vector3ui{1, 1, 1}, Math::Vector3ui{1, 1, 1});
	}

	//======================================================

	struct SDSMPhase2Constants
	{
		uint32 m_directionalLightIndex;
	};

	inline static constexpr Array<const PushConstantRange, 1> SDSMPhase2PushConstantRanges = {
		PushConstantRange{ShaderStage::Compute, 0, sizeof(SDSMPhase2Constants)}
	};

	inline static constexpr Array<const DescriptorSetLayout::Binding, 2> SDSMPhase2DescriptorBindings = {
		DescriptorSetLayout::Binding::MakeSampledImage(
			0, ShaderStage::Compute, SampledImageType::UnfilterableFloat, ImageMappingType::TwoDimensional
		),
		DescriptorSetLayout::Binding::MakeStorageBuffer(1, ShaderStage::Compute)
	};

	inline static constexpr Array<const EnumFlags<DescriptorSetLayout::Binding::Flags>, 2> SDSMPhase2DescriptorBindingFlags = {
		DescriptorSetLayout::Binding::Flags{}, DescriptorSetLayout::Binding::Flags::ShaderWrite
	};

	SDSMPhase2Pipeline::SDSMPhase2Pipeline(
		Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache, const DescriptorSetLayoutView viewInfoDescriptorSetLayout
	)
		: DescriptorSetLayout(logicalDevice, SDSMPhase2DescriptorBindings, DescriptorSetLayout::Flags{}, SDSMPhase2DescriptorBindingFlags)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "SDSM Phase 2");
#endif
		CreateBase(logicalDevice, Array<const DescriptorSetLayoutView, 2>{viewInfoDescriptorSetLayout, *this}, SDSMPhase2PushConstantRanges);
		Create(logicalDevice, shaderCache, ShaderStageInfo{"83DDBC8D-4FB6-409C-915B-7041D2DBF259"_asset}, m_pipelineLayout);
	}

	void SDSMPhase2Pipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void SDSMPhase2Pipeline::Compute(
		Rendering::LogicalDevice& logicalDevice,
		ArrayView<const DescriptorSetView, uint8> descriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		const uint8 directionalLightIndex,
		const Math::Vector2ui depthBufferResolution
	) const
	{
		const SDSMPhase2Constants constants = {directionalLightIndex};

		PushConstants(logicalDevice, computeCommandEncoder, SDSMPhase2PushConstantRanges, constants);

		if (descriptorSets.HasElements())
		{
			computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, descriptorSets, GetFirstDescriptorSetIndex());
		}

		const Math::Vector3ui threadGroupSize{8, 8, 1};
		computeCommandEncoder.Dispatch(GetNumberOfThreadGroups(depthBufferResolution, threadGroupSize), Math::Vector3ui{8, 8, 1});
	}

	//======================================================

	inline static constexpr Array<const DescriptorSetLayout::Binding, 4> SDSMPhase3DescriptorBindings = {
		DescriptorSetLayout::Binding::MakeStorageBuffer(0, ShaderStage::Compute),
		DescriptorSetLayout::Binding::MakeStorageBuffer(1, ShaderStage::Compute),
		DescriptorSetLayout::Binding::MakeStorageBuffer(2, ShaderStage::Compute),
		DescriptorSetLayout::Binding::MakeStorageBuffer(3, ShaderStage::Compute)
	};

	inline static constexpr Array<const EnumFlags<DescriptorSetLayout::Binding::Flags>, 4> SDSMPhase3DescriptorBindingFlags = {
		DescriptorSetLayout::Binding::Flags{},
		DescriptorSetLayout::Binding::Flags::ShaderWrite,
		DescriptorSetLayout::Binding::Flags::ShaderWrite,
		DescriptorSetLayout::Binding::Flags{}
	};

	SDSMPhase3Pipeline::SDSMPhase3Pipeline(Rendering::LogicalDevice& logicalDevice, ShaderCache& shaderCache)
		: DescriptorSetLayout(logicalDevice, SDSMPhase3DescriptorBindings, DescriptorSetLayout::Flags{}, SDSMPhase3DescriptorBindingFlags)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "SDSM Phase 3");
#endif
		CreateBase(logicalDevice, Array<const DescriptorSetLayoutView, 1>{*this}, {});
		Create(logicalDevice, shaderCache, ShaderStageInfo{"14E9D53A-2709-4123-8224-67AF35C22E25"_asset}, m_pipelineLayout);
	}

	void SDSMPhase3Pipeline::Destroy(LogicalDevice& logicalDevice)
	{
		ComputePipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	void SDSMPhase3Pipeline::Compute(
		ArrayView<const DescriptorSetView, uint8> descriptorSets,
		const ComputeCommandEncoderView computeCommandEncoder,
		uint32 directionalLightCount
	) const
	{
		computeCommandEncoder.BindDescriptorSets(m_pipelineLayout, descriptorSets, GetFirstDescriptorSetIndex());
		const Math::Vector3ui threadGroupSize{1, directionalLightCount, 1};
		computeCommandEncoder.Dispatch(threadGroupSize, Math::Vector3ui{4, 1, 1});
	}
}

#endif // ENABLE_SAMPLE_DISTRIBUTION_SHADOW_MAPS
