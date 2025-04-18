#include "Context/Utils.h"
#include "Context/Context.h"
#include "Context/Reference.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/HierarchyComponent.inl>
#include <Engine/Entity/Data/Component.inl>
#include <Engine/Entity/Data/ParentComponent.h>

namespace ngine::Context
{
	Guid Utils::GetGuid(Guid globalGuid, Entity::ComponentIdentifier componentIdentifier, Entity::SceneRegistry& sceneRegistry)
	{
		Entity::ComponentTypeSceneData<Data::Component>& contextComponentSceneData = sceneRegistry.GetCachedSceneData<Data::Component>();
		Entity::ComponentTypeSceneData<Data::Reference>& referenceComponentSceneData = sceneRegistry.GetCachedSceneData<Data::Reference>();
		Entity::ComponentTypeSceneData<Entity::Data::Parent>& parentComponentSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Parent>(
		);

		const Entity::ComponentIdentifier initialComponentIdentifier = componentIdentifier;
		// Iterate in two steps. First do a pure search for an exact match
		while (componentIdentifier.IsValid())
		{
			if (Optional<Data::Component*> pContext = contextComponentSceneData.GetComponentImplementation(componentIdentifier))
			{
				const Guid guid = pContext->FindGuid(globalGuid);
				if (guid.IsValid())
				{
					return guid;
				}
			}

			if (Optional<Data::Reference*> pContextReference = referenceComponentSceneData.GetComponentImplementation(componentIdentifier))
			{
				const Guid guid = GetGuid(globalGuid, pContextReference->GetReferencedComponentIdentifier(), sceneRegistry);
				if (guid.IsValid())
				{
					return guid;
				}
			}

			if (const Optional<const Entity::Data::Parent*> pParent = parentComponentSceneData.GetComponentImplementation(componentIdentifier))
			{
				componentIdentifier = Entity::ComponentIdentifier::MakeFromValidIndex(pParent->Get());
			}
			else
			{
				break;
			}
		}

		// Second step: Emplace the guid if an exact match wasn't found
		componentIdentifier = initialComponentIdentifier;
		while (componentIdentifier.IsValid())
		{
			if (Optional<Data::Component*> pContext = contextComponentSceneData.GetComponentImplementation(componentIdentifier))
			{
				return pContext->GetOrEmplaceGuid(globalGuid);
			}

			if (Optional<Data::Reference*> pContextReference = referenceComponentSceneData.GetComponentImplementation(componentIdentifier))
			{
				if (const Optional<Data::Component*> pContext = contextComponentSceneData.GetComponentImplementation(pContextReference->GetReferencedComponentIdentifier()))
				{
					return pContext->GetOrEmplaceGuid(globalGuid);
				}
			}

			if (const Optional<const Entity::Data::Parent*> pParent = parentComponentSceneData.GetComponentImplementation(componentIdentifier))
			{
				componentIdentifier = Entity::ComponentIdentifier::MakeFromValidIndex(pParent->Get());
			}
			else
			{
				break;
			}
		}

		return globalGuid;
	}

	Guid Utils::GetGuid(Guid globalGuid, Entity::HierarchyComponentBase& component, Entity::SceneRegistry& sceneRegistry)
	{
		return GetGuid(globalGuid, component.GetIdentifier(), sceneRegistry);
	}

	Entity::DataComponentResult<Data::Component>
	Utils::FindContext(Entity::HierarchyComponentBase& component, Entity::SceneRegistry& sceneRegistry)
	{
		Entity::ComponentTypeSceneData<Data::Component>& contextComponentSceneData = sceneRegistry.GetCachedSceneData<Data::Component>();
		Entity::ComponentTypeSceneData<Data::Reference>& referenceComponentSceneData = sceneRegistry.GetCachedSceneData<Data::Reference>();

		Optional<Entity::HierarchyComponentBase*> pComponent = &component;
		while (pComponent.IsValid())
		{
			if (Optional<Data::Component*> pContext = pComponent->FindDataComponentOfType<Data::Component>(contextComponentSceneData))
			{
				return Entity::DataComponentResult<Data::Component>{pContext, &static_cast<Data::Component::ParentType&>(*pComponent)};
			}

			if (Optional<Data::Reference*> pContextReference = pComponent->FindDataComponentOfType<Data::Reference>(referenceComponentSceneData))
			{
				if (const Optional<Data::Component*> pContext = pContextReference->GetComponent(sceneRegistry))
				{
					return Entity::DataComponentResult<Data::Component>{pContext, &static_cast<Data::Component::ParentType&>(*pComponent)};
				}
			}

			pComponent = pComponent->GetParentSafe();
		}

		return Entity::DataComponentResult<Data::Component>{};
	}
}
