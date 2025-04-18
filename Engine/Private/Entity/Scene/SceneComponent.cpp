#include "Entity/Scene/SceneComponent.h"
#include "Scene/Scene3DAssetType.h"
#include "Entity/Scene/SceneChildInstance.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include "Engine/Entity/ComponentType.h"
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Serialization/ComponentValue.h>
#include <Engine/Entity/ComponentValue.inl>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Data/ExternalScene.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Scene/Scene3DAssetType.h>

#include <Renderer/Assets/StaticMesh/MeshSceneTag.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>

#include <Common/Asset/Format/Guid.h>
#include <Common/Serialization/Guid.h>
#include <Common/Serialization/MergedReader.h>
#include <Common/Serialization/Serialize.h>
#include <Common/Memory/Containers/Serialization/ArrayView.h>
#include <Common/Memory/Serialization/ReferenceWrapper.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Reflection/Registry.inl>
#include <Common/IO/Log.h>

namespace ngine::Entity
{
	SceneComponent::SceneComponent(const SceneComponent& templateComponent, const Cloner& cloner)
		: Component3D(templateComponent, cloner)
	{
	}

	SceneComponent::SceneComponent(const Deserializer& deserializer)
		: SceneComponent(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<SceneComponent>().ToString().GetView()))
	{
		const Guid typeGuid = deserializer.m_reader.ReadWithDefaultValue<Guid>("typeGuid", Guid());
		if (typeGuid == Reflection::GetTypeGuid<RootSceneComponent>())
		{
			const Scene::Version version = deserializer.m_reader.ReadWithDefaultValue("type_version", Scene::Version::None);
			if (version < Scene::Version::DirectionalGravityComponent)
			{
				constexpr Asset::Guid gravitySceneAssetGuid = "7c40182a-1fc7-4313-a8f9-330f832bd852"_asset;
				Threading::JobBatch batch = Entity::ComponentValue<Entity::Component3D>::DeserializeAsync(
					*this,
					deserializer.GetSceneRegistry(),
					gravitySceneAssetGuid,
					[](const Optional<Entity::Component3D*>, Threading::JobBatch&& loadComponentBatch) mutable
					{
						if (loadComponentBatch.IsValid())
						{
							Threading::JobRunnerThread::GetCurrent()->Queue(loadComponentBatch);
						}
					}
				);
				if (batch.IsValid())
				{
					Threading::JobRunnerThread::GetCurrent()->Queue(batch);
				}
			}
		}
	}

	SceneComponent::SceneComponent(
		Scene3D& scene,
		SceneRegistry& sceneRegistry,
		const Optional<HierarchyComponentBase*> pParent,
		RootSceneComponent& rootSceneComponent,
		const EnumFlags<ComponentFlags> flags,
		const Guid instanceGuid,
		const Math::BoundingBox localBoundingBox,
		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier
	)
		: Component3D(scene, sceneRegistry, pParent, rootSceneComponent, flags, instanceGuid, localBoundingBox)
	{
		CreateDataComponent<Data::ExternalScene>(sceneRegistry, sceneTemplateIdentifier);
	}

	[[nodiscard]] bool IsAssetMeshScene(const Asset::Guid sceneAssetGuid)
	{
		if (sceneAssetGuid.IsValid())
		{
			const Tag::Identifier meshSceneTagIdentifier = System::Get<Tag::Registry>().FindOrRegister(Tags::MeshScene);
			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			const Asset::Identifier assetIdentifier = assetManager.GetAssetIdentifier(sceneAssetGuid);
			if (assetIdentifier.IsValid())
			{
				return assetManager.IsTagSet(meshSceneTagIdentifier, assetIdentifier);
			}
		}
		return false;
	}

	SceneComponent::SceneComponent(const Deserializer& deserializer, const Asset::Guid sceneAssetGuid)
		: Component3D(deserializer | Flags::IsMeshScene * IsAssetMeshScene(sceneAssetGuid))
	{
		Entity::ComponentTemplateIdentifier componentTemplateIdentifier;
		if (sceneAssetGuid.IsValid())
		{
			Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
			componentTemplateIdentifier = sceneTemplateCache.FindOrRegister(sceneAssetGuid);
		}
		CreateDataComponent<Data::ExternalScene>(deserializer.GetSceneRegistry(), componentTemplateIdentifier);
	}

	[[nodiscard]] Asset::Guid GetSceneAssetGuid(const Optional<Serialization::Reader> componentSerializer)
	{
		if (componentSerializer)
		{
			return componentSerializer.Get().ReadWithDefaultValue<Asset::Guid>("scene", Asset::Guid{});
		}
		return {};
	}

	[[nodiscard]] bool IsAssetMeshScene(const ComponentTemplateIdentifier sceneTemplateIdentifier)
	{
		if (sceneTemplateIdentifier.IsValid())
		{
			const Asset::Guid sceneAssetGuid = System::Get<Entity::Manager>().GetComponentTemplateCache().GetAssetGuid(sceneTemplateIdentifier);
			return IsAssetMeshScene(sceneAssetGuid);
		}
		return false;
	}

	SceneComponent::SceneComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer)
		: SceneComponent(deserializer, GetSceneAssetGuid(componentSerializer))
	{
	}

	SceneComponent::SceneComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer | Flags::IsMeshScene * IsAssetMeshScene(initializer.m_sceneTemplateIdentifier)))
	{
		CreateDataComponent<Data::ExternalScene>(initializer.GetSceneRegistry(), initializer.m_sceneTemplateIdentifier);

		if (initializer.m_load)
		{
			Threading::IntermediateStage& finishedStage = Threading::CreateIntermediateStage();

			Threading::JobBatch jobBatch{
				Threading::CreateCallback(
					[this, &finishedStage](Threading::JobRunnerThread& thread)
					{
						Threading::Job* pJob = System::Get<Asset::Manager>().RequestAsyncLoadAssetMetadata(
							GetSceneAsset().GetAssetGuid(),
							Threading::JobPriority::LoadScene,
							[this, &finishedStage](const ConstByteView data)
							{
								if (UNLIKELY(!data.HasElements()))
								{
									LogWarning("Scene load failed: Asset with guid {0} metadata could not be read", GetSceneAsset().GetAssetGuid());
									finishedStage.SignalExecutionFinished(*Threading::JobRunnerThread::GetCurrent());
									return;
								}

								Serialization::RootReader sceneSerializer = Serialization::GetReaderFromBuffer(
									ConstStringView{reinterpret_cast<const char*>(data.GetData()), (uint32)(data.GetDataSize() / sizeof(char))}
								);
								if (LIKELY(sceneSerializer.GetData().IsValid()))
								{
									Threading::JobBatch jobBatch;
									Serialize(sceneSerializer, jobBatch);
									if (jobBatch.IsValid())
									{
										jobBatch.QueueAsNewFinishedStage(finishedStage);
										Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
									}
									else
									{
										LogWarning("Scene load failed: Asset with guid {0} metadata could not be read", GetSceneAsset().GetAssetGuid());
										finishedStage.SignalExecutionFinished(*Threading::JobRunnerThread::GetCurrent());
									}
								}
								else
								{
									LogWarning("Scene load failed: Asset with guid {0} metadata could not be read", GetSceneAsset().GetAssetGuid());
									finishedStage.SignalExecutionFinished(*Threading::JobRunnerThread::GetCurrent());
								}
							}
						);
						if (pJob != nullptr)
						{
							pJob->Queue(thread);
						}
					},
					Threading::JobPriority::LoadScene
				),
				Threading::JobBatch::IntermediateStage
			};

			finishedStage.AddSubsequentStage(jobBatch.GetFinishedStage());
			initializer.m_pJobBatch->QueueAfterStartStage(jobBatch);
		}

		/*if (initializer.m_load)
		{
		    Threading::IntermediateStage& finishedStage = Threading::CreateIntermediateStage();
		    finishedStage.AddSubsequentStage(initializer.m_pJobBatch->GetFinishedStage());

		    Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		    Entity::ComponentSoftReference softReference{*this, initializer.GetSceneRegistry()};
		    Threading::JobBatch masterSceneLoadBatch = sceneTemplateCache.TryLoadScene(
		        initializer.m_sceneTemplateIdentifier,
		        ComponentTemplateCache::LoadListenerData(
		            *this,
		            [sceneTemplateIdentifier = initializer.m_sceneTemplateIdentifier,
		            &finishedStage,
		         &sceneRegistry = initializer.GetSceneRegistry(),
		         softReference](SceneComponent&, const ComponentTemplateIdentifier) mutable
		            {
		                if (const Optional<SceneComponent*> pSceneComponent = softReference.Find<SceneComponent>(sceneRegistry))
		                {
		                    Entity::Manager& entityManager = System::Get<Entity::Manager>();
		                    Entity::ComponentTemplateCache& sceneTemplateCache = entityManager.GetComponentTemplateCache();
		                    Entity::Component3D& templateSceneComponent =
		*sceneTemplateCache.GetAssetData(sceneTemplateIdentifier).m_pRootComponent;

		                    Entity::SceneRegistry& templateSceneRegistry = sceneTemplateCache.GetTemplateSceneRegistry();
		                    Threading::JobBatch jobBatch;
		                    templateSceneComponent.IterateDataComponents(
		                        templateSceneRegistry,
		                        [&sceneRegistry,
		                       &templateSceneRegistry,
		                       &targetComponent = *pSceneComponent,
		                       &templateSceneComponent,
		                       &jobBatch](Entity::Data::Component& templateDataComponent, ComponentTypeInterface& componentTypeInfo,
		ComponentTypeSceneDataInterface&)
		                        {
		                            Entity::ComponentValue<Entity::Data::Component> componentValue;
		                            Threading::JobBatch dataComponentBatch;
		                            componentValue.CloneFromTemplate(
		                                sceneRegistry,
		                                templateSceneRegistry,
		                                componentTypeInfo,
		                                templateDataComponent,
		                                templateSceneComponent,
		                                targetComponent,
		                                dataComponentBatch
		                            );
		                            if (dataComponentBatch.IsValid())
		                            {
		                                jobBatch.QueueAfterStartStage(dataComponentBatch);
		                            }
		                            return Memory::CallbackResult::Continue;
		                        }
		                    );

		                    for (Entity::Component3D& templateChild : templateSceneComponent.GetChildren())
		                    {
		                        Threading::JobBatch childJobBatch;
		                        pSceneComponent->GetTypeInfo()->CloneFromTemplateWithChildren(Guid::Generate(), templateChild,
		templateSceneComponent, *pSceneComponent, entityManager.GetRegistry(), sceneRegistry, templateSceneRegistry, childJobBatch);

		                        jobBatch.QueueAfterStartStage(childJobBatch);
		                    }

		                    if (jobBatch.IsValid())
		                    {
		                        jobBatch.QueueAsNewFinishedStage(finishedStage);
		                        jobBatch.GetStartJob()->Queue(*Threading::JobRunnerThread::GetCurrent());
		                    }
		                    else
		                    {
		                        finishedStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
		                    }
		                }
		                return EventCallbackResult::Remove;
		            }
		        )
		    );
		    if (masterSceneLoadBatch.IsValid())
		    {
		        initializer.m_pJobBatch->QueueAfterStartStage(masterSceneLoadBatch);
		    };
		}*/
	}

	SceneComponent::~SceneComponent()
	{
		Threading::UniqueLock lock(m_loadSceneMutex);
	}

	void SceneComponent::SetSceneTemplateIdentifier(const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier)
	{
		if (const Optional<Data::ExternalScene*> pExternalScene = FindDataComponentOfType<Data::ExternalScene>(GetSceneRegistry()))
		{
			pExternalScene->SetTemplateIdentifier(sceneTemplateIdentifier);
		}
	}

	Entity::ComponentTemplateIdentifier SceneComponent::GetSceneTemplateIdentifier() const
	{
		if (const Optional<Data::ExternalScene*> pExternalScene = FindDataComponentOfType<Data::ExternalScene>(GetSceneRegistry()))
		{
			return pExternalScene->GetTemplateIdentifier();
		}
		return {};
	}

	Asset::Guid SceneComponent::GetAssetGuid() const
	{
		if (const Optional<Data::ExternalScene*> pExternalScene = FindDataComponentOfType<Data::ExternalScene>(GetSceneRegistry()))
		{
			return pExternalScene->GetAssetGuid();
		}
		return {};
	}

	void SceneComponent::OnDisable()
	{
		Threading::UniqueLock lock(m_loadSceneMutex);
	}

	void SceneComponent::SpawnAssetChildren(const Optional<const Entity::Component3D*> templateSceneComponent)
	{
		Threading::UniqueLock lock(m_loadSceneMutex);
		const Optional<Data::ExternalScene*> pExternalScene = FindDataComponentOfType<Data::ExternalScene>(GetSceneRegistry());
		if (Ensure(pExternalScene.IsValid()))
		{
			pExternalScene->SpawnAssetChildren(*this, templateSceneComponent);
		}
	}

	Threading::JobBatch SceneComponent::DeserializeDataComponentsAndChildren(Serialization::Reader serializer)
	{
		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = GetSceneTemplateIdentifier();
		if (!sceneTemplateIdentifier.IsValid() || serializer.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UndoHistory))
		{
			return Component3D::DeserializeDataComponentsAndChildren(serializer);
		}

		Serialization::Document copiedDocument;
		copiedDocument.CopyFrom(serializer.GetValue().GetValue(), copiedDocument.GetAllocator());

		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Threading::JobBatch jobBatch{
			Threading::JobBatch::IntermediateStage,
			Threading::CreateCallback(
				[componentReference = Entity::ComponentSoftReference{*this, sceneRegistry}, &sceneRegistry](Threading::JobRunnerThread&)
				{
					const Optional<SceneComponent*> pSceneComponent = componentReference.Find<SceneComponent>(sceneRegistry);
					if (pSceneComponent.IsValid() && !pSceneComponent->IsDestroying(sceneRegistry))
					{
						Threading::UniqueLock lock(pSceneComponent->m_loadSceneMutex);
						if (pSceneComponent->IsDestroying(sceneRegistry))
						{
							return;
						}

						const Math::WorldBoundingBox newBoundingBox = pSceneComponent->GetChildBoundingBox(sceneRegistry);
						pSceneComponent->SetBoundingBox(newBoundingBox.IsZero() ? Math::WorldBoundingBox(0.5_meters) : newBoundingBox, sceneRegistry);
					}
				},
				Threading::JobPriority::LoadScene
			)
		};

		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();

		Threading::DynamicIntermediateStage& finishedStage = Threading::CreateIntermediateStage();
		finishedStage.AddSubsequentStage(jobBatch.GetFinishedStage());

		Entity::ComponentSoftReference softReference{*this, sceneRegistry};
		Threading::JobBatch masterSceneLoadBatch = sceneTemplateCache.TryLoadScene(
			sceneTemplateIdentifier,
			ComponentTemplateCache::LoadListenerData(
				*this,
				[&templateSceneComponent = sceneTemplateCache.GetAssetData(sceneTemplateIdentifier).m_pRootComponent,
		     copiedDocument = Move(copiedDocument),
		     &finishedStage,
		     &sceneRegistry,
		     softReference](SceneComponent&, const ComponentTemplateIdentifier) mutable
				{
					if (const Optional<SceneComponent*> pSceneComponent = softReference.Find<SceneComponent>(sceneRegistry))
					{
						Serialization::Data serializedData = Serialization::Data(Move(copiedDocument));

						Threading::UniqueLock lock(pSceneComponent->m_loadSceneMutex);
						const Optional<Data::ExternalScene*> pExternalScene =
							pSceneComponent->FindDataComponentOfType<Data::ExternalScene>(sceneRegistry);
						if (Ensure(pExternalScene.IsValid()))
						{
							pExternalScene->OnMasterSceneLoaded(*pSceneComponent, templateSceneComponent, Move(serializedData), finishedStage);
						}
					}
					return EventCallbackResult::Remove;
				}
			)
		);
		if (masterSceneLoadBatch.IsValid())
		{
			jobBatch.QueueAfterStartStage(masterSceneLoadBatch);
		}

		return jobBatch;
	}

	bool SceneComponent::SerializeDataComponentsAndChildren(Serialization::Writer writer) const
	{
		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = GetSceneTemplateIdentifier();
		if (!sceneTemplateIdentifier.IsValid() || writer.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UndoHistory))
		{
			return Component3D::SerializeDataComponentsAndChildren(writer);
		}

		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		const Optional<const Entity::Component3D*> pTemplateComponent =
			sceneTemplateCache.GetAssetData(sceneTemplateIdentifier).m_pRootComponent;
		if (UNLIKELY(!pTemplateComponent.IsValid() || pTemplateComponent == this))
		{
			return Component3D::SerializeDataComponentsAndChildren(writer);
		}

		const Entity::Component3D& templateComponent = *pTemplateComponent;

		const Optional<Data::ExternalScene*> pExternalScene = FindDataComponentOfType<Data::ExternalScene>(GetSceneRegistry());
		if (Ensure(pExternalScene.IsValid()))
		{
			[[maybe_unused]] const bool wasSerialized = pExternalScene->SerializeTemplateComponents(writer, *this, templateComponent);
			Assert(wasSerialized);
		}

		Serialization::TValue& typeProperties = writer.GetAsObject().FindOrCreateMember(
			Reflection::GetTypeGuid<SceneComponent>().ToString().GetView(),
			Serialization::Object(),
			writer.GetDocument()
		);

		Serialization::Writer typeWriter(typeProperties, writer.GetData());
		typeWriter.Serialize("scene", GetSceneAsset().GetAssetGuid());

		return true;
	}

	void SceneComponent::SetSceneAsset(const ScenePicker asset)
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		const ComponentTemplateIdentifier newSceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(asset.GetAssetGuid());
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

		const bool wasMeshScene = IsMeshScene();
		const bool isMeshScene = IsAssetMeshScene(newSceneTemplateIdentifier);
		if (isMeshScene != wasMeshScene)
		{
			ToggleIsMeshScene(sceneRegistry);
		}

		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = GetSceneTemplateIdentifier();
		if (sceneTemplateIdentifier.IsValid())
		{
			// Find and remove any components from the previous scene asset
			if (const Optional<const Component3D*> pPreviousComponentTemplate = sceneTemplateCache.GetAssetData(sceneTemplateIdentifier).m_pRootComponent)
			{
				for (const Entity::Component3D& previousComponentTemplateChild : pPreviousComponentTemplate->GetChildren())
				{
					const Guid templateInstanceGuid = previousComponentTemplateChild.GetInstanceGuid(sceneTemplateCache.GetTemplateSceneRegistry());

					const auto findChild =
						[this](const Guid templateInstanceGuid, Entity::SceneRegistry& sceneRegistry) -> Optional<Entity::Component3D*>
					{
						for (Entity::Component3D& child : GetChildren())
						{
							if (const Optional<SceneChildInstance*> pSceneChildInstance = child.FindDataComponentOfType<SceneChildInstance>(sceneRegistry))
							{
								if (pSceneChildInstance->m_parentTemplateInstanceGuid == templateInstanceGuid)
								{
									return child;
								}
							}
						}
						return Invalid;
					};

					if (const Optional<Entity::Component3D*> pChildComponent = findChild(templateInstanceGuid, sceneRegistry))
					{
						pChildComponent->Destroy(sceneRegistry);
					}
				}
			}
		}

		SetSceneTemplateIdentifier(newSceneTemplateIdentifier);

		Entity::ComponentSoftReference softReference{*this, sceneRegistry};
		Threading::JobBatch masterSceneLoadBatch = sceneTemplateCache.TryLoadScene(
			newSceneTemplateIdentifier,
			ComponentTemplateCache::LoadListenerData(
				*this,
				[&templateSceneComponent = sceneTemplateCache.GetAssetData(newSceneTemplateIdentifier).m_pRootComponent,
		     &sceneRegistry,
		     softReference](SceneComponent&, const ComponentTemplateIdentifier) mutable
				{
					if (const Optional<SceneComponent*> pSceneComponent = softReference.Find<SceneComponent>(sceneRegistry))
					{
						pSceneComponent->SpawnAssetChildren(templateSceneComponent);
					}
					return EventCallbackResult::Remove;
				}
			)
		);
		if (masterSceneLoadBatch.IsValid())
		{
			if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
			{
				pThread->Queue(masterSceneLoadBatch);
			}
			else
			{
				System::Get<Threading::JobManager>().Queue(masterSceneLoadBatch, Threading::JobPriority::LoadScene);
			}
		}
	}

	void SceneComponent::SetSceneGuid(const Asset::Guid assetGuid)
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		const ComponentTemplateIdentifier newSceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(assetGuid);
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

		const bool wasMeshScene = GetFlags(sceneRegistry).IsSet(ComponentFlags::IsMeshScene);
		const bool isMeshScene = IsAssetMeshScene(newSceneTemplateIdentifier);
		if (isMeshScene != wasMeshScene)
		{
			ToggleIsMeshScene(sceneRegistry);
		}

		SetSceneTemplateIdentifier(newSceneTemplateIdentifier);
	}

	Threading::JobBatch SceneComponent::SetDeseralizedSceneAsset(
		const ScenePicker asset,
		[[maybe_unused]] const Serialization::Reader objectReader,
		[[maybe_unused]] const Serialization::Reader typeReader
	)
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		const ComponentTemplateIdentifier newSceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(asset.GetAssetGuid());
		SetSceneTemplateIdentifier(newSceneTemplateIdentifier);
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

		const bool wasMeshScene = GetFlags(sceneRegistry).IsSet(ComponentFlags::IsMeshScene);
		const bool isMeshScene = IsAssetMeshScene(newSceneTemplateIdentifier);
		if (isMeshScene != wasMeshScene)
		{
			ToggleIsMeshScene(sceneRegistry);
		}
		return {};
	}

	SceneComponent::ScenePicker SceneComponent::GetSceneAsset() const
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = GetSceneTemplateIdentifier();
		return {
			Asset::Reference{
				sceneTemplateIdentifier.IsValid() ? sceneTemplateCache.GetAssetGuid(sceneTemplateIdentifier) : Asset::Guid{},
				Scene3DAssetType::AssetFormat.assetTypeGuid
			},
			Asset::Types{Array<Asset::TypeGuid, 2>{Scene3DAssetType::AssetFormat.assetTypeGuid, MeshSceneAssetType::AssetFormat.assetTypeGuid}}
		};
	}

	bool SceneComponent::CanApplyAtPoint(
		const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyAssetFlags
	) const
	{
		if (applyAssetFlags.IsSet(Entity::ApplyAssetFlags::Deep))
		{
			if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
			{
				return pAssetReference->GetTypeGuid() == Scene3DAssetType::AssetFormat.assetTypeGuid ||
				       pAssetReference->GetTypeGuid() == MeshSceneAssetType::AssetFormat.assetTypeGuid;
			}
		}
		return false;
	}

	bool SceneComponent::ApplyAtPoint(
		const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyAssetFlags
	)
	{
		if (applyAssetFlags.IsSet(Entity::ApplyAssetFlags::Deep))
		{
			if (const Optional<const Asset::Reference*> pAssetReference = applicableData.Get<Asset::Reference>())
			{
				if (pAssetReference->GetTypeGuid() == Scene3DAssetType::AssetFormat.assetTypeGuid || pAssetReference->GetTypeGuid() == MeshSceneAssetType::AssetFormat.assetTypeGuid)
				{
					SetSceneAsset(*pAssetReference);
				}
			}
		}
		return true;
	}

	void SceneComponent::IterateAttachedItems(
		const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>& callback
	)
	{
		if (!allowedTypes.Contains(Reflection::TypeDefinition::Get<Asset::Reference>()))
		{
			return;
		}

		Asset::Picker scene = GetSceneAsset();
		if (callback(scene.m_asset) == Memory::CallbackResult::Break)
		{
			return;
		}
	}

	[[maybe_unused]] const bool wasSceneRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<SceneComponent>>::Make());
	[[maybe_unused]] const bool wasSceneTypeRegistered = Reflection::Registry::RegisterType<SceneComponent>();
	[[maybe_unused]] const bool wasSceneChildTypeRegistered = Reflection::Registry::RegisterType<Entity::SceneChildInstance>();
	[[maybe_unused]] const bool wasSceneChildRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Entity::SceneChildInstance>>::Make());
}
