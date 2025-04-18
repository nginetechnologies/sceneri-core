#pragma once

#include <Engine/Entity/Component.h>
#include <Engine/Entity/ComponentIdentifier.h>

#include <Common/Reflection/Type.h>

namespace ngine::Entity
{
	struct DataComponentOwner;
	struct SceneRegistry;
}

namespace ngine::Entity::Data
{
	struct Component : public Entity::Component
	{
		using InstanceIdentifier = ComponentIdentifier;
		using BaseType = Entity::Component;
		using RootType = Component;
		using ParentType = DataComponentOwner;
		using ChildType = void;

		using BaseType::BaseType;

		//! This Initializer is the initializer needed to instantiate data components at runtime when type info is unknown
		struct DynamicInitializer : public Reflection::TypeInitializer
		{
			using BaseType = Reflection::TypeInitializer;

			explicit DynamicInitializer(BaseType&& baseInitializer, DataComponentOwner& parent, SceneRegistry& sceneRegistry)
				: BaseType(Forward<BaseType>(baseInitializer))
				, m_parent(parent)
				, m_sceneRegistry(sceneRegistry)
			{
			}

			explicit DynamicInitializer(DataComponentOwner& parent, SceneRegistry& sceneRegistry)
				: m_parent(parent)
				, m_sceneRegistry(sceneRegistry)
			{
			}

			DynamicInitializer(Threading::JobBatch& jobBatch, DataComponentOwner& parent, SceneRegistry& sceneRegistry)
				: TypeInitializer{jobBatch}
				, m_parent(parent)
				, m_sceneRegistry(sceneRegistry)
			{
			}

			[[nodiscard]] DataComponentOwner& GetParent() const
			{
				return m_parent;
			}
			[[nodiscard]] SceneRegistry& GetSceneRegistry() const
			{
				return m_sceneRegistry;
			}
		private:
			ReferenceWrapper<DataComponentOwner> m_parent;
			ReferenceWrapper<SceneRegistry> m_sceneRegistry;
		};

		struct Initializer : public Reflection::TypeInitializer
		{
			using BaseType = Reflection::TypeInitializer;
			using BaseType::BaseType;
			Initializer(DynamicInitializer&& dynamicInitializer)
				: BaseType(Forward<DynamicInitializer>(dynamicInitializer))
			{
			}
			Initializer(DataComponentOwner&, SceneRegistry&)
				: BaseType{}
			{
			}
		};

		struct Deserializer : public Reflection::TypeDeserializer
		{
			Deserializer(
				const Serialization::Reader& reader,
				const Reflection::Registry& registry,
				Threading::JobBatch& jobBatch,
				DataComponentOwner& parent,
				SceneRegistry& sceneRegistry
			)
				: TypeDeserializer{reader, registry, jobBatch}
				, m_parent(parent)
				, m_sceneRegistry(sceneRegistry)
			{
			}

			Deserializer(TypeDeserializer&& deserializer, SceneRegistry& sceneRegistry, DataComponentOwner& parent)
				: TypeDeserializer(Forward<TypeDeserializer>(deserializer))
				, m_parent(parent)
				, m_sceneRegistry(sceneRegistry)
			{
			}

			[[nodiscard]] DataComponentOwner& GetParent() const
			{
				return m_parent;
			}
			[[nodiscard]] SceneRegistry& GetSceneRegistry() const
			{
				return m_sceneRegistry;
			}
		private:
			DataComponentOwner& m_parent;
			SceneRegistry& m_sceneRegistry;
		};

		struct Cloner : public Reflection::TypeCloner
		{
			Cloner(
				Threading::JobBatch& jobBatch,
				DataComponentOwner& parent,
				const DataComponentOwner& templateParent,
				SceneRegistry& sceneRegistry,
				const SceneRegistry& templateSceneRegistry
			)
				: TypeCloner{jobBatch}
				, m_parent(parent)
				, m_templateParent(templateParent)
				, m_sceneRegistry(sceneRegistry)
				, m_templateSceneRegistry(templateSceneRegistry)
			{
			}

			[[nodiscard]] DataComponentOwner& GetParent() const
			{
				return m_parent;
			}
			[[nodiscard]] const DataComponentOwner& GetTemplateParent() const
			{
				return m_templateParent;
			}

			[[nodiscard]] SceneRegistry& GetSceneRegistry() const
			{
				return m_sceneRegistry;
			}
			[[nodiscard]] const SceneRegistry& GetTemplateSceneRegistry() const
			{
				return m_templateSceneRegistry;
			}
		private:
			DataComponentOwner& m_parent;
			const DataComponentOwner& m_templateParent;
			SceneRegistry& m_sceneRegistry;
			const SceneRegistry& m_templateSceneRegistry;
		};

		Component() = default;
		Component(Initializer&&)
		{
		}
		Component(DynamicInitializer&&)
		{
		}
		Component(const Deserializer&)
		{
		}
		Component(const Component&, const Cloner&)
		{
		}
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::Component>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::Component>(
			"0E2C0CA0-CD61-433C-A41D-916733CD7503"_guid, MAKE_UNICODE_LITERAL("Data Component"), TypeFlags::IsAbstract
		);
	};
}
