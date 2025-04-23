#include "Spawning.h"

#include "Components/Creator.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/Component3D.inl>

#include <Common/Asset/Guid.h>
#include <Common/System/Query.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Threading/Jobs/AsyncJob.h>

namespace ngine::GameFramework
{
	bool SpawnAsset(const Asset::Guid assetGuid, SpawnInitializer&& initializer, SpawnAssetCallback&& callback)
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();

		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(assetGuid);
		Threading::JobBatch loadBatch = sceneTemplateCache.TryLoadScene(
			sceneTemplateIdentifier,
			Entity::ComponentTemplateCache::LoadListenerData(
				initializer.m_pCreator,
				[initializer = Forward<SpawnInitializer>(initializer),
		     callback = Forward<SpawnAssetCallback>(callback
		     )](const Optional<Entity::SceneComponent*>, const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier) mutable
				{
					Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
					const Optional<const Entity::Component3D*> pTemplateComponent =
						sceneTemplateCache.GetAssetData(sceneTemplateIdentifier).m_pRootComponent;
					Assert(pTemplateComponent.IsValid());
					if (LIKELY(pTemplateComponent.IsValid()))
					{
						Entity::ComponentRegistry& componentRegistry = System::Get<Entity::Manager>().GetRegistry();
						Entity::SceneRegistry& sceneRegistry = initializer.m_rootScene.GetEntitySceneRegistry();
						const Entity::ComponentTypeIdentifier componentTypeIdentifier =
							componentRegistry.FindIdentifier(pTemplateComponent->GetTypeInfo()->GetTypeInterface().GetGuid());

						Optional<Entity::ComponentTypeInterface*> pComponentTypeInterface = componentRegistry.Get(componentTypeIdentifier);

						Threading::JobBatch cloningJobBatch;
						Optional<Entity::Component*> pComponent = pComponentTypeInterface->CloneFromTemplateWithChildren(
							Guid::Generate(),
							*pTemplateComponent,
							pTemplateComponent->GetParent(),
							initializer.m_rootScene.GetRootComponent(),
							componentRegistry,
							sceneRegistry,
							pTemplateComponent->GetSceneRegistry(),
							cloningJobBatch
						);
						Assert(pComponent.IsValid());
						if (LIKELY(pComponent.IsValid()))
						{
							Entity::Component3D& component = static_cast<Entity::Component3D&>(*pComponent);

							const Math::WorldTransform newWorldTransform = initializer.m_worldTransform;
							if (!newWorldTransform.IsEquivalentTo(component.GetWorldTransform()))
							{
								component.SetWorldTransform(newWorldTransform);
							}
							if (cloningJobBatch.IsValid())
							{
								cloningJobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
									[&component, &sceneRegistry, callback = Move(callback), pCreator = initializer.m_pCreator](Threading::JobRunnerThread&)
									{
										if (pCreator.IsValid())
										{
											component.CreateDataComponent<Creator>(
												Creator::Initializer{Entity::Data::Component::DynamicInitializer{component, sceneRegistry}, *pCreator}
											);
										}

										callback(component);
									},
									Threading::JobPriority::InteractivityLogic
								));

								Threading::JobRunnerThread::GetCurrent()->Queue(cloningJobBatch);
							}
							else
							{
								if (initializer.m_pCreator.IsValid())
								{
									component.CreateDataComponent<Creator>(
										Creator::Initializer{Entity::Data::Component::DynamicInitializer{component, sceneRegistry}, *initializer.m_pCreator}
									);
								}

								callback(component);
							}
						}
						else
						{
							callback(Invalid);
						}
					}
					else
					{
						callback(Invalid);
					}
					return EventCallbackResult::Remove;
				}
			)
		);
		if (loadBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(loadBatch);
		}
		return true;
	}
}
