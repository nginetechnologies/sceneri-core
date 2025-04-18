#pragma once

#include "ComponentIdentifier.h"
#include "ComponentTypeIdentifier.h"

#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>

#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

#include <Common/Function/ForwardDeclarations/Function.h>
#include <Common/Memory/CallbackResult.h>
#include <Common/Memory/ForwardDeclarations/Any.h>
#include <Common/Memory/ForwardDeclarations/UniquePtr.h>
#include <Common/Storage/Identifier.h>

namespace ngine
{
	namespace Threading
	{
		struct Job;
		struct JobBatch;
	}

	namespace Reflection
	{
		struct TypeInterface;
	}

	namespace Reflection
	{
		struct TypeDeserializer;
	}
}

namespace ngine::Entity
{
	struct Component;
	struct HierarchyComponentBase;
	struct DataComponentOwner;
	struct ComponentTypeSceneDataInterface;
	struct Manager;
	struct SceneRegistry;
	struct ComponentRegistry;
	struct ComponentTypeExtension;
	struct IndicatorTypeExtension;

	struct ComponentTypeInterface
	{
		enum class TypeFlags : uint8
		{
			IsDataComponent = 1 << 0
		};

		ComponentTypeInterface() = default;
		virtual ~ComponentTypeInterface() = default;

		void SetIdentifier(const ComponentTypeIdentifier identifier)
		{
			m_identifier = identifier;
		}
		[[nodiscard]] ComponentTypeIdentifier GetIdentifier() const
		{
			return m_identifier;
		}
		virtual const Reflection::TypeInterface& GetTypeInterface() const = 0;
		virtual Optional<const ComponentTypeExtension*> GetTypeExtension() const = 0;
		virtual Optional<const IndicatorTypeExtension*> GetIndicatorTypeExtension() const = 0;

		virtual Optional<Entity::Component*>
		DeserializeInstanceWithoutChildrenManualOnCreated(const Reflection::TypeDeserializer& deserializer_, SceneRegistry& sceneRegistry) = 0;
		virtual Optional<Entity::Component*>
		DeserializeInstanceWithChildren(const Reflection::TypeDeserializer& deserializer, SceneRegistry& sceneRegistry) = 0;
		virtual Optional<Entity::Component*>
		DeserializeInstanceWithoutChildren(const Reflection::TypeDeserializer& deserializer, SceneRegistry& sceneRegistry) = 0;
		virtual Optional<Entity::Component*> CloneFromTemplateWithChildrenManualOnCreated(
			const Guid instanceGuid,
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			Entity::ComponentRegistry& componentRegistry,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) = 0;
		virtual Optional<Entity::Component*> CloneFromTemplateWithChildren(
			const Guid instanceGuid,
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			Entity::ComponentRegistry& componentRegistry,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) = 0;
		virtual Optional<Entity::Component*> CloneFromTemplateWithoutChildren(
			const Guid instanceGuid,
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) = 0;
		virtual Optional<Entity::Component*> CloneFromTemplateAndSerializeWithoutChildrenManualOnCreated(
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			const Serialization::Reader reader,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) = 0;
		virtual Optional<Entity::Component*> CloneFromTemplateAndSerializeWithoutChildren(
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			const Serialization::Reader reader,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) = 0;
		virtual Optional<Entity::Component*> CloneFromTemplateAndSerializeWithChildrenManualOnCreated(
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			const Serialization::Reader reader,
			Entity::ComponentRegistry& componentRegistry,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) = 0;
		virtual Optional<Entity::Component*> CloneFromTemplateAndSerializeWithChildren(
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			const Serialization::Reader reader,
			Entity::ComponentRegistry& componentRegistry,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			const Optional<uint16> preferredChildIndex = Invalid
		) = 0;
		[[nodiscard]] virtual Threading::JobBatch SerializeInstanceWithChildren(
			const Serialization::Reader reader, Entity::Component& component, const Optional<Entity::Component*> parentComponent
		) = 0;
		[[nodiscard]] virtual Threading::JobBatch SerializeInstanceWithoutChildren(
			const Serialization::Reader reader, Entity::Component& component, const Optional<Entity::Component*> parentComponent
		) = 0;
		virtual bool SerializeInstanceWithChildren(
			Serialization::Writer writer, const Entity::Component& component, const Optional<const Entity::Component*> parentComponent
		) const = 0;
		virtual bool SerializeInstanceWithoutChildren(
			Serialization::Writer writer, const Entity::Component& component, const Optional<const Entity::Component*> parentComponent
		) const = 0;

		virtual Optional<Entity::Component*> CloneFromTemplateManualOnCreated(
			[[maybe_unused]] const Guid instanceGuid,
			const Entity::Component& templateComponent,
			const HierarchyComponentBase& templateParentComponent,
			HierarchyComponentBase& parent,
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			Threading::JobBatch& jobBatchOut,
			[[maybe_unused]] const Optional<uint16> preferredChildIndex = Invalid
		) = 0;
		virtual void
		OnComponentCreated(Entity::Component& component, const Optional<DataComponentOwner*> pParent, SceneRegistry& sceneRegistry) const = 0;
		virtual void OnParentComponentCreated(Entity::Component& component, DataComponentOwner& parent) const = 0;
		virtual void
		OnComponentDeserialized(Entity::Component& component, const Serialization::Reader reader, Threading::JobBatch& jobBatch) const = 0;

		[[nodiscard]] virtual UniquePtr<ComponentTypeSceneDataInterface>
		CreateSceneData(const ComponentTypeIdentifier typeIdentifier, Manager& entityManager, SceneRegistry& sceneRegistry) = 0;

		[[nodiscard]] EnumFlags<TypeFlags> GetFlags() const
		{
			return m_flags;
		}
	protected:
		ComponentTypeIdentifier m_identifier;
		EnumFlags<TypeFlags> m_flags;
	};

	ENUM_FLAG_OPERATORS(ComponentTypeInterface::TypeFlags);
}
