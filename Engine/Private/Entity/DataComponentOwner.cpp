#include "Entity/DataComponentOwner.h"

#include <Engine/Entity/ComponentTypeSceneDataInterface.h>
#include <Engine/Entity/ComponentTypeInterface.h>
#include <Engine/Entity/Data/Component.inl>

namespace ngine::Entity
{
	bool
	DataComponentOwner::RemoveDataComponentOfType(SceneRegistry& sceneRegistry, const ComponentTypeIdentifier dataComponentTypeIdentifier)
	{
		if (const Optional<ComponentTypeSceneDataInterface*> pComponentTypeSceneData = sceneRegistry.FindComponentTypeData(dataComponentTypeIdentifier))
		{
			const ComponentIdentifier identifier = GetIdentifier();
			if (sceneRegistry.OnDataComponentRemoved(identifier, dataComponentTypeIdentifier))
			{
				pComponentTypeSceneData->OnBeforeRemoveInstance(pComponentTypeSceneData->GetDataComponentUnsafe(identifier), *this);
				pComponentTypeSceneData->RemoveInstance(pComponentTypeSceneData->GetDataComponentUnsafe(identifier), *this);
				return true;
			}
		}
		return false;
	}

	Optional<Data::Component*>
	DataComponentOwner::FindFirstDataComponentImplementingType(const SceneRegistry& sceneRegistry, const Guid typeGuid) const
	{
		Optional<Data::Component*> component;
		IterateDataComponentsImplementingType(
			sceneRegistry,
			typeGuid,
			[&component,
		   typeGuid](Data::Component& dataComponent, const Entity::ComponentTypeInterface& componentTypeInfo, ComponentTypeSceneDataInterface&)
			{
				if (componentTypeInfo.GetTypeInterface().Implements(typeGuid))
				{
					component = &dataComponent;
					return Memory::CallbackResult::Break;
				}

				return Memory::CallbackResult::Continue;
			}
		);

		return component;
	}

	Optional<Data::Component*>
	DataComponentOwner::FindDataComponentOfType(const SceneRegistry& sceneRegistry, const Guid dataComponentTypeGuid) const
	{
		const ComponentIdentifier componentIdentifier = GetIdentifier();
		const ComponentTypeIdentifier dataComponentTypeIdentifier = sceneRegistry.FindComponentTypeIdentifier(dataComponentTypeGuid);
		if (sceneRegistry.HasDataComponentOfType(componentIdentifier, dataComponentTypeIdentifier))
		{
			if (Optional<ComponentTypeSceneDataInterface*> pComponentTypeInterface = sceneRegistry.FindComponentTypeData(dataComponentTypeIdentifier))
			{
				if (Optional<Component*> pComponent = pComponentTypeInterface->GetDataComponent(componentIdentifier))
				{
					return static_cast<Data::Component*>(pComponent.Get());
				}
			}
		}

		return Invalid;
	}

	bool DataComponentOwner::HasDataComponentOfType(const SceneRegistry& sceneRegistry, const Guid dataComponentTypeGuid) const
	{
		const ComponentIdentifier componentIdentifier = GetIdentifier();
		const ComponentTypeIdentifier dataComponentTypeIdentifier = sceneRegistry.FindComponentTypeIdentifier(dataComponentTypeGuid);
		return sceneRegistry.HasDataComponentOfType(componentIdentifier, dataComponentTypeIdentifier);
	}

	bool DataComponentOwner::HasDataComponentOfType(
		const SceneRegistry& sceneRegistry, const ComponentTypeIdentifier dataComponentTypeIdentifier
	) const
	{
		return sceneRegistry.HasDataComponentOfType(m_identifier, dataComponentTypeIdentifier);
	}

	bool DataComponentOwner::HasAnyDataComponents(const SceneRegistry& sceneRegistry) const
	{
		return sceneRegistry.HasDataComponents(m_identifier);
	}

	bool DataComponentOwner::HasAnyDataComponentsImplementingType(const SceneRegistry& sceneRegistry, const Guid typeGuid) const
	{
		bool found = false;
		IterateDataComponents(
			sceneRegistry,
			[&found,
		   typeGuid](Data::Component&, const Optional<const Entity::ComponentTypeInterface*> pComponentTypeInfo, ComponentTypeSceneDataInterface&)
			{
				if (pComponentTypeInfo.IsValid())
				{
					if (pComponentTypeInfo->GetTypeInterface().Implements(typeGuid))
					{
						found = true;
						return Memory::CallbackResult::Break;
					}
				}

				return Memory::CallbackResult::Continue;
			}
		);

		return found;
	}
}
