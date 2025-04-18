#include <Renderer/Assets/Material/RenderMaterial.h>
#include <Renderer/Assets/Material/RenderMaterialCache.h>
#include <Renderer/Assets/Material/RenderMaterialInstance.h>
#include <Renderer/Assets/Material/RuntimeMaterialInstance.h>
#include <Renderer/Assets/Material/RuntimeMaterial.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Assets/Texture/MipMask.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Renderer.h>

#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Memory/AddressOf.h>

#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/System/Query.h>

namespace ngine::Rendering
{
	RenderMaterialInstance::RenderMaterialInstance(RenderMaterialCache& renderMaterialCache, RenderMaterial& renderMaterial)
		: m_renderMaterialCache(renderMaterialCache)
		, m_material(renderMaterial)
	{
	}

	[[nodiscard]] ImageMappingType GetMappingType(const TexturePreset preset)
	{
		switch (preset)
		{
			case TexturePreset::EnvironmentCubemapDiffuseHDR:
			case TexturePreset::EnvironmentCubemapSpecular:
				return ImageMappingType::Cube;
			default:
				return ImageMappingType::TwoDimensional;
		}
	};

	void RenderMaterialInstance::LoadMaterialDescriptors(LogicalDevice& logicalDevice, RuntimeMaterialInstance& materialInstance)
	{
		Assert(m_material->GetMaterial().IsValid());
		const Rendering::MaterialAsset& materialAsset = *m_material->GetMaterial().GetAsset();
		if (materialAsset.GetDescriptorBindings().HasElements())
		{
			const DescriptorSetLayoutView descriptorLayout = *m_material;
			Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
			const bool allocatedDescriptorSets = thread.GetRenderData()
			                                       .GetDescriptorPool(logicalDevice.GetIdentifier())
			                                       .AllocateDescriptorSets(
																							 logicalDevice,
																							 ArrayView<const DescriptorSetLayoutView, uint8>(descriptorLayout),
																							 ArrayView<DescriptorSet, uint8>(m_descriptorSet)
																						 );
			Assert(allocatedDescriptorSets);
			if (LIKELY(allocatedDescriptorSets))
			{
				m_pDescriptorSetLoadingThread = &thread;

				const RestrictedArrayView<const MaterialAsset::DescriptorBinding, uint8> descriptorBindings = materialAsset.GetDescriptorBindings();
				RuntimeMaterialInstance::DescriptorContents::ConstView descriptorContents = materialInstance.GetDescriptorContents();

				m_descriptorContents.Reserve(descriptorBindings.GetSize());
				m_loadingTextureCount = descriptorBindings.GetSize();

				for (const MaterialAsset::DescriptorBinding& __restrict binding : descriptorBindings)
				{
					switch (binding.m_type)
					{
						case DescriptorContentType::Invalid:
							ExpectUnreachable();
						case DescriptorContentType::Texture:
						{
							const RuntimeMaterialInstance::DescriptorContent& __restrict content = descriptorContents[0];
							m_descriptorContents
								.EmplaceBack(content.m_textureData.m_textureIdentifier, Sampler(logicalDevice, content.m_textureData.m_addressMode));

							Threading::Job* pJob = logicalDevice.GetRenderer().GetTextureCache().GetOrLoadRenderTexture(
								logicalDevice.GetIdentifier(),
								content.m_textureData.m_textureIdentifier,
								GetMappingType(binding.m_samplerInfo.m_texturePreset),
								AllMips,
								TextureLoadFlags::Default,
								TextureCache::TextureLoadListenerData(*this, &RenderMaterialInstance::OnTextureLoadedAsync)
							);
							if (pJob != nullptr)
							{
								pJob->Queue(thread);
							}
						}
						break;
					}

					descriptorContents++;
				}
			}
		}
	}

	Threading::JobBatch RenderMaterialInstance::Load(SceneView& sceneView, const MaterialInstanceIdentifier identifier)
	{
		LogicalDevice& logicalDevice = sceneView.GetLogicalDevice();
		Rendering::MaterialInstanceCache& instanceCache = logicalDevice.GetRenderer().GetMaterialCache().GetInstanceCache();

		Threading::JobBatch batch = Threading::JobBatch::IntermediateStage;
		Threading::IntermediateStage& loadedInstanceStage = Threading::CreateIntermediateStage();
		loadedInstanceStage.AddSubsequentStage(batch.GetFinishedStage());

		Threading::JobBatch instanceLoadJob = instanceCache.TryLoad(
			identifier,
			Rendering::MaterialInstanceCache::OnLoadedListenerData{
				*this,
				[&loadedInstanceStage, &logicalDevice, &sceneView, identifier](RenderMaterialInstance& renderMaterialInstance)
				{
					Rendering::MaterialInstanceCache& instanceCache = logicalDevice.GetRenderer().GetMaterialCache().GetInstanceCache();
					RuntimeMaterialInstance& materialInstance = *instanceCache.GetMaterialInstance(identifier);
					Assert(materialInstance.HasFinishedLoading());

					Assert(
						materialInstance.GetMaterialIdentifier().IsValid() &&
						&renderMaterialInstance.m_material->GetMaterial() ==
							&*logicalDevice.GetRenderer().GetMaterialCache().GetMaterial(materialInstance.GetMaterialIdentifier())
					);

					materialInstance.OnDescriptorContentChanged.Add(
						renderMaterialInstance,
						[&logicalDevice](
							RenderMaterialInstance& materialInstance,
							const RuntimeDescriptorContent& previousDescriptorContent,
							const uint8 index
						)
						{
							materialInstance.OnDescriptorContentChanged(index, previousDescriptorContent, logicalDevice);
						}
					);

					materialInstance.OnParentMaterialChanged.Add(
						renderMaterialInstance,
						[&sceneView](RenderMaterialInstance& materialInstance)
						{
							materialInstance.OnParentMaterialChanged(sceneView);
						}
					);

					renderMaterialInstance.GetMaterial().CreateDescriptorSetLayout(logicalDevice);
					renderMaterialInstance.LoadMaterialDescriptors(logicalDevice, materialInstance);

					renderMaterialInstance.m_pMaterialInstance = &materialInstance;

					renderMaterialInstance.m_material->OnMaterialInstanceLoaded(identifier);

					if (!loadedInstanceStage.HasDependencies())
					{
						loadedInstanceStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
					}
					return EventCallbackResult::Remove;
				}
			}
		);
		if (instanceLoadJob.IsValid())
		{
			batch.QueueAfterStartStage(instanceLoadJob);
		}

		return batch;
	}

	void RenderMaterialInstance::Destroy(const LogicalDevice& logicalDevice)
	{
		for (DescriptorContent& descriptorContent : m_descriptorContents)
		{
			switch (descriptorContent.m_type)
			{
				case DescriptorContentType::Invalid:
					ExpectUnreachable();
				case DescriptorContentType::Texture:
				{
					descriptorContent.m_texture.m_textureView.Destroy(logicalDevice);
					descriptorContent.m_texture.m_sampler.Destroy(logicalDevice);
				}
				break;
			}
		}

		if (m_pDescriptorSetLoadingThread != nullptr)
		{
			Threading::EngineJobRunnerThread& previousLoadingThread =
				static_cast<Threading::EngineJobRunnerThread&>(*m_pDescriptorSetLoadingThread);
			previousLoadingThread.GetRenderData().DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(m_descriptorSet));
			m_pDescriptorSetLoadingThread = nullptr;
		}

		if (m_pMaterialInstance != nullptr)
		{
			Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
			for (const RuntimeMaterialInstance::DescriptorContent& descriptorContent : m_pMaterialInstance->GetDescriptorContents())
			{
				renderer.GetTextureCache()
					.RemoveRenderTextureListener(logicalDevice.GetIdentifier(), descriptorContent.m_textureData.m_textureIdentifier, this);
			}
		}
	}

	EventCallbackResult RenderMaterialInstance::
		OnTextureLoadedAsync(LogicalDevice& logicalDevice, const TextureIdentifier identifier, RenderTexture& texture, [[maybe_unused]] const MipMask changedMipValues, const EnumFlags<LoadedTextureFlags>)
	{
		Threading::UniqueLock lock(m_textureLoadMutex);

		for (DescriptorContent& __restrict descriptorContent : m_descriptorContents)
		{
			switch (descriptorContent.m_type)
			{
				case DescriptorContentType::Invalid:
					ExpectUnreachable();
				case DescriptorContentType::Texture:
				{
					if (descriptorContent.m_texture.m_identifier == identifier)
					{
						const ImageMappingType mappingType = texture.GetTotalArrayCount() == 6 ? ImageMappingType::Cube
						                                                                       : ImageMappingType::TwoDimensional;
						ImageMapping imageMapping = ImageMapping(
							logicalDevice,
							texture,
							mappingType,
							texture.GetFormat(),
							ImageAspectFlags::Color,
							texture.GetAvailableMipRange(),
							ArrayRange{0, texture.GetTotalArrayCount()}
						);

						const uint8 baseBindingIndex = m_descriptorContents.GetIteratorIndex(Memory::GetAddressOf(descriptorContent)) * 2;

						if (!descriptorContent.m_texture.m_textureView.IsValid())
						{
							Assert(m_descriptorSet.IsValid());
							if (UNLIKELY(!m_descriptorSet.IsValid()))
							{
								m_loadingTextureCount--;
								return EventCallbackResult::Keep;
							}

							Rendering::DescriptorSet::Update(
								logicalDevice,
								Array<const Rendering::DescriptorSet::UpdateInfo, 2>{
									Rendering::DescriptorSet::UpdateInfo{
										m_descriptorSet,
										baseBindingIndex,
										0,
										Rendering::DescriptorType::SampledImage,
										Array<const Rendering::DescriptorSet::ImageInfo, 1>{
											Rendering::DescriptorSet::ImageInfo{{}, imageMapping, Rendering::ImageLayout::ShaderReadOnlyOptimal}
										}.GetDynamicView()
									},
									Rendering::DescriptorSet::UpdateInfo{
										m_descriptorSet,
										uint16(baseBindingIndex + 1u),
										0,
										Rendering::DescriptorType::Sampler,
										Array<const Rendering::DescriptorSet::ImageInfo, 1>{Rendering::DescriptorSet::ImageInfo{
																																					descriptorContent.m_texture.m_sampler,
																																					{},
																																					Rendering::ImageLayout::ShaderReadOnlyOptimal
																																				}
							      }.GetDynamicView()
									}
								}.GetDynamicView()
							);

							descriptorContent.m_texture.m_textureView = Move(imageMapping);

							m_loadingTextureCount--;
						}
						else if (m_loadingTextureCount.Load() > 0)
						{
							Assert(m_descriptorSet.IsValid());
							if (UNLIKELY(!m_descriptorSet.IsValid()))
							{
								return EventCallbackResult::Keep;
							}

							Rendering::DescriptorSet::Update(
								logicalDevice,
								Array<const Rendering::DescriptorSet::UpdateInfo, 2>{
									Rendering::DescriptorSet::UpdateInfo{
										m_descriptorSet,
										baseBindingIndex,
										0,
										Rendering::DescriptorType::SampledImage,
										Array<const Rendering::DescriptorSet::ImageInfo, 1>{
											Rendering::DescriptorSet::ImageInfo{{}, imageMapping, Rendering::ImageLayout::ShaderReadOnlyOptimal}
										}.GetDynamicView()
									},
									Rendering::DescriptorSet::UpdateInfo{
										m_descriptorSet,
										uint16(baseBindingIndex + 1u),
										0,
										Rendering::DescriptorType::Sampler,
										Array<const Rendering::DescriptorSet::ImageInfo, 1>{Rendering::DescriptorSet::ImageInfo{
																																					descriptorContent.m_texture.m_sampler,
																																					{},
																																					Rendering::ImageLayout::ShaderReadOnlyOptimal
																																				}
							      }.GetDynamicView()
									}
								}.GetDynamicView()
							);

							if (descriptorContent.m_texture.m_textureView.IsValid())
							{
								descriptorContent.m_texture.m_textureView.Destroy(logicalDevice);
							}

							descriptorContent.m_texture.m_textureView = Move(imageMapping);
						}
						else
						{
							DescriptorSet newDescriptorSet;
							const DescriptorSetLayoutView descriptorLayout = *m_material;
							Threading::EngineJobRunnerThread& thread = *Threading::EngineJobRunnerThread::GetCurrent();
							const DescriptorPoolView descriptorPool = thread.GetRenderData().GetDescriptorPool(logicalDevice.GetIdentifier());
							const bool allocatedDescriptorSets = descriptorPool.AllocateDescriptorSets(
								logicalDevice,
								ArrayView<const DescriptorSetLayoutView, uint8>(descriptorLayout),
								ArrayView<DescriptorSet, uint8>(newDescriptorSet)
							);
							Assert(allocatedDescriptorSets);
							if (UNLIKELY(!allocatedDescriptorSets))
							{
								imageMapping.Destroy(logicalDevice);
								return EventCallbackResult::Keep;
							}

							const uint8 numCopies = (m_descriptorContents.GetSize() - 1) * 2;
							if (numCopies > 0)
							{
								FixedCapacityVector<DescriptorSet::CopyInfo, uint8> copyInfo(Memory::Reserve, (uint8)numCopies);
								for (uint8 otherBindingIndex = 0; otherBindingIndex < baseBindingIndex; ++otherBindingIndex)
								{
									copyInfo.EmplaceBack(DescriptorSet::CopyInfo{
										DescriptorSet::CopyInfo::Set{m_descriptorSet, otherBindingIndex, 0},
										DescriptorSet::CopyInfo::Set{newDescriptorSet, otherBindingIndex, 0},
										1
									});
								}

								for (uint8 otherBindingIndex = baseBindingIndex + 2, bindingCount = m_descriptorContents.GetSize() * 2;
								     otherBindingIndex < bindingCount;
								     ++otherBindingIndex)
								{
									copyInfo.EmplaceBack(DescriptorSet::CopyInfo{
										DescriptorSet::CopyInfo::Set{m_descriptorSet, otherBindingIndex, 0},
										DescriptorSet::CopyInfo::Set{newDescriptorSet, otherBindingIndex, 0},
										1
									});
								}

								DescriptorSet::UpdateAndCopy(
									logicalDevice,
									Array{
										DescriptorSet::UpdateInfo{
											newDescriptorSet,
											baseBindingIndex,
											0,
											DescriptorType::SampledImage,
											Array{DescriptorSet::ImageInfo{{}, imageMapping, ImageLayout::ShaderReadOnlyOptimal}},
										},
										DescriptorSet::UpdateInfo{
											newDescriptorSet,
											uint16(baseBindingIndex + 1u),
											0,
											DescriptorType::Sampler,
											Array{DescriptorSet::ImageInfo{descriptorContent.m_texture.m_sampler, {}, ImageLayout::ShaderReadOnlyOptimal}},
										}
									},
									copyInfo.GetView()
								);
							}
							else
							{
								DescriptorSet::Update(
									logicalDevice,
									Array{
										DescriptorSet::UpdateInfo{
											newDescriptorSet,
											baseBindingIndex,
											0,
											DescriptorType::SampledImage,
											Array{DescriptorSet::ImageInfo{{}, imageMapping, ImageLayout::ShaderReadOnlyOptimal}},
										},
										DescriptorSet::UpdateInfo{
											newDescriptorSet,
											uint16(baseBindingIndex + 1u),
											0,
											DescriptorType::Sampler,
											Array{DescriptorSet::ImageInfo{descriptorContent.m_texture.m_sampler, {}, ImageLayout::ShaderReadOnlyOptimal}},
										}
									}
								);
							}

							m_descriptorSet.AtomicSwap(newDescriptorSet);

							descriptorContent.m_texture.m_textureView.AtomicSwap(imageMapping);

							thread.GetRenderData().DestroyImageMapping(logicalDevice.GetIdentifier(), Move(imageMapping));
							m_pDescriptorSetLoadingThread->GetRenderData().DestroyDescriptorSet(logicalDevice.GetIdentifier(), Move(newDescriptorSet));
							m_pDescriptorSetLoadingThread = &thread;
							return EventCallbackResult::Keep;
						}
					}
				}
				break;
			}
		}
		return EventCallbackResult::Keep;
	}

	void RenderMaterialInstance::OnDescriptorContentChanged(
		const uint8 index, const RuntimeDescriptorContent& previousDescriptorContent, LogicalDevice& logicalDevice
	)
	{
		if (m_pMaterialInstance == nullptr)
		{
			// Nothing loaded yet
			return;
		}

		switch (previousDescriptorContent.m_type)
		{
			case DescriptorContentType::Invalid:
				ExpectUnreachable();
			case DescriptorContentType::Texture:
			{
				[[maybe_unused]] const bool wasRemoved = logicalDevice.GetRenderer().GetTextureCache().RemoveRenderTextureListener(
					logicalDevice.GetIdentifier(),
					previousDescriptorContent.m_textureData.m_textureIdentifier,
					this
				);
				Assert(wasRemoved);

				DescriptorContent& newDescriptorContent = m_descriptorContents[index];
				const RuntimeDescriptorContent& newRuntimeDescriptorContent = m_pMaterialInstance->GetDescriptorContents()[index];

				newDescriptorContent.m_texture.m_identifier = newRuntimeDescriptorContent.m_textureData.m_textureIdentifier;
				if (newRuntimeDescriptorContent.m_textureData.m_addressMode != previousDescriptorContent.m_textureData.m_addressMode)
				{
					// TODO: Delay deletion
					newDescriptorContent.m_texture.m_sampler.Destroy(logicalDevice);
					newDescriptorContent.m_texture.m_sampler = Sampler(logicalDevice, newRuntimeDescriptorContent.m_textureData.m_addressMode);
				}

				const MaterialAsset& materialAsset = *m_material->GetMaterial().GetAsset();
				const MaterialAsset::DescriptorBinding& descriptorBinding = materialAsset.GetDescriptorBindings()[index];

				Threading::Job* pJob = logicalDevice.GetRenderer().GetTextureCache().GetOrLoadRenderTexture(
					logicalDevice.GetIdentifier(),
					newDescriptorContent.m_texture.m_identifier,
					GetMappingType(descriptorBinding.m_samplerInfo.m_texturePreset),
					AllMips,
					TextureLoadFlags::Default,
					TextureCache::TextureLoadListenerData(*this, &RenderMaterialInstance::OnTextureLoadedAsync)
				);

				if (pJob != nullptr)
				{
					pJob->Queue(System::Get<Threading::JobManager>());
				}
			}
			break;
		}
	}

	void RenderMaterialInstance::OnParentMaterialChanged(SceneView& sceneView)
	{
		LogicalDevice& logicalDevice = sceneView.GetLogicalDevice();

		const MaterialIdentifier materialIdentifier = m_pMaterialInstance->GetMaterialIdentifier();
		if (m_pMaterialInstance == nullptr)
		{
			// Nothing loaded yet
			m_material = m_renderMaterialCache.FindOrLoad(logicalDevice.GetRenderer().GetMaterialCache(), materialIdentifier);
			return;
		}

		for (DescriptorContent& oldDescriptorContent : m_descriptorContents)
		{
			switch (oldDescriptorContent.m_type)
			{
				case DescriptorContentType::Invalid:
					ExpectUnreachable();
				case DescriptorContentType::Texture:
				{
					[[maybe_unused]] const bool wasRemoved = logicalDevice.GetRenderer().GetTextureCache().RemoveRenderTextureListener(
						logicalDevice.GetIdentifier(),
						oldDescriptorContent.m_texture.m_identifier,
						this
					);
					Assert(wasRemoved);

					// TODO: Delayed destroy
					new Sampler(Move(oldDescriptorContent.m_texture.m_sampler));
					new ImageMapping(Move(oldDescriptorContent.m_texture.m_textureView));
				}
				break;
			}
		}
		m_descriptorContents.Clear();

		m_material = m_renderMaterialCache.FindOrLoad(logicalDevice.GetRenderer().GetMaterialCache(), materialIdentifier);
		LoadMaterialDescriptors(logicalDevice, *m_pMaterialInstance);
	}

	bool RenderMaterialInstance::IsValid() const
	{
		return (m_pMaterialInstance != nullptr) && m_pMaterialInstance->IsValid() & (m_loadingTextureCount.Load() == 0);
	}
}
