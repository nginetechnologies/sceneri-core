#pragma once

#include <Common/TypeTraits/HasFunctionCallOperator.h>
#include <Common/Memory/Optional.h>

namespace ngine
{
	struct Guid;
}

namespace ngine::Entity
{
	struct HierarchyComponentBase;
	struct SceneRegistry;
}

namespace ngine::Context
{
	namespace Data
	{
		struct Component;
	}

	struct EventManager
	{
		explicit EventManager(Optional<Data::Component*> pContext)
			: m_pContext(pContext)
		{
		}

		explicit EventManager(Entity::HierarchyComponentBase& component, Entity::SceneRegistry& registry);

		static void Notify(Guid eventGuid, Entity::HierarchyComponentBase& component, Entity::SceneRegistry& registry);

		template<auto Callback, typename ObjectType>
		static void
		Subscribe(const Guid identifier, ObjectType& object, Entity::HierarchyComponentBase& component, Entity::SceneRegistry& registry);

		template<auto Callback, typename ObjectType>
		void Subscribe(Guid guid, ObjectType& object);

		template<typename... Args>
		void Notify(Guid guid, Args&&... args);

		template<typename InstanceIdentifierType, typename CallbackObject>
		EnableIf<TypeTraits::HasFunctionCallOperator<CallbackObject>>
		Subscribe(Guid guid, const InstanceIdentifierType instanceIdentifier, CallbackObject&& callback);

		template<typename ObjectType>
		void Unsubscribe(Guid guid, ObjectType& object);

		void Notify(Guid guid);

		void Deregister(Guid guid);

		template<typename InstanceIdentifierType, typename CallbackObject>
		static EnableIf<TypeTraits::HasFunctionCallOperator<CallbackObject>> Subscribe(
			const Guid identifier,
			const InstanceIdentifierType instanceIdentifier,
			Entity::HierarchyComponentBase& component,
			Entity::SceneRegistry& registry,
			CallbackObject&& callback
		);

		template<typename ObjectType>
		static void
		Unsubscribe(const Guid identifier, ObjectType& object, Entity::HierarchyComponentBase& component, Entity::SceneRegistry& registry);

		[[nodiscard]] Optional<Data::Component*> GetContext() const
		{
			return m_pContext;
		}
	protected:
		Optional<Data::Component*> m_pContext;
	};
}
