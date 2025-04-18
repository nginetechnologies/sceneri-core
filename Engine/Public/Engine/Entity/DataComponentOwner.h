#pragma once

#include <Common/Storage/Identifier.h>

#include <Engine/Entity/ComponentIdentifier.h>
#include <Engine/Entity/Component.h>
#include <Engine/Entity/ComponentTypeIdentifier.h>

namespace ngine::Entity
{
	namespace Data
	{
		struct Component;
	}

	struct SceneRegistry;

	template<typename Type>
	struct ComponentTypeSceneData;

	struct DataComponentOwner : public Component
	{
		using BaseType = Component;
		using BaseType::BaseType;
	public:
		DataComponentOwner(const ComponentIdentifier identifier)
			: m_identifier(identifier)
		{
		}

		[[nodiscard]] PURE_STATICS ComponentIdentifier GetIdentifier() const
		{
			return m_identifier;
		}

		template<typename DataComponentType, typename... Args>
		Optional<DataComponentType*> CreateDataComponent(ComponentTypeSceneData<DataComponentType>& sceneData, Args&&... args);
		template<typename DataComponentType, typename... Args>
		Optional<DataComponentType*> CreateDataComponent(SceneRegistry& sceneRegistry, Args&&... args);
		template<typename DataComponentType>
		bool RemoveDataComponentOfType(ComponentTypeSceneData<DataComponentType>& sceneData);
		template<typename DataComponentType>
		bool RemoveDataComponentOfType(const SceneRegistry& sceneRegistry);
		bool RemoveDataComponentOfType(SceneRegistry& sceneRegistry, const ComponentTypeIdentifier dataComponentTypeIdentifier);
		template<typename DataComponentType>
		bool RemoveFirstDataComponentImplementingType(SceneRegistry& sceneRegistry);
		template<typename DataComponentType>
		[[nodiscard]] Optional<DataComponentType*> FindDataComponentOfType(ComponentTypeSceneData<DataComponentType>& sceneData) const;
		template<typename DataComponentType>
		[[nodiscard]] Optional<DataComponentType*> FindDataComponentOfType(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Optional<Data::Component*> FindDataComponentOfType(const SceneRegistry& sceneRegistry, const Guid typeGuid) const;
		[[nodiscard]] bool HasDataComponentOfType(const SceneRegistry& sceneRegistry, const Guid typeGuid) const;
		template<typename DataComponentType>
		[[nodiscard]] Optional<DataComponentType*> FindFirstDataComponentImplementingType(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Optional<Data::Component*>
		FindFirstDataComponentImplementingType(const SceneRegistry& sceneRegistry, const Guid typeGuid) const;
		[[nodiscard]] bool
		HasDataComponentOfType(const SceneRegistry& sceneRegistry, const ComponentTypeIdentifier dataComponentTypeIdentifier) const;
		template<typename DataComponentType>
		[[nodiscard]] bool HasDataComponentOfType(const SceneRegistry& sceneRegistry) const;

		[[nodiscard]] bool HasAnyDataComponents(const SceneRegistry& sceneRegistry) const;

		template<typename DataComponentType>
		[[nodiscard]] bool HasAnyDataComponentsImplementingType(const SceneRegistry& sceneRegistry) const;
		[[nodiscard]] bool HasAnyDataComponentsImplementingType(const SceneRegistry& sceneRegistry, const Guid typeGuid) const;

		template<typename Callback>
		void IterateDataComponents(const SceneRegistry& sceneRegistry, Callback&& callback) const;
		template<typename DataComponentType, typename Callback>
		void IterateDataComponentsImplementingType(const SceneRegistry& sceneRegistry, Callback&& callback) const;
		template<typename Callback>
		void IterateDataComponentsImplementingType(const SceneRegistry& sceneRegistry, const Guid typeGuid, Callback&& callback) const;
	private:
		const ComponentIdentifier m_identifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::DataComponentOwner>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::DataComponentOwner>(
			"7d9220e7-a5d4-488d-a0d6-6f9f3d5e3084"_guid, MAKE_UNICODE_LITERAL("Data Component Owner"), TypeFlags::IsAbstract
		);
	};
}
