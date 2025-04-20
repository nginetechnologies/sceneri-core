#include "Pipelines/MaterialIdentifiersPipeline.h"

#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Memory/Align.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Threading/Jobs/JobBatch.h>

#include <Renderer/3rdparty/vulkan/vulkan.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Assets/StaticMesh/RenderMeshView.h>
#include <Renderer/Assets/StaticMesh/VertexNormals.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexTextureCoordinate.h>
#include <Renderer/Scene/InstanceBuffer.h>
#include <Renderer/Devices/PhysicalDeviceFeatures.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Devices/LogicalDeviceView.h>
#include <Renderer/Pipelines/PushConstantRange.h>

namespace ngine::Rendering
{
	MaterialIdentifiersPipeline::MaterialIdentifiersPipeline(
		LogicalDevice& logicalDevice, const DescriptorSetLayoutView transformBufferDescriptorSetLayout
	)
	{
		CreateBase(logicalDevice, Array<const DescriptorSetLayoutView, 1>{transformBufferDescriptorSetLayout}, PushConstantRanges);
	}

	Threading::JobBatch MaterialIdentifiersPipeline::CreatePipeline(
		LogicalDevice& logicalDevice,
		ShaderCache& shaderCache,
		const RenderPassView renderPass,
		[[maybe_unused]] const Math::Rectangleui outputArea,
		const Math::Rectangleui renderArea,
		const uint8 subpassIndex
	)
	{
		const Array vertexInputBindingDescriptions = {
			VertexInputBindingDescription{0, sizeof(VertexPosition), VertexInputRate::Vertex},
			VertexInputBindingDescription{1, sizeof(InstanceBuffer::InstanceIndexType), VertexInputRate::Instance}
		};

		const Array vertexInputAttributeDescriptions = {
			// Per-vertex attributes
			VertexInputAttributeDescription{0, 0, Format::R32G32B32_SFLOAT, 0},
			// Per-instance attributes
			VertexInputAttributeDescription{1, 1, Format::R32_UINT, 0},
		};
		const VertexStageInfo vertexStage{
			ShaderStageInfo{"110E84A9-C54C-431D-A358-916F7668A230"_asset},
			vertexInputBindingDescriptions.GetView(),
			vertexInputAttributeDescriptions.GetView()
		};

		const PrimitiveInfo primitiveInfo{PrimitiveTopology::TriangleList, PolygonMode::Fill, WindingOrder::CounterClockwise, CullMode::None};

		const Array<Viewport, 1> viewports{Viewport{renderArea}};
		const Array<Math::Rectangleui, 1> scissors{renderArea};

		Optional<const FragmentStageInfo*> pFragmentStageInfo;
#if !MATERIAL_IDENTIFIERS_DEPTH_ONLY
		const Array<ColorTargetInfo, 1> colorBlendAttachments{ColorTargetInfo{}};

		const FragmentStageInfo fragmentStage{ShaderStageInfo{"C1585C1B-BAAA-404C-B216-0F6A4F6"_asset}, colorBlendAttachments};
		pFragmentStageInfo = &fragmentStage;
#endif

		const DepthStencilInfo depthStencil{DepthStencilFlags::DepthTest | DepthStencilFlags::DepthWrite, CompareOperation::Greater};

		EnumFlags<DynamicStateFlags> dynamicStateFlags{DynamicStateFlags::CullMode};
		// TODO: Hardwired for VR / XR, change to pass in a flag instead
		// dynamicStateFlags |= DynamicStateFlags::Viewport * (outputArea != renderArea); // Temporary fix for FSR changes since this causes
		// validation errors

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
			pFragmentStageInfo,
			Optional<const MultisamplingInfo*>{},
			depthStencil,
			Optional<const GeometryStageInfo*>{},
			dynamicStateFlags
		);
	}
}
