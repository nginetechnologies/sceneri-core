#include "Pipelines/DrawNormalsPipeline.h"

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/Descriptors/DescriptorSetView.h>
#include <Renderer/Assets/StaticMesh/RenderMeshView.h>
#include <Renderer/Assets/StaticMesh/VertexNormals.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexTextureCoordinate.h>
#include <Renderer/Scene/InstanceBuffer.h>
#include <Renderer/Devices/PhysicalDeviceFeatures.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Assets/Shader/ShaderCache.h>
#include <Renderer/Scene/ViewMatrices.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Common/Math/Color.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/Length.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/Memory/Align.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Rendering::Debug
{
	struct DrawNormalsConstants
	{
		struct GeomConstants
		{
			Math::Matrix4x4f m_viewProjectionMatrix;
			Math::Vector4f m_colorAndLineLength = {0, 0, 1, 0.1f};
		};

		GeomConstants geomConstants;
	};

	inline static constexpr Array<const PushConstantRange, 1> DrawNormalsPushConstantRanges = {PushConstantRange{
		ShaderStage::Geometry | ShaderStage::Vertex, OFFSET_OF(DrawNormalsConstants, geomConstants), sizeof(DrawNormalsConstants::geomConstants)
	}};

	DrawNormalsPipeline::DrawNormalsPipeline(
		const Type type, Rendering::LogicalDevice& logicalDevice, const ArrayView<const DescriptorSetLayoutView> descriptorSetLayouts
	)
		: m_type(type)
	{
		CreateBase(logicalDevice, descriptorSetLayouts, (ArrayView<const PushConstantRange, uint8>)DrawNormalsPushConstantRanges);
	}

	Threading::JobBatch DrawNormalsPipeline::CreatePipeline(
		LogicalDevice& logicalDevice,
		ShaderCache& shaderCache,
		const RenderPassView renderPass,
		const Math::Rectangleui outputArea,
		const Math::Rectangleui renderArea,
		const uint8 subpassIndex
	)
	{
		Array<VertexInputBindingDescription, 3> vertexInputBindingDescriptions = {
			VertexInputBindingDescription{0, sizeof(VertexPosition), VertexInputRate::Vertex},
			VertexInputBindingDescription{1, sizeof(VertexNormals), VertexInputRate::Vertex},
			VertexInputBindingDescription{2, sizeof(InstanceBuffer::InstanceIndexType), VertexInputRate::Instance}
		};
		FlatVector<VertexInputAttributeDescription, 7> vertexInputAttributeDescriptions;
		switch (m_type)
		{
			case Type::Normals:
				vertexInputAttributeDescriptions = {// Per-vertex attributes
				                                    VertexInputAttributeDescription{0, 0, Format::R32G32B32_SFLOAT, 0},
				                                    VertexInputAttributeDescription{1, 1, Format::R32_UINT, 0},
				                                    // Per-instance attributes
				                                    VertexInputAttributeDescription{2, 2, Format::R32_UINT, 0}
				};
				break;
			case Type::Tangents:
				vertexInputAttributeDescriptions = {
					// Per-vertex attributes
					VertexInputAttributeDescription{0, 0, Format::R32G32B32_SFLOAT, 0},
					VertexInputAttributeDescription{1, 1, Format::R32_UINT, static_cast<uint32>(Memory::GetOffsetOf(&VertexNormals::tangent))},
					// Per-instance attributes
					VertexInputAttributeDescription{2, 2, Format::R32_UINT, 0}
				};
				break;
			case Type::Bitangents:
				vertexInputAttributeDescriptions = {
					// Per-vertex attributes
					VertexInputAttributeDescription{0, 0, Format::R32G32B32_SFLOAT, 0},
					VertexInputAttributeDescription{1, 1, Format::R32_UINT, 0},
					VertexInputAttributeDescription{2, 1, Format::R32_UINT, static_cast<uint32>(Memory::GetOffsetOf(&VertexNormals::tangent))},
					// Per-instance attributes
					// Per-instance attributes
					VertexInputAttributeDescription{3, 2, Format::R32_UINT, 0}
				};
				break;
		}

		const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = logicalDevice.GetPhysicalDevice().GetSupportedFeatures();

		Guid vertexShaderAssetGuid;
		if (supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::GeometryShader))
		{
			switch (m_type)
			{
				case Type::Normals:
					vertexShaderAssetGuid = "A302AE57-47A4-4464-A61A-83D596708778"_asset;
					break;
				case Type::Tangents:
					vertexShaderAssetGuid = "58DBB0A9-8D79-4E2E-8EBC-BBD09EB694DB"_asset;
					break;
				case Type::Bitangents:
					vertexShaderAssetGuid = "66A3C15A-2F29-4805-AFF4-B116888E9DAB"_asset;
					break;
				default:
					ExpectUnreachable();
			}
		}
		else
		{
			vertexShaderAssetGuid = "3D021D8C-37EA-4CB4-A4C4-17A0361234E2"_asset;
		}

		const VertexStageInfo vertexStage{
			ShaderStageInfo{vertexShaderAssetGuid},
			vertexInputBindingDescriptions.GetView(),
			vertexInputAttributeDescriptions.GetView()
		};

		GeometryStageInfo geometryStage;
		Optional<GeometryStageInfo*> pGeometryStage;
		if (supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::GeometryShader))
		{
			geometryStage = GeometryStageInfo{ShaderStageInfo{"{6E97329B-4E09-460D-9B15-C7C4EF827B41}"_asset}};
			pGeometryStage = geometryStage;
		}

		const PrimitiveInfo primitiveInfo{PrimitiveTopology::TriangleList, PolygonMode::Fill, WindingOrder::CounterClockwise, CullMode::Back};

		const Array<Viewport, 1> viewports{Viewport{outputArea}};
		const Array<Math::Rectangleui, 1> scissors{renderArea};

		const ColorTargetInfo colorBlendAttachment{
			ColorAttachmentBlendState{BlendFactor::SourceAlpha, BlendFactor::OneMinusSourceAlpha, BlendOperation::Add}
		};
		const FragmentStageInfo fragmentStage{
			ShaderStageInfo{"{F95C89E8-69E2-46B1-BF00-87365A701834}"_asset},
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
			pGeometryStage,
			dynamicStateFlags
		);
	}

	void DrawNormalsPipeline::DrawWithGeometryShader(
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
	) const
	{
		renderCommandEncoder.BindDescriptorSets(
			m_pipelineLayout,
			ArrayView<const DescriptorSetView, uint8>(transformBufferDescriptorSet),
			GetFirstDescriptorSetIndex()
		);

		const DrawNormalsConstants constants = {
			DrawNormalsConstants::GeomConstants{viewProjectionMatrix, {color.r, color.g, color.b, length.GetMeters()}}
		};

		PushConstants(logicalDevice, renderCommandEncoder, DrawNormalsPushConstantRanges, constants);

		{
			const Array vertexBuffers = {mesh.GetVertexBuffer(), mesh.GetVertexBuffer(), instanceBuffer};

			const Rendering::Index vertexCount = mesh.GetVertexCount();
			const Array<uint64, vertexBuffers.GetSize()> sizes{
				sizeof(Rendering::VertexPosition) * vertexCount,
				sizeof(Rendering::VertexNormals) * vertexCount,
				sizeof(InstanceBuffer::InstanceIndexType) * instanceCount
			};

			const uint64 normalsOffset = Memory::Align(sizes[0], alignof(Rendering::VertexNormals));
			const Array<uint64, vertexBuffers.GetSize()> offsets =
				{0u, normalsOffset, firstInstanceIndex * sizeof(InstanceBuffer::InstanceIndexType)};

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
