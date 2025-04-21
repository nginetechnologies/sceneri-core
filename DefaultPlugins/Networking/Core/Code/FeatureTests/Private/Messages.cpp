#include <Common/Memory/New.h>

#include <Engine/Tests/FeatureTest.h>

#include <NetworkingCore/Plugin.h>
#include <NetworkingCore/Host/LocalHost.h>
#include <NetworkingCore/Client/LocalClient.h>

#include <Common/System/Query.h>
#include <Common/Threading/AtomicEnum.h>
#include <Common/Network/Address.h>

#include <Common/Project System/PluginDatabase.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Network
{
	// TODO: Compressors
	// Math::Range
	// Math::Ratio (+ signed & unsigned?)
	// Math::WorldCoordinate (this needs to be able to differ for different games in the future)
	// Math::WorldRotation (this needs to be able to differ for different games in the future)
	// Math::LocalCoordinate (this needs to be able to differ for different games in the future)
	// Math::LocalRotation (this needs to be able to differ for different games in the future)
	// Math::Anglef (-PI - PI)
	// Math::Colorf
	// primitives (uint8, int8, etc + half)
	// string
	// Float - maybe allow specifying min, max, min / max deltas between numbers in a template etc. Then we can automatically find best
	// compression (combine with ranges in UI?).

	// i.e. Quat -> specify quality of compression (some higher level system can switch between different levels)
	// Position -> compressed to min / max
}

namespace ngine::Tests::Network
{
	using namespace ngine::Network;

	enum class StateFlags : uint8
	{
		ServerAwaitingClientConnection = 1 << 0,
		ClientAwaitingServerConnection = 1 << 1,
		ServerAwaitingClientDisconnect = 1 << 2,
		ClientAwaitingServerDisconnect = 1 << 3,
		All = ServerAwaitingClientConnection | ClientAwaitingServerConnection | ServerAwaitingClientDisconnect | ClientAwaitingServerDisconnect
	};
	ENUM_FLAG_OPERATORS(StateFlags);

	FEATURE_TEST(Networking, ConnectAndDisconnect)
	{
		[[maybe_unused]] Engine& engine = GetEngine();

		if (!engine.IsPluginLoaded(Manager::Guid))
		{
			const EnginePluginDatabase pluginDatabase(
				IO::Path::Combine(System::Get<IO::Filesystem>().GetEnginePath(), EnginePluginDatabase::FileName)
			);
			const Engine::PluginLoadResult pluginLoadResult = engine.LoadPlugin(Manager::Guid);
			ASSERT_TRUE(pluginLoadResult.pPluginInstance.IsValid());
			Threading::Atomic<bool> loaded{false};
			pluginLoadResult.jobBatch.GetFinishedStage().AddSubsequentStage(Threading::CreateCallback(
				[&loaded](Threading::JobRunnerThread&)
				{
					loaded = true;
				},
				Threading::JobPriority::LoadPlugin
			));
			Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
			thread.Queue(pluginLoadResult.jobBatch);
			while (!loaded)
			{
				thread.DoRunNextJob();
			}
		}

		[[maybe_unused]] Manager& networkManager = *System::FindPlugin<Manager>();
		LocalHost localHost;
		constexpr uint8 maximumClientCount = 1;
		const uint8 maximumChannelCount = 2;
		const uint32 incomingBandwidth = 0;
		const uint32 outgoingBandwidth = 0;
		localHost.Start(
			Network::AnyIPAddress,
			maximumClientCount,
			maximumChannelCount,
			incomingBandwidth,
			outgoingBandwidth,
			Network::LocalPeer::UpdateMode::EngineTick
		);
		EXPECT_TRUE(localHost.IsValid());
		if (!localHost.IsValid())
		{
			return;
		}

		AtomicEnumFlags<StateFlags> stateFlags{StateFlags::All};
		ClientIdentifier savedClientIdentifier;

		localHost.OnClientConnected.Add(
			localHost,
			[&stateFlags,
		   &savedClientIdentifier](LocalHost&, const ClientIdentifier clientIdentifier, RemotePeerView, const BoundObjectIdentifier)
			{
				const EnumFlags<StateFlags> previousFlags = stateFlags.FetchAnd(~StateFlags::ServerAwaitingClientConnection);
				EXPECT_TRUE(previousFlags.IsSet(StateFlags::All));
				savedClientIdentifier = clientIdentifier;
			}
		);
		localHost.OnClientDisconnected.Add(
			localHost,
			[&stateFlags, &savedClientIdentifier](LocalHost&, const ClientIdentifier clientIdentifier)
			{
				const EnumFlags<StateFlags> previousFlags = stateFlags.FetchAnd(~StateFlags::ServerAwaitingClientDisconnect);
				EXPECT_TRUE(previousFlags.IsSet(StateFlags::ServerAwaitingClientDisconnect));
				EXPECT_TRUE(previousFlags.IsNotSet(StateFlags::ServerAwaitingClientConnection | StateFlags::ClientAwaitingServerConnection));
				EXPECT_EQ(clientIdentifier, savedClientIdentifier);
			}
		);

		LocalClient localClient;
		localClient.OnConnected.Add(
			localClient,
			[&stateFlags, &savedClientIdentifier](LocalClient& localClient, RemoteHost, const BoundObjectIdentifier)
			{
				const EnumFlags<StateFlags> previousFlags = stateFlags.FetchAnd(~StateFlags::ClientAwaitingServerConnection);
				EXPECT_TRUE(previousFlags.IsSet(StateFlags::ClientAwaitingServerDisconnect));
				EXPECT_TRUE(previousFlags.IsNotSet(StateFlags::ServerAwaitingClientConnection));
				EXPECT_TRUE(previousFlags.AreAllSet(StateFlags::ServerAwaitingClientDisconnect | StateFlags::ClientAwaitingServerDisconnect));
				EXPECT_EQ(localClient.GetIdentifier(), savedClientIdentifier);

				localClient.Disconnect();
			}
		);
		localClient.OnDisconnected.Add(
			localClient,
			[&stateFlags, &savedClientIdentifier](LocalClient& localClient)
			{
				const EnumFlags<StateFlags> previousFlags = stateFlags.FetchAnd(~StateFlags::ClientAwaitingServerDisconnect);
				EXPECT_TRUE(previousFlags.IsSet(StateFlags::ClientAwaitingServerDisconnect));
				EXPECT_TRUE(previousFlags.IsNotSet(StateFlags::ServerAwaitingClientConnection | StateFlags::ClientAwaitingServerConnection));
				EXPECT_EQ(localClient.GetIdentifier(), savedClientIdentifier);
			}
		);

		localClient.Start();
		const uint32 connectionUserData = 0;
		Network::RemoteHost remoteHost = localClient.Connect(
			Address(IO::URI(MAKE_URI("localhost"))),
			maximumChannelCount,
			connectionUserData,
			Network::LocalPeer::UpdateMode::EngineTick
		);
		EXPECT_TRUE(remoteHost.IsValid());

		// Run the main thread job runner until we finished loading
		RunMainThreadJobRunner(
			[&stateFlags, &localClient]()
			{
				return stateFlags.AreAnySet(StateFlags::All) || localClient.IsQueuedOrExecuting();
			}
		);

		EXPECT_EQ(stateFlags, StateFlags{});

		// Perform one last tick for the disconnect to finish
		engine.DoTick();

		localHost.Stop();
		// Run the main thread job runner until the host is ready to shut down
		RunMainThreadJobRunner(
			[&localHost]()
			{
				return localHost.IsQueuedOrExecuting();
			}
		);
	}

	struct ClientToServerMessageData
	{
		float f;
		int32 i;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::Network::ClientToServerMessageData>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::ClientToServerMessageData>(
			"5b3cae99-6486-41b7-8356-976b77cda83e"_guid,
			MAKE_UNICODE_LITERAL("Client to Server Test Message"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("f"),
					"f",
					"{5B15D76C-23D6-46D6-86AC-979D15438CBD}"_guid,
					MAKE_UNICODE_LITERAL("f"),
					Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::ClientToServerMessageData::f
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("i"),
					"i",
					"{FF9244AB-8AD3-431E-9610-185A56632E72}"_guid,
					MAKE_UNICODE_LITERAL("i"),
					Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::ClientToServerMessageData::i
				}
			}
		);
	};
}

namespace ngine::Tests::Network
{
	struct ServerToClientMessageData
	{
		float f;
		int32 i;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::Network::ServerToClientMessageData>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::ServerToClientMessageData>(
			"f6b97008-07d9-4bc6-ab63-4557d856a107"_guid,
			MAKE_UNICODE_LITERAL("Server to Client Test Message"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("f"),
					"f",
					"{1C2D95EA-73EF-4597-B861-33B3E596FE1E}"_guid,
					MAKE_UNICODE_LITERAL("f"),
					Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::ServerToClientMessageData::f
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("i"),
					"i",
					"{A0B0C9EA-EDEE-4CFE-95F0-D5B8ADA4A8D2}"_guid,
					MAKE_UNICODE_LITERAL("i"),
					Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::ServerToClientMessageData::i
				}
			}
		);
	};
}

namespace ngine::Tests::Network
{
	struct ServerToAllClientsMessageData
	{
		int32 i{1337};
		float f{9001.f};
	};

	static Threading::Atomic<bool> receivedServerToClientMessage{false};

	void ServerToClientMessage(LocalClient&, const RemotePeer, const Channel, const ServerToClientMessageData message)
	{
		EXPECT_EQ(message.i, 9001);
		EXPECT_NEAR(message.f, 9.001f, 0.005f);
		receivedServerToClientMessage = true;
	}
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::Network::ServerToAllClientsMessageData>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::ServerToAllClientsMessageData>(
			"7e1dc4d1-0204-482b-913f-62a3ff11e7f5"_guid,
			MAKE_UNICODE_LITERAL("Server to All Clients test message"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("i"),
					"i",
					"{5DB6A900-B4ED-452D-8CEA-FEB70EEAE2D5}"_guid,
					MAKE_UNICODE_LITERAL("i"),
					Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::ServerToAllClientsMessageData::i
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("f"),
					"f",
					"{9EB2776A-0974-46BA-BA85-755BD12C7F4D}"_guid,
					MAKE_UNICODE_LITERAL("f"),
					Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::ServerToAllClientsMessageData::f
				}
			}
		);
	};

	template<>
	struct ReflectedFunction<&Tests::Network::ServerToClientMessage>
	{
		inline static constexpr Function Function{
			"61e71994-199f-45dc-a843-e11da18f8146"_guid,
			MAKE_UNICODE_LITERAL("Server to Client Message Test"),
			&Tests::Network::ServerToClientMessage,
			FunctionFlags::HostToClient,
			ReturnType{},
			Argument{"3380aa7d-24d0-4c25-89f9-5a6d31d97f2f"_guid, MAKE_UNICODE_LITERAL("Client")},
			Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Peer")},
			Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
			Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
		};
	};
}

namespace ngine::Tests::Network
{
	void
	ClientToServerMessage(LocalHost& localHost, const RemotePeer remotePeer, const Channel channel, const ClientToServerMessageData message)
	{
		EXPECT_NEAR(message.f, 1.337f, 0.005f);
		EXPECT_EQ(message.i, 1337);

		ServerToClientMessageData serverToClientMessageData{9.001f, 9001};
		const MessageTypeIdentifier serverToClientMessageTypeIdentifier = localHost.FindMessageIdentifier<&ServerToClientMessage>();
		const bool wasSent = localHost.SendMessageTo(remotePeer, serverToClientMessageTypeIdentifier, channel, serverToClientMessageData);
		EXPECT_TRUE(wasSent);
	}
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedFunction<&Tests::Network::ClientToServerMessage>
	{
		inline static constexpr Reflection::Function Function{
			"0a39e5bb-d85b-432c-b5bd-bc1befa1b05f"_guid,
			MAKE_UNICODE_LITERAL("Client to Server Message Test"),
			&Tests::Network::ClientToServerMessage,
			Reflection::FunctionFlags::ClientToHost,
			Reflection::ReturnType{},
			Reflection::Argument{"3380aa7d-24d0-4c25-89f9-5a6d31d97f2f"_guid, MAKE_UNICODE_LITERAL("Host")},
			Reflection::Argument{"b9c412c3-61d4-425d-8875-e7040d506f20"_guid, MAKE_UNICODE_LITERAL("Remote Peer")},
			Reflection::Argument{"7fc60919-7869-4963-b4f1-3dc4f1dc2d45"_guid, MAKE_UNICODE_LITERAL("Channel")},
			Reflection::Argument{"f09a56d4-f0f7-4b74-915e-da295da0d41f"_guid, MAKE_UNICODE_LITERAL("Message View")}
		};
	};
}

namespace ngine::Tests::Network
{
	FEATURE_TEST(Networking, DynamicMessageTypes)
	{
		[[maybe_unused]] Engine& engine = GetEngine();

		if (!engine.IsPluginLoaded(Manager::Guid))
		{
			const EnginePluginDatabase pluginDatabase(
				IO::Path::Combine(System::Get<IO::Filesystem>().GetEnginePath(), EnginePluginDatabase::FileName)
			);
			const Engine::PluginLoadResult pluginLoadResult = engine.LoadPlugin(Manager::Guid);
			ASSERT_TRUE(pluginLoadResult.pPluginInstance.IsValid());
			Threading::Atomic<bool> loaded{false};
			pluginLoadResult.jobBatch.GetFinishedStage().AddSubsequentStage(Threading::CreateCallback(
				[&loaded](Threading::JobRunnerThread&)
				{
					loaded = true;
				},
				Threading::JobPriority::LoadPlugin
			));
			Threading::JobRunnerThread& thread = *Threading::JobRunnerThread::GetCurrent();
			thread.Queue(pluginLoadResult.jobBatch);
			while (!loaded)
			{
				thread.DoRunNextJob();
			}
		}

		Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
		reflectionRegistry.RegisterDynamicGlobalFunction<&Tests::Network::ClientToServerMessage>();
		reflectionRegistry.RegisterDynamicGlobalFunction<&Tests::Network::ServerToClientMessage>();

		[[maybe_unused]] Manager& networkManager = *System::FindPlugin<Manager>();
		LocalHost localHost;
		constexpr uint8 maximumClientCount = 1;
		const uint8 maximumChannelCount = 2;
		const uint32 incomingBandwidth = 0;
		const uint32 outgoingBandwidth = 0;
		localHost.Start(
			Network::AnyIPAddress,
			maximumClientCount,
			maximumChannelCount,
			incomingBandwidth,
			outgoingBandwidth,
			Network::LocalPeer::UpdateMode::EngineTick
		);
		EXPECT_TRUE(localHost.IsValid());
		if (!localHost.IsValid())
		{
			return;
		}

		const MessageTypeIdentifier clientToServerMessageTypeIdentifier = localHost.FindMessageIdentifier<&ClientToServerMessage>();
		bool once{false};

		LocalClient localClient;
		localClient.OnMessageTypeRegistered.Add(
			&localClient,
			[clientToServerMessageTypeIdentifier, &once](LocalClient& localClient, const MessageTypeIdentifier messageTypeIdentifier)
			{
				if (messageTypeIdentifier == clientToServerMessageTypeIdentifier)
				{
					EXPECT_FALSE(once);
					once = true;

					ClientToServerMessageData messageData{1.337f, 1337};
					const Channel channel{0};
					const bool wasSent = localClient.SendMessageToHost(messageTypeIdentifier, channel, messageData);
					EXPECT_TRUE(wasSent);
				}
			}
		);

		localClient.Start();
		const uint32 connectionUserData = 0;
		Network::RemoteHost remoteHost = localClient.Connect(
			Address(IO::URI(MAKE_URI("localhost"))),
			maximumChannelCount,
			connectionUserData,
			Network::LocalPeer::UpdateMode::EngineTick
		);
		EXPECT_TRUE(remoteHost.IsValid());

		// Run the main thread job runner until we finished loading
		RunMainThreadJobRunner(
			[]()
			{
				return !receivedServerToClientMessage;
			}
		);
		EXPECT_TRUE(receivedServerToClientMessage);

		{
			localClient.Disconnect();

			// Run the main thread job runner until we finished disconnecting
			RunMainThreadJobRunner(
				[&localClient]()
				{
					return localClient.IsConnected() || localClient.IsQueuedOrExecuting();
				}
			);
		}

		// Perform one last tick for the disconnect to finish
		engine.DoTick();

		localHost.Stop();
		// Run the main thread job runner until the host is ready to shut down
		RunMainThreadJobRunner(
			[&localHost]()
			{
				return localHost.IsQueuedOrExecuting();
			}
		);
	}
}
