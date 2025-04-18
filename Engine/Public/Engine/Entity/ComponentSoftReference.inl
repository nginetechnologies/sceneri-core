#pragma once

#include "ComponentReference.h"

#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Entity/ComponentSoftReference.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/HierarchyComponentBase.h>
#include <Engine/Entity/Data/InstanceGuid.h>

namespace ngine::Entity
{
	namespace Internal
	{
		template<typename ComponentType>
		[[nodiscard]] static ComponentSoftReference::Instance GetSoftReferenceInstance(
			const ComponentType& component, const ComponentTypeIdentifier typeIdentifier, const SceneRegistry& sceneRegistry
		)
		{
			const Optional<ComponentTypeSceneDataInterface*> pTypeSceneDataInterface = sceneRegistry.FindComponentTypeData(typeIdentifier);
			Assert(pTypeSceneDataInterface.IsValid());
			if (LIKELY(pTypeSceneDataInterface.IsValid()))
			{
				GenericComponentInstanceIdentifier instanceIdentifier;
				if constexpr (TypeTraits::IsFinal<ComponentType>)
				{
					instanceIdentifier =
						static_cast<ComponentTypeSceneData<ComponentType>&>(*pTypeSceneDataInterface).GetComponentInstanceIdentifier(component);
				}
				else
				{
					instanceIdentifier = pTypeSceneDataInterface->GetComponentInstanceIdentifier(component);
				}

				if constexpr (TypeTraits::IsBaseOf<DataComponentOwner, ComponentType>)
				{
					const ComponentIdentifier componentIdentifier = component.GetIdentifier();

					ComponentTypeSceneData<Data::InstanceGuid>& instanceGuidSceneData = sceneRegistry.GetCachedSceneData<Data::InstanceGuid>();
					const Optional<Data::InstanceGuid*> pInstanceGuid = instanceGuidSceneData.GetComponentImplementation(componentIdentifier);
					return {instanceIdentifier, pInstanceGuid.IsValid() ? Guid(*pInstanceGuid) : Guid()};
				}
				else
				{
					return {instanceIdentifier};
				}
			}
			else
			{
				return {};
			}
		}
	}

	template<typename ComponentType>
	inline ComponentSoftReference::ComponentSoftReference(
		const ComponentTypeIdentifier typeIdentifier, const ComponentType& component, const SceneRegistry& sceneRegistry
	)
		: m_typeIdentifier(typeIdentifier)
		, m_instance(Internal::GetSoftReferenceInstance(component, typeIdentifier, sceneRegistry))
	{
	}

	template<typename ComponentType>
	inline ComponentSoftReference::ComponentSoftReference(const ComponentType& component, const SceneRegistry& sceneRegistry)
		: ComponentSoftReference(component.GetTypeIdentifier(sceneRegistry), component, sceneRegistry)
	{
		static_assert(TypeTraits::IsBaseOf<Entity::HierarchyComponentBase, ComponentType>);
	}

	template<typename ComponentType>
	inline ComponentSoftReference::ComponentSoftReference(
		const ComponentTypeIdentifier typeIdentifier, const Optional<ComponentType*> pComponent, const SceneRegistry& sceneRegistry
	)
		: m_typeIdentifier(typeIdentifier)
		, m_instance(pComponent.IsValid() ? Internal::GetSoftReferenceInstance(*pComponent, typeIdentifier, sceneRegistry) : Instance{})
	{
		static_assert(TypeTraits::IsBaseOf<Entity::DataComponentOwner, ComponentType>);
	}

	template<typename ComponentType>
	inline ComponentSoftReference::ComponentSoftReference(const Optional<ComponentType*> pComponent, const SceneRegistry& sceneRegistry)
		: ComponentSoftReference(
				pComponent.IsValid() ? pComponent->GetTypeIdentifier(sceneRegistry) : ComponentTypeIdentifier{}, pComponent, sceneRegistry
			)
	{
		static_assert(TypeTraits::IsBaseOf<Entity::DataComponentOwner, ComponentType>);
	}

	template<typename ComponentType>
	inline Optional<ComponentType*> ComponentSoftReference::Find(const SceneRegistry& sceneRegistry) const
	{
		const Optional<Component*> pComponent = Find(sceneRegistry);
		if (pComponent.IsValid())
		{
			if constexpr (TypeTraits::IsBaseOf<Entity::HierarchyComponentBase, ComponentType>)
			{
				Entity::HierarchyComponentBase& hierarchyComponent = static_cast<Entity::HierarchyComponentBase&>(*pComponent);
				const EnumFlags<Entity::ComponentFlags> componentFlags = hierarchyComponent.GetFlags(sceneRegistry);
				if (componentFlags.IsNotSet(Entity::ComponentFlags::IsDestroying))
				{
					return hierarchyComponent.As<ComponentType>(sceneRegistry);
				}
				else
				{
					return Invalid;
				}
			}
			else
			{
				return static_cast<ComponentType&>(*pComponent);
			}
		}
		return Invalid;
	}
}
