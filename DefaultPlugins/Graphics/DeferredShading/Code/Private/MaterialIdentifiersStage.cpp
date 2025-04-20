#include "MaterialIdentifiersStage.h"

#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Data/RenderItem/StaticMeshIdentifier.h>
#include <Engine/Entity/Data/RenderItem/MaterialInstanceIdentifier.h>
#include <Engine/Scene/Scene.h>

#include <Renderer/Commands/CommandBufferView.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Commands/ClearValue.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneData.h>
#include <Renderer/Renderer.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Assets/Material/RuntimeMaterial.h>
#include <Renderer/Assets/Material/RuntimeMaterialInstance.h>
#include <Renderer/Assets/StaticMesh/ForwardDeclarations/VertexPosition.h>
#include <Renderer/RenderOutput/RenderOutput.h>

#include <Renderer/3rdparty/vulkan/vulkan.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/Wrappers/BufferMemoryBarrier.h>

#include <Common/System/Query.h>
#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Math/Power.h>
#include <Common/Memory/AddressOf.h>
#include <Common/Memory/OffsetOf.h>

namespace ngine::Rendering
{
	MaterialIdentifiersStage::MaterialIdentifiersStage(SceneView& sceneView)
		: RenderItemStage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw)
		, m_sceneView(sceneView)
		, m_pipeline(m_sceneView.GetLogicalDevice(), m_sceneView.GetTransformBufferDescriptorSetLayout())
	{
		const SceneRenderStageIdentifier stageIdentifier = System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(Guid);
		m_sceneView.RegisterRenderItemStage(stageIdentifier, *this);
		m_sceneView.SetStageDependentOnCameraProperties(stageIdentifier);
	}

	MaterialIdentifiersStage::~MaterialIdentifiersStage()
	{
		LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		m_pipeline.Destroy(logicalDevice);

		VisibleStaticMeshes::Destroy(logicalDevice);

		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(Guid);
		m_sceneView.DeregisterRenderItemStage(stageIdentifier);
	}

	void MaterialIdentifiersStage::OnBeforeRenderPassDestroyed()
	{
		m_pipeline.PrepareForResize(m_sceneView.GetLogicalDevice());
	}

	Threading::JobBatch MaterialIdentifiersStage::AssignRenderPass(
		const RenderPassView renderPass, const Math::Rectangleui outputArea, const Math::Rectangleui fullRenderArea, const uint8 subpassIndex
	)
	{
		return m_pipeline
		  .CreatePipeline(m_logicalDevice, m_logicalDevice.GetShaderCache(), renderPass, outputArea, fullRenderArea, subpassIndex);
	}

	// Temporary until we refactor buffer creation to use a pool
	constexpr uint32 MaximumVisibleRenderItemCount = 20000;

	void MaterialIdentifiersStage::OnRenderItemsBecomeVisible(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(Guid);

		VisibleStaticMeshes::AddRenderItems(
			m_sceneView,
			*m_sceneView.GetSceneChecked(),
			m_sceneView.GetLogicalDevice(),
			stageIdentifier,
			renderItems,
			MaximumVisibleRenderItemCount,
			graphicsCommandEncoder,
			perFrameStagingBuffer
		);
	}

	void MaterialIdentifiersStage::OnVisibleRenderItemsReset(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(Guid);

		VisibleStaticMeshes::ResetRenderItems(
			m_sceneView,
			*m_sceneView.GetSceneChecked(),
			m_sceneView.GetLogicalDevice(),
			stageIdentifier,
			renderItems,
			MaximumVisibleRenderItemCount,
			graphicsCommandEncoder,
			perFrameStagingBuffer
		);
	}

	void MaterialIdentifiersStage::OnRenderItemsBecomeHidden(
		const Entity::RenderItemMask& renderItems,
		SceneBase& scene,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
		VisibleStaticMeshes::RemoveRenderItems(
			m_sceneView.GetLogicalDevice(),
			renderItems,
			scene,
			graphicsCommandEncoder,
			perFrameStagingBuffer
		);
	}

	void MaterialIdentifiersStage::OnVisibleRenderItemTransformsChanged(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		UNUSED(renderItems);
		UNUSED(graphicsCommandEncoder);
		UNUSED(perFrameStagingBuffer);
	}

	Threading::JobBatch MaterialIdentifiersStage::LoadRenderItemsResources(const Entity::RenderItemMask& renderItems)
	{
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		Entity::SceneRegistry& sceneRegistry = m_sceneView.GetSceneChecked()->GetEntitySceneRegistry();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				m_sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			if (pVisibleComponent->IsStaticMesh(sceneRegistry))
			{
				return VisibleStaticMeshes::TryLoadRenderItemResources(m_sceneView.GetLogicalDevice(), *pVisibleComponent, sceneRegistry);
			}
		}
		return {};
	}

	void MaterialIdentifiersStage::OnSceneUnloaded()
	{
		VisibleStaticMeshes::OnSceneUnloaded(m_sceneView.GetLogicalDevice(), *m_sceneView.GetSceneChecked());
	}

	void MaterialIdentifiersStage::
		OnActiveCameraPropertiesChanged([[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer&)
	{
	}

	bool MaterialIdentifiersStage::ShouldRecordCommands() const
	{
		return m_sceneView.HasActiveCamera() && VisibleStaticMeshes::HasVisibleItems() && m_pipeline.IsValid();
	}

	void MaterialIdentifiersStage::RecordRenderPassCommands(
		RenderCommandEncoder& renderCommandEncoder,
		const ViewMatrices& viewMatrices,
		[[maybe_unused]] const Math::Rectangleui renderArea,
		[[maybe_unused]] const uint8 subpassIndex
	)
	{
		renderCommandEncoder.BindPipeline(m_pipeline);

		const MaterialIdentifiersPipeline::Constants constants{MaterialIdentifiersPipeline::Constants::VertexConstants{
			viewMatrices.GetMatrix(ViewMatrices::Type::View),
			viewMatrices.GetMatrix(ViewMatrices::Type::ViewProjection)
		}};
		m_pipeline
			.PushConstants(m_sceneView.GetLogicalDevice(), renderCommandEncoder, MaterialIdentifiersPipeline::PushConstantRanges, constants);

		const DescriptorSetView transformBufferDescriptorSet = m_sceneView.GetTransformBufferDescriptorSet();
		Array<DescriptorSetView, 1> descriptorSets{transformBufferDescriptorSet};
		renderCommandEncoder.BindDescriptorSets(m_pipeline, descriptorSets, m_pipeline.GetFirstDescriptorSetIndex());

		// TODO: Need to solve bending and camera facing vertex shaders with this, shadows and selection
		// TODO: We'd want a unique object identifier, then we can discard and skip depth testing entirely in the material stage. Simple if
		// check -> discard Essentially becomes the selection stage (could then remove that extra render target) Theoretically we could also
		// encode vertex data to not have to run vertex shaders again later. Would need: outTexCoord (vec2), outNormal (rgb10a2), outTangent
		// (vec3)

		const VisibleRenderItems::VisibleInstanceGroups::ConstDynamicView instanceGroups = VisibleStaticMeshes::GetVisibleItems();
		for (const Optional<VisibleRenderItems::InstanceGroup*> pInstanceGroup : instanceGroups)
		{
			if (pInstanceGroup == nullptr)
			{
				continue;
			}

			if (pInstanceGroup->m_instanceBuffer.GetInstanceCount() > 0)
			{
				const InstanceGroup& __restrict instanceGroup = static_cast<const InstanceGroup&>(*pInstanceGroup);

#if !MATERIAL_IDENTIFIERS_DEPTH_ONLY
				const MaterialIdentifiersConstants::FragmentConstants fragmentConstants{0};
				m_pipeline.PushConstants(
					renderCommandEncoder,
					ShaderStage::Fragment,
					fragmentConstants,
					OFFSET_OF(MaterialIdentifiersConstants, fragmentConstants)
				);
#endif
				uint32 firstInstanceIndex = instanceGroup.m_instanceBuffer.GetFirstInstanceIndex();

				renderCommandEncoder.SetCullMode(m_sceneView.GetLogicalDevice(), CullMode::Back * !instanceGroup.m_isTwoSided);

				{
					const Array vertexBuffers{instanceGroup.m_renderMeshView.GetVertexBuffer(), instanceGroup.m_instanceBuffer.GetBuffer()};
					const Rendering::Index vertexCount = instanceGroup.m_renderMeshView.GetVertexCount();

					const Array<uint64, vertexBuffers.GetSize()> sizes = {
						sizeof(Rendering::VertexPosition) * vertexCount,
						sizeof(InstanceBuffer::InstanceIndexType) * instanceGroup.m_instanceBuffer.GetInstanceCount()
					};

					const Array<uint64, vertexBuffers.GetSize()> offsets = {0u, firstInstanceIndex * sizeof(InstanceBuffer::InstanceIndexType)};
					renderCommandEncoder.BindVertexBuffers(vertexBuffers, offsets.GetDynamicView(), sizes.GetDynamicView());
				}

				// Actual instance index will be 0 since buffers were bound with an offset
				firstInstanceIndex = 0;

				const uint32 firstIndex = 0u;
				const int32_t vertexOffset = 0;
				renderCommandEncoder.DrawIndexed(
					instanceGroup.m_renderMeshView.GetIndexBuffer(),
					0,
					sizeof(Rendering::Index) * instanceGroup.m_renderMeshView.GetIndexCount(),
					instanceGroup.m_renderMeshView.GetIndexCount(),
					instanceGroup.m_instanceBuffer.GetInstanceCount(),
					firstIndex,
					vertexOffset,
					firstInstanceIndex
				);
			}
		}
	}

	VisibleRenderItems::InstanceGroup::SupportResult MaterialIdentifiersStage::InstanceGroup::SupportsComponent(
		const LogicalDevice& logicalDevice, const Entity::HierarchyComponentBase& renderItem, Entity::SceneRegistry& sceneRegistry
	) const
	{
		SupportResult result = VisibleStaticMeshes::InstanceGroup::SupportsComponent(logicalDevice, renderItem, sceneRegistry);
		if (result != SupportResult::Supported)
		{
			return result;
		}

		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();
		const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier =
			materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(renderItem.GetIdentifier());

		Rendering::MaterialCache& materialCache = const_cast<LogicalDevice&>(logicalDevice).GetRenderer().GetMaterialCache();
		const Optional<Rendering::RuntimeMaterialInstance*> pMaterialInstance =
			materialCache.GetInstanceCache().GetMaterialInstance(materialInstanceIdentifier);
		if (pMaterialInstance.IsInvalid() || !pMaterialInstance->HasFinishedLoading())
		{
			Threading::JobBatch jobBatch = materialCache.GetInstanceCache().TryLoad(materialInstanceIdentifier);
			if (jobBatch.IsValid())
			{
				Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
			}

			return SupportResult::Unknown;
		}

		if (!pMaterialInstance->IsValid())
		{
			return SupportResult::Unknown;
		}

		const Rendering::RuntimeMaterial& runtimeMaterial = *materialCache.GetMaterial(pMaterialInstance->GetMaterialIdentifier());
		const Rendering::MaterialAsset& materialAsset = *runtimeMaterial.GetAsset();
		return (m_isTwoSided == materialAsset.m_twoSided) ? SupportResult::Supported : SupportResult::Unsupported;
	}

	UniquePtr<VisibleRenderItems::InstanceGroup> MaterialIdentifiersStage::CreateInstanceGroup(
		LogicalDevice& logicalDevice,
		const Entity::HierarchyComponentBase& component,
		Entity::SceneRegistry& sceneRegistry,
		const uint32 maximumInstanceCount
	)
	{
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();
		const Entity::ComponentIdentifier componentIdentifier = component.GetIdentifier();
		const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier =
			materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);

		Rendering::RuntimeMaterialInstance& materialInstance =
			*m_sceneView.GetLogicalDevice().GetRenderer().GetMaterialCache().GetInstanceCache().GetMaterialInstance(materialInstanceIdentifier);
		Rendering::MaterialCache& materialCache = m_sceneView.GetLogicalDevice().GetRenderer().GetMaterialCache();
		if (!materialInstance.HasFinishedLoading())
		{
			Threading::JobBatch jobBatch = materialCache.GetInstanceCache().TryLoad(materialInstanceIdentifier);
			if (jobBatch.IsValid())
			{
				Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
			}

			return nullptr;
		}

		if (!materialInstance.IsValid())
		{
			return nullptr;
		}

		UniquePtr<InstanceGroup> pInstanceGroup = UniquePtr<InstanceGroup>::Make(logicalDevice, maximumInstanceCount);
		if (LIKELY(pInstanceGroup->m_instanceBuffer.IsValid()))
		{
			const Rendering::RuntimeMaterial& runtimeMaterial = *materialCache.GetMaterial(materialInstance.GetMaterialIdentifier());

			const Rendering::StaticMeshIdentifier meshIdentifier =
				staticMeshIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);
			pInstanceGroup->m_meshIdentifierIndex = meshIdentifier.GetFirstValidIndex();
			const Rendering::MaterialAsset& materialAsset = *runtimeMaterial.GetAsset();
			pInstanceGroup->m_isTwoSided = materialAsset.m_twoSided;

			MeshCache& meshCache = const_cast<LogicalDevice&>(logicalDevice).GetRenderer().GetMeshCache();
			Threading::JobBatch jobBatch = meshCache.TryLoadRenderMesh(
				logicalDevice.GetIdentifier(),
				meshIdentifier,
				Rendering::MeshCache::RenderMeshLoadListenerData{
					*pInstanceGroup,
					[](InstanceGroup& instanceGroup, RenderMeshView newRenderMesh, const EnumFlags<MeshCache::LoadedMeshFlags>)
					{
						instanceGroup.m_renderMeshView = newRenderMesh;
						return EventCallbackResult::Keep;
					}
				}
			);
			if (jobBatch.IsValid())
			{
				Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
			}
			return pInstanceGroup;
		}

		if (pInstanceGroup.IsValid())
		{
			pInstanceGroup->Destroy(logicalDevice);
		}
		return nullptr;
	}
}
