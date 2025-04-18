#pragma once

#include "../ComponentValue.h"

#include <Engine/Entity/Data/Component.h>
#include <Common/System/Query.h>
#include <Engine/Entity/ComponentTypeInterface.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/Manager.h>
#include <Common/Reflection/Registry.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>

#include <Common/Threading/Jobs/JobBatch.h>
#include <Engine/Threading/JobRunnerThread.h>

#include <Common/Serialization/Guid.h>

namespace ngine::Entity
{
	template<typename ComponentType>
	inline bool ComponentValue<ComponentType>::Serialize(Serialization::Writer serializer, const Optional<const ParentType*> parent) const
	{
		if (m_pComponent.IsInvalid())
		{
			return false;
		}

		const Entity::ComponentRegistry& registry = System::Get<Entity::Manager>().GetRegistry();
		const Entity::ComponentTypeIdentifier typeIdentifier = registry.FindIdentifier(m_pComponent->GetTypeGuid());
		const Optional<const ComponentTypeInterface*> pComponentTypeInfo = registry.Get(typeIdentifier);
		Assert(pComponentTypeInfo.IsValid());
		if (LIKELY(pComponentTypeInfo.IsValid()))
		{
			return Serialize(serializer, *pComponentTypeInfo, parent);
		}
		return false;
	}

	template<typename ComponentType>
	inline bool ComponentValue<ComponentType>::Serialize(
		Serialization::Writer serializer, const ComponentTypeInterface& componentTypeInfo, const Optional<const ParentType*> parent
	) const
	{
		if (m_pComponent.IsInvalid())
		{
			return false;
		}

		return componentTypeInfo.SerializeInstanceWithChildren(serializer, *m_pComponent, parent);
	}

	template<typename ComponentType>
	inline bool ComponentValue<ComponentType>::Serialize(
		const Serialization::Reader reader, ParentType& parent, SceneRegistry& sceneRegistry, Threading::JobBatch& jobBatchOut
	)
	{
		Entity::Manager& entityManager = System::Get<Entity::Manager>();

		Entity::ComponentRegistry& registry = entityManager.GetRegistry();

		Entity::ComponentTypeIdentifier typeIdentifier;
		if constexpr (TypeTraits::IsBaseOf<Data::Component, ComponentType>)
		{
			const Optional<Guid> typeGuid = reader.Read<Guid>("typeGuid");
			if (typeGuid.IsInvalid())
			{
				return false;
			}

			typeIdentifier = registry.FindIdentifier(typeGuid.Get());
			if (typeIdentifier.IsInvalid())
			{
				return false;
			}

			if (reader.ReadWithDefaultValue<bool>("removed", false))
			{
				parent.RemoveDataComponentOfType(sceneRegistry, typeIdentifier);
				return false;
			}
		}
		else
		{
			if (reader.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::UseWithinSessionInstance))
			{
				using TypeIdentifierInternalType = typename ComponentTypeIdentifier::InternalType;
				typeIdentifier = ComponentTypeIdentifier::MakeFromValue(reader.ReadWithDefaultValue<TypeIdentifierInternalType>(
					"typeIdentifier",
					TypeIdentifierInternalType(ComponentTypeIdentifier::Invalid)
				));
				Assert(typeIdentifier.IsValid());
			}
			else
			{
				const Optional<Guid> typeGuid = reader.Read<Guid>("typeGuid");
				if (typeGuid.IsInvalid())
				{
					return false;
				}

				typeIdentifier = registry.FindIdentifier(typeGuid.Get());
				if (typeIdentifier.IsInvalid())
				{
					return false;
				}
			}
		}

		const Optional<ComponentTypeInterface*> pComponentTypeInfo = registry.Get(typeIdentifier);
		if (LIKELY(pComponentTypeInfo.IsValid()))
		{
			Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
			typename ComponentType::Deserializer deserializer{
				Reflection::TypeDeserializer{reader, reflectionRegistry, jobBatchOut},
				sceneRegistry,
				parent
			};
			Optional<Component*> pComponent = pComponentTypeInfo->DeserializeInstanceWithChildren(deserializer, sceneRegistry);
			m_pComponent = pComponent.IsValid() ? Optional<ComponentType*>(&static_cast<ComponentType&>(*pComponent)) : Invalid;
			return m_pComponent.IsValid();
		}
		return false;
	}

	template<typename ComponentType>
	inline bool ComponentValue<ComponentType>::Serialize(const Serialization::Reader reader, ParentType& parent, SceneRegistry& sceneRegistry)
	{
		Threading::JobBatch jobBatch;
		const bool result = Serialize(reader, parent, sceneRegistry, jobBatch);
		if (jobBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
		}
		return result;
	}

	template<typename ComponentType>
	inline bool ComponentValue<ComponentType>::CloneFromTemplate(
		SceneRegistry& sceneRegistry,
		const SceneRegistry& templateSceneRegistry,
		ComponentTypeInterface& typeInfo,
		const Component& templateComponent,
		const ParentType& templateParentComponent,
		ParentType& parent,
		Threading::JobBatch& jobBatchOut
	)
	{
		Optional<Component*> pComponent = typeInfo.CloneFromTemplateWithoutChildren(
			Guid::Generate(),
			templateComponent,
			static_cast<const HierarchyComponentBase&>(templateParentComponent),
			static_cast<HierarchyComponentBase&>(parent),
			sceneRegistry,
			templateSceneRegistry,
			jobBatchOut
		);
		m_pComponent = pComponent.IsValid() ? Optional<ComponentType*>(&static_cast<ComponentType&>(*pComponent)) : Invalid;
		return m_pComponent.IsValid();
	}

	template<typename ComponentType>
	inline bool ComponentValue<ComponentType>::CloneFromTemplateAndSerialize(
		SceneRegistry& sceneRegistry,
		const SceneRegistry& templateSceneRegistry,
		ComponentTypeInterface& typeInfo,
		const Component& templateComponent,
		const ParentType& templateParentComponent,
		ParentType& parent,
		const Serialization::Reader reader,
		Threading::JobBatch& jobBatchOut
	)
	{
		Optional<Component*> pComponent = typeInfo.CloneFromTemplateAndSerializeWithoutChildren(
			templateComponent,
			static_cast<const HierarchyComponentBase&>(templateParentComponent),
			static_cast<HierarchyComponentBase&>(parent),
			reader,
			sceneRegistry,
			templateSceneRegistry,
			jobBatchOut
		);
		m_pComponent = pComponent.IsValid() ? Optional<ComponentType*>(&static_cast<ComponentType&>(*pComponent)) : Invalid;
		return m_pComponent.IsValid();
	}

	template<typename ComponentType>
	inline bool ComponentValue<ComponentType>::CloneFromTemplate(
		SceneRegistry& sceneRegistry,
		const SceneRegistry& templateSceneRegistry,
		ComponentTypeInterface& typeInfo,
		const Component& templateComponent,
		const ParentType& templateParentComponent,
		ParentType& parent
	)
	{
		Threading::JobBatch jobBatch;
		const bool result =
			CloneFromTemplate(sceneRegistry, templateSceneRegistry, typeInfo, templateComponent, templateParentComponent, parent, jobBatch);
		if (jobBatch.IsValid())
		{
			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);
		}
		return result;
	}
}
