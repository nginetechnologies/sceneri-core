#pragma once

#include <NetworkingCore/LocalPeer.h>
#include <NetworkingCore/Host/RemoteHost.h>
#include <Common/Network/Port.h>

#include <Common/IO/ForwardDeclarations/ZeroTerminatedURIView.h>
#include <Common/Function/Event.h>
#include <Common/Function/ThreadSafeEvent.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/Time/Timestamp.h>

namespace ngine::Network
{
	struct Address;
	struct LocalHost;

	struct LocalClient final : public LocalPeer
	{
		LocalClient();
		LocalClient(const LocalClient& other) = delete;
		LocalClient(LocalClient&&) = delete;
		LocalClient& operator=(const LocalClient&) = delete;
		LocalClient& operator=(LocalClient&&) = delete;
		~LocalClient();

		bool Start(
			const uint8 maximumChannelCount = 2,
			const uint32 maximumOutgoingConnectionCount = 1,
			const uint32 incomingBandwidth = 0,
			const uint32 outgoingBandwidth = 0
		);
		void Stop();

		RemoteHost Connect(const Address address, const uint32 maximumChannelCount = 2, const uint32 connectionUserData = 0);
		void Disconnect();
		void ForceDisconnect();

		[[nodiscard]] Address GetLocalHostAddress() const;
		[[nodiscard]] Address GetPublicHostAddress() const;

		template<typename... MessageTypes>
		void SendMessageTo(
			RemotePeer peer, const MessageTypeIdentifier messageTypeIdentifier, const Channel channel, const MessageTypes&... messages
		) = delete;
		void SendMessageTo(
			RemotePeer peer,
			const MessageTypeIdentifier messageTypeIdentifier,
			const Channel channel,
			const EncodedMessageBuffer&& encodedMessageBuffer
		) = delete;

		bool
		SendMessageToHost(const MessageTypeIdentifier messageTypeIdentifier, const Channel channel, EncodedMessageBuffer&& encodedMessageBuffer)
		{
			const bool canSend =
				m_sendableMessageTypeFlags.AreAllSet(m_messageTypes[messageTypeIdentifier].m_flags & MessageTypeFlags::ClientToHost);
			if (LIKELY(canSend))
			{
				return LocalPeer::SendMessageTo(m_remoteHost, messageTypeIdentifier, channel, Forward<EncodedMessageBuffer>(encodedMessageBuffer));
			}
			return false;
		}
		template<typename... MessageTypes>
		bool SendMessageToHost(const MessageTypeIdentifier messageTypeIdentifier, const Channel channel, const MessageTypes&... messages)
		{
			return SendMessageToHost(
				messageTypeIdentifier,
				channel,
				EncodeMessageBuffer(messageTypeIdentifier, AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...), messages...)
			);
		}
		template<typename... MessageTypes>
		bool SendMessageToHost(
			const MessageTypeIdentifier messageTypeIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		)
		{
			const bool canPassAuthorityCheck =
				HasAuthorityOfBoundObject(boundObjectIdentifier) ||
				m_messageTypes[messageTypeIdentifier].m_flags.IsSet(MessageTypeFlags::AllowClientToHostWithoutAuthority);
			if (LIKELY(canPassAuthorityCheck))
			{
				return SendMessageToHost(
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
		template<auto Function, typename... MessageTypes>
		bool SendMessageToHost(const BoundObjectIdentifier boundObjectIdentifier, const Channel channel, const MessageTypes&... messages)
		{
			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				const bool canPassAuthorityCheck =
					HasAuthorityOfBoundObject(boundObjectIdentifier) ||
					m_messageTypes[messageTypeIdentifier].m_flags.IsSet(MessageTypeFlags::AllowClientToHostWithoutAuthority);
				if (LIKELY(canPassAuthorityCheck))
				{
					return SendMessageToHost(
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
			}
			return false;
		}

		template<typename... MessageTypes>
		[[nodiscard]] EncodedMessageBuffer EncodeForwardedMessageBuffer(
			const MessageTypeIdentifier forwardMessageTypeIdentifier,
			const MessageTypeIdentifier messageTypeIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const EnumFlags<MessageTypeFlags> messageTypeFlags,
			const MessageTypes&... messages
		)
		{
			if (m_sendableMessageTypeFlags.AreAllSet(m_messageTypes[messageTypeIdentifier].m_flags & messageTypeFlags))
			{
				const bool canPassAuthorityCheck = HasAuthorityOfBoundObject(boundObjectIdentifier);
				if (LIKELY(canPassAuthorityCheck))
				{
					const uint32 messageRequiredSizeInBits = GetMessageTotalCompressedSizeInBits<MessageTypes...>(messageTypeIdentifier, messages...);
					const uint32 forwardMessageRequiredSizeInBits = GetMessageFixedCompressedSizeInBits(forwardMessageTypeIdentifier);

					MessageBuffer messageBuffer = AcquireMessageBuffer(messageRequiredSizeInBits + forwardMessageRequiredSizeInBits);
					BitView targetView = messageBuffer.GetView();
					const bool wasMainMessageCompressed = CompressMessage(forwardMessageTypeIdentifier, targetView);
					Assert(wasMainMessageCompressed);

					const bool wasForwardedMessageCompressed =
						CompressMessages(messageTypeIdentifier, boundObjectIdentifier, targetView, messages...);
					Assert(wasForwardedMessageCompressed);
					if (LIKELY(wasMainMessageCompressed && wasForwardedMessageCompressed))
					{
						return EncodedMessageBuffer{Move(messageBuffer), targetView};
					}
				}
			}

			return {};
		}

		template<typename... MessageTypes>
		bool SendMessageToRemoteClients(
			const MessageTypeIdentifier messageTypeIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		)
		{
			const bool canPassAuthorityCheck = HasAuthorityOfBoundObject(boundObjectIdentifier);
			if (LIKELY(canPassAuthorityCheck))
			{
				const MessageTypeIdentifier forwardMessageTypeIdentifier = MessageTypeIdentifier::MakeFromValidIndex(
					(MessageTypeIdentifier::IndexType)DefaultMessageType::RequestForwardMessageToOtherClients
				);
				EncodedMessageBuffer encodedMessageBuffer = EncodeForwardedMessageBuffer<MessageTypes...>(
					forwardMessageTypeIdentifier,
					messageTypeIdentifier,
					boundObjectIdentifier,
					MessageTypeFlags::ClientToClient,
					messages...
				);
				if (LIKELY(encodedMessageBuffer.IsValid()))
				{
					return m_remoteHost.SendMessageTo(Move(encodedMessageBuffer), channel);
				}
			}
			return false;
		}
		template<auto Function, typename... MessageTypes>
		bool
		SendMessageToRemoteClients(const BoundObjectIdentifier boundObjectIdentifier, const Channel channel, const MessageTypes&... messages)
		{
			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				return SendMessageToRemoteClients<MessageTypes...>(messageTypeIdentifier, boundObjectIdentifier, channel, messages...);
			}
			return false;
		}

		template<typename... MessageTypes>
		bool SendMessageToAllRemotes(
			const MessageTypeIdentifier messageTypeIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		)
		{
			const bool canPassAuthorityCheck = HasAuthorityOfBoundObject(boundObjectIdentifier);
			if (LIKELY(canPassAuthorityCheck))
			{
				const MessageTypeIdentifier forwardMessageTypeIdentifier = MessageTypeIdentifier::MakeFromValidIndex(
					(MessageTypeIdentifier::IndexType)DefaultMessageType::RequestForwardMessageToAllRemotes
				);
				EncodedMessageBuffer encodedMessageBuffer = EncodeForwardedMessageBuffer<MessageTypes...>(
					forwardMessageTypeIdentifier,
					messageTypeIdentifier,
					boundObjectIdentifier,
					MessageTypeFlags::ClientToClient | MessageTypeFlags::ClientToHost,
					messages...
				);
				if (LIKELY(encodedMessageBuffer.IsValid()))
				{
					return m_remoteHost.SendMessageTo(Move(encodedMessageBuffer), channel);
				}
			}
			return false;
		}
		template<auto Function, typename... MessageTypes>
		bool SendMessageToAllRemotes(const BoundObjectIdentifier boundObjectIdentifier, const Channel channel, const MessageTypes&... messages)
		{
			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier<Function>();
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				return SendMessageToAllRemotes<MessageTypes...>(messageTypeIdentifier, boundObjectIdentifier, channel, messages...);
			}
			return false;
		}

		template<auto FirstMember, auto... Members>
		bool InvalidateProperties(const BoundObjectIdentifier boundObjectIdentifier)
		{
			using OwnerType = typename TypeTraits::MemberOwnerType<decltype(FirstMember)>;
			const auto& reflectedOwner = Reflection::GetType<OwnerType>();
			const Guid propertyStreamMessageGuid = reflectedOwner.GetGuid();

			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier(propertyStreamMessageGuid);
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				const bool canSend =
					m_sendableMessageTypeFlags.AreAllSet(m_messageTypes[messageTypeIdentifier].m_flags & MessageTypeFlags::ClientToHost);
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
							m_toHostPropagatedPropertyData.Invalidate(messageTypeIdentifier, boundObjectIdentifier, invalidatedPropertyMask);
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
		bool FlushProperties()
		{
			using OwnerType = typename TypeTraits::MemberOwnerType<decltype(FirstMember)>;
			const auto& reflectedOwner = Reflection::GetType<OwnerType>();
			const Guid propertyStreamMessageGuid = reflectedOwner.GetGuid();

			const MessageTypeIdentifier messageTypeIdentifier = FindMessageIdentifier(propertyStreamMessageGuid);
			Assert(messageTypeIdentifier.IsValid());
			if (LIKELY(messageTypeIdentifier.IsValid()))
			{
				m_toHostPropagatedPropertyData.FlushProperties(messageTypeIdentifier);
				return true;
			}
			return false;
		}

		[[nodiscard]] bool HasPendingPropagatedPropertiesToSend() const
		{
			return m_toHostPropagatedPropertyData.HasPendingDataToSend();
		}

		using BindObjectCallback = Function<void(BoundObjectIdentifier), 24>;
		void BindObject(const Guid persistentObjectGuid, const AnyView object, BindObjectCallback&& callback);
		void BindObject(const Guid persistentObjectGuid, const BoundObjectIdentifier boundObjectIdentifier, const AnyView object);
		void BindObject(const BoundObjectIdentifier boundObjectIdentifier, const AnyView object);
		void UnbindObject(const BoundObjectIdentifier boundObjectIdentifier)
		{
			m_boundObjects[boundObjectIdentifier] = {};
		}
		template<typename Type>
		[[nodiscard]] Optional<Type*> GetBoundObject(const BoundObjectIdentifier boundObjectIdentifier) const
		{
			return const_cast<LocalClient&>(*this).m_boundObjects[boundObjectIdentifier].Get<Type>();
		}

		[[nodiscard]] ClientIdentifier GetIdentifier() const
		{
			return m_clientIdentifier;
		}

		[[nodiscard]] bool IsConnecting() const
		{
			return m_flags.IsSet(Flags::Connecting);
		}
		[[nodiscard]] bool IsConnected() const
		{
			return m_clientIdentifier.IsValid();
		}
		[[nodiscard]] bool IsConnectingOrConnected() const
		{
			return m_flags.IsSet(Flags::Connecting) || m_clientIdentifier.IsValid();
		}
		[[nodiscard]] bool IsDisconnecting() const
		{
			return m_flags.IsSet(Flags::Disconnecting);
		}
		[[nodiscard]] bool IsDisconnected() const;

		Event<void(void*, Network::RemoteHost, const BoundObjectIdentifier boundObjectIdentifier), 24> OnConnected;
		Event<void(void*), 24> OnDisconnected;
		Event<void(void*, MessageTypeIdentifier), 24> OnMessageTypeRegistered;

		enum class Flags : uint8
		{
			Connecting = 1 << 0,
			Disconnecting = 1 << 1
		};

		[[nodiscard]] Time::Timestamp ConvertHostTimestampToLocal(const Time::Timestamp hostTimestamp) const
		{
			return Time::Timestamp::FromNanoseconds(
				static_cast<uint64>(static_cast<int64>(hostTimestamp.GetNanoseconds()) + m_hostTimeOffsetNanoseconds)
			);
		}
		[[nodiscard]] Time::Timestamp ConvertLocalTimestampToHost(const Time::Timestamp localTimestamp) const
		{
			return Time::Timestamp::FromNanoseconds(
				static_cast<uint64>(static_cast<int64>(localTimestamp.GetNanoseconds()) - m_hostTimeOffsetNanoseconds)
			);
		}

		[[nodiscard]] Time::Timestamp GetRoundTripTime() const;
	protected:
		friend struct Reflection::ReflectedType<LocalClient>;
		friend LocalHost;

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override final
		{
			return "Networked Local Client Tick";
		}
#endif

		virtual Result OnExecute(Threading::JobRunnerThread&) override final;

		virtual void OnPeerConnected(const RemotePeer) override final
		{
			// Note: this is called when we first successfuly made a connection to the remote server.
			// OnConnectedToRemoteServer below is the real event we use, which is when we've received a unique client identifier that can be used
			// to identify the client.
		}
		virtual void OnPeerDisconnected(const RemotePeer) override final;

		[[nodiscard]] virtual bool CanHandleBoundObjectMessage(
			const BoundObjectIdentifier boundObjectIdentifier, const RemotePeer remotePeer, const EnumFlags<MessageTypeFlags> messageTypeFlags
		) const override final;

		void OnConnectedToRemoteServer(
			const ClientIdentifier clientIdentifier, const Network::RemoteHost remoteHost, const BoundObjectIdentifier boundObjectIdentifier
		);
		void OnDisconnectedInternal();

		void OnReceivedConnectedMessage(const RemoteHost remoteHost, const Channel, ConstBitView& messageView);
		void OnReceivedNewMessageType(const RemoteHost remoteHost, const Channel, ConstBitView& messageView);
		void OnReceivedNewPropertyStreamMessageType(const RemoteHost remoteHost, const Channel, ConstBitView& messageView);
		void OnReceivedBatchMessageType(const RemoteHost remoteHost, const Channel, ConstBitView& messageView);
		void OnReceivedNewBoundObject(const RemoteHost remoteHost, const Channel, ConstBitView& messageView);
		void OnReceivedPropertyStream(
			const MessageTypeIdentifier messageTypeIdentifier, const RemoteHost remoteHost, const Channel, ConstBitView& messageView
		);
		void OnReceivedConfirmPropertyReceipt(const RemotePeer remotePeer, const Channel, ConstBitView& messageView);
		void OnBoundObjectAuthorityGiven(const RemotePeer remotePeer, const Channel, ConstBitView& messageView);
		void OnBoundObjectAuthorityRevoked(const RemotePeer remotePeer, const Channel, ConstBitView& messageView);
		void OnReceiveForwardedMessage(const RemotePeer remotePeer, const Channel, ConstBitView& messageView);
		void OnReceivedTimeSyncResponseMessage(const RemoteHost remoteHost, const Channel, ConstBitView& messageView);
	private:
		void RegisterDefaultMessages();

		void EnableUpdate();
		void DisableUpdate();
	protected:
		AtomicEnumFlags<Flags> m_flags;

		struct BoundObjectInfo
		{
			BoundObjectIdentifier boundObjectIdentifier;
			AnyView m_object;
			BindObjectCallback m_callback;
		};

		ClientIdentifier m_clientIdentifier;
		Threading::SharedMutex m_boundObjectLookupMapMutex;
		UnorderedMap<Guid, BoundObjectInfo, Guid::Hash> m_boundObjectLookupMap;
		Network::RemoteHost m_remoteHost;

		// Offset from the local client's timestamps to those of the host
		int64 m_hostTimeOffsetNanoseconds;

		PerPeerPropagatedPropertyData m_toHostPropagatedPropertyData;
	};

	ENUM_FLAG_OPERATORS(LocalClient::Flags);

	inline bool LocalClient::IsDisconnected() const
	{
		return m_flags.AreNoneSet(Flags::Connecting | Flags::Disconnecting) && m_clientIdentifier.IsInvalid();
	}
}
