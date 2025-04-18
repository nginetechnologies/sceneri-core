#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>

#include <NetworkingCore/Client/LocalClient.h>

#include <Common/Storage/Identifier.h>
#include <Common/Network/Address.h>

namespace ngine::Entity
{
	template<typename DataComponentType>
	struct DataComponentResult;
}

namespace ngine::Network::Session
{
	struct BoundComponent;
	struct Client;

	struct LocalClient final : public Entity::Data::HierarchyComponent
	{
		using InstanceIdentifier = TIdentifier<uint32, 4>;
		using BaseType = Entity::Data::HierarchyComponent;
		using ParentType = Client;

		using Initializer = DynamicInitializer;
		LocalClient(Initializer&& initializer);

		bool Connect(Client& owner, const Address address, const uint8 maximumChannelCount = 2, const uint32 connectionUserData = 0);
		void Disconnect(Client& owner);

		[[nodiscard]] bool IsConnecting() const
		{
			return m_localClient.IsConnecting();
		}
		[[nodiscard]] bool IsConnected() const
		{
			return m_localClient.IsConnected();
		}
		[[nodiscard]] bool IsConnectingOrConnected() const
		{
			return m_localClient.IsConnectingOrConnected();
		}
		[[nodiscard]] bool IsDisconnecting() const
		{
			return m_localClient.IsDisconnecting();
		}
		[[nodiscard]] bool IsDisconnected() const
		{
			return m_localClient.IsDisconnected();
		}

		[[nodiscard]] ClientIdentifier GetIdentifier() const
		{
			return m_localClient.GetIdentifier();
		}

		[[nodiscard]] Address GetLocalHostAddress() const
		{
			return m_localClient.GetLocalHostAddress();
		}
		[[nodiscard]] Address GetPublicHostAddress() const
		{
			return m_localClient.GetPublicHostAddress();
		}

		template<typename Type>
		[[nodiscard]] Optional<Type*> GetBoundObject(const BoundObjectIdentifier boundObjectIdentifier) const
		{
			return m_localClient.GetBoundObject<Type>(boundObjectIdentifier);
		}

		[[nodiscard]] Network::LocalClient& GetLocalClient()
		{
			return m_localClient;
		}
		[[nodiscard]] const Network::LocalClient& GetLocalClient() const
		{
			return m_localClient;
		}

		//! Searches upwards in the hierarchy until it encounters a local client component
		[[nodiscard]] static Entity::DataComponentResult<LocalClient>
		Find(const Entity::HierarchyComponentBase& startingComponent, Entity::SceneRegistry& sceneRegistry);
	protected:
		friend BoundComponent;
	protected:
		Network::LocalClient m_localClient;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::Session::LocalClient>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::Session::LocalClient>(
			"{4883D555-D7E6-4B09-93AD-ABDB7835CDC5}"_guid,
			MAKE_UNICODE_LITERAL("Local Client Component"),
			Reflection::TypeFlags::DisableDynamicDeserialization | Reflection::TypeFlags::DisableDynamicCloning
		);
	};
}
