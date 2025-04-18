#pragma once

#include "ComponentValue.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Manager.h>
#include <Engine/Entity/Scene/ComponentTemplateCache.h>
#include <Engine/Entity/ComponentTypeInterface.h>
#include <Engine/Entity/Component3D.h>

#include <Common/Threading/Jobs/JobBatch.h>

namespace ngine::Entity
{
	template<typename ComponentType>
	template<typename... Args, typename ComponentType_, typename>
	inline ComponentValue<ComponentType>::ComponentValue(ComponentTypeSceneData<ComponentType>& typeSceneData, Args&&... args)
		: m_pComponent(typeSceneData.CreateInstance(Forward<Args>(args)...))
	{
	}

	template<typename ComponentType>
	template<typename... Args, typename ComponentType_, typename>
	inline ComponentValue<ComponentType>::ComponentValue(
		const Optional<ComponentTypeSceneData<ComponentType>*> pTypeSceneData, Args&&... args
	)
		: m_pComponent(pTypeSceneData.IsValid() ? pTypeSceneData->CreateInstance(Forward<Args>(args)...) : nullptr)
	{
	}

	template<typename ComponentType>
	template<typename... Args>
	inline ComponentValue<ComponentType>::ComponentValue(SceneRegistry& sceneRegistry, Args&&... args)
		: ComponentValue(sceneRegistry.GetOrCreateComponentTypeData<ComponentType>(), Forward<Args>(args)...)
	{
	}

	template<typename ComponentType>
	inline ComponentValue<ComponentType>::ComponentValue(
		ComponentTypeSceneDataInterface& typeSceneDataInterface, typename ComponentType::DynamicInitializer&& initializer
	)
		: m_pComponent(static_cast<ComponentType*>(
				typeSceneDataInterface.CreateInstanceDynamic(Any(Forward<ComponentType::DynamicInitializer>(initializer))).Get()
			))
	{
	}

	template<typename ComponentType>
	inline ComponentValue<ComponentType>::ComponentValue(
		SceneRegistry& sceneRegistry, const ComponentTypeIdentifier typeIdentifier, typename ComponentType::DynamicInitializer&& initializer
	)
		: ComponentValue(
				sceneRegistry.GetOrCreateComponentTypeData(typeIdentifier), Forward<typename ComponentType::DynamicInitializer>(initializer)
			)
	{
	}

	template<typename ComponentType>
	inline ComponentValue<ComponentType>::ComponentValue(
		const Optional<ComponentTypeSceneDataInterface*> pTypeSceneDataInterface, typename ComponentType::DynamicInitializer&& initializer
	)
		: m_pComponent(
				pTypeSceneDataInterface.IsValid()
					? static_cast<ComponentType*>(
							pTypeSceneDataInterface->CreateInstanceDynamic(Any(Forward<typename ComponentType::DynamicInitializer>(initializer))).Get()
						)
					: nullptr
			)
	{
	}

	template<typename ComponentType>
	/* static */ Threading::JobBatch ComponentValue<ComponentType>::DeserializeAsync(
		ParentType& parent,
		Entity::SceneRegistry& sceneRegistry,
		const Asset::Guid assetGuid,
		DeserializedCallback&& callback,
		const Guid instanceGuid
	)
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		static Threading::Atomic<uint64> counter{0};
		return sceneTemplateCache.TryLoadScene(
			sceneTemplateCache.FindOrRegister(assetGuid),
			Entity::ComponentTemplateCache::LoadListenerData(
				reinterpret_cast<void*>(counter++),
				[&sceneTemplateCache, &parent, &sceneRegistry, callback = Forward<DeserializedCallback>(callback), instanceGuid](
					void*,
					const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier
				) mutable
				{
					Optional<const Entity::Component3D*> pTemplateComponent =
						sceneTemplateCache.GetAssetData(sceneTemplateIdentifier).m_pRootComponent;
					if (pTemplateComponent.IsInvalid())
					{
						callback(nullptr, {});
						return EventCallbackResult::Remove;
					}

					if (const Optional<Entity::ComponentTypeInterface*> pComponentTypeInterface = pTemplateComponent->GetTypeInfo())
					{
						Threading::JobBatch jobBatch;
						Optional<Entity::Component*> pComponent = pComponentTypeInterface->CloneFromTemplateWithChildren(
							instanceGuid,
							*pTemplateComponent,
							pTemplateComponent->GetParent(),
							parent,
							System::Get<Entity::Manager>().GetRegistry(),
							sceneRegistry,
							pTemplateComponent->GetSceneRegistry(),
							jobBatch
						);
						if (pComponent.IsValid())
						{
							callback(static_cast<ComponentType&>(*pComponent), Move(jobBatch));
						}
						else
						{
							callback(nullptr, {});
						}
					}
					else
					{
						callback(nullptr, {});
					}
					return EventCallbackResult::Remove;
				}
			)
		);
	}
}
