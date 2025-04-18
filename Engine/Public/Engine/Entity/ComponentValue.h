#pragma once

#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

namespace ngine
{
	struct Scene3D;

	namespace Threading
	{
		struct Job;
		struct JobBatch;
	}
}

namespace ngine::Entity
{
	struct Component;
	struct ComponentTypeInterface;
	struct ComponentTypeSceneDataInterface;
	template<typename ComponentType>
	struct ComponentTypeSceneData;

	//! Represents the value type of a component
	//! When serialized from disk this creates a completely new instance based on the source
	//! For a version that reads the exact instance that was written, see ComponentReference.
	template<typename ComponentType>
	struct ComponentValue
	{
		using ParentType = typename ComponentType::ParentType;

		ComponentValue() = default;
		ComponentValue(ComponentType& component)
			: m_pComponent(&component)
		{
		}
		//! Creates a new component instance
		template<typename... Args, typename ComponentType_ = ComponentType, typename = EnableIf<!TypeTraits::IsAbstract<ComponentType_>>>
		ComponentValue(ComponentTypeSceneData<ComponentType>& typeSceneData, Args&&... args);
		//! Creates a new component instance
		template<typename... Args>
		ComponentValue(SceneRegistry& sceneRegistry, Args&&... args);

		ComponentValue(ComponentTypeSceneDataInterface& typeSceneDataInterface, typename ComponentType::DynamicInitializer&& initializer);
		ComponentValue(
			SceneRegistry& sceneRegistry, const ComponentTypeIdentifier typeIdentifier, typename ComponentType::DynamicInitializer&& initializer
		);

		bool Serialize(Serialization::Writer serializer, const Optional<const ParentType*> parent = Invalid) const;
		bool Serialize(
			Serialization::Writer serializer, const ComponentTypeInterface& componentTypeInfo, const Optional<const ParentType*> parent = Invalid
		) const;
		bool Serialize(const Serialization::Reader reader, ParentType& parent, SceneRegistry& sceneRegistry);
		bool Serialize(const Serialization::Reader reader, ParentType& parent, SceneRegistry& sceneRegistry, Threading::JobBatch& jobBatchOut);

		using DeserializedCallback = Function<void(Optional<ComponentType*>, Threading::JobBatch&&), 24>;
		[[nodiscard]] static Threading::JobBatch DeserializeAsync(
			ParentType& parent,
			Entity::SceneRegistry& sceneRegistry,
			const Asset::Guid assetGuid,
			DeserializedCallback&& callback,
			const Guid instanceGuid = Guid::Generate()
		);

		bool CloneFromTemplate(
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			ComponentTypeInterface& typeInfo,
			const Component& templateComponent,
			const ParentType& templateParentComponent,
			ParentType& parent
		);
		bool CloneFromTemplate(
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			ComponentTypeInterface& typeInfo,
			const Component& templateComponent,
			const ParentType& templateParentComponent,
			ParentType& parent,
			Threading::JobBatch& jobBatchOut
		);
		bool CloneFromTemplateAndSerialize(
			SceneRegistry& sceneRegistry,
			const SceneRegistry& templateSceneRegistry,
			ComponentTypeInterface& typeInfo,
			const Component& templateComponent,
			const ParentType& templateParentComponent,
			ParentType& parent,
			const Serialization::Reader reader,
			Threading::JobBatch& jobBatchOut
		);

		[[nodiscard]] ComponentType& operator*() const noexcept
		{
			return *m_pComponent;
		}

		[[nodiscard]] ComponentType* operator->() const noexcept
		{
			return m_pComponent;
		}

		[[nodiscard]] operator Optional<ComponentType*>() const
		{
			return m_pComponent;
		}

		[[nodiscard]] Optional<ComponentType*> Get() const
		{
			return m_pComponent;
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_pComponent.IsValid();
		}
	protected:
		template<typename... Args, typename ComponentType_ = ComponentType, typename = EnableIf<!TypeTraits::IsAbstract<ComponentType_>>>
		ComponentValue(const Optional<ComponentTypeSceneData<ComponentType>*> typeSceneData, Args&&... args);
		ComponentValue(
			const Optional<ComponentTypeSceneDataInterface*> pTypeSceneDataInterface, typename ComponentType::DynamicInitializer&& initializer
		);
	protected:
		Optional<ComponentType*> m_pComponent;
	};
}
