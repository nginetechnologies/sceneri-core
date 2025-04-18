#include <NetworkingCore/Components/BoundComponent.h>
#include <NetworkingCore/Components/HostComponent.h>
#include <NetworkingCore/Components/LocalHostComponent.h>
#include <NetworkingCore/Components/ClientComponent.h>
#include <NetworkingCore/Components/LocalClientComponent.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Data/InstanceGuid.h>
#include <Engine/Entity/HierarchyComponent.inl>

#include <Common/Reflection/Registry.inl>

namespace ngine::Network::Session
{
	BoundComponent::BoundComponent(Initializer&& initializer)
	{
		Entity::HierarchyComponentBase& owner = initializer.GetParent();
		const Entity::ComponentIdentifier componentIdentifier = owner.GetIdentifier();
		Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData =
			*initializer.GetSceneRegistry().FindComponentTypeData<Entity::Data::InstanceGuid>();

		const Optional<Entity::Data::InstanceGuid*> pInstanceGuid = instanceGuidSceneData.GetComponentImplementation(componentIdentifier);
		Assert(pInstanceGuid.IsValid(), "Component must have instance guid to bind to network!");
		if (LIKELY(pInstanceGuid.IsValid()))
		{
			const Guid instanceGuid = pInstanceGuid->GetGuid();

			Entity::HierarchyComponentBase& rootParent = owner.GetRootParent();

			if (Optional<Session::LocalClient*> pLocalClientComponent = Session::LocalClient::Find(initializer.GetParent(), initializer.GetSceneRegistry()))
			{
				pLocalClientComponent->m_localClient.BindObject(
					instanceGuid,
					AnyView(owner),
					[this](const Network::BoundObjectIdentifier boundObjectIdentifier)
					{
						OnAssignedBoundObjectIdentifier(boundObjectIdentifier);
					}
				);
			}

			if (const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(rootParent, initializer.GetSceneRegistry());
			    pLocalHostComponent.IsValid() && pLocalHostComponent->IsStarted())
			{
				const Network::BoundObjectIdentifier boundObjectIdentifier =
					pLocalHostComponent->m_localHost.BindObject(instanceGuid, AnyView(owner));
				OnAssignedBoundObjectIdentifier(boundObjectIdentifier);
			}
		}
	}

	BoundComponent::BoundComponent(const Deserializer& deserializer)
	{
		Entity::HierarchyComponentBase& owner = deserializer.GetParent();
		const Entity::ComponentIdentifier componentIdentifier = owner.GetIdentifier();
		Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData =
			*deserializer.GetSceneRegistry().FindComponentTypeData<Entity::Data::InstanceGuid>();

		const Optional<Entity::Data::InstanceGuid*> pInstanceGuid = instanceGuidSceneData.GetComponentImplementation(componentIdentifier);
		Assert(pInstanceGuid.IsValid(), "Component must have instance guid to bind to network!");
		if (LIKELY(pInstanceGuid.IsValid()))
		{
			const Guid instanceGuid = pInstanceGuid->GetGuid();

			Entity::HierarchyComponentBase& rootParent = owner.GetRootParent();

			if (Optional<Session::LocalClient*> pLocalClientComponent = Session::LocalClient::Find(deserializer.GetParent(), deserializer.GetSceneRegistry()))
			{
				pLocalClientComponent->m_localClient.BindObject(
					instanceGuid,
					AnyView(owner),
					[this](const Network::BoundObjectIdentifier boundObjectIdentifier)
					{
						OnAssignedBoundObjectIdentifier(boundObjectIdentifier);
					}
				);
			}

			if (const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(rootParent, deserializer.GetSceneRegistry());
			    pLocalHostComponent.IsValid() && pLocalHostComponent->IsStarted())
			{
				const Network::BoundObjectIdentifier boundObjectIdentifier =
					pLocalHostComponent->m_localHost.BindObject(instanceGuid, AnyView(owner));
				OnAssignedBoundObjectIdentifier(boundObjectIdentifier);
			}
		}
	}

	BoundComponent::BoundComponent(const BoundComponent&, const Cloner& cloner)
	{
		Entity::HierarchyComponentBase& owner = cloner.GetParent();
		const Entity::ComponentIdentifier componentIdentifier = owner.GetIdentifier();
		Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData =
			*cloner.GetSceneRegistry().FindComponentTypeData<Entity::Data::InstanceGuid>();

		const Optional<Entity::Data::InstanceGuid*> pInstanceGuid = instanceGuidSceneData.GetComponentImplementation(componentIdentifier);
		Assert(pInstanceGuid.IsValid(), "Component must have instance guid to bind to network!");
		if (LIKELY(pInstanceGuid.IsValid()))
		{
			const Guid instanceGuid = pInstanceGuid->GetGuid();

			Entity::HierarchyComponentBase& rootParent = owner.GetRootParent();

			if (Optional<Session::LocalClient*> pLocalClientComponent = Session::LocalClient::Find(cloner.GetParent(), cloner.GetSceneRegistry()))
			{
				pLocalClientComponent->m_localClient.BindObject(
					instanceGuid,
					AnyView(owner),
					[this](const Network::BoundObjectIdentifier boundObjectIdentifier)
					{
						OnAssignedBoundObjectIdentifier(boundObjectIdentifier);
					}
				);
			}

			if (const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(rootParent, cloner.GetSceneRegistry());
			    pLocalHostComponent.IsValid() && pLocalHostComponent->IsStarted())
			{
				const Network::BoundObjectIdentifier boundObjectIdentifier =
					pLocalHostComponent->m_localHost.BindObject(instanceGuid, AnyView(owner));
				OnAssignedBoundObjectIdentifier(boundObjectIdentifier);
			}
		}
	}

	BoundComponent::BoundComponent(
		Entity::HierarchyComponentBase& owner, Entity::SceneRegistry& sceneRegistry, const Network::BoundObjectIdentifier boundObjectIdentifier
	)
		: m_boundObjectIdentifier(boundObjectIdentifier)
	{
		const Entity::ComponentIdentifier componentIdentifier = owner.GetIdentifier();
		Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::InstanceGuid>();

		const Optional<Entity::Data::InstanceGuid*> pInstanceGuid = instanceGuidSceneData.GetComponentImplementation(componentIdentifier);
		Assert(pInstanceGuid.IsValid(), "Component must have instance guid to bind to network!");
		if (LIKELY(pInstanceGuid.IsValid()))
		{
			const Guid instanceGuid = pInstanceGuid->GetGuid();

			Entity::HierarchyComponentBase& rootParent = owner.GetRootParent();

			if (Optional<Session::LocalClient*> pLocalClientComponent = Session::LocalClient::Find(owner, sceneRegistry))
			{
				pLocalClientComponent->m_localClient.BindObject(instanceGuid, boundObjectIdentifier, AnyView(owner));
			}

			if (const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(rootParent, sceneRegistry);
			    pLocalHostComponent.IsValid() && pLocalHostComponent->IsStarted())
			{
				pLocalHostComponent->m_localHost.BindObject(boundObjectIdentifier, AnyView(owner));
			}
		}
	}

	BoundComponent::BoundComponent(
		Entity::HierarchyComponentBase& owner,
		Entity::SceneRegistry& sceneRegistry,
		Network::LocalClient& localClient,
		const Network::BoundObjectIdentifier boundObjectIdentifier
	)
		: m_boundObjectIdentifier(boundObjectIdentifier)
	{
		const Entity::ComponentIdentifier componentIdentifier = owner.GetIdentifier();
		Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData =
			*sceneRegistry.FindComponentTypeData<Entity::Data::InstanceGuid>();

		const Optional<Entity::Data::InstanceGuid*> pInstanceGuid = instanceGuidSceneData.GetComponentImplementation(componentIdentifier);
		Assert(pInstanceGuid.IsValid(), "Component must have instance guid to bind to network!");
		if (LIKELY(pInstanceGuid.IsValid()))
		{
			const Guid instanceGuid = pInstanceGuid->GetGuid();

			localClient.BindObject(instanceGuid, boundObjectIdentifier, AnyView(owner));

			Entity::HierarchyComponentBase& rootParent = owner.GetRootParent();
			if (const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(rootParent, sceneRegistry);
			    pLocalHostComponent.IsValid() && pLocalHostComponent->IsStarted())
			{
				pLocalHostComponent->m_localHost.BindObject(boundObjectIdentifier, AnyView(owner));
			}
		}
	}

	BoundComponent::BoundComponent(
		Entity::HierarchyComponentBase& owner, Entity::SceneRegistry& sceneRegistry, Network::LocalHost& localHost, const Guid instanceGuid
	)
		: m_boundObjectIdentifier(localHost.BindObject(instanceGuid, AnyView(owner)))
	{
		if (Optional<Session::LocalClient*> pLocalClientComponent = Session::LocalClient::Find(owner, sceneRegistry))
		{
			pLocalClientComponent->m_localClient.BindObject(instanceGuid, m_boundObjectIdentifier, AnyView(owner));
		}
	}

	void BoundComponent::OnDestroying(ParentType& owner)
	{
		if (m_boundObjectIdentifier.IsValid())
		{
			Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
			Entity::HierarchyComponentBase& rootParent = owner.GetRootParent();
			if (const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(rootParent, sceneRegistry);
			    pLocalHostComponent.IsValid())
			{
				pLocalHostComponent->m_localHost.UnbindObject(m_boundObjectIdentifier);
			}

			if (Optional<Session::LocalClient*> pLocalClientComponent = Session::LocalClient::Find(owner, sceneRegistry))
			{
				pLocalClientComponent->m_localClient.UnbindObject(m_boundObjectIdentifier);
			}

			rootParent.IterateChildrenOfType<Session::Client>(
				sceneRegistry,
				[boundObjectIdentifier = m_boundObjectIdentifier, &sceneRegistry](Session::Client& client)
				{
					if (const Optional<Session::LocalClient*> pLocalClient = client.FindDataComponentOfType<Session::LocalClient>(sceneRegistry))
					{
						pLocalClient->m_localClient.UnbindObject(boundObjectIdentifier);
					}
					return Memory::CallbackResult::Continue;
				}
			);
		}
	}

	void BoundComponent::OnAssignedBoundObjectIdentifier(const Network::BoundObjectIdentifier boundObjectIdentifier)
	{
		Assert(m_boundObjectIdentifier.IsInvalid() || m_boundObjectIdentifier == boundObjectIdentifier);
		m_boundObjectIdentifier = boundObjectIdentifier;

		Threading::UniqueLock lock(m_queuedMessagesMutex);
		Vector<QueuedMessage> queuedMessages = Move(m_queuedMessages);
		for (QueuedMessage& queuedMessage : queuedMessages)
		{
			BitView targetView{queuedMessage.m_encodedMessageBuffer.GetView()};
			const bool compressedMessageTypeIdentifier = queuedMessage.m_peer.CompressMessage(queuedMessage.m_messageTypeIdentifier, targetView);
			Assert(compressedMessageTypeIdentifier);
			const bool compressedBoundObjectIdentifier = queuedMessage.m_peer.CompressMessage(boundObjectIdentifier, targetView);
			Assert(compressedBoundObjectIdentifier);
			if (LIKELY(compressedMessageTypeIdentifier && compressedBoundObjectIdentifier))
			{
				switch (queuedMessage.m_type)
				{
					case QueuedMessage::Type::HostToAllClients:
					{
						Network::LocalHost& localHost = static_cast<Network::LocalHost&>(queuedMessage.m_peer);
						const bool wasBroadcast = localHost.BroadcastMessageToAllClients(
							queuedMessage.m_messageTypeIdentifier,
							queuedMessage.m_channel,
							Move(queuedMessage.m_encodedMessageBuffer)
						);
						Assert(wasBroadcast);
					}
					break;
					case QueuedMessage::Type::HostToRemoteClients:
					{
						Network::LocalHost& localHost = static_cast<Network::LocalHost&>(queuedMessage.m_peer);
						const bool wasBroadcast = localHost.BroadcastMessageToRemoteClients(
							queuedMessage.m_messageTypeIdentifier,
							queuedMessage.m_channel,
							Move(queuedMessage.m_encodedMessageBuffer)
						);
						Assert(wasBroadcast);
					}
					break;
					case QueuedMessage::Type::HostToClient:
					{
						Network::LocalHost& localHost = static_cast<Network::LocalHost&>(queuedMessage.m_peer);
						const bool wasBroadcast = localHost.SendMessageTo(
							queuedMessage.m_clientIdentifier,
							queuedMessage.m_messageTypeIdentifier,
							queuedMessage.m_channel,
							Move(queuedMessage.m_encodedMessageBuffer)
						);
						Assert(wasBroadcast);
					}
					break;
					case QueuedMessage::Type::HostToOtherClients:
					{
						Network::LocalHost& localHost = static_cast<Network::LocalHost&>(queuedMessage.m_peer);
						const bool wasBroadcast = localHost.BroadcastMessageToOtherClients(
							queuedMessage.m_clientIdentifier,
							queuedMessage.m_messageTypeIdentifier,
							queuedMessage.m_channel,
							Move(queuedMessage.m_encodedMessageBuffer)
						);
						Assert(wasBroadcast);
					}
					break;
					case QueuedMessage::Type::ClientToHost:
					{
						Network::LocalClient& localClient = static_cast<Network::LocalClient&>(queuedMessage.m_peer);
						const bool wasBroadcast = localClient.SendMessageToHost(
							queuedMessage.m_messageTypeIdentifier,
							queuedMessage.m_channel,
							Move(queuedMessage.m_encodedMessageBuffer)
						);
						Assert(wasBroadcast);
					}
					break;
				}
			}
		}
	}

	Optional<Network::LocalHost*> BoundComponent::GetLocalHost(const ParentType& owner, Entity::SceneRegistry& sceneRegistry) const
	{
		const Entity::HierarchyComponentBase& rootParent = owner.GetRootParent();
		if (const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(rootParent, sceneRegistry);
		    pLocalHostComponent.IsValid() && pLocalHostComponent->IsStarted())
		{
			return pLocalHostComponent->m_localHost;
		}
		return Invalid;
	}

	Optional<Network::LocalClient*> BoundComponent::GetLocalClient(const ParentType& owner, Entity::SceneRegistry& sceneRegistry) const
	{
		if (const Entity::DataComponentResult<Network::Session::LocalClient> dataComponentQueryResult = owner.FindFirstDataComponentOfTypeInParents<Network::Session::LocalClient>(sceneRegistry))
		{
			return dataComponentQueryResult.m_pDataComponent->m_localClient;
		}
		return Invalid;
	}

	Optional<Entity::HierarchyComponentBase*> BoundComponent::SpawnChildOnAllClients(
		ParentType& owner, Entity::SceneRegistry& sceneRegistry, Entity::ComponentTypeSceneDataInterface& childComponentTypeSceneData
	)
	{
		// Make sure we are the server
		const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(owner, sceneRegistry);
		Assert(pLocalHostComponent.IsValid(), "Must be host to spawn components over network!");
		Assert(pLocalHostComponent->IsStarted(), "Host must be active");
		if (UNLIKELY(pLocalHostComponent.IsInvalid() || !pLocalHostComponent->IsStarted()))
		{
			return Invalid;
		}

		const Entity::ComponentTypeInterface& componentType = childComponentTypeSceneData.GetTypeInterface();
		const Reflection::TypeInterface& typeInterface = componentType.GetTypeInterface();

		ParentType::DynamicInitializer initializer{owner, sceneRegistry};
		const Optional<Entity::Component*> pLocalComponent = childComponentTypeSceneData.CreateInstanceDynamic(initializer);
		Assert(pLocalComponent.IsValid());
		if (LIKELY(pLocalComponent.IsValid()))
		{
			Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData =
				*sceneRegistry.FindComponentTypeData<Entity::Data::InstanceGuid>();

			Entity::HierarchyComponentBase& component = static_cast<Entity::HierarchyComponentBase&>(*pLocalComponent);

			const Optional<Entity::Data::InstanceGuid*> pInstanceGuid = instanceGuidSceneData.GetComponentImplementation(component.GetIdentifier()
			);
			Network::Channel channel{0};
			pLocalHostComponent->m_localHost.BroadcastMessageToAllClients<&BoundComponent::OnClientSpawnChild>(
				m_boundObjectIdentifier,
				channel,
				ClientToServerDataSpawnChildData{typeInterface.GetGuid(), pInstanceGuid.IsValid() ? pInstanceGuid->GetGuid() : Guid::Generate()}
			);
			return component;
		}
		else
		{
			return Invalid;
		}
	}

	Optional<Entity::HierarchyComponentBase*> BoundComponent::SpawnChildOnClient(
		ParentType& owner,
		Entity::SceneRegistry& sceneRegistry,
		const ClientIdentifier clientIdentifier,
		Entity::ComponentTypeSceneDataInterface& childComponentTypeSceneData
	)
	{
		Entity::HierarchyComponentBase& rootParent = owner.GetRootParent();
		// Make sure we are the server
		const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(rootParent, sceneRegistry);
		Assert(pLocalHostComponent.IsValid(), "Must be host to spawn components over network!");
		Assert(pLocalHostComponent->IsStarted(), "Host must be active");
		if (UNLIKELY(pLocalHostComponent.IsInvalid() || !pLocalHostComponent->IsStarted()))
		{
			return Invalid;
		}

		const Entity::ComponentTypeInterface& componentType = childComponentTypeSceneData.GetTypeInterface();
		const Reflection::TypeInterface& typeInterface = componentType.GetTypeInterface();

		ParentType::DynamicInitializer initializer{owner, sceneRegistry};
		const Optional<Entity::Component*> pLocalComponent = childComponentTypeSceneData.CreateInstanceDynamic(initializer);
		Assert(pLocalComponent.IsValid());
		if (LIKELY(pLocalComponent.IsValid()))
		{
			Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData =
				*sceneRegistry.FindComponentTypeData<Entity::Data::InstanceGuid>();

			Entity::HierarchyComponentBase& component = static_cast<Entity::HierarchyComponentBase&>(*pLocalComponent);

			const Optional<Entity::Data::InstanceGuid*> pInstanceGuid = instanceGuidSceneData.GetComponentImplementation(component.GetIdentifier()
			);
			Network::Channel channel{0};
			pLocalHostComponent->m_localHost.SendMessageTo<&BoundComponent::OnClientSpawnChild>(
				clientIdentifier,
				m_boundObjectIdentifier,
				channel,
				ClientToServerDataSpawnChildData{typeInterface.GetGuid(), pInstanceGuid.IsValid() ? pInstanceGuid->GetGuid() : Guid::Generate()}
			);
			return component;
		}
		else
		{
			return Invalid;
		}
	}

	void BoundComponent::OnClientSpawnChild(
		Entity::HierarchyComponentBase& owner,
		Network::Session::BoundComponent&,
		Network::LocalClient&,
		const ClientToServerDataSpawnChildData data
	)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		const Entity::ComponentTypeIdentifier componentTypeIdentifier = sceneRegistry.FindComponentTypeIdentifier(data.m_componentTypeGuid);
		const Optional<Entity::ComponentTypeSceneDataInterface*> pChildComponentTypeSceneData =
			sceneRegistry.GetOrCreateComponentTypeData(componentTypeIdentifier);
		Assert(pChildComponentTypeSceneData.IsValid());
		if (LIKELY(pChildComponentTypeSceneData.IsValid()))
		{
			ParentType::DynamicInitializer initializer{owner, sceneRegistry};
			[[maybe_unused]] const Optional<Entity::Component*> pLocalComponent = pChildComponentTypeSceneData->CreateInstanceDynamic(initializer
			);
			Assert(pLocalComponent.IsValid());
		}
	}

	Optional<Entity::HierarchyComponentBase*> BoundComponent::SpawnBoundChildOnAllClients(
		ParentType& owner, Entity::SceneRegistry& sceneRegistry, Entity::ComponentTypeSceneDataInterface& childComponentTypeSceneData
	)
	{
		Entity::HierarchyComponentBase& rootParent = owner.GetRootParent();
		// Make sure we are the server
		const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(rootParent, sceneRegistry);
		Assert(pLocalHostComponent.IsValid(), "Must be host to spawn components over network!");
		Assert(pLocalHostComponent->IsStarted(), "Host must be active");
		if (UNLIKELY(pLocalHostComponent.IsInvalid() || !pLocalHostComponent->IsStarted()))
		{
			return Invalid;
		}

		const Entity::ComponentTypeInterface& componentType = childComponentTypeSceneData.GetTypeInterface();
		const Reflection::TypeInterface& typeInterface = componentType.GetTypeInterface();

		ParentType::DynamicInitializer initializer{owner, sceneRegistry};
		const Optional<Entity::Component*> pLocalComponent = childComponentTypeSceneData.CreateInstanceDynamic(initializer);
		Assert(pLocalComponent.IsValid());
		if (LIKELY(pLocalComponent.IsValid()))
		{
			Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData =
				*sceneRegistry.FindComponentTypeData<Entity::Data::InstanceGuid>();

			Entity::HierarchyComponentBase& component = static_cast<Entity::HierarchyComponentBase&>(*pLocalComponent);

			const Optional<Entity::Data::InstanceGuid*> pInstanceGuid = instanceGuidSceneData.GetComponentImplementation(component.GetIdentifier()
			);
			Assert(pInstanceGuid.IsValid(), "Component must have instance guid to bind to network!");
			if (LIKELY(pInstanceGuid.IsValid()))
			{
				const Guid instanceGuid = pInstanceGuid->GetGuid();
				BoundComponent& boundComponent =
					*component
						 .CreateDataComponent<BoundComponent>(sceneRegistry, component, sceneRegistry, pLocalHostComponent->m_localHost, instanceGuid);

				Network::Channel channel{0};
				pLocalHostComponent->m_localHost.BroadcastMessageToAllClients<&BoundComponent::OnClientSpawnBoundChild>(
					m_boundObjectIdentifier,
					channel,
					ClientToServerDataSpawnBoundChildData{typeInterface.GetGuid(), instanceGuid, boundComponent.GetIdentifier()}
				);
			}
			return component;
		}
		return Invalid;
	}

	Optional<Entity::HierarchyComponentBase*> BoundComponent::SpawnBoundChildOnClient(
		ParentType& owner,
		Entity::SceneRegistry& sceneRegistry,
		const ClientIdentifier clientIdentifier,
		Entity::ComponentTypeSceneDataInterface& childComponentTypeSceneData
	)
	{
		Entity::HierarchyComponentBase& rootParent = owner.GetRootParent();
		// Make sure we are the server
		const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(rootParent, sceneRegistry);
		Assert(pLocalHostComponent.IsValid(), "Must be host to spawn components over network!");
		Assert(pLocalHostComponent->IsStarted(), "Host must be active");
		if (UNLIKELY(pLocalHostComponent.IsInvalid() || !pLocalHostComponent->IsStarted()))
		{
			return Invalid;
		}

		const Entity::ComponentTypeInterface& componentType = childComponentTypeSceneData.GetTypeInterface();
		const Reflection::TypeInterface& typeInterface = componentType.GetTypeInterface();

		ParentType::DynamicInitializer initializer{owner, sceneRegistry};
		const Optional<Entity::Component*> pLocalComponent = childComponentTypeSceneData.CreateInstanceDynamic(initializer);
		Assert(pLocalComponent.IsValid());
		if (LIKELY(pLocalComponent.IsValid()))
		{
			Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData =
				*sceneRegistry.FindComponentTypeData<Entity::Data::InstanceGuid>();

			Entity::HierarchyComponentBase& component = static_cast<Entity::HierarchyComponentBase&>(*pLocalComponent);

			const Optional<Entity::Data::InstanceGuid*> pInstanceGuid = instanceGuidSceneData.GetComponentImplementation(component.GetIdentifier()
			);
			Assert(pInstanceGuid.IsValid(), "Component must have instance guid to bind to network!");
			if (LIKELY(pInstanceGuid.IsValid()))
			{
				const Guid instanceGuid = pInstanceGuid->GetGuid();
				BoundComponent& boundComponent =
					*component
						 .CreateDataComponent<BoundComponent>(sceneRegistry, component, sceneRegistry, pLocalHostComponent->m_localHost, instanceGuid);

				Network::Channel channel{0};
				pLocalHostComponent->m_localHost.SendMessageTo<&BoundComponent::OnClientSpawnBoundChild>(
					clientIdentifier,
					m_boundObjectIdentifier,
					channel,
					ClientToServerDataSpawnBoundChildData{typeInterface.GetGuid(), instanceGuid, boundComponent.GetIdentifier()}
				);
			}
			return component;
		}
		return Invalid;
	}

	void BoundComponent::OnClientSpawnBoundChild(
		Entity::HierarchyComponentBase& owner,
		Network::Session::BoundComponent&,
		Network::LocalClient& localClient,
		const ClientToServerDataSpawnBoundChildData data
	)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		const Entity::ComponentTypeIdentifier componentTypeIdentifier = sceneRegistry.FindComponentTypeIdentifier(data.m_componentTypeGuid);
		const Optional<Entity::ComponentTypeSceneDataInterface*> pChildComponentTypeSceneData =
			sceneRegistry.GetOrCreateComponentTypeData(componentTypeIdentifier);
		Assert(pChildComponentTypeSceneData.IsValid());
		if (LIKELY(pChildComponentTypeSceneData.IsValid()))
		{
			ParentType::DynamicInitializer initializer{owner, sceneRegistry};
			const Optional<Entity::Component*> pLocalComponent = pChildComponentTypeSceneData->CreateInstanceDynamic(initializer);
			Assert(pLocalComponent.IsValid());

			Entity::HierarchyComponentBase& component = static_cast<Entity::HierarchyComponentBase&>(*pLocalComponent);

			Entity::ComponentTypeSceneData<Entity::Data::InstanceGuid>& instanceGuidSceneData =
				*sceneRegistry.FindComponentTypeData<Entity::Data::InstanceGuid>();
			const Optional<Entity::Data::InstanceGuid*> pInstanceGuid = instanceGuidSceneData.GetComponentImplementation(component.GetIdentifier()
			);
			Assert(pInstanceGuid.IsValid(), "Component must have instance guid to bind to network!");
			if (LIKELY(pInstanceGuid.IsValid()))
			{
				pChildComponentTypeSceneData->OnInstanceGuidChanged(*pInstanceGuid, data.m_instanceGuid);
				*pInstanceGuid = data.m_instanceGuid;
			}

			Entity::ComponentTypeSceneData<BoundComponent>& boundComponentTypeSceneData = *sceneRegistry.FindComponentTypeData<BoundComponent>();
			[[maybe_unused]] const Optional<BoundComponent*> pBoundComponent =
				boundComponentTypeSceneData
					.CreateInstance(component.GetIdentifier(), component, component, sceneRegistry, localClient, data.m_boundObjectIdentifier);
			Assert(pBoundComponent.IsValid());
		}
	}

	Optional<Entity::Data::Component*> BoundComponent::AddDataComponent(
		ParentType& owner, Entity::SceneRegistry& sceneRegistry, Entity::ComponentTypeSceneDataInterface& dataComponentTypeSceneData
	)
	{
		Entity::HierarchyComponentBase& rootParent = owner.GetRootParent();
		// Make sure we are the server
		const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(rootParent, sceneRegistry);
		Assert(pLocalHostComponent.IsValid(), "Must be host to spawn components over network!");
		Assert(pLocalHostComponent->IsStarted(), "Host must be active");
		if (UNLIKELY(pLocalHostComponent.IsInvalid() || !pLocalHostComponent->IsStarted()))
		{
			return Invalid;
		}

		const Entity::ComponentTypeInterface& componentType = dataComponentTypeSceneData.GetTypeInterface();
		const Reflection::TypeInterface& typeInterface = componentType.GetTypeInterface();

		Entity::Data::Component::DynamicInitializer initializer{owner, sceneRegistry};
		const Optional<Entity::Component*> pLocalComponent = dataComponentTypeSceneData.CreateInstanceDynamic(initializer);
		Assert(pLocalComponent.IsValid());
		if (LIKELY(pLocalComponent.IsValid()))
		{
			Network::Channel channel{0};
			pLocalHostComponent->m_localHost.BroadcastMessageToAllClients<&BoundComponent::OnClientAddDataComponent>(
				m_boundObjectIdentifier,
				channel,
				ClientToServerDataAddDataComponentData{typeInterface.GetGuid()}
			);
			return static_cast<Entity::Data::Component&>(*pLocalComponent);
		}
		return Invalid;
	}

	Optional<Entity::Data::Component*> BoundComponent::AddDataComponent(
		ParentType& owner,
		Entity::SceneRegistry& sceneRegistry,
		const ClientIdentifier clientIdentifier,
		Entity::ComponentTypeSceneDataInterface& dataComponentTypeSceneData
	)
	{
		Entity::HierarchyComponentBase& rootParent = owner.GetRootParent();
		// Make sure we are the server
		const Optional<Session::LocalHost*> pLocalHostComponent = Session::LocalHost::Find(rootParent, sceneRegistry);
		Assert(pLocalHostComponent.IsValid(), "Must be host to spawn components over network!");
		Assert(pLocalHostComponent->IsStarted(), "Host must be active");
		if (UNLIKELY(pLocalHostComponent.IsInvalid() || !pLocalHostComponent->IsStarted()))
		{
			return Invalid;
		}

		const Entity::ComponentTypeInterface& componentType = dataComponentTypeSceneData.GetTypeInterface();
		const Reflection::TypeInterface& typeInterface = componentType.GetTypeInterface();

		Optional<Entity::Component*> pLocalComponent;
		if (owner.HasDataComponentOfType(sceneRegistry, dataComponentTypeSceneData.GetIdentifier()))
		{
			pLocalComponent = dataComponentTypeSceneData.GetDataComponent(owner.GetIdentifier());
		}
		else
		{
			Entity::Data::Component::DynamicInitializer initializer{owner, sceneRegistry};
			pLocalComponent = dataComponentTypeSceneData.CreateInstanceDynamic(initializer);
		}
		Assert(pLocalComponent.IsValid());
		if (LIKELY(pLocalComponent.IsValid()))
		{
			Network::Channel channel{0};
			pLocalHostComponent->m_localHost.SendMessageTo<&BoundComponent::OnClientAddDataComponent>(
				clientIdentifier,
				m_boundObjectIdentifier,
				channel,
				ClientToServerDataAddDataComponentData{typeInterface.GetGuid()}
			);
			return static_cast<Entity::Data::Component&>(*pLocalComponent);
		}
		return Invalid;
	}

	void BoundComponent::OnClientAddDataComponent(
		Entity::HierarchyComponentBase& owner,
		Network::Session::BoundComponent&,
		Network::LocalClient&,
		const ClientToServerDataAddDataComponentData data
	)
	{
		Entity::SceneRegistry& sceneRegistry = owner.GetSceneRegistry();
		const Entity::ComponentTypeIdentifier componentTypeIdentifier = sceneRegistry.FindComponentTypeIdentifier(data.m_componentTypeGuid);
		if (!sceneRegistry.HasDataComponentOfType(owner.GetIdentifier(), componentTypeIdentifier))
		{
			const Optional<Entity::ComponentTypeSceneDataInterface*> pDataComponentTypeSceneData =
				sceneRegistry.GetOrCreateComponentTypeData(componentTypeIdentifier);
			Assert(pDataComponentTypeSceneData.IsValid());
			if (LIKELY(pDataComponentTypeSceneData.IsValid()))
			{
				Entity::Data::Component::DynamicInitializer initializer{owner, sceneRegistry};
				[[maybe_unused]] const Optional<Entity::Component*> pLocalComponent = pDataComponentTypeSceneData->CreateInstanceDynamic(initializer
				);
				Assert(pLocalComponent.IsValid());
			}
		}
	}

	[[maybe_unused]] const bool wasBoundComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<BoundComponent>>::Make());
	[[maybe_unused]] const bool wasBoundComponentTypeRegistered = Reflection::Registry::RegisterType<BoundComponent>();
}
