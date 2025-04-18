#include "Assets/Material/MaterialCache.h"
#include "Assets/Material/MaterialAsset.h"

#include <Common/Memory/OffsetOf.h>
#include <Common/Serialization/Deserialize.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Threading/Jobs/JobBatch.h>

#include <Renderer/Assets/Material/RuntimeMaterial.h>
#include <Renderer/Assets/Material/RuntimeMaterialInstance.h>
#include <Renderer/Renderer.h>

#include <Engine/Asset/AssetType.inl>
#include <Common/System/Query.h>
#include <Common/IO/Log.h>
#include <Common/Serialization/Guid.h>

namespace ngine::Rendering
{
	[[nodiscard]] PURE_LOCALS_AND_POINTERS MaterialCache& MaterialInstanceCache::GetMaterialCache()
	{
		return Memory::GetOwnerFromMember(*this, &MaterialCache::m_instanceCache);
	}

	[[nodiscard]] PURE_LOCALS_AND_POINTERS const MaterialCache& MaterialInstanceCache::GetMaterialCache() const
	{
		return Memory::GetConstOwnerFromMember(*this, &MaterialCache::m_instanceCache);
	}

	MaterialInstanceInfo::MaterialInstanceInfo(
		UniquePtr<RuntimeMaterialInstance>&& pMaterialInstance, MaterialInstanceLoadingCallback&& loadingCallback
	)
		: m_pMaterialInstance(Forward<UniquePtr<RuntimeMaterialInstance>>(pMaterialInstance))
		, m_loadingCallback(Forward<MaterialInstanceLoadingCallback>(loadingCallback))
	{
	}
	MaterialInstanceInfo::MaterialInstanceInfo(MaterialInstanceInfo&& other) = default;
	MaterialInstanceInfo::~MaterialInstanceInfo() = default;

	MaterialInstanceCache::MaterialInstanceCache()
	{
	}

	MaterialInstanceCache::~MaterialInstanceCache()
	{
	}

	MaterialInstanceIdentifier MaterialInstanceCache::Clone(const MaterialInstanceIdentifier templateIdentifier)
	{
		return RegisterProceduralAsset(
			[templateIdentifier, this](const MaterialInstanceIdentifier, const Asset::Guid) mutable
			{
				return MaterialInstanceInfo{
					UniquePtr<RuntimeMaterialInstance>::Make(templateIdentifier, MaterialInstanceFlags::IsClone),
					[this, templateIdentifier](const MaterialInstanceIdentifier identifier) -> Threading::JobBatch
					{
						Threading::JobBatch jobBatch = Threading::JobBatch::IntermediateStage;
						Threading::IntermediateStage& finishedLoadingTemplateStage = Threading::CreateIntermediateStage();
						finishedLoadingTemplateStage.AddSubsequentStage(jobBatch.GetFinishedStage());

						Threading::JobBatch templateLoadJobBatch = TryLoad(
							templateIdentifier,
							OnLoadedListenerData{
								reinterpret_cast<void*>(identifier.GetIndex()),
								[&finishedLoadingTemplateStage](const void*)
								{
									finishedLoadingTemplateStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
									return EventCallbackResult::Remove;
								}
							}
						);
						if (templateLoadJobBatch.IsValid())
						{
							jobBatch.QueueAfterStartStage(templateLoadJobBatch);
						}

						Threading::Job& cloneFromTemplateJob = Threading::CreateCallback(
							[this, templateIdentifier, identifier](Threading::JobRunnerThread&)
							{
								RuntimeMaterialInstance& runtimeMaterialInstance = *GetMaterialInstance(templateIdentifier);
								Assert(runtimeMaterialInstance.HasFinishedLoading());
								if (LIKELY(runtimeMaterialInstance.IsValid()))
								{
									*GetAssetData(identifier).m_pMaterialInstance = runtimeMaterialInstance;
									OnLoadingFinished(identifier);
								}
								else
								{
									OnLoadingFailed(identifier);
								}
							},
							Threading::JobPriority::LoadMaterialInstance
						);

						jobBatch.QueueAsNewFinishedStage(cloneFromTemplateJob);
						return jobBatch;
					}
				};
			}
		);
	}

	MaterialInstanceIdentifier
	MaterialInstanceCache::Create(const MaterialIdentifier materialIdentifier, RuntimeDescriptorContent&& descriptorContent)
	{
		return RegisterProceduralAsset(
			[materialIdentifier,
		   descriptorContent = Forward<Rendering::RuntimeDescriptorContent>(descriptorContent),
		   this](const MaterialInstanceIdentifier, const Asset::Guid) mutable
			{
				return MaterialInstanceInfo{
					UniquePtr<RuntimeMaterialInstance>::Make(
						materialIdentifier,
						MaterialInstanceIdentifier{},
						Move(descriptorContent),
						MaterialInstanceFlags{}
					),
					[this](const MaterialInstanceIdentifier identifier) -> Threading::JobBatch
					{
						OnLoadingFinished(identifier);
						return {};
					}
				};
			}
		);
	}

	MaterialInstanceIdentifier MaterialInstanceCache::FindOrRegisterAsset(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[this](const MaterialInstanceIdentifier, const Asset::Guid)
			{
				return MaterialInstanceInfo{
					UniquePtr<RuntimeMaterialInstance>::Make(MaterialInstanceIdentifier{}, MaterialInstanceFlags{}),
					[this](const MaterialInstanceIdentifier identifier) -> Threading::JobBatch
					{
						return System::Get<Asset::Manager>().RequestAsyncLoadAssetMetadata(
							GetAssetGuid(identifier),
							Threading::JobPriority::LoadMaterialInstance,
							[this, identifier](const ConstByteView data)
							{
								if (LIKELY(data.HasElements()))
								{
									RuntimeMaterialInstance& materialInstance = *GetMaterialInstance(identifier);

									Serialization::RootReader materialInstanceSerializer = Serialization::GetReaderFromBuffer(
										ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
									);
									if (UNLIKELY(!materialInstanceSerializer.GetData().IsValid()))
									{
										System::Get<Log>().Error(
											SOURCE_LOCATION,
											"Material instance {0} could not be parsed from buffer!",
											GetAssetGuid(identifier).ToString()
										);
										OnLoadingFailed(identifier);
										return;
									}

									Serialization::Reader reader{materialInstanceSerializer};
									const Guid materialAssetGuid = reader.ReadWithDefaultValue<Guid>("material", {});
									Assert(materialAssetGuid.IsValid());

									const MaterialIdentifier materialIdentifier =
										System::Get<Rendering::Renderer>().GetMaterialCache().FindOrRegisterAsset(materialAssetGuid);

									MaterialCache& materialCache = GetMaterialCache();
									Threading::JobBatch materialLoadJobBatch = materialCache.TryLoad(
										materialIdentifier,
										MaterialCache::OnLoadedListenerData{
											this,
											[identifier, &materialInstance, materialInstanceSerializer = Move(materialInstanceSerializer), materialIdentifier](
												MaterialInstanceCache& materialInstanceCache
											)
											{
												Serialization::Reader reader{materialInstanceSerializer};
												const bool wasDeserialized = materialInstance.Serialize(reader, materialIdentifier);
												if (LIKELY(wasDeserialized))
												{
													materialInstanceCache.OnLoadingFinished(identifier);
												}
												else
												{
													System::Get<Log>().Error(
														SOURCE_LOCATION,
														"Material instance {0} could not be deserialized from buffer!",
														materialInstanceCache.GetAssetGuid(identifier).ToString()
													);
													materialInstanceCache.OnLoadingFailed(identifier);
												}
												return EventCallbackResult::Remove;
											}
										}
									);
									if (materialLoadJobBatch.IsValid())
									{
										Threading::JobRunnerThread::GetCurrent()->Queue(materialLoadJobBatch);
									}
								}
								else
								{
									System::Get<Log>().Error(
										SOURCE_LOCATION,
										"Material instance data was empty when loading asset {0}!",
										GetAssetGuid(identifier).ToString()
									);
									OnLoadingFailed(identifier);
								}
							}
						);
					}
				};
			}
		);
	}

	void MaterialInstanceCache::OnLoadingFinished(const MaterialInstanceIdentifier identifier)
	{
		RuntimeMaterialInstance& materialInstance = *GetMaterialInstance(identifier);
		materialInstance.OnLoadingFinished();

		{
			Threading::SharedLock lock(m_instanceRequesterMutex);
			auto it = m_instanceRequesterMap.Find(identifier);
			if (it != m_instanceRequesterMap.end())
			{
				InstanceRequesters& requesters = *it->second;
				lock.Unlock();
				requesters.m_onLoaded();
			}
		}

		[[maybe_unused]] const bool wasCleared = m_loadingMaterialInstances.Clear(identifier);
		Assert(wasCleared);
	}

	void MaterialInstanceCache::OnLoadingFailed(const MaterialInstanceIdentifier identifier)
	{
		RuntimeMaterialInstance& materialInstance = *GetMaterialInstance(identifier);
		materialInstance.OnLoadingFailed();

		{
			Threading::SharedLock lock(m_instanceRequesterMutex);
			auto it = m_instanceRequesterMap.Find(identifier);
			if (it != m_instanceRequesterMap.end())
			{
				InstanceRequesters& requesters = *it->second;
				lock.Unlock();
				requesters.m_onLoaded();
			}
		}

		[[maybe_unused]] const bool wasCleared = m_loadingMaterialInstances.Clear(identifier);
		Assert(wasCleared);
	}

	Threading::JobBatch MaterialInstanceCache::TryLoad(const MaterialInstanceIdentifier identifier, OnLoadedListenerData&& newListenerData)
	{
		auto getInstanceRequesters = [this, identifier]() -> InstanceRequesters&
		{
			{
				Threading::SharedLock lock(m_instanceRequesterMutex);
				auto it = m_instanceRequesterMap.Find(identifier);
				if (it != m_instanceRequesterMap.end())
				{
					return *it->second;
				}
			}

			Threading::UniqueLock lock(m_instanceRequesterMutex);
			auto it = m_instanceRequesterMap.Find(identifier);
			if (it != m_instanceRequesterMap.end())
			{
				return it->second;
			}
			else
			{
				it = m_instanceRequesterMap.Emplace(MaterialInstanceIdentifier(identifier), UniqueRef<InstanceRequesters>::Make());
				return it->second;
			}
		};

		InstanceRequesters* pRequesters = newListenerData.m_callback.IsValid() ? &getInstanceRequesters() : nullptr;

		if (pRequesters != nullptr)
		{
			pRequesters->m_onLoaded.Emplace(Forward<OnLoadedListenerData>(newListenerData));
		}

		MaterialInstanceInfo& materialInstanceInfo = GetAssetData(identifier);
		UniquePtr<RuntimeMaterialInstance>& pMaterialInstance = materialInstanceInfo.m_pMaterialInstance;
		Assert(pMaterialInstance.IsValid());
		if (pMaterialInstance->HasFinishedLoading())
		{
			if (pRequesters != nullptr)
			{
				[[maybe_unused]] const bool wasExecuted = pRequesters->m_onLoaded.Execute(newListenerData.m_identifier);
				Assert(wasExecuted);
			}
		}
		else if (m_loadingMaterialInstances.Set(identifier))
		{
			if (!pMaterialInstance->HasFinishedLoading())
			{
				return materialInstanceInfo.m_loadingCallback(identifier);
			}
			else
			{
				// Callback should've been invoked
				m_loadingMaterialInstances.Clear(identifier);
			}
		}
		else
		{
			// Callback should've been invoked by another thread
		}

		return {};
	}

	bool MaterialInstanceCache::RemoveOnLoadListener(
		const MaterialInstanceIdentifier identifier, const OnLoadedListenerIdentifier listenerIdentifier
	)
	{
		Threading::UniqueLock lock(m_instanceRequesterMutex);
		auto it = m_instanceRequesterMap.Find(identifier);
		if (it != m_instanceRequesterMap.end())
		{
			InstanceRequesters& requesters = *it->second;
			lock.Unlock();
			return requesters.m_onLoaded.Remove(listenerIdentifier);
		}
		return false;
	}

	[[nodiscard]] PURE_LOCALS_AND_POINTERS Renderer& MaterialCache::GetRenderer()
	{
		return Memory::GetOwnerFromMember(*this, &Renderer::m_materialCache);
	}

	[[nodiscard]] PURE_LOCALS_AND_POINTERS const Renderer& MaterialCache::GetRenderer() const
	{
		return Memory::GetConstOwnerFromMember(*this, &Renderer::m_materialCache);
	}

	MaterialInfo::MaterialInfo(UniquePtr<RuntimeMaterial>&& pMaterial, MaterialLoadingCallback&& loadingCallback)
		: m_pMaterial(Forward<UniquePtr<RuntimeMaterial>>(pMaterial))
		, m_loadingCallback(Forward<MaterialLoadingCallback>(loadingCallback))
	{
	}
	MaterialInfo::MaterialInfo(MaterialInfo&& other) = default;
	MaterialInfo::~MaterialInfo() = default;
	MaterialCache::MaterialCache() = default;
	MaterialCache::~MaterialCache() = default;

	MaterialIdentifier MaterialCache::FindOrRegisterAsset(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[this](const MaterialIdentifier identifier, const Asset::Guid) mutable
			{
				return MaterialInfo{
					UniquePtr<RuntimeMaterial>::Make(identifier),
					[this](const MaterialIdentifier identifier) -> Threading::JobBatch
					{
						return System::Get<Asset::Manager>().RequestAsyncLoadAssetMetadata(
							GetAssetGuid(identifier),
							Threading::JobPriority::LoadMaterialInstance,
							[this, identifier](const ConstByteView data)
							{
								if (LIKELY(data.HasElements()))
								{
									RuntimeMaterial& material = *GetMaterial(identifier);

									const bool wasDeserialized = Serialization::DeserializeFromBuffer(
										ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))},
										material
									);
									if (LIKELY(wasDeserialized))
									{
										OnLoadingFinished(identifier);
									}
									else
									{
										System::Get<Log>()
											.Error(SOURCE_LOCATION, "Material {0} could not be deserialized from buffer!", GetAssetGuid(identifier).ToString());
										OnLoadingFailed(identifier);
									}
								}
								else
								{
									System::Get<Log>()
										.Error(SOURCE_LOCATION, "Material data was empty when loading asset {0}!", GetAssetGuid(identifier).ToString());
									OnLoadingFailed(identifier);
								}
							}
						);
					}
				};
			}
		);
	}

	void MaterialCache::OnLoadingFinished(const MaterialIdentifier identifier)
	{
		RuntimeMaterial& material = *GetMaterial(identifier);
		material.OnLoadingFinished();

		{
			Threading::SharedLock lock(m_instanceRequesterMutex);
			auto it = m_instanceRequesterMap.Find(identifier);
			if (it != m_instanceRequesterMap.end())
			{
				InstanceRequesters& requesters = *it->second;
				lock.Unlock();
				requesters.m_onLoaded();
			}
		}

		[[maybe_unused]] const bool wasCleared = m_loadingMaterials.Clear(identifier);
		Assert(wasCleared);
	}

	void MaterialCache::OnLoadingFailed(const MaterialIdentifier identifier)
	{
		RuntimeMaterial& material = *GetMaterial(identifier);
		material.OnLoadingFailed();

		{
			Threading::SharedLock lock(m_instanceRequesterMutex);
			auto it = m_instanceRequesterMap.Find(identifier);
			if (it != m_instanceRequesterMap.end())
			{
				InstanceRequesters& requesters = *it->second;
				lock.Unlock();
				requesters.m_onLoaded();
			}
		}

		[[maybe_unused]] const bool wasCleared = m_loadingMaterials.Clear(identifier);
		Assert(wasCleared);
	}

	Threading::JobBatch MaterialCache::TryLoad(const MaterialIdentifier identifier, OnLoadedListenerData&& newListenerData)
	{
		auto getInstanceRequesters = [this, identifier]() -> InstanceRequesters&
		{
			{
				Threading::SharedLock lock(m_instanceRequesterMutex);
				auto it = m_instanceRequesterMap.Find(identifier);
				if (it != m_instanceRequesterMap.end())
				{
					return it->second;
				}
			}

			Threading::UniqueLock lock(m_instanceRequesterMutex);
			auto it = m_instanceRequesterMap.Find(identifier);
			if (it != m_instanceRequesterMap.end())
			{
				return it->second;
			}
			else
			{
				it = m_instanceRequesterMap.Emplace(MaterialIdentifier(identifier), UniqueRef<InstanceRequesters>::Make());
				return it->second;
			}
		};

		InstanceRequesters* pRequesters = newListenerData.m_callback.IsValid() ? &getInstanceRequesters() : nullptr;

		pRequesters->m_onLoaded.Emplace(Forward<OnLoadedListenerData>(newListenerData));

		MaterialInfo& materialInfo = GetAssetData(identifier);
		UniquePtr<RuntimeMaterial>& pMaterial = materialInfo.m_pMaterial;
		Assert(pMaterial.IsValid());
		if (pMaterial->HasFinishedLoading())
		{
			if (pRequesters != nullptr)
			{
				[[maybe_unused]] const bool wasExecuted = pRequesters->m_onLoaded.Execute(newListenerData.m_identifier);
				Assert(wasExecuted);
			}
		}
		else if (m_loadingMaterials.Set(identifier))
		{
			if (!pMaterial->HasFinishedLoading())
			{
				return materialInfo.m_loadingCallback(identifier);
			}
			else
			{
				// Callback should've been invoked
				m_loadingMaterials.Clear(identifier);
			}
		}
		else
		{
			// Callback should've been invoked by another thread
		}

		return {};
	}

	bool MaterialCache::RemoveOnLoadListener(const MaterialIdentifier identifier, const OnLoadedListenerIdentifier listenerIdentifier)
	{
		Threading::UniqueLock lock(m_instanceRequesterMutex);
		auto it = m_instanceRequesterMap.Find(identifier);
		if (it != m_instanceRequesterMap.end())
		{
			InstanceRequesters& requesters = *it->second;
			lock.Unlock();
			return requesters.m_onLoaded.Remove(listenerIdentifier);
		}
		return false;
	}
}
