#include "Stages/MaterialsStage.h"

#include <Renderer/Renderer.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneData.h>
#include <Renderer/Stages/MaterialStage.h>
#include <Renderer/Stages/PerFrameStagingBuffer.h>
#include <Renderer/Assets/Material/RuntimeMaterial.h>
#include <Renderer/Assets/Material/RuntimeMaterialInstance.h>
#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/BlitCommandEncoder.h>
#include <Renderer/Buffers/StagingBuffer.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/Data/RenderItem/StaticMeshIdentifier.h>
#include <Engine/Entity/Data/RenderItem/MaterialInstanceIdentifier.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/System/Query.h>

namespace ngine::Rendering
{
	MaterialsStage::MaterialsStage(SceneView& sceneView)
		: RenderItemStage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw)
		, m_sceneView(sceneView)
		, m_renderItemsDataBuffer(
				sceneView.GetLogicalDevice().GetPhysicalDevice().GetSupportedFeatures().AreAllSet(
					PhysicalDeviceFeatures::BufferDeviceAddress | PhysicalDeviceFeatures::PartiallyBoundDescriptorBindings |
					PhysicalDeviceFeatures::AccelerationStructure
				)
					? StorageBuffer(
							sceneView.GetLogicalDevice(),
							sceneView.GetLogicalDevice().GetPhysicalDevice(),
							sceneView.GetLogicalDevice().GetDeviceMemoryPool(),
							sizeof(RenderItemData) * Entity::RenderItemIdentifier::MaximumCount
						)
					: StorageBuffer()
			)
	{
		Rendering::StageCache& stageCache = sceneView.GetLogicalDevice().GetRenderer().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier =
			stageCache.FindOrRegisterAsset(TypeGuid, MAKE_UNICODE_LITERAL("Materials"), Rendering::StageFlags::Hidden);
		sceneView.RegisterRenderItemStage(stageIdentifier, *this);
	}

	MaterialsStage::~MaterialsStage()
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(TypeGuid);
		m_sceneView.DeregisterRenderItemStage(stageIdentifier);

		m_renderItemsDataBuffer.Destroy(m_sceneView.GetLogicalDevice(), m_sceneView.GetLogicalDevice().GetDeviceMemoryPool());

		// Deregister material listeners
		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		Rendering::MaterialCache& materialCache = renderer.GetMaterialCache();

		for (Optional<MaterialStage*>& pMaterial : materialCache.GetValidElementView(m_materialStages.GetView()))
		{
			if (pMaterial.IsValid())
			{
				const typename MaterialIdentifier::IndexType materialIndex = m_materialStages.GetView().GetIteratorIndex(&pMaterial);
				const MaterialIdentifier materialIdentifier = MaterialIdentifier::MakeFromValidIndex(materialIndex);

				if (const Optional<Rendering::RuntimeMaterial*> pRuntimeMaterial = materialCache.GetAssetData(materialIdentifier).m_pMaterial)
				{
					pRuntimeMaterial->OnMaterialInstanceParentChanged.Remove(this);
				}
			}
		}
	}

	void MaterialsStage::RegisterStage(const Rendering::MaterialIdentifier identifier, MaterialStage& stage)
	{
		m_materialStages[identifier] = &stage;

		Rendering::MaterialCache& materialCache = System::Get<Rendering::Renderer>().GetMaterialCache();
		Rendering::RuntimeMaterial& runtimeMaterial = *materialCache.GetAssetData(identifier).m_pMaterial;

		runtimeMaterial.OnMaterialInstanceParentChanged.Add(
			*this,
			[](
				MaterialsStage& stage,
				const MaterialInstanceIdentifier materialInstanceIdentifier,
				const MaterialIdentifier previousParent,
				const MaterialIdentifier newParent
			)
			{
				MaterialStage& previousMaterialStage = *stage.m_materialStages[previousParent];
				MaterialStage& newMaterialStage = *stage.m_materialStages[newParent];
				[[maybe_unused]] const bool wasMoved =
					newMaterialStage.MoveMaterialInstancesFrom(previousMaterialStage, materialInstanceIdentifier);
				Assert(wasMoved);
			}
		);
	}

	void MaterialsStage::OnRenderItemsBecomeVisible(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Entity::RenderItemMask renderItemsTemp = renderItems;
		Entity::RenderItemMask stageRenderItems;
		Entity::RenderItemMask invalidRenderItems;

		LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();
		Rendering::MaterialCache& materialCache = logicalDevice.GetRenderer().GetMaterialCache();

		Entity::SceneRegistry& sceneRegistry = m_sceneView.GetSceneChecked()->GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();

		auto findMaterialIdentifier =
			[&sceneView = m_sceneView, &materialCache, &materialInstanceIdentifierSceneData](const uint32 renderItemIndex)
		{
			const Entity::ComponentIdentifier componentIdentifier =
				sceneView.GetVisibleRenderItemComponentIdentifier(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(componentIdentifier.IsValid());
			if (UNLIKELY_ERROR(componentIdentifier.IsInvalid()))
			{
				return Rendering::MaterialIdentifier{};
			}

			const Optional<Entity::Data::RenderItem::MaterialInstanceIdentifier*> pMaterialInstanceComponent =
				materialInstanceIdentifierSceneData.GetComponentImplementation(componentIdentifier);
			Assert(pMaterialInstanceComponent.IsValid());
			if (UNLIKELY_ERROR(pMaterialInstanceComponent.IsInvalid()))
			{
				return Rendering::MaterialIdentifier{};
			}

			const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier = *pMaterialInstanceComponent;

			Rendering::RuntimeMaterialInstance& materialInstance =
				*materialCache.GetInstanceCache().GetMaterialInstance(materialInstanceIdentifier);
			if (!materialInstance.HasFinishedLoading())
			{
				Threading::JobBatch jobBatch = materialCache.GetInstanceCache().TryLoad(materialInstanceIdentifier);
				if (jobBatch.IsValid())
				{
					Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
				}

				return Rendering::MaterialIdentifier{};
			}

			return materialInstance.GetMaterialIdentifier();
		};

		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();

		do
		{
			Rendering::MaterialIdentifier currentMaterialIdentifier;
			do
			{
				const Memory::BitIndex<uint32> firstRenderItemIndex = renderItemsTemp.GetFirstSetIndex();
				if (firstRenderItemIndex.IsInvalid())
				{
					return;
				}
				currentMaterialIdentifier = findMaterialIdentifier(*firstRenderItemIndex);
				if (!currentMaterialIdentifier.IsValid())
				{
					renderItemsTemp.Clear(Entity::RenderItemIdentifier::MakeFromValidIndex(*firstRenderItemIndex));
				}
			} while (!currentMaterialIdentifier.IsValid());

			for (const uint32 renderItemIndex : renderItemsTemp.GetSetBitsIterator(0, maximumUsedRenderItemCount))
			{
				const Rendering::MaterialIdentifier materialIdentifier = findMaterialIdentifier(renderItemIndex);
				if (materialIdentifier == currentMaterialIdentifier)
				{
					stageRenderItems.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
				}
				else if (!materialIdentifier.IsValid())
				{
					invalidRenderItems.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
				}
			}

			if (LIKELY(m_materialStages[currentMaterialIdentifier] != nullptr))
			{
				MaterialStage& stage = *m_materialStages[currentMaterialIdentifier];
				stage.m_visibleRenderItems |= stageRenderItems;
				m_activeMaterials.Set(currentMaterialIdentifier);
				stage.OnRenderItemsBecomeVisibleFromMaterialsStage(
					stageRenderItems,
					graphicsCommandEncoder,
					perFrameStagingBuffer

				);

				if (m_renderItemsDataBuffer.IsValid())
				{
					for (const uint32 renderItemIndex : stageRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
					{
						const Entity::ComponentIdentifier componentIdentifier =
							m_sceneView.GetVisibleRenderItemComponentIdentifier(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
						Assert(componentIdentifier.IsValid());
						if (UNLIKELY_ERROR(componentIdentifier.IsInvalid()))
						{
							continue;
						}

						const Optional<Entity::Data::RenderItem::MaterialInstanceIdentifier*> pMaterialInstanceComponent =
							materialInstanceIdentifierSceneData.GetComponentImplementation(componentIdentifier);
						Assert(pMaterialInstanceComponent.IsValid());
						if (UNLIKELY_ERROR(pMaterialInstanceComponent.IsInvalid()))
						{
							continue;
						}

						const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier = *pMaterialInstanceComponent;

						Rendering::RuntimeMaterialInstance& materialInstance =
							*materialCache.GetInstanceCache().GetMaterialInstance(materialInstanceIdentifier);
						const Rendering::MaterialAsset& materialAsset = *stage.GetMaterial().GetMaterial().GetAsset();

						const auto findTextureIdentifier =
							[descriptorBindings = materialAsset.GetDescriptorBindings(),
						   descriptorContents = materialInstance.GetDescriptorContents()](const TexturePreset texturePreset)
						{
							for (const MaterialAsset::DescriptorBinding& descriptorBinding : descriptorBindings)
							{
								switch (descriptorBinding.m_type)
								{
									case DescriptorContentType::Invalid:
										ExpectUnreachable();
									case DescriptorContentType::Texture:
									{
										if (descriptorBinding.m_samplerInfo.m_texturePreset == texturePreset)
										{
											const uint8 descriptorIndex = descriptorBindings.GetIteratorIndex(&descriptorBinding);
											return descriptorContents[descriptorIndex].m_textureData.m_textureIdentifier;
										}
									}
									break;
								}
							}

							return TextureIdentifier();
						};

						const TextureIdentifier diffuseTextureIdentifier = findTextureIdentifier(TexturePreset::Diffuse);
						const TextureIdentifier roughnessTextureIdentifier = findTextureIdentifier(TexturePreset::Roughness);

						const Optional<Entity::Data::RenderItem::StaticMeshIdentifier*> pStaticMeshIdentifierComponent =
							staticMeshIdentifierSceneData.GetComponentImplementation(componentIdentifier);
						Assert(pStaticMeshIdentifierComponent.IsValid());
						if (UNLIKELY_ERROR(pStaticMeshIdentifierComponent.IsInvalid()))
						{
							continue;
						}

						const Rendering::StaticMeshIdentifier staticMeshIdentifier = *pStaticMeshIdentifierComponent;

						RenderItemData renderItemData{
							staticMeshIdentifier.GetFirstValidIndex(),
							diffuseTextureIdentifier.IsValid() ? diffuseTextureIdentifier.GetFirstValidIndex() : -1,
							roughnessTextureIdentifier.IsValid() ? roughnessTextureIdentifier.GetFirstValidIndex() : -1
						};
						perFrameStagingBuffer.CopyToBuffer(
							m_logicalDevice,
							m_logicalDevice.GetCommandQueue(QueueFamily::Graphics),
							graphicsCommandEncoder,
							ConstByteView::Make(renderItemData),
							m_renderItemsDataBuffer,
							sizeof(RenderItemData) * renderItemIndex
						);
					}
				}
			}

			renderItemsTemp.Clear(stageRenderItems);
			renderItemsTemp.Clear(invalidRenderItems);
			stageRenderItems.ClearAll();
		} while (renderItemsTemp.AreAnySet());

		if (m_pForwardingStage.IsValid())
		{
			stageRenderItems.ClearAll();

			for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
			{
				const Rendering::MaterialIdentifier materialIdentifier = findMaterialIdentifier(renderItemIndex);
				if (materialIdentifier.IsValid())
				{
					if (LIKELY(m_materialStages[materialIdentifier] != nullptr))
					{
						MaterialStage& stage = *m_materialStages[materialIdentifier];
						const Rendering::MaterialAsset& materialAsset = *stage.GetMaterial().GetMaterial().GetAsset();
						const Asset::Guid vertexShaderAssetGuid = materialAsset.m_vertexShaderAssetGuid;
						if (vertexShaderAssetGuid == "5f7722b2-b69b-494b-876b-6951feb6283c"_asset || vertexShaderAssetGuid == "9a4afd2e-3b55-4c06-aa18-7ed5f538298b"_asset)
						{
							const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
							stageRenderItems.Set(renderItemIdentifier);
						}
					}
				}
			}

			m_pForwardingStage->OnRenderItemsBecomeVisible(stageRenderItems, graphicsCommandEncoder, perFrameStagingBuffer);
		}
	}

	void MaterialsStage::OnVisibleRenderItemsReset(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();

		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		Rendering::StageCache& stageCache = renderer.GetStageCache();
		Rendering::MaterialCache& materialCache = renderer.GetMaterialCache();
		const typename MaterialIdentifier::IndexType maximumUsedMaterialCount = materialCache.GetMaximumUsedIdentifierCount();
		Scene& scene = m_sceneView.GetScene();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(TypeGuid);
		for (const typename MaterialIdentifier::IndexType materialIndex : m_activeMaterials.GetSetBitsIterator(0, maximumUsedMaterialCount))
		{
			const MaterialIdentifier materialIdentifier = MaterialIdentifier::MakeFromValidIndex(materialIndex);
			MaterialStage& stage = *m_materialStages[materialIdentifier];

			const Entity::RenderItemMask materialRenderItems = renderItems & stage.m_visibleRenderItems;
			if (materialRenderItems.AreAnySet())
			{
				stage.m_visibleRenderItems &= ~materialRenderItems;

				m_materialStages[materialIdentifier]
					->OnRenderItemsBecomeHidden(materialRenderItems, scene, graphicsCommandEncoder, perFrameStagingBuffer);

				if (stage.m_visibleRenderItems.AreNoneSet())
				{
					m_activeMaterials.Clear(materialIdentifier);
				}

				for (const uint32 renderItemIndex : materialRenderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
				{
					const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
					m_sceneView.GetSubmittedRenderItemStageMask(renderItemIdentifier).Clear(stageIdentifier);
					m_sceneView.GetQueuedRenderItemStageMask(renderItemIdentifier).Clear(stageIdentifier);
				}
			}
		}

		OnRenderItemsBecomeVisible(
			renderItems,
			graphicsCommandEncoder,
			perFrameStagingBuffer

		);

		if (m_pForwardingStage.IsValid())
		{
			m_pForwardingStage->OnVisibleRenderItemsReset(renderItems, graphicsCommandEncoder, perFrameStagingBuffer);
		}
	}

	void MaterialsStage::OnRenderItemsBecomeHidden(
		const Entity::RenderItemMask& renderItems,
		SceneBase& scene,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Rendering::MaterialCache& materialCache = System::Get<Rendering::Renderer>().GetMaterialCache();
		const typename MaterialIdentifier::IndexType maximumUsedMaterialCount = materialCache.GetMaximumUsedIdentifierCount();
		for (const typename MaterialIdentifier::IndexType materialIndex : m_activeMaterials.GetSetBitsIterator(0, maximumUsedMaterialCount))
		{
			const MaterialIdentifier materialIdentifier = MaterialIdentifier::MakeFromValidIndex(materialIndex);
			MaterialStage& stage = *m_materialStages[materialIdentifier];

			const Entity::RenderItemMask materialRenderItems = renderItems & stage.m_visibleRenderItems;
			if (materialRenderItems.AreAnySet())
			{
				stage.m_visibleRenderItems &= ~materialRenderItems;

				m_materialStages[materialIdentifier]
					->OnRenderItemsBecomeHidden(materialRenderItems, scene, graphicsCommandEncoder, perFrameStagingBuffer);

				if (stage.m_visibleRenderItems.AreNoneSet())
				{
					m_activeMaterials.Clear(materialIdentifier);
				}
			}
		}

		if (m_pForwardingStage.IsValid())
		{
			m_pForwardingStage->OnRenderItemsBecomeHidden(renderItems, scene, graphicsCommandEncoder, perFrameStagingBuffer);
		}
	}

	Threading::JobBatch MaterialsStage::LoadRenderItemsResources(const Entity::RenderItemMask& renderItems)
	{
		Threading::JobBatch loadMaterialInstancesJobBatch = Threading::JobBatch::IntermediateStage;

		IdentifierMask<MaterialInstanceIdentifier> requestedMaterialInstances;

		Rendering::MaterialCache& materialCache = m_sceneView.GetLogicalDevice().GetRenderer().GetMaterialCache();
		Rendering::MaterialInstanceCache& materialInstanceCache = materialCache.GetInstanceCache();

		Entity::SceneRegistry& sceneRegistry = m_sceneView.GetSceneChecked()->GetEntitySceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();

		for (const Entity::RenderItemIdentifier::IndexType renderItemIndex : renderItems.GetSetBitsIterator())
		{
			const Entity::ComponentIdentifier componentIdentifier =
				m_sceneView.GetVisibleRenderItemComponentIdentifier(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(componentIdentifier.IsValid());
			if (UNLIKELY_ERROR(componentIdentifier.IsInvalid()))
			{
				continue;
			}

			const Optional<Entity::Data::RenderItem::MaterialInstanceIdentifier*> pMaterialInstanceComponent =
				materialInstanceIdentifierSceneData.GetComponentImplementation(componentIdentifier);
			Assert(pMaterialInstanceComponent.IsValid());
			if (UNLIKELY_ERROR(pMaterialInstanceComponent.IsInvalid()))
			{
				continue;
			}

			const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier = *pMaterialInstanceComponent;
			Rendering::RuntimeMaterialInstance& materialInstance = *materialInstanceCache.GetMaterialInstance(materialInstanceIdentifier);
			if (!materialInstance.HasFinishedLoading() && !requestedMaterialInstances.IsSet(materialInstanceIdentifier))
			{
				requestedMaterialInstances.Set(materialInstanceIdentifier);

				Threading::JobBatch materialInstanceJobBatch = materialInstanceCache.TryLoad(materialInstanceIdentifier);
				loadMaterialInstancesJobBatch.QueueAfterStartStage(materialInstanceJobBatch);
			}
		}

		Threading::IntermediateStage& finishedStage = Threading::CreateIntermediateStage();

		Threading::Job& loadRenderItemsResources = Threading::CreateCallback(
			[this, renderItems, &finishedStage, &materialInstanceCache, &materialInstanceIdentifierSceneData](Threading::JobRunnerThread&)
			{
				Entity::RenderItemMask renderItemsTemp = renderItems;
				Entity::RenderItemMask stageRenderItems;
				Entity::RenderItemMask invalidRenderItems;

				const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
					m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();

				Threading::JobBatch loadRenderItemResourcesJobBatch;

				do
				{
					Rendering::MaterialIdentifier currentMaterialIdentifier;

					do
					{
						const Memory::BitIndex<uint32> firstRenderItemIndex = renderItemsTemp.GetFirstSetIndex();
						if (firstRenderItemIndex.IsInvalid())
						{
							finishedStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
							return;
						}

						const Entity::ComponentIdentifier componentIdentifier =
							m_sceneView.GetVisibleRenderItemComponentIdentifier(Entity::RenderItemIdentifier::MakeFromValidIndex(*firstRenderItemIndex));
						Assert(componentIdentifier.IsValid());

						const Optional<Entity::Data::RenderItem::MaterialInstanceIdentifier*> pMaterialInstanceComponent =
							materialInstanceIdentifierSceneData.GetComponentImplementation(componentIdentifier);
						Assert(pMaterialInstanceComponent.IsValid());

						const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier = *pMaterialInstanceComponent;

						Rendering::RuntimeMaterialInstance& materialInstance = *materialInstanceCache.GetMaterialInstance(materialInstanceIdentifier);
						currentMaterialIdentifier = materialInstance.GetMaterialIdentifier();
						if (!currentMaterialIdentifier.IsValid())
						{
							renderItemsTemp.Clear(Entity::RenderItemIdentifier::MakeFromValidIndex(*firstRenderItemIndex));
						}
					} while (currentMaterialIdentifier.IsInvalid());

					for (const uint32 renderItemIndex : renderItemsTemp.GetSetBitsIterator(0, maximumUsedRenderItemCount))
					{
						const Entity::ComponentIdentifier componentIdentifier =
							m_sceneView.GetVisibleRenderItemComponentIdentifier(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
						Assert(componentIdentifier.IsValid());
						if (UNLIKELY(!componentIdentifier.IsValid()))
						{
							continue;
						}

						const Optional<Entity::Data::RenderItem::MaterialInstanceIdentifier*> pMaterialInstanceComponent =
							materialInstanceIdentifierSceneData.GetComponentImplementation(componentIdentifier);
						Assert(pMaterialInstanceComponent.IsValid());
						if (UNLIKELY(!pMaterialInstanceComponent.IsValid()))
						{
							continue;
						}

						const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier = *pMaterialInstanceComponent;

						Rendering::RuntimeMaterialInstance& materialInstance = *materialInstanceCache.GetMaterialInstance(materialInstanceIdentifier);
						const Rendering::MaterialIdentifier materialIdentifier = materialInstance.GetMaterialIdentifier();
						if (materialIdentifier == currentMaterialIdentifier)
						{
							stageRenderItems.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
						}
						else if (!materialIdentifier.IsValid())
						{
							invalidRenderItems.Set(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
						}
					}

					if (LIKELY(m_materialStages[currentMaterialIdentifier] != nullptr))
					{
						MaterialStage& stage = *m_materialStages[currentMaterialIdentifier];
						stage.m_visibleRenderItems |= stageRenderItems;
						m_activeMaterials.Set(currentMaterialIdentifier);
						Threading::JobBatch batch = stage.LoadRenderItemsResources(stageRenderItems);
						loadRenderItemResourcesJobBatch.QueueAfterStartStage(batch);
					}

					renderItemsTemp.Clear(stageRenderItems);
					renderItemsTemp.Clear(invalidRenderItems);
					stageRenderItems.ClearAll();
				} while (renderItemsTemp.AreAnySet());

				if (loadRenderItemResourcesJobBatch.IsValid())
				{
					loadRenderItemResourcesJobBatch.GetFinishedStage().AddSubsequentStage(finishedStage);
					Threading::JobRunnerThread::GetCurrent()->Queue(loadRenderItemResourcesJobBatch);
				}
				else
				{
					finishedStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
				}
			},
			Threading::JobPriority::LoadMaterialStageResources
		);

		loadMaterialInstancesJobBatch.GetFinishedStage().AddSubsequentStage(loadRenderItemsResources);

		Threading::JobBatch fullBatch;
		fullBatch.QueueAfterStartStage(loadMaterialInstancesJobBatch);
		finishedStage.AddSubsequentStage(fullBatch.GetFinishedStage());
		return fullBatch;
	}

	void MaterialsStage::OnVisibleRenderItemTransformsChanged(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		Rendering::MaterialCache& materialCache = System::Get<Rendering::Renderer>().GetMaterialCache();
		const typename MaterialIdentifier::IndexType maximumUsedMaterialCount = materialCache.GetMaximumUsedIdentifierCount();
		for (const typename MaterialIdentifier::IndexType materialIndex : m_activeMaterials.GetSetBitsIterator(0, maximumUsedMaterialCount))
		{
			const MaterialIdentifier materialIdentifier = MaterialIdentifier::MakeFromValidIndex(materialIndex);
			MaterialStage& stage = *m_materialStages[materialIdentifier];

			const Entity::RenderItemMask materialRenderItems = renderItems & stage.m_visibleRenderItems;
			if (materialRenderItems.AreAnySet())
			{
				stage.OnVisibleRenderItemTransformsChanged(
					materialRenderItems,
					graphicsCommandEncoder,
					perFrameStagingBuffer

				);
			}
		}

		if (m_pForwardingStage.IsValid())
		{
			m_pForwardingStage->OnVisibleRenderItemTransformsChanged(renderItems, graphicsCommandEncoder, perFrameStagingBuffer);
		}
	}

	void MaterialsStage::
		OnActiveCameraPropertiesChanged([[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer&)
	{
	}
}
