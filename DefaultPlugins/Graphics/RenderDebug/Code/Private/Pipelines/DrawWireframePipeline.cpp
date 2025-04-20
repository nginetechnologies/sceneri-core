#include "Pipelines/DrawWireframePipeline.h"

#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/Length.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Memory/Align.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Threading/Jobs/JobBatch.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Assets/StaticMesh/RenderMeshView.h>
#include <Renderer/Assets/StaticMesh/VertexNormals.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexTextureCoordinate.h>
#include <Renderer/Scene/InstanceBuffer.h>
#include <Renderer/Devices/PhysicalDeviceFeatures.h>
#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Pipelines/PushConstantRange.h>
#include <Renderer/Descriptors/DescriptorSetView.h>

namespace ngine::Rendering::Debug
{
	struct DrawWireframeConstants
	{
		struct VertexConstants
		{
			Math::Matrix4x4f m_viewProjectionMatrix;
		};

		VertexConstants vertexConstants;
	};

	inline static constexpr Array<const PushConstantRange, 1> DrawWireframePushConstantRanges = {PushConstantRange{
		ShaderStage::Vertex, OFFSET_OF(DrawWireframeConstants, vertexConstants), sizeof(DrawWireframeConstants::vertexConstants)
	}};

	DrawWireframePipeline::DrawWireframePipeline(
		Rendering::LogicalDevice& logicalDevice, const ArrayView<const DescriptorSetLayoutView> descriptorSetLayouts
	)
	{
		CreateBase(logicalDevice, descriptorSetLayouts, DrawWireframePushConstantRanges);
	}

	Threading::JobBatch DrawWireframePipeline::CreatePipeline(
		LogicalDevice& logicalDevice,
		ShaderCache& shaderCache,
		const RenderPassView renderPass,
		const Math::Rectangleui outputArea,
		const Math::Rectangleui renderArea,
		const uint8 subpassIndex
	)
	{
		Array<VertexInputBindingDescription, 2> vertexInputBindingDescriptions = {
			VertexInputBindingDescription{0, sizeof(VertexPosition), VertexInputRate::Vertex},
			VertexInputBindingDescription{1, sizeof(InstanceBuffer::InstanceIndexType), VertexInputRate::Instance}
		};
		FlatVector<VertexInputAttributeDescription, 7> vertexInputAttributeDescriptions = {
			// Per-vertex attributes
			VertexInputAttributeDescription{0, 0, Format::R32G32B32_SFLOAT, 0},
			// Per-instance attributes
			VertexInputAttributeDescription{1, 1, Format::R32_UINT, 0}
		};
		const VertexStageInfo vertexStage{
			ShaderStageInfo{"52B8A528-11FA-4675-B5ED-A6BBFFBBC21B"_asset},
			vertexInputBindingDescriptions.GetView(),
			vertexInputAttributeDescriptions.GetView()
		};

		const PrimitiveInfo primitiveInfo{PrimitiveTopology::TriangleList, PolygonMode::Line, WindingOrder::CounterClockwise, CullMode::None};

		const Array<Viewport, 1> viewports{Viewport{outputArea}};
		const Array<Math::Rectangleui, 1> scissors{renderArea};

		const ColorTargetInfo colorBlendAttachment{
			ColorAttachmentBlendState{BlendFactor::SourceAlpha, BlendFactor::OneMinusSourceAlpha, BlendOperation::Add}
		};
		const FragmentStageInfo fragmentStage{
			ShaderStageInfo{"19F1176B-A487-436F-A9FC-121BFF5A5FB6"_asset},
			ArrayView<const ColorTargetInfo>(colorBlendAttachment)
		};

		const DepthStencilInfo depthStencil{DepthStencilFlags::DepthTest, CompareOperation::Greater};

		EnumFlags<DynamicStateFlags> dynamicStateFlags;
		// TODO: Hardwired for VR / XR, change to pass in a flag instead
		dynamicStateFlags |= DynamicStateFlags::Viewport * (outputArea != renderArea);

		return CreateAsync(
			logicalDevice,
			shaderCache,
			m_pipelineLayout,
			renderPass,
			vertexStage,
			primitiveInfo,
			viewports,
			scissors,
			subpassIndex,
			fragmentStage,
			Optional<const MultisamplingInfo*>{},
			depthStencil,
			Optional<const GeometryStageInfo*>{},
			dynamicStateFlags
		);
	}

	void DrawWireframePipeline::Draw(
		Rendering::LogicalDevice& logicalDevice,
		uint32 firstInstanceIndex,
		const uint32 instanceCount,
		const Math::Matrix4x4f& viewProjectionMatrix,
		const RenderMeshView mesh,
		const BufferView instanceBuffer,
		const DescriptorSetView transformBufferDescriptorSet,
		const RenderCommandEncoderView renderCommandEncoder
	) const
	{
		renderCommandEncoder.BindDescriptorSets(
			m_pipelineLayout,
			ArrayView<const DescriptorSetView, uint8>(transformBufferDescriptorSet),
			GetFirstDescriptorSetIndex()
		);

		const DrawWireframeConstants constants = {DrawWireframeConstants::VertexConstants{viewProjectionMatrix}};

		PushConstants(logicalDevice, renderCommandEncoder, DrawWireframePushConstantRanges, constants);

		{
			const Array vertexBuffers{mesh.GetVertexBuffer(), instanceBuffer};
			const Rendering::Index vertexCount = mesh.GetVertexCount();

			const Array<uint64, vertexBuffers.GetSize()> offsets{0u, firstInstanceIndex * sizeof(InstanceBuffer::InstanceIndexType)};
			const Array<uint64, vertexBuffers.GetSize()> sizes{
				sizeof(Rendering::VertexPosition) * vertexCount,
				sizeof(InstanceBuffer::InstanceIndexType) * instanceCount
			};
			renderCommandEncoder.BindVertexBuffers(vertexBuffers, offsets.GetDynamicView(), sizes.GetDynamicView());
		}

		// Actual instance index will be 0 since buffers were bound with an offset
		firstInstanceIndex = 0;

		const uint32 firstIndex = 0u;
		const int32_t vertexOffset = 0;
		renderCommandEncoder.DrawIndexed(
			mesh.GetIndexBuffer(),
			0,
			sizeof(Rendering::Index) * mesh.GetIndexCount(),
			mesh.GetIndexCount(),
			instanceCount,
			firstIndex,
			vertexOffset,
			firstInstanceIndex
		);
	}
}
