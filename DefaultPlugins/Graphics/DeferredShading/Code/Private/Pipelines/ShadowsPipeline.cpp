#include "Pipelines/ShadowsPipeline.h"

#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
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
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Pipelines/PushConstantRange.h>

namespace ngine::Rendering
{
	namespace WithGeometryShader
	{
		inline static constexpr Array ShadowDescriptorBindings = {DescriptorSetLayout::Binding::MakeStorageBuffer(0, ShaderStage::Geometry)};

		inline static constexpr ArrayView<const PushConstantRange, uint8> ShadowPushConstantRanges = {};
	}

	namespace WithoutGeometryShader
	{
		inline static constexpr Array ShadowDescriptorBindings = {DescriptorSetLayout::Binding::MakeStorageBuffer(0, ShaderStage::Vertex)};
	}

	// TODO: Need to move the descriptor binding of the transform buffer out
	// Just use the set and layout already made in TransformBuffer
	ShadowsPipeline::ShadowsPipeline(
		Rendering::LogicalDevice& logicalDevice, const DescriptorSetLayoutView transformBufferDescriptorSetLayout
	)
		: DescriptorSetLayout(
				logicalDevice,
				logicalDevice.GetPhysicalDevice().GetSupportedFeatures().AreAllSet(
					PhysicalDeviceFeatures::GeometryShader | PhysicalDeviceFeatures::LayeredRendering
				)
					? WithGeometryShader::ShadowDescriptorBindings.GetView()
					: WithoutGeometryShader::ShadowDescriptorBindings.GetView()
			)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		DescriptorSetLayout::SetDebugName(logicalDevice, "Shadows");
#endif
		CreateBase(
			logicalDevice,
			Array<const DescriptorSetLayoutView, 2>{transformBufferDescriptorSetLayout, *this},
			logicalDevice.GetPhysicalDevice().GetSupportedFeatures().AreAllSet(
				PhysicalDeviceFeatures::GeometryShader | PhysicalDeviceFeatures::LayeredRendering
			)
				? WithGeometryShader::ShadowPushConstantRanges
				: WithoutGeometryShader::ShadowPushConstantRanges.GetView()
		);
	}

	void ShadowsPipeline::Destroy(LogicalDevice& logicalDevice)
	{
		GraphicsPipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);
	}

	Threading::JobBatch ShadowsPipeline::CreatePipeline(
		LogicalDevice& logicalDevice,
		ShaderCache& shaderCache,
		const RenderPassView renderPass,
		const Math::Rectangleui outputArea,
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
			ShaderStageInfo{
				logicalDevice.GetPhysicalDevice().GetSupportedFeatures().AreAllSet(
					PhysicalDeviceFeatures::GeometryShader | PhysicalDeviceFeatures::LayeredRendering
				)
					? "C27E5EBD-CC26-4744-8F21-A3508B59B41A"_asset
					: "3D021D8C-37EA-4CB4-A4C4-17A0361234E2"_asset
			},
			vertexInputBindingDescriptions.GetView(),
			vertexInputAttributeDescriptions.GetView()
		};

		constexpr int depthBiasConstant = 1;
		constexpr float depthBiasSlope = 8.0f;
		constexpr float depthBiasClamp = 0.f;

		const PrimitiveInfo primitiveInfo{
			PrimitiveTopology::TriangleList,
			PolygonMode::Fill,
			WindingOrder::Clockwise, // TODO face winding is reversed on shadows (view and/or projection matrix issue). By the way culling
		                           // front faces instead of back facing is a valid strategy for shadows. (to avoid shadow acne)
			CullMode::Back,
			PrimitiveFlags::DepthBias | PrimitiveFlags::DepthClamp,
			depthBiasConstant,
			depthBiasClamp,
			depthBiasSlope
		};

		const Array<Viewport, 1> viewports{Viewport{outputArea}};
		const Array<Math::Rectangleui, 1> scissors{renderArea};

		GeometryStageInfo geometryStage;
		Optional<GeometryStageInfo*> pGeometryStage;
		if (logicalDevice.GetPhysicalDevice().GetSupportedFeatures().AreAllSet(
					PhysicalDeviceFeatures::GeometryShader | PhysicalDeviceFeatures::LayeredRendering
				))
		{
			geometryStage = {"F7A367EF-A5A3-41C2-AF51-E9B759D52044"_asset};
			pGeometryStage = geometryStage;
		}

		const DepthStencilInfo depthStencil{
			DepthStencilFlags::DepthTest | DepthStencilFlags::DepthWrite,
			CompareOperation::LessOrEqual,
			StencilOperationState{
				StencilOperation::Keep,
				StencilOperation::Keep,
				StencilOperation::Keep,
				CompareOperation::AlwaysSucceed,
				0,
				0,
				0
			},
			StencilOperationState{
				StencilOperation::Keep,
				StencilOperation::Keep,
				StencilOperation::Keep,
				CompareOperation::AlwaysSucceed,
				0,
				0,
				0
			},
		};

		EnumFlags<DynamicStateFlags> dynamicStateFlags{};
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
			Optional<const FragmentStageInfo*>{},
			Optional<const MultisamplingInfo*>{},
			depthStencil,
			pGeometryStage,
			dynamicStateFlags
		);
	}

	void ShadowsPipeline::Draw(
		uint32 firstInstanceIndex,
		const uint32 instanceCount,
		const RenderMeshView mesh,
		const BufferView instanceBuffer,
		const RenderCommandEncoderView renderCommandEncoder
	) const
	{
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
