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
	struct DrawNormalsPipeline final : public GraphicsPipeline
	{
		enum class Type : uint8
		{
			Normals,
			Tangents,
			Bitangents
		};

		DrawNormalsPipeline(
			const Type type, Rendering::LogicalDevice& logicalDevice, const ArrayView<const DescriptorSetLayoutView> descriptorSetLayouts
		);
		DrawNormalsPipeline(const DrawNormalsPipeline&) = delete;
		DrawNormalsPipeline& operator=(const DrawNormalsPipeline&) = delete;
		DrawNormalsPipeline(DrawNormalsPipeline&& other) = default;
		DrawNormalsPipeline& operator=(DrawNormalsPipeline&&) = delete;

		void DrawWithGeometryShader(
			Rendering::LogicalDevice& logicalDevice,
			const Math::Color color,
			const Math::Lengthf length,
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
	protected:
		const Type m_type;
	};
}
