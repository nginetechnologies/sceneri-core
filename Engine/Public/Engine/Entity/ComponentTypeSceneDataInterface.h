#pragma once

#include <Engine/Entity/ComponentIdentifier.h>
#include <Engine/Entity/ComponentInstanceIdentifier.h>
#include <Engine/Entity/ComponentTypeIdentifier.h>
#include <Engine/Entity/ForwardDeclarations/ComponentTypeSceneData.h>

#include <Common/Memory/ForwardDeclarations/AnyView.h>
#include <Common/Storage/Identifier.h>

namespace ngine
{
	struct FrameTime;
	struct Scene3D;

	namespace Reflection
	{
		struct TypeInitializer;
	}

	namespace Threading
	{
		struct StageBase;
	}
}

namespace ngine::Entity
{
	struct Component;
	struct DataComponentOwner;
	struct Manager;
	struct SceneRegistry;
	struct ComponentTypeInterface;
	struct ComponentStage;

	struct ComponentTypeSceneDataInterface
	{
		ComponentTypeSceneDataInterface(
			const ComponentTypeIdentifier identifier, ComponentTypeInterface& componentType, Manager& entityManager, SceneRegistry& sceneRegistry
		)
			: m_identifier(identifier)
			, m_componentType(componentType)
			, m_manager(entityManager)
			, m_sceneRegistry(sceneRegistry)
		{
		}
		virtual ~ComponentTypeSceneDataInterface() = default;

		virtual void OnBeforeRemoveInstance(Component& component, const Optional<DataComponentOwner*> pParent) = 0;
		virtual void RemoveInstance(Component& component, const Optional<DataComponentOwner*> pParent) = 0;
		virtual void DisableInstance(Component& component, const Optional<DataComponentOwner*> pParent) = 0;
		virtual void EnableInstance(Component& component, const Optional<DataComponentOwner*> pParent) = 0;
		virtual void DetachInstanceFromTree(Component& component, const Optional<DataComponentOwner*> pParent) = 0;
		virtual void AttachInstanceToTree(Component& component, const Optional<DataComponentOwner*> pParent) = 0;
		virtual void DestroyAllInstances() = 0;

		virtual void PauseInstanceSimulation(Component& component, const Optional<DataComponentOwner*> pParent) = 0;
		virtual void ResumeInstanceSimulation(Component& component, const Optional<DataComponentOwner*> pParent) = 0;

		virtual Optional<ComponentStage*> GetUpdateStage() = 0;
		virtual Optional<ComponentStage*> GetBeforePhysicsUpdateStage() = 0;
		virtual Optional<ComponentStage*> GetFixedPhysicsUpdateStage() = 0;
		virtual Optional<ComponentStage*> GetAfterPhysicsUpdateStage() = 0;

		[[nodiscard]] ComponentTypeIdentifier GetIdentifier() const
		{
			return m_identifier;
		}
		[[nodiscard]] ComponentTypeInterface& GetTypeInterface() const
		{
			return m_componentType;
		}
		[[nodiscard]] SceneRegistry& GetSceneRegistry() const
		{
			return m_sceneRegistry;
		}
		[[nodiscard]] Manager& GetManager() const
		{
			return m_manager;
		}

		[[nodiscard]] virtual GenericComponentInstanceIdentifier GetComponentInstanceIdentifier(const Component& component) const = 0;
		[[nodiscard]] virtual GenericComponentInstanceIdentifier FindComponentInstanceIdentifier(const Guid instanceGuid) const = 0;
		[[nodiscard]] virtual GenericComponentInstanceIdentifier FindDataComponentInstanceIdentifier(const ComponentIdentifier) const = 0;
		[[nodiscard]] virtual Guid FindComponentInstanceGuid(const GenericComponentInstanceIdentifier identifier) const = 0;
		[[nodiscard]] virtual Optional<Component*> FindComponent(const GenericComponentInstanceIdentifier identifier) = 0;
		[[nodiscard]] virtual void* GetComponentAddress(const GenericComponentInstanceIdentifier identifier) = 0;
		[[nodiscard]] virtual Optional<Component*> GetDataComponent(const ComponentIdentifier) = 0;
		// TODO: Make removing data components always go through scene data to avoid this
		[[nodiscard]] virtual Component& GetDataComponentUnsafe(const ComponentIdentifier) = 0;

		virtual Optional<Component*> CreateInstanceDynamic(AnyView initializer) = 0;

		virtual void OnInstanceGuidChanged(const Guid previousGuid, const Guid newGuid) = 0;
	protected:
		const ComponentTypeIdentifier m_identifier;
		ComponentTypeInterface& m_componentType;
		Manager& m_manager;
		SceneRegistry& m_sceneRegistry;
	};
}
