#include <Renderer/Assets/Material/RenderMaterial.h>
#include <Renderer/Assets/Material/RenderMaterialInstance.h>
#include <Renderer/Assets/Material/RuntimeMaterial.h>
#include <Renderer/Assets/Material/RuntimeMaterialInstance.h>
#include <Renderer/Assets/Material/MaterialAssetType.h>

#include <Common/Math/Matrix4x4.h>
#include <Common/Math/Primitives/Rectangle.h>
#include <Common/Memory/OffsetOf.h>
#include <Common/TypeTraits/IsSame.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Reflection/Registry.inl>

#include <Common/System/Query.h>
#include <Engine/Asset/AssetManager.h>

#include <Renderer/Wrappers/RenderPassView.h>
#include <Renderer/SampleCount.h>
#include <Renderer/Assets/StaticMesh/RenderMeshView.h>
#include <Renderer/Assets/StaticMesh/VertexNormals.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexTextureCoordinate.h>
#include <Renderer/Assets/Texture/TextureAsset.h>
#include <Renderer/Assets/Texture/RenderTargetAsset.h>
#include <Renderer/Renderer.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneViewDrawer.h>
#include <Renderer/Scene/TransformBuffer.h>
#include <Renderer/Scene/InstanceBuffer.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Commands/RenderCommandEncoderView.h>
#include <Renderer/Pipelines/PushConstantRange.h>

#include <Renderer/Vulkan/Includes.h>

#include <Common/Math/Vector4.h>

namespace ngine::Rendering
{
	inline FixedCapacityVector<DescriptorSetLayout::Binding, uint8> GetDescriptorSetBindings(const MaterialAsset& __restrict materialAsset)
	{
		FixedCapacityVector<DescriptorSetLayout::Binding, uint8> bindings(Memory::Reserve, materialAsset.GetDescriptorBindings().GetSize() * 2);

		for (const MaterialAsset::DescriptorBinding& __restrict descriptorBinding : materialAsset.GetDescriptorBindings())
		{
			switch (descriptorBinding.m_type)
			{
				case DescriptorContentType::Invalid:
					ExpectUnreachable();
				case DescriptorContentType::Texture:
				{
					const uint32 baseLocation = descriptorBinding.m_location * 2;
					bindings.EmplaceBack(DescriptorSetLayout::Binding::MakeSampledImage(
						baseLocation,
						descriptorBinding.m_shaderStages,
						SampledImageType::Float,
						[](const TexturePreset texturePreset)
						{
							switch (texturePreset)
							{
								case TexturePreset::Greyscale8:
								case TexturePreset::GreyscaleWithAlpha8:
								case TexturePreset::Diffuse:
								case TexturePreset::DiffuseWithAlphaMask:
								case TexturePreset::Normals:
								case TexturePreset::Metalness:
								case TexturePreset::Roughness:
								case TexturePreset::AmbientOcclusion:
								case TexturePreset::Depth:
								case TexturePreset::EmissionColor:
								case TexturePreset::Explicit:
								case TexturePreset::Alpha:
								case TexturePreset::DiffuseWithAlphaTransparency:
								case TexturePreset::BRDF:
								case TexturePreset::EmissionFactor:
									return ImageMappingType::TwoDimensional;
								case TexturePreset::EnvironmentCubemapDiffuseHDR:
								case TexturePreset::EnvironmentCubemapSpecular:
									return ImageMappingType::Cube;
								case TexturePreset::Unknown:
								case TexturePreset::Count:
									ExpectUnreachable();
							}
							ExpectUnreachable();
						}(descriptorBinding.m_samplerInfo.m_texturePreset)
					));
					bindings.EmplaceBack(
						DescriptorSetLayout::Binding::MakeSampler(baseLocation + 1, descriptorBinding.m_shaderStages, SamplerBindingType::Filtering)
					);
				}
				break;
			}
		}

		return bindings;
	}

	inline FixedCapacityVector<PushConstantRange, uint8> PopulatePushConstantRanges(const MaterialAsset& __restrict materialAsset)
	{
		const ArrayView<const PushConstantDefinition> pushConstants = materialAsset.GetPushConstants();

		FixedCapacityVector<PushConstantRange, uint8> pushConstantRanges(Memory::Reserve, uint8(pushConstants.GetSize() + 1));

		uint32 currentOffset = 0;

		auto emplacePushConstantRange =
			[&pushConstantRanges, &currentOffset](const EnumFlags<ShaderStage> shaderStages, const uint16 size, const uint16 alignment)
		{
			currentOffset = Memory::Align(currentOffset, alignment);

			if (pushConstantRanges.IsEmpty() || pushConstantRanges.GetLastElement().m_shaderStages != shaderStages)
			{
				pushConstantRanges.EmplaceBack(PushConstantRange{shaderStages, currentOffset, size});
			}
			else
			{
				PushConstantRange& __restrict lastRange = pushConstantRanges.GetLastElement();
				currentOffset = Memory::Align(currentOffset, alignment);
				lastRange.m_range =
					Math::Range<uint32>::Make(lastRange.m_range.GetMinimum(), (currentOffset + size) - lastRange.m_range.GetMinimum());
			}

			currentOffset += size;
		};

		if (materialAsset.GetVertexShaderAssetGuid() == "5f7722b2-b69b-494b-876b-6951feb6283c"_guid || materialAsset.GetVertexShaderAssetGuid() == "9a4afd2e-3b55-4c06-aa18-7ed5f538298b"_guid)
		{
			// GDC workaround to support Web
			emplacePushConstantRange(ShaderStage::Vertex, sizeof(Math::Vector4f), alignof(Math::Vector4f));
		}

		for (const PushConstantDefinition& __restrict pushConstantDefinition : materialAsset.GetPushConstants())
		{
			emplacePushConstantRange(pushConstantDefinition.m_shaderStages, pushConstantDefinition.m_size, pushConstantDefinition.m_alignment);
		}

		return pushConstantRanges;
	}

	RenderMaterial::RenderMaterial(RenderMaterialCache& cache, const RuntimeMaterial& material)
		: m_cache(cache)
		, m_material(material)
	{
	}

	RenderMaterial::~RenderMaterial() = default;

	void RenderMaterial::CreateDescriptorSetLayout(Rendering::LogicalDevice& logicalDevice)
	{
		Assert(m_material->HasFinishedLoading());
		const MaterialAsset& materialAsset = *m_material->GetAsset();
		if (materialAsset.GetDescriptorBindings().HasElements() && !DescriptorSetLayout::IsValid())
		{
			Threading::UniqueLock lock(m_descriptorLoadingMutex);
			if (!DescriptorSetLayout::IsValid())
			{
				static_cast<DescriptorSetLayout&>(*this) = DescriptorSetLayout{logicalDevice, GetDescriptorSetBindings(materialAsset)};
#if RENDERER_OBJECT_DEBUG_NAMES
				if (m_debugName.HasElements())
				{
					DescriptorSetLayout::SetDebugName(logicalDevice, m_debugName);
				}
#endif
				Assert(DescriptorSetLayout::IsValid());
			}
		}
	}

	void RenderMaterial::CreateBasePipeline(
		Rendering::LogicalDevice& logicalDevice,
		const DescriptorSetLayoutView viewInfoDescriptorSetLayout,
		const DescriptorSetLayoutView transformBufferDescriptorSetLayout
	)
	{
		const MaterialAsset& materialAsset = *m_material->GetAsset();
		m_pushConstantRanges = PopulatePushConstantRanges(materialAsset);

#if RENDERER_OBJECT_DEBUG_NAMES
		m_debugName = String(materialAsset.GetName().GetView());
#endif

		if (materialAsset.GetDescriptorBindings().HasElements())
		{
			CreateDescriptorSetLayout(logicalDevice);
			GraphicsPipeline::CreateBase(
				logicalDevice,
				Array<DescriptorSetLayoutView, 3>{viewInfoDescriptorSetLayout, transformBufferDescriptorSetLayout, *this}.GetDynamicView(),
				m_pushConstantRanges
			);
		}
		else
		{
			GraphicsPipeline::CreateBase(
				logicalDevice,
				Array<DescriptorSetLayoutView, 2>{viewInfoDescriptorSetLayout, transformBufferDescriptorSetLayout}.GetDynamicView(),
				m_pushConstantRanges
			);
		}
		Assert(PipelineLayoutView{*this}.IsValid());
	}

	void RenderMaterial::Destroy(LogicalDevice& logicalDevice)
	{
		GraphicsPipeline::Destroy(logicalDevice);
		DescriptorSetLayout::Destroy(logicalDevice);

		System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache().IterateElements(
			m_materialInstances.GetView(),
			[&logicalDevice](UniquePtr<RenderMaterialInstance>& pMaterialInstance)
			{
				if (pMaterialInstance != nullptr)
				{
					pMaterialInstance->Destroy(logicalDevice);
				}
			}
		);
	}

	Threading::JobBatch RenderMaterial::CreatePipeline(
		LogicalDevice& logicalDevice,
		ShaderCache& shaderCache,
		const RenderPassView renderPass,
		[[maybe_unused]] const Math::Rectangleui outputArea,
		const Math::Rectangleui renderArea,
		const uint8 subpassIndex
	)
	{
		Assert(PipelineLayoutView{*this}.IsValid());
		const MaterialAsset& materialAsset = *m_material->GetAsset();
		const EnumFlags<MaterialAsset::VertexAttributes> vertexAttributes = materialAsset.m_requiredVertexAttributes;

		InlineVector<VertexInputBindingDescription, (uint8)MaterialAsset::VertexAttributes::Count> vertexInputBindingDescriptions;
		InlineVector<VertexInputAttributeDescription, (uint8)MaterialAsset::VertexAttributes::Count * 2> vertexInputAttributeDescriptions;

		uint8 bindingIndex = 0;
		uint8 shaderIndex = 0;
		for (const MaterialAsset::VertexAttributes vertexAttribute : vertexAttributes)
		{
			switch (vertexAttribute)
			{
				case MaterialAsset::VertexAttributes::Position:
					vertexInputBindingDescriptions.EmplaceBack(
						VertexInputBindingDescription{bindingIndex, sizeof(VertexPosition), VertexInputRate::Vertex}
					);

					vertexInputAttributeDescriptions.EmplaceBack(
						VertexInputAttributeDescription{shaderIndex++, bindingIndex, Format::R32G32B32_SFLOAT, 0}
					);
					break;
				case MaterialAsset::VertexAttributes::Normals:
					vertexInputBindingDescriptions.EmplaceBack(
						VertexInputBindingDescription{bindingIndex, sizeof(VertexNormals), VertexInputRate::Vertex}
					);

					vertexInputAttributeDescriptions.EmplaceBack(VertexInputAttributeDescription{shaderIndex++, bindingIndex, Format::R32_UINT, 0});
					vertexInputAttributeDescriptions.EmplaceBack(VertexInputAttributeDescription{
						shaderIndex++,
						bindingIndex,
						Format::R32_UINT,
						static_cast<uint32>(Memory::GetOffsetOf(&VertexNormals::tangent))
					});
					break;
				case MaterialAsset::VertexAttributes::TextureCoordinates:
					vertexInputBindingDescriptions.EmplaceBack(
						VertexInputBindingDescription{bindingIndex, sizeof(VertexTextureCoordinate), VertexInputRate::Vertex}
					);

					vertexInputAttributeDescriptions.EmplaceBack(
						VertexInputAttributeDescription{shaderIndex++, bindingIndex, Format::R32G32_SFLOAT, 0}
					);
					break;
				case MaterialAsset::VertexAttributes::InstanceIdentifier:
					vertexInputBindingDescriptions.EmplaceBack(
						VertexInputBindingDescription{bindingIndex, sizeof(InstanceBuffer::InstanceIndexType), VertexInputRate::Instance}
					);

					vertexInputAttributeDescriptions.EmplaceBack(VertexInputAttributeDescription{shaderIndex++, bindingIndex, Format::R32_UINT, 0});
					break;
				case MaterialAsset::VertexAttributes::All:
					ExpectUnreachable();
			}
			bindingIndex++;
		}

		struct PipelineInfo
		{
			InlineVector<VertexInputBindingDescription, (uint8)MaterialAsset::VertexAttributes::Count> m_vertexInputBindingDescriptions;
			InlineVector<VertexInputAttributeDescription, (uint8)MaterialAsset::VertexAttributes::Count * 2> m_vertexInputAttributeDescriptions;

			VertexStageInfo m_vertexStageInfo;
			FragmentStageInfo m_fragmentStageInfo;
			Optional<FragmentStageInfo*> m_pFragmentStage;
			PrimitiveInfo m_primitiveInfo;
			DepthStencilInfo m_depthStencilInfo;
			Array<Viewport, 1> m_viewports;
			Array<Math::Rectangleui, 1> m_scissors;

			FixedCapacityVector<ColorTargetInfo, uint8> m_colorBlendAttachmentStates;
			Threading::AtomicBitset<255> m_colorAttachments;
		};

		const bool hasFragmentShader = materialAsset.GetPixelShaderAssetGuid().IsValid();
		UniquePtr<PipelineInfo> pPipelineInfo = UniquePtr<PipelineInfo>::Make(PipelineInfo{
			Move(vertexInputBindingDescriptions),
			Move(vertexInputAttributeDescriptions),
			VertexStageInfo{ShaderStageInfo{materialAsset.GetVertexShaderAssetGuid()}},
			FragmentStageInfo{},
			Optional<FragmentStageInfo*>{},
			PrimitiveInfo{
				PrimitiveTopology::TriangleList,
				PolygonMode::Fill,
				WindingOrder::CounterClockwise,
				CullMode::Back * !materialAsset.m_twoSided,
				PrimitiveFlags::DepthClamp * materialAsset.m_enableDepthClamp
			},
			DepthStencilInfo{
				(DepthStencilFlags::DepthTest * materialAsset.m_enableDepthTest) |
					(DepthStencilFlags::DepthWrite * materialAsset.m_enableDepthWrite) |
					(DepthStencilFlags::StencilTest * materialAsset.m_stencilTestSettings.m_enable),
				materialAsset.m_depthCompareOperation,
				StencilOperationState{
					materialAsset.m_stencilTestSettings.m_failureOperation,
					materialAsset.m_stencilTestSettings.m_passOperation,
					materialAsset.m_stencilTestSettings.m_depthFailOperation,
					materialAsset.m_stencilTestSettings.m_compareOperation,
					materialAsset.m_stencilTestSettings.m_compareMask,
					materialAsset.m_stencilTestSettings.m_writeMask,
					materialAsset.m_stencilTestSettings.m_reference
				},
				StencilOperationState{
					materialAsset.m_stencilTestSettings.m_failureOperation,
					materialAsset.m_stencilTestSettings.m_passOperation,
					materialAsset.m_stencilTestSettings.m_depthFailOperation,
					materialAsset.m_stencilTestSettings.m_compareOperation,
					materialAsset.m_stencilTestSettings.m_compareMask,
					materialAsset.m_stencilTestSettings.m_writeMask,
					materialAsset.m_stencilTestSettings.m_reference
				}
			},
			Array<Viewport, 1>{Viewport{renderArea}},
			Array<Math::Rectangleui, 1>{renderArea},
			hasFragmentShader ? FixedCapacityVector<ColorTargetInfo, uint8>{Memory::Reserve, materialAsset.m_attachments.GetSize()}
												: FixedCapacityVector<ColorTargetInfo, uint8>{}
		});
		PipelineInfo& pipelineInfo = *pPipelineInfo;

		pipelineInfo.m_vertexStageInfo.m_bindingDescriptions = pipelineInfo.m_vertexInputBindingDescriptions.GetView();
		pipelineInfo.m_vertexStageInfo.m_attributeDescriptions = pipelineInfo.m_vertexInputAttributeDescriptions.GetView();

		Asset::Manager& assetManager = System::Get<Asset::Manager>();

		Threading::JobBatch jobBatch;

		if (hasFragmentShader)
		{
			const ArrayView<const MaterialAsset::Attachment, uint8> attachments = materialAsset.m_attachments;
			for (const MaterialAsset::Attachment& attachment : attachments)
			{
				const uint8 attachmentIndex = attachments.GetIteratorIndex(&attachment);
				constexpr Asset::Guid defaultViewOutputRenderTargetGuid = "F81D546B-7C63-4B54-BB62-49ACEE00B429"_asset;

				if (attachment.m_renderTargetAssetGuid == defaultViewOutputRenderTargetGuid)
				{
					pipelineInfo.m_colorAttachments.Set(attachmentIndex);
				}
				else if ((attachment.m_renderTargetAssetGuid != SceneViewDrawer::DefaultDepthStencilRenderTargetGuid) & attachment.m_renderTargetAssetGuid.IsValid())
				{
					Threading::Job* pRenderTargetLoadJob = assetManager.RequestAsyncLoadAssetMetadata(
						attachment.m_renderTargetAssetGuid,
						Threading::JobPriority::LoadMaterialStageResources,
						[renderTargetAssetGuid = attachment.m_renderTargetAssetGuid, &logicalDevice, attachmentIndex, &pipelineInfo](
							const ConstByteView data
						)
						{
							Assert(data.HasElements());
							if (UNLIKELY(!data.HasElements()))
							{
								return;
							}

							Serialization::Data renderTargetData(
								ConstStringView{reinterpret_cast<const char*>(data.GetData()), static_cast<uint32>(data.GetDataSize() / sizeof(char))}
							);
							Assert(renderTargetData.IsValid());
							if (UNLIKELY(!renderTargetData.IsValid()))
							{
								return;
							}

							Asset::Manager& assetManager = System::Get<Asset::Manager>();
							const RenderTargetAsset attachmentTextureAsset(renderTargetData, assetManager.GetAssetPath(renderTargetAssetGuid));
							const Serialization::Reader renderTargetReader(renderTargetData);

							const EnumFlags<PhysicalDeviceFeatures> supportedDeviceFeatures = logicalDevice.GetPhysicalDevice().GetSupportedFeatures();
							const TextureAsset::BinaryType desiredBinaryType =
								supportedDeviceFeatures.IsSet(PhysicalDeviceFeatures::TextureCompressionASTC_LDR)
									? Rendering::TextureAsset::BinaryType::ASTC
									: Rendering::TextureAsset::BinaryType::BC;
							const Format desiredFormat = attachmentTextureAsset.GetBinaryAssetInfo(desiredBinaryType).GetFormat();

							const bool isColor = Rendering::GetFormatInfo(desiredFormat).m_flags.AreNoneSet(Rendering::FormatFlags::DepthStencil);
							if (isColor)
							{
								pipelineInfo.m_colorAttachments.Set(attachmentIndex);
							}
						}
					);
					if (pRenderTargetLoadJob != nullptr)
					{
						jobBatch.QueueAfterStartStage(*pRenderTargetLoadJob);
					}
				}
			}

			if (jobBatch.IsValid())
			{
				jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
					[this, &pipelineInfo = *pPipelineInfo](Threading::JobRunnerThread&)
					{
						const MaterialAsset& materialAsset = *m_material->GetAsset();
						const ArrayView<const MaterialAsset::Attachment, uint8> attachments = materialAsset.m_attachments;
						for (const uint8 attachmentIndex : pipelineInfo.m_colorAttachments.GetSetBitsIterator())
						{
							const MaterialAsset::Attachment& __restrict attachment = attachments[attachmentIndex];
							pipelineInfo.m_colorBlendAttachmentStates.EmplaceBack(
								attachment.m_colorBlending.m_colorBlendState,
								attachment.m_colorBlending.m_alphaBlendState,
								attachment.m_storeType == AttachmentStoreType::Store ? ColorChannel::All : ColorChannel{}
							);
						}

						pipelineInfo.m_fragmentStageInfo = {
							ShaderStageInfo{materialAsset.GetPixelShaderAssetGuid()},
							pipelineInfo.m_colorBlendAttachmentStates.GetView()
						};
						pipelineInfo.m_pFragmentStage = pipelineInfo.m_fragmentStageInfo;
					},
					Threading::JobPriority::LoadMaterialStageResources
				));
			}
			else
			{
				for (const uint8 attachmentIndex : pipelineInfo.m_colorAttachments.GetSetBitsIterator())
				{
					const MaterialAsset::Attachment& __restrict attachment = attachments[attachmentIndex];
					pipelineInfo.m_colorBlendAttachmentStates.EmplaceBack(
						attachment.m_colorBlending.m_colorBlendState,
						attachment.m_colorBlending.m_alphaBlendState,
						attachment.m_storeType == AttachmentStoreType::Store ? ColorChannel::All : ColorChannel{}
					);
				}

				pipelineInfo.m_fragmentStageInfo = {
					ShaderStageInfo{materialAsset.GetPixelShaderAssetGuid()},
					pipelineInfo.m_colorBlendAttachmentStates.GetView()
				};
				pipelineInfo.m_pFragmentStage = pipelineInfo.m_fragmentStageInfo;
			}
		}

		EnumFlags<DynamicStateFlags> dynamicStateFlags{};
		// TODO: Hardwired for VR / XR, change to pass in a flag instead
		// dynamicStateFlags |= DynamicStateFlags::Viewport * (outputArea != renderArea); // Needs to be commented out for FSR

		if (jobBatch.IsValid())
		{
			Threading::IntermediateStage& finishedStage = Threading::CreateIntermediateStage();

			jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[this,
			   &logicalDevice,
			   &shaderCache,
			   renderPass,
			   subpassIndex,
			   dynamicStateFlags,
			   pPipelineInfo = Move(pPipelineInfo),
			   &finishedStage](Threading::JobRunnerThread& thread)
				{
					PipelineInfo& pipelineInfo = *pPipelineInfo;
					Threading::JobBatch createAsyncJobBatch = CreateAsync(
						logicalDevice,
						shaderCache,
						m_pipelineLayout,
						renderPass,
						pipelineInfo.m_vertexStageInfo,
						pipelineInfo.m_primitiveInfo,
						pipelineInfo.m_viewports,
						pipelineInfo.m_scissors,
						subpassIndex,
						pipelineInfo.m_pFragmentStage,
						Optional<const MultisamplingInfo*>{},
						pipelineInfo.m_depthStencilInfo,
						Optional<const GeometryStageInfo*>{},
						dynamicStateFlags
					);
					createAsyncJobBatch.QueueAsNewFinishedStage(finishedStage);
					thread.Queue(createAsyncJobBatch);
				},
				Threading::JobPriority::LoadMaterialStageResources
			));

			jobBatch.QueueAsNewFinishedStage(finishedStage);
		}
		else
		{
			Threading::JobBatch createAsyncJobBatch = CreateAsync(
				logicalDevice,
				shaderCache,
				m_pipelineLayout,
				renderPass,
				pipelineInfo.m_vertexStageInfo,
				pipelineInfo.m_primitiveInfo,
				pipelineInfo.m_viewports,
				pipelineInfo.m_scissors,
				subpassIndex,
				pipelineInfo.m_pFragmentStage,
				Optional<const MultisamplingInfo*>{},
				pipelineInfo.m_depthStencilInfo,
				Optional<const GeometryStageInfo*>{},
				dynamicStateFlags
			);
			jobBatch.QueueAsNewFinishedStage(createAsyncJobBatch);
		}

		return jobBatch;
	}

	void RenderMaterial::Draw(
		uint32 firstInstanceIndex,
		const uint32 instanceCount,
		const RenderMeshView mesh,
		const BufferView instanceBuffer,
		const RenderMaterialInstance& materialInstance,
		const RenderCommandEncoderView renderCommandEncoder
	) const
	{
		const DescriptorSetView materialInstanceDescriptorSet = materialInstance.GetDescriptorSet();
		if (materialInstanceDescriptorSet.IsValid())
		{
			const uint32 descriptorOffset = 2;
			Array<DescriptorSetView, 1> descriptorSets{materialInstanceDescriptorSet};
			renderCommandEncoder.BindDescriptorSets(m_pipelineLayout, descriptorSets, descriptorOffset + GetFirstDescriptorSetIndex());
		}

		{
			const MaterialAsset& materialAsset = *m_material->GetAsset();
			EnumFlags<MaterialAsset::VertexAttributes> vertexAttributes = materialAsset.m_requiredVertexAttributes;

			InlineVector<BufferView, (uint8)MaterialAsset::VertexAttributes::Count> vertexBuffers;
			InlineVector<uint64, (uint8)MaterialAsset::VertexAttributes::Count> bufferOffsets;
			InlineVector<uint64, (uint8)MaterialAsset::VertexAttributes::Count> bufferSizes;

			const BufferView meshBuffer = mesh.GetVertexBuffer();
			const Rendering::Index vertexCount = mesh.GetVertexCount();

			const uint64 normalsOffset = Memory::Align(sizeof(Rendering::VertexPosition) * vertexCount, alignof(Rendering::VertexNormals));
			const uint64 textureCoordinatesOffset =
				Memory::Align(normalsOffset + sizeof(Rendering::VertexNormals) * vertexCount, alignof(Rendering::VertexTextureCoordinate));

			for (const MaterialAsset::VertexAttributes vertexAttribute : vertexAttributes)
			{
				switch (vertexAttribute)
				{
					case MaterialAsset::VertexAttributes::Position:
						vertexBuffers.EmplaceBack(meshBuffer);
						bufferOffsets.EmplaceBack(0u);
						bufferSizes.EmplaceBack(sizeof(Rendering::VertexPosition) * vertexCount);
						break;
					case MaterialAsset::VertexAttributes::Normals:
						vertexBuffers.EmplaceBack(meshBuffer);
						bufferOffsets.EmplaceBack(normalsOffset);
						bufferSizes.EmplaceBack(sizeof(Rendering::VertexNormals) * vertexCount);
						break;
					case MaterialAsset::VertexAttributes::TextureCoordinates:
						vertexBuffers.EmplaceBack(meshBuffer);
						bufferOffsets.EmplaceBack(textureCoordinatesOffset);
						bufferSizes.EmplaceBack(sizeof(Rendering::VertexTextureCoordinate) * vertexCount);
						break;
					case MaterialAsset::VertexAttributes::InstanceIdentifier:
						vertexBuffers.EmplaceBack(instanceBuffer);
						bufferOffsets.EmplaceBack(firstInstanceIndex * sizeof(InstanceBuffer::InstanceIndexType));
						bufferSizes.EmplaceBack(sizeof(InstanceBuffer::InstanceIndexType) * instanceCount);
						break;
					case MaterialAsset::VertexAttributes::All:
						ExpectUnreachable();
				}
			}
			renderCommandEncoder.BindVertexBuffers(vertexBuffers.GetView(), bufferOffsets.GetView(), bufferSizes.GetView());
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

	Threading::JobBatch RenderMaterial::LoadRenderMaterialInstanceResources(SceneView& sceneView, const MaterialInstanceIdentifier identifier)
	{
		UniquePtr<RenderMaterialInstance>& pMaterialInstance = m_materialInstances[identifier];
		if (!pMaterialInstance.IsValid())
		{
			if (m_loadingMaterialInstances.Set(identifier))
			{
				pMaterialInstance.CreateInPlace(m_cache, *this);
				return pMaterialInstance->Load(sceneView, identifier);
			}
		}
		return {};
	}

	const RenderMaterialInstance& RenderMaterial::GetOrLoadMaterialInstance(SceneView& sceneView, const MaterialInstanceIdentifier identifier)
	{
		UniquePtr<RenderMaterialInstance>& pMaterialInstance = m_materialInstances[identifier];
		if (pMaterialInstance.IsValid())
		{
			return *pMaterialInstance;
		}

		if (m_loadingMaterialInstances.Set(identifier))
		{
			pMaterialInstance.CreateInPlace(m_cache, *this);

			if (Threading::JobBatch jobBatch = pMaterialInstance->Load(sceneView, identifier))
			{
				Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
			}
			return *pMaterialInstance;
		}
		else
		{
			while (!pMaterialInstance.IsValid())
				;
			return *pMaterialInstance;
		}
	}
	[[maybe_unused]] const bool wasMaterialAssetTypeTypeRegistered = Reflection::Registry::RegisterType<MaterialAssetType>();
}
