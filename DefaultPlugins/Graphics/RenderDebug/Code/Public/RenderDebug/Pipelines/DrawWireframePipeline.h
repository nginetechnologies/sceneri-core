#pragma once

#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>
#include <Common/Math/ForwardDeclarations/Matrix4x4.h>
#include <Common/Math/ForwardDeclarations/Color.h>
#include <Common/Math/ForwardDeclarations/WorldCoordinate.h>
#include <Common/Math/ForwardDeclarations/Length.h>

#include <Renderer/Pipelines/GraphicsPipeline.h>

namespace ngine::Rendering
{
	struct LogicalDeviceView;
	struct RenderPassView;
	struct ShaderCache;
	struct RenderMeshView;
	struct RenderCommandEncoderView;
}

namespace ngine::Rendering::Debug
{
	struct DrawWireframePipeline final : Rendering::GraphicsPipeline
	{
		DrawWireframePipeline(Rendering::LogicalDevice& logicalDevice, const ArrayView<const DescriptorSetLayoutView> descriptorSetLayouts);
		DrawWireframePipeline(const DrawWireframePipeline&) = delete;
		DrawWireframePipeline& operator=(const DrawWireframePipeline&) = delete;
		DrawWireframePipeline(DrawWireframePipeline&& other) = default;
		DrawWireframePipeline& operator=(DrawWireframePipeline&&) = delete;

		void Draw(
			Rendering::LogicalDevice& logicalDevice,
			uint32 firstInstanceIndex,
			const uint32 instanceCount,
			const Math::Matrix4x4f& viewProjectionMatrix,
			const RenderMeshView mesh,
			const BufferView instanceBuffer,
			const DescriptorSetView transformBufferDescriptorSet,
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
