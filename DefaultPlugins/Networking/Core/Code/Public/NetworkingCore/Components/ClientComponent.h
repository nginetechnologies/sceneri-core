#pragma once

#include <Engine/Entity/HierarchyComponent.h>

#include <NetworkingCore/Client/ClientIdentifier.h>

#include <Common/Storage/Identifier.h>

namespace ngine::Entity
{
	struct SceneRegistry;
}

namespace ngine::Network::Session
{
	struct Client final : public Entity::HierarchyComponent<Entity::HierarchyComponentBase>
	{
		using InstanceIdentifier = TIdentifier<uint32, 9>;
		using BaseType = Entity::HierarchyComponent<Entity::HierarchyComponentBase>;
		using ParentType = HierarchyComponentBase;

		Client(Initializer&& initializer);

		[[nodiscard]] virtual PURE_STATICS Entity::SceneRegistry& GetSceneRegistry() const override
		{
			return m_sceneRegistry;
		}

		[[nodiscard]] static Optional<Client*> Find(const ClientIdentifier identifier, const Entity::HierarchyComponentBase& rootComponent);

		void SetClientIdentifier(const ClientIdentifier identifier)
		{
			m_identifier = identifier;
		}
		[[nodiscard]] ClientIdentifier GetClientIdentifier() const
		{
			return m_identifier;
		}
	protected:
		friend struct Reflection::ReflectedType<Client>;
	private:
		Entity::SceneRegistry& m_sceneRegistry;
		ClientIdentifier m_identifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::Session::Client>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::Session::Client>(
			"{A1B618DB-74F3-45A6-B51C-0E8E90DAC128}"_guid,
			MAKE_UNICODE_LITERAL("Client Component"),
			Reflection::TypeFlags::DisableDynamicInstantiation | Reflection::TypeFlags::DisableDeletionFromUserInterface |
				Reflection::TypeFlags::DisableDynamicCloning
		);
	};
}
