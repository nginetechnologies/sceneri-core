#pragma once

#include <NetworkingCore/LocalPeerView.h>
#include <NetworkingCore/RemotePeer.h>
#include <NetworkingCore/Message/DefaultMessageType.h>
#include <NetworkingCore/Message/MessageTypeIdentifier.h>
#include <NetworkingCore/Message/MessageTypeFlags.h>
#include <NetworkingCore/Message/EncodedMessageBuffer.h>
#include <NetworkingCore/Components/BoundObjectIdentifier.h>

#include <Engine/Event/Identifier.h>

#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/TimerHandle.h>
#include <Common/Threading/Mutexes/Mutex.h>
#include <Common/Threading/Mutexes/SharedMutex.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/AtomicEnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Reflection/Type.h>
#include <Common/Reflection/TypeDefinition.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/CircularBuffer.h>
#include <Common/Memory/UniqueRef.h>
#include <Common/Memory/IsAligned.h>
#include <Common/Memory/AtomicBitset.h>
#include <Common/Compression/Compress.h>
#include <Common/Compression/Decompress.h>
#include <Common/Memory/Containers/BitView.h>
#include <Common/Memory/AnyView.h>
#include <Common/Math/Range.h>
#include <Common/Math/Wrap.h>
#include <Common/Storage/Compression/Identifier.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Time/Duration.h>
#include <Common/Platform/Pure.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/DynamicFunction.h>

struct _ENetHost;
typedef _ENetHost ENetHost;
struct _ENetEvent;
typedef _ENetEvent ENetEvent;

namespace ngine::Reflection
{
	struct TypeInterface;
	struct FunctionInfo;
	struct FunctionData;
}

namespace ngine::Events
{
	struct Manager;
}

namespace ngine::Entity
{
	struct SceneRegistry;
}

namespace ngine::Network
{
	using RemoteFunction = Scripting::VM::DynamicFunction;

	//! Represents a local host or local client
	//! Must be registered for updates to function
	struct LocalPeer : public LocalPeerView, public Threading::Job
	{
		LocalPeer()
			: Job(Threading::JobPriority::RealtimeNetworking)
		{
		}
		LocalPeer(LocalPeer&& other)
			: LocalPeerView(static_cast<LocalPeerView&&>(other))
			, Job(Threading::JobPriority::RealtimeNetworking)
		{
		}
		LocalPeer(const LocalPeer& other)
			: LocalPeerView(static_cast<const LocalPeerView&>(other))
			, Job(Threading::JobPriority::RealtimeNetworking)
		{
		}
		~LocalPeer();

		enum class UpdateMode : uint8
		{
			Disabled,
			//! Network updates are done completely asynchronously on a fixed update rate
			Asynchronous,
			//! Network updates are 1:1 tied to engine ticks
			EngineTick
		};
		inline static constexpr UpdateMode DefaultUpdateMode{UpdateMode::EngineTick};

		virtual void OnAwaitExternalFinish(Threading::JobRunnerThread& thread) override final;

		bool SendMessageTo(
			RemotePeer peer, const MessageTypeIdentifier messageTypeIdentifier, const Channel channel, EncodedMessageBuffer&& encodedMessageBuffer
		)
		{
			const bool canSend =
				m_sendableMessageTypeFlags.AreAllSet(m_messageTypes[messageTypeIdentifier].m_flags & MessageTypeFlags::ValidationMask);
			if (LIKELY(canSend && encodedMessageBuffer.IsValid()))
			{
				return peer.SendMessageTo(Forward<EncodedMessageBuffer>(encodedMessageBuffer), channel);
			}
			return false;
		}
		template<typename... MessageTypes>
		bool SendMessageTo(
			RemotePeer peer, const MessageTypeIdentifier messageTypeIdentifier, const Channel channel, const MessageTypes&... messages
		)
		{
			return SendMessageTo(
				peer,
				messageTypeIdentifier,
				channel,
				EncodeMessageBuffer(messageTypeIdentifier, AcquireMessageBuffer<MessageTypes...>(messageTypeIdentifier, messages...), messages...)
			);
		}
		template<typename... MessageTypes>
		bool SendMessageTo(
			RemotePeer peer,
			const MessageTypeIdentifier messageTypeIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			const Channel channel,
			const MessageTypes&... messages
		)
		{
			return SendMessageTo(
				peer,
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

		void FlushPendingMessages();

		template<typename MessageType>
		[[nodiscard]] static MessageType DecompressMessageBuffer(ConstBitView& messageView)
		{
			MessageType message;
			[[maybe_unused]] const bool wasDecompressed = DecompressMessageBuffer<MessageType>(message, messageView);
			Assert(wasDecompressed);
			return Move(message);
		}

		[[nodiscard]] bool HasMessage(const Guid functionGuid) const
		{
			return m_messageTypeLookupMap.Contains(functionGuid);
		}
		[[nodiscard]] MessageTypeIdentifier FindMessageIdentifier(const Guid functionGuid) const
		{
			auto it = m_messageTypeLookupMap.Find(functionGuid);
			if (it != m_messageTypeLookupMap.end())
			{
				return it->second;
			}
			return {};
		}
		template<auto Function>
		[[nodiscard]] PURE_STATICS MessageTypeIdentifier FindMessageIdentifier() const
		{
			return FindMessageIdentifier(Reflection::GetFunctionGuid<Function>());
		}

		[[nodiscard]] bool HasAuthorityOfBoundObject(const BoundObjectIdentifier boundObjectIdentifier) const
		{
			return m_boundObjectAuthorityMask.IsSet(boundObjectIdentifier);
		}

		void SetEntitySceneRegistry(const Optional<Entity::SceneRegistry*> pSceneRegistry)
		{
			m_pEntitySceneRegistry = pSceneRegistry;
		}

		//! Index of a property unique to a specific type
		inline static constexpr uint8 MaximumPropertyCount = 16u;
		using PropertyIdentifier = TIdentifier<uint32, Memory::GetBitWidth(MaximumPropertyCount), MaximumPropertyCount>;
		using PropertyIndex = PropertyIdentifier::IndexType;
		using PropertyMask = Bitset<PropertyIdentifier::MaximumCount>;

		struct SendWindow
		{
			inline static constexpr SequenceNumber MaximumSequenceNumber = Math::NumericLimits<SequenceNumber>::Max;
			inline static constexpr SequenceNumber WindowSize = 4096;

			SendWindow() = default;
			SendWindow(const SequenceNumber lastAcknowledgedSequenceNumber, const SequenceNumber lastSentSequenceNumber)
				: m_lastAcknowledgedSequenceNumber(lastAcknowledgedSequenceNumber)
				, m_lastSentSequenceNumber(lastSentSequenceNumber)
			{
			}

			[[nodiscard]] Optional<SequenceNumber> GetNewSequenceNumber()
			{
				const Math::Range<uint32> usableRange = GetUsableRange();
				if (usableRange.GetSize() > 0)
				{
					return (SequenceNumber)Math::Wrap(usableRange.GetMinimum(), 0u, (uint32)MaximumSequenceNumber);
				}
				else
				{
					return Invalid;
				}
			}
			[[nodiscard]] Optional<SequenceNumber> GetFirstSentSequenceNumber()
			{
				const SequenceNumber firstSentSequence = (SequenceNumber
				)Math::Wrap(m_lastAcknowledgedSequenceNumber + 1u, 0u, (uint32)MaximumSequenceNumber);
				return Optional<SequenceNumber>{firstSentSequence, m_lastSentSequenceNumber != m_lastAcknowledgedSequenceNumber};
			}
			[[nodiscard]] Optional<SequenceNumber> GetLastSentSequenceNumber()
			{
				return Optional<SequenceNumber>{m_lastSentSequenceNumber, m_lastSentSequenceNumber != m_lastAcknowledgedSequenceNumber};
			}

			[[nodiscard]] Optional<SequenceNumber> GetLastAcknowledgedSequenceNumber()
			{
				return Optional<SequenceNumber>{m_lastAcknowledgedSequenceNumber, m_lastSentSequenceNumber != m_lastAcknowledgedSequenceNumber};
			}

			[[nodiscard]] SequenceNumber GetSentCount() const
			{
				return (SequenceNumber)GetSentRange().GetSize();
			}
			[[nodiscard]] SequenceNumber GetSendableCount() const
			{
				return (SequenceNumber)GetUsableRange().GetSize();
			}

			void OnSequenceSent(const SequenceNumber sequenceNumber)
			{
				Assert(sequenceNumber == *GetNewSequenceNumber());
				Assert(sequenceNumber != m_lastSentSequenceNumber);
				m_lastSentSequenceNumber = sequenceNumber;
				m_sequenceSendTimestamps[sequenceNumber] = Time::Timestamp::GetCurrent();
			}

			enum class AcknowledgmentResult
			{
				Rejected,
				Accepted,
				AcceptedLastSentSequence
			};

			//! Called when a sequence's confirmation receipt arrives
			//! Returns whether or not the acknowledged sequence was accepted
			[[nodiscard]] AcknowledgmentResult OnSequenceAcknowledged(const SequenceNumber sequenceNumber)
			{
				const Math::Range<uint32> sentRange = GetSentRange();

				if (sentRange.Contains(sequenceNumber))
				{
					m_lastAcknowledgedSequenceNumber = sequenceNumber;
					if (sequenceNumber == m_lastSentSequenceNumber)
					{
						return AcknowledgmentResult::AcceptedLastSentSequence;
					}
					else
					{
						m_lastRoundTripTime = (Time::Timestamp::GetCurrent() - m_sequenceSendTimestamps[sequenceNumber]).GetDuration();
						return AcknowledgmentResult::Accepted;
					}
				}
				else
				{
					const uint32 overflowEnd = Math::Wrap(sentRange.GetEnd(), 0u, (uint32)MaximumSequenceNumber);
					if (overflowEnd < sentRange.GetMinimum())
					{
						const Math::Range<uint32> overflowSentRange = Math::Range<uint32>::Make(0, overflowEnd);
						if (overflowSentRange.Contains(sequenceNumber))
						{
							m_lastAcknowledgedSequenceNumber = sequenceNumber;
							if (sequenceNumber == m_lastSentSequenceNumber)
							{
								return AcknowledgmentResult::AcceptedLastSentSequence;
							}
							else
							{
								m_lastRoundTripTime = (Time::Timestamp::GetCurrent() - m_sequenceSendTimestamps[sequenceNumber]).GetDuration();
								return AcknowledgmentResult::Accepted;
							}
						}
					}
				}

				return AcknowledgmentResult::Rejected;
			}

			[[nodiscard]] Time::Durationd GetLastRoundTripTime() const
			{
				return m_lastRoundTripTime;
			}
		protected:
			[[nodiscard]] Math::Range<uint32> GetSendWindow() const
			{
				const uint32 firstSendableSequence = Math::Wrap(m_lastAcknowledgedSequenceNumber + 1u, 0u, (uint32)MaximumSequenceNumber);
				return Math::Range<uint32>::Make(firstSendableSequence, WindowSize);
			}
			[[nodiscard]] Math::Range<uint32> GetSentRange() const
			{
				const uint32 firstSentSequence = Math::Wrap(m_lastAcknowledgedSequenceNumber + 1u, 0u, (uint32)MaximumSequenceNumber);
				if (m_lastSentSequenceNumber != m_lastAcknowledgedSequenceNumber)
				{
					if (firstSentSequence <= m_lastSentSequenceNumber)
					{
						const uint32 sentCount = m_lastSentSequenceNumber - firstSentSequence + 1;
						const Math::Range<uint32> sentRange = Math::Range<uint32>::Make(firstSentSequence, sentCount);
						Assert(GetSendWindow().Contains(sentRange));
						return sentRange;
					}
					else
					{
						const uint32 sentCount = MaximumSequenceNumber - firstSentSequence + m_lastSentSequenceNumber + 2;
						const Math::Range<uint32> sentRange = Math::Range<uint32>::Make(firstSentSequence, sentCount);
						Assert(GetSendWindow().Contains(sentRange));
						return sentRange;
					}
				}
				else
				{
					return Math::Range<uint32>::Make(firstSentSequence, 0);
				}
			}
			[[nodiscard]] Math::Range<uint32> GetUsableRange() const
			{
				const Math::Range<uint32> sendWindow = GetSendWindow();
				const Math::Range<uint32> sentRange = GetSentRange();

				const uint32 firstSendableSequence = sentRange.GetEnd();
				const Math::Range<uint32> sendableRange = Math::Range<uint32>::Make(firstSendableSequence, WindowSize);

				return sendableRange.Mask(sendWindow);
			}
		protected:
			SequenceNumber m_lastAcknowledgedSequenceNumber{MaximumSequenceNumber};
			SequenceNumber m_lastSentSequenceNumber{MaximumSequenceNumber};
			Array<Time::Timestamp, MaximumSequenceNumber> m_sequenceSendTimestamps;
			Time::Durationd m_lastRoundTripTime{0_seconds};
		};

		// TODO: ReceiveWindow

		struct PerPeerPropagatedPropertyData
		{
			enum class Flags : uint8
			{
				HasPendingDataToSend = 1 << 0
			};

			[[nodiscard]] bool HasPendingDataToSend() const
			{
				return m_flags.IsSet(Flags::HasPendingDataToSend);
			}

			void ProcessPendingData(
				LocalPeer& localPeer, const EnumFlags<Reflection::PropertyFlags> requiredFlags, RemotePeer remotePeer, const Channel channel
			);
			void ProcessConfirmationReceipt(const ConfirmPropagatedPropertyReceiptMessage message);

			void Invalidate(
				const MessageTypeIdentifier messageTypeIdentifier,
				const BoundObjectIdentifier boundObjectIdentifier,
				const PropertyMask invalidatedPropertyMask
			);

			//! Called to force ignoring of the update frequency for a frame
			//! Useful for immediate actions such as jumps etc
			void FlushProperties(const MessageTypeIdentifier messageTypeIdentifier);

			void Reset();

			using ObjectPropertyMaskMap = UnorderedMap<BoundObjectIdentifier::IndexType, PropertyMask>;
			struct TypeInfo
			{
				Threading::Mutex m_mutex;
				ObjectPropertyMaskMap m_objectPropertyMaskMap;
				Time::Timestamp m_lastSendTime;

				SendWindow m_sendWindow;
				bool m_changed{false};
			};

			[[nodiscard]] TypeInfo& GetOrEmplaceTypeInfo(const MessageTypeIdentifier messageTypeIdentifier);

			AtomicEnumFlags<Flags> m_flags;

			using TypeMap = UnorderedMap<MessageTypeIdentifier::IndexType, UniqueRef<TypeInfo>>;
			Threading::SharedMutex m_typeLookupMapMutex;
			TypeMap m_typeLookupMap;
		};

		template<auto Member, auto... Members>
		[[nodiscard]] bool GetPropertyMask(PropertyMask& maskOut)
		{
			const auto& reflectedProperty = Reflection::GetProperty<Member>();
			auto propagatedPropertyIt = m_propagatedPropertyLookupMap.Find(reflectedProperty.m_guid);
			if (LIKELY(propagatedPropertyIt != m_propagatedPropertyLookupMap.end()))
			{
				const PropagatedProperty& __restrict propagatedProperty = propagatedPropertyIt->second;
				maskOut.Set(propagatedProperty.localIndex);
				if constexpr (sizeof...(Members) > 0)
				{
					return GetPropertyMask<Members...>(maskOut);
				}
				else
				{
					return true;
				}
			}
			else
			{
				return false;
			}
		}

		[[nodiscard]] MessageBuffer AcquireMessageBuffer(const uint32 requiredSizeInBits);

		template<typename... MessageTypes>
		[[nodiscard]] MessageBuffer AcquireMessageBuffer(const MessageTypeIdentifier messageTypeIdentifier, const MessageTypes&... messages)
		{
			return AcquireMessageBuffer(GetMessageTotalCompressedSizeInBits<MessageTypes...>(messageTypeIdentifier, messages...));
		}

		[[nodiscard]] MessageBuffer AcquireMessageBuffer(const MessageTypeIdentifier messageTypeIdentifier)
		{
			Assert(m_messageTypes[messageTypeIdentifier].m_flags.IsNotSet(MessageTypeFlags::HasDynamicCompressedDataSize));
			return AcquireMessageBuffer(GetMessageFixedCompressedSizeInBits(messageTypeIdentifier));
		}

		template<typename... MessageTypes>
		EncodedMessageBuffer
		EncodeMessageBuffer(const MessageTypeIdentifier messageTypeIdentifier, MessageBuffer&& messageBuffer, const MessageTypes&... messages)
		{
			BitView targetView = messageBuffer.GetView();

			const bool wasCompressed = CompressMessages(messageTypeIdentifier, targetView, messages...);
			Assert(wasCompressed);
			if (LIKELY(wasCompressed))
			{
				return EncodedMessageBuffer(Forward<MessageBuffer>(messageBuffer), targetView);
			}
			else
			{
				return {};
			}
		}

		template<typename... MessageTypes>
		EncodedMessageBuffer EncodeMessageBuffer(
			const MessageTypeIdentifier messageTypeIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			MessageBuffer&& messageBuffer,
			const MessageTypes&... messages
		)
		{
			BitView targetView = messageBuffer.GetView();

			const bool wasCompressed = CompressMessages(messageTypeIdentifier, boundObjectIdentifier, targetView, messages...);
			Assert(wasCompressed);
			if (LIKELY(wasCompressed))
			{
				return EncodedMessageBuffer(Forward<MessageBuffer>(messageBuffer), targetView);
			}
			else
			{
				return {};
			}
		}

		template<typename Type>
		[[nodiscard]] static bool CompressMessage(const Type& source, BitView& target)
		{
			return Compression::Compress(source, target, Reflection::PropertyFlags::SentWithNetworkedFunctions);
		}
	protected:
		template<typename Type>
		[[nodiscard]] static constexpr uint32 CalculateFixedCompressedDataSize()
		{
			return Compression::CalculateFixedDataSize<Type>(Reflection::PropertyFlags::SentWithNetworkedFunctions);
		}
		template<typename MessageType>
		[[nodiscard]] static constexpr uint32 CalculateDynamicCompressedDataSize(const MessageType& messageType)
		{
			return Compression::CalculateDynamicDataSize<MessageType>(messageType, Reflection::PropertyFlags::SentWithNetworkedFunctions);
		}

		[[nodiscard]] uint32 GetMessageFixedCompressedSizeInBits(const MessageTypeIdentifier messageTypeIdentifier) const
		{
			return m_messageTypes[messageTypeIdentifier].m_fixedCompressedDataSizeInBits;
		}
		template<typename... MessageTypes>
		[[nodiscard]] uint32 GetMessageDynamicCompressedSizeInBits(const MessageTypes&... messages) const
		{
			return (CalculateDynamicCompressedDataSize<MessageTypes>(messages) + ... + 0);
		}
		template<typename... MessageTypes>
		[[nodiscard]] uint32
		GetMessageTotalCompressedSizeInBits(const MessageTypeIdentifier messageTypeIdentifier, const MessageTypes&... messages) const
		{
			uint32 result = GetMessageFixedCompressedSizeInBits(messageTypeIdentifier);
			if (m_messageTypes[messageTypeIdentifier].m_flags.IsSet(MessageTypeFlags::HasDynamicCompressedDataSize))
			{
				result += GetMessageDynamicCompressedSizeInBits<MessageTypes...>(messages...);
			}

			return result;
		}

		template<typename... MessageTypes>
		bool CompressMessages(const MessageTypeIdentifier messageTypeIdentifier, BitView& targetView, const MessageTypes&... messages)
		{
			return CompressMessage(messageTypeIdentifier, targetView) && (CompressMessage(messages, targetView) && ...);
		}

		template<typename... MessageTypes>
		bool CompressMessages(
			const MessageTypeIdentifier messageTypeIdentifier,
			const BoundObjectIdentifier boundObjectIdentifier,
			BitView& targetView,
			const MessageTypes&... messages
		)
		{
			return CompressMessage(messageTypeIdentifier, targetView) && CompressMessage(boundObjectIdentifier, targetView) &&
			       (CompressMessage(messages, targetView) && ...);
		}

		template<typename Type>
		[[nodiscard]] static bool DecompressMessageBuffer(Type& target, ConstBitView& source)
		{
			return Compression::Decompress(target, source, Reflection::PropertyFlags::SentWithNetworkedFunctions);
		}

		LocalPeer(ENetHost* pNetHost);

		virtual void OnPeerConnected(const RemotePeer peer) = 0;
		virtual void OnPeerDisconnected(const RemotePeer peer) = 0;

		struct MessageType
		{
			Guid m_functionGuid;
			RemoteFunction m_function;
			//! Size of the compressed data that is known per type
			//! If the message type falgs indicate that this type contains dynamic data then we will allocate more at runtime.
			uint16 m_fixedCompressedDataSizeInBits;
			uint32 m_uncompressedDataSize;
			EnumFlags<MessageTypeFlags> m_flags;
			Vector<Reflection::TypeDefinition> m_argumentTypeDefinitions;
		};
		[[nodiscard]] MessageType CreateMessageType(
			const Guid functionGuid,
			const uint16 compressedDataSizeInBits,
			const Optional<const Reflection::TypeInterface*> pOwningTypeInterface,
			const Reflection::FunctionInfo& functionInfo,
			const Reflection::FunctionData& functionData
		) const;
		void RegisterMessageType(const MessageTypeIdentifier messageTypeIdentifier, MessageType&& messageType)
		{
			m_messageTypeLookupMap.Emplace(messageType.m_functionGuid, messageTypeIdentifier);

			m_messageTypes[messageTypeIdentifier] = Forward<MessageType>(messageType);
			m_maximumMessageTypeIndex = Math::Max(m_maximumMessageTypeIndex, messageTypeIdentifier.GetIndex());
		}

		[[nodiscard]] static constexpr EnumFlags<MessageTypeFlags> GetMessageTypeFlags(const EnumFlags<Reflection::FunctionFlags> functionFlags)
		{
			return (MessageTypeFlags::HostToClient * functionFlags.IsSet(Reflection::FunctionFlags::HostToClient)) |
			       (MessageTypeFlags::HostToAllClients * functionFlags.IsSet(Reflection::FunctionFlags::HostToClient)) |
			       (MessageTypeFlags::ClientToHost * functionFlags.IsSet(Reflection::FunctionFlags::ClientToHost)) |
			       (MessageTypeFlags::ClientToClient * functionFlags.IsSet(Reflection::FunctionFlags::ClientToClient)) |
			       (MessageTypeFlags::ClientToAllClients * functionFlags.IsSet(Reflection::FunctionFlags::ClientToClient)) |
			       (MessageTypeFlags::IsObjectFunction * functionFlags.IsSet(Reflection::FunctionFlags::IsMemberFunction)) |
			       (MessageTypeFlags::AllowClientToHostWithoutAuthority *
			        functionFlags.IsSet(Reflection::FunctionFlags::AllowClientToHostWithoutAuthority));
		}

		template<typename Type>
		void RegisterUnhandledDefaultMessageType(const DefaultMessageType defaultMessageType)
		{
			constexpr uint32 fixedDataSizeInBits = CalculateFixedCompressedDataSize<MessageTypeIdentifier>() +
			                                       CalculateFixedCompressedDataSize<Type>();

			EnumFlags<MessageTypeFlags> messageTypeFlags;
			if constexpr (Compression::IsDynamicallyCompressed<Type>(Reflection::PropertyFlags::SentWithNetworkedFunctions))
			{
				messageTypeFlags |= MessageTypeFlags::HasDynamicCompressedDataSize;
			}

			const MessageTypeIdentifier messageTypeIdentifier = MessageTypeIdentifier::MakeFromValidIndex((MessageTypeIdentifier::IndexType
			)defaultMessageType);
			m_messageTypes[messageTypeIdentifier] = MessageType{{}, {}, fixedDataSizeInBits, sizeof(Type), messageTypeFlags};
			m_maximumMessageTypeIndex = Math::Max(m_maximumMessageTypeIndex, messageTypeIdentifier.GetIndex());
		}

		template<typename Type, auto Function>
		void RegisterDefaultMessageType(const DefaultMessageType defaultMessageType)
		{
			constexpr const auto& function = Reflection::GetFunction<Function>();
			constexpr uint16 dataSizeInBits = (uint16)(CalculateFixedCompressedDataSize<MessageTypeIdentifier>() +
			                                           CalculateFixedCompressedDataSize<Type>());

			EnumFlags<MessageTypeFlags> messageTypeFlags = GetMessageTypeFlags(function.GetFlags()) & ~MessageTypeFlags::IsObjectFunction;
			if constexpr (Compression::IsDynamicallyCompressed<Type>(Reflection::PropertyFlags::SentWithNetworkedFunctions))
			{
				messageTypeFlags |= MessageTypeFlags::HasDynamicCompressedDataSize;
			}

			RegisterMessageType(
				MessageTypeIdentifier::MakeFromValidIndex((MessageTypeIdentifier::IndexType)defaultMessageType),
				MessageType{function.GetGuid(), RemoteFunction::Make<function.GetFunction()>(), dataSizeInBits, sizeof(Type), messageTypeFlags}
			);
		}
	protected:
		void ProcessMessages();
		bool ProcessMessageInternal(ENetEvent& event);
		//! Parses the full message, validating that it can be handled
		//! Returns the message type identifier and populates the registers with arguments
		[[nodiscard]] Optional<MessageTypeIdentifier> PreprocessMessage(
			ConstBitView& messageView,
			const EnumFlags<MessageTypeFlags> receivableMessageTypeFlags,
			const RemotePeer remotePeer,
			const Channel channel,
			Scripting::VM::Registers& registers
		);
		bool HandleMessage(ConstBitView& messageView, const RemotePeer remotePeer, const Channel channel);
		[[nodiscard]] virtual bool CanHandleBoundObjectMessage(
			const BoundObjectIdentifier boundObjectIdentifier, const RemotePeer remotePeer, const EnumFlags<MessageTypeFlags> messageTypeFlags
		) const = 0;

		[[nodiscard]] Optional<Reflection::PropertyOwner*> GetBoundObjectPropertyOwner(
			const BoundObjectIdentifier boundObjectIdentifier, const Guid typeGuid, const EnumFlags<MessageTypeFlags> messageTypeFlags
		);

		void ChangeUpdateMode(const UpdateMode mode);
	protected:
		UpdateMode m_updateMode{UpdateMode::Disabled};
		Threading::Mutex m_updateMutex;

		TIdentifierArray<MessageType, MessageTypeIdentifier> m_messageTypes{Memory::Zeroed};
		UnorderedMap<Guid, MessageTypeIdentifier, Guid::Hash> m_messageTypeLookupMap;

		struct PropagatedProperty
		{
			PropertyIndex localIndex;
			const Reflection::PropertyInfo& propertyInfo;
			Reflection::DynamicPropertyInstance dynamicPropertyInstance;
		};
		//! Lookup map for all property indices
		UnorderedMap<Guid, PropagatedProperty, Guid::Hash> m_propagatedPropertyLookupMap;

		using TypePropertyGuids = Vector<Guid, PropertyIdentifier::IndexType>;
		TIdentifierArray<TypePropertyGuids, MessageTypeIdentifier> m_propagatedPropertyTypeGuids;

		TIdentifierArray<AnyView, BoundObjectIdentifier> m_boundObjects{Memory::Zeroed};
		//! Mask indicating whether the local client has authority over a bound object
		IdentifierMask<BoundObjectIdentifier> m_boundObjectAuthorityMask;

		MessageTypeIdentifier::IndexType m_maximumMessageTypeIndex{0};

		EnumFlags<MessageTypeFlags> m_receivableMessageTypeFlags;
		EnumFlags<MessageTypeFlags> m_sendableMessageTypeFlags;

		Threading::TimerHandle m_asyncUpdateTimerHandle;
		Time::Timestamp m_lastUpdateTime;

		Optional<Entity::SceneRegistry*> m_pEntitySceneRegistry;
	};

	ENUM_FLAG_OPERATORS(LocalPeer::PerPeerPropagatedPropertyData::Flags);
}
