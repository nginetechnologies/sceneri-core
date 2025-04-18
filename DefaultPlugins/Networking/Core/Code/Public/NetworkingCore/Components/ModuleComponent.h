#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>

#include <NetworkingCore/Components/BoundObjectIdentifier.h>
#include <NetworkingCore/Client/ClientIdentifier.h>

#include <Common/Storage/Identifier.h>

namespace ngine::Network
{
	struct RemoteClient;
	struct RemoteHost;
}

namespace ngine::Network::Session
{
	struct BoundComponent;
	struct Host;
	struct LocalHost;
	struct Client;
	struct LocalClient;

	struct Module : public Entity::Data::HierarchyComponent
	{
		using InstanceIdentifier = TIdentifier<uint8, 1>;

		using BaseType = Entity::Data::HierarchyComponent;

		Module(Initializer&& initializer = {});
		Module(const Deserializer&);
		Module(const Module&, const Cloner&);

		virtual void OnRemoteClientConnectedToHost(Host& host, LocalHost& localHost, Network::ClientIdentifier clientIdentifier) = 0;
		virtual void OnRemoteClientDisconnectedFromHost(Host& host, LocalHost& localHost, Network::ClientIdentifier clientIdentifier) = 0;
		virtual void OnLocalClientDisconnectedFromHost(Client& client, LocalClient& localClient) = 0;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::Session::Module>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::Session::Module>(
			"0013F0EA-067C-42AB-A9D2-5D74549FD2C3"_guid, MAKE_UNICODE_LITERAL("Session Module Component"), Reflection::TypeFlags::IsAbstract
		);
	};
}
