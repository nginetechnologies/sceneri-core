#include <Common/Memory/New.h>

#include <Engine/Tests/FeatureTest.h>

#include <NetworkingCore/Plugin.h>
#include <NetworkingCore/Components/BoundComponent.h>
#include <NetworkingCore/Components/HostComponent.h>
#include <NetworkingCore/Components/LocalHostComponent.h>
#include <NetworkingCore/Components/ClientComponent.h>
#include <NetworkingCore/Components/LocalClientComponent.h>
#include <NetworkingCore/Components/ModuleComponent.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentValue.inl>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/RootComponent.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <Common/System/Query.h>
#include <Common/Threading/AtomicEnum.h>
#include <Common/Math/Radius.h>
#include <Common/Network/Address.h>

#include <Common/Project System/PluginDatabase.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Tests::Network
{
	using namespace ngine::Network;

	struct SessionModule final : public Network::Session::Module
	{
		using InstanceIdentifier = TIdentifier<uint8, 4>;

		using BaseType = Module;
		using BaseType::BaseType;

		SessionModule(DynamicInitializer&&)
		{
			EXPECT_FALSE(m_hasConnectedToRemoteServer);
			EXPECT_FALSE(m_hasConnectedToRemoteClient);
			m_hasConnectedToRemoteServer = true;
		}

		virtual void OnRemoteClientConnectedToHost(Network::Session::Host&, Network::Session::LocalHost&, Network::ClientIdentifier) override
		{
			EXPECT_FALSE(m_hasConnectedToRemoteClient);
			EXPECT_FALSE(m_hasConnectedToRemoteServer);
			m_hasConnectedToRemoteClient = true;
		}

		virtual void
		OnRemoteClientDisconnectedFromHost(Network::Session::Host&, Network::Session::LocalHost&, Network::ClientIdentifier) override
		{
			EXPECT_TRUE(m_hasConnectedToRemoteClient);
			EXPECT_FALSE(m_hasConnectedToRemoteServer);
			m_hasConnectedToRemoteClient = false;
		}

		virtual void OnLocalClientDisconnectedFromHost(Network::Session::Client&, Network::Session::LocalClient&) override
		{
			EXPECT_TRUE(m_hasConnectedToRemoteServer);
			EXPECT_FALSE(m_hasConnectedToRemoteClient);
			m_hasConnectedToRemoteServer = false;
		}

		bool m_hasConnectedToRemoteServer{false};
		bool m_hasConnectedToRemoteClient{false};
	};

	[[maybe_unused]] const bool wasLobbyComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<SessionModule>>::Make());
	[[maybe_unused]] const bool wasLobbyTypeRegistered = Reflection::Registry::RegisterType<SessionModule>();
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::Network::SessionModule>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::SessionModule>(
			"{A006B58E-0DCF-464A-B791-56AF6A2E757D}"_guid,
			MAKE_UNICODE_LITERAL("Test Lobby Module"),
			Reflection::TypeFlags::DisableDynamicDeserialization | Reflection::TypeFlags::DisableDynamicCloning
		);
	};
}

namespace ngine::Tests::Network
{
	using namespace ngine::Network;
	FEATURE_TEST(Networking, BindNetworkedComponentStandaloneServer)
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

		// Keep two scenes, for the host and another for the client to simulate two machines
		Array<Entity::SceneRegistry, 2> sceneRegistries;

		Array<ReferenceWrapper<Entity::RootComponent>, 2> rootComponents{
			*sceneRegistries[0].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[0]),
			*sceneRegistries[1].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[1])
		};

		constexpr uint8 maximumClientCount = 1;

		Session::Host& host = *sceneRegistries[0].GetOrCreateComponentTypeData<Session::Host>()->CreateInstance(
			Session::Host::Initializer{Entity::HierarchyComponentBase::DynamicInitializer{*rootComponents[0], sceneRegistries[0]}}
		);
		EXPECT_EQ(rootComponents[0]->FindFirstChildOfType<Session::Host>(), &host);
		EXPECT_EQ(host.GetParentSafe(), &rootComponents[0]);

		Session::LocalHost& localHost = *host.CreateDataComponent<Session::LocalHost>(
			sceneRegistries[0],
			Session::LocalHost::Initializer{
				Entity::Data::HierarchyComponent::DynamicInitializer{host, sceneRegistries[0]},
				Network::AnyIPAddress,
				maximumClientCount,
				Network::LocalPeer::UpdateMode::EngineTick
			}
		);
		EXPECT_TRUE(localHost.GetLocalHost().IsValid());
		if (!localHost.GetLocalHost().IsValid())
		{
			return;
		}

		Session::Client& client = *sceneRegistries[1].GetOrCreateComponentTypeData<Session::Client>()->CreateInstance(
			Session::Client::Initializer{*rootComponents[1], sceneRegistries[1]}
		);
		EXPECT_EQ(rootComponents[1]->FindFirstChildOfType<Session::Client>(), &client);
		EXPECT_EQ(client.GetParentSafe(), &rootComponents[1]);

		[[maybe_unused]] Session::LocalClient& localClientComponent = *client.CreateDataComponent<Session::LocalClient>(
			sceneRegistries[1],
			Session::LocalClient::Initializer{Entity::Data::HierarchyComponent::DynamicInitializer{client, sceneRegistries[1]}}
		);

		Array<Optional<SessionModule*>, 2> sessionModules{host.CreateDataComponent<SessionModule>(sceneRegistries[0]), Invalid};

		Array<UniquePtr<Scene>, 2> scenes{
			UniquePtr<Scene>::Make(
				sceneRegistries[0],
				*rootComponents[0],
				1024_meters,
				"5ebf8a78-c5fa-4e6c-81be-d35a788ab1b4"_guid,
				Scene::Flags::IsDisabled
			),
			UniquePtr<Scene>::Make(sceneRegistries[1], client, 1024_meters, "6e0fe654-8eb2-4db0-9ee7-fd39451b37b3"_guid, Scene::Flags::IsDisabled)
		};

		constexpr Guid componentInstanceGuid = "dc2ebe03-faca-4f91-b722-89b39c376580"_guid;

		Array<Entity::ComponentValue<Entity::Component3D>, 2> components{
			Entity::ComponentValue<Entity::Component3D>{
				sceneRegistries[0],
				Entity::Component3D::Initializer{
					scenes[0]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			},
			Entity::ComponentValue<Entity::Component3D>{
				sceneRegistries[1],
				Entity::Component3D::Initializer{
					scenes[1]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			}
		};
		EXPECT_TRUE(components[0].IsValid());
		EXPECT_TRUE(components[1].IsValid());

		// Create and bind the server component
		Array<Optional<Session::BoundComponent*>, 2> boundComponents{
			*components[0]->CreateDataComponent<Session::BoundComponent>(Session::BoundComponent::Initializer{*components[0], sceneRegistries[0]}
		  ),
			Invalid
		};
		EXPECT_TRUE(components[0].IsValid());

		{
			EXPECT_FALSE(sessionModules[0]->m_hasConnectedToRemoteServer);
			EXPECT_FALSE(sessionModules[0]->m_hasConnectedToRemoteClient);
			EXPECT_FALSE(sessionModules[1].IsValid());
			EXPECT_FALSE(client.HasDataComponentOfType<SessionModule>(sceneRegistries[1]));

			const uint8 maximumChannelCount = 2;
			const uint32 connectionUserData = 0;
			const bool startedConnecting = localClientComponent.Connect(
				client,
				Address(IO::URI(MAKE_URI("localhost"))),
				maximumChannelCount,
				connectionUserData,
				Network::LocalPeer::UpdateMode::EngineTick
			);
			EXPECT_TRUE(startedConnecting);
			if (!startedConnecting)
			{
				return;
			}

			// Start binding the client's equivalent of this component
			boundComponents[1] = *components[1]->CreateDataComponent<Session::BoundComponent>(
				Session::BoundComponent::Initializer{*components[1], sceneRegistries[1]}
			);

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&localClientComponent, &networkComponent = *boundComponents[1]]()
				{
					return !localClientComponent.IsConnected() || !networkComponent.IsBound();
				}
			);

			EXPECT_FALSE(sessionModules[0]->m_hasConnectedToRemoteServer);
			EXPECT_TRUE(sessionModules[0]->m_hasConnectedToRemoteClient);

			sessionModules[1] = client.FindDataComponentOfType<SessionModule>(sceneRegistries[1]);
			EXPECT_TRUE(sessionModules[1].IsValid());

			// EXPECT_FALSE(sessionModules[1]->m_hasConnectedToRemoteServer);
			// EXPECT_FALSE(sessionModules[1]->m_hasConnectedToRemoteClient);
			EXPECT_TRUE(sessionModules[1]->m_hasConnectedToRemoteServer);
			EXPECT_FALSE(sessionModules[1]->m_hasConnectedToRemoteClient);
		}

		{
			localClientComponent.Disconnect(client);

			// Run the main thread job runner until we finished disconnecting
			RunMainThreadJobRunner(
				[&localClientComponent]()
				{
					return localClientComponent.IsConnected();
				}
			);

			// Perform one last tick for the disconnect to finish
			engine.DoTick();
		}

		EXPECT_FALSE(sessionModules[0]->m_hasConnectedToRemoteServer);
		EXPECT_FALSE(sessionModules[0]->m_hasConnectedToRemoteClient);
		EXPECT_FALSE(sessionModules[1]->m_hasConnectedToRemoteServer);
		EXPECT_FALSE(sessionModules[1]->m_hasConnectedToRemoteClient);
	}

	struct MessageData
	{
		int i{1337};
		float f{9001.f};
	};

	struct Bound3DComponent final : public Entity::Component3D
	{
		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 1>;

		using BaseType::BaseType;

		void ServerToClient(Network::Session::BoundComponent& boundComponent, Network::LocalClient& localClient, const MessageData message)
		{
			EXPECT_FALSE(m_receivedServerToClient);
			m_receivedServerToClient = true;

			EXPECT_EQ(message.i, 4321);
			EXPECT_EQ(message.f, 4.321f);

			const Network::Channel channel{0};
			boundComponent.SendMessageToHost<&Bound3DComponent::ClientToServer>(localClient, channel, MessageData{9001, 1.337f});
		}
		void ClientToServer(Network::Session::BoundComponent&, Network::LocalHost&, const Network::ClientIdentifier, const MessageData message)
		{
			EXPECT_FALSE(m_receivedClientToServer);
			m_receivedClientToServer = true;

			EXPECT_EQ(message.i, 9001);
			EXPECT_EQ(message.f, 1.337f);
		}

		bool m_receivedServerToClient{false};
		bool m_receivedClientToServer{false};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::Network::Bound3DComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::Bound3DComponent>(
			"102D570C-872C-4420-84A9-4DCF198CC4D9"_guid,
			MAKE_UNICODE_LITERAL("Bound 3D Component"),
			TypeFlags(),
			Tags{},
			Properties{},
			Functions{
				Function{
					"{47A5B923-08DB-40C6-A35E-1A1F01B748C5}"_guid,
					MAKE_UNICODE_LITERAL("Server to Client"),
					&Tests::Network::Bound3DComponent::ServerToClient,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("message")}
				},
				Function{
					"{C402505D-CB6D-4640-BC19-1035E8F6062B}"_guid,
					MAKE_UNICODE_LITERAL("Client to Server"),
					&Tests::Network::Bound3DComponent::ClientToServer,
					FunctionFlags::ClientToHost,
					Reflection::ReturnType{},
					Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("localHost")},
					Reflection::Argument{"1C382ED7-FE03-4E4F-AC2B-8DDEF8B4FB3B"_guid, MAKE_UNICODE_LITERAL("clientIdentifier")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("message")}
				}
			}
		);
	};
}

namespace ngine::Tests::Network
{
	[[maybe_unused]] const bool wasBound3DComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Bound3DComponent>>::Make());
	[[maybe_unused]] const bool wasBound3DComponentTypeRegistered = Reflection::Registry::RegisterType<Bound3DComponent>();

	FEATURE_TEST(Networking, BindNetworkedComponentAndMessageStandaloneServer)
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

		// Keep two scenes, for the host and another for the client to simulate two machines
		Array<Entity::SceneRegistry, 2> sceneRegistries;

		Array<ReferenceWrapper<Entity::RootComponent>, 2> rootComponents{
			*sceneRegistries[0].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[0]),
			*sceneRegistries[1].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[1])
		};

		constexpr uint8 maximumClientCount = 1;

		Session::Host& host = *sceneRegistries[0].GetOrCreateComponentTypeData<Session::Host>()->CreateInstance(
			Session::Host::Initializer{Entity::HierarchyComponentBase::DynamicInitializer{*rootComponents[0], sceneRegistries[0]}}
		);
		Session::LocalHost& localHost = *host.CreateDataComponent<Session::LocalHost>(
			sceneRegistries[0],
			Session::LocalHost::Initializer{
				Entity::Data::HierarchyComponent::DynamicInitializer{host, sceneRegistries[0]},
				Network::AnyIPAddress,
				maximumClientCount,
				Network::LocalPeer::UpdateMode::EngineTick
			}
		);
		EXPECT_TRUE(localHost.GetLocalHost().IsValid());
		if (!localHost.GetLocalHost().IsValid())
		{
			return;
		}

		Session::Client& client = *sceneRegistries[1].GetOrCreateComponentTypeData<Session::Client>()->CreateInstance(
			Session::Client::Initializer{*rootComponents[1], sceneRegistries[1]}
		);
		[[maybe_unused]] Session::LocalClient& localClientComponent = *client.CreateDataComponent<Session::LocalClient>(
			sceneRegistries[1],
			Session::LocalClient::Initializer{Entity::Data::HierarchyComponent::DynamicInitializer{client, sceneRegistries[1]}}
		);

		Array<UniquePtr<Scene>, 2> scenes{
			UniquePtr<Scene>::Make(
				sceneRegistries[0],
				*rootComponents[0],
				1024_meters,
				"5ebf8a78-c5fa-4e6c-81be-d35a788ab1b4"_guid,
				Scene::Flags::IsDisabled
			),
			UniquePtr<Scene>::Make(sceneRegistries[1], client, 1024_meters, "6e0fe654-8eb2-4db0-9ee7-fd39451b37b3"_guid, Scene::Flags::IsDisabled)
		};

		constexpr Guid componentInstanceGuid = "dc2ebe03-faca-4f91-b722-89b39c376580"_guid;

		Array<Entity::ComponentValue<Bound3DComponent>, 2> components{
			Entity::ComponentValue<Bound3DComponent>{
				sceneRegistries[0],
				Bound3DComponent::Initializer{
					scenes[0]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			},
			Entity::ComponentValue<Bound3DComponent>{
				sceneRegistries[1],
				Bound3DComponent::Initializer{
					scenes[1]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			}
		};
		EXPECT_TRUE(components[0].IsValid());
		EXPECT_TRUE(components[1].IsValid());

		// Create and bind the server component
		Array<Optional<Session::BoundComponent*>, 2> boundComponents{
			*components[0]->CreateDataComponent<Session::BoundComponent>(Session::BoundComponent::Initializer{*components[0], sceneRegistries[0]}
		  ),
			Invalid
		};
		EXPECT_TRUE(components[0].IsValid());

		{
			const uint8 maximumChannelCount = 2;
			const uint32 connectionUserData = 0;
			const bool startedConnecting = localClientComponent.Connect(
				client,
				Address(IO::URI(MAKE_URI("localhost"))),
				maximumChannelCount,
				connectionUserData,
				Network::LocalPeer::UpdateMode::EngineTick
			);
			EXPECT_TRUE(startedConnecting);
			if (!startedConnecting)
			{
				return;
			}

			// Start binding the client's equivalent of this component
			boundComponents[1] = *components[1]->CreateDataComponent<Session::BoundComponent>(
				Session::BoundComponent::Initializer{*components[1], sceneRegistries[1]}
			);

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&localClientComponent, &networkComponent = *boundComponents[1]]()
				{
					return !localClientComponent.IsConnected() || !networkComponent.IsBound();
				}
			);
		}

		// Test delegating authority
		{
			EXPECT_TRUE(boundComponents[0]->HasAuthority(*components[0], sceneRegistries[0]));
			EXPECT_FALSE(boundComponents[1]->HasAuthority(*components[1], sceneRegistries[1]));
			EXPECT_TRUE(boundComponents[0]->DelegateAuthority(*components[0], sceneRegistries[0], localClientComponent.GetIdentifier()));

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&networkComponent = *boundComponents[1], &owner = *components[1], &sceneRegistry = sceneRegistries[1]]()
				{
					return !networkComponent.HasAuthority(owner, sceneRegistry);
				}
			);

			EXPECT_TRUE(boundComponents[1]->HasAuthority(*components[1], sceneRegistries[1]));
			EXPECT_FALSE(boundComponents[0]->HasAuthority(*components[0], sceneRegistries[0]));
		}

		EXPECT_FALSE(components[0]->m_receivedClientToServer);
		EXPECT_FALSE(components[0]->m_receivedServerToClient);
		EXPECT_FALSE(components[1]->m_receivedClientToServer);
		EXPECT_FALSE(components[1]->m_receivedServerToClient);

		{
			Channel channel{0};
			const bool wasSent = boundComponents[0]->BroadcastToAllClients<&Bound3DComponent::ServerToClient>(
				*components[0],
				sceneRegistries[0],
				channel,
				MessageData{4321, 4.321f}
			);
			EXPECT_TRUE(wasSent);
		}

		// Wait for client to receive the message
		RunMainThreadJobRunner(
			[&clientComponent = *components[1], &serverComponent = *components[0]]()
			{
				return !clientComponent.m_receivedServerToClient && !serverComponent.m_receivedClientToServer;
			}
		);

		{
			localClientComponent.Disconnect(client);

			// Run the main thread job runner until we finished disconnecting
			RunMainThreadJobRunner(
				[&localClientComponent]()
				{
					return localClientComponent.IsConnected();
				}
			);

			// Perform one last tick for the disconnect to finish
			engine.DoTick();
		}
	}

	struct BoundDataComponent final : public Entity::Data::Component
	{
		using BaseType = Component;
		using InstanceIdentifier = TIdentifier<uint32, 1>;

		using BaseType::BaseType;

		void ServerToClient(
			Entity::HierarchyComponentBase& owner,
			Network::Session::BoundComponent& boundComponent,
			Network::LocalClient& localClient,
			const MessageData message
		)
		{
			EXPECT_EQ(owner.FindDataComponentOfType<BoundDataComponent>(owner.GetSceneRegistry()), this);
			EXPECT_FALSE(m_receivedServerToClient);
			m_receivedServerToClient = true;

			EXPECT_EQ(message.i, 4321);
			EXPECT_EQ(message.f, 4.321f);

			const Network::Channel channel{0};
			boundComponent.SendMessageToHost<&BoundDataComponent::ClientToServer>(localClient, channel, MessageData{9001, 1.337f});
		}
		void ClientToServer(
			Entity::HierarchyComponentBase& owner,
			Network::Session::BoundComponent&,
			Network::LocalHost&,
			const Network::ClientIdentifier,
			const MessageData message
		)
		{
			EXPECT_EQ(owner.FindDataComponentOfType<BoundDataComponent>(owner.GetSceneRegistry()), this);
			EXPECT_FALSE(m_receivedClientToServer);
			m_receivedClientToServer = true;

			EXPECT_EQ(message.i, 9001);
			EXPECT_EQ(message.f, 1.337f);
		}

		bool m_receivedServerToClient{false};
		bool m_receivedClientToServer{false};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::Network::BoundDataComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::BoundDataComponent>(
			"{40F8BEE2-B98C-4A47-BAB4-713D088C2912}"_guid,
			MAKE_UNICODE_LITERAL("Bound Data Component"),
			TypeFlags(),
			Tags{},
			Properties{},
			Functions{
				Function{
					"{D620085B-DEF8-4008-BDAD-4B69EEFDAE13}"_guid,
					MAKE_UNICODE_LITERAL("Server to Client"),
					&Tests::Network::BoundDataComponent::ServerToClient,
					FunctionFlags::HostToClient,
					Reflection::ReturnType{},
					Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("owner")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("localClient")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("message")}
				},
				Function{
					"{A427ADD1-526C-4703-B6E4-7BEB8EB09AAE}"_guid,
					MAKE_UNICODE_LITERAL("Client to Server"),
					&Tests::Network::BoundDataComponent::ClientToServer,
					FunctionFlags::ClientToHost,
					Reflection::ReturnType{},
					Reflection::Argument{"7b013251-7b9f-4a06-8333-cac15431117d"_guid, MAKE_UNICODE_LITERAL("owner")},
					Reflection::Argument{"ec306a54-3f68-49af-94f3-bffe806b20f3"_guid, MAKE_UNICODE_LITERAL("boundComponent")},
					Reflection::Argument{"1C382ED7-FE03-4E4F-AC2B-8DDEF8B4FB3B"_guid, MAKE_UNICODE_LITERAL("localHost")},
					Reflection::Argument{"bcde10ae-0ba6-4b0c-8dec-1f2f44e4c082"_guid, MAKE_UNICODE_LITERAL("clientIdentifier")},
					Reflection::Argument{"9e162937-d204-48ea-a6e6-106d0b5d3cac"_guid, MAKE_UNICODE_LITERAL("message")}
				}
			}
		);
	};
}

namespace ngine::Tests::Network
{
	[[maybe_unused]] const bool wasBoundDataComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<BoundDataComponent>>::Make());
	[[maybe_unused]] const bool wasBoundDataComponentTypeRegistered = Reflection::Registry::RegisterType<BoundDataComponent>();

	FEATURE_TEST(Networking, BindNetworkedDataComponentAndMessageStandaloneServer)
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

		// Keep two scenes, for the host and another for the client to simulate two machines
		Array<Entity::SceneRegistry, 2> sceneRegistries;

		Array<ReferenceWrapper<Entity::RootComponent>, 2> rootComponents{
			*sceneRegistries[0].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[0]),
			*sceneRegistries[1].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[1])
		};

		constexpr uint8 maximumClientCount = 1;

		Session::Host& host = *sceneRegistries[0].GetOrCreateComponentTypeData<Session::Host>()->CreateInstance(
			Session::Host::Initializer{Entity::HierarchyComponentBase::DynamicInitializer{*rootComponents[0], sceneRegistries[0]}}
		);
		Session::LocalHost& localHost = *host.CreateDataComponent<Session::LocalHost>(
			sceneRegistries[0],
			Session::LocalHost::Initializer{
				Entity::Data::HierarchyComponent::DynamicInitializer{host, sceneRegistries[0]},
				Network::AnyIPAddress,
				maximumClientCount,
				Network::LocalPeer::UpdateMode::EngineTick
			}
		);
		EXPECT_TRUE(localHost.GetLocalHost().IsValid());
		if (!localHost.GetLocalHost().IsValid())
		{
			return;
		}

		Session::Client& client = *sceneRegistries[1].GetOrCreateComponentTypeData<Session::Client>()->CreateInstance(
			Session::Client::Initializer{*rootComponents[1], sceneRegistries[1]}
		);
		[[maybe_unused]] Session::LocalClient& localClientComponent = *client.CreateDataComponent<Session::LocalClient>(
			sceneRegistries[1],
			Session::LocalClient::Initializer{Entity::Data::HierarchyComponent::DynamicInitializer{client, sceneRegistries[1]}}
		);

		Array<UniquePtr<Scene>, 2> scenes{
			UniquePtr<Scene>::Make(
				sceneRegistries[0],
				*rootComponents[0],
				1024_meters,
				"5ebf8a78-c5fa-4e6c-81be-d35a788ab1b4"_guid,
				Scene::Flags::IsDisabled
			),
			UniquePtr<Scene>::Make(sceneRegistries[1], client, 1024_meters, "6e0fe654-8eb2-4db0-9ee7-fd39451b37b3"_guid, Scene::Flags::IsDisabled)
		};

		constexpr Guid componentInstanceGuid = "dc2ebe03-faca-4f91-b722-89b39c376580"_guid;

		Array<Entity::ComponentValue<Entity::Component3D>, 2> components{
			Entity::ComponentValue<Entity::Component3D>{
				sceneRegistries[0],
				Entity::Component3D::Initializer{
					scenes[0]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			},
			Entity::ComponentValue<Entity::Component3D>{
				sceneRegistries[1],
				Entity::Component3D::Initializer{
					scenes[1]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			}
		};
		EXPECT_TRUE(components[0].IsValid());
		EXPECT_TRUE(components[1].IsValid());

		Array<ReferenceWrapper<BoundDataComponent>, 2> boundDataComponents{
			*components[0]->CreateDataComponent<BoundDataComponent>(
				sceneRegistries[0],
				BoundDataComponent::DynamicInitializer{*components[0], sceneRegistries[0]}
			),
			*components[1]->CreateDataComponent<BoundDataComponent>(
				sceneRegistries[1],
				BoundDataComponent::DynamicInitializer{*components[1], sceneRegistries[1]}
			),
		};

		// Create and bind the server component
		Array<Optional<Session::BoundComponent*>, 2> boundComponents{
			*components[0]->CreateDataComponent<Session::BoundComponent>(Session::BoundComponent::Initializer{*components[0], sceneRegistries[0]}
		  ),
			Invalid
		};
		EXPECT_TRUE(components[0].IsValid());

		{
			const uint8 maximumChannelCount = 2;
			const uint32 connectionUserData = 0;
			const bool startedConnecting = localClientComponent.Connect(
				client,
				Address(IO::URI(MAKE_URI("localhost"))),
				maximumChannelCount,
				connectionUserData,
				Network::LocalPeer::UpdateMode::EngineTick
			);
			EXPECT_TRUE(startedConnecting);
			if (!startedConnecting)
			{
				return;
			}

			// Start binding the client's equivalent of this component
			boundComponents[1] = *components[1]->CreateDataComponent<Session::BoundComponent>(
				Session::BoundComponent::Initializer{*components[1], sceneRegistries[1]}
			);

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&localClientComponent, &networkComponent = *boundComponents[1]]()
				{
					return !localClientComponent.IsConnected() || !networkComponent.IsBound();
				}
			);
		}

		// Test delegating authority
		{
			EXPECT_TRUE(boundComponents[0]->HasAuthority(*components[0], sceneRegistries[0]));
			EXPECT_FALSE(boundComponents[1]->HasAuthority(*components[1], sceneRegistries[1]));
			EXPECT_TRUE(boundComponents[0]->DelegateAuthority(*components[0], sceneRegistries[0], localClientComponent.GetIdentifier()));

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&networkComponent = *boundComponents[1], &owner = *components[1], &sceneRegistry = sceneRegistries[1]]()
				{
					return !networkComponent.HasAuthority(owner, sceneRegistry);
				}
			);

			EXPECT_TRUE(boundComponents[1]->HasAuthority(*components[1], sceneRegistries[1]));
			EXPECT_FALSE(boundComponents[0]->HasAuthority(*components[0], sceneRegistries[0]));
		}

		EXPECT_FALSE(boundDataComponents[0]->m_receivedClientToServer);
		EXPECT_FALSE(boundDataComponents[0]->m_receivedServerToClient);
		EXPECT_FALSE(boundDataComponents[1]->m_receivedClientToServer);
		EXPECT_FALSE(boundDataComponents[1]->m_receivedServerToClient);

		{
			Channel channel{0};
			const bool wasSent = boundComponents[0]->BroadcastToAllClients<&BoundDataComponent::ServerToClient>(
				*components[0],
				sceneRegistries[0],
				channel,
				MessageData{4321, 4.321f}
			);
			EXPECT_TRUE(wasSent);
		}

		// Wait for client to receive the message
		RunMainThreadJobRunner(
			[&clientComponent = *boundDataComponents[1], &serverComponent = *boundDataComponents[0]]()
			{
				return !clientComponent.m_receivedServerToClient && !serverComponent.m_receivedClientToServer;
			}
		);

		{
			localClientComponent.Disconnect(client);

			// Run the main thread job runner until we finished disconnecting
			RunMainThreadJobRunner(
				[&localClientComponent]()
				{
					return localClientComponent.IsConnected();
				}
			);

			// Perform one last tick for the disconnect to finish
			engine.DoTick();
		}
	}

	enum class PropagatedFlags : uint8
	{
		One = 1 << 0,
		Two = 1 << 1,
		Four = 1 << 2,
		Eight = 1 << 3,
		Sixteen = 1 << 4,
		ThirtyTwo = 1 << 5,
		SixtyFour = 1 << 6
	};
	ENUM_FLAG_OPERATORS(PropagatedFlags);

	struct Bound3DComponentPropagation final : public Entity::Component3D
	{
		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 1>;

		using BaseType::BaseType;

		EnumFlags<PropagatedFlags> m_propagatedServerToClientFlags;
		EnumFlags<PropagatedFlags> m_propagatedClientToServerFlags;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::Network::PropagatedFlags>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::PropagatedFlags>(
			"{0610F348-C206-4EE6-81DE-EA5DDF7B63F1}"_guid,
			MAKE_UNICODE_LITERAL("Test Flags"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{Tests::Network::PropagatedFlags::One, MAKE_UNICODE_LITERAL("1")},
				Reflection::EnumTypeEntry{Tests::Network::PropagatedFlags::Two, MAKE_UNICODE_LITERAL("2")},
				Reflection::EnumTypeEntry{Tests::Network::PropagatedFlags::Four, MAKE_UNICODE_LITERAL("4")},
				Reflection::EnumTypeEntry{Tests::Network::PropagatedFlags::Eight, MAKE_UNICODE_LITERAL("8")},
				Reflection::EnumTypeEntry{Tests::Network::PropagatedFlags::Sixteen, MAKE_UNICODE_LITERAL("16")},
				Reflection::EnumTypeEntry{Tests::Network::PropagatedFlags::ThirtyTwo, MAKE_UNICODE_LITERAL("32")},
				Reflection::EnumTypeEntry{Tests::Network::PropagatedFlags::SixtyFour, MAKE_UNICODE_LITERAL("64")},
			}}
		);
	};

	template<>
	struct ReflectedType<Tests::Network::Bound3DComponentPropagation>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::Bound3DComponentPropagation>(
			"{44EE2A52-3E8E-4FBC-88F6-57DBD2FEBF57}"_guid,
			MAKE_UNICODE_LITERAL("Bound 3D Component Propagation"),
			TypeFlags(),
			Tags{},
			Properties{
				Property{
					MAKE_UNICODE_LITERAL("Server to Client propagated flags"),
					"serverToClientFlags",
					"{770C4C3D-1149-4EC3-B894-4709DE21CB2E}"_guid,
					MAKE_UNICODE_LITERAL("TestComponent"),
					PropertyFlags::PropagateHostToClient,
					&Tests::Network::Bound3DComponentPropagation::m_propagatedServerToClientFlags
				},
				Property{
					MAKE_UNICODE_LITERAL("Client to Server propagated flags"),
					"clientToServerFlags",
					"{E66F99E6-CD36-485D-9936-6B8CADA81702}"_guid,
					MAKE_UNICODE_LITERAL("TestComponent"),
					PropertyFlags::PropagateClientToHost,
					&Tests::Network::Bound3DComponentPropagation::m_propagatedClientToServerFlags
				}
			}
		);
	};
}

namespace ngine::Tests::Network
{
	[[maybe_unused]] const bool wasBound3DComponentPropagationRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Bound3DComponentPropagation>>::Make());
	[[maybe_unused]] const bool wasBound3DComponentPropagationTypeRegistered =
		Reflection::Registry::RegisterType<Bound3DComponentPropagation>();

	FEATURE_TEST(Networking, BindNetworkedComponentAndPropagatePropertiesStandaloneServer)
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

		// Keep two scenes, for the host and another for the client to simulate two machines
		Array<Entity::SceneRegistry, 2> sceneRegistries;

		Array<ReferenceWrapper<Entity::RootComponent>, 2> rootComponents{
			*sceneRegistries[0].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[0]),
			*sceneRegistries[1].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[1])
		};

		constexpr uint8 maximumClientCount = 1;

		Session::Host& host = *sceneRegistries[0].GetOrCreateComponentTypeData<Session::Host>()->CreateInstance(
			Session::Host::Initializer{Entity::HierarchyComponentBase::DynamicInitializer{*rootComponents[0], sceneRegistries[0]}}
		);
		Session::LocalHost& localHost = *host.CreateDataComponent<Session::LocalHost>(
			sceneRegistries[0],
			Session::LocalHost::Initializer{
				Entity::Data::HierarchyComponent::DynamicInitializer{host, sceneRegistries[0]},
				Network::AnyIPAddress,
				maximumClientCount,
				Network::LocalPeer::UpdateMode::EngineTick
			}
		);
		EXPECT_TRUE(localHost.GetLocalHost().IsValid());
		if (!localHost.GetLocalHost().IsValid())
		{
			return;
		}

		Session::Client& client = *sceneRegistries[1].GetOrCreateComponentTypeData<Session::Client>()->CreateInstance(
			Session::Client::Initializer{*rootComponents[1], sceneRegistries[1]}
		);
		[[maybe_unused]] Session::LocalClient& localClientComponent = *client.CreateDataComponent<Session::LocalClient>(
			sceneRegistries[1],
			Session::LocalClient::Initializer{Entity::Data::HierarchyComponent::DynamicInitializer{client, sceneRegistries[1]}}
		);

		Array<UniquePtr<Scene>, 2> scenes{
			UniquePtr<Scene>::Make(
				sceneRegistries[0],
				*rootComponents[0],
				1024_meters,
				"5ebf8a78-c5fa-4e6c-81be-d35a788ab1b4"_guid,
				Scene::Flags::IsDisabled
			),
			UniquePtr<Scene>::Make(sceneRegistries[1], client, 1024_meters, "6e0fe654-8eb2-4db0-9ee7-fd39451b37b3"_guid, Scene::Flags::IsDisabled)
		};

		constexpr Guid componentInstanceGuid = "dc2ebe03-faca-4f91-b722-89b39c376580"_guid;

		Array<Entity::ComponentValue<Bound3DComponentPropagation>, 2> components{
			Entity::ComponentValue<Bound3DComponentPropagation>{
				sceneRegistries[0],
				Bound3DComponentPropagation::Initializer{
					scenes[0]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			},
			Entity::ComponentValue<Bound3DComponentPropagation>{
				sceneRegistries[1],
				Bound3DComponentPropagation::Initializer{
					scenes[1]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			}
		};
		EXPECT_TRUE(components[0].IsValid());
		EXPECT_TRUE(components[1].IsValid());

		// Create and bind the server component
		Array<Optional<Session::BoundComponent*>, 2> boundComponents{
			*components[0]->CreateDataComponent<Session::BoundComponent>(Session::BoundComponent::Initializer{*components[0], sceneRegistries[0]}
		  ),
			Invalid
		};
		EXPECT_TRUE(components[0].IsValid());

		{
			const uint8 maximumChannelCount = 2;
			const uint32 connectionUserData = 0;
			const bool startedConnecting = localClientComponent.Connect(
				client,
				Address(IO::URI(MAKE_URI("localhost"))),
				maximumChannelCount,
				connectionUserData,
				Network::LocalPeer::UpdateMode::EngineTick
			);
			EXPECT_TRUE(startedConnecting);
			if (!startedConnecting)
			{
				return;
			}

			// Start binding the client's equivalent of this component
			boundComponents[1] = *components[1]->CreateDataComponent<Session::BoundComponent>(
				Session::BoundComponent::Initializer{*components[1], sceneRegistries[1]}
			);

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&localClientComponent, &networkComponent = *boundComponents[1]]()
				{
					return !localClientComponent.IsConnected() || !networkComponent.IsBound();
				}
			);
		}

		// Now test property propagation from server to client
		{
			EXPECT_TRUE(components[0]->m_propagatedServerToClientFlags.AreNoneSet());
			EXPECT_TRUE(components[1]->m_propagatedServerToClientFlags.AreNoneSet());
			components[0]->m_propagatedServerToClientFlags.Set(PropagatedFlags::Sixteen);
			EXPECT_TRUE(components[1]->m_propagatedServerToClientFlags.AreNoneSet());

			const bool wasInvalidated = boundComponents[0]->InvalidateProperties<&Bound3DComponentPropagation::m_propagatedServerToClientFlags>(
				*components[0],
				sceneRegistries[0]
			);
			EXPECT_TRUE(wasInvalidated);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundClientComponent = *components[1]]()
				{
					return boundClientComponent.m_propagatedServerToClientFlags.IsNotSet(PropagatedFlags::Sixteen);
				}
			);
			EXPECT_EQ(components[0]->m_propagatedServerToClientFlags, PropagatedFlags::Sixteen);
			EXPECT_EQ(components[1]->m_propagatedServerToClientFlags, PropagatedFlags::Sixteen);
		}

		// Test delegating authority to the object
		{
			EXPECT_TRUE(boundComponents[0]->HasAuthority(*components[0], sceneRegistries[0]));
			EXPECT_FALSE(boundComponents[1]->HasAuthority(*components[1], sceneRegistries[1]));

			boundComponents[0]->DelegateAuthority(*components[0], sceneRegistries[0], localClientComponent.GetIdentifier());

			// Run the main thread job runner until we received authority
			RunMainThreadJobRunner(
				[&boundComponent = *boundComponents[1], &component = *components[1], &sceneRegistry = sceneRegistries[1]]()
				{
					return !boundComponent.HasAuthority(component, sceneRegistry);
				}
			);

			EXPECT_FALSE(boundComponents[0]->HasAuthority(*components[0], sceneRegistries[0]));
			EXPECT_TRUE(boundComponents[1]->HasAuthority(*components[1], sceneRegistries[1]));
		}

		// Now test property propagation from client to server
		{
			EXPECT_TRUE(components[0]->m_propagatedClientToServerFlags.AreNoneSet());
			EXPECT_TRUE(components[1]->m_propagatedClientToServerFlags.AreNoneSet());
			components[1]->m_propagatedClientToServerFlags.Set(PropagatedFlags::Two);
			components[1]->m_propagatedClientToServerFlags.Set(PropagatedFlags::ThirtyTwo);
			EXPECT_TRUE(components[0]->m_propagatedClientToServerFlags.AreNoneSet());

			const bool wasInvalidated = boundComponents[1]->InvalidateProperties<&Bound3DComponentPropagation::m_propagatedClientToServerFlags>(
				*components[1],
				sceneRegistries[1]
			);
			EXPECT_TRUE(wasInvalidated);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundServerComponent = *components[0]]()
				{
					return !boundServerComponent.m_propagatedClientToServerFlags.AreAllSet(PropagatedFlags::Two | PropagatedFlags::ThirtyTwo);
				}
			);
			EXPECT_EQ(components[0]->m_propagatedClientToServerFlags, PropagatedFlags::Two | PropagatedFlags::ThirtyTwo);
			EXPECT_EQ(components[1]->m_propagatedClientToServerFlags, PropagatedFlags::Two | PropagatedFlags::ThirtyTwo);
		}

		{
			localClientComponent.Disconnect(client);

			// Run the main thread job runner until we finished disconnecting
			RunMainThreadJobRunner(
				[&localClientComponent]()
				{
					return localClientComponent.IsConnected();
				}
			);

			// Perform one last tick for the disconnect to finish
			engine.DoTick();
		}
	}

	struct BoundDataPropagationComponent final : public Entity::Data::Component
	{
		using BaseType = Component;
		using InstanceIdentifier = TIdentifier<uint32, 1>;

		using BaseType::BaseType;

		EnumFlags<PropagatedFlags> m_propagatedServerToClientFlags;
		EnumFlags<PropagatedFlags> m_propagatedClientToServerFlags;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::Network::BoundDataPropagationComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::Network::BoundDataPropagationComponent>(
			"{FD3F5C9D-F539-4098-A541-B96D5237AB55}"_guid,
			MAKE_UNICODE_LITERAL("Bound Data Component"),
			TypeFlags(),
			Tags{},
			Properties{
				Property{
					MAKE_UNICODE_LITERAL("Server to Client propagated flags"),
					"serverToClientFlags",
					"{897A1520-A866-4B73-9A2C-172C9067267C}"_guid,
					MAKE_UNICODE_LITERAL("TestComponent"),
					PropertyFlags::PropagateHostToClient,
					&Tests::Network::BoundDataPropagationComponent::m_propagatedServerToClientFlags
				},
				Property{
					MAKE_UNICODE_LITERAL("Client to Server propagated flags"),
					"clientToServerFlags",
					"{ACEE9176-2DA9-4839-A225-D79418C5BB28}"_guid,
					MAKE_UNICODE_LITERAL("TestComponent"),
					PropertyFlags::PropagateClientToHost,
					&Tests::Network::BoundDataPropagationComponent::m_propagatedClientToServerFlags
				}
			}
		);
	};
}

namespace ngine::Tests::Network
{
	[[maybe_unused]] const bool wasBoundDataPropagationComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<BoundDataPropagationComponent>>::Make());
	[[maybe_unused]] const bool wasBoundDataPropagationComponentTypeRegistered =
		Reflection::Registry::RegisterType<BoundDataPropagationComponent>();

	FEATURE_TEST(Networking, BindNetworkedDataComponentAndPropagatePropertiesStandaloneServer)
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

		// Keep two scenes, for the host and another for the client to simulate two machines
		Array<Entity::SceneRegistry, 2> sceneRegistries;

		Array<ReferenceWrapper<Entity::RootComponent>, 2> rootComponents{
			*sceneRegistries[0].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[0]),
			*sceneRegistries[1].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[1])
		};

		constexpr uint8 maximumClientCount = 1;

		Session::Host& host = *sceneRegistries[0].GetOrCreateComponentTypeData<Session::Host>()->CreateInstance(
			Session::Host::Initializer{Entity::HierarchyComponentBase::DynamicInitializer{*rootComponents[0], sceneRegistries[0]}}
		);
		Session::LocalHost& localHost = *host.CreateDataComponent<Session::LocalHost>(
			sceneRegistries[0],
			Session::LocalHost::Initializer{
				Entity::Data::HierarchyComponent::DynamicInitializer{host, sceneRegistries[0]},
				Network::AnyIPAddress,
				maximumClientCount,
				Network::LocalPeer::UpdateMode::EngineTick
			}
		);
		EXPECT_TRUE(localHost.GetLocalHost().IsValid());
		if (!localHost.GetLocalHost().IsValid())
		{
			return;
		}

		Session::Client& client = *sceneRegistries[1].GetOrCreateComponentTypeData<Session::Client>()->CreateInstance(
			Session::Client::Initializer{*rootComponents[1], sceneRegistries[1]}
		);
		[[maybe_unused]] Session::LocalClient& localClientComponent = *client.CreateDataComponent<Session::LocalClient>(
			sceneRegistries[1],
			Session::LocalClient::Initializer{Entity::Data::HierarchyComponent::DynamicInitializer{client, sceneRegistries[1]}}
		);

		Array<UniquePtr<Scene>, 2> scenes{
			UniquePtr<Scene>::Make(
				sceneRegistries[0],
				*rootComponents[0],
				1024_meters,
				"5ebf8a78-c5fa-4e6c-81be-d35a788ab1b4"_guid,
				Scene::Flags::IsDisabled
			),
			UniquePtr<Scene>::Make(sceneRegistries[1], client, 1024_meters, "6e0fe654-8eb2-4db0-9ee7-fd39451b37b3"_guid, Scene::Flags::IsDisabled)
		};

		constexpr Guid componentInstanceGuid = "dc2ebe03-faca-4f91-b722-89b39c376580"_guid;

		Array<Entity::ComponentValue<Entity::Component3D>, 2> components{
			Entity::ComponentValue<Entity::Component3D>{
				sceneRegistries[0],
				Entity::Component3D::Initializer{
					scenes[0]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			},
			Entity::ComponentValue<Entity::Component3D>{
				sceneRegistries[1],
				Entity::Component3D::Initializer{
					scenes[1]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			}
		};
		EXPECT_TRUE(components[0].IsValid());
		EXPECT_TRUE(components[1].IsValid());

		Array<ReferenceWrapper<BoundDataPropagationComponent>, 2> boundDataComponents{
			*components[0]->CreateDataComponent<BoundDataPropagationComponent>(
				sceneRegistries[0],
				BoundDataPropagationComponent::DynamicInitializer{*components[0], sceneRegistries[0]}
			),
			*components[1]->CreateDataComponent<BoundDataPropagationComponent>(
				sceneRegistries[1],
				BoundDataPropagationComponent::DynamicInitializer{*components[1], sceneRegistries[1]}
			),
		};

		// Create and bind the server component
		Array<Optional<Session::BoundComponent*>, 2> boundComponents{
			*components[0]->CreateDataComponent<Session::BoundComponent>(Session::BoundComponent::Initializer{*components[0], sceneRegistries[0]}
		  ),
			Invalid
		};
		EXPECT_TRUE(components[0].IsValid());

		{
			const uint8 maximumChannelCount = 2;
			const uint32 connectionUserData = 0;
			const bool startedConnecting = localClientComponent.Connect(
				client,
				Address(IO::URI(MAKE_URI("localhost"))),
				maximumChannelCount,
				connectionUserData,
				Network::LocalPeer::UpdateMode::EngineTick
			);
			EXPECT_TRUE(startedConnecting);
			if (!startedConnecting)
			{
				return;
			}

			// Start binding the client's equivalent of this component
			boundComponents[1] = *components[1]->CreateDataComponent<Session::BoundComponent>(
				Session::BoundComponent::Initializer{*components[1], sceneRegistries[1]}
			);

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&localClientComponent, &networkComponent = *boundComponents[1]]()
				{
					return !localClientComponent.IsConnected() || !networkComponent.IsBound();
				}
			);
		}

		// Now test property propagation from server to client
		{
			EXPECT_TRUE(boundDataComponents[0]->m_propagatedServerToClientFlags.AreNoneSet());
			EXPECT_TRUE(boundDataComponents[1]->m_propagatedServerToClientFlags.AreNoneSet());
			boundDataComponents[0]->m_propagatedServerToClientFlags.Set(PropagatedFlags::Sixteen);
			EXPECT_TRUE(boundDataComponents[1]->m_propagatedServerToClientFlags.AreNoneSet());

			const bool wasInvalidated = boundComponents[0]->InvalidateProperties<&BoundDataPropagationComponent::m_propagatedServerToClientFlags>(
				*components[0],
				sceneRegistries[0]
			);
			EXPECT_TRUE(wasInvalidated);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundClientComponent = *boundDataComponents[1]]()
				{
					return boundClientComponent.m_propagatedServerToClientFlags.IsNotSet(PropagatedFlags::Sixteen);
				}
			);
			EXPECT_EQ(boundDataComponents[0]->m_propagatedServerToClientFlags, PropagatedFlags::Sixteen);
			EXPECT_EQ(boundDataComponents[1]->m_propagatedServerToClientFlags, PropagatedFlags::Sixteen);
		}

		// Test delegating authority to the object
		{
			EXPECT_TRUE(boundComponents[0]->HasAuthority(*components[0], sceneRegistries[0]));
			EXPECT_FALSE(boundComponents[1]->HasAuthority(*components[1], sceneRegistries[1]));

			boundComponents[0]->DelegateAuthority(*components[0], sceneRegistries[0], localClientComponent.GetIdentifier());

			// Run the main thread job runner until we received authority
			RunMainThreadJobRunner(
				[&boundComponent = *boundComponents[1], &component = *components[1], &sceneRegistry = sceneRegistries[1]]()
				{
					return !boundComponent.HasAuthority(component, sceneRegistry);
				}
			);

			EXPECT_FALSE(boundComponents[0]->HasAuthority(*components[0], sceneRegistries[0]));
			EXPECT_TRUE(boundComponents[1]->HasAuthority(*components[1], sceneRegistries[1]));
		}

		// Now test property propagation from client to server
		{
			EXPECT_TRUE(boundDataComponents[0]->m_propagatedClientToServerFlags.AreNoneSet());
			EXPECT_TRUE(boundDataComponents[1]->m_propagatedClientToServerFlags.AreNoneSet());
			boundDataComponents[1]->m_propagatedClientToServerFlags.Set(PropagatedFlags::Two);
			boundDataComponents[1]->m_propagatedClientToServerFlags.Set(PropagatedFlags::ThirtyTwo);
			EXPECT_TRUE(boundDataComponents[0]->m_propagatedClientToServerFlags.AreNoneSet());

			const bool wasInvalidated = boundComponents[1]->InvalidateProperties<&BoundDataPropagationComponent::m_propagatedClientToServerFlags>(
				*components[1],
				sceneRegistries[1]
			);
			EXPECT_TRUE(wasInvalidated);

			// Run the main thread job runner until we received the message
			RunMainThreadJobRunner(
				[&boundServerComponent = *boundDataComponents[0]]()
				{
					return !boundServerComponent.m_propagatedClientToServerFlags.AreAllSet(PropagatedFlags::Two | PropagatedFlags::ThirtyTwo);
				}
			);
			EXPECT_EQ(boundDataComponents[0]->m_propagatedClientToServerFlags, PropagatedFlags::Two | PropagatedFlags::ThirtyTwo);
			EXPECT_EQ(boundDataComponents[1]->m_propagatedClientToServerFlags, PropagatedFlags::Two | PropagatedFlags::ThirtyTwo);
		}

		{
			localClientComponent.Disconnect(client);

			// Run the main thread job runner until we finished disconnecting
			RunMainThreadJobRunner(
				[&localClientComponent]()
				{
					return localClientComponent.IsConnected();
				}
			);

			// Perform one last tick for the disconnect to finish
			engine.DoTick();
		}
	}

	FEATURE_TEST(Networking, SpawnNetworkedComponentStandaloneServer)
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

		// Keep two scenes, for the host and another for the client to simulate two machines
		Array<Entity::SceneRegistry, 2> sceneRegistries;

		Array<ReferenceWrapper<Entity::RootComponent>, 2> rootComponents{
			*sceneRegistries[0].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[0]),
			*sceneRegistries[1].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[1])
		};

		constexpr uint8 maximumClientCount = 1;

		Session::Host& host = *sceneRegistries[0].GetOrCreateComponentTypeData<Session::Host>()->CreateInstance(
			Session::Host::Initializer{Entity::HierarchyComponentBase::DynamicInitializer{*rootComponents[0], sceneRegistries[0]}}
		);
		Session::LocalHost& localHost = *host.CreateDataComponent<Session::LocalHost>(
			sceneRegistries[0],
			Session::LocalHost::Initializer{
				Entity::Data::HierarchyComponent::DynamicInitializer{host, sceneRegistries[0]},
				Network::AnyIPAddress,
				maximumClientCount,
				Network::LocalPeer::UpdateMode::EngineTick
			}
		);
		EXPECT_TRUE(localHost.GetLocalHost().IsValid());
		if (!localHost.GetLocalHost().IsValid())
		{
			return;
		}

		Session::Client& client = *sceneRegistries[1].GetOrCreateComponentTypeData<Session::Client>()->CreateInstance(
			Session::Client::Initializer{*rootComponents[1], sceneRegistries[1]}
		);
		[[maybe_unused]] Session::LocalClient& localClientComponent = *client.CreateDataComponent<Session::LocalClient>(
			sceneRegistries[1],
			Session::LocalClient::Initializer{Entity::Data::HierarchyComponent::DynamicInitializer{client, sceneRegistries[1]}}
		);

		Array<UniquePtr<Scene>, 2> scenes{
			UniquePtr<Scene>::Make(
				sceneRegistries[0],
				*rootComponents[0],
				1024_meters,
				"E1E1A174-31B4-4511-B5E4-279DAECD5B10"_guid,
				Scene::Flags::IsDisabled
			),
			UniquePtr<Scene>::Make(sceneRegistries[1], client, 1024_meters, "92BB4CF6-1E90-40AC-8E3D-92BA6404E4A0"_guid, Scene::Flags::IsDisabled)
		};

		constexpr Guid componentInstanceGuid = "dc2ebe03-faca-4f91-b722-89b39c376580"_guid;

		Array<Entity::ComponentValue<Entity::Component3D>, 2> components{
			Entity::ComponentValue<Entity::Component3D>{
				sceneRegistries[0],
				Entity::Component3D::Initializer{
					scenes[0]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			},
			Entity::ComponentValue<Entity::Component3D>{
				sceneRegistries[1],
				Entity::Component3D::Initializer{
					scenes[1]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			}
		};
		EXPECT_TRUE(components[0].IsValid());
		EXPECT_TRUE(components[1].IsValid());

		// Create and bind the server component
		Array<Optional<Session::BoundComponent*>, 2> boundComponents{
			*components[0]->CreateDataComponent<Session::BoundComponent>(Session::BoundComponent::Initializer{*components[0], sceneRegistries[0]}
		  ),
			Invalid
		};
		EXPECT_TRUE(components[0].IsValid());

		{
			const uint8 maximumChannelCount = 2;
			const uint32 connectionUserData = 0;
			const bool startedConnecting = localClientComponent.Connect(
				client,
				Address(IO::URI(MAKE_URI("localhost"))),
				maximumChannelCount,
				connectionUserData,
				Network::LocalPeer::UpdateMode::EngineTick
			);
			EXPECT_TRUE(startedConnecting);
			if (!startedConnecting)
			{
				return;
			}

			// Start binding the client's equivalent of this component
			boundComponents[1] = *components[1]->CreateDataComponent<Session::BoundComponent>(
				Session::BoundComponent::Initializer{*components[1], sceneRegistries[1]}
			);

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&localClientComponent, &networkComponent = *boundComponents[1]]()
				{
					return !localClientComponent.IsConnected() || !networkComponent.IsBound();
				}
			);
		}

		const Optional<Entity::HierarchyComponentBase*> pLocalComponent = boundComponents[0]->SpawnBoundChildOnAllClients(
			*components[0],
			sceneRegistries[0],
			*sceneRegistries[0].FindComponentTypeData<Entity::Component3D>()
		);
		EXPECT_TRUE(pLocalComponent.IsValid());
		EXPECT_TRUE(pLocalComponent->Is<Entity::Component3D>());
		EXPECT_TRUE(pLocalComponent->HasDataComponentOfType<Session::BoundComponent>(sceneRegistries[0]));

		// Run the main thread job runner until we finished connecting and the network component hasn't been bound
		RunMainThreadJobRunner(
			[&clientComponent = *components[1], &clientRegistry = sceneRegistries[1]]()
			{
				return !clientComponent.HasChildren() ||
			         !clientComponent.GetChild(0).HasDataComponentOfType<Session::BoundComponent>(clientRegistry);
			}
		);

		EXPECT_TRUE(components[1]->GetChild(0).Is<Entity::Component3D>());
		EXPECT_TRUE(components[1]->GetChild(0).HasDataComponentOfType<Session::BoundComponent>(sceneRegistries[1]));

		{
			localClientComponent.Disconnect(client);

			// Run the main thread job runner until we finished disconnecting
			RunMainThreadJobRunner(
				[&localClientComponent]()
				{
					return localClientComponent.IsConnected();
				}
			);

			// Perform one last tick for the disconnect to finish
			engine.DoTick();
		}
	}

	FEATURE_TEST(Networking, AddNetworkedDataComponentStandaloneServer)
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

		// Keep two scenes, for the host and another for the client to simulate two machines
		Array<Entity::SceneRegistry, 2> sceneRegistries;

		Array<ReferenceWrapper<Entity::RootComponent>, 2> rootComponents{
			*sceneRegistries[0].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[0]),
			*sceneRegistries[1].CreateComponentTypeData<Entity::RootComponent>()->CreateInstance(sceneRegistries[1])
		};

		Session::Client& client = *sceneRegistries[1].GetOrCreateComponentTypeData<Session::Client>()->CreateInstance(
			Session::Client::Initializer{*rootComponents[1], sceneRegistries[1]}
		);
		[[maybe_unused]] Session::LocalClient& localClientComponent = *client.CreateDataComponent<Session::LocalClient>(
			sceneRegistries[1],
			Session::LocalClient::Initializer{Entity::Data::HierarchyComponent::DynamicInitializer{client, sceneRegistries[1]}}
		);

		constexpr uint8 maximumClientCount = 1;

		Session::Host& host = *sceneRegistries[0].GetOrCreateComponentTypeData<Session::Host>()->CreateInstance(
			Session::Host::Initializer{Entity::HierarchyComponentBase::DynamicInitializer{*rootComponents[0], sceneRegistries[0]}}
		);
		Session::LocalHost& localHost = *host.CreateDataComponent<Session::LocalHost>(
			sceneRegistries[0],
			Session::LocalHost::Initializer{
				Entity::Data::HierarchyComponent::DynamicInitializer{host, sceneRegistries[0]},
				Network::AnyIPAddress,
				maximumClientCount,
				Network::LocalPeer::UpdateMode::EngineTick
			}
		);
		EXPECT_TRUE(localHost.GetLocalHost().IsValid());
		if (!localHost.GetLocalHost().IsValid())
		{
			return;
		}

		Array<UniquePtr<Scene>, 2> scenes{
			UniquePtr<Scene>::Make(
				sceneRegistries[0],
				*rootComponents[0],
				1024_meters,
				"E1E1A174-31B4-4511-B5E4-279DAECD5B10"_guid,
				Scene::Flags::IsDisabled
			),
			UniquePtr<Scene>::Make(sceneRegistries[1], client, 1024_meters, "92BB4CF6-1E90-40AC-8E3D-92BA6404E4A0"_guid, Scene::Flags::IsDisabled)
		};

		constexpr Guid componentInstanceGuid = "dc2ebe03-faca-4f91-b722-89b39c376580"_guid;

		Array<Entity::ComponentValue<Entity::Component3D>, 2> components{
			Entity::ComponentValue<Entity::Component3D>{
				sceneRegistries[0],
				Entity::Component3D::Initializer{
					scenes[0]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			},
			Entity::ComponentValue<Entity::Component3D>{
				sceneRegistries[1],
				Entity::Component3D::Initializer{
					scenes[1]->GetRootComponent(),
					Math::LocalTransform{Math::Identity},
					Math::BoundingBox(0.001_meters),
					Entity::Component3D::Flags{},
					componentInstanceGuid
				}
			}
		};
		EXPECT_TRUE(components[0].IsValid());
		EXPECT_TRUE(components[1].IsValid());

		// Create and bind the server component
		Array<Optional<Session::BoundComponent*>, 2> boundComponents{
			*components[0]->CreateDataComponent<Session::BoundComponent>(Session::BoundComponent::Initializer{*components[0], sceneRegistries[0]}
		  ),
			Invalid
		};
		EXPECT_TRUE(components[0].IsValid());

		{
			const uint8 maximumChannelCount = 2;
			const uint32 connectionUserData = 0;
			const bool startedConnecting = localClientComponent.Connect(
				client,
				Address(IO::URI(MAKE_URI("localhost"))),
				maximumChannelCount,
				connectionUserData,
				Network::LocalPeer::UpdateMode::EngineTick
			);
			EXPECT_TRUE(startedConnecting);
			if (!startedConnecting)
			{
				return;
			}

			// Start binding the client's equivalent of this component
			boundComponents[1] = *components[1]->CreateDataComponent<Session::BoundComponent>(
				Session::BoundComponent::Initializer{*components[1], sceneRegistries[1]}
			);

			// Run the main thread job runner until we finished connecting and the network component hasn't been bound
			RunMainThreadJobRunner(
				[&localClientComponent, &networkComponent = *boundComponents[1]]()
				{
					return !localClientComponent.IsConnected() || !networkComponent.IsBound();
				}
			);
		}

		const Optional<Entity::Data::Component*> pLocalComponent = boundComponents[0]->AddDataComponent(
			*components[0],
			sceneRegistries[0],
			*sceneRegistries[0].GetOrCreateComponentTypeData<BoundDataComponent>()
		);
		EXPECT_TRUE(pLocalComponent.IsValid());
		EXPECT_TRUE(components[0]->HasDataComponentOfType<BoundDataComponent>(sceneRegistries[0]));

		// Run the main thread job runner until we finished connecting and the network component hasn't been bound
		RunMainThreadJobRunner(
			[&clientComponent = *components[1], &clientRegistry = sceneRegistries[1]]()
			{
				return !clientComponent.HasDataComponentOfType<BoundDataComponent>(clientRegistry);
			}
		);

		{
			localClientComponent.Disconnect(client);

			// Run the main thread job runner until we finished disconnecting
			RunMainThreadJobRunner(
				[&localClientComponent]()
				{
					return localClientComponent.IsConnected();
				}
			);

			// Perform one last tick for the disconnect to finish
			engine.DoTick();
		}
	}
}
