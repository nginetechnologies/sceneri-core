#include <Common/Memory/New.h>

#include <Engine/Tests/FeatureTest.h>

#include <NetworkingCore/Plugin.h>
#include <NetworkingCore/Host/LocalHost.h>
#include <NetworkingCore/Host/RemoteHost.h>
#include <NetworkingCore/Client/LocalClient.h>
#include <NetworkingCore/Components/BoundObjectIdentifier.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentValue.inl>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <Common/System/Query.h>
#include <Common/Threading/AtomicEnum.h>
#include <Common/Math/Radius.h>
#include <Common/Compression/Compressor.h>
#include <Common/Network/Address.h>

#include <Common/Project System/PluginDatabase.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Tests::Network
{
	using namespace ngine::Network;

	struct BoundObjectTest : public Reflection::PropertyOwner
	{
		struct ClientToServerData
		{
			bool falseBool;
			bool trueBool;
			float floatValue;
			uint32 integerValue;
		};
		void ClientToServer(const LocalHost&, const RemoteHost, const Channel, const ClientToServerData data)
		{
			EXPECT_FALSE(data.falseBool);
			EXPECT_TRUE(data.trueBool);
			EXPECT_NEAR(data.floatValue, 133.7f, 0.005f);
			EXPECT_EQ(data.integerValue, 9001);

			EXPECT_FALSE(m_receivedClientToServer);
			m_receivedClientToServer = true;
		}
		void ClientToServerWithoutAuthority(const LocalHost&, const RemoteHost, const Channel, const ClientToServerData data)
		{
			EXPECT_FALSE(data.falseBool);
			EXPECT_TRUE(data.trueBool);
			EXPECT_NEAR(data.floatValue, 133.7f, 0.005f);
			EXPECT_EQ(data.integerValue, 9001);

			EXPECT_FALSE(m_receivedClientToServer);
			m_receivedClientToServer = true;
		}

		void ClientToOtherClients(const LocalClient&, const RemotePeer, const Channel, const ClientToServerData data)
		{
			EXPECT_FALSE(data.falseBool);
			EXPECT_TRUE(data.trueBool);
			EXPECT_NEAR(data.floatValue, 133.7f, 0.005f);
			EXPECT_EQ(data.integerValue, 9001);

			EXPECT_FALSE(m_receivedClientToOtherClients);
			m_receivedClientToOtherClients = true;
		}
		void ClientToAllRemotes(const LocalPeer&, const RemotePeer, const Channel, const ClientToServerData data)
		{
			EXPECT_FALSE(data.falseBool);
			EXPECT_TRUE(data.trueBool);
			EXPECT_NEAR(data.floatValue, 133.7f, 0.005f);
			EXPECT_EQ(data.integerValue, 9001);

			EXPECT_FALSE(m_receivedClientToAllRemotes);
			m_receivedClientToAllRemotes = true;
		}

		struct ServerToClientData
		{
			bool trueBool;
			bool falseBool;
			float floatValue;
			uint32 integerValue;
		};

		void ServerToClient(const LocalClient&, const RemoteHost, const Channel, const ServerToClientData data)
		{
			EXPECT_TRUE(data.trueBool);
			EXPECT_FALSE(data.falseBool);
			EXPECT_NEAR(data.floatValue, 133.7f, 0.005f);
			EXPECT_EQ(data.integerValue, 9001);

			EXPECT_FALSE(m_receivedServerToClient);
			m_receivedServerToClient = true;
		}

		enum class Flags : uint8
		{
			One = 1 << 0,
			Two = 1 << 1,
			Four = 1 << 2,
			Eight = 1 << 3,
			Sixteen = 1 << 4,
			ThirtyTwo = 1 << 5,
			SixtyFour = 1 << 6
		};

		EnumFlags<Flags> m_propagatedServerToClientFlags;
		EnumFlags<Flags> m_propagatedClientToServerFlags;
		EnumFlags<Flags> m_propagatedClientToAllRemotesFlags;

		Threading::Atomic<bool> m_receivedClientToServer{false};
		Threading::Atomic<bool> m_receivedServerToClient{false};
		Threading::Atomic<bool> m_receivedClientToOtherClients{false};
		Threading::Atomic<bool> m_receivedClientToAllRemotes{false};
	};
	ENUM_FLAG_OPERATORS(BoundObjectTest::Flags);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::Network::BoundObjectTest::ServerToClientData>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::BoundObjectTest::ServerToClientData>(
			"3ac481d1-d5ef-491f-8869-04a569e5174e"_guid,
			MAKE_UNICODE_LITERAL("Server to Client Data"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning | Reflection::TypeFlags::DisableDynamicDeserialization |
				Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("True Bool"),
					"trueBool",
					"{D6DAA97A-AECC-4E84-B605-06A6B7737DBB}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::BoundObjectTest::ServerToClientData::trueBool
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("False Bool"),
					"falseBool",
					"{42E22A88-8A91-494A-97AE-2E9A1EB24BF9}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::BoundObjectTest::ServerToClientData::falseBool
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Float"),
					"float",
					"{5AB8BF46-A701-4FA3-BFDC-DBE737212289}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::BoundObjectTest::ServerToClientData::floatValue
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Integer"),
					"integer",
					"{DE26395D-CA66-432A-8409-7ED435CC3AF9}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::BoundObjectTest::ServerToClientData::integerValue
				}
			},
			Reflection::Functions{}
		);
	};

	template<>
	struct ReflectedType<Tests::Network::BoundObjectTest::ClientToServerData>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::BoundObjectTest::ClientToServerData>(
			"e6926159-b9ab-42b4-a37f-583b278e0d67"_guid,
			MAKE_UNICODE_LITERAL("Client to Server Data"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation | Reflection::TypeFlags::DisableDynamicInstantiation |
				Reflection::TypeFlags::DisableDynamicCloning | Reflection::TypeFlags::DisableDynamicDeserialization |
				Reflection::TypeFlags::DisableWriteToDisk,
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("False Bool"),
					"falseBool",
					"{3E1BDF37-24DB-4F4F-8A5A-4B5895A25DCA}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::BoundObjectTest::ClientToServerData::falseBool
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("True Bool"),
					"trueBool",
					"{C4D528B7-56D3-42B3-A61B-A97B5D08387A}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::BoundObjectTest::ClientToServerData::trueBool
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Float"),
					"float",
					"{8006E630-9473-435F-91E4-4182000EBC93}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::BoundObjectTest::ClientToServerData::floatValue
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Integer"),
					"integer",
					"{58868C7B-6BC8-4317-897B-E42D102280B1}"_guid,
					MAKE_UNICODE_LITERAL("Network"),
					Reflection::PropertyFlags::Transient | Reflection::PropertyFlags::SentWithNetworkedFunctions,
					&Tests::Network::BoundObjectTest::ClientToServerData::integerValue
				}
			},
			Reflection::Functions{}
		);
	};

	template<>
	struct ReflectedType<Tests::Network::BoundObjectTest::Flags>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::BoundObjectTest::Flags>(
			"{DF35C7D1-5685-496A-912F-79121BB68D3B}"_guid,
			MAKE_UNICODE_LITERAL("Test Flags"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{Tests::Network::BoundObjectTest::Flags::One, MAKE_UNICODE_LITERAL("1")},
				Reflection::EnumTypeEntry{Tests::Network::BoundObjectTest::Flags::Two, MAKE_UNICODE_LITERAL("2")},
				Reflection::EnumTypeEntry{Tests::Network::BoundObjectTest::Flags::Four, MAKE_UNICODE_LITERAL("4")},
				Reflection::EnumTypeEntry{Tests::Network::BoundObjectTest::Flags::Eight, MAKE_UNICODE_LITERAL("8")},
				Reflection::EnumTypeEntry{Tests::Network::BoundObjectTest::Flags::Sixteen, MAKE_UNICODE_LITERAL("16")},
				Reflection::EnumTypeEntry{Tests::Network::BoundObjectTest::Flags::ThirtyTwo, MAKE_UNICODE_LITERAL("32")},
				Reflection::EnumTypeEntry{Tests::Network::BoundObjectTest::Flags::SixtyFour, MAKE_UNICODE_LITERAL("64")},
			}}
		);
	};

	template<>
	struct ReflectedType<Tests::Network::BoundObjectTest>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::BoundObjectTest>(
			"d549ad6b-fc70-4255-af2a-ac639f7513fa"_guid,
			MAKE_UNICODE_LITERAL("Networked Test Object"),
			Reflection::TypeFlags{},
			Tags{},
			Properties{
				Property{
					MAKE_UNICODE_LITERAL("Server to Client propagated flags"),
					"serverToClientFlags",
					"{95187048-35DD-4BB2-B689-D518946399E0}"_guid,
					MAKE_UNICODE_LITERAL("TestComponent"),
					PropertyFlags::PropagateHostToClient,
					&Tests::Network::BoundObjectTest::m_propagatedServerToClientFlags
				},
				Property{
					MAKE_UNICODE_LITERAL("Client to Server propagated flags"),
					"clientToServerFlags",
					"{EB079351-7E68-4604-B982-B70498A07D19}"_guid,
					MAKE_UNICODE_LITERAL("TestComponent"),
					PropertyFlags::PropagateClientToHost,
					&Tests::Network::BoundObjectTest::m_propagatedClientToServerFlags
				},
				Property{
					MAKE_UNICODE_LITERAL("Client to All Remotes propagated flags"),
					"clientToAllRemotesFlags",
					"{FB0A5085-2E0A-42EF-B9AF-DE0638D0E134}"_guid,
					MAKE_UNICODE_LITERAL("TestComponent"),
					PropertyFlags::PropagateClientToHost | PropertyFlags::PropagateClientToClient,
					&Tests::Network::BoundObjectTest::m_propagatedClientToAllRemotesFlags
				}
			},
			Functions{
				Function{
					"2349100e-f0bd-43b0-a218-a55056184d7b"_guid,
					MAKE_UNICODE_LITERAL("Client to Server Function Test"),
					&Tests::Network::BoundObjectTest::ClientToServer,
					FunctionFlags::ClientToHost,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("localHost")},
					Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("remotePeer")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("channel")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"{80C36A1F-8BD7-4283-8405-C18D5EDD5C58}"_guid,
					MAKE_UNICODE_LITERAL("Client to Server Function Test Without Authority"),
					&Tests::Network::BoundObjectTest::ClientToServerWithoutAuthority,
					FunctionFlags::ClientToHost | FunctionFlags::AllowClientToHostWithoutAuthority,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("localHost")},
					Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("remotePeer")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("channel")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"{F8A87BEE-B399-40A1-BBED-1E10102FAD8C}"_guid,
					MAKE_UNICODE_LITERAL("Client to Other Clients"),
					&Tests::Network::BoundObjectTest::ClientToOtherClients,
					FunctionFlags::ClientToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("localHost")},
					Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("remotePeer")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("channel")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"{AB390007-8874-4875-B93F-3D607FE60096}"_guid,
					MAKE_UNICODE_LITERAL("Client to Server Function Test"),
					&Tests::Network::BoundObjectTest::ClientToAllRemotes,
					FunctionFlags::ClientToHost | FunctionFlags::ClientToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("localHost")},
					Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("remotePeer")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("channel")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				},
				Function{
					"67296438-342a-4114-8483-7429179c16a2"_guid,
					MAKE_UNICODE_LITERAL("Server to Client Function Test"),
					&Tests::Network::BoundObjectTest::ServerToClient,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("remotePeer")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("channel")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("data")}
				}
			}
		);
	};
}

namespace ngine::Tests::Network
{
	FEATURE_TEST(Networking, BindObjectStandaloneServer)
	{
		Engine& engine = GetEngine();

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

		if (!System::Get<Reflection::Registry>().FindTypeInterface(Reflection::GetTypeGuid<BoundObjectTest>()))
		{
			System::Get<Reflection::Registry>().RegisterDynamicType<BoundObjectTest>();
		}

		[[maybe_unused]] Manager& networkManager = *System::FindPlugin<Manager>();

		Network::LocalHost host;
		constexpr uint8 maximumClientCount = 1;
		const uint8 maximumChannelCount = 2;
		const uint32 incomingBandwidth = 0;
		const uint32 outgoingBandwidth = 0;
		host.Start(
			Network::AnyIPAddress,
			maximumClientCount,
			maximumChannelCount,
			incomingBandwidth,
			outgoingBandwidth,
			Network::LocalPeer::UpdateMode::EngineTick
		);

		constexpr Guid boundObjectGuid = "b918cde0-272f-4dec-8467-54b2e3c29154"_guid;

		Array<Tests::Network::BoundObjectTest, 2> boundObjects;

		// Bind the server object
		const BoundObjectIdentifier hostBoundObjectIdentifier = host.BindObject(boundObjectGuid, AnyView(boundObjects[0]));
		EXPECT_TRUE(hostBoundObjectIdentifier.IsValid());

		host.OnClientConnected.Add(
			host,
			[](LocalHost&, const ClientIdentifier, RemotePeerView, const BoundObjectIdentifier)
			{
			}
		);
		host.OnClientDisconnected.Add(
			host,
			[](LocalHost&, const ClientIdentifier)
			{
			}
		);

		Network::LocalClient localClient;
		localClient.Start();

		{
			Threading::Atomic<bool> hasConnected{0};

			localClient.OnConnected.Add(
				localClient,
				[&hasConnected](LocalClient&, Network::RemoteHost, const BoundObjectIdentifier)
				{
					hasConnected = true;
				}
			);

			const uint32 connectionUserData = 0;
			Network::RemoteHost remoteHost = localClient.Connect(
				Address(IO::URI(MAKE_URI("localhost"))),
				maximumChannelCount,
				connectionUserData,
				Network::LocalPeer::UpdateMode::EngineTick
			);
			EXPECT_TRUE(remoteHost.IsValid());

			BoundObjectIdentifier clientBoundObjectIdentifier;
			// Start binding the client's equivalent of the object
			localClient.BindObject(
				boundObjectGuid,
				AnyView(boundObjects[1]),
				[hostBoundObjectIdentifier, &clientBoundObjectIdentifier](const BoundObjectIdentifier boundObjectIdentifier)
				{
					EXPECT_FALSE(clientBoundObjectIdentifier.IsValid());
					EXPECT_EQ(boundObjectIdentifier, hostBoundObjectIdentifier);
					clientBoundObjectIdentifier = boundObjectIdentifier;
				}
			);

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&hasConnected, &clientBoundObjectIdentifier]()
				{
					return !hasConnected || !clientBoundObjectIdentifier.IsValid();
				}
			);
		}

		{
			Threading::Atomic<bool> hasDisconnected{false};

			localClient.OnDisconnected.Add(
				localClient,
				[&hasDisconnected](LocalClient&)
				{
					hasDisconnected = true;
				}
			);

			localClient.Disconnect();

			// Run the main thread job runner until we finished disconnecting
			RunMainThreadJobRunner(
				[&hasDisconnected, &localClient]()
				{
					return !hasDisconnected || localClient.IsQueuedOrExecuting();
				}
			);

			// Perform one last tick for the disconnect to finish
			engine.DoTick();
		}

		host.Stop();
		// Run the main thread job runner until the host is ready to shut down
		RunMainThreadJobRunner(
			[&host]()
			{
				return host.IsQueuedOrExecuting();
			}
		);
	}

	FEATURE_TEST(Networking, BindObjectAndMessageStandaloneServer)
	{
		Engine& engine = GetEngine();

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

		if (!System::Get<Reflection::Registry>().FindTypeInterface(Reflection::GetTypeGuid<BoundObjectTest>()).IsValid())
		{
			System::Get<Reflection::Registry>().RegisterDynamicType<BoundObjectTest>();
		}

		[[maybe_unused]] Manager& networkManager = *System::FindPlugin<Manager>();

		Network::LocalHost host;
		constexpr uint8 maximumClientCount = 1;
		const uint8 maximumChannelCount = 2;
		const uint32 incomingBandwidth = 0;
		const uint32 outgoingBandwidth = 0;
		host.Start(
			Network::AnyIPAddress,
			maximumClientCount,
			maximumChannelCount,
			incomingBandwidth,
			outgoingBandwidth,
			Network::LocalPeer::UpdateMode::EngineTick
		);

		constexpr Guid boundObjectGuid = "0952ad47-492b-4db7-a519-76af8b79725e"_guid;

		Array<Tests::Network::BoundObjectTest, 2> boundObjects;

		// Bind the server object
		const BoundObjectIdentifier hostBoundObjectIdentifier = host.BindObject(boundObjectGuid, AnyView(boundObjects[0]));
		EXPECT_TRUE(hostBoundObjectIdentifier.IsValid());

		host.OnClientConnected.Add(
			host,
			[](LocalHost&, const ClientIdentifier, RemotePeerView, const BoundObjectIdentifier)
			{
			}
		);
		host.OnClientDisconnected.Add(
			host,
			[](LocalHost&, const ClientIdentifier)
			{
			}
		);

		LocalClient localClient;
		localClient.Start();

		{
			Threading::Atomic<bool> hasConnected{0};

			localClient.OnConnected.Add(
				localClient,
				[&hasConnected](LocalClient&, Network::RemoteHost, const BoundObjectIdentifier)
				{
					hasConnected = true;
				}
			);

			const uint32 connectionUserData = 0;
			Network::RemoteHost remoteHost = localClient.Connect(
				Address(IO::URI(MAKE_URI("localhost"))),
				maximumChannelCount,
				connectionUserData,
				Network::LocalPeer::UpdateMode::EngineTick
			);
			EXPECT_TRUE(remoteHost.IsValid());

			BoundObjectIdentifier clientBoundObjectIdentifier;
			// Start binding the client's equivalent of the object
			localClient.BindObject(
				boundObjectGuid,
				AnyView(boundObjects[1]),
				[hostBoundObjectIdentifier, &clientBoundObjectIdentifier](const BoundObjectIdentifier boundObjectIdentifier)
				{
					EXPECT_FALSE(clientBoundObjectIdentifier.IsValid());
					EXPECT_EQ(boundObjectIdentifier, hostBoundObjectIdentifier);
					clientBoundObjectIdentifier = boundObjectIdentifier;
				}
			);

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&hasConnected, &clientBoundObjectIdentifier]()
				{
					return !hasConnected || !clientBoundObjectIdentifier.IsValid();
				}
			);
		}

		// Now test messaging
		{

			Network::Channel channel{0};
			BoundObjectTest::ClientToServerData clientToServerData{false, true, 133.7f, 9001};
			const bool wasSent =
				localClient.SendMessageToHost<&BoundObjectTest::ClientToServerWithoutAuthority, BoundObjectTest::ClientToServerData>(
					hostBoundObjectIdentifier,
					channel,
					clientToServerData
				);
			EXPECT_TRUE(wasSent);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundServerObject = boundObjects[0]]()
				{
					return !boundServerObject.m_receivedClientToServer;
				}
			);
		}

		{
			Network::Channel channel{0};
			BoundObjectTest::ServerToClientData serverToClientData{true, false, 133.7f, 9001};
			const bool wasSent =
				host.BroadcastMessageToAllClients<&BoundObjectTest::ServerToClient>(hostBoundObjectIdentifier, channel, serverToClientData);
			EXPECT_TRUE(wasSent);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundClientObject = boundObjects[1]]()
				{
					return !boundClientObject.m_receivedServerToClient;
				}
			);
		}

		{
			Threading::Atomic<bool> hasDisconnected{false};

			localClient.OnDisconnected.Add(
				localClient,
				[&hasDisconnected](LocalClient&)
				{
					hasDisconnected = true;
				}
			);

			localClient.Disconnect();

			// Run the main thread job runner until we finished disconnecting
			RunMainThreadJobRunner(
				[&hasDisconnected, &localClient]()
				{
					return !hasDisconnected || localClient.IsQueuedOrExecuting();
				}
			);

			// Perform one last tick for the disconnect to finish
			engine.DoTick();
		}

		host.Stop();
		// Run the main thread job runner until the host is ready to shut down
		RunMainThreadJobRunner(
			[&host]()
			{
				return host.IsQueuedOrExecuting();
			}
		);
	}

	FEATURE_TEST(Networking, BindObjectAndDelegateAuthorityStandaloneServer)
	{
		Engine& engine = GetEngine();

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

		if (!System::Get<Reflection::Registry>().FindTypeInterface(Reflection::GetTypeGuid<BoundObjectTest>()).IsValid())
		{
			System::Get<Reflection::Registry>().RegisterDynamicType<BoundObjectTest>();
		}

		[[maybe_unused]] Manager& networkManager = *System::FindPlugin<Manager>();

		Network::LocalHost host;
		constexpr uint8 maximumClientCount = 2;
		const uint8 maximumChannelCount = 2;
		const uint32 incomingBandwidth = 0;
		const uint32 outgoingBandwidth = 0;
		host.Start(
			Network::AnyIPAddress,
			maximumClientCount,
			maximumChannelCount,
			incomingBandwidth,
			outgoingBandwidth,
			Network::LocalPeer::UpdateMode::EngineTick
		);

		constexpr Guid boundObjectGuid = "0952ad47-492b-4db7-a519-76af8b79725e"_guid;

		Array<Tests::Network::BoundObjectTest, maximumClientCount + 1> boundObjects;

		// Bind the server object
		const BoundObjectIdentifier hostBoundObjectIdentifier = host.BindObject(boundObjectGuid, AnyView(boundObjects[0]));
		EXPECT_TRUE(hostBoundObjectIdentifier.IsValid());

		host.OnClientConnected.Add(
			host,
			[](LocalHost&, const ClientIdentifier, RemotePeerView, const BoundObjectIdentifier)
			{
			}
		);
		host.OnClientDisconnected.Add(
			host,
			[](LocalHost&, const ClientIdentifier)
			{
			}
		);

		Array<LocalClient, maximumClientCount> localClients;
		for (LocalClient& localClient : localClients)
		{
			localClient.Start();
		}

		for (LocalClient& localClient : localClients)
		{
			const uint8 clientIndex = localClients.GetIteratorIndex(&localClient);
			Threading::Atomic<bool> hasConnected{false};

			localClient.OnConnected.Add(
				localClient,
				[&hasConnected](LocalClient&, Network::RemoteHost, const BoundObjectIdentifier)
				{
					hasConnected = true;
				}
			);

			const uint32 connectionUserData = 0;
			Network::RemoteHost remoteHost = localClient.Connect(
				Address(IO::URI(MAKE_URI("localhost"))),
				maximumChannelCount,
				connectionUserData,
				Network::LocalPeer::UpdateMode::EngineTick
			);
			EXPECT_TRUE(remoteHost.IsValid());

			BoundObjectIdentifier clientBoundObjectIdentifier;
			// Start binding the client's equivalent of the object
			localClient.BindObject(
				boundObjectGuid,
				AnyView(boundObjects[1 + clientIndex]),
				[hostBoundObjectIdentifier, &clientBoundObjectIdentifier](const BoundObjectIdentifier boundObjectIdentifier)
				{
					EXPECT_FALSE(clientBoundObjectIdentifier.IsValid());
					EXPECT_EQ(boundObjectIdentifier, hostBoundObjectIdentifier);
					clientBoundObjectIdentifier = boundObjectIdentifier;
				}
			);

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&hasConnected, &clientBoundObjectIdentifier]()
				{
					return !hasConnected || !clientBoundObjectIdentifier.IsValid();
				}
			);
		}

		EXPECT_TRUE(host.HasAuthorityOfBoundObject(hostBoundObjectIdentifier));
		EXPECT_FALSE(localClients[0].HasAuthorityOfBoundObject(hostBoundObjectIdentifier));
		EXPECT_FALSE(localClients[1].HasAuthorityOfBoundObject(hostBoundObjectIdentifier));

		// Attempt sending without authority
		for (LocalClient& localClient : localClients)
		{
			{
				Network::Channel channel{0};
				BoundObjectTest::ClientToServerData clientToServerData{false, true, 133.7f, 9001};
				const bool wasSent = localClient.SendMessageToHost<&BoundObjectTest::ClientToServer, BoundObjectTest::ClientToServerData>(
					hostBoundObjectIdentifier,
					channel,
					clientToServerData
				);
				EXPECT_FALSE(wasSent);
				EXPECT_FALSE(boundObjects[0].m_receivedClientToServer);
				EXPECT_FALSE(boundObjects[1].m_receivedClientToServer);
				EXPECT_FALSE(boundObjects[2].m_receivedClientToServer);
			}
			{
				Network::Channel channel{0};
				BoundObjectTest::ClientToServerData clientToServerData{false, true, 133.7f, 9001};
				const bool wasSent =
					localClient.SendMessageToRemoteClients<&BoundObjectTest::ClientToOtherClients, BoundObjectTest::ClientToServerData>(
						hostBoundObjectIdentifier,
						channel,
						clientToServerData
					);
				EXPECT_FALSE(wasSent);
				EXPECT_FALSE(boundObjects[0].m_receivedClientToOtherClients);
				EXPECT_FALSE(boundObjects[1].m_receivedClientToOtherClients);
				EXPECT_FALSE(boundObjects[2].m_receivedClientToOtherClients);
			}
			{
				Network::Channel channel{0};
				BoundObjectTest::ClientToServerData clientToServerData{false, true, 133.7f, 9001};
				const bool wasSent =
					localClient.SendMessageToRemoteClients<&BoundObjectTest::ClientToAllRemotes, BoundObjectTest::ClientToServerData>(
						hostBoundObjectIdentifier,
						channel,
						clientToServerData
					);
				EXPECT_FALSE(wasSent);
				EXPECT_FALSE(boundObjects[0].m_receivedClientToOtherClients);
				EXPECT_FALSE(boundObjects[1].m_receivedClientToOtherClients);
				EXPECT_FALSE(boundObjects[2].m_receivedClientToOtherClients);
			}
		}

		// Test delegating authority to the object
		{
			EXPECT_TRUE(host.HasAuthorityOfBoundObject(hostBoundObjectIdentifier));
			EXPECT_FALSE(localClients[0].HasAuthorityOfBoundObject(hostBoundObjectIdentifier));
			EXPECT_FALSE(localClients[1].HasAuthorityOfBoundObject(hostBoundObjectIdentifier));

			host.DelegateBoundObjectAuthority(hostBoundObjectIdentifier, localClients[0].GetIdentifier());

			// Run the main thread job runner until we received authority
			RunMainThreadJobRunner(
				[&localClient = localClients[0], hostBoundObjectIdentifier]()
				{
					return !localClient.HasAuthorityOfBoundObject(hostBoundObjectIdentifier);
				}
			);

			EXPECT_FALSE(host.HasAuthorityOfBoundObject(hostBoundObjectIdentifier));
			EXPECT_TRUE(localClients[0].HasAuthorityOfBoundObject(hostBoundObjectIdentifier));
			EXPECT_FALSE(localClients[1].HasAuthorityOfBoundObject(hostBoundObjectIdentifier));
		}

		// Now make sure the client with authority can send the message
		{
			EXPECT_FALSE(boundObjects[0].m_receivedClientToServer);
			EXPECT_FALSE(boundObjects[1].m_receivedClientToServer);
			EXPECT_FALSE(boundObjects[2].m_receivedClientToServer);

			Network::Channel channel{0};
			BoundObjectTest::ClientToServerData clientToServerData{false, true, 133.7f, 9001};
			const bool wasSent = localClients[0].SendMessageToHost<&BoundObjectTest::ClientToServer, BoundObjectTest::ClientToServerData>(
				hostBoundObjectIdentifier,
				channel,
				clientToServerData
			);
			EXPECT_TRUE(wasSent);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundServerObject = boundObjects[0]]()
				{
					return !boundServerObject.m_receivedClientToServer;
				}
			);

			EXPECT_TRUE(boundObjects[0].m_receivedClientToServer);
			EXPECT_FALSE(boundObjects[1].m_receivedClientToServer);
			EXPECT_FALSE(boundObjects[2].m_receivedClientToServer);
		}

		// Also make sure the other client without authority can't send the message
		{
			Network::Channel channel{0};
			BoundObjectTest::ClientToServerData clientToServerData{false, true, 133.7f, 9001};
			const bool wasSent = localClients[1].SendMessageToHost<&BoundObjectTest::ClientToServer, BoundObjectTest::ClientToServerData>(
				hostBoundObjectIdentifier,
				channel,
				clientToServerData
			);
			EXPECT_FALSE(wasSent);
		}

		{
			EXPECT_FALSE(boundObjects[0].m_receivedClientToOtherClients);
			EXPECT_FALSE(boundObjects[1].m_receivedClientToOtherClients);
			EXPECT_FALSE(boundObjects[2].m_receivedClientToOtherClients);

			Network::Channel channel{0};
			BoundObjectTest::ClientToServerData clientToServerData{false, true, 133.7f, 9001};
			const bool wasSent =
				localClients[0].SendMessageToRemoteClients<&BoundObjectTest::ClientToOtherClients, BoundObjectTest::ClientToServerData>(
					hostBoundObjectIdentifier,
					channel,
					clientToServerData
				);
			EXPECT_TRUE(wasSent);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundOtherClientObject = boundObjects[2]]()
				{
					return !boundOtherClientObject.m_receivedClientToOtherClients;
				}
			);

			EXPECT_FALSE(boundObjects[0].m_receivedClientToOtherClients);
			EXPECT_FALSE(boundObjects[1].m_receivedClientToOtherClients);
			EXPECT_TRUE(boundObjects[2].m_receivedClientToOtherClients);
		}
		{
			EXPECT_FALSE(boundObjects[0].m_receivedClientToAllRemotes);
			EXPECT_FALSE(boundObjects[1].m_receivedClientToAllRemotes);
			EXPECT_FALSE(boundObjects[2].m_receivedClientToAllRemotes);

			Network::Channel channel{0};
			BoundObjectTest::ClientToServerData clientToServerData{false, true, 133.7f, 9001};
			const bool wasSent =
				localClients[0].SendMessageToAllRemotes<&BoundObjectTest::ClientToAllRemotes, BoundObjectTest::ClientToServerData>(
					hostBoundObjectIdentifier,
					channel,
					clientToServerData
				);
			EXPECT_TRUE(wasSent);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundServerObject = boundObjects[0], &boundOtherClientObject = boundObjects[2]]()
				{
					return !boundServerObject.m_receivedClientToAllRemotes || !boundOtherClientObject.m_receivedClientToAllRemotes;
				}
			);

			EXPECT_TRUE(boundObjects[0].m_receivedClientToAllRemotes);
			EXPECT_FALSE(boundObjects[1].m_receivedClientToAllRemotes);
			EXPECT_TRUE(boundObjects[2].m_receivedClientToAllRemotes);
		}

		{
			Network::Channel channel{0};
			BoundObjectTest::ServerToClientData serverToClientData{true, false, 133.7f, 9001};
			const bool wasSent =
				host.BroadcastMessageToAllClients<&BoundObjectTest::ServerToClient>(hostBoundObjectIdentifier, channel, serverToClientData);
			EXPECT_TRUE(wasSent);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundClientObject1 = boundObjects[1], &boundClientObject2 = boundObjects[2]]()
				{
					return !boundClientObject1.m_receivedServerToClient || !boundClientObject2.m_receivedServerToClient;
				}
			);

			EXPECT_FALSE(boundObjects[0].m_receivedServerToClient);
			EXPECT_TRUE(boundObjects[1].m_receivedServerToClient);
			EXPECT_TRUE(boundObjects[2].m_receivedServerToClient);
		}

		for (LocalClient& localClient : localClients)
		{
			Threading::Atomic<bool> hasDisconnected{false};

			localClient.OnDisconnected.Add(
				localClient,
				[&hasDisconnected](LocalClient&)
				{
					hasDisconnected = true;
				}
			);

			localClient.Disconnect();

			// Run the main thread job runner until we finished disconnecting
			RunMainThreadJobRunner(
				[&hasDisconnected, &localClient, &host]()
				{
					return !hasDisconnected || localClient.IsQueuedOrExecuting() || host.IsQueuedOrExecuting();
				}
			);

			// Perform one last tick for the disconnect to finish
			engine.DoTick();
		}
	}

	FEATURE_TEST(Networking, BindObjectAndPropagatePropertyStandaloneServer)
	{
		Engine& engine = GetEngine();

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

		if (!System::Get<Reflection::Registry>().FindTypeInterface(Reflection::GetTypeGuid<BoundObjectTest>()).IsValid())
		{
			System::Get<Reflection::Registry>().RegisterDynamicType<BoundObjectTest>();
		}

		[[maybe_unused]] Manager& networkManager = *System::FindPlugin<Manager>();

		Network::LocalHost host;
		constexpr uint8 maximumClientCount = 2;
		const uint8 maximumChannelCount = 2;
		const uint32 incomingBandwidth = 0;
		const uint32 outgoingBandwidth = 0;
		host.Start(
			Network::AnyIPAddress,
			maximumClientCount,
			maximumChannelCount,
			incomingBandwidth,
			outgoingBandwidth,
			Network::LocalPeer::UpdateMode::EngineTick
		);

		constexpr Guid boundObjectGuid = "0952ad47-492b-4db7-a519-76af8b79725e"_guid;

		Array<Tests::Network::BoundObjectTest, maximumClientCount + 1> boundObjects;

		// Bind the server object
		const BoundObjectIdentifier hostBoundObjectIdentifier =
			host.BindObject(boundObjectGuid, AnyView((Reflection::PropertyOwner&)boundObjects[0]));
		EXPECT_TRUE(hostBoundObjectIdentifier.IsValid());

		host.OnClientConnected.Add(
			host,
			[](LocalHost&, const ClientIdentifier, RemotePeerView, const BoundObjectIdentifier)
			{
			}
		);
		host.OnClientDisconnected.Add(
			host,
			[](LocalHost&, const ClientIdentifier)
			{
			}
		);

		Array<LocalClient, maximumClientCount> localClients;
		for (LocalClient& localClient : localClients)
		{
			localClient.Start();
		}

		for (LocalClient& localClient : localClients)
		{
			const uint8 clientIndex = localClients.GetIteratorIndex(&localClient);
			Threading::Atomic<bool> hasConnected{false};

			localClient.OnConnected.Add(
				localClient,
				[&hasConnected](LocalClient&, Network::RemoteHost, const BoundObjectIdentifier)
				{
					hasConnected = true;
				}
			);

			const uint32 connectionUserData = 0;
			Network::RemoteHost remoteHost = localClient.Connect(
				Address(IO::URI(MAKE_URI("localhost"))),
				maximumChannelCount,
				connectionUserData,
				Network::LocalPeer::UpdateMode::EngineTick
			);
			EXPECT_TRUE(remoteHost.IsValid());

			BoundObjectIdentifier clientBoundObjectIdentifier;
			// Start binding the client's equivalent of the object
			localClient.BindObject(
				boundObjectGuid,
				AnyView((Reflection::PropertyOwner&)boundObjects[1 + clientIndex]),
				[hostBoundObjectIdentifier, &clientBoundObjectIdentifier](const BoundObjectIdentifier boundObjectIdentifier)
				{
					EXPECT_FALSE(clientBoundObjectIdentifier.IsValid());
					EXPECT_EQ(boundObjectIdentifier, hostBoundObjectIdentifier);
					clientBoundObjectIdentifier = boundObjectIdentifier;
				}
			);

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&hasConnected, &clientBoundObjectIdentifier]()
				{
					return !hasConnected || !clientBoundObjectIdentifier.IsValid();
				}
			);
		}

		// Make sure that non-authorative clients can't propagate properties
		for (LocalClient& localClient : localClients)
		{
			const bool wasInvalidated =
				localClient.InvalidateProperties<&BoundObjectTest::m_propagatedClientToServerFlags>(hostBoundObjectIdentifier);
			EXPECT_FALSE(wasInvalidated);
		}

		// Now test property propagation from server to client
		{
			EXPECT_TRUE(boundObjects[0].m_propagatedServerToClientFlags.AreNoneSet());
			EXPECT_TRUE(boundObjects[1].m_propagatedServerToClientFlags.AreNoneSet());
			EXPECT_TRUE(boundObjects[2].m_propagatedServerToClientFlags.AreNoneSet());
			boundObjects[0].m_propagatedServerToClientFlags.Set(BoundObjectTest::Flags::Sixteen);

			EXPECT_FALSE(host.HasPendingPropagatedPropertiesToSend());
			EXPECT_FALSE(localClients[0].HasPendingPropagatedPropertiesToSend());
			EXPECT_FALSE(localClients[1].HasPendingPropagatedPropertiesToSend());

			const bool wasInvalidated =
				host.InvalidatePropertiesToAllClients<&BoundObjectTest::m_propagatedServerToClientFlags>(hostBoundObjectIdentifier);
			EXPECT_TRUE(wasInvalidated);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundClientObject1 = boundObjects[1], &boundClientObject2 = boundObjects[2]]()
				{
					return boundClientObject1.m_propagatedServerToClientFlags.IsNotSet(BoundObjectTest::Flags::Sixteen) ||
				         boundClientObject2.m_propagatedServerToClientFlags.IsNotSet(BoundObjectTest::Flags::Sixteen);
				}
			);
			EXPECT_EQ(boundObjects[0].m_propagatedServerToClientFlags, BoundObjectTest::Flags::Sixteen);
			EXPECT_EQ(boundObjects[1].m_propagatedServerToClientFlags, BoundObjectTest::Flags::Sixteen);
			EXPECT_EQ(boundObjects[2].m_propagatedServerToClientFlags, BoundObjectTest::Flags::Sixteen);

			// Wait for all propagated properties to confirm
			RunMainThreadJobRunner(
				[&host]()
				{
					return host.HasPendingPropagatedPropertiesToSend();
				}
			);

			EXPECT_FALSE(host.HasPendingPropagatedPropertiesToSend());
			EXPECT_FALSE(localClients[0].HasPendingPropagatedPropertiesToSend());
			EXPECT_FALSE(localClients[1].HasPendingPropagatedPropertiesToSend());
		}

		// Test delegating authority to the object
		{
			EXPECT_TRUE(host.HasAuthorityOfBoundObject(hostBoundObjectIdentifier));
			EXPECT_FALSE(localClients[0].HasAuthorityOfBoundObject(hostBoundObjectIdentifier));
			EXPECT_FALSE(localClients[1].HasAuthorityOfBoundObject(hostBoundObjectIdentifier));

			host.DelegateBoundObjectAuthority(hostBoundObjectIdentifier, localClients[0].GetIdentifier());

			// Run the main thread job runner until we received authority
			RunMainThreadJobRunner(
				[&localClient = localClients[0], hostBoundObjectIdentifier]()
				{
					return !localClient.HasAuthorityOfBoundObject(hostBoundObjectIdentifier);
				}
			);

			EXPECT_FALSE(host.HasAuthorityOfBoundObject(hostBoundObjectIdentifier));
			EXPECT_TRUE(localClients[0].HasAuthorityOfBoundObject(hostBoundObjectIdentifier));
			EXPECT_FALSE(localClients[1].HasAuthorityOfBoundObject(hostBoundObjectIdentifier));
		}

		// Make sure that the newly non-authorative host & other client can't propagate properties
		{
			EXPECT_FALSE(host.InvalidatePropertiesToAllClients<&BoundObjectTest::m_propagatedClientToServerFlags>(hostBoundObjectIdentifier));
			EXPECT_FALSE(host.InvalidatePropertiesToClient<&BoundObjectTest::m_propagatedClientToServerFlags>(
				localClients[0].GetIdentifier(),
				hostBoundObjectIdentifier
			));
			EXPECT_FALSE(host.InvalidatePropertiesToClient<&BoundObjectTest::m_propagatedClientToServerFlags>(
				localClients[1].GetIdentifier(),
				hostBoundObjectIdentifier
			));
			EXPECT_FALSE(localClients[1].InvalidateProperties<&BoundObjectTest::m_propagatedClientToServerFlags>(hostBoundObjectIdentifier));
		}

		// Now test property propagation from client to server
		{
			EXPECT_TRUE(boundObjects[0].m_propagatedClientToServerFlags.AreNoneSet());
			EXPECT_TRUE(boundObjects[1].m_propagatedClientToServerFlags.AreNoneSet());
			EXPECT_TRUE(boundObjects[2].m_propagatedClientToServerFlags.AreNoneSet());
			boundObjects[1].m_propagatedClientToServerFlags.Set(BoundObjectTest::Flags::Two);
			boundObjects[1].m_propagatedClientToServerFlags.Set(BoundObjectTest::Flags::ThirtyTwo);

			EXPECT_FALSE(host.HasPendingPropagatedPropertiesToSend());
			EXPECT_FALSE(localClients[0].HasPendingPropagatedPropertiesToSend());
			EXPECT_FALSE(localClients[1].HasPendingPropagatedPropertiesToSend());

			const bool wasInvalidated =
				localClients[0].InvalidateProperties<&BoundObjectTest::m_propagatedClientToServerFlags>(hostBoundObjectIdentifier);
			EXPECT_TRUE(wasInvalidated);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundServerObject = boundObjects[0]]()
				{
					return !boundServerObject.m_propagatedClientToServerFlags.AreAllSet(
						BoundObjectTest::Flags::Two | BoundObjectTest::Flags::ThirtyTwo
					);
				}
			);
			EXPECT_EQ(boundObjects[0].m_propagatedClientToServerFlags, BoundObjectTest::Flags::Two | BoundObjectTest::Flags::ThirtyTwo);
			EXPECT_EQ(boundObjects[1].m_propagatedClientToServerFlags, BoundObjectTest::Flags::Two | BoundObjectTest::Flags::ThirtyTwo);
			EXPECT_TRUE(boundObjects[2].m_propagatedClientToServerFlags.AreNoneSet());

			// Wait for all propagated properties to confirm
			RunMainThreadJobRunner(
				[&localClient = localClients[0]]()
				{
					return localClient.HasPendingPropagatedPropertiesToSend();
				}
			);

			EXPECT_FALSE(host.HasPendingPropagatedPropertiesToSend());
			EXPECT_FALSE(localClients[0].HasPendingPropagatedPropertiesToSend());
			EXPECT_FALSE(localClients[1].HasPendingPropagatedPropertiesToSend());
		}

		// Now test property propagation from client to all remotes
		{
			EXPECT_TRUE(boundObjects[0].m_propagatedClientToAllRemotesFlags.AreNoneSet());
			EXPECT_TRUE(boundObjects[1].m_propagatedClientToAllRemotesFlags.AreNoneSet());
			EXPECT_TRUE(boundObjects[2].m_propagatedClientToAllRemotesFlags.AreNoneSet());
			boundObjects[1].m_propagatedClientToAllRemotesFlags.Set(BoundObjectTest::Flags::Two);
			boundObjects[1].m_propagatedClientToAllRemotesFlags.Set(BoundObjectTest::Flags::ThirtyTwo);

			EXPECT_FALSE(host.HasPendingPropagatedPropertiesToSend());
			EXPECT_FALSE(localClients[0].HasPendingPropagatedPropertiesToSend());
			EXPECT_FALSE(localClients[1].HasPendingPropagatedPropertiesToSend());

			const bool wasInvalidated =
				localClients[0].InvalidateProperties<&BoundObjectTest::m_propagatedClientToAllRemotesFlags>(hostBoundObjectIdentifier);
			EXPECT_TRUE(wasInvalidated);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundServerObject = boundObjects[0], &boundOtherClientObject = boundObjects[2]]()
				{
					return !boundServerObject.m_propagatedClientToAllRemotesFlags.AreAllSet(
									 BoundObjectTest::Flags::Two | BoundObjectTest::Flags::ThirtyTwo
								 ) ||
				         !boundOtherClientObject.m_propagatedClientToAllRemotesFlags.AreAllSet(
									 BoundObjectTest::Flags::Two | BoundObjectTest::Flags::ThirtyTwo
								 );
				}
			);
			EXPECT_EQ(boundObjects[0].m_propagatedClientToAllRemotesFlags, BoundObjectTest::Flags::Two | BoundObjectTest::Flags::ThirtyTwo);
			EXPECT_EQ(boundObjects[1].m_propagatedClientToAllRemotesFlags, BoundObjectTest::Flags::Two | BoundObjectTest::Flags::ThirtyTwo);
			EXPECT_EQ(boundObjects[2].m_propagatedClientToAllRemotesFlags, BoundObjectTest::Flags::Two | BoundObjectTest::Flags::ThirtyTwo);

			// Wait for all propagated properties to confirm
			RunMainThreadJobRunner(
				[&host, &localClient = localClients[0]]()
				{
					return localClient.HasPendingPropagatedPropertiesToSend() || host.HasPendingPropagatedPropertiesToSend();
				}
			);

			EXPECT_FALSE(host.HasPendingPropagatedPropertiesToSend());
			EXPECT_FALSE(localClients[0].HasPendingPropagatedPropertiesToSend());
			EXPECT_FALSE(localClients[1].HasPendingPropagatedPropertiesToSend());
		}

		for (LocalClient& localClient : localClients)
		{
			Threading::Atomic<bool> hasDisconnected{false};

			localClient.OnDisconnected.Add(
				localClient,
				[&hasDisconnected](LocalClient&)
				{
					hasDisconnected = true;
				}
			);

			localClient.Disconnect();

			// Run the main thread job runner until we finished disconnecting
			RunMainThreadJobRunner(
				[&hasDisconnected, &localClient]()
				{
					return !hasDisconnected || localClient.IsQueuedOrExecuting();
				}
			);

			// Perform one last tick for the disconnect to finish
			engine.DoTick();
		}

		host.Stop();
		// Run the main thread job runner until the host is ready to shut down
		RunMainThreadJobRunner(
			[&host]()
			{
				return host.IsQueuedOrExecuting();
			}
		);
	}
}
