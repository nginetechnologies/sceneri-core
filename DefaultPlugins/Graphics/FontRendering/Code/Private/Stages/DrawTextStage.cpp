#include "Stages/DrawTextStage.h"
#include "Components/TextComponent.h"
#include "Manager.h"
#include "FontAtlas.h"

#include <Common/Math/Color.h>
#include <Common/Math/Vector2/Min.h>
#include <Common/Math/Vector2/Max.h>
#include <Common/Math/Clamp.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/RecurringAsyncJob.h>

#include <Renderer/Renderer.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneData.h>
#include <Renderer/Assets/Material/MaterialAsset.h>
#include <Renderer/Assets/Material/RenderMaterial.h>
#include <Renderer/Assets/Material/RenderMaterialInstance.h>
#include <Renderer/Assets/Material/RuntimeMaterialInstance.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Assets/Texture/MipMask.h>
#include <Renderer/Jobs/QueueSubmissionJob.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Commands/UnifiedCommandBuffer.h>
#include <Renderer/Commands/BarrierCommandEncoder.h>
#include <Renderer/Commands/ClearValue.h>

#include <Engine/Asset/AssetManager.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Scene/Scene.h>

#include <Renderer/Wrappers/GpuCheckpoint.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/SubpassDependency.h>

#include <Common/System/Query.h>

namespace ngine::Font
{
	namespace DrawTextHelpers
	{
		inline static constexpr Array<Rendering::AttachmentReference, 1> ColorAttachments = {
			Rendering::AttachmentReference{0u, Rendering::ImageLayout::ColorAttachmentOptimal}
		};

		inline static Array<Rendering::AttachmentDescription, 1> CreateAttachments()
		{
			return {Rendering::AttachmentDescription{
				Rendering::Format::R8G8B8A8_UNORM_PACK8,
				Rendering::SampleCount::One,
				Rendering::AttachmentLoadType::Clear,
				Rendering::AttachmentStoreType::Store,
				Rendering::AttachmentLoadType::Undefined,
				Rendering::AttachmentStoreType::Undefined,
				Rendering::ImageLayout::Undefined,
				Rendering::ImageLayout::ColorAttachmentOptimal
			}};
		}

		inline static constexpr Array<Rendering::SubpassDependency, 2> ApplySubpassDependencies = {
			Rendering::SubpassDependency{
				Rendering::ExternalSubpass,
				0,
				Rendering::PipelineStageFlags::TopOfPipe,
				Rendering::PipelineStageFlags::ColorAttachmentOutput,
				Rendering::AccessFlags::MemoryRead,
				Rendering::AccessFlags::ColorAttachmentReadWrite,
				Rendering::DependencyFlags()
			},
			Rendering::SubpassDependency{
				0,
				Rendering::ExternalSubpass,
				Rendering::PipelineStageFlags::ColorAttachmentOutput,
				Rendering::PipelineStageFlags::BottomOfPipe,
				Rendering::AccessFlags::ColorAttachmentReadWrite,
				Rendering::AccessFlags::MemoryRead,
				Rendering::DependencyFlags()
			}
		};

		inline Rendering::Framebuffer CreateFramebuffer(
			const Rendering::LogicalDeviceView logicalDevice,
			const Rendering::RenderPassView renderPass,
			const Rendering::ImageMappingView renderTargetAttachment,
			const Math::Vector2ui outputSize
		)
		{
			const Array<const Rendering::ImageMappingView, 1> imageAttachments = {renderTargetAttachment};
			return Rendering::Framebuffer(logicalDevice, renderPass, imageAttachments, outputSize);
		}
	}

	inline static constexpr Asset::Guid FontRenderTargetAssetGuid = "279316D6-A4B3-42E4-995A-5D1AC5A95BFA"_asset;
	inline static constexpr Math::Vector2ui RenderTargetResolution = {512, 512};

	DrawTextStage::DrawTextStage(Rendering::SceneView& sceneView)
		: RenderItemStage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw)
		, m_sceneView(sceneView)
		, m_renderPass(
				sceneView.GetLogicalDevice(),
				DrawTextHelpers::CreateAttachments(),
				DrawTextHelpers::ColorAttachments,
				{},
				{},
				DrawTextHelpers::ApplySubpassDependencies
			)
		, m_pipeline(sceneView.GetLogicalDevice())
	{
		const Rendering::SceneRenderStageIdentifier stageIdentifier =
			sceneView.GetLogicalDevice().GetRenderer().GetStageCache().FindOrRegisterAsset(
				TypeGuid,
				UnicodeString(MAKE_UNICODE_LITERAL("Draw Text")),
				Rendering::StageFlags::Hidden
			);
		sceneView.RegisterRenderItemStage(stageIdentifier, *this);

		const uint8 subpassIndex = 0;
		Threading::JobBatch jobBatch = m_pipeline.CreatePipeline(
			sceneView.GetLogicalDevice(),
			sceneView.GetLogicalDevice().GetShaderCache(),
			m_renderPass,
			{Math::Zero, RenderTargetResolution},
			{Math::Zero, RenderTargetResolution},
			subpassIndex
		);
		Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
	}

	DrawTextStage::~DrawTextStage()
	{
		Rendering::LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		m_renderPass.Destroy(logicalDevice);

		Rendering::Renderer& renderer = logicalDevice.GetRenderer();
		Rendering::TextureCache& textureCache = renderer.GetTextureCache();

		Manager& fontManager = *System::FindPlugin<Manager>();
		Cache& fontCache = fontManager.GetCache();

		for (const Optional<AtlasInfo*> pAtlasInfo : fontCache.GetInstanceIdentifiers().GetValidElementView(m_atlases.GetView()))
		{
			if (pAtlasInfo.IsValid())
			{
				pAtlasInfo->m_fontImageMapping.Destroy(logicalDevice);
				pAtlasInfo->m_fontSampler.Destroy(logicalDevice);

				if (pAtlasInfo->m_pDescriptorLoadingThread != nullptr)
				{
					Threading::EngineJobRunnerThread& previousEngineThread =
						static_cast<Threading::EngineJobRunnerThread&>(*pAtlasInfo->m_pDescriptorLoadingThread);
					previousEngineThread.GetRenderData().DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(pAtlasInfo->m_fontDescriptorSet));
				}

				for (const Optional<RenderTargetInfo*> pRenderTargetInfo : pAtlasInfo->m_renderTargets)
				{
					pRenderTargetInfo->m_renderTargetMapping.Destroy(logicalDevice);
					pRenderTargetInfo->m_framebuffer.Destroy(logicalDevice);

					{
						[[maybe_unused]] const bool wasListenerRemoved =
							textureCache.RemoveRenderTextureListener(logicalDevice.GetIdentifier(), pRenderTargetInfo->m_renderTargetIdentifier, this);
						Assert(wasListenerRemoved);
					}
				}
			}
		}

		m_pipeline.Destroy(logicalDevice);

		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const Rendering::SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(TypeGuid);
		m_sceneView.DeregisterRenderItemStage(stageIdentifier);
	}

	inline static constexpr float RenderedTextScale = 1.f;

	Optional<Math::Rectangleui> DrawTextStage::RenderTargetInfo::TryEmplaceText(
		const Entity::RenderItemIdentifier renderItemIdentifier,
		const ConstUnicodeStringView text,
		const Math::Color color,
		const uint32 textWidth,
		const uint32 atlasRowHeight
	)
	{
		m_availableSpace.Reserve(m_availableSpace.GetSize() + 2);

		Math::Rectangleui* __restrict bestAvailableSpaceIt = nullptr;
		for (Math::Rectangleui& availableSpace : m_availableSpace)
		{
			if ((availableSpace.GetSize() >= Math::Vector2ui{textWidth, atlasRowHeight}).AreAllSet())
			{
				if (bestAvailableSpaceIt == nullptr || (availableSpace.GetSize().GetLengthSquared() < bestAvailableSpaceIt->GetSize().GetLengthSquared()))
				{
					bestAvailableSpaceIt = &availableSpace;
				}
			}
		}

		if (bestAvailableSpaceIt != nullptr)
		{
			const Math::Vector2ui textPosition = bestAvailableSpaceIt->m_position;
			if (bestAvailableSpaceIt->GetSize().y > atlasRowHeight)
			{
				// Split into two
				Math::Rectangleui newLowerArea = *bestAvailableSpaceIt + Math::Vector2ui{0, atlasRowHeight};

				*bestAvailableSpaceIt -= Math::Vector2ui{0, newLowerArea.m_size.y};
				const ArrayView<Math::Rectangleui> availableSpaceRange = m_availableSpace.GetView();
				Math::Rectangleui* const upperBoundIt = std::lower_bound(
					availableSpaceRange.begin().Get(),
					availableSpaceRange.end().Get(),
					newLowerArea.m_position,
					[](const Math::Rectangleui existingArea, const Math::Vector2ui newPosition) -> bool
					{
						const uint32 existingIndex = existingArea.m_position.x + existingArea.m_position.y * RenderTargetResolution.x;
						const uint32 newIndex = newPosition.x + newPosition.y * RenderTargetResolution.x;
						return existingIndex < newIndex;
					}
				);
				m_availableSpace.Emplace(upperBoundIt, Memory::Uninitialized, newLowerArea);
			}
			*bestAvailableSpaceIt += Math::Vector2ui{textWidth, 0};

			m_registeredTexts.Emplace(
				renderItemIdentifier.GetFirstValidIndex(),
				TextInfo{UnicodeString(text), color, Math::Rectangleui{textPosition, Math::Vector2ui{textWidth, atlasRowHeight}}}
			);
			return Math::Rectangleui{textPosition, Math::Vector2ui{textWidth, atlasRowHeight}};
		}
		return Invalid;
	}

	void DrawTextStage::RenderTargetInfo::ReturnTextArea(const Math::Rectangleui textArea)
	{
		// Find the range into which we can place this text
		// We sort the available spaces by column (x) and then row (y)
		const ArrayView<Math::Rectangleui> availableSpaceRange = m_availableSpace.GetView();
		Math::Rectangleui* lowerBoundIt = std::lower_bound(
			availableSpaceRange.begin().Get(),
			availableSpaceRange.end().Get(),
			textArea.m_position,
			[](const Math::Rectangleui existingArea, const Math::Vector2ui newPosition) -> bool
			{
				const uint32 existingIndex = existingArea.m_position.x + existingArea.m_position.y * RenderTargetResolution.x;
				const uint32 newIndex = newPosition.x + newPosition.y * RenderTargetResolution.x;
				return existingIndex < newIndex;
			}
		);

		const uint32 index = m_availableSpace.GetIteratorIndex(lowerBoundIt) + 1;
		m_availableSpace.Emplace(lowerBoundIt, Memory::Uninitialized, textArea);
		lowerBoundIt = m_availableSpace.begin() + index;

		for (auto begin = m_availableSpace.begin().Get(), it = Math::Max(lowerBoundIt - 1, begin), last = m_availableSpace.end().Get() - 1;
		     it != last;)
		{
			// Try to merge with what's to the right of us
			const Math::Rectangleui& rightNeighbor = *(it + 1);
			Math::Rectangleui& __restrict entryArea = *it;
			if ((entryArea.m_size.x == RenderTargetResolution.x) & (rightNeighbor.m_size.x == RenderTargetResolution.x))
			{
				Assert(entryArea.m_position.x == 0);
				Assert(rightNeighbor.m_position.x == 0);

				// This row is full, see if we can merge vertically
				if (entryArea.GetEndPosition().y == rightNeighbor.m_position.y)
				{
					// Merge vertically
					entryArea.m_size.y += rightNeighbor.m_size.y;
					m_availableSpace.Remove(&rightNeighbor);
					last--;

					continue;
				}
			}
			else if (rightNeighbor.m_position.x == entryArea.GetEndPosition().x)
			{
				Assert(rightNeighbor.m_position.y == entryArea.m_position.y);
				Assert(rightNeighbor.m_size.y == entryArea.m_size.y);

				entryArea.m_size.x += rightNeighbor.m_size.x;
				m_availableSpace.Remove(&rightNeighbor);
				last--;

				continue;
			}

			++it;
		}
	}

	void DrawTextStage::
		OnRenderItemsBecomeVisible(const Entity::RenderItemMask& renderItems, const Rendering::CommandEncoderView, Rendering::PerFrameStagingBuffer&)
	{
		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		Rendering::StageCache& stageCache = renderer.GetStageCache();
		const Rendering::SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(TypeGuid);

		Manager& fontManager = *System::FindPlugin<Manager>();
		Cache& fontCache = fontManager.GetCache();

		Threading::JobBatch fullBatch;

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

			Assert(
				pVisibleComponent->Is<TextComponent>(),
				"This must be validated in the Editor to ensure this stage can only be added to TextComponents"
			);
			const TextComponent& textComponent = static_cast<const TextComponent&>(*pVisibleComponent);

			const InstanceProperties instanceProperties{
				textComponent.GetFontIdentifier(),
				textComponent.GetFontSize(),
				RenderedTextScale,
				Modifier::None,
				DefaultFontWeight,
				DefaultCharacters
			};

			const InstanceIdentifier instanceIdentifier = fontCache.FindOrRegisterInstance(instanceProperties);
			UniquePtr<AtlasInfo>& pAtlas = m_atlases[instanceIdentifier];
			if (!pAtlas.IsValid())
			{
				pAtlas.CreateInPlace();
				Threading::JobBatch batch = LoadAtlasResources(instanceIdentifier, instanceProperties, *pAtlas);

				// Create the first render target while we're at it so we don't delay loading (doesn't depend on the atlas)
				RenderTargetInfo& renderTargetInfo = *pAtlas->m_renderTargets.EmplaceBack(UniquePtr<RenderTargetInfo>::Make(RenderTargetResolution)
				);
				if (Threading::Job* pJob = LoadRenderTarget(renderTargetInfo))
				{
					batch.QueueAfterStartStage(*pJob);
				}

				Threading::JobRunnerThread::GetCurrent()->Queue(batch);
				// Skip until the atlas has loaded
				continue;
			}
			else if (!pAtlas->CanRegisterRenderItems())
			{
				// Skip until the atlas has loaded
				continue;
			}

			const ConstUnicodeStringView text = textComponent.GetText();
			const Math::Color textColor = textComponent.GetTextColor();
			const Math::Vector2ui textSize = pAtlas->m_pFontAtlas->CalculateSize(text);
			const uint32 atlasRowHeight = pAtlas->m_pFontAtlas->GetMaximumGlyphWidth().y;

			const Entity::RenderItemIdentifier renderItemIdentifier = Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex);
			Optional<RenderTargetInfo*> pSelectedRenderTarget;
			for (UniquePtr<RenderTargetInfo>& pRenderTarget : pAtlas->m_renderTargets)
			{
				if (Optional<Math::Rectangleui> textArea = pRenderTarget->TryEmplaceText(renderItemIdentifier, text, textColor, textSize.x, atlasRowHeight))
				{
					pSelectedRenderTarget = pRenderTarget;
					m_renderItemInfo[renderItemIdentifier].m_pRenderTarget = pSelectedRenderTarget;
					m_renderItemInfo[renderItemIdentifier].m_textArea = *textArea;
					m_renderItemInfo[renderItemIdentifier].m_realHeight = textSize.y;
					goto was_emplaced;
				}
				else if (!pRenderTarget->IsValid())
				{
					goto failed;
				}
			}

			// Item could not fit into any of the existing render targets
			// Create a new one.
			if (textSize.x <= RenderTargetResolution.x)
			{
				RenderTargetInfo& renderTargetInfo = *pAtlas->m_renderTargets.EmplaceBack(UniquePtr<RenderTargetInfo>::Make(RenderTargetResolution)
				);
				if (Threading::Job* pJob = LoadRenderTarget(renderTargetInfo))
				{
					pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
					// Skip until loaded
					continue;
				}
			}
			else
			{
				Assert(false, "Text was too big to fit into a render target!");
				continue;
			}

failed:
			continue;

was_emplaced:

			m_sceneView.GetSubmittedRenderItemStageMask(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex)).Set(stageIdentifier);

			// Customize the material instance attached to the text component to render our render target
			Threading::JobBatch batch;
			Threading::JobBatch materialInstanceLoadBatch = renderer.GetMaterialCache().GetInstanceCache().TryLoad(
				textComponent.GetMaterialInstanceIdentifier(),
				Rendering::MaterialInstanceCache::OnLoadedListenerData{
					*this,
					[this, &renderTargetInfo = *pSelectedRenderTarget, &textComponent, textSize](DrawTextStage&)
					{
						Rendering::MaterialCache& materialCache = System::Get<Rendering::Renderer>().GetMaterialCache();
						Rendering::MaterialInstanceCache& materialInstanceCache = materialCache.GetInstanceCache();
						Rendering::RuntimeMaterialInstance& materialInstance =
							*materialInstanceCache.GetMaterialInstance(textComponent.GetMaterialInstanceIdentifier());

						Assert(materialInstance.HasFinishedLoading() && materialInstance.IsValid());
						if (UNLIKELY(!materialInstance.IsValid()))
						{
							return EventCallbackResult::Remove;
						}

						if (Ensure(materialInstance.GetDescriptorContents().HasElements()))
						{
							materialInstance.UpdateDescriptorContent(
								0,
								Rendering::RuntimeDescriptorContent{renderTargetInfo.m_renderTargetIdentifier, Rendering::AddressMode::Repeat}
							);
						}

						// One pixel offset to allow alpha blending some space
						const Math::Vector2ui offset{1};
						const Math::Rectangleui textAtlasArea = {
							Math::Max(
								m_renderItemInfo[textComponent.GetRenderItemIdentifier()].m_textArea.m_position,
								Math::Vector2ui{
									offset,
								}
							) -
								offset,
							textSize + offset
						};
						const Math::Rectanglef ratio = (Math::Rectanglef)textAtlasArea / Math::Vector2f{RenderTargetResolution};

						struct PushConstantsData
						{
							Math::Vector4f m_scale;
							Math::Vector4f m_offsetAndRatio;
						};

						const float aspectRatio = (float)textAtlasArea.m_size.x /
				                              (float)m_renderItemInfo[textComponent.GetRenderItemIdentifier()].m_realHeight;
						const float textScale = (float)textComponent.GetFontSize().GetPixels() * RenderedTextScale;

						PushConstantsData pushConstantsData{
							{aspectRatio * textScale, 1 * textScale, 0, 0},
							{ratio.m_position.x, ratio.m_position.y, ratio.m_size.x, ratio.m_size.y}
						};

						materialInstance.GetPushConstantsData().CopyFrom(ConstByteView::Make(pushConstantsData));
						return EventCallbackResult::Remove;
					}
				}
			);
			if (materialInstanceLoadBatch.IsValid())
			{
				batch.QueueAfterStartStage(materialInstanceLoadBatch);
			}

			fullBatch.QueueAfterStartStage(batch);
		}

		if (fullBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(fullBatch);
		}
	}

	void DrawTextStage::OnVisibleRenderItemsReset(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView commandEncoder,
		Rendering::PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		// Temp
		OnRenderItemsBecomeHidden(renderItems, *m_sceneView.GetSceneChecked(), commandEncoder, perFrameStagingBuffer);
		OnRenderItemsBecomeVisible(renderItems, commandEncoder, perFrameStagingBuffer);
	}

	void DrawTextStage::
		OnRenderItemsBecomeHidden(const Entity::RenderItemMask& renderItems, SceneBase&, const Rendering::CommandEncoderView, Rendering::PerFrameStagingBuffer&)
	{
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			RenderItemInfo& renderItemInfo = m_renderItemInfo[Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex)];

			Optional<RenderTargetInfo*> pRenderTarget = renderItemInfo.m_pRenderTarget;
			renderItemInfo.m_pRenderTarget = Invalid;

			if (pRenderTarget.IsValid())
			{
				pRenderTarget->ReturnTextArea(renderItemInfo.m_textArea);
				auto it = pRenderTarget->m_registeredTexts.Find(renderItemIndex);
				Assert(it != pRenderTarget->m_registeredTexts.end());
				pRenderTarget->m_registeredTexts.Remove(it);
			}
		}
	}

	void DrawTextStage::
		OnVisibleRenderItemTransformsChanged([[maybe_unused]] const Entity::RenderItemMask& renderItems, const Rendering::CommandEncoderView, Rendering::PerFrameStagingBuffer&)
	{
		// We don't rely on the transform at all so no need to react.
	}

	[[nodiscard]] Threading::JobBatch DrawTextStage::LoadAtlasResources(
		const InstanceIdentifier atlasInstanceIdentifier, InstanceProperties instanceProperties, AtlasInfo& __restrict atlasInfo
	)
	{
		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		Manager& fontManager = *System::FindPlugin<Manager>();
		Cache& fontCache = fontManager.GetCache();

		Threading::JobBatch batch = fontCache.TryLoadInstance(
			atlasInstanceIdentifier,
			instanceProperties,
			System::Get<Asset::Manager>(),
			[&fontCache, &atlasInfo](const InstanceIdentifier fontInstanceIdentifier)
			{
				const Atlas& fontAtlas = *fontCache.GetInstanceAtlas(fontInstanceIdentifier);
				atlasInfo.m_pFontAtlas = &fontAtlas;
			}
		);

		const Rendering::TextureIdentifier atlasTextureIdentifier = fontCache.GetInstanceAtlas(atlasInstanceIdentifier)->GetTextureIdentifier();
		Rendering::TextureCache& textureCache = renderer.GetTextureCache();
		Optional<Threading::Job*> pLoadAtlasRenderTextureJob = textureCache.GetOrLoadRenderTexture(
			m_logicalDevice.GetIdentifier(),
			atlasTextureIdentifier,
			Rendering::ImageMappingType::TwoDimensional,
			Rendering::AllMips,
			Rendering::TextureLoadFlags::Default & ~Rendering::TextureLoadFlags::LoadDummy,
			Rendering::TextureCache::TextureLoadListenerData{
				*this,
				[this,
		     &atlasInfo](DrawTextStage&, Rendering::LogicalDevice& logicalDevice, const Rendering::TextureIdentifier, const Rendering::RenderTexture& texture, Rendering::MipMask, const EnumFlags<Rendering::LoadedTextureFlags>)
					-> EventCallbackResult
				{
					// Create the descriptor and sampler
					const Rendering::DescriptorSetLayoutView descriptorLayout = m_pipeline;
					Threading::EngineJobRunnerThread& jobRunnerThread =
						static_cast<Threading::EngineJobRunnerThread&>(*Threading::JobRunnerThread::GetCurrent());

					if (atlasInfo.m_fontDescriptorSet.IsValid())
					{
						Threading::EngineJobRunnerThread& previousEngineThread =
							static_cast<Threading::EngineJobRunnerThread&>(*atlasInfo.m_pDescriptorLoadingThread);
						previousEngineThread.GetRenderData().DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(atlasInfo.m_fontDescriptorSet));
					}

					const Rendering::DescriptorPoolView descriptorPool =
						jobRunnerThread.GetRenderData().GetDescriptorPool(logicalDevice.GetIdentifier());
					[[maybe_unused]] const bool allocatedDescriptorSets = descriptorPool.AllocateDescriptorSets(
						logicalDevice,
						ArrayView<const Rendering::DescriptorSetLayoutView, uint8>(descriptorLayout),
						ArrayView<Rendering::DescriptorSet, uint8>(atlasInfo.m_fontDescriptorSet)
					);
					Assert(allocatedDescriptorSets);
					atlasInfo.m_pDescriptorLoadingThread = &jobRunnerThread;

					if (atlasInfo.m_fontImageMapping.IsValid())
					{
						atlasInfo.m_fontImageMapping.Destroy(logicalDevice);
					}

					atlasInfo.m_fontImageMapping = Rendering::ImageMapping(
						logicalDevice,
						texture,
						Rendering::ImageMapping::Type::TwoDimensional,
						texture.GetFormat(),
						Rendering::ImageAspectFlags::Color,
						texture.GetAvailableMipRange(),
						Rendering::ArrayRange{0, 1}
					);

					if (atlasInfo.m_fontSampler.IsValid())
					{
						atlasInfo.m_fontSampler.Destroy(logicalDevice);
					}

					Rendering::Sampler fontSampler(logicalDevice);

					Rendering::DescriptorSet::Update(
						logicalDevice,
						Array<const Rendering::DescriptorSet::UpdateInfo, 2>{
							Rendering::DescriptorSet::UpdateInfo{
								atlasInfo.m_fontDescriptorSet,
								0,
								0,
								Rendering::DescriptorType::SampledImage,
								Array<const Rendering::DescriptorSet::ImageInfo, 1>{
									Rendering::DescriptorSet::ImageInfo{{}, atlasInfo.m_fontImageMapping, Rendering::ImageLayout::ShaderReadOnlyOptimal}
								}.GetDynamicView()
							},
							Rendering::DescriptorSet::UpdateInfo{
								atlasInfo.m_fontDescriptorSet,
								1,
								0,
								Rendering::DescriptorType::Sampler,
								Array<const Rendering::DescriptorSet::ImageInfo, 1>{
									Rendering::DescriptorSet::ImageInfo{fontSampler, {}, Rendering::ImageLayout::ShaderReadOnlyOptimal}
								}.GetDynamicView()
							}
						}.GetDynamicView()
					);

					atlasInfo.m_fontSampler = Move(fontSampler);
					return EventCallbackResult::Keep;
				}
			}
		);
		if (pLoadAtlasRenderTextureJob.IsValid())
		{
			batch.QueueAsNewFinishedStage(*pLoadAtlasRenderTextureJob);
		}

		return batch;
	}

	Optional<Threading::Job*> DrawTextStage::LoadRenderTarget(RenderTargetInfo& renderTargetInfo)
	{
		Rendering::TextureCache& textureCache = m_sceneView.GetLogicalDevice().GetRenderer().GetTextureCache();
		const Rendering::RenderTargetTemplateIdentifier identifierRenderTargetTemplateIdentifier =
			textureCache.FindOrRegisterRenderTargetTemplate(FontRenderTargetAssetGuid);
		renderTargetInfo.m_renderTargetIdentifier = textureCache.RegisterProceduralRenderTargetAsset();
		const Rendering::MipMask mipMask = Rendering::MipMask::FromSizeToLargest(RenderTargetResolution);
		return textureCache.GetOrLoadRenderTarget(
			m_sceneView.GetLogicalDevice(),
			renderTargetInfo.m_renderTargetIdentifier,
			identifierRenderTargetTemplateIdentifier,
			Rendering::SampleCount::One,
			RenderTargetResolution,
			mipMask,
			Rendering::ArrayRange{0, 1},
			Rendering::TextureCache::TextureLoadListenerData{
				*this,
				[&renderTargetInfo, mipMask](
					DrawTextStage& drawTextStage,
					const Rendering::LogicalDevice& logicalDevice,
					const Rendering::TextureIdentifier,
					Rendering::RenderTexture& texture,
					const Rendering::MipMask,
					[[maybe_unused]] const EnumFlags<Rendering::LoadedTextureFlags> flags
				) -> EventCallbackResult
				{
					Assert(!flags.IsSet(Rendering::LoadedTextureFlags::IsDummy));

					if (Ensure(texture.IsValid()))
					{
						Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
						Rendering::UnifiedCommandBuffer commandBuffer(
							logicalDevice,
							thread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics),
							logicalDevice.GetCommandQueue(Rendering::QueueFamily::Graphics)
						);
						Rendering::CommandEncoderView commandEncoder = commandBuffer.BeginEncoding(logicalDevice);

						{
							Rendering::BarrierCommandEncoder barrierCommandEncoder = commandEncoder.BeginBarrier();

							barrierCommandEncoder.TransitionImageLayout(
								Rendering::PipelineStageFlags::Transfer | Rendering::PipelineStageFlags::FragmentShader,
								Rendering::AccessFlags::TransferWrite | Rendering::AccessFlags::ShaderRead,
								Rendering::ImageLayout::ShaderReadOnlyOptimal,
								texture,
								Rendering::ImageSubresourceRange{
									Rendering::ImageAspectFlags::Color,
									Rendering::MipRange{0, mipMask.GetSize()},
									Rendering::ArrayRange{0, 1}
								}
							);
						}

						const Rendering::EncodedCommandBufferView encodedCommandBuffer = commandBuffer.StopEncoding();

						Rendering::QueueSubmissionParameters parameters;
						parameters.m_finishedCallback = [&logicalDevice, commandBuffer = Move(commandBuffer), &thread]() mutable
						{
							thread.QueueExclusiveCallbackFromAnyThread(
								Threading::JobPriority::DeallocateResourcesMin,
								[commandBuffer = Move(commandBuffer), &logicalDevice](Threading::JobRunnerThread& thread) mutable
								{
									Threading::EngineJobRunnerThread& engineThread = static_cast<Threading::EngineJobRunnerThread&>(thread);
									commandBuffer.Destroy(
										logicalDevice,
										engineThread.GetRenderData().GetCommandPool(logicalDevice.GetIdentifier(), Rendering::QueueFamily::Graphics)
									);
								}
							);
						};

						logicalDevice.GetQueueSubmissionJob(Rendering::QueueFamily::Graphics)
							.Queue(
								Threading::JobPriority::CreateRenderTargetSubmission,
								ArrayView<const Rendering::EncodedCommandBufferView, uint16>(encodedCommandBuffer),
								Move(parameters)
							);
					}

					renderTargetInfo.m_renderTargetMapping = Rendering::ImageMapping(
						logicalDevice,
						texture,
						Rendering::ImageMappingType::TwoDimensional,
						texture.GetFormat(),
						Rendering::ImageAspectFlags::Color,
						Rendering::MipRange{0, 1},
						Rendering::ArrayRange{0, 1}
					);
					renderTargetInfo.m_pRenderTargetTexture = texture;
					const uint8 loadedRenderTargetCount = renderTargetInfo.m_loadedRenderTargetCount++;
					if (loadedRenderTargetCount == 0)
					{
						renderTargetInfo.m_framebuffer = Rendering::Framebuffer(DrawTextHelpers::CreateFramebuffer(
							drawTextStage.m_sceneView.GetLogicalDevice(),
							drawTextStage.m_renderPass,
							renderTargetInfo.m_renderTargetMapping,
							RenderTargetResolution
						));
					}
					return EventCallbackResult::Keep;
				}
			}
		);
	}

	Threading::JobBatch DrawTextStage::LoadRenderItemsResources(const Entity::RenderItemMask& renderItems)
	{
		Threading::JobBatch fullBatch;

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

			Assert(
				pVisibleComponent->Is<TextComponent>(),
				"This must be validated in the Editor to ensure this stage can only be added to TextComponents"
			);
			const TextComponent& textComponent = static_cast<const TextComponent&>(*pVisibleComponent);

			Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
			Manager& fontManager = *System::FindPlugin<Manager>();
			Cache& fontCache = fontManager.GetCache();

			const InstanceProperties instanceProperties{
				textComponent.GetFontIdentifier(),
				textComponent.GetFontSize(),
				RenderedTextScale,
				Modifier::None,
				DefaultFontWeight,
				DefaultCharacters
			};

			const InstanceIdentifier instanceIdentifier = fontCache.FindOrRegisterInstance(instanceProperties);

			Threading::JobBatch batch;
			AtlasInfo& atlasInfo = *m_atlases[instanceIdentifier];
			if (!atlasInfo.CanRegisterRenderItems())
			{
				batch = LoadAtlasResources(instanceIdentifier, instanceProperties, atlasInfo);
			}

			Threading::JobBatch materialInstanceLoadBatch =
				renderer.GetMaterialCache().GetInstanceCache().TryLoad(textComponent.GetMaterialInstanceIdentifier());
			if (materialInstanceLoadBatch.IsValid())
			{
				batch.QueueAfterStartStage(materialInstanceLoadBatch);
			}

			batch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[&renderTargetInfo = *atlasInfo.m_renderTargets[0], &textComponent](Threading::JobRunnerThread&)
				{
					Rendering::MaterialCache& materialCache = System::Get<Rendering::Renderer>().GetMaterialCache();
					Rendering::MaterialInstanceCache& materialInstanceCache = materialCache.GetInstanceCache();
					Rendering::RuntimeMaterialInstance& materialInstance =
						*materialInstanceCache.GetMaterialInstance(textComponent.GetMaterialInstanceIdentifier());

					if (Ensure(materialInstance.GetDescriptorContents().HasElements()))
					{
						materialInstance.UpdateDescriptorContent(
							0,
							Rendering::RuntimeDescriptorContent{renderTargetInfo.m_renderTargetIdentifier, Rendering::AddressMode::Repeat}
						);
					}
				},
				Threading::JobPriority::LoadFont
			));

			fullBatch.QueueAfterStartStage(batch);
		}

		return fullBatch;
	}

	void DrawTextStage::OnSceneUnloaded()
	{
	}

	void DrawTextStage::
		OnActiveCameraPropertiesChanged([[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, Rendering::PerFrameStagingBuffer&)
	{
	}

	void DrawTextStage::OnBeforeRecordCommands(const Rendering::CommandEncoderView graphicsCommandEncoder)
	{
		Manager& fontManager = *System::FindPlugin<Manager>();
		Cache& fontCache = fontManager.GetCache();
		Rendering::TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
		const Rendering::LogicalDeviceIdentifier logicalDeviceIdentifier = m_sceneView.GetLogicalDevice().GetIdentifier();

		Rendering::BarrierCommandEncoder barrierCommandEncoder = graphicsCommandEncoder.BeginBarrier();

		for (const Optional<AtlasInfo*> pAtlasInfo : fontCache.GetInstanceIdentifiers().GetValidElementView(m_atlases.GetView()))
		{
			if (!pAtlasInfo.IsValid() || !pAtlasInfo->CanRender(textureCache, logicalDeviceIdentifier))
			{
				continue;
			}

			for (const Optional<RenderTargetInfo*> pRenderTargetInfo : pAtlasInfo->m_renderTargets)
			{
				if (!pRenderTargetInfo->IsValid())
				{
					continue;
				}

				barrierCommandEncoder.TransitionImageLayout(
					Rendering::PipelineStageFlags::ColorAttachmentOutput,
					Rendering::AccessFlags::ColorAttachmentWrite,
					Rendering::ImageLayout::ColorAttachmentOptimal,
					*pRenderTargetInfo->m_pRenderTargetTexture,
					Rendering::ImageSubresourceRange{Rendering::ImageAspectFlags::Color}
				);
			}
		}
	}

	void DrawTextStage::RecordCommands(const Rendering::CommandEncoderView graphicsCommandEncoder)
	{
#if RENDERER_OBJECT_DEBUG_NAMES
		const Rendering::DebugMarker debugMarker{graphicsCommandEncoder, m_sceneView.GetLogicalDevice(), "Draw Text", "#FF0000"_color};
#endif

		constexpr Array<Rendering::ClearValue, 1> clearValues = {Math::Color{0.f, 0.f, 0.f, 0.f}};

		const Math::Rectangleui renderArea = {Math::Zero, RenderTargetResolution};

		Manager& fontManager = *System::FindPlugin<Manager>();
		Cache& fontCache = fontManager.GetCache();
		Rendering::TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
		const Rendering::LogicalDeviceIdentifier logicalDeviceIdentifier = m_sceneView.GetLogicalDevice().GetIdentifier();

		for (const Optional<AtlasInfo*> pAtlasInfo : fontCache.GetInstanceIdentifiers().GetValidElementView(m_atlases.GetView()))
		{
			if (!pAtlasInfo.IsValid() || !pAtlasInfo->CanRender(textureCache, logicalDeviceIdentifier))
			{
				continue;
			}

			for (const Optional<RenderTargetInfo*> pRenderTargetInfo : pAtlasInfo->m_renderTargets)
			{
				if (!pRenderTargetInfo->IsValid())
				{
					continue;
				}

				{
					Rendering::RenderCommandEncoder renderCommandEncoder = graphicsCommandEncoder.BeginRenderPass(
						m_logicalDevice,
						m_renderPass,
						pRenderTargetInfo->m_framebuffer,
						renderArea,
						clearValues,
						pRenderTargetInfo->m_registeredTexts.GetSize()
					);
					renderCommandEncoder.BindPipeline(m_pipeline);

					for (const auto& it : pRenderTargetInfo->m_registeredTexts)
					{
						const RenderTargetInfo::TextInfo& __restrict textInfo = it.second;
						const Math::Vector2i relativeStartPosition = (Math::Vector2i)textInfo.m_allocatedArea.GetPosition() -
						                                             (Math::Vector2i)renderArea.GetPosition();

						const Math::Vector2f startPositionRatio = Math::Vector2f{-1.f, -1.f} +
						                                          ((Math::Vector2f)relativeStartPosition / (Math::Vector2f)renderArea.GetSize()) * 2.f;

						m_pipeline.Draw(
							m_logicalDevice,
							renderCommandEncoder,
							*pAtlasInfo->m_pFontAtlas,
							pAtlasInfo->m_fontDescriptorSet,
							textInfo.m_text,
							startPositionRatio,
							Math::Vector2f{1.f, 1.f} / (Math::Vector2f)RenderTargetResolution,
							textInfo.m_color,
							1
						);
					}
				}
			}
		}
	}

	void DrawTextStage::OnAfterRecordCommands(const Rendering::CommandEncoderView graphicsCommandEncoder)
	{
		Manager& fontManager = *System::FindPlugin<Manager>();
		Cache& fontCache = fontManager.GetCache();
		Rendering::TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
		const Rendering::LogicalDeviceIdentifier logicalDeviceIdentifier = m_sceneView.GetLogicalDevice().GetIdentifier();

		Rendering::BarrierCommandEncoder barrierCommandEncoder = graphicsCommandEncoder.BeginBarrier();

		for (const Optional<AtlasInfo*> pAtlasInfo : fontCache.GetInstanceIdentifiers().GetValidElementView(m_atlases.GetView()))
		{
			if (!pAtlasInfo.IsValid() || !pAtlasInfo->CanRender(textureCache, logicalDeviceIdentifier))
			{
				continue;
			}

			for (const Optional<RenderTargetInfo*> pRenderTargetInfo : pAtlasInfo->m_renderTargets)
			{
				if (!pRenderTargetInfo->IsValid())
				{
					continue;
				}

				barrierCommandEncoder.TransitionImageLayout(
					Rendering::PipelineStageFlags::FragmentShader,
					Rendering::AccessFlags::ShaderRead,
					Rendering::ImageLayout::ShaderReadOnlyOptimal,
					*pRenderTargetInfo->m_pRenderTargetTexture,
					Rendering::ImageSubresourceRange{Rendering::ImageAspectFlags::Color}
				);
			}
		}
	}

	bool DrawTextStage::AtlasInfo::CanRegisterRenderItems() const
	{
		return m_pFontAtlas.IsValid() && m_pFontAtlas->HasLoadedGlyphs();
	}

	bool DrawTextStage::AtlasInfo::CanRender(
		Rendering::TextureCache& textureCache, const Rendering::LogicalDeviceIdentifier logicalDeviceIdentifier
	) const
	{
		return m_pFontAtlas.IsValid() && textureCache.IsRenderTextureLoaded(logicalDeviceIdentifier, m_pFontAtlas->GetTextureIdentifier()) &&
		       m_fontDescriptorSet.IsValid() && m_fontSampler.IsValid();
	}

	bool DrawTextStage::ShouldRecordCommands() const
	{
		if (m_pipeline.IsValid())
		{
			Manager& fontManager = *System::FindPlugin<Manager>();
			Cache& fontCache = fontManager.GetCache();
			Rendering::TextureCache& textureCache = System::Get<Rendering::Renderer>().GetTextureCache();
			const Rendering::LogicalDeviceIdentifier logicalDeviceIdentifier = m_sceneView.GetLogicalDevice().GetIdentifier();

			for (const Optional<AtlasInfo*> pAtlasInfo : fontCache.GetInstanceIdentifiers().GetValidElementView(m_atlases.GetView()))
			{
				if (pAtlasInfo.IsValid() && pAtlasInfo->CanRender(textureCache, logicalDeviceIdentifier) && pAtlasInfo->m_renderTargets.GetView().Any(
                    [](const Optional<RenderTargetInfo*> pRenderTarget)
                    {
                        return pRenderTarget->IsValid() && !pRenderTarget->m_registeredTexts.IsEmpty();
                    }
                ))
				{
					return true;
				}
			}
		}

		return false;
	}
}
