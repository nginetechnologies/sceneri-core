#include "Plugin.h"

#include <enet/enet.h>

#include <Engine/Engine.h>
#include <Engine/Event/Identifier.h>
#include <Engine/Entity/HierarchyComponentBase.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Memory/Containers/ByteView.h>
#include <Common/IO/URI.h>
#include <Common/IO/Log.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Memory/Compression/Bitset.h>
#include <Common/System/Query.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Network/Address.h>
#include <Common/Math/Frequency.h>

#include "NetworkingCore/Client/LocalClient.h"
#include "NetworkingCore/Host/LocalHost.h"
#include "NetworkingCore/Components/BoundComponent.h"

namespace ngine::Network
{
	Manager::Manager(Application&)
	{
		static ENetCallbacks callbacks{
			Memory::Allocate,
			Memory::Deallocate,
			[]()
			{
			}
		};
		[[maybe_unused]] const int result = enet_initialize_with_callbacks(ENET_VERSION, &callbacks);
		Assert(result == 0);
	}

	void Manager::OnLoaded([[maybe_unused]] Application& application)
	{
	}

	Manager::~Manager()
	{
		enet_deinitialize();
	}
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::LocalHost>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::LocalHost>(
			"{EC30B93F-69AE-4F71-BCF3-4B0FE61A0FDA}"_guid,
			MAKE_UNICODE_LITERAL("Local Host"),
			TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicDeserialization,
			Tags{},
			Properties{},
			Functions{
				Function{
					"{55478017-F3E4-4A11-8D1E-384D42528505}"_guid,
					MAKE_UNICODE_LITERAL("On Received Confirm Property Receipt"),
					&Network::LocalHost::OnReceivedConfirmPropertyReceipt,
					FunctionFlags::ClientToHost,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Client")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				},
				Function{
					"{BFF004C5-2FC0-44C0-8DF2-B3A64D53D49A}"_guid,
					MAKE_UNICODE_LITERAL("On Received Forward Request To Other Clients"),
					&Network::LocalHost::OnReceivedForwardRequestToOtherClients,
					FunctionFlags::ClientToHost,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Client")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				},
				Function{
					"{CB1788C7-D6A5-4B2B-BAC8-3F7BF9DDCBBF}"_guid,
					MAKE_UNICODE_LITERAL("On Received Forward Request To All Remotes"),
					&Network::LocalHost::OnReceivedForwardRequestToAllRemotes,
					FunctionFlags::ClientToHost,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Client")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				},
				Function{
					"{0af9c6bd-b196-472d-a9f0-835a39911839}"_guid,
					MAKE_UNICODE_LITERAL("On Received Time Sync"),
					&Network::LocalHost::OnReceivedTimeSyncRequest,
					FunctionFlags::ClientToHost,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Client")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				}
			}
		);
	};

	template<>
	struct ReflectedType<Network::LocalClient>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::LocalClient>(
			"6e724a13-4629-4582-a772-3cad39d5b34f"_guid,
			MAKE_UNICODE_LITERAL("Local Client"),
			TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicDeserialization,
			Tags{},
			Properties{},
			Functions{
				Function{
					"ff2ebad7-56bb-49df-bbbd-6f04f09d22c0"_guid,
					MAKE_UNICODE_LITERAL("On Connected to Server"),
					&Network::LocalClient::OnReceivedConnectedMessage,
					FunctionFlags::HostToClient,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Host")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				},
				Function{
					"a77bbf20-1a65-4539-9c24-d1d7cde3bf56"_guid,
					MAKE_UNICODE_LITERAL("On Register New Message Type"),
					&Network::LocalClient::OnReceivedNewMessageType,
					FunctionFlags::HostToClient,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Host")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				},
				Function{
					"0CDBAD12-A656-4EC1-B2A4-3FE3E0A348F1"_guid,
					MAKE_UNICODE_LITERAL("On Register Property Stream Message Type"),
					&Network::LocalClient::OnReceivedNewPropertyStreamMessageType,
					FunctionFlags::HostToClient,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Host")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				},
				Function{
					"4dff2ae9-4e79-4969-88f2-d0eb1d272b69"_guid,
					MAKE_UNICODE_LITERAL("On Receive Batch Message"),
					&Network::LocalClient::OnReceivedBatchMessageType,
					FunctionFlags::HostToClient,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Host")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				},
				Function{
					"93338599-55d9-44ba-8baa-844cb788e996"_guid,
					MAKE_UNICODE_LITERAL("On Object Bound"),
					&Network::LocalClient::OnReceivedNewBoundObject,
					FunctionFlags::HostToClient,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Host")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				},
				Function{
					"{A7E5D6D1-56ED-45B5-91F5-74CEB108B24E}"_guid,
					MAKE_UNICODE_LITERAL("On Received Confirm Property Receipt"),
					&Network::LocalClient::OnReceivedConfirmPropertyReceipt,
					FunctionFlags::HostToClient,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Host")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				},
				Function{
					"{0F0B25A9-E29F-439D-BC3A-FD012959598A}"_guid,
					MAKE_UNICODE_LITERAL("On Bound Object Authority Given"),
					&Network::LocalClient::OnBoundObjectAuthorityGiven,
					FunctionFlags::HostToClient,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Host")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				},
				Function{
					"{D668316B-2B84-4554-8753-895FA9995739}"_guid,
					MAKE_UNICODE_LITERAL("On Bound Object Authority Revoked"),
					&Network::LocalClient::OnBoundObjectAuthorityRevoked,
					FunctionFlags::HostToClient,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Host")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				},
				Function{
					"{54FA0C12-BFB2-454E-8A5F-B7001521E818}"_guid,
					MAKE_UNICODE_LITERAL("On Receive Forwarded Message"),
					&Network::LocalClient::OnReceiveForwardedMessage,
					FunctionFlags::HostToClient,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Host")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				},
				Function{
					"9eea171d-676c-455a-8c96-a7498e4fb580"_guid,
					MAKE_UNICODE_LITERAL("On Receive Time Sync Response"),
					&Network::LocalClient::OnReceivedTimeSyncResponseMessage,
					FunctionFlags::HostToClient,
					ReturnType{},
					Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Host")},
					Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
					Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
				}
			}
		);
	};
}

namespace ngine::Network
{
	void RemotePeer::Disconnect(const uint32 disconnectUserData)
	{
		enet_peer_disconnect(m_pNetPeer, disconnectUserData);
	}

	void RemotePeer::ForceDisconnect()
	{
		enet_peer_reset(m_pNetPeer);
	}

	bool
	RemotePeer::SendMessageTo(EncodedMessageBuffer&& encodedMessageBuffer, const Channel channel, const EnumFlags<MessageFlags> messageFlags)
	{
		Assert(m_pNetPeer != nullptr);
		if (LIKELY(m_pNetPeer != nullptr))
		{
			uint32 rawFlags = ENET_PACKET_FLAG_NO_ALLOCATE;
			rawFlags |= ENET_PACKET_FLAG_RELIABLE * messageFlags.IsSet(MessageFlags::Reliable);
			rawFlags |= ENET_PACKET_FLAG_UNSEQUENCED * messageFlags.IsSet(MessageFlags::UnreliableUnsequenced);

			const ConstByteView data = encodedMessageBuffer.ReleaseOwnership();
			ENetPacket* pPacket = enet_packet_create(data.GetData(), data.GetDataSize(), rawFlags);
			pPacket->userData = const_cast<ByteType*>(data.GetData());
			pPacket->freeCallback = [](ENetPacket* pPacket)
			{
				EncodedMessageBuffer buffer{ByteView(reinterpret_cast<ByteType*>(pPacket->userData), pPacket->dataLength)};
			};
			return enet_peer_send(m_pNetPeer, channel.Get(), pPacket) == 0;
		}
		else
		{
			return false;
		}
	}

	LocalPeer::LocalPeer(ENetHost* pNetHost)
		: LocalPeerView(pNetHost)
		, Job(Threading::JobPriority::RealtimeNetworking)
	{
		enet_host_compress_with_range_coder(pNetHost);
	}

	LocalPeer::~LocalPeer()
	{
		if (m_pNetHost != nullptr)
		{
			enet_host_destroy(m_pNetHost);
		}
	}

	MessageBuffer LocalPeer::AcquireMessageBuffer(const uint32 requiredSizeInBits)
	{
		const uint32 totalMessageSize = (uint32)Math::Ceil((float)requiredSizeInBits / 8.f);
		MessageBuffer buffer(totalMessageSize);
		buffer.GetView().ZeroInitialize();
		// TODO: Create a message pool to avoid runtime allocations
		return buffer;
	}

	inline static constexpr Time::Durationf UpdateFrequency = (120_hz).GetDuration();

	Threading::Job::Result LocalHost::OnExecute(Threading::JobRunnerThread&)
	{
		for (const ClientIdentifier clientIdentifier : m_clientIdentifiers.GetValidElementView(m_clientIdentifiers.GetView()))
		{
			if (const Optional<PerPeerPropagatedPropertyData*> pPropagatedPropertyData = m_perClientPropagatedPropertyData[clientIdentifier])
			{
				Network::Channel channel{1};
				pPropagatedPropertyData
					->ProcessPendingData(*this, Reflection::PropertyFlags::PropagateClientToHost, m_remoteClients[clientIdentifier], channel);
			}
		}

		ProcessMessages();
		return Result::AwaitExternalFinish;
	}

	Threading::Job::Result LocalClient::OnExecute(Threading::JobRunnerThread&)
	{
		Network::Channel channel{1};
		m_toHostPropagatedPropertyData.ProcessPendingData(*this, Reflection::PropertyFlags::PropagateClientToHost, m_remoteHost, channel);

		ProcessMessages();
		return Result::AwaitExternalFinish;
	}

	void LocalPeer::OnAwaitExternalFinish(Threading::JobRunnerThread& thread)
	{
		SignalExecutionFinished(thread);

		switch (m_updateMode)
		{
			case UpdateMode::Asynchronous:
				if (m_asyncUpdateTimerHandle.IsValid())
				{
					thread.GetJobManager().ScheduleAsyncJob(m_asyncUpdateTimerHandle, UpdateFrequency, *this);
				}
				break;
			case UpdateMode::EngineTick:
			case UpdateMode::Disabled:
				break;
		}
	}

	void LocalPeer::FlushPendingMessages()
	{
		enet_host_flush(m_pNetHost);
	}

	bool LocalPeer::ProcessMessageInternal(ENetEvent& event)
	{
		if (m_pNetHost != nullptr)
		{
			Threading::UniqueLock lock(m_updateMutex);
			m_lastUpdateTime = Time::Timestamp::GetCurrent();
			return enet_host_service(m_pNetHost, &event, 0) > 0;
		}
		else
		{
			return false;
		}
	}

	void LocalPeer::ProcessMessages()
	{
		ENetEvent event;
		while (ProcessMessageInternal(event))
		{
			switch (event.type)
			{
				case ENET_EVENT_TYPE_NONE:
					break;

				case ENET_EVENT_TYPE_CONNECT:
					OnPeerConnected(event.peer);
					break;
				case ENET_EVENT_TYPE_RECEIVE:
				{
					ConstByteView data{event.packet->data, event.packet->dataLength};
					ConstBitView messageView{data};

					HandleMessage(messageView, event.peer, Channel{event.channelID});
					Assert(messageView.GetByteCount() <= 1 || messageView.GetCount() < 8, "Received unused bytes from the network!");

					enet_packet_destroy(event.packet);
				}
				break;

				case ENET_EVENT_TYPE_DISCONNECT:
					OnPeerDisconnected(event.peer);
					break;
			}
		}
	}

	bool LocalHost::CanHandleBoundObjectMessage(
		const BoundObjectIdentifier boundObjectIdentifier, const RemotePeer remotePeer, const EnumFlags<MessageTypeFlags> messageTypeFlags
	) const
	{
		const RemoteClient remoteClient = reinterpret_cast<const RemoteClient&>(remotePeer);
		if (messageTypeFlags.IsSet(MessageTypeFlags::AllowClientToHostWithoutAuthority))
		{
			return true;
		}

		return !HasAuthorityOfBoundObject(boundObjectIdentifier) &&
		       m_boundObjectsAuthorityClients[boundObjectIdentifier] == remoteClient.GetIdentifier().GetFirstValidIndex();
	}

	bool LocalClient::CanHandleBoundObjectMessage(
		const BoundObjectIdentifier boundObjectIdentifier, const RemotePeer remotePeer, const EnumFlags<MessageTypeFlags> messageTypeFlags
	) const
	{
		const RemoteHost remoteHost = reinterpret_cast<const RemoteHost&>(remotePeer);
		UNUSED(boundObjectIdentifier);
		UNUSED(remoteHost);
		UNUSED(messageTypeFlags);

		// TODO: Only supporting host <-> client actual communication so we always accept host messages
		// Later this should check for P2P logic
		return true;
	}

	Optional<MessageTypeIdentifier> LocalPeer::PreprocessMessage(
		ConstBitView& messageView,
		const EnumFlags<MessageTypeFlags> receivableMessageTypeFlags,
		const RemotePeer remotePeer,
		const Channel channel,
		Scripting::VM::Registers& registers
	)
	{
		const MessageTypeIdentifier messageTypeIdentifier = DecompressMessageBuffer<MessageTypeIdentifier>(messageView);
		// Workaround for latest (21.0 dev) clang broken warning
		PUSH_CLANG_WARNINGS
		DISABLE_CLANG_WARNING("-Wunused-result")
		if (UNLIKELY(!messageTypeIdentifier.IsValid() || messageTypeIdentifier.GetFirstValidIndex() >= m_messageTypes.GetView().GetSize()))
			POP_CLANG_WARNINGS
			{
				LogError("Rejected invalid message from client or host");
				return Invalid;
			}

		const MessageType& __restrict messageType = m_messageTypes[messageTypeIdentifier];
		/*const uint32 totalMessageSize = (uint32)Math::Ceil((float)messageType.m_compressedDataSizeInBits / 8.f);

		const MessageTypeIdentifier batchedMessageTypeIdentifier = MessageTypeIdentifier::MakeFromValidIndex( (MessageTypeIdentifier::IndexType
		  )DefaultMessageType::BatchMessages
		  );

		const bool isAppropriatelySized = messageView.GetByteCount() == totalMessageSize || messageTypeIdentifier ==
		batchedMessageTypeIdentifier; Assert(isAppropriatelySized); if (UNLIKELY(!isAppropriatelySized))
		{
		  LogError("Rejected inappropriately sized message from client or host");
		  return;
		}*/

		// TODO: Validate Client to Client

		// Validate that the sender was allowed to send this message
		const bool canReceiveMessage = receivableMessageTypeFlags.AreAnySet(messageType.m_flags & MessageTypeFlags::FromMask) ||
		                               messageType.m_flags.AreNoneSet();
		Assert(canReceiveMessage);
		if (LIKELY(canReceiveMessage))
		{
			const BoundObjectIdentifier boundObjectIdentifier = messageType.m_flags.IsSet(MessageTypeFlags::IsObjectFunction)
			                                                      ? DecompressMessageBuffer<BoundObjectIdentifier>(messageView)
			                                                      : BoundObjectIdentifier{};

			// Validate bound object authority
			const bool failedBoundObjectAuthorityCheck = boundObjectIdentifier.IsValid() &&
			                                             !CanHandleBoundObjectMessage(boundObjectIdentifier, remotePeer, messageType.m_flags);
			Assert(!failedBoundObjectAuthorityCheck);
			if (UNLIKELY(failedBoundObjectAuthorityCheck))
			{
				LogMessage("Rejected message from non-authorative client");
				return Invalid;
			}

			auto decompressArguments = [&messageType, &messageView]() -> Optional<FixedSizeVector<ByteType>>
			{
				FixedSizeVector<ByteType> decompressedData(Memory::ConstructWithSize, Memory::Uninitialized, messageType.m_uncompressedDataSize);
				ArrayView<ByteType> decompressedDataView{decompressedData};

				for (const Reflection::TypeDefinition& argumentTypeDefinition : messageType.m_argumentTypeDefinitions)
				{
					const ByteType* pBegin = Memory::Align((const ByteType*)decompressedDataView.begin(), argumentTypeDefinition.GetAlignment());
					decompressedDataView += static_cast<uint32>(pBegin - decompressedDataView.begin());

					argumentTypeDefinition.DefaultConstructAt(decompressedDataView.GetData());
					const bool wasDecompressed = argumentTypeDefinition.DecompressStoredObject(decompressedDataView.GetData(), messageView);
					Assert(wasDecompressed);
					if (LIKELY(wasDecompressed))
					{
						decompressedDataView += argumentTypeDefinition.GetSize();
					}
					else
					{
						return Invalid;
					}
				}
				return Move(decompressedData);
			};

			if (messageType.m_flags.IsSet(MessageTypeFlags::IsStreamedPropertyFunction))
			{
				registers.PushArgument<0, LocalPeer&>(*this);
				registers.PushArgument<1, MessageTypeIdentifier>(messageTypeIdentifier);
				registers.PushArgument<2, RemotePeer>(remotePeer);
				registers.PushArgument<3, Channel>(channel);
				registers.PushArgument<4, ConstBitView&>(messageView);
			}
			else if (messageType.m_flags.IsSet(MessageTypeFlags::IsDataComponentFunction))
			{
				Assert(m_pEntitySceneRegistry.IsValid());
				Entity::SceneRegistry& sceneRegistry = *m_pEntitySceneRegistry;

				const Optional<Entity::HierarchyComponentBase*> pComponent =
					m_boundObjects[boundObjectIdentifier].Get<Entity::HierarchyComponentBase>();
				if (pComponent.IsValid() && pComponent->GetFlags(sceneRegistry).IsNotSet(Entity::ComponentFlags::IsDestroying))
				{
					Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
					const Reflection::FunctionData functionData = reflectionRegistry.FindFunction(messageType.m_functionGuid);

					Optional<const Reflection::TypeInterface*> pOwningTypeInterface =
						reflectionRegistry.FindTypeInterface(functionData.m_owningTypeGuid);
					Assert(pOwningTypeInterface.IsValid());

					const Entity::ComponentTypeIdentifier componentTypeIdentifier =
						sceneRegistry.FindComponentTypeIdentifier(pOwningTypeInterface->GetGuid());
					const Optional<Entity::ComponentTypeSceneDataInterface*> pComponentTypeSceneData =
						sceneRegistry.GetOrCreateComponentTypeData(componentTypeIdentifier);

					const Entity::ComponentIdentifier componentIdentifier = pComponent->GetIdentifier();
					const Entity::GenericComponentInstanceIdentifier componentInstanceIdentifier =
						pComponentTypeSceneData->FindDataComponentInstanceIdentifier(componentIdentifier);
					Assert(componentInstanceIdentifier.IsValid());
					if (LIKELY(componentInstanceIdentifier.IsValid()))
					{
						void* pComponentAddress = pComponentTypeSceneData->GetComponentAddress(componentInstanceIdentifier);

						Entity::ComponentTypeSceneData<Session::BoundComponent>& boundComponentTypeSceneData =
							*sceneRegistry.FindComponentTypeData<Session::BoundComponent>();
						Session::BoundComponent& boundComponent =
							static_cast<Session::BoundComponent&>(boundComponentTypeSceneData.GetDataComponentUnsafe(componentIdentifier));

						registers.PushArgument<0, void*>(pComponentAddress);
						registers.PushArgument<1, Entity::HierarchyComponentBase&>(*pComponent);
						registers.PushArgument<2, Session::BoundComponent&>(boundComponent);
						registers.PushArgument<3, LocalPeer&>(*this);

						const Optional<FixedSizeVector<ByteType>> decompressedData = decompressArguments();
						if (LIKELY(decompressedData.IsValid()))
						{
							if (messageType.m_flags.AreAllSet(MessageTypeFlags::ClientToHost))
							{
								RemoteClient remoteClient = static_cast<const RemoteClient&>(remotePeer);
								registers.PushArgument<4, ClientIdentifier>(remoteClient.GetIdentifier());
								registers.PushDynamicArgument<5>(decompressedData->GetView());
							}
							else if (decompressedData->HasElements())
							{
								registers.PushDynamicArgument<4>(decompressedData->GetView());
							}
						}
						else
						{
							LogError("Discarding received data component remote function, failed to decompress arguments!");
							return Invalid;
						}
					}
					else
					{
						LogError("Discarding received data component remote function, data component did not exist!");
						return Invalid;
					}
				}
				else
				{
					LogError("Discarding received data component remote function, component did not exist!");
					return Invalid;
				}
			}
			else if (messageType.m_flags.IsSet(MessageTypeFlags::IsComponentFunction))
			{
				Assert(m_pEntitySceneRegistry.IsValid());
				Entity::SceneRegistry& sceneRegistry = *m_pEntitySceneRegistry;
				const Optional<Entity::HierarchyComponentBase*> pComponent =
					m_boundObjects[boundObjectIdentifier].Get<Entity::HierarchyComponentBase>();
				if (pComponent.IsValid() && pComponent->GetFlags(sceneRegistry).IsNotSet(Entity::ComponentFlags::IsDestroying))
				{
					Entity::ComponentTypeSceneDataInterface& componentTypeSceneData = *pComponent->GetTypeSceneData();
					const Entity::GenericComponentInstanceIdentifier componentInstanceIdentifier =
						componentTypeSceneData.GetComponentInstanceIdentifier(*pComponent);
					void* pComponentAddress = componentTypeSceneData.GetComponentAddress(componentInstanceIdentifier);

					Entity::ComponentTypeSceneData<Session::BoundComponent>& boundComponentTypeSceneData =
						*sceneRegistry.FindComponentTypeData<Session::BoundComponent>();
					Session::BoundComponent& boundComponent =
						static_cast<Session::BoundComponent&>(boundComponentTypeSceneData.GetDataComponentUnsafe(pComponent->GetIdentifier()));

					registers.PushArgument<0, void*>(pComponentAddress);
					registers.PushArgument<1, Session::BoundComponent&>(boundComponent);
					registers.PushArgument<2, LocalPeer&>(*this);

					const Optional<FixedSizeVector<ByteType>> decompressedData = decompressArguments();
					if (LIKELY(decompressedData.IsValid()))
					{
						if (messageType.m_flags.AreAllSet(MessageTypeFlags::ClientToHost))
						{
							RemoteClient remoteClient = static_cast<const RemoteClient&>(remotePeer);
							registers.PushArgument<3, ClientIdentifier>(remoteClient.GetIdentifier());
							registers.PushDynamicArgument<4>(decompressedData->GetView());
						}
						else if (decompressedData->HasElements())
						{
							registers.PushDynamicArgument<3>(decompressedData->GetView());
						}
					}
					else
					{
						LogError("Discarding received component remote function, failed to decompress arguments!");
						return Invalid;
					}
				}
				else
				{
					LogError("Discarding received component remote function, component did not exist!");
					return Invalid;
				}
			}
			else if (messageType.m_flags.IsSet(MessageTypeFlags::IsObjectFunction))
			{
				void* pObject = m_boundObjects[boundObjectIdentifier].GetData();
				Assert(pObject != nullptr);
				if (LIKELY(pObject != nullptr))
				{
					registers.PushArgument<0, void*>(pObject);
					registers.PushArgument<1, LocalPeer&>(*this);
					registers.PushArgument<2, RemotePeer>(remotePeer);
					registers.PushArgument<3, Channel>(channel);

					const Optional<FixedSizeVector<ByteType>> decompressedData = decompressArguments();
					if (LIKELY(decompressedData.IsValid()))
					{
						if (decompressedData->HasElements())
						{
							registers.PushDynamicArgument<4>(decompressedData->GetView());
						}
					}
					else
					{
						LogError("Discarding received object remote function, failed to decompress arguments!");
						return Invalid;
					}
				}
				else
				{
					LogError("Discarding received object remote function, object did not exist!");
					return Invalid;
				}
			}
			else
			{
				registers.PushArgument<0, LocalPeer&>(*this);
				registers.PushArgument<1, RemotePeer>(remotePeer);
				registers.PushArgument<2, Channel>(channel);

				if (messageTypeIdentifier.GetFirstValidIndex() < (MessageTypeIdentifier::IndexType)DefaultMessageType::Count)
				{
					registers.PushArgument<3, ConstBitView&>(messageView);
				}
				else
				{
					const Optional<FixedSizeVector<ByteType>> decompressedData = decompressArguments();
					if (LIKELY(decompressedData.IsValid()))
					{
						if (decompressedData->HasElements())
						{
							registers.PushDynamicArgument<3>(decompressedData->GetView());
						}
					}
					else
					{
						LogError("Discarding received remote function, failed to decompress arguments!");
						return Invalid;
					}
				}
			}

			Assert(messageType.m_function.IsValid());
			if (LIKELY(messageType.m_function.IsValid()))
			{
				return messageTypeIdentifier;
			}
			else
			{
				LogError("Received message without local function");
				return Invalid;
			}
		}
		else
		{
			LogError("Rejected message from client or host");
			return Invalid;
		}
	}

	bool LocalPeer::HandleMessage(ConstBitView& messageView, const RemotePeer remotePeer, const Channel channel)
	{
		Scripting::VM::Registers registers;
		const Optional<MessageTypeIdentifier> messageTypeIdentifier =
			PreprocessMessage(messageView, m_receivableMessageTypeFlags, remotePeer, channel, registers);
		if (LIKELY(messageTypeIdentifier.IsValid()))
		{
			const MessageType& __restrict messageType = m_messageTypes[*messageTypeIdentifier];
			messageType.m_function(registers[0], registers[1], registers[2], registers[3], registers[4], registers[5]);
		}
		return messageTypeIdentifier.IsValid();
	}

	LocalClient::LocalClient()
	{
		m_receivableMessageTypeFlags = MessageTypeFlags::FromClient | MessageTypeFlags::ToClient | MessageTypeFlags::FromHost |
		                               MessageTypeFlags::ToAllClients;
		m_sendableMessageTypeFlags = MessageTypeFlags::ToClient | MessageTypeFlags::ToHost | MessageTypeFlags::FromClient;

		RegisterDefaultMessages();
	}

	void LocalClient::RegisterDefaultMessages()
	{
		for (DefaultMessageType messageType = DefaultMessageType::First; messageType < DefaultMessageType::Count; ++messageType)
		{
			switch (messageType)
			{
				case DefaultMessageType::LocalPeerConnected:
					RegisterDefaultMessageType<LocalPeerConnectedMessage, &LocalClient::OnReceivedConnectedMessage>(
						DefaultMessageType::LocalPeerConnected
					);
					break;

				case DefaultMessageType::RegisterNewMessageType:
					RegisterDefaultMessageType<RegisterMessageTypeMessage, &LocalClient::OnReceivedNewMessageType>(
						DefaultMessageType::RegisterNewMessageType
					);
					break;
				case DefaultMessageType::RegisterPropertyStreamMessage:
					RegisterDefaultMessageType<RegisterPropertyStreamMessage, &LocalClient::OnReceivedNewPropertyStreamMessageType>(
						DefaultMessageType::RegisterPropertyStreamMessage
					);
					break;
				case DefaultMessageType::BatchMessages:
					RegisterDefaultMessageType<BatchMessage, &LocalClient::OnReceivedBatchMessageType>(DefaultMessageType::BatchMessages);
					break;
				case DefaultMessageType::ObjectBound:
					RegisterDefaultMessageType<ObjectBoundMessage, &LocalClient::OnReceivedNewBoundObject>(DefaultMessageType::ObjectBound);
					break;
				case DefaultMessageType::ConfirmPropagatedPropertyReceipt:
					RegisterDefaultMessageType<ConfirmPropagatedPropertyReceiptMessage, &LocalClient::OnReceivedConfirmPropertyReceipt>(
						DefaultMessageType::ConfirmPropagatedPropertyReceipt
					);
					break;
				case DefaultMessageType::BoundObjectAuthorityGivenToLocalClient:
					RegisterDefaultMessageType<BoundObjectAuthorityChangedMessage, &LocalClient::OnBoundObjectAuthorityGiven>(
						DefaultMessageType::BoundObjectAuthorityGivenToLocalClient
					);
					break;
				case DefaultMessageType::BoundObjectAuthorityRevokedFromLocalClient:
					RegisterDefaultMessageType<BoundObjectAuthorityChangedMessage, &LocalClient::OnBoundObjectAuthorityRevoked>(
						DefaultMessageType::BoundObjectAuthorityRevokedFromLocalClient
					);
					break;
				case DefaultMessageType::RequestForwardMessageToOtherClients:
					RegisterUnhandledDefaultMessageType<ForwardedMessage>(DefaultMessageType::RequestForwardMessageToOtherClients);
					break;
				case DefaultMessageType::RequestForwardMessageToAllRemotes:
					RegisterUnhandledDefaultMessageType<ForwardedMessage>(DefaultMessageType::RequestForwardMessageToAllRemotes);
					break;
				case DefaultMessageType::ReceivedForwardedMessage:
					RegisterDefaultMessageType<ForwardedMessage, &LocalClient::OnReceiveForwardedMessage>(DefaultMessageType::ReceivedForwardedMessage
					);
					break;

				case DefaultMessageType::RequestTimeSync:
					RegisterUnhandledDefaultMessageType<RequestTimeSyncMessage>(DefaultMessageType::RequestTimeSync);
					break;
				case DefaultMessageType::ReceivedTimeSyncResponse:
					RegisterDefaultMessageType<ReceivedTimeSyncResponseMessage, &LocalClient::OnReceivedTimeSyncResponseMessage>(
						DefaultMessageType::ReceivedTimeSyncResponse
					);
					break;

				case DefaultMessageType::Count:
					ExpectUnreachable();
			}
		}
	}

	bool LocalClient::Start(
		const uint8 maximumChannelCount,
		const uint32 maximumOutgoingConnectionCount,
		const uint32 incomingBandwidth,
		const uint32 outgoingBandwidth
	)
	{
		Assert(m_flags.AreNoneSet(Flags::Connecting | Flags::Disconnecting) && m_clientIdentifier.IsInvalid());
#if !PLATFORM_WEB
		m_pNetHost = enet_host_create(nullptr, maximumOutgoingConnectionCount, maximumChannelCount, incomingBandwidth, outgoingBandwidth);
		if constexpr (PROFILE_BUILD)
		{
			if (IsDebuggerAttached() && m_pNetHost != nullptr)
			{
				for (ENetPeer& peer : ArrayView<ENetPeer, size>{m_pNetHost->peers, m_pNetHost->peerCount})
				{
					enet_peer_timeout(&peer, 100000000ull, 100000000ull, 100000000ull);
				}
			}
		}
#else
		UNUSED(maximumChannelCount);
		UNUSED(maximumOutgoingConnectionCount);
		UNUSED(incomingBandwidth);
		UNUSED(outgoingBandwidth);
		m_clientIdentifier = ClientIdentifier::MakeFromValidIndex(0);
#endif
		return m_pNetHost != nullptr;
	}

	void LocalClient::Stop()
	{
		Assert(m_flags.AreNoneSet(Flags::Connecting | Flags::Disconnecting) && !m_remoteHost.IsValid());
		if (m_pNetHost != nullptr)
		{
			if (m_updateMode != UpdateMode::Disabled)
			{
				ChangeUpdateMode(UpdateMode::Disabled);
			}
			enet_host_destroy(m_pNetHost);
			m_pNetHost = nullptr;
		}
	}

	LocalClient::~LocalClient()
	{
		Assert(m_flags.AreNoneSet(Flags::Connecting | Flags::Disconnecting) && m_clientIdentifier.IsInvalid());
		if (m_updateMode != UpdateMode::Disabled)
		{
			LocalPeer::ChangeUpdateMode(UpdateMode::Disabled);
		}
	}

	void LocalPeer::ChangeUpdateMode(const UpdateMode mode)
	{
		Assert(m_updateMode != mode);

		switch (mode)
		{
			case UpdateMode::Asynchronous:
			{
				Assert(m_pNetHost != nullptr);
				Assert(!m_asyncUpdateTimerHandle.IsValid());
				m_asyncUpdateTimerHandle = System::Get<Threading::JobManager>().ScheduleAsyncJob(UpdateFrequency, *this);
			}
			break;
			case UpdateMode::EngineTick:
			{
				Assert(m_pNetHost != nullptr);
				Engine& engine = System::Get<Engine>();
				engine.ModifyFrameGraph(
					[this, &engine]()
					{
						engine.GetStartTickStage().AddSubsequentStage(*this);
						AddSubsequentStage(engine.GetEndTickStage());
					}
				);
			}
			break;
			case UpdateMode::Disabled:
			{
				switch (m_updateMode)
				{
					case UpdateMode::Disabled:
						break;
					case UpdateMode::Asynchronous:
					{
						const Threading::TimerHandle timerHandle = m_asyncUpdateTimerHandle;
						m_asyncUpdateTimerHandle = {};
						if (timerHandle.IsValid())
						{
							System::Get<Threading::JobManager>().CancelAsyncJob(timerHandle);
						}
					}
					break;
					case UpdateMode::EngineTick:
					{
						Engine& engine = System::Get<Engine>();
						if (engine.GetStartTickStage().IsDirectlyFollowedBy(*this))
						{
							engine.ModifyFrameGraph(
								[this, &engine]()
								{
									Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
									engine.GetStartTickStage().RemoveSubsequentStage(*this, thread, Threading::Job::RemovalFlags{});
									RemoveSubsequentStage(engine.GetEndTickStage(), thread, Threading::Job::RemovalFlags{});
								}
							);
						}
					}
					break;
				}
			}
			break;
		}
		m_updateMode = mode;
	}

	void LocalClient::ChangeUpdateMode(const UpdateMode mode)
	{
		LocalPeer::ChangeUpdateMode(mode);
		if (mode == UpdateMode::Disabled)
		{
			Assert(m_flags.AreNoneSet(Flags::Connecting | Flags::Disconnecting) && !m_remoteHost.IsValid());
		}
	}

	RemoteHost LocalClient::Connect(
		const Address address, const uint32 maximumChannelCount, const uint32 connectionUserData, const UpdateMode updateMode
	)
	{
		Assert(m_pNetHost != nullptr);
		if (LIKELY(m_pNetHost != nullptr))
		{
			Assert(address.GetType() == IPAddress::Type::IPv4, "TODO: Add IPv6 support to ENet");
			if (LIKELY(address.GetType() == IPAddress::Type::IPv4))
			{
				Assert(m_clientIdentifier.IsInvalid());
				[[maybe_unused]] EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::Connecting);
				Assert(previousFlags.AreNoneSet(Flags::Connecting | Flags::Disconnecting));

				ENetAddress rawAddress;
				rawAddress.host = address.GetIPAddress().GetIPv4().Get();
				rawAddress.port = address.GetPort().Get();
				RemoteHost remoteHost = enet_host_connect(m_pNetHost, &rawAddress, maximumChannelCount, connectionUserData);

				if (LIKELY(remoteHost.IsValid()))
				{
					ChangeUpdateMode(updateMode);
				}
				else
				{
					previousFlags = m_flags.FetchAnd(~Flags::Connecting);
					Assert(previousFlags.IsSet(Flags::Connecting));
				}

				m_remoteHost = remoteHost;
				return remoteHost;
			}
			else
			{
				return {};
			}
		}
		else
		{
			return {};
		}
	}

	Address LocalClient::GetLocalHostAddress() const
	{
		Assert(m_remoteHost.IsValid());
		if (LIKELY(m_remoteHost.IsValid()))
		{
			const ENetAddress enetAddress = m_remoteHost.m_pNetPeer->address;
			return Address{IPAddress4{enetAddress.host}, enetAddress.port};
		}
		else
		{
			return {};
		}
	}

	Address LocalClient::GetPublicHostAddress() const
	{
		Assert(m_remoteHost.IsValid());
		if (LIKELY(m_remoteHost.IsValid()))
		{
			const ENetAddress enetAddress = m_remoteHost.m_pNetPeer->address;
			IPAddress4 ipv4Address{enetAddress.host};
			ipv4Address = ipv4Address.GetReverseLookupAddress();
			return Address{ipv4Address, enetAddress.port};
		}
		else
		{
			return {};
		}
	}

	void LocalClient::OnConnectedToRemoteServer(
		const ClientIdentifier clientIdentifier, const Network::RemoteHost remoteHost, const BoundObjectIdentifier boundObjectIdentifier
	)
	{
		Assert(RemotePeerView{m_remoteHost} == remoteHost);
		m_clientIdentifier = clientIdentifier;

		[[maybe_unused]] const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::Connecting);
		Assert(previousFlags.IsSet(Flags::Connecting) && previousFlags.IsNotSet(Flags::Disconnecting));

		OnConnected(remoteHost, boundObjectIdentifier);
	}

	void LocalClient::OnPeerDisconnected([[maybe_unused]] const RemotePeer remotePeer)
	{
		Assert(RemotePeerView{m_remoteHost} == remotePeer);

		[[maybe_unused]] const EnumFlags<Flags> previousFlags = m_flags.FetchAnd(~Flags::Disconnecting);
		Assert(previousFlags.IsSet(Flags::Disconnecting) && previousFlags.IsNotSet(Flags::Connecting));

		if (m_remoteHost.IsValid())
		{
			m_remoteHost = {};

			LocalPeer::ChangeUpdateMode(UpdateMode::Disabled);

			OnDisconnectedInternal();
		}
	}

	void LocalClient::Disconnect()
	{
		[[maybe_unused]] EnumFlags<Flags> previousFlags = m_flags.FetchOr(Flags::Disconnecting);
		Assert(previousFlags.IsNotSet(Flags::Disconnecting));
		Assert(previousFlags.IsSet(Flags::Connecting) || m_clientIdentifier.IsValid());

		previousFlags = m_flags.FetchAnd(~Flags::Connecting);
		if (previousFlags.IsSet(Flags::Connecting))
		{
			[[maybe_unused]] const bool wasCleared = m_flags.TryClearFlags(Flags::Disconnecting);
			Assert(wasCleared);

			Assert(m_clientIdentifier.IsInvalid());
			m_remoteHost.Disconnect(0);
			m_remoteHost = {};

			ChangeUpdateMode(UpdateMode::Disabled);

			OnDisconnectedInternal();
		}
		else
		{
			m_remoteHost.Disconnect(0);
		}
	}

	void LocalClient::OnDisconnectedInternal()
	{
		OnDisconnected();
		m_clientIdentifier = {};

		{
			Threading::UniqueLock lock(m_boundObjectLookupMapMutex);
			m_boundObjectLookupMap.Clear();
		}
		m_toHostPropagatedPropertyData.Reset();
		m_messageTypes.GetView().ZeroInitialize();
		m_messageTypeLookupMap.Clear();
		m_propagatedPropertyLookupMap.Clear();
		m_propagatedPropertyTypeGuids.GetView().ZeroInitialize();
		m_boundObjects.GetView().ZeroInitialize();
		m_boundObjectAuthorityMask.ClearAll();
		m_maximumMessageTypeIndex = 0;

		RegisterDefaultMessages();
	}

	void LocalClient::ForceDisconnect()
	{
		m_flags.ClearFlags(~(Flags::Connecting | Flags::Disconnecting));
		if (m_remoteHost.IsValid())
		{
			m_remoteHost.ForceDisconnect();
			m_remoteHost = {};

			ChangeUpdateMode(UpdateMode::Disabled);

			OnDisconnectedInternal();
		}
	}

	void LocalClient::BindObject(const Guid persistentObjectGuid, const AnyView object, BindObjectCallback&& callback)
	{
		{
			Threading::SharedLock lock(m_boundObjectLookupMapMutex);
			auto it = m_boundObjectLookupMap.Find(persistentObjectGuid);
			if (it != m_boundObjectLookupMap.end())
			{
				if (it->second.boundObjectIdentifier.IsValid())
				{
					m_boundObjects[it->second.boundObjectIdentifier] = object;
					callback(it->second.boundObjectIdentifier);
				}
				else
				{
					Assert(!it->second.m_callback.IsValid());
					it->second.m_object = object;
					it->second.m_callback = Forward<BindObjectCallback>(callback);
				}
				return;
			}
		}

		Threading::UniqueLock lock(m_boundObjectLookupMapMutex);
		auto it = m_boundObjectLookupMap.Find(persistentObjectGuid);
		if (it != m_boundObjectLookupMap.end())
		{
			if (it->second.boundObjectIdentifier.IsValid())
			{
				m_boundObjects[it->second.boundObjectIdentifier] = object;
				callback(it->second.boundObjectIdentifier);
			}
			else
			{
				Assert(!it->second.m_callback.IsValid());
				it->second.m_object = object;
				it->second.m_callback = Forward<BindObjectCallback>(callback);
			}
		}
		else
		{
			m_boundObjectLookupMap
				.Emplace(persistentObjectGuid, BoundObjectInfo{BoundObjectIdentifier{}, object, Forward<BindObjectCallback>(callback)});
		}
	}

	void LocalClient::BindObject(const Guid persistentObjectGuid, const BoundObjectIdentifier boundObjectIdentifier, const AnyView object)
	{
		m_boundObjects[boundObjectIdentifier] = object;

		{
			Threading::SharedLock lock(m_boundObjectLookupMapMutex);
			auto it = m_boundObjectLookupMap.Find(persistentObjectGuid);
			if (it != m_boundObjectLookupMap.end())
			{
				if (it->second.boundObjectIdentifier.IsValid())
				{
					Assert(it->second.boundObjectIdentifier == boundObjectIdentifier);
					Assert(m_boundObjects[boundObjectIdentifier].IsInvalid() || m_boundObjects[boundObjectIdentifier] == object);
					m_boundObjects[boundObjectIdentifier] = object;
				}
				else
				{
					Assert(!it->second.m_callback.IsValid());
					it->second.m_object = object;
				}
				return;
			}
		}

		Threading::UniqueLock lock(m_boundObjectLookupMapMutex);
		auto it = m_boundObjectLookupMap.Find(persistentObjectGuid);
		if (it != m_boundObjectLookupMap.end())
		{
			if (it->second.boundObjectIdentifier.IsValid())
			{
				Assert(it->second.boundObjectIdentifier == boundObjectIdentifier);
				Assert(m_boundObjects[boundObjectIdentifier].IsInvalid() || m_boundObjects[boundObjectIdentifier] == object);
				m_boundObjects[boundObjectIdentifier] = object;
			}
			else
			{
				Assert(!it->second.m_callback.IsValid());
				it->second.m_object = object;
			}
		}
		else
		{
			m_boundObjectLookupMap.Emplace(persistentObjectGuid, BoundObjectInfo{BoundObjectIdentifier{}, object});
		}
	}

	void LocalClient::BindObject(const BoundObjectIdentifier boundObjectIdentifier, const AnyView object)
	{
		m_boundObjects[boundObjectIdentifier] = object;
	}

	void LocalClient::OnReceivedConnectedMessage(const RemoteHost remoteHost, const Channel channel, ConstBitView& messageView)
	{
		// message.m_hostTimestamp = T1
		const Time::Timestamp receivedTime = m_lastUpdateTime; // = T2

		const LocalPeerConnectedMessage message = DecompressMessageBuffer<LocalPeerConnectedMessage>(messageView);
		OnConnectedToRemoteServer(message.m_clientIdentifier, remoteHost, message.m_clientBoundObjectIdentifier);

		const MessageTypeIdentifier::IndexType requestTimeMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
		)DefaultMessageType::RequestTimeSync;
		const MessageTypeIdentifier requestTimeMessageTypeIdentifier =
			MessageTypeIdentifier::MakeFromValidIndex(requestTimeMessageTypeIdentifierIndex);
		MessageBuffer messageBuffer = AcquireMessageBuffer(requestTimeMessageTypeIdentifier);
		const Time::Timestamp sentTime = Time::Timestamp::GetCurrent(); // T3
		m_remoteHost.SendMessageTo(
			EncodeMessageBuffer(
				requestTimeMessageTypeIdentifier,
				Move(messageBuffer),
				RequestTimeSyncMessage{message.m_hostTimestamp, receivedTime, sentTime}
			),
			channel
		);
		// Make sure the time response is sent immediately
		FlushPendingMessages();
	}

	Time::Timestamp LocalClient::GetRoundTripTime() const
	{
		return Time::Timestamp::FromMilliseconds(m_remoteHost.m_pNetPeer->roundTripTime);
	}

	void LocalClient::OnReceivedTimeSyncResponseMessage(const RemoteHost, const Channel, ConstBitView& messageView)
	{
		const ReceivedTimeSyncResponseMessage message = DecompressMessageBuffer<ReceivedTimeSyncResponseMessage>(messageView);

		/*const Time::Timestamp receiptTime = Time::Timestamp::GetCurrent();
		const Time::Timestamp roundTripTime = receiptTime - message.m_clientTimestamp;

		// We assume symmetry, so half of RTT is an approximation.
		const Time::Timestamp oneWayLatency = roundTripTime / 2;
		const Time::Timestamp clientTimeOffsetForLatency = message.m_clientTimestamp + oneWayLatency;
		const int64 offsetInNanoseconds = static_cast<int64>(message.m_hostTimestamp.GetNanoseconds()) -
		  static_cast<int64>(clientTimeOffsetForLatency.GetNanoseconds());*/

		// Calculate how far ahead (or behind) the server clock is compared to the client clock at the moment the client sent the request
		m_hostTimeOffsetNanoseconds = message.m_timeOffsetInNanoseconds;
	}

	[[nodiscard]] LocalPeer::MessageType LocalPeer::CreateMessageType(
		const Guid functionGuid,
		const uint16 compressedDataSizeInBits,
		const Optional<const Reflection::TypeInterface*> pOwningTypeInterface,
		const Reflection::FunctionInfo& functionInfo,
		const Reflection::FunctionData& functionData
	) const
	{
		EnumFlags<MessageTypeFlags> typeMessageTypeFlags;
		if (pOwningTypeInterface.IsValid())
		{
			typeMessageTypeFlags |= MessageTypeFlags::IsObjectFunction |
			                        (MessageTypeFlags::IsComponentFunction * pOwningTypeInterface->Implements<Entity::Component>()) |
			                        (MessageTypeFlags::IsDataComponentFunction * pOwningTypeInterface->Implements<Entity::Data::Component>());
		}

		ArrayView<const Reflection::Argument, uint16> functionArguments = functionInfo.m_getArgumentsFunction(functionInfo);

		EnumFlags<MessageTypeFlags> messageTypeFlags = GetMessageTypeFlags(functionData.m_flags) | typeMessageTypeFlags;

		// Skip default arguments that we provide statically (not sent over the network)
		if (messageTypeFlags.IsSet(MessageTypeFlags::IsDataComponentFunction))
		{
			functionArguments += 3 + messageTypeFlags.AreAllSet(MessageTypeFlags::ClientToHost);
		}
		else if (messageTypeFlags.IsSet(MessageTypeFlags::IsComponentFunction))
		{
			functionArguments += 2 + messageTypeFlags.AreAllSet(MessageTypeFlags::ClientToHost);
		}
		else
		{
			functionArguments += 3;
		}

		Vector<Reflection::TypeDefinition> argumentTypeDefinitions(Memory::Reserve, functionArguments.GetSize());
		for (const Reflection::Argument& __restrict functionArgument : functionArguments)
		{
			argumentTypeDefinitions.EmplaceBack(functionArgument.m_type);
		}

		uint32 uncompressedDataSize{0};
		for (const Reflection::TypeDefinition& __restrict argumentTypeDefinition : argumentTypeDefinitions)
		{
			uncompressedDataSize = Memory::Align(uncompressedDataSize, argumentTypeDefinition.GetAlignment());
			uncompressedDataSize += argumentTypeDefinition.GetSize();
		}

		for (const Reflection::TypeDefinition& __restrict argumentTypeDefinition : argumentTypeDefinitions)
		{
			messageTypeFlags |= MessageTypeFlags::HasDynamicCompressedDataSize *
			                    argumentTypeDefinition.HasDynamicCompressedData(Reflection::PropertyFlags::SentWithNetworkedFunctions);
		}

		return MessageType{
			functionGuid,
			LIKELY(functionData.IsValid()) ? functionData.m_function : RemoteFunction{},
			compressedDataSizeInBits,
			uncompressedDataSize,
			messageTypeFlags,
			Move(argumentTypeDefinitions)
		};
	}

	void LocalClient::OnReceivedNewMessageType(const RemoteHost, const Channel, ConstBitView& messageView)
	{
		const RegisterMessageTypeMessage message = DecompressMessageBuffer<RegisterMessageTypeMessage>(messageView);

		Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();

		const Reflection::FunctionData functionData = reflectionRegistry.FindFunction(message.m_functionGuid);
		Assert(functionData.IsValid());
		if (LIKELY(functionData.IsValid()))
		{
			Optional<const Reflection::TypeInterface*> pOwningTypeInterface;
			Optional<const Reflection::FunctionInfo*> pFunctionInfo;
			if (functionData.m_owningTypeGuid.IsValid())
			{
				pOwningTypeInterface = reflectionRegistry.FindTypeInterface(functionData.m_owningTypeGuid);
				pOwningTypeInterface->IterateFunctions(
					[functionGuid = message.m_functionGuid, &pFunctionInfo](const Reflection::FunctionInfo& functionInfo)
					{
						if (functionInfo.m_guid == functionGuid)
						{
							pFunctionInfo = &functionInfo;
						}
					}
				);
			}
			else
			{
				pFunctionInfo = reflectionRegistry.FindGlobalFunctionDefinition(message.m_functionGuid);
			}

			Assert(pFunctionInfo.IsValid());
			if (LIKELY(pFunctionInfo.IsValid()))
			{
				RegisterMessageType(
					message.m_messageTypeIdentifier,
					CreateMessageType(
						message.m_functionGuid,
						message.m_fixedCompressedDataSizeInBits,
						pOwningTypeInterface,
						*pFunctionInfo,
						functionData
					)
				);

				OnMessageTypeRegistered(message.m_messageTypeIdentifier);
			}
		}
	}

	void LocalClient::OnReceivedNewPropertyStreamMessageType(const RemoteHost, const Channel, ConstBitView& messageView)
	{
		const RegisterPropertyStreamMessage message = DecompressMessageBuffer<RegisterPropertyStreamMessage>(messageView);

		Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();

		const Optional<const Reflection::TypeInterface*> pTypeInterface = reflectionRegistry.FindTypeInterface(message.m_typeGuid);
		Assert(pTypeInterface.IsValid());
		if (LIKELY(pTypeInterface.IsValid()))
		{
			EnumFlags<MessageTypeFlags> messageTypeFlags =
				MessageTypeFlags::IsStreamedPropertyFunction |
				(MessageTypeFlags::IsComponentFunction * pTypeInterface->Implements<Entity::Component>()) |
				(MessageTypeFlags::IsDataComponentFunction * pTypeInterface->Implements<Entity::Data::Component>());

			TypePropertyGuids& typePropertyGuids = m_propagatedPropertyTypeGuids[message.m_messageTypeIdentifier];
			typePropertyGuids.Reserve(message.m_propertyCount);

			for (PropertyIndex propertyIndex = 0; propertyIndex < message.m_propertyCount; ++propertyIndex)
			{
				const Guid propertyGuid = messageView.UnpackAndSkip<Guid>();

				Reflection::TypeInterface::PropertyQueryResult queriedProperty = pTypeInterface->FindProperty(propertyGuid);
				const Optional<const Reflection::PropertyInfo*> pPropertyInfo = queriedProperty.Get<0>();
				Assert(pPropertyInfo.IsValid());
				if (LIKELY(pPropertyInfo.IsValid()))
				{
					const bool isNetworkPropagated = pPropertyInfo->m_flags.AreAnySet(Reflection::PropertyFlags::IsNetworkPropagatedMask);
					Assert(isNetworkPropagated);
					if (LIKELY(isNetworkPropagated))
					{
						messageTypeFlags |=
							(MessageTypeFlags::ClientToHost * pPropertyInfo->m_flags.IsSet(Reflection::PropertyFlags::PropagateClientToHost)) |
							(MessageTypeFlags::HostToClient * pPropertyInfo->m_flags.IsSet(Reflection::PropertyFlags::PropagateHostToClient)) |
							(MessageTypeFlags::HostToAllClients * pPropertyInfo->m_flags.IsSet(Reflection::PropertyFlags::PropagateHostToClient)) |
							(MessageTypeFlags::ClientToClient * pPropertyInfo->m_flags.IsSet(Reflection::PropertyFlags::PropagateClientToClient)) |
							(MessageTypeFlags::ClientToAllClients * pPropertyInfo->m_flags.IsSet(Reflection::PropertyFlags::PropagateClientToClient));

						// Create index unique to the type
						m_propagatedPropertyLookupMap
							.Emplace(Guid(pPropertyInfo->m_guid), PropagatedProperty{propertyIndex, *pPropertyInfo, Move(queriedProperty.Get<1>())});

						typePropertyGuids.EmplaceBack(pPropertyInfo->m_guid);
					}
					else
					{
						LogError("Discarded invalid property stream, property was not propagated");
						return;
					}
				}
				else
				{
					LogError("Discarded invalid property stream, property was not found");
					return;
				}
			}

			RegisterMessageType(
				message.m_messageTypeIdentifier,
				MessageType{message.m_typeGuid, RemoteFunction::Make<&LocalClient::OnReceivedPropertyStream>(), 0, 0, messageTypeFlags}
			);

			OnMessageTypeRegistered(message.m_messageTypeIdentifier);
		}
	}

	void LocalClient::OnReceivedBatchMessageType(const RemoteHost remoteHost, const Channel channel, ConstBitView& messageView)
	{
		const BatchMessage message = DecompressMessageBuffer<BatchMessage>(messageView);
		for (uint32 messageIndex = 0; messageIndex < message.m_messageCount; ++messageIndex)
		{
			HandleMessage(messageView, remoteHost, channel);
		}
	}

	void LocalClient::OnReceivedNewBoundObject(const RemoteHost, const Channel, ConstBitView& messageView)
	{
		const ObjectBoundMessage message = DecompressMessageBuffer<ObjectBoundMessage>(messageView);

		{
			Threading::SharedLock lock(m_boundObjectLookupMapMutex);
			auto it = m_boundObjectLookupMap.Find(message.m_persistentObjectGuid);
			if (it != m_boundObjectLookupMap.end())
			{
				it->second.boundObjectIdentifier = message.m_boundObjectIdentifier;

				m_boundObjects[message.m_boundObjectIdentifier] = it->second.m_object;

				if (it->second.m_callback.IsValid())
				{
					it->second.m_callback(message.m_boundObjectIdentifier);
				}
				return;
			}
		}

		Threading::UniqueLock lock(m_boundObjectLookupMapMutex);
		{
			auto it = m_boundObjectLookupMap.Find(message.m_persistentObjectGuid);
			if (it != m_boundObjectLookupMap.end())
			{
				it->second.boundObjectIdentifier = message.m_boundObjectIdentifier;

				m_boundObjects[message.m_boundObjectIdentifier] = it->second.m_object;

				if (it->second.m_callback.IsValid())
				{
					it->second.m_callback(message.m_boundObjectIdentifier);
				}
				return;
			}
		}
		m_boundObjectLookupMap.Emplace(message.m_persistentObjectGuid, BoundObjectInfo{message.m_boundObjectIdentifier});
	}

	Optional<Reflection::PropertyOwner*> LocalPeer::GetBoundObjectPropertyOwner(
		const BoundObjectIdentifier boundObjectIdentifier, const Guid typeGuid, const EnumFlags<MessageTypeFlags> messageTypeFlags
	)
	{
		if (messageTypeFlags.IsSet(MessageTypeFlags::IsDataComponentFunction))
		{
			Assert(m_pEntitySceneRegistry.IsValid());
			Entity::SceneRegistry& sceneRegistry = *m_pEntitySceneRegistry;
			const Optional<Entity::HierarchyComponentBase*> pComponent =
				m_boundObjects[boundObjectIdentifier].Get<Entity::HierarchyComponentBase>();
			if (pComponent.IsValid() && pComponent->GetFlags(sceneRegistry).IsNotSet(Entity::ComponentFlags::IsDestroying))
			{
				Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
				Optional<const Reflection::TypeInterface*> pOwningTypeInterface = reflectionRegistry.FindTypeInterface(typeGuid);
				Assert(pOwningTypeInterface.IsValid());

				const Entity::ComponentTypeIdentifier componentTypeIdentifier =
					sceneRegistry.FindComponentTypeIdentifier(pOwningTypeInterface->GetGuid());
				const Optional<Entity::ComponentTypeSceneDataInterface*> pComponentTypeSceneData =
					sceneRegistry.GetOrCreateComponentTypeData(componentTypeIdentifier);

				const Entity::ComponentIdentifier componentIdentifier = pComponent->GetIdentifier();
				const Optional<Entity::Component*> pDataComponent = pComponentTypeSceneData->GetDataComponent(componentIdentifier);
				Assert(pDataComponent.IsValid());
				if (LIKELY(pDataComponent.IsValid()))
				{
					return *pDataComponent;
				}
				else
				{
					return Invalid;
				}
			}
			else
			{
				return Invalid;
			}
		}
		else if (messageTypeFlags.IsSet(MessageTypeFlags::IsComponentFunction))
		{
			Assert(m_pEntitySceneRegistry.IsValid());
			Entity::SceneRegistry& sceneRegistry = *m_pEntitySceneRegistry;

			const Optional<Entity::HierarchyComponentBase*> pComponent =
				m_boundObjects[boundObjectIdentifier].Get<Entity::HierarchyComponentBase>();
			if (pComponent.IsValid() && pComponent->GetFlags(sceneRegistry).IsNotSet(Entity::ComponentFlags::IsDestroying))
			{
				return *pComponent;
			}
			else
			{
				return Invalid;
			}
		}
		else
		{
			return m_boundObjects[boundObjectIdentifier].Get<Reflection::PropertyOwner>();
		}
	}

	void LocalHost::OnReceivedPropertyStream(
		const MessageTypeIdentifier messageTypeIdentifier, RemoteClient remoteClient, const Channel channel, ConstBitView& messageView
	)
	{
		const ClientIdentifier sourceClientIdentifier = remoteClient.GetIdentifier();

		const MessageType& __restrict messageType = m_messageTypes[messageTypeIdentifier];

		const SequenceNumber sequenceNumber = *Compression::Decompress<SequenceNumber>(messageView);
		const BoundObjectIdentifier::IndexType objectCount = Compression::Decompress<BoundObjectIdentifier>(messageView)->GetIndex();
		Assert(objectCount > 0);

		const ArrayView<const Guid, PropertyIndex> typePropertyGuids = m_propagatedPropertyTypeGuids[messageTypeIdentifier];
		const PropertyIndex typeMaximumPropertyCount = typePropertyGuids.GetSize();
		const PropertyIndex typePropertyMaskBitCount = (PropertyIndex)Memory::GetBitWidth((1u << typeMaximumPropertyCount) - 1u);

		for (BoundObjectIdentifier::IndexType objectIndex = 0; objectIndex < objectCount; ++objectIndex)
		{
			const BoundObjectIdentifier boundObjectIdentifier = *Compression::Decompress<BoundObjectIdentifier>(messageView);
			Assert(boundObjectIdentifier.IsValid());

			// Read the property mask
			PropertyMask propertyMask;
			const bool wasPropertyMaskDecompressed =
				messageView.UnpackAndSkip(BitView::Make(propertyMask, Math::Range<size>::Make(0, typePropertyMaskBitCount)));

			Assert(wasPropertyMaskDecompressed);
			if (UNLIKELY(!wasPropertyMaskDecompressed))
			{
				messageView = {};
				return;
			}

			Assert(propertyMask.AreAnySet());

			PropertyMask queuedClientPropagationPropertyMask;

			for (const PropertyIndex propertyIndex : propertyMask.GetSetBitsIterator())
			{
				const Guid propertyGuid = typePropertyGuids[propertyIndex];
				const auto propertyIt = m_propagatedPropertyLookupMap.Find(propertyGuid);
				Assert(propertyIt != m_propagatedPropertyLookupMap.end());
				if (UNLIKELY(propertyIt == m_propagatedPropertyLookupMap.end()))
				{
					messageView = {};
					return;
				}

				const PropagatedProperty& __restrict propagatedProperty = propertyIt->second;

				const Optional<Reflection::PropertyOwner*> pPropertyOwner =
					GetBoundObjectPropertyOwner(boundObjectIdentifier, messageType.m_functionGuid, messageType.m_flags);
				if (LIKELY(pPropertyOwner.IsValid()))
				{
					Any decompressedValue{Memory::Uninitialized, propagatedProperty.propertyInfo.m_typeDefinition};

					const bool wasDecompressed = propagatedProperty.propertyInfo.m_typeDefinition.DecompressStoredObject(
						decompressedValue.GetData(),
						messageView,
						Reflection::PropertyFlags::PropagateClientToHost
					);
					Assert(wasDecompressed);
					if (LIKELY(wasDecompressed))
					{
						propagatedProperty.dynamicPropertyInstance.SetValue(*pPropertyOwner, Invalid, Move(decompressedValue));

						// Propagate data to other clients if necessary
						if (propagatedProperty.propertyInfo.m_flags.IsSet(Reflection::PropertyFlags::PropagateClientToClient))
						{
							queuedClientPropagationPropertyMask.Set(propagatedProperty.localIndex);
						}
					}
					else
					{
						messageView = {};
						return;
					}
				}
				else
				{
					messageView = {};
					return;
				}
			}

			// Propagate data to other clients if necessary
			if (queuedClientPropagationPropertyMask.AreAnySet())
			{
				const ArrayView<const ClientIdentifier::IndexType> localClientIdentifiers = m_localClientIdentifiers;
				for (const ClientIdentifier clientIdentifier : m_clientIdentifiers.GetValidElementView(m_clientIdentifiers.GetView()))
				{
					if (clientIdentifier.GetIndex() != sourceClientIdentifier.GetIndex() && m_remoteClients[clientIdentifier].IsValid() && !localClientIdentifiers.Contains(clientIdentifier.GetFirstValidIndex()))
					{
						m_perClientPropagatedPropertyData[clientIdentifier]
							->Invalidate(messageTypeIdentifier, boundObjectIdentifier, queuedClientPropagationPropertyMask);
					}
				}
			}
		}

		const MessageTypeIdentifier::IndexType confirmReceiptMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
		)DefaultMessageType::ConfirmPropagatedPropertyReceipt;
		const MessageTypeIdentifier confirmReceiptMessageTypeIdentifier =
			MessageTypeIdentifier::MakeFromValidIndex(confirmReceiptMessageTypeIdentifierIndex);
		EncodedMessageBuffer encodedMessageBuffer = EncodeMessageBuffer(
			confirmReceiptMessageTypeIdentifier,
			AcquireMessageBuffer(confirmReceiptMessageTypeIdentifier),
			ConfirmPropagatedPropertyReceiptMessage{sequenceNumber, messageTypeIdentifier}
		);
		Assert(encodedMessageBuffer.IsValid());
		if (LIKELY(encodedMessageBuffer.IsValid()))
		{
			remoteClient.SendMessageTo(Move(encodedMessageBuffer), channel, MessageFlags::UnreliableUnsequenced);
		}
	}

	void LocalClient::OnReceivedPropertyStream(
		const MessageTypeIdentifier messageTypeIdentifier, RemoteHost remoteHost, const Channel channel, ConstBitView& messageView
	)
	{
		const MessageType& __restrict messageType = m_messageTypes[messageTypeIdentifier];

		const SequenceNumber sequenceNumber = *Compression::Decompress<SequenceNumber>(messageView);
		const BoundObjectIdentifier::IndexType objectCount = Compression::Decompress<BoundObjectIdentifier>(messageView)->GetIndex();
		Assert(objectCount > 0);

		const ArrayView<const Guid, PropertyIndex> typePropertyGuids = m_propagatedPropertyTypeGuids[messageTypeIdentifier];
		const PropertyIndex typeMaximumPropertyCount = typePropertyGuids.GetSize();
		const PropertyIndex typePropertyMaskBitCount = (PropertyIndex)Memory::GetBitWidth((1u << typeMaximumPropertyCount) - 1u);

		for (BoundObjectIdentifier::IndexType objectIndex = 0; objectIndex < objectCount; ++objectIndex)
		{
			const BoundObjectIdentifier boundObjectIdentifier = *Compression::Decompress<BoundObjectIdentifier>(messageView);
			Assert(boundObjectIdentifier.IsValid());

			// Read the property mask
			PropertyMask propertyMask;
			const bool wasPropertyMaskDecompressed =
				messageView.UnpackAndSkip(BitView::Make(propertyMask, Math::Range<size>::Make(0, typePropertyMaskBitCount)));

			Assert(wasPropertyMaskDecompressed);
			if (UNLIKELY(!wasPropertyMaskDecompressed))
			{
				messageView = {};
				return;
			}

			Assert(propertyMask.AreAnySet());

			for (const PropertyIndex propertyIndex : propertyMask.GetSetBitsIterator())
			{
				const Guid propertyGuid = typePropertyGuids[propertyIndex];
				const auto propertyIt = m_propagatedPropertyLookupMap.Find(propertyGuid);
				Assert(propertyIt != m_propagatedPropertyLookupMap.end());
				if (UNLIKELY(propertyIt == m_propagatedPropertyLookupMap.end()))
				{
					messageView = {};
					return;
				}

				const PropagatedProperty& __restrict propagatedProperty = propertyIt->second;

				const Optional<Reflection::PropertyOwner*> pPropertyOwner =
					GetBoundObjectPropertyOwner(boundObjectIdentifier, messageType.m_functionGuid, messageType.m_flags);
				if (LIKELY(pPropertyOwner.IsValid()))
				{
					Any decompressedValue{Memory::Uninitialized, propagatedProperty.propertyInfo.m_typeDefinition};

					const bool wasDecompressed = propagatedProperty.propertyInfo.m_typeDefinition.DecompressStoredObject(
						decompressedValue.GetData(),
						messageView,
						Reflection::PropertyFlags::PropagateClientToHost
					);
					Assert(wasDecompressed);
					if (LIKELY(wasDecompressed))
					{
						propagatedProperty.dynamicPropertyInstance.SetValue(*pPropertyOwner, Invalid, Move(decompressedValue));
					}
					else
					{
						messageView = {};
						return;
					}
				}
				else
				{
					messageView = {};
					return;
				}
			}
		}

		const MessageTypeIdentifier::IndexType confirmReceiptMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
		)DefaultMessageType::ConfirmPropagatedPropertyReceipt;
		const MessageTypeIdentifier confirmReceiptMessageTypeIdentifier =
			MessageTypeIdentifier::MakeFromValidIndex(confirmReceiptMessageTypeIdentifierIndex);
		EncodedMessageBuffer encodedMessageBuffer = EncodeMessageBuffer(
			confirmReceiptMessageTypeIdentifier,
			AcquireMessageBuffer(confirmReceiptMessageTypeIdentifier),
			ConfirmPropagatedPropertyReceiptMessage{sequenceNumber, messageTypeIdentifier}
		);
		Assert(encodedMessageBuffer.IsValid());
		if (LIKELY(encodedMessageBuffer.IsValid()))
		{
			remoteHost.SendMessageTo(Move(encodedMessageBuffer), channel, MessageFlags::UnreliableUnsequenced);
		}
	}

	LocalHost::LocalHost()
	{
		m_receivableMessageTypeFlags = MessageTypeFlags::ToHost | MessageTypeFlags::FromClient;
		m_sendableMessageTypeFlags = MessageTypeFlags::FromHost | MessageTypeFlags::ToClient | MessageTypeFlags::ToAllClients;

		// Default to host authority over all current and future objects
		m_boundObjectAuthorityMask.SetAll();

		m_boundObjectIdentifier = m_boundObjectIdentifiers.AcquireIdentifier();

		for (DefaultMessageType messageType = DefaultMessageType::First; messageType < DefaultMessageType::Count; ++messageType)
		{
			[[maybe_unused]] const MessageTypeIdentifier messageTypeIdentifier = m_messageTypeIdentifiers.AcquireIdentifier();
			Assert(messageTypeIdentifier.GetFirstValidIndex() == (uint8)messageType);

			switch (messageType)
			{
				case DefaultMessageType::LocalPeerConnected:
					RegisterUnhandledDefaultMessageType<LocalPeerConnectedMessage>(DefaultMessageType::LocalPeerConnected);
					break;
				case DefaultMessageType::RegisterNewMessageType:
					RegisterUnhandledDefaultMessageType<RegisterMessageTypeMessage>(DefaultMessageType::RegisterNewMessageType);
					break;
				case DefaultMessageType::RegisterPropertyStreamMessage:
					RegisterUnhandledDefaultMessageType<RegisterPropertyStreamMessage>(DefaultMessageType::RegisterPropertyStreamMessage);
					break;
				case DefaultMessageType::BatchMessages:
					RegisterUnhandledDefaultMessageType<BatchMessage>(DefaultMessageType::BatchMessages);
					break;
				case DefaultMessageType::ObjectBound:
					RegisterUnhandledDefaultMessageType<ObjectBoundMessage>(DefaultMessageType::ObjectBound);
					break;
				case DefaultMessageType::ConfirmPropagatedPropertyReceipt:
					RegisterDefaultMessageType<ConfirmPropagatedPropertyReceiptMessage, &LocalHost::OnReceivedConfirmPropertyReceipt>(
						DefaultMessageType::ConfirmPropagatedPropertyReceipt
					);
					break;
				case DefaultMessageType::BoundObjectAuthorityGivenToLocalClient:
					RegisterUnhandledDefaultMessageType<BoundObjectAuthorityChangedMessage>(DefaultMessageType::BoundObjectAuthorityGivenToLocalClient
					);
					break;
				case DefaultMessageType::BoundObjectAuthorityRevokedFromLocalClient:
					RegisterUnhandledDefaultMessageType<BoundObjectAuthorityChangedMessage>(
						DefaultMessageType::BoundObjectAuthorityRevokedFromLocalClient
					);
					break;
				case DefaultMessageType::RequestForwardMessageToOtherClients:
					RegisterDefaultMessageType<ForwardedMessage, &LocalHost::OnReceivedForwardRequestToOtherClients>(
						DefaultMessageType::RequestForwardMessageToOtherClients
					);
					break;
				case DefaultMessageType::RequestForwardMessageToAllRemotes:
					RegisterDefaultMessageType<ForwardedMessage, &LocalHost::OnReceivedForwardRequestToAllRemotes>(
						DefaultMessageType::RequestForwardMessageToAllRemotes
					);
					break;
				case DefaultMessageType::ReceivedForwardedMessage:
					RegisterUnhandledDefaultMessageType<ForwardedMessage>(DefaultMessageType::ReceivedForwardedMessage);
					break;

				case DefaultMessageType::RequestTimeSync:
					RegisterDefaultMessageType<RequestTimeSyncMessage, &LocalHost::OnReceivedTimeSyncRequest>(DefaultMessageType::RequestTimeSync);
					break;
				case DefaultMessageType::ReceivedTimeSyncResponse:
					RegisterUnhandledDefaultMessageType<ReceivedTimeSyncResponseMessage>(DefaultMessageType::ReceivedTimeSyncResponse);
					break;
				case DefaultMessageType::Count:
					ExpectUnreachable();
			}
		}

		Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
		reflectionRegistry.IterateTypeInterfaces(
			[this, &reflectionRegistry](const Reflection::TypeInterface& typeInterface)
			{
				if (typeInterface.GetGuid() == Reflection::GetTypeGuid<LocalHost>() || typeInterface.GetGuid() == Reflection::GetTypeGuid<LocalClient>())
				{
					return Memory::CallbackResult::Continue;
				}

				const EnumFlags<MessageTypeFlags> typeMessageTypeFlags{
					(MessageTypeFlags::IsComponentFunction * typeInterface.Implements<Entity::Component>()) |
					(MessageTypeFlags::IsDataComponentFunction * typeInterface.Implements<Entity::Data::Component>())
				};

				if (typeInterface.GetFlags().IsSet(Reflection::TypeFlags::HasNetworkedFunctions))
				{
					typeInterface.IterateFunctions(
						[this, &reflectionRegistry, &typeInterface](const Reflection::FunctionInfo& functionInfo)
						{
							if (functionInfo.m_flags.AreAnySet(Reflection::FunctionFlags::IsNetworkedMask))
							{
								const Reflection::FunctionData functionData = reflectionRegistry.FindFunction(functionInfo.m_guid);
								Assert(functionData.IsValid());
								RegisterMessageType(CreateMessageType(typeInterface, functionInfo, functionData));
							}
						}
					);
				}

				if (typeInterface.GetFlags().IsSet(Reflection::TypeFlags::HasNetworkedProperties))
				{
					EnumFlags<MessageTypeFlags> messageTypeFlags{typeMessageTypeFlags | MessageTypeFlags::IsStreamedPropertyFunction};

					PropertyIndex propertyIndex = 0;

					typeInterface.IterateProperties(
						[&messageTypeFlags, &propertyIndex, &propagatedPropertyLookupMap = m_propagatedPropertyLookupMap](
							const Reflection::PropertyInfo& propertyInfo,
							Reflection::DynamicPropertyInstance&& dynamicPropertyInstance
						)
						{
							if (propertyInfo.m_flags.AreAnySet(Reflection::PropertyFlags::IsNetworkPropagatedMask))
							{
								messageTypeFlags |=
									(MessageTypeFlags::ClientToHost * propertyInfo.m_flags.IsSet(Reflection::PropertyFlags::PropagateClientToHost)) |
									(MessageTypeFlags::HostToClient * propertyInfo.m_flags.IsSet(Reflection::PropertyFlags::PropagateHostToClient)) |
									(MessageTypeFlags::HostToAllClients * propertyInfo.m_flags.IsSet(Reflection::PropertyFlags::PropagateHostToClient)) |
									(MessageTypeFlags::ClientToClient * propertyInfo.m_flags.IsSet(Reflection::PropertyFlags::PropagateClientToClient)) |
									(MessageTypeFlags::ClientToAllClients * propertyInfo.m_flags.IsSet(Reflection::PropertyFlags::PropagateClientToClient));

								// Create index unique to the type
								const PropertyIndex localPropertyIndex = propertyIndex++;
								propagatedPropertyLookupMap.Emplace(
									Guid(propertyInfo.m_guid),
									PropagatedProperty{
										localPropertyIndex,
										propertyInfo,
										Forward<Reflection::DynamicPropertyInstance>(dynamicPropertyInstance)
									}
								);
							}
						}
					);

					const MessageTypeIdentifier messageTypeIdentifier = m_messageTypeIdentifiers.AcquireIdentifier();
					m_propertyStreamMessageTypes.Set(messageTypeIdentifier);

					TypePropertyGuids& typePropertyGuids = m_propagatedPropertyTypeGuids[messageTypeIdentifier];
					typePropertyGuids.Reserve(propertyIndex);
					typeInterface.IterateProperties(
						[&typePropertyGuids](const Reflection::PropertyInfo& propertyInfo, Reflection::DynamicPropertyInstance&&)
						{
							if (propertyInfo.m_flags.AreAnySet(Reflection::PropertyFlags::IsNetworkPropagatedMask))
							{
								typePropertyGuids.EmplaceBack(propertyInfo.m_guid);
							}
						}
					);

					LocalPeer::RegisterMessageType(
						messageTypeIdentifier,
						MessageType{typeInterface.GetGuid(), RemoteFunction::Make<&LocalHost::OnReceivedPropertyStream>(), 0, 0, messageTypeFlags}
					);
				}
				return Memory::CallbackResult::Continue;
			}
		);

		reflectionRegistry.IterateGlobalFunctions(
			[this, &reflectionRegistry](const Reflection::FunctionInfo& functionInfo)
			{
				if (functionInfo.m_flags.AreAnySet(Reflection::FunctionFlags::IsNetworkedMask))
				{
					if (!HasMessage(functionInfo.m_guid))
					{
						const Reflection::FunctionData functionData = reflectionRegistry.FindFunction(functionInfo.m_guid);
						Assert(functionData.IsValid());
						RegisterMessageType(CreateMessageType(Invalid, functionInfo, functionData));
					}
				}
				return Memory::CallbackResult::Continue;
			}
		);
	}

	LocalHost::~LocalHost()
	{
		if (m_updateMode != UpdateMode::Disabled)
		{
			LocalPeer::ChangeUpdateMode(UpdateMode::Disabled);
		}
	}

	bool LocalHost::Start(
		const Address address,
		const uint32 maximumClientCount,
		const uint8 maximumChannelCount,
		const uint32 incomingBandwidth,
		const uint32 outgoingBandwidth,
		const UpdateMode updateMode
	)
	{
		Assert(!IsValid());
		Assert(address.GetType() == IPAddress::Type::IPv4, "TODO: Add IPv6 support to ENet");
		if (LIKELY(address.GetType() == IPAddress::Type::IPv4))
		{
			ENetAddress rawAddress;
			rawAddress.host = address.GetIPAddress().GetIPv4().Get();
			rawAddress.port = address.GetPort().Get();
#if !PLATFORM_WEB
			m_pNetHost = enet_host_create(&rawAddress, maximumClientCount, maximumChannelCount, incomingBandwidth, outgoingBandwidth);
#endif
			if (LIKELY(m_pNetHost != nullptr))
			{
				if constexpr (PROFILE_BUILD)
				{
					if (IsDebuggerAttached())
					{
						for (ENetPeer& peer : ArrayView<ENetPeer, size>{m_pNetHost->peers, m_pNetHost->peerCount})
						{
							enet_peer_timeout(&peer, 100000000ull, 100000000ull, 100000000ull);
						}
					}
				}
				ChangeUpdateMode(updateMode);
			}
			UNUSED(address);
			UNUSED(maximumClientCount);
			UNUSED(maximumChannelCount);
			UNUSED(incomingBandwidth);
			UNUSED(outgoingBandwidth);
#endif
		}
		return m_pNetHost != nullptr;
	}

	void LocalHost::ChangeUpdateMode(const UpdateMode mode)
	{
		LocalPeer::ChangeUpdateMode(mode);
	}

	void LocalHost::OnPeerConnected(RemotePeer peer)
	{
		const ClientIdentifier clientIdentifier = m_clientIdentifiers.AcquireIdentifier();
		Assert(clientIdentifier.IsValid());
		if (LIKELY(clientIdentifier.IsValid()))
		{
			RemoteClient remoteClient = static_cast<const RemoteClient&>(peer);
			m_remoteClients[clientIdentifier] = remoteClient;

			Assert(m_perClientPropagatedPropertyData[clientIdentifier].IsInvalid());
			m_perClientPropagatedPropertyData[clientIdentifier].CreateInPlace();

			remoteClient.OnConnected(clientIdentifier);

			const BoundObjectIdentifier clientBoundObjectIdentifier = m_boundObjectIdentifiers.AcquireIdentifier();

			{
				const MessageTypeIdentifier::IndexType batchedMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
				)DefaultMessageType::BatchMessages;
				const MessageTypeIdentifier batchedMessageTypeIdentifier =
					MessageTypeIdentifier::MakeFromValidIndex(batchedMessageTypeIdentifierIndex);

				uint16 messageCount{0};
				uint32 batchedMessageSize{0};

				// Calculate message count and sizes
				{
					const MessageTypeIdentifier::IndexType localPeerConnectedMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
					)DefaultMessageType::LocalPeerConnected;
					const MessageTypeIdentifier localPeerConnectedMessageTypeIdentifier =
						MessageTypeIdentifier::MakeFromValidIndex(localPeerConnectedMessageTypeIdentifierIndex);
					messageCount += 1;
					batchedMessageSize += m_messageTypes[localPeerConnectedMessageTypeIdentifier].m_fixedCompressedDataSizeInBits;
				}

				{
					const uint16 functionMessageCount = m_functionMessageTypes.GetNumberOfSetBits();
					messageCount += functionMessageCount;

					const MessageTypeIdentifier::IndexType registerNewMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
					)DefaultMessageType::RegisterNewMessageType;
					const MessageTypeIdentifier registerNewMessageTypeIdentifier =
						MessageTypeIdentifier::MakeFromValidIndex(registerNewMessageTypeIdentifierIndex);

					batchedMessageSize += m_messageTypes[registerNewMessageTypeIdentifier].m_fixedCompressedDataSizeInBits * functionMessageCount;
				}

				{
					const uint16 propertyStreamMessageCount = m_propertyStreamMessageTypes.GetNumberOfSetBits();
					messageCount += propertyStreamMessageCount;

					batchedMessageSize += (CalculateFixedCompressedDataSize<MessageTypeIdentifier>() +
					                       CalculateFixedCompressedDataSize<RegisterPropertyStreamMessage>()) *
					                      propertyStreamMessageCount;

					for (const MessageTypeIdentifier::IndexType messageTypeIdentifierIndex : m_propertyStreamMessageTypes.GetSetBitsIterator())
					{
						const MessageTypeIdentifier messageTypeIdentifier = MessageTypeIdentifier::MakeFromValidIndex(messageTypeIdentifierIndex);
						TypePropertyGuids& typePropertyGuids = m_propagatedPropertyTypeGuids[messageTypeIdentifier];
						batchedMessageSize += CalculateFixedCompressedDataSize<Guid>() * typePropertyGuids.GetSize();
					}
				}

				{
					const MessageTypeIdentifier::IndexType bindObjectMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
					)DefaultMessageType::ObjectBound;
					const MessageTypeIdentifier bindObjectMessageTypeIdentifier =
						MessageTypeIdentifier::MakeFromValidIndex(bindObjectMessageTypeIdentifierIndex);
					const uint32 bindObjectMessageSize = m_messageTypes[bindObjectMessageTypeIdentifier].m_fixedCompressedDataSizeInBits;

					for (const BoundObjectIdentifier boundObjectIdentifier :
					     m_boundObjectIdentifiers.GetValidElementView(m_boundObjectIdentifiers.GetView()))
					{
						if (m_boundObjectIdentifierGuids[boundObjectIdentifier].IsValid())
						{
							messageCount++;
							batchedMessageSize += bindObjectMessageSize;
						}
					}
				}

				MessageBuffer messageBuffer =
					AcquireMessageBuffer(GetMessageFixedCompressedSizeInBits(batchedMessageTypeIdentifier) + batchedMessageSize);
				BitView targetView = messageBuffer.GetView();

				{
					BatchMessage batchedMessage{messageCount};
					const bool wasCompressed = CompressMessages(batchedMessageTypeIdentifier, targetView, batchedMessage);
					Assert(wasCompressed);
					if (UNLIKELY(!wasCompressed))
					{
						peer.Disconnect(0);
						return;
					}
				}

				// Send the batched messages
				for (const MessageTypeIdentifier::IndexType messageTypeIdentifierIndex : m_functionMessageTypes.GetSetBitsIterator())
				{
					const MessageTypeIdentifier messageTypeIdentifier = MessageTypeIdentifier::MakeFromValidIndex(messageTypeIdentifierIndex);
					const MessageType& __restrict messageType = m_messageTypes[messageTypeIdentifier];

					const MessageTypeIdentifier::IndexType registerMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
					)DefaultMessageType::RegisterNewMessageType;
					const MessageTypeIdentifier registerMessageTypeIdentifier =
						MessageTypeIdentifier::MakeFromValidIndex(registerMessageTypeIdentifierIndex);

					const RegisterMessageTypeMessage messageData{
						messageTypeIdentifier,
						messageType.m_functionGuid,
						messageType.m_fixedCompressedDataSizeInBits
					};
					const bool wasCompressed = CompressMessages(registerMessageTypeIdentifier, targetView, messageData);
					Assert(wasCompressed);
					if (UNLIKELY(!wasCompressed))
					{
						peer.Disconnect(0);
						return;
					}
				}

				for (const MessageTypeIdentifier::IndexType messageTypeIdentifierIndex : m_propertyStreamMessageTypes.GetSetBitsIterator())
				{
					const MessageTypeIdentifier messageTypeIdentifier = MessageTypeIdentifier::MakeFromValidIndex(messageTypeIdentifierIndex);
					const MessageType& __restrict messageType = m_messageTypes[messageTypeIdentifier];

					const MessageTypeIdentifier::IndexType registerPropertyStreamIdentifierIndex = (MessageTypeIdentifier::IndexType
					)DefaultMessageType::RegisterPropertyStreamMessage;
					const MessageTypeIdentifier registerPropertyStreamIdentifier =
						MessageTypeIdentifier::MakeFromValidIndex(registerPropertyStreamIdentifierIndex);

					TypePropertyGuids& typePropertyGuids = m_propagatedPropertyTypeGuids[messageTypeIdentifier];

					const RegisterPropertyStreamMessage messageData{messageTypeIdentifier, messageType.m_functionGuid, typePropertyGuids.GetSize()};
					const bool wasCompressed = CompressMessages(registerPropertyStreamIdentifier, targetView, messageData);
					Assert(wasCompressed);
					if (UNLIKELY(!wasCompressed))
					{
						peer.Disconnect(0);
						return;
					}

					for (const Guid propertyGuid : typePropertyGuids)
					{
						const bool wasPacked = targetView.PackAndSkip(ConstBitView::Make(propertyGuid));
						Assert(wasPacked);
						if (UNLIKELY(!wasPacked))
						{
							peer.Disconnect(0);
							return;
						}
					}
				}

				{
					const MessageTypeIdentifier::IndexType bindObjectMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
					)DefaultMessageType::ObjectBound;
					const MessageTypeIdentifier bindObjectMessageTypeIdentifier =
						MessageTypeIdentifier::MakeFromValidIndex(bindObjectMessageTypeIdentifierIndex);

					for (const BoundObjectIdentifier boundObjectIdentifier :
					     m_boundObjectIdentifiers.GetValidElementView(m_boundObjectIdentifiers.GetView()))
					{
						const Guid persistentObjectGuid = m_boundObjectIdentifierGuids[boundObjectIdentifier];
						if (persistentObjectGuid.IsValid())
						{
							const ObjectBoundMessage messageData{boundObjectIdentifier, persistentObjectGuid};
							const bool wasCompressed = CompressMessages(bindObjectMessageTypeIdentifier, targetView, messageData);
							Assert(wasCompressed);
							if (UNLIKELY(!wasCompressed))
							{
								peer.Disconnect(0);
								return;
							}
						}
					}
				}

				{
					const MessageTypeIdentifier::IndexType localPeerConnectedMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
					)DefaultMessageType::LocalPeerConnected;
					const MessageTypeIdentifier localPeerConnectedMessageTypeIdentifier =
						MessageTypeIdentifier::MakeFromValidIndex(localPeerConnectedMessageTypeIdentifierIndex);

					const LocalPeerConnectedMessage messageData{clientIdentifier, clientBoundObjectIdentifier, Time::Timestamp::GetCurrent()};
					const bool wasCompressed = CompressMessages(localPeerConnectedMessageTypeIdentifier, targetView, messageData);
					Assert(wasCompressed);
					if (UNLIKELY(!wasCompressed))
					{
						peer.Disconnect(0);
						return;
					}
				}

				EncodedMessageBuffer encodedMessageBuffer{Move(messageBuffer), targetView};

				const Channel channel{0};
				const bool wasSent = peer.SendMessageTo(Move(encodedMessageBuffer), channel);
				Assert(wasSent);
				if (UNLIKELY(!wasSent))
				{
					peer.Disconnect(0);
					return;
				}
				// Always notify peer of connection as soon as possible
				FlushPendingMessages();
			}

			OnClientConnected(clientIdentifier, remoteClient, clientBoundObjectIdentifier);
		}
		else
		{
			peer.Disconnect(0);
		}
	}
	void LocalHost::OnPeerDisconnected(const RemotePeer peer)
	{
		RemoteClient remoteClient = static_cast<const RemoteClient&>(peer);
		const ClientIdentifier clientIdentifier = remoteClient.GetIdentifier();

		OnClientDisconnected(clientIdentifier);

		m_remoteClients[clientIdentifier] = RemoteClient{};
		remoteClient.OnDisconnected();

		m_perClientPropagatedPropertyData[clientIdentifier].DestroyElement();

		m_clientIdentifiers.ReturnIdentifier(clientIdentifier);
	}

	void LocalHost::Stop()
	{
		if (m_updateMode != UpdateMode::Disabled)
		{
			ChangeUpdateMode(UpdateMode::Disabled);
		}

		Assert(m_pNetHost != nullptr);
		if (LIKELY(m_pNetHost != nullptr))
		{
			enet_host_destroy(m_pNetHost);
			m_pNetHost = nullptr;
		}
	}

	Address LocalHost::GetLocalAddress() const
	{
		Assert(m_pNetHost != nullptr);
		if (LIKELY(m_pNetHost != nullptr))
		{
			const ENetAddress enetAddress = m_pNetHost->address;
			return Address{IPAddress4{enetAddress.host}, enetAddress.port};
		}
		else
		{
			return {};
		}
	}

	Address LocalHost::GetPublicAddress() const
	{
		Assert(m_pNetHost != nullptr);
		if (LIKELY(m_pNetHost != nullptr))
		{
			const ENetAddress enetAddress = m_pNetHost->address;
			IPAddress4 ipv4Address{enetAddress.host};
			ipv4Address = ipv4Address.GetReverseLookupAddress();
			return Address{ipv4Address, enetAddress.port};
		}
		else
		{
			return {};
		}
	}

	bool LocalHost::BroadcastMessage(
		EncodedMessageBuffer&& encodedMessageBuffer, const Channel channel, const EnumFlags<MessageFlags> messageFlags
	)
	{
		Assert(m_pNetHost != nullptr);
		if (LIKELY(m_pNetHost != nullptr))
		{
			uint32 rawFlags = ENET_PACKET_FLAG_NO_ALLOCATE;
			rawFlags |= ENET_PACKET_FLAG_RELIABLE * messageFlags.IsSet(MessageFlags::Reliable);
			rawFlags |= ENET_PACKET_FLAG_UNSEQUENCED * messageFlags.IsSet(MessageFlags::UnreliableUnsequenced);

			const ConstByteView data = encodedMessageBuffer.ReleaseOwnership();
			ENetPacket* pPacket = enet_packet_create(data.GetData(), data.GetDataSize(), rawFlags);
			pPacket->userData = const_cast<ByteType*>(data.GetData());
			pPacket->freeCallback = [](ENetPacket* pPacket)
			{
				EncodedMessageBuffer buffer{ByteView(reinterpret_cast<ByteType*>(pPacket->userData), pPacket->dataLength)};
			};
			enet_host_broadcast(m_pNetHost, channel.Get(), pPacket);
			return true;
		}
		else
		{
			return false;
		}
	}

	void LocalClient::OnBoundObjectAuthorityGiven(RemotePeer, const Channel, ConstBitView& messageView)
	{
		const BoundObjectAuthorityChangedMessage message = DecompressMessageBuffer<BoundObjectAuthorityChangedMessage>(messageView);
		Assert(!m_boundObjectAuthorityMask.IsSet(message.m_boundObjectIdentifier));
		m_boundObjectAuthorityMask.Set(message.m_boundObjectIdentifier);
	}

	void LocalClient::OnBoundObjectAuthorityRevoked(RemotePeer, const Channel, ConstBitView& messageView)
	{
		const BoundObjectAuthorityChangedMessage message = DecompressMessageBuffer<BoundObjectAuthorityChangedMessage>(messageView);
		Assert(m_boundObjectAuthorityMask.IsSet(message.m_boundObjectIdentifier));
		m_boundObjectAuthorityMask.Clear(message.m_boundObjectIdentifier);
	}

	void LocalHost::DelegateBoundObjectAuthority(const BoundObjectIdentifier boundObjectIdentifier, const ClientIdentifier clientIdentifier)
	{
		Assert(m_clientIdentifiers.IsIdentifierActive(clientIdentifier));
		if (!m_boundObjectAuthorityMask.IsSet(boundObjectIdentifier))
		{
			const ClientIdentifier currentClientIdentifier =
				ClientIdentifier::MakeFromValidIndex(m_boundObjectsAuthorityClients[boundObjectIdentifier]);
			Assert(currentClientIdentifier != clientIdentifier);

			const MessageTypeIdentifier::IndexType revokeAuthorityMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
			)DefaultMessageType::BoundObjectAuthorityRevokedFromLocalClient;
			const MessageTypeIdentifier revokeAuthorityMessageTypeIdentifier =
				MessageTypeIdentifier::MakeFromValidIndex(revokeAuthorityMessageTypeIdentifierIndex);
			SendMessageTo(
				currentClientIdentifier,
				revokeAuthorityMessageTypeIdentifier,
				Channel{0},
				BoundObjectAuthorityChangedMessage{boundObjectIdentifier}
			);
		}
		else
		{
			m_boundObjectAuthorityMask.Clear(boundObjectIdentifier);
		}

		const MessageTypeIdentifier::IndexType giveAuthorityMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
		)DefaultMessageType::BoundObjectAuthorityGivenToLocalClient;
		const MessageTypeIdentifier giveAuthorityMessageTypeIdentifier =
			MessageTypeIdentifier::MakeFromValidIndex(giveAuthorityMessageTypeIdentifierIndex);
		SendMessageTo(
			clientIdentifier,
			giveAuthorityMessageTypeIdentifier,
			Channel{0},
			BoundObjectAuthorityChangedMessage{boundObjectIdentifier}
		);

		m_boundObjectsAuthorityClients[boundObjectIdentifier] = clientIdentifier.GetFirstValidIndex();
	}

	void LocalHost::RevokeBoundObjectAuthority(const BoundObjectIdentifier boundObjectIdentifier)
	{
		Assert(!m_boundObjectAuthorityMask.IsSet(boundObjectIdentifier));
		const ClientIdentifier clientIdentifier = ClientIdentifier::MakeFromValidIndex(m_boundObjectsAuthorityClients[boundObjectIdentifier]);

		const MessageTypeIdentifier::IndexType revokeAuthorityMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
		)DefaultMessageType::BoundObjectAuthorityRevokedFromLocalClient;
		const MessageTypeIdentifier revokeAuthorityMessageTypeIdentifier =
			MessageTypeIdentifier::MakeFromValidIndex(revokeAuthorityMessageTypeIdentifierIndex);
		SendMessageTo(
			clientIdentifier,
			revokeAuthorityMessageTypeIdentifier,
			Channel{0},
			BoundObjectAuthorityChangedMessage{boundObjectIdentifier}
		);

		m_boundObjectAuthorityMask.Set(boundObjectIdentifier);
	}

	void LocalHost::OnReceivedForwardRequestToOtherClients(RemotePeer remotePeer, const Channel channel, ConstBitView& messageView)
	{
		// Make a copy of the message to preprocess before we handle or forward
		ConstBitView copiedMessageView = messageView;

		const EnumFlags<MessageTypeFlags> receivableMessageTypeFlags{MessageTypeFlags::ToClient | MessageTypeFlags::FromClient};

		Scripting::VM::Registers registers;
		const Optional<MessageTypeIdentifier> messageTypeIdentifier =
			PreprocessMessage(copiedMessageView, receivableMessageTypeFlags, remotePeer, channel, registers);
		Assert(messageTypeIdentifier.IsValid());
		if (LIKELY(messageTypeIdentifier.IsValid()))
		{
			MessageBuffer messageBuffer((uint32)messageView.GetByteCount());
			messageBuffer.GetView().ZeroInitialize();
			BitView(messageBuffer.GetView(), Math::Range<size>::Make(0, messageView.GetCount())).Pack(messageView);
			EncodedMessageBuffer encodedMessageBuffer{Move(messageBuffer), BitView{}};

			const RemoteClient remoteClient = reinterpret_cast<const RemoteClient&>(remotePeer);
			const ClientIdentifier sourceClientIdentifier = remoteClient.GetIdentifier();
			for (const ClientIdentifier clientIdentifier : m_clientIdentifiers.GetValidElementView(m_clientIdentifiers.GetView()))
			{
				if (clientIdentifier.GetIndex() != sourceClientIdentifier.GetIndex() && m_remoteClients[clientIdentifier].IsValid())
				{
					[[maybe_unused]] const bool wasSent =
						m_remoteClients[clientIdentifier].SendMessageTo(EncodedMessageBuffer(encodedMessageBuffer), channel);
					Assert(wasSent);
				}
			}
			messageView = {};
		}
		else
		{
			LogError("Rejected invalid forwarded message from client");
		}
	}

	void LocalHost::OnReceivedForwardRequestToAllRemotes(RemotePeer remotePeer, const Channel channel, ConstBitView& messageView)
	{
		// Make a copy of the message to preprocess before we handle or forward
		ConstBitView validatedMessageView = messageView;

		const EnumFlags<MessageTypeFlags> receivableMessageTypeFlags{MessageTypeFlags::ToClient | MessageTypeFlags::FromClient};

		Scripting::VM::Registers registers;
		const Optional<MessageTypeIdentifier> messageTypeIdentifier =
			PreprocessMessage(validatedMessageView, receivableMessageTypeFlags, remotePeer, channel, registers);
		Assert(messageTypeIdentifier.IsValid());
		if (LIKELY(messageTypeIdentifier.IsValid()))
		{
			// Check if we also have a local client, in that case don't repeat the same message
			const ArrayView<const ClientIdentifier::IndexType> localClientIdentifiers = m_localClientIdentifiers;
			const RemoteClient remoteClient = reinterpret_cast<const RemoteClient&>(remotePeer);
			const ClientIdentifier sourceClientIdentifier = remoteClient.GetIdentifier();

			ConstBitView hostMessageView{messageView};
			const bool handledMessageLocally = localClientIdentifiers.Contains(sourceClientIdentifier.GetFirstValidIndex()) ||
			                                   HandleMessage(hostMessageView, remotePeer, channel);
			Assert(handledMessageLocally);
			if (LIKELY(handledMessageLocally))
			{
				MessageBuffer messageBuffer((uint32)messageView.GetByteCount());
				messageBuffer.GetView().ZeroInitialize();
				BitView(messageBuffer.GetView(), Math::Range<size>::Make(0, messageView.GetCount())).Pack(messageView);
				EncodedMessageBuffer encodedMessageBuffer{Move(messageBuffer), BitView{}};

				for (const ClientIdentifier clientIdentifier : m_clientIdentifiers.GetValidElementView(m_clientIdentifiers.GetView()))
				{
					if (clientIdentifier.GetIndex() != sourceClientIdentifier.GetIndex() && m_remoteClients[clientIdentifier].IsValid() && !localClientIdentifiers.Contains(clientIdentifier.GetFirstValidIndex()))
					{
						[[maybe_unused]] const bool wasSent =
							m_remoteClients[clientIdentifier].SendMessageTo(EncodedMessageBuffer(encodedMessageBuffer), channel);
						Assert(wasSent);
					}
				}

				messageView = {};
			}
			else
			{
				LogError("Rejected invalid unprocessed forwarded message from client");
			}
		}
		else
		{
			LogError("Rejected invalid forwarded message from client");
		}
	}

	void LocalClient::OnReceiveForwardedMessage(const RemotePeer remotePeer, const Channel channel, ConstBitView& messageView)
	{
		[[maybe_unused]] const bool wasHandled = HandleMessage(messageView, remotePeer, channel);
		Assert(wasHandled);
	}

	void LocalPeer::PerPeerPropagatedPropertyData::ProcessPendingData(
		LocalPeer& localPeer, const EnumFlags<Reflection::PropertyFlags> requiredFlags, RemotePeer remotePeer, const Channel channel
	)
	{
		if (m_flags.IsNotSet(Flags::HasPendingDataToSend))
		{
			return;
		}

		// TODO: Expose tweaking per type, currently optimizing for players
		const Math::Frequencyd updateFrequency = 120_hz;

		Threading::UniqueLock lock(m_typeLookupMapMutex);
		Assert(m_typeLookupMap.HasElements());
		for (TypeMap::PairType& __restrict typePair : m_typeLookupMap)
		{
			const MessageTypeIdentifier messageTypeIdentifier = MessageTypeIdentifier::MakeFromValidIndex(typePair.first);
			const MessageType& __restrict messageType = localPeer.m_messageTypes[messageTypeIdentifier];

			TypeInfo& __restrict typeInfo = *typePair.second;
			Threading::UniqueLock typeLock(typeInfo.m_mutex);

			const Time::Timestamp elapsedTime = Time::Timestamp::GetCurrent() - typeInfo.m_lastSendTime;
			if (elapsedTime < Time::Timestamp{updateFrequency})
			{
				continue;
			}

			// Whether the data has changed since we last sent a sequence
			const bool hasTypeDataChanged = typeInfo.m_changed;
			const Optional<uint16> newSequenceNumber = hasTypeDataChanged ? typeInfo.m_sendWindow.GetNewSequenceNumber()
			                                                              : typeInfo.m_sendWindow.GetLastSentSequenceNumber();
			if (newSequenceNumber.IsInvalid())
			{
				continue;
			}

			const ArrayView<const Guid, PropertyIndex> typePropertyGuids = localPeer.m_propagatedPropertyTypeGuids[messageTypeIdentifier];
			const PropertyIndex typeMaximumPropertyCount = typePropertyGuids.GetSize();
			const PropertyIndex typePropertyMaskBitCount = (PropertyIndex)Memory::GetBitWidth((1u << typeMaximumPropertyCount) - 1u);

			const BoundObjectIdentifier::IndexType boundObjectCount = (BoundObjectIdentifier::IndexType
			)typeInfo.m_objectPropertyMaskMap.GetSize();
			Assert(boundObjectCount > 0);

			uint32 totalMessageBitCount{0};

			// Calculate message size
			{
				// OnReceivedPropertyStream message type guid
				totalMessageBitCount += CalculateFixedCompressedDataSize<MessageTypeIdentifier>();
				// Sequence number
				totalMessageBitCount += CalculateFixedCompressedDataSize<SequenceNumber>();
				// Number of objects
				totalMessageBitCount += CalculateFixedCompressedDataSize<BoundObjectIdentifier>();

				// Bound object identifiers & property mask
				const uint32 objectHeaderBitCount = CalculateFixedCompressedDataSize<BoundObjectIdentifier>() + typePropertyMaskBitCount;
				totalMessageBitCount += objectHeaderBitCount * boundObjectCount;

				for (const ObjectPropertyMaskMap::PairType& __restrict objectPropertyMaskPair : typeInfo.m_objectPropertyMaskMap)
				{
					const PropertyMask& __restrict propertyMask = objectPropertyMaskPair.second;
					Assert(propertyMask.AreAnySet());

					for (const PropertyIndex propertyIndex : propertyMask.GetSetBitsIterator())
					{
						const Guid propertyGuid = typePropertyGuids[propertyIndex];
						const auto propertyIt = localPeer.m_propagatedPropertyLookupMap.Find(propertyGuid);
						Assert(propertyIt != localPeer.m_propagatedPropertyLookupMap.end());
						if (LIKELY(propertyIt != localPeer.m_propagatedPropertyLookupMap.end()))
						{
							const PropagatedProperty& __restrict propagatedProperty = propertyIt->second;
							const Reflection::TypeDefinition& __restrict propertyTypeDefinition = propagatedProperty.propertyInfo.m_typeDefinition;
							totalMessageBitCount += propertyTypeDefinition.CalculateFixedCompressedDataSize(requiredFlags);

							const BoundObjectIdentifier boundObjectIdentifier = BoundObjectIdentifier::MakeFromValidIndex(objectPropertyMaskPair.first);
							const Optional<Reflection::PropertyOwner*> pPropertyOwner =
								localPeer.GetBoundObjectPropertyOwner(boundObjectIdentifier, messageType.m_functionGuid, messageType.m_flags);
							if (LIKELY(pPropertyOwner.IsValid()))
							{
								Any propertyValue = propagatedProperty.dynamicPropertyInstance.GetValue(*pPropertyOwner, Invalid);
								totalMessageBitCount +=
									propertyTypeDefinition.CalculateObjectDynamicCompressedDataSize(propertyValue.GetData(), requiredFlags);
							}
						}
					}
				}
			}

			MessageBuffer messageBuffer(localPeer.AcquireMessageBuffer(totalMessageBitCount));
			BitView targetView = messageBuffer.GetView();

			bool compressedAll = Compression::Compress(messageTypeIdentifier, targetView, requiredFlags);
			// Write the sequence number
			compressedAll &= Compression::Compress(*newSequenceNumber, targetView, requiredFlags);
			// Write the number of bound objects
			compressedAll &= Compression::Compress(BoundObjectIdentifier::MakeFromIndex(boundObjectCount), targetView, requiredFlags);

			// Write the objects
			for (const ObjectPropertyMaskMap::PairType& __restrict objectPropertyMaskPair : typeInfo.m_objectPropertyMaskMap)
			{
				const BoundObjectIdentifier boundObjectIdentifier = BoundObjectIdentifier::MakeFromValidIndex(objectPropertyMaskPair.first);

				// Write the object identifier
				compressedAll &= Compression::Compress(boundObjectIdentifier, targetView, requiredFlags);
				// Write the property mask
				const PropertyMask& __restrict propertyMask = objectPropertyMaskPair.second;
				compressedAll &= targetView.PackAndSkip(ConstBitView::Make(propertyMask, Math::Range<size>::Make(0, typePropertyMaskBitCount)));

				for (const PropertyIndex propertyIndex : propertyMask.GetSetBitsIterator())
				{
					const Guid propertyGuid = typePropertyGuids[propertyIndex];
					const auto propertyIt = localPeer.m_propagatedPropertyLookupMap.Find(propertyGuid);
					Assert(propertyIt != localPeer.m_propagatedPropertyLookupMap.end());
					if (LIKELY(propertyIt != localPeer.m_propagatedPropertyLookupMap.end()))
					{
						const PropagatedProperty& __restrict propagatedProperty = propertyIt->second;
						const Reflection::TypeDefinition& __restrict propertyTypeDefinition = propagatedProperty.propertyInfo.m_typeDefinition;

						const Optional<Reflection::PropertyOwner*> pPropertyOwner =
							localPeer.GetBoundObjectPropertyOwner(boundObjectIdentifier, messageType.m_functionGuid, messageType.m_flags);
						if (LIKELY(pPropertyOwner.IsValid()))
						{
							Any propertyValue = propagatedProperty.dynamicPropertyInstance.GetValue(*pPropertyOwner, Invalid);

							compressedAll &= propertyTypeDefinition.CompressStoredObject(
								propertyValue.GetData(),
								targetView,
								Reflection::PropertyFlags::PropagateClientToHost
							);
						}
						else
						{
							compressedAll = false;
						}
					}
					else
					{
						compressedAll = false;
					}
				}
			}

			Assert(compressedAll);
			if (LIKELY(compressedAll))
			{
				EncodedMessageBuffer encodedMessageBuffer{Move(messageBuffer), targetView};

				remotePeer.SendMessageTo(Move(encodedMessageBuffer), channel, MessageFlags::UnreliableUnsequenced);
				typeInfo.m_lastSendTime = Time::Timestamp::GetCurrent();
				typeInfo.m_changed = false;
				if (hasTypeDataChanged)
				{
					typeInfo.m_sendWindow.OnSequenceSent(*newSequenceNumber);
				}
			}
		}
	}

	void LocalClient::OnReceivedConfirmPropertyReceipt(RemotePeer, const Channel, ConstBitView& messageView)
	{
		const ConfirmPropagatedPropertyReceiptMessage message = DecompressMessageBuffer<ConfirmPropagatedPropertyReceiptMessage>(messageView);
		m_toHostPropagatedPropertyData.ProcessConfirmationReceipt(message);
	}

	void LocalHost::OnReceivedConfirmPropertyReceipt(RemotePeer remotePeer, const Channel, ConstBitView& messageView)
	{
		const ConfirmPropagatedPropertyReceiptMessage message = DecompressMessageBuffer<ConfirmPropagatedPropertyReceiptMessage>(messageView);
		RemoteClient remoteClient = reinterpret_cast<RemoteClient&>(remotePeer);

		const Optional<PerPeerPropagatedPropertyData*> pPropagatedPropertyData =
			m_perClientPropagatedPropertyData[remoteClient.GetIdentifier()];
		Assert(pPropagatedPropertyData.IsValid());
		if (LIKELY(pPropagatedPropertyData.IsValid()))
		{
			pPropagatedPropertyData->ProcessConfirmationReceipt(message);
		}
	}

	void LocalPeer::PerPeerPropagatedPropertyData::ProcessConfirmationReceipt(const ConfirmPropagatedPropertyReceiptMessage message)
	{
		Threading::UniqueLock lock(m_typeLookupMapMutex);
		for (auto it = m_typeLookupMap.begin(), endIt = m_typeLookupMap.end(); it != endIt;)
		{
			TypeInfo& __restrict typeInfo = *it->second;
			Threading::UniqueLock typeLock(typeInfo.m_mutex);

			switch (typeInfo.m_sendWindow.OnSequenceAcknowledged(message.m_sequenceNumber))
			{
				case SendWindow::AcknowledgmentResult::Rejected:
					++it;
					break;
				case SendWindow::AcknowledgmentResult::Accepted:
					++it;
					break;
				case SendWindow::AcknowledgmentResult::AcceptedLastSentSequence:
				{
					if (!typeInfo.m_changed)
					{
						typeLock.Unlock();
						it = m_typeLookupMap.Remove(it);
						endIt = m_typeLookupMap.end();
					}
					else
					{
						++it;
					}
				}
				break;
			}
		}

		if (m_typeLookupMap.IsEmpty())
		{
			m_flags.Clear(Flags::HasPendingDataToSend);
		}

		/*if (m_inFlightSequenceNumbers.IsSet(receivedSequenceNumber))
		{
		  if (receivedSequenceNumber <= lastSequenceNumber)
		  {
		    const Memory::BitIndex<SequenceNumber> firstSetIndex = m_inFlightSequenceNumbers.GetFirstSetIndex();
		    Assert(firstSetIndex.IsValid());

		    // Clear the received sequence number and any others that came before it
		    for (SequenceNumber i = *firstSetIndex; i < (uint64(receivedSequenceNumber) + 1); ++i)
		    {
		      m_inFlightSequenceNumbers.Clear((SequenceNumber)i);
		    }

		    const IdentifierMask<BoundObjectIdentifier> invalidatedObjectStreams = m_invalidatedObjectStreams;
		    for (const BoundObjectIdentifier::IndexType boundObjectIdentifierIndex : invalidatedObjectStreams.GetSetBitsIterator())
		    {
		      const BoundObjectIdentifier boundObjectIdentifier = BoundObjectIdentifier::MakeFromValidIndex(boundObjectIdentifierIndex);

		      Threading::Atomic<SequenceNumber>& lastSentSequenceNumberReference = m_lastSentSequenceNumbers[boundObjectIdentifier];
		      const SequenceNumber lastSentSequenceNumber = lastSentSequenceNumberReference;
		      if (lastSentSequenceNumber >= *firstSetIndex && lastSentSequenceNumber <= receivedSequenceNumber)
		      {
		        m_invalidatedObjectStreams.Clear(boundObjectIdentifier);
		      }
		    }
		  }
		  else // if(receivedSequenceNumber > lastSequenceNumber)
		  {
		    Assert(false, "TODO");
		  }
		}
		else
		{
		  // discard
		}*/
	}

	LocalPeer::PerPeerPropagatedPropertyData::TypeInfo&
	LocalPeer::PerPeerPropagatedPropertyData::GetOrEmplaceTypeInfo(const MessageTypeIdentifier messageTypeIdentifier)
	{
		{
			Threading::SharedLock lock(m_typeLookupMapMutex);
			auto typeIt = m_typeLookupMap.Find(messageTypeIdentifier.GetFirstValidIndex());
			if (typeIt != m_typeLookupMap.end())
			{
				return *typeIt->second;
			}
		}

		Threading::UniqueLock lock(m_typeLookupMapMutex);
		auto typeIt = m_typeLookupMap.Find(messageTypeIdentifier.GetFirstValidIndex());
		if (typeIt == m_typeLookupMap.end())
		{
			typeIt = m_typeLookupMap.Emplace(messageTypeIdentifier.GetFirstValidIndex(), UniqueRef<TypeInfo>::Make());
		}
		return *typeIt->second;
	}

	void LocalPeer::PerPeerPropagatedPropertyData::Invalidate(
		const MessageTypeIdentifier messageTypeIdentifier,
		const BoundObjectIdentifier boundObjectIdentifier,
		const PropertyMask invalidatedPropertyMask
	)
	{
		{
			TypeInfo& __restrict typeInfo = GetOrEmplaceTypeInfo(messageTypeIdentifier);
			Threading::UniqueLock lock(typeInfo.m_mutex);

			auto objectIt = typeInfo.m_objectPropertyMaskMap.Find(boundObjectIdentifier.GetFirstValidIndex());
			if (objectIt == typeInfo.m_objectPropertyMaskMap.end())
			{
				objectIt = typeInfo.m_objectPropertyMaskMap.Emplace(boundObjectIdentifier.GetFirstValidIndex(), PropertyMask{});
			}

			PropertyMask& __restrict propertyMask = objectIt->second;
			propertyMask |= invalidatedPropertyMask;
			typeInfo.m_changed = true;
		}
		m_flags |= Flags::HasPendingDataToSend;
	}

	void LocalPeer::PerPeerPropagatedPropertyData::FlushProperties(const MessageTypeIdentifier messageTypeIdentifier)
	{
		Threading::SharedLock lock(m_typeLookupMapMutex);
		auto typeIt = m_typeLookupMap.Find(messageTypeIdentifier.GetFirstValidIndex());
		if (typeIt != m_typeLookupMap.end())
		{
			TypeInfo& __restrict typeInfo = *typeIt->second;
			lock.Unlock();

			Threading::UniqueLock typeLock(typeInfo.m_mutex);
			typeInfo.m_lastSendTime = Time::Timestamp{};
		}
	}

	void LocalPeer::PerPeerPropagatedPropertyData::Reset()
	{
		m_flags.Clear();
		{
			Threading::UniqueLock lock(m_typeLookupMapMutex);
			m_typeLookupMap.Clear();
		}
	}

	Time::Timestamp LocalHost::GetClientRoundTripTime(const ClientIdentifier clientIdentifier) const
	{
		return Time::Timestamp::FromMilliseconds(m_remoteClients[clientIdentifier].m_pNetPeer->roundTripTime);
	}

	void LocalHost::OnReceivedTimeSyncRequest(RemotePeer remotePeer, const Channel channel, ConstBitView& messageView)
	{
		// message.m_hostTimestamp = T1
		// message.m_clientReceivedTimestamp = T2
		// message.m_clientSentTimestamp = T3
		const Time::Timestamp receiptTime = m_lastUpdateTime; // T4

		const RequestTimeSyncMessage message = DecompressMessageBuffer<RequestTimeSyncMessage>(messageView);
		RemoteClient remoteClient = reinterpret_cast<RemoteClient&>(remotePeer);

		const int64 offsetInNanoseconds = (((int64)message.m_clientReceivedTimestamp.GetNanoseconds() -
		                                    (int64)message.m_hostTimestamp.GetNanoseconds()) +
		                                   ((int64)message.m_clientSentTimestamp.GetNanoseconds() - (int64)receiptTime.GetNanoseconds())) /
		                                  2;

		/*const Time::Timestamp roundTripTime = receiptTime - message.m_hostTimestamp;

		// We assume symmetry, so half of RTT is an approximation.
		const Time::Timestamp oneWayLatency = roundTripTime / 2;
		const Time::Timestamp hostTimeOffsetForLatency = message.m_hostTimestamp + oneWayLatency;
		const int64 offsetInNanoseconds = static_cast<int64>(message.m_clientTimestamp.GetNanoseconds()) -
		  static_cast<int64>(hostTimeOffsetForLatency.GetNanoseconds());*/

		// Calculate how far ahead (or behind) the server clock is compared to the client clock at the moment the host sent the request
		m_clientTimeOffsetNanoseconds[remoteClient.GetIdentifier()] = offsetInNanoseconds;

		const MessageTypeIdentifier::IndexType receivedTimeSyncResponseMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
		)DefaultMessageType::ReceivedTimeSyncResponse;
		const MessageTypeIdentifier receivedTimeSyncResponseMessageTypeIdentifier =
			MessageTypeIdentifier::MakeFromValidIndex(receivedTimeSyncResponseMessageTypeIdentifierIndex);
		remoteClient.SendMessageTo(
			EncodeMessageBuffer(
				receivedTimeSyncResponseMessageTypeIdentifier,
				AcquireMessageBuffer(receivedTimeSyncResponseMessageTypeIdentifier),
				ReceivedTimeSyncResponseMessage{-offsetInNanoseconds}
			),
			channel
		);
		// Make sure the time response is sent immediately
		FlushPendingMessages();
	}

	ClientIdentifier RemoteClient::GetIdentifier() const
	{
		return ClientIdentifier::MakeFromValue((uint32) reinterpret_cast<uint64>(m_pNetPeer->data));
	}

	void RemoteClient::OnConnected(const ClientIdentifier identifier)
	{
		m_pNetPeer->data = reinterpret_cast<void*>((uint64)identifier.GetValue());
	}
	void RemoteClient::OnDisconnected()
	{
		m_pNetPeer->data = nullptr;
	}

	[[maybe_unused]] const bool wasLocalClientRegistered = Reflection::Registry::RegisterType<LocalClient>();
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::Network::Manager>();
#else
extern "C" NETWORKINGCORE_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::Network::Manager(application);
}
#endif
