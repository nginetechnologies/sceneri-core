#pragma once

#include <NetworkingCore/LocalPeer.h>
#include <NetworkingCore/Client/RemoteClient.h>
#include <Common/Network/Port.h>
#include <NetworkingCore/Channel.h>
#include <NetworkingCore/Message/DefaultMessageType.h>

#include <Common/IO/URI.h>
#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Function/Event.h>
#include <Common/Memory/UniquePtr.h>

namespace ngine::Network
{
	struct Address;

	struct LocalHost final : public LocalPeer
	{
		LocalHost();
		LocalHost(const LocalHost& other) = delete;
		LocalHost(LocalHost&&) = delete;
		LocalHost& operator=(const LocalHost&) = delete;
		LocalHost& operator=(LocalHost&&) = delete;
		~LocalHost();

		bool Start(
			const Address address,
			const uint32 maximumClientCount,
			const uint8 maximumChannelCount = 2,
			const uint32 incomingBandwidth = 0,
			const uint32 outgoingBandwidth = 0
		);
		void Stop();

		[[nodiscard]] bool IsStarted() const
		{
			return m_pNetHost != nullptr;
		}

		[[nodiscard]] Address GetLocalAddress() const;
		[[nodiscard]] Address GetPublicAddress() const;

		bool BroadcastMessage(
			EncodedMessageBuffer&& messageBuffer, const Channel channel, const EnumFlags<MessageFlags> messageFlags = MessageFlags::Reliable
		);

		[[nodiscard]] RemoteClient GetRemoteClient(const ClientIdentifier identifier) const
		{
			return m_remoteClients[identifier];
		}

		MessageTypeIdentifier RegisterMessageType(MessageType&& messageType)
		{
			const MessageTypeIdentifier messageTypeIdentifier = m_messageTypeIdentifiers.AcquireIdentifier();

			m_functionMessageTypes.Set(messageTypeIdentifier);

			if (IsValid())
			{
				const MessageTypeIdentifier::IndexType registerMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
				)DefaultMessageType::RegisterNewMessageType;
				const MessageTypeIdentifier registerMessageTypeIdentifier =
					MessageTypeIdentifier::MakeFromValidIndex(registerMessageTypeIdentifierIndex);
				const RegisterMessageTypeMessage messageData{
					messageTypeIdentifier,
					messageType.m_functionGuid,
					messageType.m_fixedCompressedDataSizeInBits
				};
				const Channel channel{0};
				BroadcastMessageToAllClients(registerMessageTypeIdentifier, channel, messageData);
			}

			LocalPeer::RegisterMessageType(messageTypeIdentifier, Forward<MessageType>(messageType));

			return messageTypeIdentifier;
		}

		[[nodiscard]] MessageType CreateMessageType(
			const Optional<const Reflection::TypeInterface*> pOwningTypeInterface,
			const Reflection::FunctionInfo& __restrict functionInfo,
			const Reflection::FunctionData& __restrict functionData
		) const
		{
			MessageType messageType = LocalPeer::CreateMessageType(functionInfo.m_guid, 0, pOwningTypeInterface, functionInfo, functionData);

			uint32 compressedDataSizeInBits = CalculateFixedCompressedDataSize<MessageTypeIdentifier>();
			if (pOwningTypeInterface.IsValid())
			{
				compressedDataSizeInBits += CalculateFixedCompressedDataSize<BoundObjectIdentifier>();
			}
			for (const Reflection::TypeDefinition& __restrict functionArgumentTypeDefinition : messageType.m_argumentTypeDefinitions)
			{
				compressedDataSizeInBits +=
					functionArgumentTypeDefinition.CalculateFixedCompressedDataSize(Reflection::PropertyFlags::SentWithNetworkedFunctions);
			}
			messageType.m_fixedCompressedDataSizeInBits = (uint16)compressedDataSizeInBits;

			return Move(messageType);
		}

		BoundObjectIdentifier BindObject(const Guid persistentObjectGuid, const AnyView object)
		{
			const BoundObjectIdentifier boundObjectIdentifier = m_boundObjectIdentifiers.AcquireIdentifier();

			m_boundObjectIdentifierGuids[boundObjectIdentifier] = persistentObjectGuid;
			m_boundObjects[boundObjectIdentifier] = object;

			{
				const MessageTypeIdentifier::IndexType bindObjectMessageTypeIdentifierIndex = (MessageTypeIdentifier::IndexType
				)DefaultMessageType::ObjectBound;
				const MessageTypeIdentifier bindObjectMessageTypeIdentifier =
					MessageTypeIdentifier::MakeFromValidIndex(bindObjectMessageTypeIdentifierIndex);
				const ObjectBoundMessage messageData{boundObjectIdentifier, persistentObjectGuid};
				const Channel channel{0};
				BroadcastMessageToAllClients(bindObjectMessageTypeIdentifier, channel, messageData);
			}

			return boundObjectIdentifier;
		}
		void BindObject(const BoundObjectIdentifier boundObjectIdentifier, const AnyView object)
		{
			m_boundObjects[boundObjectIdentifier] = object;
		}
		void UnbindObject(const BoundObjectIdentifier objectIdentifier)
		{
			m_boundObjectIdentifierGuids[objectIdentifier] = {};
			m_boundObjectIdentifiers.ReturnIdentifier(objectIdentifier);
			m_boundObjects[objectIdentifier] = {};

			// Note: Not notifying other clients on unbind to save bandwidth, that is up to the user.
		}
		template<typename Type>
		[[nodiscard]] Optional<Type*> GetBoundObject(const BoundObjectIdentifier boundObjectIdentifier) const
		{
			return const_cast<LocalHost&>(*this).m_boundObjects[boundObjectIdentifier].Get<Type>();
		}

		//! Delegates authority over a bound object to the specified client
		void DelegateBoundObjectAuthority(const BoundObjectIdentifier boundObjectIdentifier, const ClientIdentifier clientIdentifier);
		//! Revokes bound object authority from a remote client and returns it to the host
		void RevokeBoundObjectAuthority(const BoundObjectIdentifier boundObjectIdentifier);

		using LocalPeer::SendMessageTo;
		bool SendMessageTo(
			const ClientIdentifier clientIdentifier,
			const MessageTypeIdentifier messageTypeIdentifier,
			const Channel channel,
			EncodedMessageBuffer&& encodedMessageBuffer
		)
		{
			const bool canSend =
				m_sendableMessageTypeFlags.AreAllSet(m_messageTypes[messageTypeIdentifier].m_flags & MessageTypeFlags::HostToClient);
			Assert(canSend);
			if (LIKELY(canSend))
			{
				return LocalPeer::SendMessageTo(
					m_remoteClients[clientIdentifier],
					messageTypeIdentifier,
					channel,
					Forward<EncodedMessageBuffer>(encodedMessageBuffer)
				);
			}
			return false;
		}
		template<typename... MessageTypes>
		bool SendMessageTo(
			const ClientIdentifier clientIdentifier,
			const MessageTypeIdentifier messageTypeIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		)
		{
			return SendMessageTo(
				clientIdentifier,
				messageTypeIdentifier,
				channel,
				EncodeMessageBuffer(messageTypeIdentifier, AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...), messages...)
			);
		}
		template<typename... MessageTypes>
		bool SendMessageTo(
			const ClientIdentifier clientIdentifier,
			const MessageTypeIdentifier messageTypeIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			EncodedMessageBuffer&& encodedMessageBuffer
		)
		{
			const bool canSend =
				m_sendableMessageTypeFlags.AreAllSet(m_messageTypes[messageTypeIdentifier].m_flags & MessageTypeFlags::HostToClient);
			Assert(canSend);
			if (LIKELY(canSend))
			{
				return LocalPeer::SendMessageTo(
					m_remoteClients[clientIdentifier],
					messageTypeIdentifier,
					boundObjectIdentifier,
					channel,
					Forward<EncodedMessageBuffer>(encodedMessageBuffer)
				);
			}
			return false;
		}
		template<typename... MessageTypes>
		bool SendMessageTo(
			const ClientIdentifier clientIdentifier,
			const MessageTypeIdentifier messageTypeIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		)
		{
			return SendMessageTo(
				clientIdentifier,
				messageTypeIdentifier,
				channel,
				EncodeMessageBuffer(
					messageTypeIdentifier,
					boundObjectIdentifier,
					AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...),
					messages...
				)
			);
		}
		template<auto Function>
		bool SendMessageTo(
			const ClientIdentifier clientIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			EncodedMessageBuffer&& encodedMessageBuffer
		)
		{
			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				return SendMessageTo(
					clientIdentifier,
					messageTypeIdentifier,
					boundObjectIdentifier,
					channel,
					Forward<EncodedMessageBuffer>(encodedMessageBuffer)
				);
			}
			return false;
		}
		template<auto Function, typename... MessageTypes>
		bool SendMessageTo(
			const ClientIdentifier clientIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		)
		{
			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier<Function>();
			return SendMessageTo(
				clientIdentifier,
				messageTypeIdentifier,
				channel,
				EncodeMessageBuffer(
					messageTypeIdentifier,
					boundObjectIdentifier,
					AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...),
					messages...
				)
			);
		}

		bool BroadcastMessageToAllClients(
			const MessageTypeIdentifier messageTypeIdentifier, const Channel channel, EncodedMessageBuffer&& encodedMessageBuffer
		)
		{
			const bool canSend =
				m_sendableMessageTypeFlags.AreAllSet(m_messageTypes[messageTypeIdentifier].m_flags & MessageTypeFlags::HostToAllClients);
			Assert(canSend);
			if (LIKELY(canSend && encodedMessageBuffer.IsValid()))
			{
				return BroadcastMessage(Forward<EncodedMessageBuffer>(encodedMessageBuffer), channel);
			}
			return false;
		}
		template<typename... MessageTypes>
		bool
		BroadcastMessageToAllClients(const MessageTypeIdentifier messageTypeIdentifier, const Channel channel, const MessageTypes&... messages)
		{
			return BroadcastMessageToAllClients(
				messageTypeIdentifier,
				channel,
				EncodeMessageBuffer(messageTypeIdentifier, AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...), messages...)
			);
		}
		template<typename... MessageTypes>
		bool BroadcastMessageToAllClients(
			const MessageTypeIdentifier messageTypeIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		)
		{
			return BroadcastMessageToAllClients(
				messageTypeIdentifier,
				channel,
				EncodeMessageBuffer(
					messageTypeIdentifier,
					boundObjectIdentifier,
					AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...),
					messages...
				)
			);
		}
		template<auto Function>
		bool BroadcastMessageToAllClients(const Channel channel, EncodedMessageBuffer&& encodedMessageBuffer)
		{
			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				return BroadcastMessageToAllClients(messageTypeIdentifier, channel, Forward<EncodedMessageBuffer>(encodedMessageBuffer));
			}
			return false;
		}
		template<auto Function, typename... MessageTypes>
		bool
		BroadcastMessageToAllClients(const BoundObjectIdentifier boundObjectIdentifier, const Channel channel, const MessageTypes&... messages)
		{
			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				return BroadcastMessageToAllClients(
					messageTypeIdentifier,
					channel,
					EncodeMessageBuffer(
						messageTypeIdentifier,
						boundObjectIdentifier,
						AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...),
						messages...
					)
				);
			}
			return false;
		}

		bool BroadcastMessageToOtherClients(
			const ClientIdentifier clientIdentifier,
			const MessageTypeIdentifier messageTypeIdentifier,
			const Channel channel,
			EncodedMessageBuffer&& encodedMessageBuffer
		)
		{
			const bool canSend =
				m_sendableMessageTypeFlags.AreAllSet(m_messageTypes[messageTypeIdentifier].m_flags & MessageTypeFlags::HostToAllClients);
			Assert(canSend);
			if (LIKELY(canSend && encodedMessageBuffer.IsValid()))
			{
				bool wereAllSent{true};
				for (const ClientIdentifier otherClientIdentifier : m_clientIdentifiers.GetValidElementView(m_clientIdentifiers.GetView()))
				{
					if (otherClientIdentifier.GetIndex() != clientIdentifier.GetIndex() && m_remoteClients[otherClientIdentifier].IsValid())
					{
						const bool wasSent = m_remoteClients[otherClientIdentifier].SendMessageTo(EncodedMessageBuffer(encodedMessageBuffer), channel);
						Assert(wasSent);
						wereAllSent &= wasSent;
					}
				}
				return wereAllSent;
			}
			return false;
		}
		template<typename... MessageTypes>
		bool BroadcastMessageToOtherClients(
			const ClientIdentifier clientIdentifier,
			const MessageTypeIdentifier messageTypeIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		)
		{
			return BroadcastMessageToOtherClients(
				clientIdentifier,
				messageTypeIdentifier,
				channel,
				EncodeMessageBuffer(messageTypeIdentifier, AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...), messages...)
			);
		}
		template<typename... MessageTypes>
		bool BroadcastMessageToOtherClients(
			const ClientIdentifier clientIdentifier,
			const MessageTypeIdentifier messageTypeIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		)
		{
			return BroadcastMessageToOtherClients(
				clientIdentifier,
				messageTypeIdentifier,
				channel,
				EncodeMessageBuffer(
					messageTypeIdentifier,
					boundObjectIdentifier,
					AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...),
					messages...
				)
			);
		}
		template<auto Function>
		bool BroadcastMessageToOtherClients(
			const ClientIdentifier clientIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			EncodedMessageBuffer&& encodedMessageBuffer
		)
		{
			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				return BroadcastMessageToOtherClients(
					clientIdentifier,
					messageTypeIdentifier,
					boundObjectIdentifier,
					channel,
					Forward<EncodedMessageBuffer>(encodedMessageBuffer)
				);
			}
			return false;
		}
		template<auto Function, typename... MessageTypes>
		bool BroadcastMessageToOtherClients(
			const ClientIdentifier clientIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		)
		{
			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				return BroadcastMessageToOtherClients<MessageTypes...>(
					clientIdentifier,
					messageTypeIdentifier,
					boundObjectIdentifier,
					channel,
					messages...
				);
			}
			return false;
		}

		bool BroadcastMessageToRemoteClients(
			const MessageTypeIdentifier messageTypeIdentifier, const Channel channel, EncodedMessageBuffer&& encodedMessageBuffer
		)
		{
			const bool canSend =
				m_sendableMessageTypeFlags.AreAllSet(m_messageTypes[messageTypeIdentifier].m_flags & MessageTypeFlags::HostToAllClients);
			Assert(canSend);
			if (LIKELY(canSend && encodedMessageBuffer.IsValid()))
			{
				bool wereAllSent{true};
				for (const ClientIdentifier otherClientIdentifier : m_clientIdentifiers.GetValidElementView(m_clientIdentifiers.GetView()))
				{
					if (!IsLocalClient(otherClientIdentifier) && m_remoteClients[otherClientIdentifier].IsValid())
					{
						const bool wasSent = m_remoteClients[otherClientIdentifier].SendMessageTo(EncodedMessageBuffer(encodedMessageBuffer), channel);
						Assert(wasSent);
						wereAllSent &= wasSent;
					}
				}
				return wereAllSent;
			}
			return false;
		}
		template<typename... MessageTypes>
		bool BroadcastMessageToRemoteClients(
			const MessageTypeIdentifier messageTypeIdentifier, const Channel channel, const MessageTypes&... messages
		)
		{
			return BroadcastMessageToRemoteClients(
				messageTypeIdentifier,
				channel,
				EncodeMessageBuffer(messageTypeIdentifier, AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...), messages...)
			);
		}
		template<typename... MessageTypes>
		bool BroadcastMessageToRemoteClients(
			const MessageTypeIdentifier messageTypeIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		)
		{
			return BroadcastMessageToRemoteClients(
				messageTypeIdentifier,
				boundObjectIdentifier,
				channel,
				EncodeMessageBuffer(
					messageTypeIdentifier,
					boundObjectIdentifier,
					AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...),
					messages...
				)
			);
		}
		template<auto Function>
		bool BroadcastMessageToRemoteClients(
			const BoundObjectIdentifier boundObjectIdentifier, const Channel channel, EncodedMessageBuffer&& encodedMessageBuffer
		)
		{
			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				return BroadcastMessageToRemoteClients(
					messageTypeIdentifier,
					boundObjectIdentifier,
					channel,
					Forward<EncodedMessageBuffer>(encodedMessageBuffer)
				);
			}
			return false;
		}
		template<auto Function, typename... MessageTypes>
		bool BroadcastMessageToRemoteClients(
			const BoundObjectIdentifier boundObjectIdentifier, const Channel channel, const MessageTypes&... messages
		)
		{
			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				return BroadcastMessageToRemoteClients<MessageTypes...>(messageTypeIdentifier, boundObjectIdentifier, channel, messages...);
			}
			return false;
		}

		template<auto FirstMember, auto... Members>
		bool InvalidatePropertiesToClient(const ClientIdentifier clientIdentifier, const BoundObjectIdentifier boundObjectIdentifier)
		{
			using OwnerType = typename TypeTraits::MemberOwnerType<decltype(FirstMember)>;
			const auto& reflectedOwner = Reflection::GetType<OwnerType>();
			const Guid propertyStreamMessageGuid = reflectedOwner.GetGuid();

			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier(propertyStreamMessageGuid);
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				const bool canSend =
					m_sendableMessageTypeFlags.AreAllSet(m_messageTypes[messageTypeIdentifier].m_flags & MessageTypeFlags::HostToAllClients);
				Assert(canSend);
				if (LIKELY(canSend))
				{
					const bool canPassAuthorityCheck = HasAuthorityOfBoundObject(boundObjectIdentifier);
					if (LIKELY(canPassAuthorityCheck))
					{
						PropertyMask invalidatedPropertyMask;
						const bool retrievedMask = GetPropertyMask<FirstMember, Members...>(invalidatedPropertyMask);
						Assert(retrievedMask);
						if (LIKELY(retrievedMask))
						{
							const Optional<PerPeerPropagatedPropertyData*> pPropagatedPropertyData = m_perClientPropagatedPropertyData[clientIdentifier];
							Assert(pPropagatedPropertyData.IsValid());
							if (LIKELY(pPropagatedPropertyData.IsValid()))
							{
								pPropagatedPropertyData->Invalidate(messageTypeIdentifier, boundObjectIdentifier, invalidatedPropertyMask);
								return true;
							}
						}
					}
				}
			}
			return false;
		}
		template<auto FirstMember, auto... Members>
		bool InvalidatePropertiesToAllClients(const BoundObjectIdentifier boundObjectIdentifier)
		{
			using OwnerType = typename TypeTraits::MemberOwnerType<decltype(FirstMember)>;
			const auto& reflectedOwner = Reflection::GetType<OwnerType>();
			const Guid propertyStreamMessageGuid = reflectedOwner.GetGuid();

			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier(propertyStreamMessageGuid);
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				const bool canSend =
					m_sendableMessageTypeFlags.AreAllSet(m_messageTypes[messageTypeIdentifier].m_flags & MessageTypeFlags::HostToAllClients);
				Assert(canSend);
				if (LIKELY(canSend))
				{
					const bool canPassAuthorityCheck = HasAuthorityOfBoundObject(boundObjectIdentifier);
					if (LIKELY(canPassAuthorityCheck))
					{
						PropertyMask invalidatedPropertyMask;
						const bool retrievedMask = GetPropertyMask<FirstMember, Members...>(invalidatedPropertyMask);
						Assert(retrievedMask);
						if (LIKELY(retrievedMask))
						{
							for (const ClientIdentifier clientIdentifier : m_clientIdentifiers.GetValidElementView(m_clientIdentifiers.GetView()))
							{
								if (const Optional<PerPeerPropagatedPropertyData*> pPropagatedPropertyData = m_perClientPropagatedPropertyData[clientIdentifier])
								{
									pPropagatedPropertyData->Invalidate(messageTypeIdentifier, boundObjectIdentifier, invalidatedPropertyMask);
								}
							}
							return true;
						}
					}
				}
			}
			return false;
		}

		//! Called to force ignoring of the update frequency for a frame
		//! Useful for immediate actions such as jumps etc
		template<auto FirstMember, auto... Members>
		bool FlushPropertiesToClient(const ClientIdentifier clientIdentifier)
		{
			using OwnerType = typename TypeTraits::MemberOwnerType<decltype(FirstMember)>;
			const auto& reflectedOwner = Reflection::GetType<OwnerType>();
			const Guid propertyStreamMessageGuid = reflectedOwner.GetGuid();

			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier(propertyStreamMessageGuid);
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				if (const Optional<PerPeerPropagatedPropertyData*> pPropagatedPropertyData = m_perClientPropagatedPropertyData[clientIdentifier])
				{
					pPropagatedPropertyData->FlushProperties(messageTypeIdentifier);
				}
				return true;
			}
			return false;
		}

		//! Called to force ignoring of the update frequency for a frame
		//! Useful for immediate actions such as jumps etc
		template<auto FirstMember, auto... Members>
		bool FlushPropertiesToAllClients()
		{
			using OwnerType = typename TypeTraits::MemberOwnerType<decltype(FirstMember)>;
			const auto& reflectedOwner = Reflection::GetType<OwnerType>();
			const Guid propertyStreamMessageGuid = reflectedOwner.GetGuid();

			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier(propertyStreamMessageGuid);
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				for (const ClientIdentifier clientIdentifier : m_clientIdentifiers.GetValidElementView(m_clientIdentifiers.GetView()))
				{
					if (const Optional<PerPeerPropagatedPropertyData*> pPropagatedPropertyData = m_perClientPropagatedPropertyData[clientIdentifier])
					{
						pPropagatedPropertyData->FlushProperties(messageTypeIdentifier);
					}
				}
				return true;
			}
			return false;
		}

		[[nodiscard]] bool HasPendingPropagatedPropertiesToSend() const
		{
			for (const ClientIdentifier clientIdentifier : m_clientIdentifiers.GetValidElementView(m_clientIdentifiers.GetView()))
			{
				if (const Optional<PerPeerPropagatedPropertyData*> pPropagatedPropertyData = m_perClientPropagatedPropertyData[clientIdentifier])
				{
					if (pPropagatedPropertyData->HasPendingDataToSend())
					{
						return true;
					}
				}
			}
			return false;
		}
		[[nodiscard]] bool HasPendingPropagatedPropertiesToSend(const ClientIdentifier clientIdentifier) const
		{
			if (const Optional<PerPeerPropagatedPropertyData*> pPropagatedPropertyData = m_perClientPropagatedPropertyData[clientIdentifier])
			{
				if (pPropagatedPropertyData->HasPendingDataToSend())
				{
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] Time::Timestamp
		ConvertClientTimestampToLocal(const ClientIdentifier clientIdentifier, const Time::Timestamp clientTimestamp) const
		{
			if (const Optional<PerPeerPropagatedPropertyData*> pPropagatedPropertyData = m_perClientPropagatedPropertyData[clientIdentifier])
			{
				return Time::Timestamp::FromNanoseconds(
					static_cast<uint64>(static_cast<int64>(clientTimestamp.GetNanoseconds()) + m_clientTimeOffsetNanoseconds[clientIdentifier])
				);
			}
			return {};
		}
		[[nodiscard]] Time::Timestamp
		ConvertLocalTimestampToClient(const ClientIdentifier clientIdentifier, const Time::Timestamp localTimestamp) const
		{
			if (const Optional<PerPeerPropagatedPropertyData*> pPropagatedPropertyData = m_perClientPropagatedPropertyData[clientIdentifier])
			{
				return Time::Timestamp::FromNanoseconds(
					static_cast<uint64>(static_cast<int64>(localTimestamp.GetNanoseconds()) - m_clientTimeOffsetNanoseconds[clientIdentifier])
				);
			}
			return {};
		}

		[[nodiscard]] Time::Timestamp GetClientRoundTripTime(const ClientIdentifier clientIdentifier) const;

		Event<void(void*, ClientIdentifier, RemoteClient, BoundObjectIdentifier), 24> OnClientConnected;
		Event<void(void*, ClientIdentifier), 24> OnClientDisconnected;

		void RegisterLocalClient(const ClientIdentifier clientIdentifier)
		{
			Assert(!m_localClientIdentifiers.Contains(clientIdentifier.GetFirstValidIndex()));
			m_localClientIdentifiers.EmplaceBack(clientIdentifier.GetFirstValidIndex());
		}
		void DeregisterLocalClient(const ClientIdentifier clientIdentifier)
		{
			Assert(m_localClientIdentifiers.Contains(clientIdentifier.GetFirstValidIndex()));
			m_localClientIdentifiers.RemoveFirstOccurrence(clientIdentifier.GetFirstValidIndex());
		}
		[[nodiscard]] bool IsLocalClient(const ClientIdentifier clientIdentifier) const
		{
			return m_localClientIdentifiers.Contains(clientIdentifier.GetFirstValidIndex());
		}
	protected:
		friend struct Reflection::ReflectedType<LocalHost>;

		virtual Result OnExecute(Threading::JobRunnerThread&) override final;

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override final
		{
			return "Networked Host Tick";
		}
#endif

		virtual void OnPeerConnected(RemotePeer peer) override final;
		virtual void OnPeerDisconnected(const RemotePeer peer) override final;

		[[nodiscard]] virtual bool CanHandleBoundObjectMessage(
			const BoundObjectIdentifier boundObjectIdentifier, const RemotePeer remotePeer, const EnumFlags<MessageTypeFlags> messageTypeFlags
		) const override final;
	private:
		void EnableUpdate();
		void DisableUpdate();

		void OnReceivedPropertyStream(
			const MessageTypeIdentifier messageTypeIdentifier, const RemoteClient remoteHost, const Channel, ConstBitView& messageView
		);
		void OnReceivedConfirmPropertyReceipt(const RemotePeer remotePeer, const Channel, ConstBitView& messageView);
		void OnReceivedForwardRequestToOtherClients(const RemotePeer remotePeer, const Channel, ConstBitView& messageView);
		void OnReceivedForwardRequestToAllRemotes(const RemotePeer remotePeer, const Channel, ConstBitView& messageView);
		void OnReceivedTimeSyncRequest(const RemotePeer remotePeer, const Channel, ConstBitView& messageView);
	protected:
		TSaltedIdentifierStorage<ClientIdentifier> m_clientIdentifiers;
		TIdentifierArray<RemoteClient, ClientIdentifier> m_remoteClients;
		InlineVector<ClientIdentifier::IndexType, 1> m_localClientIdentifiers;

		TSaltedIdentifierStorage<BoundObjectIdentifier> m_boundObjectIdentifiers;
		BoundObjectIdentifier m_boundObjectIdentifier;
		TIdentifierArray<Guid, BoundObjectIdentifier> m_boundObjectIdentifierGuids;
		TIdentifierArray<ClientIdentifier::IndexType, BoundObjectIdentifier> m_boundObjectsAuthorityClients{Memory::Zeroed};

		TSaltedIdentifierStorage<MessageTypeIdentifier> m_messageTypeIdentifiers;

		IdentifierMask<MessageTypeIdentifier> m_functionMessageTypes;
		IdentifierMask<MessageTypeIdentifier> m_propertyStreamMessageTypes;

		TIdentifierArray<UniquePtr<PerPeerPropagatedPropertyData>, ClientIdentifier> m_perClientPropagatedPropertyData;
		// Offset from the local host's timestamps to those of clients
		TIdentifierArray<int64, ClientIdentifier> m_clientTimeOffsetNanoseconds;
		;
	};
}
