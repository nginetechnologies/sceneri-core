#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>

#include <NetworkingCore/Components/BoundObjectIdentifier.h>
#include <NetworkingCore/Components/LocalHostComponent.h>
#include <NetworkingCore/Components/LocalClientComponent.h>
#include <NetworkingCore/Client/LocalClient.h>
#include <NetworkingCore/Host/LocalHost.h>
#include <NetworkingCore/Host/RemoteHost.h>

#include <Common/Storage/Identifier.h>
#include <Common/Memory/Containers/ForwardDeclarations/BitView.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Network/Address.h>
#include <Common/Threading/Mutexes/Mutex.h>

namespace ngine::Entity
{
	struct SceneRegistry;
	struct ComponentTypeSceneDataInterface;
	struct HierarchyComponentBase;

	namespace Data
	{
		struct Component;
	}
}

namespace ngine::Network::Session
{
	struct LocalClient;
	struct LocalHost;

	struct BoundComponent final : public Entity::Data::HierarchyComponent
	{
		using InstanceIdentifier = Network::BoundObjectIdentifier;

		using BaseType = Entity::Data::HierarchyComponent;

		struct Initializer
		{
			Initializer(DynamicInitializer&& dynamicInitializer)
				: m_parent(dynamicInitializer.GetParent())
				, m_sceneRegistry(dynamicInitializer.GetSceneRegistry())
			{
			}

			Initializer(Entity::HierarchyComponentBase& parent, Entity::SceneRegistry& sceneRegistry)
				: m_parent(parent)
				, m_sceneRegistry(sceneRegistry)
			{
			}

			[[nodiscard]] Entity::HierarchyComponentBase& GetParent() const
			{
				return m_parent;
			}
			[[nodiscard]] Entity::SceneRegistry& GetSceneRegistry() const
			{
				return m_sceneRegistry;
			}
		private:
			Entity::HierarchyComponentBase& m_parent;
			Entity::SceneRegistry& m_sceneRegistry;
		};

		BoundComponent(Initializer&& initializer);
		BoundComponent(const Deserializer&);
		BoundComponent(const BoundComponent&, const Cloner&);
		BoundComponent(const Network::BoundObjectIdentifier boundObjectIdentifier)
			: m_boundObjectIdentifier(boundObjectIdentifier)
		{
		}
		BoundComponent(
			Entity::HierarchyComponentBase& owner,
			Entity::SceneRegistry& sceneRegistry,
			const Network::BoundObjectIdentifier boundObjectIdentifier
		);
		BoundComponent(
			Entity::HierarchyComponentBase& owner,
			Entity::SceneRegistry& sceneRegistry,
			Network::LocalClient& localClient,
			const Network::BoundObjectIdentifier boundObjectIdentifier
		);
		BoundComponent(
			Entity::HierarchyComponentBase& owner, Entity::SceneRegistry& sceneRegistry, Network::LocalHost& localHost, const Guid instanceGuid
		);

		void OnDestroying(ParentType& owner);

		[[nodiscard]] bool IsBound() const
		{
			return m_boundObjectIdentifier.IsValid();
		}

		[[nodiscard]] Optional<Network::LocalHost*> GetLocalHost(const ParentType& owner, Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Optional<Network::LocalClient*> GetLocalClient(const ParentType& owner, Entity::SceneRegistry& sceneRegistry) const;

		[[nodiscard]] bool HasAuthority(const ParentType& owner, Entity::SceneRegistry& sceneRegistry) const
		{
			Assert(m_boundObjectIdentifier.IsValid());
			if (LIKELY(m_boundObjectIdentifier.IsValid()))
			{
				if (const Optional<Network::LocalHost*> pLocalHost = GetLocalHost(owner, sceneRegistry))
				{
					if (pLocalHost->HasAuthorityOfBoundObject(m_boundObjectIdentifier))
					{
						return true;
					}
				}

				if (const Optional<Network::LocalClient*> pLocalClient = GetLocalClient(owner, sceneRegistry))
				{
					if (pLocalClient->HasAuthorityOfBoundObject(m_boundObjectIdentifier))
					{
						return true;
					}
				}
			}

			return false;
		}

		bool DelegateAuthority(ParentType& owner, Entity::SceneRegistry& sceneRegistry, const ClientIdentifier clientIdentifier) const
		{
			Assert(m_boundObjectIdentifier.IsValid());
			if (LIKELY(m_boundObjectIdentifier.IsValid()))
			{
				if (const Optional<Network::LocalHost*> pLocalHost = GetLocalHost(owner, sceneRegistry))
				{
					pLocalHost->DelegateBoundObjectAuthority(m_boundObjectIdentifier, clientIdentifier);
					return true;
				}
			}
			return false;
		}
		bool RevokeAuthority(ParentType& owner, Entity::SceneRegistry& sceneRegistry) const
		{
			Assert(m_boundObjectIdentifier.IsValid());
			if (LIKELY(m_boundObjectIdentifier.IsValid()))
			{
				if (const Optional<Network::LocalHost*> pLocalHost = GetLocalHost(owner, sceneRegistry))
				{
					pLocalHost->RevokeBoundObjectAuthority(m_boundObjectIdentifier);
					return true;
				}
			}
			return false;
		}

		template<auto Function, typename... MessageTypes>
		bool BroadcastToAllClients(Network::LocalHost& localHost, const Channel channel, const MessageTypes&... messages) const
		{
			static_assert(Reflection::GetFunction<Function>().m_flags.IsSet(Reflection::FunctionFlags::HostToClient));

			const MessageTypeIdentifier messageTypeIdentifier = localHost.FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				EncodedMessageBuffer encodedMessageBuffer = localHost.EncodeMessageBuffer(
					messageTypeIdentifier,
					m_boundObjectIdentifier,
					localHost.AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...),
					messages...
				);
				if (m_boundObjectIdentifier.IsValid())
				{
					const bool wasBroadcast = localHost.BroadcastMessageToAllClients(messageTypeIdentifier, channel, Move(encodedMessageBuffer));
					Assert(wasBroadcast);
					return wasBroadcast;
				}
				else
				{
					Threading::UniqueLock lock(m_queuedMessagesMutex);
					m_queuedMessages.EmplaceBack(
						QueuedMessage{QueuedMessage::Type::HostToAllClients, localHost, messageTypeIdentifier, channel, Move(encodedMessageBuffer)}
					);
					return true;
				}
			}
			return false;
		}

		template<auto Function, typename... MessageTypes>
		bool BroadcastToRemoteClients(Network::LocalHost& localHost, const Channel channel, const MessageTypes&... messages) const
		{
			static_assert(Reflection::GetFunction<Function>().m_flags.IsSet(Reflection::FunctionFlags::HostToClient));

			const MessageTypeIdentifier messageTypeIdentifier = localHost.FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				EncodedMessageBuffer encodedMessageBuffer = localHost.EncodeMessageBuffer(
					messageTypeIdentifier,
					m_boundObjectIdentifier,
					localHost.AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...),
					messages...
				);
				if (m_boundObjectIdentifier.IsValid())
				{
					const bool wasBroadcast = localHost.BroadcastMessageToRemoteClients(messageTypeIdentifier, channel, Move(encodedMessageBuffer));
					Assert(wasBroadcast);
					return wasBroadcast;
				}
				else
				{
					Threading::UniqueLock lock(m_queuedMessagesMutex);
					m_queuedMessages.EmplaceBack(
						QueuedMessage{QueuedMessage::Type::HostToRemoteClients, localHost, messageTypeIdentifier, channel, Move(encodedMessageBuffer)}
					);
					return true;
				}
			}
			return false;
		}
		template<auto Function, typename... MessageTypes>
		bool BroadcastToRemoteClients(Network::LocalClient& localClient, const Channel channel, const MessageTypes&... messages) const
		{
			static_assert(Reflection::GetFunction<Function>().m_flags.IsSet(Reflection::FunctionFlags::ClientToClient));

			const MessageTypeIdentifier forwardMessageTypeIdentifier = MessageTypeIdentifier::MakeFromValidIndex((MessageTypeIdentifier::IndexType
			)DefaultMessageType::RequestForwardMessageToOtherClients);
			const MessageTypeIdentifier messageTypeIdentifier = localClient.FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				EncodedMessageBuffer encodedMessageBuffer = localClient.EncodeForwardedMessageBuffer<MessageTypes...>(
					forwardMessageTypeIdentifier,
					messageTypeIdentifier,
					m_boundObjectIdentifier,
					MessageTypeFlags::ClientToClient,
					messages...
				);
				if (LIKELY(encodedMessageBuffer.IsValid()))
				{
					if (m_boundObjectIdentifier.IsValid())
					{
						const bool wasBroadcast = localClient.SendMessageToHost(forwardMessageTypeIdentifier, channel, Move(encodedMessageBuffer));
						Assert(wasBroadcast);
						return wasBroadcast;
					}
					else
					{
						Threading::UniqueLock lock(m_queuedMessagesMutex);
						m_queuedMessages.EmplaceBack(QueuedMessage{
							QueuedMessage::Type::ClientToHost,
							localClient,
							forwardMessageTypeIdentifier,
							channel,
							Move(encodedMessageBuffer)
						});
						return true;
					}
				}
			}
			return false;
		}

		template<auto Function, typename... MessageTypes>
		bool BroadcastToAllRemotes(Network::LocalHost& localHost, const Channel channel, const MessageTypes&... messages) const
		{
			const bool wasBroadcast = BroadcastToAllClients<Function, MessageTypes...>(localHost, channel, messages...);
			Assert(wasBroadcast);
			return wasBroadcast;
		}
		template<auto Function, typename... MessageTypes>
		bool BroadcastToAllRemotes(Network::LocalClient& localClient, const Channel channel, const MessageTypes&... messages) const
		{
			static_assert(Reflection::GetFunction<Function>().m_flags.IsSet(Reflection::FunctionFlags::ClientToClient));

			const MessageTypeIdentifier forwardMessageTypeIdentifier = MessageTypeIdentifier::MakeFromValidIndex((MessageTypeIdentifier::IndexType
			)DefaultMessageType::RequestForwardMessageToAllRemotes);
			const MessageTypeIdentifier messageTypeIdentifier = localClient.FindMessageIdentifier<Function>();
			EncodedMessageBuffer encodedMessageBuffer = localClient.EncodeForwardedMessageBuffer<MessageTypes...>(
				forwardMessageTypeIdentifier,
				messageTypeIdentifier,
				m_boundObjectIdentifier,
				MessageTypeFlags::ClientToClient | MessageTypeFlags::ClientToHost,
				messages...
			);
			if (LIKELY(encodedMessageBuffer.IsValid()))
			{
				if (m_boundObjectIdentifier.IsValid())
				{
					const bool wasBroadcast = localClient.SendMessageToHost(forwardMessageTypeIdentifier, channel, Move(encodedMessageBuffer));
					Assert(wasBroadcast);
					return wasBroadcast;
				}
				else
				{
					Threading::UniqueLock lock(m_queuedMessagesMutex);
					m_queuedMessages.EmplaceBack(
						QueuedMessage{QueuedMessage::Type::ClientToHost, localClient, forwardMessageTypeIdentifier, channel, Move(encodedMessageBuffer)}
					);
					return true;
				}
			}
			return false;
		}

		template<auto Function, typename... MessageTypes>
		bool SendMessageToClient(
			Network::LocalHost& localHost, const ClientIdentifier clientIdentifier, const Channel channel, const MessageTypes&... messages
		) const
		{
			static_assert(Reflection::GetFunction<Function>().m_flags.IsSet(Reflection::FunctionFlags::HostToClient));

			const MessageTypeIdentifier messageTypeIdentifier = localHost.FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				EncodedMessageBuffer encodedMessageBuffer = localHost.EncodeMessageBuffer(
					messageTypeIdentifier,
					m_boundObjectIdentifier,
					localHost.AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...),
					messages...
				);
				if (m_boundObjectIdentifier.IsValid())
				{
					const bool wasSent = localHost.SendMessageTo(clientIdentifier, messageTypeIdentifier, channel, Move(encodedMessageBuffer));
					Assert(wasSent);
					return wasSent;
				}
				else
				{
					Threading::UniqueLock lock(m_queuedMessagesMutex);
					m_queuedMessages.EmplaceBack(QueuedMessage{
						QueuedMessage::Type::HostToClient,
						localHost,
						messageTypeIdentifier,
						channel,
						Move(encodedMessageBuffer),
						clientIdentifier
					});
					return true;
				}
			}
			return false;
		}

		template<auto Function, typename... MessageTypes>
		bool SendMessageToHost(Network::LocalClient& localClient, const Channel channel, const MessageTypes&... messages) const
		{
			static_assert(Reflection::GetFunction<Function>().m_flags.IsSet(Reflection::FunctionFlags::ClientToHost));

			const MessageTypeIdentifier messageTypeIdentifier = localClient.FindMessageIdentifier<Function>();
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				EncodedMessageBuffer encodedMessageBuffer = localClient.EncodeMessageBuffer(
					messageTypeIdentifier,
					m_boundObjectIdentifier,
					localClient.AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...),
					messages...
				);
				if (m_boundObjectIdentifier.IsValid())
				{
					const bool wasSent = localClient.SendMessageToHost(messageTypeIdentifier, channel, Move(encodedMessageBuffer));
					Assert(wasSent);
					return wasSent;
				}
				else
				{
					Threading::UniqueLock lock(m_queuedMessagesMutex);
					m_queuedMessages.EmplaceBack(
						QueuedMessage{QueuedMessage::Type::ClientToHost, localClient, messageTypeIdentifier, channel, Move(encodedMessageBuffer)}
					);
					return true;
				}
			}
			return false;
		}

		template<auto Function, typename... MessageTypes>
		bool BroadcastToAllClients(
			ParentType& owner, Entity::SceneRegistry& sceneRegistry, const Channel channel, const MessageTypes&... messages
		) const
		{
			constexpr EnumFlags<Reflection::FunctionFlags> functionFlags = Reflection::GetFunction<Function>().m_flags;
			static_assert(functionFlags.AreAnySet(Reflection::FunctionFlags::ClientToClient | Reflection::FunctionFlags::HostToClient));

			if constexpr (functionFlags.IsSet(Reflection::FunctionFlags::ClientToClient))
			{
				if (const Optional<Network::LocalClient*> pLocalClient = GetLocalClient(owner, sceneRegistry))
				{
					const bool wasBroadcast = BroadcastToRemoteClients<Function, MessageTypes...>(*pLocalClient, channel, messages...);
					Assert(wasBroadcast);
					// Assert(false, "TODO: Handle locally?");
					return wasBroadcast;
				}
			}

			if constexpr (functionFlags.IsSet(Reflection::FunctionFlags::HostToClient))
			{
				if (const Optional<Network::LocalHost*> pLocalHost = GetLocalHost(owner, sceneRegistry))
				{
					const bool wasBroadcast = BroadcastToAllClients<Function, MessageTypes...>(*pLocalHost, channel, messages...);
					Assert(wasBroadcast);
					return wasBroadcast;
				}
			}
			return false;
		}

		template<auto Function, typename... MessageTypes>
		bool BroadcastToAllRemotes(
			ParentType& owner, Entity::SceneRegistry& sceneRegistry, const Channel channel, const MessageTypes&... messages
		) const
		{
			const Optional<Network::LocalClient*> pLocalClient = GetLocalClient(owner, sceneRegistry);
			const Optional<Network::LocalHost*> pLocalHost = GetLocalHost(owner, sceneRegistry);

			constexpr EnumFlags<Reflection::FunctionFlags> functionFlags = Reflection::GetFunction<Function>().m_flags;
			static_assert(functionFlags.AreAnySet(Reflection::FunctionFlags::ClientToClient | Reflection::FunctionFlags::HostToClient));

			if (pLocalClient.IsValid())
			{
				if constexpr (functionFlags.IsSet(Reflection::FunctionFlags::HostToClient))
				{
					if (pLocalHost.IsValid())
					{
						const MessageTypeIdentifier messageTypeIdentifier = pLocalHost->FindMessageIdentifier<Function>();
						EncodedMessageBuffer encodedMessageBuffer = pLocalHost->EncodeMessageBuffer(
							messageTypeIdentifier,
							m_boundObjectIdentifier,
							pLocalHost->AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...),
							messages...
						);
						if (m_boundObjectIdentifier.IsValid())
						{
							const bool wasBroadcast = pLocalHost->BroadcastMessageToOtherClients(
								pLocalClient->GetIdentifier(),
								messageTypeIdentifier,
								channel,
								Move(encodedMessageBuffer)
							);
							Assert(wasBroadcast);
							return wasBroadcast;
						}
						else
						{
							Threading::UniqueLock lock(m_queuedMessagesMutex);
							m_queuedMessages.EmplaceBack(QueuedMessage{
								QueuedMessage::Type::HostToOtherClients,
								*pLocalHost,
								messageTypeIdentifier,
								channel,
								Move(encodedMessageBuffer),
								pLocalClient->GetIdentifier()
							});
							return true;
						}
					}
				}

				if constexpr (functionFlags.IsSet(Reflection::FunctionFlags::ClientToClient))
				{
					return BroadcastToAllRemotes<Function, MessageTypes...>(*pLocalClient, channel, messages...);
				}
			}
			else if (pLocalHost.IsValid())
			{
				if constexpr (functionFlags.IsSet(Reflection::FunctionFlags::HostToClient))
				{
					const bool wasBroadcast = BroadcastToAllClients<Function, MessageTypes...>(*pLocalHost, channel, messages...);
					Assert(wasBroadcast);
					return wasBroadcast;
				}
			}
			return false;
		}

		template<auto Function, typename... MessageTypes>
		bool SendMessageToClient(
			ParentType& owner,
			Entity::SceneRegistry& sceneRegistry,
			const ClientIdentifier clientIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		) const
		{
			static_assert(Reflection::GetFunction<Function>().m_flags.IsSet(Reflection::FunctionFlags::HostToClient));

			const Optional<Network::LocalHost*> pLocalHost = GetLocalHost(owner, sceneRegistry);
			Assert(pLocalHost.IsValid());
			if (LIKELY(pLocalHost.IsValid()))
			{
				const bool wasSent = SendMessageToClient<Function, MessageTypes...>(*pLocalHost, clientIdentifier, channel, messages...);
				Assert(wasSent);
				return wasSent;
			}
			return false;
		}
		template<auto Function, typename... MessageTypes>
		bool
		SendMessageToHost(ParentType& owner, Entity::SceneRegistry& sceneRegistry, const Channel channel, const MessageTypes&... messages) const
		{
			static_assert(Reflection::GetFunction<Function>().m_flags.IsSet(Reflection::FunctionFlags::ClientToHost));

			const Optional<Network::LocalClient*> pLocalClient = GetLocalClient(owner, sceneRegistry);
			Assert(pLocalClient.IsValid());
			if (LIKELY(pLocalClient.IsValid()))
			{
				const bool wasSent = SendMessageToHost<Function, MessageTypes...>(*pLocalClient, channel, messages...);
				Assert(wasSent);
				return wasSent;
			}
			return false;
		}

		template<auto... Members>
		bool InvalidateProperties(Network::LocalClient& localClient) const
		{
			Assert(m_boundObjectIdentifier.IsValid());
			if (LIKELY(m_boundObjectIdentifier.IsValid()))
			{
				const bool wasInvalidated = localClient.InvalidateProperties<Members...>(m_boundObjectIdentifier);
				Assert(wasInvalidated);
				return wasInvalidated;
			}
			else
			{
				return false;
			}
		}
		template<auto... Members>
		bool InvalidateProperties(Network::LocalHost& localHost) const
		{
			Assert(m_boundObjectIdentifier.IsValid());
			if (LIKELY(m_boundObjectIdentifier.IsValid()))
			{
				const bool wasInvalidated = localHost.InvalidatePropertiesToAllClients<Members...>(m_boundObjectIdentifier);
				Assert(wasInvalidated);
				return wasInvalidated;
			}
			else
			{
				return false;
			}
		}
		template<auto... Members>
		bool InvalidateProperties(ParentType& owner, Entity::SceneRegistry& sceneRegistry) const
		{
			Assert(m_boundObjectIdentifier.IsValid());
			if (LIKELY(m_boundObjectIdentifier.IsValid()))
			{
				if (const Optional<Network::LocalClient*> pLocalClient = GetLocalClient(owner, sceneRegistry))
				{
					if (pLocalClient->HasAuthorityOfBoundObject(m_boundObjectIdentifier))
					{
						const bool wasInvalidated = pLocalClient->InvalidateProperties<Members...>(m_boundObjectIdentifier);
						Assert(wasInvalidated);
						return wasInvalidated;
					}
				}
				if (const Optional<Network::LocalHost*> pLocalHost = GetLocalHost(owner, sceneRegistry))
				{
					if (pLocalHost->HasAuthorityOfBoundObject(m_boundObjectIdentifier))
					{
						const bool wasInvalidated = pLocalHost->InvalidatePropertiesToAllClients<Members...>(m_boundObjectIdentifier);
						Assert(wasInvalidated);
						return wasInvalidated;
					}
				}
				Assert(false, "Failed to invalidate properties");
			}
			return false;
		}

		template<auto... Members>
		bool FlushProperties(Network::LocalClient& localClient) const
		{
			Assert(m_boundObjectIdentifier.IsValid());
			if (LIKELY(m_boundObjectIdentifier.IsValid()))
			{
				const bool wasInvalidated = localClient.FlushProperties<Members...>();
				Assert(wasInvalidated);
				return wasInvalidated;
			}
			else
			{
				return false;
			}
		}
		template<auto... Members>
		bool FlushProperties(Network::LocalHost& localHost) const
		{
			Assert(m_boundObjectIdentifier.IsValid());
			if (LIKELY(m_boundObjectIdentifier.IsValid()))
			{
				const bool wasInvalidated = localHost.FlushPropertiesToAllClients<Members...>();
				Assert(wasInvalidated);
				return wasInvalidated;
			}
			else
			{
				return false;
			}
		}
		template<auto... Members>
		bool FlushProperties(ParentType& owner, Entity::SceneRegistry& sceneRegistry) const
		{
			if (const Optional<Network::LocalClient*> pLocalClient = GetLocalClient(owner, sceneRegistry))
			{
				const bool wasInvalidated = pLocalClient->FlushProperties<Members...>();
				Assert(wasInvalidated);
				return wasInvalidated;
			}
			if (const Optional<Network::LocalHost*> pLocalHost = GetLocalHost(owner, sceneRegistry))
			{
				const bool wasInvalidated = pLocalHost->FlushPropertiesToAllClients<Members...>();
				Assert(wasInvalidated);
				return wasInvalidated;
			}
			return false;
		}

		[[nodiscard]] BoundObjectIdentifier GetIdentifier() const
		{
			return m_boundObjectIdentifier;
		}

		//! Spawns a child and attaches it to our parent / owner.
		//! Ensures that the child is spawned on all remote clients
		Optional<Entity::HierarchyComponentBase*> SpawnChildOnAllClients(
			ParentType& owner, Entity::SceneRegistry& sceneRegistry, Entity::ComponentTypeSceneDataInterface& childComponentTypeSceneData
		);
		//! Spawns a child and attaches it to our parent / owner.
		//! Ensures that the child is spawned on the specified client
		Optional<Entity::HierarchyComponentBase*> SpawnChildOnClient(
			ParentType& owner,
			Entity::SceneRegistry& sceneRegistry,
			const ClientIdentifier clientIdentifier,
			Entity::ComponentTypeSceneDataInterface& childComponentTypeSceneData
		);
		//! Spawns a child and attaches it to our parent / owner.
		//! Ensures that the child is spawned on all remote clients
		//! Automatically binds the spawned child to the network (aka attaches a BoundComponent to it)
		Optional<Entity::HierarchyComponentBase*> SpawnBoundChildOnAllClients(
			ParentType& owner, Entity::SceneRegistry& sceneRegistry, Entity::ComponentTypeSceneDataInterface& childComponentTypeSceneData
		);
		//! Spawns a child and attaches it to our parent / owner.
		//! Ensures that the child is spawned on the specified client
		//! Automatically binds the spawned child to the network (aka attaches a BoundComponent to it)
		Optional<Entity::HierarchyComponentBase*> SpawnBoundChildOnClient(
			ParentType& owner,
			Entity::SceneRegistry& sceneRegistry,
			const ClientIdentifier clientIdentifier,
			Entity::ComponentTypeSceneDataInterface& childComponentTypeSceneData
		);

		//! Adds a data component on our parent / owner
		//! Ensures that the component is added on all remote clients
		Optional<Entity::Data::Component*> AddDataComponent(
			ParentType& owner, Entity::SceneRegistry& sceneRegistry, Entity::ComponentTypeSceneDataInterface& dataComponentTypeSceneData
		);
		//! Adds a data component on our parent / owner
		//! Ensures that the component is added on the specified client
		Optional<Entity::Data::Component*> AddDataComponent(
			ParentType& owner,
			Entity::SceneRegistry& sceneRegistry,
			const ClientIdentifier clientIdentifier,
			Entity::ComponentTypeSceneDataInterface& dataComponentTypeSceneData
		);
	private:
		friend struct Reflection::ReflectedType<BoundComponent>;

		void OnAssignedBoundObjectIdentifier(const Network::BoundObjectIdentifier boundObjectIdentifier);

		struct ClientToServerDataSpawnChildData
		{
			// TODO: Bind component types to network so we can send a uint16 instead
			Guid m_componentTypeGuid;
			Guid m_instanceGuid;
		};
		void OnClientSpawnChild(
			Entity::HierarchyComponentBase& owner,
			Network::Session::BoundComponent& boundComponent,
			Network::LocalClient& localClient,
			const ClientToServerDataSpawnChildData data
		);

		struct ClientToServerDataSpawnBoundChildData
		{
			// TODO: Bind component types to network so we can send a uint16 instead
			Guid m_componentTypeGuid;
			Guid m_instanceGuid;
			BoundObjectIdentifier m_boundObjectIdentifier;
		};
		friend struct Reflection::ReflectedType<BoundComponent::ClientToServerDataSpawnBoundChildData>;
		void OnClientSpawnBoundChild(
			Entity::HierarchyComponentBase& owner,
			Network::Session::BoundComponent& boundComponent,
			Network::LocalClient& localClient,
			const ClientToServerDataSpawnBoundChildData data
		);

		struct ClientToServerDataAddDataComponentData
		{
			// TODO: Bind component types to network so we can send a uint16 instead
			Guid m_componentTypeGuid;
		};
		void OnClientAddDataComponent(
			Entity::HierarchyComponentBase& owner,
			Network::Session::BoundComponent& boundComponent,
			Network::LocalClient& localClient,
			const ClientToServerDataAddDataComponentData data
		);
	private:
		Network::BoundObjectIdentifier m_boundObjectIdentifier;

		struct QueuedMessage
		{
			enum class Type : uint8
			{
				HostToAllClients,
				HostToRemoteClients,
				HostToClient,
				HostToOtherClients,
				ClientToHost
			};

			Type m_type;
			Network::LocalPeer& m_peer;
			MessageTypeIdentifier m_messageTypeIdentifier;
			Channel m_channel;
			EncodedMessageBuffer m_encodedMessageBuffer;
			ClientIdentifier m_clientIdentifier;
		};

		mutable Threading::Mutex m_queuedMessagesMutex;
		mutable Vector<QueuedMessage> m_queuedMessages;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::Session::BoundComponent::ClientToServerDataSpawnBoundChildData>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::Session::BoundComponent::ClientToServerDataSpawnBoundChildData>(
			"{3A1EE416-034A-4BC5-AFD9-25719861942B}"_guid,
			MAKE_UNICODE_LITERAL("Client to Server Spawn Bound Child Message"),
			TypeFlags{},
			Tags{},
			Properties{
				Property{
					MAKE_UNICODE_LITERAL("Component Type Guid"),
					"componentTypeGuid",
					"{6F15FBC7-F603-48D2-9652-CA73FA094BBE}"_guid,
					MAKE_UNICODE_LITERAL("ClientToServerDataSpawnBoundChildData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&Network::Session::BoundComponent::ClientToServerDataSpawnBoundChildData::m_componentTypeGuid
				},
				Property{
					MAKE_UNICODE_LITERAL("Instance Guid"),
					"instanceGuid",
					"{303D19E9-7D3F-4314-A973-AEB7C6C7DE9D}"_guid,
					MAKE_UNICODE_LITERAL("ClientToServerDataSpawnBoundChildData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&Network::Session::BoundComponent::ClientToServerDataSpawnBoundChildData::m_instanceGuid
				},
				Property{
					MAKE_UNICODE_LITERAL("Bound Object Identifier"),
					"boundObjectIdentifier",
					"{D9C3381D-57A3-4A25-9C76-CB390397CF6A}"_guid,
					MAKE_UNICODE_LITERAL("ClientToServerDataSpawnBoundChildData"),
					PropertyFlags::SentWithNetworkedFunctions,
					&Network::Session::BoundComponent::ClientToServerDataSpawnBoundChildData::m_boundObjectIdentifier
				}
			}
		);
	};

	template<>
	struct ReflectedType<Network::Session::BoundComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::Session::BoundComponent>(
			"de693274-60b1-4973-a474-9bfd21432dd4"_guid,
			MAKE_UNICODE_LITERAL("Session Bound Component"),
			TypeFlags{},
			Tags{},
			Properties{},
			Functions{
				Function{
					"A396B39F-8C4E-40E7-BD15-B918DE79B4E7"_guid,
					MAKE_UNICODE_LITERAL("Spawn Component on Client"),
					&Network::Session::BoundComponent::OnClientSpawnChild,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("remoteHost")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("channel")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"{A5A41750-8B04-41FE-AF40-52BC0E653C8C}"_guid,
					MAKE_UNICODE_LITERAL("Spawn Component on Client"),
					&Network::Session::BoundComponent::OnClientSpawnBoundChild,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("remoteHost")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("channel")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"{EE4559BE-F8E4-4B43-8740-0AD992394B65}"_guid,
					MAKE_UNICODE_LITERAL("Add Data Component on Client"),
					&Network::Session::BoundComponent::OnClientAddDataComponent,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("remoteHost")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("channel")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				}
			}
		);
	};
}
