#include "Entity/Scene/ComponentTemplateCache.h"
#include "Scene/Scene.h"
#include "Entity/Scene/SceneChildInstance.h"

#include <Engine/Asset/AssetType.inl>
#include <Common/System/Query.h>
#include <Engine/Entity/Serialization/ComponentValue.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Data/InstanceGuid.h>

#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Serialization/Deserialize.h>
#include <Common/Asset/Format/Guid.h>
#include <Common/IO/Log.h>

namespace ngine::Entity
{
	ComponentTemplateCache::ComponentTemplateCache(Asset::Manager& assetManager)
		: m_pTemplateScene(UniquePtr<Scene>::Make(
				m_templateSceneRegistry, Invalid, 10000_meters, Guid::Generate(), Scene::Flags::IsDisabled | Scene::Flags::IsTemplate
			))
	{
		RegisterAssetModifiedCallback(assetManager);
	}

	ComponentTemplateCache::~ComponentTemplateCache() = default;

	SceneTemplate::~SceneTemplate() = default;

	ComponentTemplateIdentifier ComponentTemplateCache::FindOrRegister(const Asset::Guid guid)
	{
		return BaseType::FindOrRegisterAsset(
			guid,
			[](const ComponentTemplateIdentifier, const Asset::Guid) -> SceneTemplate
			{
				return SceneTemplate();
			}
		);
	}

	Threading::JobBatch ComponentTemplateCache::TryLoadScene(const ComponentTemplateIdentifier identifier, LoadListenerData&& newListenerData)
	{
		LoadEvent* pSceneRequesters;

		{
			Threading::SharedLock readLock(m_sceneRequesterMutex);
			decltype(m_sceneRequesterMap)::iterator it = m_sceneRequesterMap.Find(identifier);
			if (it != m_sceneRequesterMap.end())
			{
				pSceneRequesters = it->second.Get();
			}
			else
			{
				readLock.Unlock();
				Threading::UniqueLock writeLock(m_sceneRequesterMutex);
				it = m_sceneRequesterMap.Find(identifier);
				if (it != m_sceneRequesterMap.end())
				{
					pSceneRequesters = it->second.Get();
				}
				else
				{
					pSceneRequesters =
						m_sceneRequesterMap.Emplace(ComponentTemplateIdentifier(identifier), UniquePtr<LoadEvent>::Make())->second.Get();
				}
			}
		}

		pSceneRequesters->Emplace(Forward<LoadListenerData>(newListenerData));

		SceneTemplate& sceneTemplate = GetAssetData(identifier);
		if (sceneTemplate.m_pRootComponent.IsInvalid())
		{
			if (m_loadingScenes.Set(identifier))
			{
				if (sceneTemplate.m_pRootComponent.IsInvalid())
				{
					const Asset::Guid assetGuid = GetAssetGuid(identifier);

					Threading::Job* pLoadAssetJob = System::Get<Asset::Manager>().RequestAsyncLoadAssetMetadata(
						assetGuid,
						Threading::JobPriority::LoadSceneTemplate,
						[assetGuid, &sceneTemplate, this, identifier, pSceneRequesters](const ConstByteView data)
						{
							if (UNLIKELY(!data.HasElements()))
							{
								System::Get<Log>()
									.Warning(SOURCE_LOCATION, "Scene template load failed: Asset with guid {0} metadata could not be read", assetGuid);
								[[maybe_unused]] const bool wasCleared = m_loadingScenes.Clear(identifier);
								Assert(wasCleared);
								(*pSceneRequesters)(identifier);
								return;
							}

							Serialization::RootReader sceneSerializer = Serialization::GetReaderFromBuffer(
								ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
							);
							if (UNLIKELY(!sceneSerializer.GetData().IsValid()))
							{
								System::Get<Log>()
									.Warning(SOURCE_LOCATION, "Scene template load failed: Asset with guid {0} metadata was invalid", assetGuid);
								[[maybe_unused]] const bool wasCleared = m_loadingScenes.Clear(identifier);
								Assert(wasCleared);
								(*pSceneRequesters)(identifier);
								return;
							}

							const Serialization::Reader reader = sceneSerializer;
							Optional<Guid> typeGuid = reader.Read<Guid>("typeGuid");
							if (UNLIKELY(!typeGuid.IsValid()))
							{
								System::Get<Log>()
									.Warning(SOURCE_LOCATION, "Scene template load failed: Asset with guid {0} was missing type guid", assetGuid);
								[[maybe_unused]] const bool wasCleared = m_loadingScenes.Clear(identifier);
								Assert(wasCleared);
								(*pSceneRequesters)(identifier);
								return;
							}

							if (*typeGuid == Reflection::GetTypeGuid<RootSceneComponent>())
							{
								*typeGuid = Reflection::GetTypeGuid<Entity::SceneComponent>();
							}

							Entity::Manager& entityManager = System::Get<Entity::Manager>();
							Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();

							Entity::ComponentRegistry& registry = entityManager.GetRegistry();
							const Entity::ComponentTypeIdentifier typeIdentifier = registry.FindIdentifier(typeGuid.Get());
							if (UNLIKELY(!typeIdentifier.IsValid()))
							{
								System::Get<Log>()
									.Warning(SOURCE_LOCATION, "Scene template load failed: Asset with guid {0} component type could not be found", assetGuid);
								[[maybe_unused]] const bool wasCleared = m_loadingScenes.Clear(identifier);
								Assert(wasCleared);
								(*pSceneRequesters)(identifier);
								return;
							}

							const Optional<ComponentTypeInterface*> pComponentTypeInfo = registry.Get(typeIdentifier);
							if (UNLIKELY(!pComponentTypeInfo.IsValid()))
							{
								System::Get<Log>()
									.Warning(SOURCE_LOCATION, "Scene template load failed: Asset with guid {0} component type could not be found", assetGuid);
								[[maybe_unused]] const bool wasCleared = m_loadingScenes.Clear(identifier);
								Assert(wasCleared);
								(*pSceneRequesters)(identifier);
								return;
							}

							Threading::JobBatch componentLoadJobBatchOut;
							Component3D::Deserializer deserializer{
								Reflection::TypeDeserializer{reader, reflectionRegistry, componentLoadJobBatchOut},
								m_pTemplateScene->GetEntitySceneRegistry(),
								m_pTemplateScene->GetRootComponent()
							};
							Optional<Component*> pComponent =
								pComponentTypeInfo->DeserializeInstanceWithoutChildren(deserializer, m_pTemplateScene->GetEntitySceneRegistry());
							if (UNLIKELY(!pComponent.IsValid()))
							{
								LogWarning("Scene template load failed: Asset with guid {0} failed to deserialize", assetGuid);
								[[maybe_unused]] const bool wasCleared = m_loadingScenes.Clear(identifier);
								Assert(wasCleared);
								(*pSceneRequesters)(identifier);
								return;
							}

							Component3D& component = static_cast<Component3D&>(*pComponent);
							sceneTemplate.m_pRootComponent = &component;

							if (*typeGuid == Reflection::GetTypeGuid<RootSceneComponent>() || *typeGuid == Reflection::GetTypeGuid<SceneComponent>())
							{
								SceneComponent& sceneComponent = static_cast<SceneComponent&>(component);
								sceneComponent.SetSceneTemplateIdentifier(identifier);
							}

							Threading::JobBatch childJobBatch = component.Component3D::DeserializeDataComponentsAndChildren(deserializer.m_reader);
							deserializer.m_pJobBatch->QueueAsNewFinishedStage(childJobBatch);

							if (*typeGuid == Reflection::GetTypeGuid<SceneComponent>() || *typeGuid == Reflection::GetTypeGuid<RootSceneComponent>())
							{
								using ApplySceneChildInstance = void (*)(
									Component3D& component,
									ComponentTypeSceneData<SceneChildInstance>& sceneChildData,
									ComponentTypeSceneData<Data::InstanceGuid>& instanceGuidSceneData
								);
								static ApplySceneChildInstance applySceneChildInstance = [](
																																					 Component3D& __restrict component,
																																					 ComponentTypeSceneData<SceneChildInstance>& sceneChildData,
																																					 ComponentTypeSceneData<Data::InstanceGuid>& instanceGuidSceneData
																																				 )
								{
									const Guid componentInstanceGuid = instanceGuidSceneData.GetComponentImplementationUnchecked(component.GetIdentifier());

									if (const Optional<SceneChildInstance*> pSceneChildInstance = component.FindDataComponentOfType<SceneChildInstance>(sceneChildData))
									{
										pSceneChildInstance->m_parentTemplateInstanceGuid = componentInstanceGuid;
									}
									else
									{
										component.CreateDataComponent<SceneChildInstance>(sceneChildData, componentInstanceGuid, componentInstanceGuid);
									}

									for (Entity::Component3D& child : component.GetChildren())
									{
										applySceneChildInstance(child, sceneChildData, instanceGuidSceneData);
									}
								};

								Entity::SceneRegistry& templateSceneRegistry = m_pTemplateScene->GetEntitySceneRegistry();
								ComponentTypeSceneData<SceneChildInstance>& sceneChildData =
									*templateSceneRegistry.GetOrCreateComponentTypeData<SceneChildInstance>();
								ComponentTypeSceneData<Data::InstanceGuid>& instanceGuidSceneData =
									*templateSceneRegistry.GetOrCreateComponentTypeData<Data::InstanceGuid>();
								for (Entity::Component3D& child : component.GetChildren())
								{
									applySceneChildInstance(child, sceneChildData, instanceGuidSceneData);
								}
							}

							pComponentTypeInfo->OnComponentDeserialized(component, deserializer.m_reader, *deserializer.m_pJobBatch);

							if (componentLoadJobBatchOut.IsValid())
							{
								componentLoadJobBatchOut.QueueAsNewFinishedStage(Threading::CreateCallback(
									[this, identifier, pSceneRequesters](Threading::JobRunnerThread&)
									{
										[[maybe_unused]] const bool wasSet = m_loadedScenes.Set(identifier);
										Assert(wasSet);

										[[maybe_unused]] const bool wasCleared = m_loadingScenes.Clear(identifier);
										Assert(wasCleared);

										{
											Threading::SharedLock readLock(m_sceneRequesterMutex);
											const decltype(m_sceneRequesterMap)::const_iterator it = m_sceneRequesterMap.Find(identifier);
											if (it != m_sceneRequesterMap.end())
											{
												readLock.Unlock();
												(*pSceneRequesters)(identifier);
											}
										}
									},
									Threading::JobPriority::LoadScene
								));
								Threading::JobRunnerThread::GetCurrent()->Queue(componentLoadJobBatchOut);
							}
							else
							{
								[[maybe_unused]] const bool wasSet = m_loadedScenes.Set(identifier);
								Assert(wasSet);

								[[maybe_unused]] const bool wasCleared = m_loadingScenes.Clear(identifier);
								Assert(wasCleared);

								(*pSceneRequesters)(identifier);
							}
						}
					);

					if (LIKELY(pLoadAssetJob != nullptr))
					{
						Threading::JobBatch jobBatch{*pLoadAssetJob, Threading::CreateIntermediateStage("Finish Loading Scene Template Asset Stage")};
						return jobBatch;
					}
					else
					{
						return {};
					}
				}
			}
			else
			{
				return {};
			}
		}

		if (m_loadedScenes.IsSet(identifier))
		{
			// Scene template was already loaded, only notify the requester
			[[maybe_unused]] const bool wasExecuted = pSceneRequesters->Execute(newListenerData.m_identifier, identifier);
			Assert(wasExecuted);
		}
		return {};
	}

	bool ComponentTemplateCache::MigrateInstance(
		const ComponentTemplateIdentifier identifier, Component3D& otherComponent, SceneRegistry& otherSceneRegistry
	)
	{
		LoadEvent* pSceneRequesters;

		{
			Threading::SharedLock readLock(m_sceneRequesterMutex);
			decltype(m_sceneRequesterMap)::iterator it = m_sceneRequesterMap.Find(identifier);
			if (it != m_sceneRequesterMap.end())
			{
				pSceneRequesters = it->second.Get();
			}
			else
			{
				readLock.Unlock();
				Threading::UniqueLock writeLock(m_sceneRequesterMutex);
				it = m_sceneRequesterMap.Find(identifier);
				if (it != m_sceneRequesterMap.end())
				{
					pSceneRequesters = it->second.Get();
				}
				else
				{
					pSceneRequesters =
						m_sceneRequesterMap.Emplace(ComponentTemplateIdentifier(identifier), UniquePtr<LoadEvent>::Make())->second.Get();
				}
			}
		}

		SceneTemplate& sceneTemplate = GetAssetData(identifier);
		if (m_loadingScenes.Set(identifier))
		{
			m_loadedScenes.Clear(identifier);

			if (sceneTemplate.m_pRootComponent.IsValid())
			{
				sceneTemplate.m_pRootComponent->Destroy(m_templateSceneRegistry);
				sceneTemplate.m_pRootComponent = {};
			}

			Threading::JobBatch jobBatch;
			Component3D::Cloner cloner{jobBatch, m_pTemplateScene->GetRootComponent(), m_templateSceneRegistry, otherSceneRegistry};
			ComponentTypeInterface& componentTypeInterface = *otherComponent.GetTypeInfo();
			const Guid newInstanceGuid = Guid::Generate();
			const Optional<Entity::Component*> pNewComponent = componentTypeInterface.CloneFromTemplateManualOnCreated(
				newInstanceGuid,
				otherComponent,
				otherComponent.GetParent(),
				m_pTemplateScene->GetRootComponent(),
				m_templateSceneRegistry,
				otherSceneRegistry,
				jobBatch
			);
			Assert(pNewComponent.IsValid());
			if (LIKELY(pNewComponent.IsValid()))
			{
				Entity::HierarchyComponentBase& newComponent = static_cast<HierarchyComponentBase&>(*pNewComponent);

				if (const Optional<SceneChildInstance*> pSceneChildInstance = otherComponent.FindDataComponentOfType<SceneChildInstance>(otherSceneRegistry))
				{
					pSceneChildInstance->m_sourceTemplateInstanceGuid = newInstanceGuid;
				}
				else
				{
					otherComponent.CreateDataComponent<SceneChildInstance>(otherSceneRegistry, newInstanceGuid, newInstanceGuid);
				}

				newComponent.RemoveDataComponentOfType<SceneChildInstance>(m_templateSceneRegistry);

				using CloneChild = void (*)(
					HierarchyComponentBase& otherComponent,
					const HierarchyComponentBase& otherParentComponent,
					HierarchyComponentBase& parent,
					Entity::ComponentRegistry& componentRegistry,
					SceneRegistry& sceneRegistry,
					SceneRegistry& otherSceneRegistry,
					Threading::JobBatch& jobBatchOut
				);
				static CloneChild cloneChild = [](
																				 HierarchyComponentBase& otherComponent,
																				 const HierarchyComponentBase& otherParentComponent,
																				 HierarchyComponentBase& parent,
																				 Entity::ComponentRegistry& componentRegistry,
																				 SceneRegistry& sceneRegistry,
																				 SceneRegistry& otherSceneRegistry,
																				 Threading::JobBatch& jobBatchOut
																			 )
				{
					ComponentTypeInterface& typeInterface =
						*componentRegistry.Get(componentRegistry.FindIdentifier(otherComponent.GetTypeGuid(otherSceneRegistry)));
					const Guid newInstanceGuid = Guid::Generate();
					Optional<Entity::Component*> pNewComponent = typeInterface.CloneFromTemplateManualOnCreated(
						newInstanceGuid,
						otherComponent,
						otherParentComponent,
						parent,
						sceneRegistry,
						otherSceneRegistry,
						jobBatchOut,
						Invalid
					);
					if (LIKELY(pNewComponent.IsValid()))
					{
						HierarchyComponentBase& newComponent = static_cast<HierarchyComponentBase&>(*pNewComponent);

						if (const Optional<SceneChildInstance*> pOtherSceneChildInstance = otherComponent.FindDataComponentOfType<SceneChildInstance>(otherSceneRegistry))
						{
							if (const Optional<SceneChildInstance*> pNewSceneChildInstance = newComponent.FindDataComponentOfType<SceneChildInstance>(sceneRegistry))
							{
								const Guid otherInstanceGuid = otherComponent.GetInstanceGuid(otherSceneRegistry);
								if (pOtherSceneChildInstance->m_sourceTemplateInstanceGuid == otherInstanceGuid)
								{
									newComponent.RemoveDataComponentOfType<SceneChildInstance>(sceneRegistry);
								}
								else
								{
									pNewSceneChildInstance->m_sourceTemplateInstanceGuid = pOtherSceneChildInstance->m_sourceTemplateInstanceGuid;
									pNewSceneChildInstance->m_parentTemplateInstanceGuid = pOtherSceneChildInstance->m_parentTemplateInstanceGuid;
									pNewSceneChildInstance->m_flags = pOtherSceneChildInstance->m_flags;
								}
							}

							pOtherSceneChildInstance->m_sourceTemplateInstanceGuid = newInstanceGuid;
						}
						else
						{
							Assert(!newComponent.HasDataComponentOfType<SceneChildInstance>(sceneRegistry));
							otherComponent.CreateDataComponent<SceneChildInstance>(otherSceneRegistry, newInstanceGuid, newInstanceGuid);
						}

						for (HierarchyComponentBase& otherChild : otherComponent.GetChildren())
						{
							cloneChild(otherChild, otherComponent, newComponent, componentRegistry, sceneRegistry, otherSceneRegistry, jobBatchOut);
						}

						typeInterface.OnComponentCreated(*pNewComponent, parent, sceneRegistry);
					}
				};

				Entity::Manager& entityManager = System::Get<Entity::Manager>();
				for (HierarchyComponentBase& otherChild : otherComponent.GetChildren())
				{
					cloneChild(
						otherChild,
						otherComponent,
						newComponent,
						entityManager.GetRegistry(),
						m_templateSceneRegistry,
						otherSceneRegistry,
						jobBatch
					);
				}

				componentTypeInterface.OnComponentCreated(newComponent, m_pTemplateScene->GetRootComponent(), m_templateSceneRegistry);

				sceneTemplate.m_pRootComponent = static_cast<Entity::Component3D&>(newComponent);

				[[maybe_unused]] const bool wasSet = m_loadedScenes.Set(identifier);
				Assert(wasSet);

				[[maybe_unused]] const bool wasCleared = m_loadingScenes.Clear(identifier);
				Assert(wasCleared);

				(*pSceneRequesters)(identifier);
				return true;
			}
			else
			{
				[[maybe_unused]] const bool wasCleared = m_loadingScenes.Clear(identifier);
				Assert(wasCleared);

				(*pSceneRequesters)(identifier);
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	bool ComponentTemplateCache::HasSceneLoaded(const ComponentTemplateIdentifier identifier) const
	{
		return m_loadedScenes.IsSet(identifier);
	}

	void ComponentTemplateCache::Reset()
	{
		m_loadingScenes.Clear(m_loadedScenes);
		for (const ComponentTemplateIdentifier::IndexType identifierIndex : m_loadedScenes.GetSetBitsIterator())
		{
			const ComponentTemplateIdentifier identifier = ComponentTemplateIdentifier::MakeFromValidIndex(identifierIndex);
			SceneTemplate& sceneTemplate = GetAssetData(identifier);
			if (sceneTemplate.m_pRootComponent.IsValid())
			{
				sceneTemplate.m_pRootComponent->Destroy(m_templateSceneRegistry);
			}
			sceneTemplate.m_pRootComponent = {};
			m_loadedScenes.Clear(identifier);
			m_loadingScenes.Clear(identifier);
		}
		for (const ComponentTemplateIdentifier::IndexType identifierIndex : m_loadingScenes.GetSetBitsIterator())
		{
			const ComponentTemplateIdentifier identifier = ComponentTemplateIdentifier::MakeFromValidIndex(identifierIndex);
			SceneTemplate& sceneTemplate = GetAssetData(identifier);
			if (sceneTemplate.m_pRootComponent.IsValid())
			{
				sceneTemplate.m_pRootComponent->Destroy(m_templateSceneRegistry);
			}
			sceneTemplate.m_pRootComponent = {};
			m_loadedScenes.Clear(identifier);
			m_loadingScenes.Clear(identifier);
		}
		m_pTemplateScene->ProcessFullDestroyedComponentsQueue();
	}

	void ComponentTemplateCache::Reset(const ComponentTemplateIdentifier identifier)
	{
		SceneTemplate& sceneTemplate = GetAssetData(identifier);
		if (sceneTemplate.m_pRootComponent.IsValid())
		{
			sceneTemplate.m_pRootComponent->Destroy(m_templateSceneRegistry);
			m_pTemplateScene->ProcessFullDestroyedComponentsQueue();
		}
		sceneTemplate.m_pRootComponent = {};
		m_loadedScenes.Clear(identifier);
		m_loadingScenes.Clear(identifier);
	}

	void ComponentTemplateCache::OnAssetModified(const Asset::Guid, const IdentifierType, [[maybe_unused]] const IO::PathView filePath)
	{
		// TODO: Reload master scene and notify child instances
	}
}
