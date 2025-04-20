#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>
#include <Common/Math/ForwardDeclarations/Matrix4x4.h>
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

	namespace WithoutGeometryShader
	{
		struct ShadowConstants
		{
			struct VertexConstants
			{
				uint32 shadowMatrixIndex;
			};

			VertexConstants vertexConstants;
		};

		inline static constexpr Array<const PushConstantRange, 1> ShadowPushConstantRanges = {
			PushConstantRange{ShaderStage::Vertex, OFFSET_OF(ShadowConstants, vertexConstants), sizeof(ShadowConstants::VertexConstants)}
		};
	}

	struct ShadowsPipeline final : public DescriptorSetLayout, public GraphicsPipeline
	{
		ShadowsPipeline(Rendering::LogicalDevice& logicalDevice, const DescriptorSetLayoutView transformBufferDescriptorSetLayout);
		ShadowsPipeline(const ShadowsPipeline&) = delete;
		ShadowsPipeline& operator=(const ShadowsPipeline&) = delete;
		ShadowsPipeline(ShadowsPipeline&& other) = default;
		ShadowsPipeline& operator=(ShadowsPipeline&&) = delete;

		[[nodiscard]] bool IsValid() const
		{
			return GraphicsPipeline::IsValid() & DescriptorSetLayout::IsValid();
		}

		void Destroy(LogicalDevice& logicalDevice);

		void Draw(
			uint32 firstInstanceIndex,
			const uint32 instanceCount,
			const RenderMeshView mesh,
			const BufferView instanceBuffer,
			const RenderCommandEncoderView renderCommandEncoder
		) const;
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
