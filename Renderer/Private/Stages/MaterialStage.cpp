#include "Stages/MaterialStage.h"
#include "Stages/MaterialsStage.h"
#include "Stages/StartFrameStage.h"

#include <Common/System/Query.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Data/RenderItem/StaticMeshIdentifier.h>
#include <Engine/Entity/Data/RenderItem/MaterialInstanceIdentifier.h>
#include <Engine/Entity/Data/RenderItem/VisibilityListener.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Scene/Scene.h>

#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneData.h>
#include <Renderer/Framegraph/Framegraph.h>
#include <Renderer/Assets/Material/RenderMaterialInstance.h>
#include <Renderer/Assets/Material/RuntimeMaterial.h>
#include <Renderer/Assets/Material/RuntimeMaterialInstance.h>
#include <Renderer/Assets/Texture/TextureAsset.h>
#include <Renderer/Assets/Texture/RenderTargetAsset.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/PushConstants/PushConstantsData.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Renderer.h>
#include <Renderer/Vulkan/Includes.h>

#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/SubpassDependency.h>
#include <Renderer/Wrappers/BufferMemoryBarrier.h>

#if STAGE_DEPENDENCY_PROFILING
#include <Common/Memory/Containers/Format/String.h>
#include <Common/IO/Format/ZeroTerminatedPathView.h>
#include <Common/IO/Format/Path.h>
#endif

#include <Common/Math/Vector4.h>
#include <Common/Math/Mod.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Memory/AddressOf.h>

namespace ngine::Rendering
{
	MaterialStage::MaterialStage(
		const MaterialIdentifier materialIdentifier, SceneView& sceneView, const float renderAreaFactor, const EnumFlags<Flags> flags
	)
		: RenderItemStage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw, flags)
		, m_sceneView(sceneView)
		, m_material(
				m_sceneView.GetRenderMaterialCache().FindOrLoad(sceneView.GetLogicalDevice().GetRenderer().GetMaterialCache(), materialIdentifier)
			)
		, m_renderAreaFactor(renderAreaFactor)
	{
		Assert(materialIdentifier.IsValid());
		MaterialCache& materialCache = sceneView.GetLogicalDevice().GetRenderer().GetMaterialCache();
		Threading::JobBatch jobBatch = materialCache.TryLoad(
			materialIdentifier,
			MaterialCache::OnLoadedListenerData{
				*this,
				[materialIdentifier](MaterialStage& materialStage)
				{
					if (const Optional<const Rendering::MaterialAsset*> pMaterialAsset = *materialStage.GetMaterial().GetMaterial().GetAsset())
					{
#if STAGE_DEPENDENCY_PROFILING
						materialStage.m_debugMarkerName =
							String().Format("MaterialStage ({})", pMaterialAsset->GetMetaDataFilePath().GetFileNameWithoutExtensions());
#endif

						Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
						const SceneRenderStageIdentifier stageIdentifier =
							stageCache.FindOrRegisterAsset(pMaterialAsset->GetGuid(), pMaterialAsset->GetName(), Rendering::StageFlags::Hidden);

						materialStage.m_sceneView.RegisterRenderItemStage(stageIdentifier, materialStage);
						materialStage.m_sceneView.GetMaterialsStage().RegisterStage(materialIdentifier, materialStage);

						for (const Asset::Guid dependentStage : pMaterialAsset->GetDependentStages())
						{
							const SceneRenderStageIdentifier dependentStageIdentifier =
								System::Get<Rendering::Renderer>().GetStageCache().FindIdentifier(dependentStage);
							Rendering::SceneRenderStage& stage = *materialStage.m_sceneView.GetSceneRenderStage(dependentStageIdentifier);
							stage.AddDependency(materialStage);
						}
					}
					return EventCallbackResult::Remove;
				}
			}
		);
		// TODO: Return the job batch and move the responsibility of queuing it
		if (jobBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
		}
	}

	MaterialStage::~MaterialStage()
	{
		if (const Optional<const MaterialAsset*> pMaterialAsset = m_material.GetMaterial().GetAsset())
		{
			TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();

			for (const MaterialAsset::Attachment& attachment : pMaterialAsset->m_attachments)
			{
				const TextureIdentifier renderTargetIdentifier = textureCache.FindIdentifier(attachment.m_renderTargetAssetGuid);
				if (renderTargetIdentifier.IsValid())
				{
					[[maybe_unused]] const bool wasRemoved =
						textureCache.RemoveRenderTextureListener(m_sceneView.GetLogicalDevice().GetIdentifier(), renderTargetIdentifier, this);
				}
			}

			Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
			const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(pMaterialAsset->GetGuid());
			m_sceneView.DeregisterRenderItemStage(stageIdentifier);
		}

		MaterialCache& materialCache = m_sceneView.GetLogicalDevice().GetRenderer().GetMaterialCache();
		[[maybe_unused]] const bool wasRemoved = materialCache.RemoveOnLoadListener(m_material.GetMaterial().GetIdentifier(), this);

		m_material.Destroy(m_logicalDevice);

		VisibleStaticMeshes::Destroy(m_logicalDevice);
	}

	void MaterialStage::OnBeforeRenderPassDestroyed()
	{
		m_material.PrepareForResize(m_logicalDevice);
	}

	Threading::JobBatch MaterialStage::AssignRenderPass(
		const RenderPassView renderPass, const Math::Rectangleui outputArea, const Math::Rectangleui fullRenderArea, const uint8 subpassIndex
	)
	{
		MaterialCache& materialCache = m_sceneView.GetLogicalDevice().GetRenderer().GetMaterialCache();
		Threading::JobBatch jobBatch{Threading::JobBatch::IntermediateStage};
		Threading::IntermediateStage& finishedStage = Threading::CreateIntermediateStage();
		finishedStage.AddSubsequentStage(jobBatch.GetFinishedStage());

		Threading::JobBatch materialJobBatch = materialCache.TryLoad(
			m_material.GetMaterial().GetIdentifier(),
			MaterialCache::OnLoadedListenerData{
				*this,
				[renderPass, outputArea, fullRenderArea, subpassIndex, &finishedStage](MaterialStage& materialStage)
				{
					Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
					if (materialStage.m_material.GetMaterial().IsValid())
					{
						const PipelineLayoutView pipelineLayout{materialStage.m_material};
						if (!pipelineLayout.IsValid())
						{
							materialStage.m_material.CreateBasePipeline(
								materialStage.m_logicalDevice,
								materialStage.m_sceneView.GetMatrices().GetDescriptorSetLayout(),
								materialStage.m_sceneView.GetTransformBufferDescriptorSetLayout()
							);
						}

						Threading::JobBatch pipelineJobBatch = materialStage.m_material.CreatePipeline(
							materialStage.m_logicalDevice,
							materialStage.m_logicalDevice.GetShaderCache(),
							renderPass,
							outputArea,
							{fullRenderArea.GetPosition(), (Math::Vector2ui)((Math::Vector2f)fullRenderArea.GetSize())},
							subpassIndex
						);
						if (pipelineJobBatch.IsValid())
						{
							pipelineJobBatch.GetFinishedStage().AddSubsequentStage(finishedStage);
							thread.Queue(pipelineJobBatch.GetStartStage());
						}
						else
						{
							finishedStage.SignalExecutionFinishedAndDestroying(thread);
						}
					}
					else
					{
						finishedStage.SignalExecutionFinishedAndDestroying(thread);
					}
					return EventCallbackResult::Remove;
				}
			}
		);
		jobBatch.QueueAfterStartStage(materialJobBatch);
		return jobBatch;
	}

	// Temporary until we refactor buffer creation to use a pool
	constexpr uint32 MaximumVisibleRenderItemCount = 20000;

	void MaterialStage::OnRenderItemsBecomeVisible(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const Rendering::MaterialAsset& materialAsset = *m_material.GetMaterial().GetAsset();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(materialAsset.GetGuid());
		return VisibleStaticMeshes::AddRenderItems(
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

	void MaterialStage::OnVisibleRenderItemsReset(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const Rendering::MaterialAsset& materialAsset = *m_material.GetMaterial().GetAsset();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(materialAsset.GetGuid());
		return VisibleStaticMeshes::ResetRenderItems(
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

	void MaterialStage::OnRenderItemsBecomeVisibleFromMaterialsStage(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(MaterialsStage::TypeGuid);
		return VisibleStaticMeshes::AddRenderItems(
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

	void MaterialStage::OnVisibleRenderItemsResetFromMaterialsStage(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(MaterialsStage::TypeGuid);
		return VisibleStaticMeshes::ResetRenderItems(
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

	void MaterialStage::OnRenderItemsBecomeHidden(
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

	void MaterialStage::OnVisibleRenderItemTransformsChanged(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		UNUSED(renderItems);
		UNUSED(graphicsCommandEncoder);
		UNUSED(perFrameStagingBuffer);
	}

	Threading::JobBatch MaterialStage::LoadRenderItemsResources(const Entity::RenderItemMask& renderItems)
	{
		Entity::SceneRegistry& sceneRegistry = m_sceneView.GetScene().GetEntitySceneRegistry();

		Threading::JobBatch fullBatch;
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();

		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				m_sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			const Entity::ComponentIdentifier componentIdentifier = pVisibleComponent->GetIdentifier();
			const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier =
				materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);

			Threading::JobBatch batch;

			Threading::JobBatch materialBatch = m_material.LoadRenderMaterialInstanceResources(m_sceneView, materialInstanceIdentifier);
			if (materialBatch.IsValid())
			{
				batch.QueueAfterStartStage(materialBatch.GetStartStage());
			}

			{
				Threading::JobBatch meshBatch =
					VisibleStaticMeshes::TryLoadRenderItemResources(m_sceneView.GetLogicalDevice(), *pVisibleComponent, sceneRegistry);
				batch.QueueAfterStartStage(meshBatch);
			}
			fullBatch.QueueAfterStartStage(batch);
		}

		return fullBatch;
	}

	void MaterialStage::OnSceneUnloaded()
	{
		VisibleStaticMeshes::OnSceneUnloaded(m_sceneView.GetLogicalDevice(), *m_sceneView.GetSceneChecked());
	}

	void MaterialStage::
		OnActiveCameraPropertiesChanged([[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer&)
	{
	}

	bool MaterialStage::ShouldRecordCommands() const
	{
		const Optional<const Rendering::MaterialAsset*> pMaterialAsset = *m_material.GetMaterial().GetAsset();
		const bool isValid = m_material.IsValid();
		const bool hasAnyClearOperations = isValid && pMaterialAsset.IsValid() &&
		                                   pMaterialAsset->m_attachments.GetView().Any(
																				 [](const auto& attachment)
																				 {
																					 return attachment.m_loadType == AttachmentLoadType::Clear;
																				 }
																			 );
		return isValid && (hasAnyClearOperations || HasVisibleItems()) && RenderItemStage::ShouldRecordCommands();
	}

	void MaterialStage::RecordRenderPassCommands(
		RenderCommandEncoder& renderCommandEncoder,
		const ViewMatrices&,
		[[maybe_unused]] const Math::Rectangleui renderArea,
		[[maybe_unused]] const uint8 subpassIndex
	)
	{
		if (!m_material.IsValid())
		{
			return;
		}
		renderCommandEncoder.BindPipeline(m_material);

		const DescriptorSetView viewInfoDescriptorSet = m_sceneView.GetMatrices().GetDescriptorSet();
		const DescriptorSetView transformBufferDescriptorSet = m_sceneView.GetTransformBufferDescriptorSet();
		Array<DescriptorSetView, 2> descriptorSets{viewInfoDescriptorSet, transformBufferDescriptorSet};
		renderCommandEncoder.BindDescriptorSets(m_material, descriptorSets, m_material.GetFirstDescriptorSetIndex());

		const MaterialAsset& __restrict materialAsset = *m_material.GetMaterial().GetAsset();

		PushConstantsData::Container pushConstantBuffer;
		PushConstantsData::ViewType pushConstantData = PushConstantsData::ViewType::Make(pushConstantBuffer);

		const ArrayView<const PushConstantRange> pushConstantRanges = m_material.GetPushConstantRanges();

		const bool requiresJitterOffsets = materialAsset.GetVertexShaderAssetGuid() == "5f7722b2-b69b-494b-876b-6951feb6283c"_guid ||
		                                   materialAsset.GetVertexShaderAssetGuid() == "9a4afd2e-3b55-4c06-aa18-7ed5f538298b"_guid;
		if (requiresJitterOffsets)
		{
			// GDC demo workaround for needing push constant at index 0 to support Web
			Math::Vector4f dummy{};
			pushConstantData.WriteAndSkip(dummy);
		}

		const VisibleRenderItems::VisibleInstanceGroups::ConstDynamicView instanceGroups = GetVisibleItems();
		for (const Optional<VisibleRenderItems::InstanceGroup*> pInstanceGroup : instanceGroups)
		{
			if (pInstanceGroup == nullptr)
			{
				continue;
			}

			if (pInstanceGroup->m_instanceBuffer.GetInstanceCount() > 0)
			{
				const InstanceGroup& instanceGroup = static_cast<const InstanceGroup&>(*pInstanceGroup);

				const RenderMaterialInstance& __restrict materialInstance = *instanceGroup.m_materialInstance;

				// TODO: Look into specialization constants
				// https://github.com/SaschaWillems/Vulkan/blob/master/examples/specializationconstants/specializationconstants.cpp
				// http://web.engr.oregonstate.edu/~mjb/vulkan/Handouts/SpecializationConstants.1pp.pdf

				const MaterialAsset::PushConstants::Container::ConstView pushConstantDefinitions = materialAsset.GetPushConstants();
				if (pushConstantDefinitions.HasElements() | requiresJitterOffsets)
				{
					const PushConstantsData::ConstViewType pushConstants = materialInstance.GetMaterialInstance().GetPushConstantsData();
					PushConstantsData::SizeType baseOffset = 0;
					PushConstantsData::SizeType pushConstantOffset =
						PushConstantsData::SizeType((uintptr)pushConstantData.GetData() - (uintptr)pushConstantData.GetData());
					for (const PushConstantDefinition& __restrict pushConstantDefinition : pushConstantDefinitions)
					{
						pushConstantOffset = Memory::Align(pushConstantOffset, pushConstantDefinition.m_alignment);
						pushConstantData.GetSubView(PushConstantsData::SizeType(pushConstantOffset + baseOffset), pushConstantDefinition.m_size)
							.CopyFrom(pushConstants.GetSubView(baseOffset, pushConstantDefinition.m_size));
						baseOffset += pushConstantDefinition.m_size;
					}

					m_material.PushConstants(m_logicalDevice, renderCommandEncoder, pushConstantRanges, pushConstantBuffer);
				}

				m_material.Draw(
					instanceGroup.m_instanceBuffer.GetFirstInstanceIndex(),
					instanceGroup.m_instanceBuffer.GetInstanceCount(),
					instanceGroup.m_renderMeshView,
					instanceGroup.m_instanceBuffer.GetBuffer(),
					instanceGroup.m_materialInstance,
					renderCommandEncoder
				);
			}
		}
	}

	VisibleRenderItems::InstanceGroup::SupportResult MaterialStage::InstanceGroup::SupportsComponent(
		const LogicalDevice&, const Entity::HierarchyComponentBase& component, Entity::SceneRegistry& sceneRegistry
	) const
	{
		const Entity::ComponentIdentifier componentIdentifier = component.GetIdentifier();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();

		const Rendering::StaticMeshIdentifier meshIdentifier =
			staticMeshIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);
		const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier =
			materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);

		return (meshIdentifier.IsValid() &&
		        (m_meshIdentifierIndex == meshIdentifier.GetFirstValidIndex()) & (m_materialInstanceIdentifier == materialInstanceIdentifier))
		         ? SupportResult::Supported
		         : SupportResult::Unsupported;
	}

	bool MaterialStage::MoveMaterialInstancesFrom(
		MaterialStage& otherStage, const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier
	)
	{
		for (UniquePtr<VisibleRenderItems::InstanceGroup>& pInstanceGroup :
		     otherStage.m_instanceGroupIdentifiers.GetValidElementView(otherStage.m_visibleInstanceGroups.GetView()))
		{
			if (pInstanceGroup == nullptr)
			{
				continue;
			}

			InstanceGroup& finalInstanceGroup = static_cast<InstanceGroup&>(*pInstanceGroup);
			if (finalInstanceGroup.m_materialInstanceIdentifier == materialInstanceIdentifier)
			{
				const VisibleInstanceGroupIdentifier newInstanceGroupIdentifier = m_instanceGroupIdentifiers.AcquireIdentifier();
				Assert(newInstanceGroupIdentifier.IsValid());
				if (LIKELY(newInstanceGroupIdentifier.IsValid()))
				{
					Assert(m_visibleInstanceGroups[newInstanceGroupIdentifier].IsInvalid());
					m_visibleInstanceGroups[newInstanceGroupIdentifier] = UniquePtr<InstanceGroup>::Make(Move(finalInstanceGroup));
					m_instanceGroupCount++;

					const VisibleInstanceGroupIdentifier previousInstanceGroupIdentifier =
						VisibleInstanceGroupIdentifier::MakeFromValidIndex(otherStage.m_visibleInstanceGroups.GetElementIndex(pInstanceGroup));
					otherStage.m_visibleInstanceGroups[previousInstanceGroupIdentifier].DestroyElement();
					otherStage.m_instanceGroupIdentifiers.ReturnIdentifier(previousInstanceGroupIdentifier);
					otherStage.m_instanceGroupCount--;
					return true;
				}
				break;
			}
		}
		return false;
	}

	UniquePtr<VisibleRenderItems::InstanceGroup> MaterialStage::CreateInstanceGroup(
		LogicalDevice& logicalDevice,
		const Entity::HierarchyComponentBase& component,
		Entity::SceneRegistry& sceneRegistry,
		const uint32 maximumInstanceCount
	)
	{
		const Entity::ComponentIdentifier componentIdentifier = component.GetIdentifier();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();

		const Rendering::StaticMeshIdentifier meshIdentifier =
			staticMeshIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);
		const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier =
			materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(componentIdentifier);

		const Rendering::RenderMaterialInstance& renderMaterialInstance =
			m_material.GetOrLoadMaterialInstance(m_sceneView, materialInstanceIdentifier);
		if (!renderMaterialInstance.IsValid())
		{
			return nullptr;
		}

		UniquePtr<InstanceGroup> pInstanceGroup =
			UniquePtr<InstanceGroup>::Make(logicalDevice, materialInstanceIdentifier, renderMaterialInstance, maximumInstanceCount);
		if (LIKELY(pInstanceGroup->m_instanceBuffer.IsValid()))
		{
			pInstanceGroup->m_meshIdentifierIndex = meshIdentifier.GetFirstValidIndex();
			MeshCache& meshCache = const_cast<MeshCache&>(logicalDevice.GetRenderer().GetMeshCache());
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

			Entity::ComponentTypeSceneData<Entity::Data::RenderItem::VisibilityListener>& visibilityListenerSceneData =
				sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::VisibilityListener>();
			if (const Optional<Entity::Data::RenderItem::VisibilityListener*> pVisibilityListener = visibilityListenerSceneData.GetComponentImplementation(componentIdentifier))
			{
				pVisibilityListener->Invoke(logicalDevice, jobBatch);
			}

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
