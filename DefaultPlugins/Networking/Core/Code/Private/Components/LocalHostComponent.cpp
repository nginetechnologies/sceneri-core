#include <NetworkingCore/Components/LocalHostComponent.h>
#include <NetworkingCore/Components/HostComponent.h>
#include <NetworkingCore/Components/ClientComponent.h>
#include <NetworkingCore/Components/ModuleComponent.h>
#include <NetworkingCore/Components/BoundComponent.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/HierarchyComponent.inl>

#include <Common/Reflection/Registry.inl>
#include <Common/Network/Address.h>

namespace ngine::Network::Session
{
	Host::Host(Initializer&& initializer)
		: HierarchyComponent(Forward<Initializer>(initializer))
		, m_sceneRegistry(initializer.GetSceneRegistry())
	{
	}

	LocalHost::LocalHost(Initializer&& initializer)
		: HierarchyComponent(initializer)
	{
		m_localHost.SetEntitySceneRegistry(initializer.GetSceneRegistry());

		[[maybe_unused]] const bool wasStarted = m_localHost.Start(initializer.m_address, initializer.m_maximumClientCount);
		Assert(wasStarted);

		Host& host = initializer.GetParent().AsExpected<Host>();

		const BoundObjectIdentifier hostBoundObjectIdentifier = BoundObjectIdentifier::MakeFromValidIndex(0);
		m_localHost.BindObject(hostBoundObjectIdentifier, AnyView(static_cast<Entity::HierarchyComponentBase&>(host)));
		BoundComponent& hostBoundComponent =
			*host.CreateDataComponent<BoundComponent>(initializer.GetSceneRegistry(), hostBoundObjectIdentifier);

		m_localHost.OnClientConnected.Add(
			*this,
			[&host, &hostBoundComponent](
				LocalHost& localHost,
				Network::ClientIdentifier clientIdentifier,
				Network::RemoteClient,
				const BoundObjectIdentifier clientBoundObjectIdentifier
			)
			{
				Entity::SceneRegistry& sceneRegistry = host.GetSceneRegistry();
				Entity::ComponentTypeSceneData<Client>& clientTypeSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Client>();
				Client& client = *clientTypeSceneData.CreateInstance(Client::Initializer{host, sceneRegistry});

				client.SetClientIdentifier(clientIdentifier);

				localHost.m_localHost.BindObject(clientBoundObjectIdentifier, AnyView(static_cast<Entity::HierarchyComponentBase&>(client)));
				BoundComponent& clientBoundComponent =
					*client.CreateDataComponent<BoundComponent>(host.GetSceneRegistry(), clientBoundObjectIdentifier);

				host.IterateDataComponentsImplementingType<Module>(
					sceneRegistry,
					[&sceneRegistry, &host, &localHost, clientIdentifier, &hostBoundComponent, &clientBoundComponent, &client](
						Module& moduleComponent,
						const Entity::ComponentTypeInterface&,
						Entity::ComponentTypeSceneDataInterface& typeSceneData
					)
					{
						hostBoundComponent.AddDataComponent(host, sceneRegistry, clientIdentifier, typeSceneData);
						clientBoundComponent.AddDataComponent(client, sceneRegistry, clientIdentifier, typeSceneData);

						moduleComponent.OnRemoteClientConnectedToHost(host, localHost, clientIdentifier);

						return Memory::CallbackResult::Continue;
					}
				);
			}
		);
		m_localHost.OnClientDisconnected.Add(
			*this,
			[&host](LocalHost& localHost, const Network::ClientIdentifier clientIdentifier)
			{
				// TODO: Destroy components

				host.IterateDataComponentsImplementingType<Module>(
					host.GetSceneRegistry(),
					[&host,
			     &localHost,
			     clientIdentifier](Module& moduleComponent, const Entity::ComponentTypeInterface&, Entity::ComponentTypeSceneDataInterface&)
					{
						moduleComponent.OnRemoteClientDisconnectedFromHost(host, localHost, clientIdentifier);
						return Memory::CallbackResult::Continue;
					}
				);
			}
		);
	}

	void LocalHost::Restart(const Network::Address address, const uint32 maximumClientCount)
	{
		m_localHost.Stop();
		m_localHost.Start(address, maximumClientCount);
	}

	void LocalHost::Stop(ParentType&)
	{
		m_localHost.Stop();
		// TODO: Destroy the owner
		// owner.Destroy();
	}

	void LocalHost::OnDestroying()
	{
		m_localHost.Stop();
	}

	/* static */ Entity::DataComponentResult<LocalHost>
	LocalHost::Find(const Entity::HierarchyComponentBase& startingComponent, Entity::SceneRegistry& sceneRegistry)
	{
		Entity::DataComponentResult<LocalHost> result;
		if (const Optional<Session::Host*> pHost = startingComponent.FindFirstParentOfType<Session::Host>(sceneRegistry))
		{
			if (result.m_pDataComponent = pHost->FindDataComponentOfType<Session::LocalHost>(sceneRegistry); result.IsValid())
			{
				result.m_pDataComponentOwner = pHost;
				return result;
			}
		}

		if (const Optional<Session::Host*> pHostComponent = startingComponent.FindFirstChildOfType<Session::Host>(sceneRegistry);
		    const Optional<Session::LocalHost*> pLocalHostComponent =
		      pHostComponent.IsValid() ? pHostComponent->FindDataComponentOfType<Session::LocalHost>(sceneRegistry) : Invalid)
		{
			return {pLocalHostComponent, pHostComponent};
		}
		return {};
	}

	Module::Module(Initializer&&)
	{
	}

	Module::Module(const Deserializer&)
	{
	}

	Module::Module(const Module&, const Cloner&)
	{
	}

	[[maybe_unused]] const bool wasLocalHostComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<LocalHost>>::Make());
	[[maybe_unused]] const bool wasLocalHostTypeRegistered = Reflection::Registry::RegisterType<LocalHost>();
	[[maybe_unused]] const bool wasHostComponentRegistered = Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Host>>::Make(
	));
	[[maybe_unused]] const bool wasHostTypeRegistered = Reflection::Registry::RegisterType<Host>();
	[[maybe_unused]] const bool wasSessionModuleComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Module>>::Make());
	[[maybe_unused]] const bool wasSessionModuleTypeRegistered = Reflection::Registry::RegisterType<Module>();
}
