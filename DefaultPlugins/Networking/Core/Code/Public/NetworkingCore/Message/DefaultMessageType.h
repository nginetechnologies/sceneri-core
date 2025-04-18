#pragma once

#include <NetworkingCore/Message/MessageTypeIdentifier.h>
#include <NetworkingCore/Client/ClientIdentifier.h>
#include <NetworkingCore/Components/BoundObjectIdentifier.h>
#include <NetworkingCore/SequenceNumber.h>

#include <Common/Storage/Compression/Identifier.h>
#include <Common/Reflection/Type.h>
#include <Common/Time/Timestamp.h>

namespace ngine::Network
{
	enum class DefaultMessageType : uint8
	{
		First,
		//! Sent from host to client when that client has just connected to the host
		LocalPeerConnected = First,
		//! Sent to register a dynamic message type that can be sent from server to client
		RegisterNewMessageType,
		//! Sent to register a dynamic stream of properties for a specific type
		RegisterPropertyStreamMessage,
		//! Used to send multiple messages inside one buffer to save bandwidth
		BatchMessages,
		//! Sent when a new object is bound to the network
		ObjectBound,
		//! Sent when a peer receives propagated properties to confirm receipt
		ConfirmPropagatedPropertyReceipt,
		//! Used to delegate authority of a bound object to a specific client
		BoundObjectAuthorityGivenToLocalClient,
		//! Used to remove authority of a bound object to a specific client
		BoundObjectAuthorityRevokedFromLocalClient,
		// Sent to the host to request forwarding a message to all other clients
		RequestForwardMessageToOtherClients,
		// Sent to the host to request forwarding a message to all other clients, and process on the remote
		RequestForwardMessageToAllRemotes,
		// Sent to a client when a message was forwarded from another client
		ReceivedForwardedMessage,
		// Sent to the host to request synchronizing timestamps
		RequestTimeSync,
		// Sent to a client when the host received a time sync request
		ReceivedTimeSyncResponse,
		Count
	};
	ENUM_FLAG_OPERATORS(DefaultMessageType);

	//! Sent from host to client when that client has just connected to the host
	struct LocalPeerConnectedMessage
	{
		LocalPeerConnectedMessage() = default;
		LocalPeerConnectedMessage(
			const ClientIdentifier clientIdentifier, const BoundObjectIdentifier clientBoundObjectIdentifier, const Time::Timestamp hostTimestamp
		)
			: m_clientIdentifier(clientIdentifier)
			, m_clientBoundObjectIdentifier(clientBoundObjectIdentifier)
			, m_hostTimestamp(hostTimestamp)
		{
		}

		ClientIdentifier m_clientIdentifier;
		BoundObjectIdentifier m_clientBoundObjectIdentifier;
		Time::Timestamp m_hostTimestamp;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::LocalPeerConnectedMessage>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::LocalPeerConnectedMessage>(
			"d41f4dd3-e350-44a5-924f-f5619cd9c608"_guid,
			MAKE_UNICODE_LITERAL("Local Peer Connected Message"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning | Reflection::TypeFlags::DisableDynamicDeserialization |
				Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Remote Client Identifier"),
					"remoteClientID",
					"{BE118146-9A07-49A7-9499-76231F3317C8}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::LocalPeerConnectedMessage::m_clientIdentifier
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Client Bound Object Identifier"),
					"clientBoundObjectIdentifier",
					"{B0C30D8D-B613-47D4-AA75-BC28AF7CDF0A}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::LocalPeerConnectedMessage::m_clientBoundObjectIdentifier
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Client Timestamp"),
					"hostTimestamp",
					"b6eeb962-f979-46e1-9870-cedcceb0065d"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::LocalPeerConnectedMessage::m_hostTimestamp
				}
			}
		);
	};
}

namespace ngine::Network
{
	struct RegisterMessageTypeMessage
	{
		RegisterMessageTypeMessage() = default;
		RegisterMessageTypeMessage(
			const MessageTypeIdentifier messageTypeIdentifier, const Guid functionGuid, const uint16 fixedCompressedDataSizeInBits
		)
			: m_messageTypeIdentifier(messageTypeIdentifier)
			, m_functionGuid(functionGuid)
			, m_fixedCompressedDataSizeInBits(fixedCompressedDataSizeInBits)
		{
		}

		MessageTypeIdentifier m_messageTypeIdentifier;
		Guid m_functionGuid;
		uint16 m_fixedCompressedDataSizeInBits;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::RegisterMessageTypeMessage>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::RegisterMessageTypeMessage>(
			"eccfc714-bfc6-49f2-96ff-ccd5fde9d97f"_guid,
			MAKE_UNICODE_LITERAL("Register Message Type Message"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning | Reflection::TypeFlags::DisableDynamicDeserialization |
				Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Message Type Identifier"),
					"messageTypeID",
					"{B0C30D8D-B613-47D4-AA75-BC28AF7CDF0A}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::RegisterMessageTypeMessage::m_messageTypeIdentifier
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Function Guid"),
					"functionGuid",
					"{D55ED263-4985-4E6B-9366-35D7B634C40C}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::RegisterMessageTypeMessage::m_functionGuid
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Fixed Compressed Data Size"),
					"size",
					"{5D318299-241D-4670-A7DD-BF9ADE2E53BC}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::RegisterMessageTypeMessage::m_fixedCompressedDataSizeInBits
				}
			},
			Reflection::Functions{}
		);
	};
}

namespace ngine::Network
{
	struct RegisterPropertyStreamMessage
	{
		RegisterPropertyStreamMessage() = default;
		RegisterPropertyStreamMessage(const MessageTypeIdentifier messageTypeIdentifier, const Guid typeGuid, const uint8 propertyCount)
			: m_messageTypeIdentifier(messageTypeIdentifier)
			, m_typeGuid(typeGuid)
			, m_propertyCount(propertyCount)
		{
		}

		MessageTypeIdentifier m_messageTypeIdentifier;
		Guid m_typeGuid;
		uint8 m_propertyCount;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::RegisterPropertyStreamMessage>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::RegisterPropertyStreamMessage>(
			"{F41565AB-F634-48FE-A91F-252487E54E88}"_guid,
			MAKE_UNICODE_LITERAL("Register Property Stream Message"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning | Reflection::TypeFlags::DisableDynamicDeserialization |
				Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Message Type Identifier"),
					"messageTypeID",
					"{F3D10CA8-E45E-4201-BA50-4170D42DE660}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::RegisterPropertyStreamMessage::m_messageTypeIdentifier
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Type Guid"),
					"typeGuid",
					"{3FC2057B-627D-42DF-BEA9-785AB5DC3718}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::RegisterPropertyStreamMessage::m_typeGuid
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Property Count"),
					"propertyCount",
					"{EBAD1F02-CE63-4B72-880D-28FDDDBAB991}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::RegisterPropertyStreamMessage::m_propertyCount
				}
			},
			Reflection::Functions{}
		);
	};
}

namespace ngine::Network
{
	struct BatchMessage
	{
		BatchMessage() = default;
		BatchMessage(const uint16 messageCount)
			: m_messageCount(messageCount)
		{
		}

		uint16 m_messageCount;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::BatchMessage>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::BatchMessage>(
			"583b0fef-f56f-494e-8a54-177b52a57a2e"_guid,
			MAKE_UNICODE_LITERAL("Batch Message"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning | Reflection::TypeFlags::DisableDynamicDeserialization |
				Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{Reflection::Property{
				MAKE_UNICODE_LITERAL("Message Count"),
				"eventGuid",
				"{465D3460-29CD-48B2-B7CB-A948045B5C6B}"_guid,
				MAKE_UNICODE_LITERAL("Network"),
				Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
				&Network::BatchMessage::m_messageCount
			}}
		);
	};
}

namespace ngine::Network
{
	struct ObjectBoundMessage
	{
		ObjectBoundMessage() = default;
		ObjectBoundMessage(const BoundObjectIdentifier boundObjectIdentifier, const Guid persistentObjectGuid)
			: m_boundObjectIdentifier(boundObjectIdentifier)
			, m_persistentObjectGuid(persistentObjectGuid)
		{
		}

		BoundObjectIdentifier m_boundObjectIdentifier;
		Guid m_persistentObjectGuid;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::ObjectBoundMessage>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::ObjectBoundMessage>(
			"b9fa1677-6de4-4ac2-8540-b9c63d3472d4"_guid,
			MAKE_UNICODE_LITERAL("Object Bound Message"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning | Reflection::TypeFlags::DisableDynamicDeserialization |
				Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Bound Object Identifier"),
					"boundObjectIdentifier",
					"{9B0F80B8-D5D0-48DE-8478-222FADD95E3B}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::ObjectBoundMessage::m_boundObjectIdentifier
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Persistent Object Guid"),
					"objectGuid",
					"{96794D62-BE51-4A96-AE56-64F8E0022A78}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::ObjectBoundMessage::m_persistentObjectGuid
				}
			}
		);
	};
}

namespace ngine::Network
{
	struct ConfirmPropagatedPropertyReceiptMessage
	{
		ConfirmPropagatedPropertyReceiptMessage() = default;
		ConfirmPropagatedPropertyReceiptMessage(const SequenceNumber sequenceNumber, const MessageTypeIdentifier messageTypeIdentifier)
			: m_sequenceNumber(sequenceNumber)
			, m_messageTypeIdentifier(messageTypeIdentifier)
		{
		}

		SequenceNumber m_sequenceNumber;
		MessageTypeIdentifier m_messageTypeIdentifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::ConfirmPropagatedPropertyReceiptMessage>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::ConfirmPropagatedPropertyReceiptMessage>(
			"{6FDA1B86-7A20-49C0-BC36-0F9D63AA8BD7}"_guid,
			MAKE_UNICODE_LITERAL("Confirm Propagated Property Receipt Message"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning | Reflection::TypeFlags::DisableDynamicDeserialization |
				Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Sequence Number"),
					"sequenceNumber",
					"{83ACA648-7F2D-4EBD-8E70-ABC59AB320FD}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::ConfirmPropagatedPropertyReceiptMessage::m_sequenceNumber
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Message Type Identifier"),
					"messageTypeIdentifier",
					"{1DDCF464-7060-41F6-83CA-03C358E7E84F}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::ConfirmPropagatedPropertyReceiptMessage::m_messageTypeIdentifier
				}
			}
		);
	};
}

namespace ngine::Network
{
	struct BoundObjectAuthorityChangedMessage
	{
		BoundObjectAuthorityChangedMessage() = default;
		BoundObjectAuthorityChangedMessage(const BoundObjectIdentifier boundObjectIdentifier)
			: m_boundObjectIdentifier(boundObjectIdentifier)
		{
		}

		BoundObjectIdentifier m_boundObjectIdentifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::BoundObjectAuthorityChangedMessage>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::BoundObjectAuthorityChangedMessage>(
			"{48DCEED2-0876-4B27-B55B-58C4678D4A03}"_guid,
			MAKE_UNICODE_LITERAL("Bound Object Authority Changed Message"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning | Reflection::TypeFlags::DisableDynamicDeserialization |
				Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{Reflection::Property{
				MAKE_UNICODE_LITERAL("Bound Object Identifier"),
				"boundObjectIdentifier",
				"{D73A234B-1FB1-42A6-83BB-11BF91D00E12}"_guid,
				MAKE_UNICODE_LITERAL("Network"),
				Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
				&Network::BoundObjectAuthorityChangedMessage::m_boundObjectIdentifier
			}}
		);
	};
}

namespace ngine::Network
{
	struct ForwardedMessage
	{
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::ForwardedMessage>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::ForwardedMessage>(
			"{B1B02701-77AA-44D1-A4D9-41402EA3A660}"_guid,
			MAKE_UNICODE_LITERAL("Forward Message"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning | Reflection::TypeFlags::DisableDynamicDeserialization |
				Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{}
		);
	};
}

namespace ngine::Network
{
	//! Sent to the host to request synchronizing timestamps
	struct RequestTimeSyncMessage
	{
		RequestTimeSyncMessage() = default;
		RequestTimeSyncMessage(
			const Time::Timestamp hostTimestamp, const Time::Timestamp clientReceivedTimestamp, const Time::Timestamp clientSentTimestamp
		)
			: m_hostTimestamp(hostTimestamp)
			, m_clientReceivedTimestamp(clientReceivedTimestamp)
			, m_clientSentTimestamp(clientSentTimestamp)
		{
		}

		Time::Timestamp m_hostTimestamp;
		Time::Timestamp m_clientReceivedTimestamp;
		Time::Timestamp m_clientSentTimestamp;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::RequestTimeSyncMessage>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::RequestTimeSyncMessage>(
			"d41f4dd3-e350-44a5-924f-f5619cd9c608"_guid,
			MAKE_UNICODE_LITERAL("Request Time Sync Message"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning | Reflection::TypeFlags::DisableDynamicDeserialization |
				Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Host Timestamp"),
					"hostTimestamp",
					"30afc1f5-d255-4afb-994a-cd20ba24335b"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::RequestTimeSyncMessage::m_hostTimestamp
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Client Timestamp"),
					"clientReceivedTimestamp",
					"172823b8-3195-4702-851f-247cbd221e70"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::RequestTimeSyncMessage::m_clientReceivedTimestamp
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Client Timestamp"),
					"clientSentTimestamp",
					"dfd58895-92de-4b12-bb1c-0c44ae643039"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Network::RequestTimeSyncMessage::m_clientSentTimestamp
				}
			}
		);
	};
}

namespace ngine::Network
{
	//! Sent to the host to request synchronizing timestamps
	struct ReceivedTimeSyncResponseMessage
	{
		ReceivedTimeSyncResponseMessage() = default;
		ReceivedTimeSyncResponseMessage(const int64 timeOffsetInNanoseconds)
			: m_timeOffsetInNanoseconds(timeOffsetInNanoseconds)
		{
		}

		int64 m_timeOffsetInNanoseconds;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Network::ReceivedTimeSyncResponseMessage>
	{
		inline static constexpr auto Type = Reflection::Reflect<Network::ReceivedTimeSyncResponseMessage>(
			"1b304630-3c83-4b0a-8db2-95b9d13d5cbe"_guid,
			MAKE_UNICODE_LITERAL("Received Time Sync Response Message"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning | Reflection::TypeFlags::DisableDynamicDeserialization |
				Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{Reflection::Property{
				MAKE_UNICODE_LITERAL("Client Timestamp"),
				"timeOffsetInNanoseconds",
				"172823b8-3195-4702-851f-247cbd221e70"_guid,
				MAKE_UNICODE_LITERAL("Network"),
				Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
				&Network::ReceivedTimeSyncResponseMessage::m_timeOffsetInNanoseconds
			}}
		);
	};
}
