#include <NetworkingCore/Components/LocalClientComponent.h>
#include <NetworkingCore/Components/ClientComponent.h>
#include <NetworkingCore/Components/HostComponent.h>
#include <NetworkingCore/Components/ModuleComponent.h>
#include <NetworkingCore/Components/BoundComponent.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/HierarchyComponent.inl>

#include <Common/Reflection/Registry.inl>
#include <Common/Network/Address.h>

namespace ngine::Network::Session
{
	Client::Client(Initializer&& initializer)
		: HierarchyComponent(Forward<Initializer>(initializer))
		, m_sceneRegistry(initializer.GetSceneRegistry())
	{
	}

	Optional<Client*> Client::Find(const ClientIdentifier identifier, const Entity::HierarchyComponentBase& rootComponent)
	{
		Optional<Client*> pClient;
		rootComponent.IterateChildrenOfType<Session::Client>(
			[identifier, &pClient](Session::Client& client)
			{
				if (client.GetClientIdentifier() == identifier)
				{
					pClient = &client;
					return Memory::CallbackResult::Break;
				}
				else
				{
					return Memory::CallbackResult::Continue;
				}
			}
		);
		return pClient;
	}

	LocalClient::LocalClient(Initializer&& initializer)
		: HierarchyComponent(initializer)
	{
		m_localClient.SetEntitySceneRegistry(initializer.GetSceneRegistry());

		Client& client = initializer.GetParent().AsExpected<Client>();

		m_localClient.OnConnected.Add(
			*this,
			[&client](LocalClient& localClient, Network::RemoteHost, const BoundObjectIdentifier clientBoundObjectIdentifier)
			{
				client.SetClientIdentifier(localClient.GetIdentifier());
				localClient.m_localClient.BindObject(clientBoundObjectIdentifier, AnyView(static_cast<Entity::HierarchyComponentBase&>(client)));
				client.CreateDataComponent<BoundComponent>(client.GetSceneRegistry(), clientBoundObjectIdentifier);

				{
					Entity::SceneRegistry& sceneRegistry = client.GetSceneRegistry();
					Entity::ComponentTypeSceneData<Host>& hostTypeSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Host>();
					Host& host = *hostTypeSceneData.CreateInstance(Host::Initializer{client.GetParent(), sceneRegistry});

					const BoundObjectIdentifier hostBoundObjectIdentifier = BoundObjectIdentifier::MakeFromValidIndex(0);
					localClient.m_localClient.BindObject(hostBoundObjectIdentifier, AnyView(static_cast<Entity::HierarchyComponentBase&>(host)));
					host.CreateDataComponent<BoundComponent>(host.GetSceneRegistry(), hostBoundObjectIdentifier);
				}

				if (const Optional<LocalHost*> pLocalHost = LocalHost::Find(client.GetRootParent(), client.GetSceneRegistry());
			      pLocalHost.IsValid() && pLocalHost->IsStarted())
				{
					pLocalHost->GetLocalHost().RegisterLocalClient(localClient.GetIdentifier());
				}

				/*client.IterateDataComponentsImplementingType<Module>(
			    sceneRegistry,
			    [&client,
			     &localClient,
			     remoteHost](Module& moduleComponent, const Entity::ComponentTypeInterface&, Entity::ComponentTypeSceneDataInterface&)
			    {
			      moduleComponent.OnLocalClientConnectedToHost(client, localClient, remoteHost);
			      return Memory::CallbackResult::Continue;
			    }
			  );*/
			}
		);
		m_localClient.OnDisconnected.Add(
			*this,
			[&client](LocalClient& localClient)
			{
				if (localClient.IsConnected())
				{
					if (const Optional<LocalHost*> pLocalHost = LocalHost::Find(client.GetRootParent(), client.GetSceneRegistry());
				      pLocalHost.IsValid() && pLocalHost->IsStarted())
					{
						pLocalHost->GetLocalHost().DeregisterLocalClient(localClient.GetIdentifier());
					}
				}

				localClient.m_localClient.Stop();
				client.SetClientIdentifier({});

				// TODO: Destroy components

				client.IterateDataComponentsImplementingType<Module>(
					client.GetSceneRegistry(),
					[&client, &localClient](Module& moduleComponent, const Entity::ComponentTypeInterface&, Entity::ComponentTypeSceneDataInterface&)
					{
						moduleComponent.OnLocalClientDisconnectedFromHost(client, localClient);
						return Memory::CallbackResult::Continue;
					}
				);
			}
		);
	}

	void LocalClient::OnDestroying()
	{
		m_localClient.ForceDisconnect();
	}

	bool LocalClient::Connect(
		Client& owner,
		const Network::Address address,
		const uint8 maximumChannelCount,
		const uint32 connectionUserData,
		const Network::LocalPeer::UpdateMode updateMode
	)
	{
		if (m_localClient.IsConnectingOrConnected() || m_localClient.IsConnected())
		{
			m_localClient.ForceDisconnect();
			m_localClient.Stop();
		}

		[[maybe_unused]] const bool wasStarted = m_localClient.Start(maximumChannelCount);
		Assert(wasStarted);
		const bool connected = m_localClient.Connect(address, maximumChannelCount, connectionUserData, updateMode).IsValid();
		owner.SetClientIdentifier(m_localClient.GetIdentifier());
		return connected;
	}

	void LocalClient::Disconnect(Client&)
	{
		m_localClient.Disconnect();
	}

	/* static */ Entity::DataComponentResult<LocalClient>
	LocalClient::Find(const Entity::HierarchyComponentBase& startingComponent, Entity::SceneRegistry& sceneRegistry)
	{
		Entity::DataComponentResult<LocalClient> result;
		if (const Optional<Session::Client*> pClient = startingComponent.FindFirstParentOfType<Session::Client>(sceneRegistry))
		{
			if (result.m_pDataComponent = pClient->FindDataComponentOfType<Session::LocalClient>(sceneRegistry); result.IsValid())
			{
				result.m_pDataComponentOwner = pClient;
				return result;
			}
		}
		return {};
	}

	[[maybe_unused]] const bool wasLocalClientComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<LocalClient>>::Make());
	[[maybe_unused]] const bool wasLocalClientTypeRegistered = Reflection::Registry::RegisterType<LocalClient>();
	[[maybe_unused]] const bool wasClientComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Client>>::Make());
	[[maybe_unused]] const bool wasClientTypeRegistered = Reflection::Registry::RegisterType<Client>();
}
