#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/ForwardDeclarations/Color.h>
#include <Common/Math/ForwardDeclarations/WorldCoordinate.h>
#include <Common/Platform/OffsetOf.h>

#include <Renderer/Pipelines/GraphicsPipeline.h>
#include <Renderer/Descriptors/DescriptorSetLayout.h>
#include <Renderer/Devices/PhysicalDeviceFeatures.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <DeferredShading/Features.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct RenderCommandEncoderView;
	struct ShaderCache;
	struct RenderPassView;
	struct RenderMeshView;
	struct TransformBufferView;

	struct InstanceGroupPerLogicalDeviceData;

#define MATERIAL_IDENTIFIERS_DEPTH_ONLY 1

	struct MaterialIdentifiersPipeline final : public GraphicsPipeline
	{
		struct Constants
		{
			struct VertexConstants
			{
				Math::Matrix4x4f viewMatrix;
				Math::Matrix4x4f viewProjectionMatrix;
			};
#if !MATERIAL_IDENTIFIERS_DEPTH_ONLY
			struct FragmentConstants
			{
				uint32 materialIdentifier;
			};
#endif

			VertexConstants vertexConstants;
#if !MATERIAL_IDENTIFIERS_DEPTH_ONLY
			FragmentConstants fragmentConstants;
#endif
		};

		inline static constexpr Array PushConstantRanges = {
			PushConstantRange{ShaderStage::Vertex, OFFSET_OF(Constants, vertexConstants), sizeof(Constants::VertexConstants)},
#if !MATERIAL_IDENTIFIERS_DEPTH_ONLY
			PushConstantRange{ShaderStage::Fragment, OFFSET_OF(Constants, fragmentConstants), sizeof(Constants::FragmentConstants)}
#endif
		};

		MaterialIdentifiersPipeline(LogicalDevice& logicalDevice, const DescriptorSetLayoutView transformBufferDescriptorSetLayout);
		MaterialIdentifiersPipeline(const MaterialIdentifiersPipeline&) = delete;
		MaterialIdentifiersPipeline& operator=(const MaterialIdentifiersPipeline&) = delete;
		MaterialIdentifiersPipeline(MaterialIdentifiersPipeline&& other) = default;
		MaterialIdentifiersPipeline& operator=(MaterialIdentifiersPipeline&&) = delete;

		[[nodiscard]] Threading::JobBatch CreatePipeline(
			LogicalDevice& logicalDevice,
			ShaderCache& shaderCache,
			const RenderPassView renderPass,
			const Math::Rectangleui outputArea,
			const Math::Rectangleui renderArea,
			const uint8 subpassIndex
		);
	};
}
