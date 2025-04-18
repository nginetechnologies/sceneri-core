#include <Common/Memory/New.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/ComponentTypeIdentifier.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Tests/FeatureTest.h>

#include <Common/CommandLine/CommandLineInitializationParameters.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Serialization/SerializedData.h>
#include <Common/Serialization/Writer.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Tests
{
	struct DataComponentNoProperties : public Entity::Data::Component3D
	{
		using BaseType = Component3D;
		using BaseType::BaseType;
	};

	struct DataComponentWithProperties : public Entity::Data::Component3D
	{
		using BaseType = Component3D;
		using BaseType::BaseType;
		DataComponentWithProperties(const DataComponentWithProperties& templateComponent, const Cloner&)
			: m_name(templateComponent.m_name)
		{
		}
		DataComponentWithProperties(const Deserializer& deserializer)
			: m_name(deserializer.m_reader.ReadWithDefaultValue<UnicodeString>("name", {}))
		{
		}

		UnicodeString m_name;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::DataComponentNoProperties>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::DataComponentNoProperties>(
			"748ce24b-c9b0-4b0d-98f4-83da6a77d9e1"_guid,
			MAKE_UNICODE_LITERAL("Editor Info"),
			TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDynamicInstantiation
		);
	};

	template<>
	struct ReflectedType<Tests::DataComponentWithProperties>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::DataComponentWithProperties>(
			"42f66b83-8c3a-4830-bbd8-379dc4f6752d"_guid,
			MAKE_UNICODE_LITERAL("Test Component"),
			TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDynamicInstantiation,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Name"),
				"name",
				"{955D9730-A4A3-4E2E-82A8-CFA8CD08F333}"_guid,
				MAKE_UNICODE_LITERAL("Test Component"),
				&Tests::DataComponentWithProperties::m_name
			)}
		);
	};
}

namespace ngine::Tests
{
	FEATURE_TEST(Scene, SceneLoadAndSave)
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();

		constexpr Asset::Guid assetGuid = "{80676731-4758-4FE7-A8EE-C1A4ED8DD045}"_asset;
		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(assetGuid);

		static constexpr ConstZeroTerminatedStringView sceneJson = R"({
    "guid": "80676731-4758-4fe7-a8ee-c1a4ed8dd045",
    "type_version": 1,
    "typeGuid": "2a2bce0a-6314-481b-a5a0-7d01e1c00b35",
    "instanceGuid": "5c66ede5-cf54-4057-8b8f-949e5739e0bc",
    "de8f8da2-d37f-41da-bc6e-580435fe605f": {
        "disabled": true
    },
    "2a2bce0a-6314-481b-a5a0-7d01e1c00b35": {
        "radius": 4096
    }
})";

		System::Get<Asset::Manager>().RegisterAsset(assetGuid, Asset::DatabaseEntry{}, Asset::Identifier{});

		System::Get<Asset::Manager>().RegisterAsyncLoadCallback(
			assetGuid,
			[](const Guid, const IO::PathView, Threading::JobPriority, IO::AsyncLoadCallback&& callback, const ByteView, const Math::Range<size>)
				-> Optional<Threading::Job*>
			{
				return Threading::CreateCallback(
					[callback = Forward<IO::AsyncLoadCallback>(callback)](Threading::JobRunnerThread&)
					{
						callback(sceneJson.GetView());
					},
					Threading::JobPriority::LoadScene
				);
			}
		);

		{
			Threading::JobBatch jobBatch;
			Entity::SceneRegistry sceneRegistry;
			UniquePtr<Scene> pScene = UniquePtr<Scene>::Make(sceneRegistry, Invalid, sceneTemplateIdentifier, jobBatch, Scene::Flags::IsDisabled);

			static Threading::Atomic<bool> finished = false;
			jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[](Threading::JobRunnerThread&)
				{
					finished = true;
				},
				Threading::JobPriority::LoadScene
			));

			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);

			// Run the main thread job runner until we finished loading
			RunMainThreadJobRunner(
				[]()
				{
					return !finished;
				}
			);

			EXPECT_EQ(pScene->GetGuid(), assetGuid);
			EXPECT_EQ(pScene->GetRootComponent().GetInstanceGuid(), "{5C66EDE5-CF54-4057-8B8F-949E5739E0BC}"_guid);
			EXPECT_EQ(pScene->GetRootComponent().GetRadius(), 4096_meters);

			{
				Serialization::Data data(rapidjson::Type::kObjectType, Serialization::ContextFlags::ToDisk);
				Serialization::Writer writer(data);
				const bool wasWritten = writer.SerializeInPlace(*pScene);
				EXPECT_TRUE(wasWritten);

				String writtenJson = data.SaveToBuffer<String>(Serialization::SavingFlags::HumanReadable);
				EXPECT_STREQ((const char*)writtenJson.GetZeroTerminated(), sceneJson);
			}

			pScene->Disable();
		}

		System::Get<Asset::Manager>().RemoveAsyncLoadCallback(assetGuid);
		System::Get<Asset::Manager>().RemoveAsset(assetGuid);
	}

	FEATURE_TEST(Scene, SceneLoadAndSaveNested)
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();

		constexpr Asset::Guid mainSceneAssetGuid = "69798ef6-394f-4615-8064-14d748751a72"_asset;
		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(mainSceneAssetGuid);

		constexpr Asset::Guid nestedSceneAssetGuid = "096bf901-bb9c-ac2d-783b-e84beaba8e1b"_asset;

		static constexpr ConstZeroTerminatedStringView mainSceneJson = R"({
    "guid": "69798ef6-394f-4615-8064-14d748751a72",
    "type_version": 1,
    "typeGuid": "2a2bce0a-6314-481b-a5a0-7d01e1c00b35",
    "instanceGuid": "6bd881ba-0be0-4f16-bec4-6c7a19b7bcd8",
    "de8f8da2-d37f-41da-bc6e-580435fe605f": {
        "disabled": true
    },
    "2a2bce0a-6314-481b-a5a0-7d01e1c00b35": {
        "radius": 4096
    },
    "children": [
        {
            "typeGuid": "0c3f4f29-0d53-4878-922a-cca7d6af7bb6",
            "instanceGuid": "5c8f80ca-e4fc-418a-a6aa-6bf110800860",
            "data_components": [
                {
                    "typeGuid": "1c6d4883-40c7-476d-95ee-90a82135974b",
                    "templateInstanceGuid": "4608f70f-ba69-4fc4-99ae-0afa786986df"
                }
            ],
            "children": [
                {
                    "typeGuid": "de8f8da2-d37f-41da-bc6e-580435fe605f",
                    "instanceGuid": "2e1e6880-a640-459d-a33a-8ea43f54f8b7",
                    "de8f8da2-d37f-41da-bc6e-580435fe605f": {
                        "position": [
                            1,
                            2,
                            3
                        ]
                    },
                    "data_components": [
                        {
                            "typeGuid": "1c6d4883-40c7-476d-95ee-90a82135974b",
                            "templateInstanceGuid": "e92d7022-5167-4574-a079-f8ffdd165d67"
                        }
                    ]
                }
            ],
            "0c3f4f29-0d53-4878-922a-cca7d6af7bb6": {
                "scene": "096bf901-bb9c-ac2d-783b-e84beaba8e1b"
            }
        }
    ]
})";

		static constexpr ConstZeroTerminatedStringView nestedSceneJson = R"({
    "guid": "096bf901-bb9c-ac2d-783b-e84beaba8e1b",
    "source": "../SC_Buzzy_AR.fbx",
    "assetTypeGuid": "807d69a9-56ec-4032-bb92-d081df336672",
    "typeGuid": "0c3f4f29-0d53-4878-922a-cca7d6af7bb6",
    "instanceGuid": "4608f70f-ba69-4fc4-99ae-0afa786986df",
    "de8f8da2-d37f-41da-bc6e-580435fe605f": {
        "disabled": true
    },
    "2a2bce0a-6314-481b-a5a0-7d01e1c00b35": {
        "radius": 4096
    },
    "children": [
        {
            "typeGuid": "de8f8da2-d37f-41da-bc6e-580435fe605f",
            "instanceGuid": "{E92D7022-5167-4574-A079-F8FFDD165D67}"
        },
        {
            "typeGuid": "de8f8da2-d37f-41da-bc6e-580435fe605f",
            "instanceGuid": "{79138FF2-8BEB-44BF-A41A-D32E6D99A7A6}"
        }
    ]
})";

		System::Get<Asset::Manager>().RegisterAsset(mainSceneAssetGuid, Asset::DatabaseEntry{}, Asset::Identifier{});
		System::Get<Asset::Manager>().RegisterAsyncLoadCallback(
			mainSceneAssetGuid,
			[](const Guid, const IO::PathView, Threading::JobPriority, IO::AsyncLoadCallback&& callback, const ByteView, const Math::Range<size>)
				-> Optional<Threading::Job*>
			{
				return Threading::CreateCallback(
					[callback = Forward<IO::AsyncLoadCallback>(callback)](Threading::JobRunnerThread&)
					{
						callback(mainSceneJson.GetView());
					},
					Threading::JobPriority::LoadScene
				);
			}
		);

		System::Get<Asset::Manager>().RegisterAsset(nestedSceneAssetGuid, Asset::DatabaseEntry{}, Asset::Identifier{});
		System::Get<Asset::Manager>().RegisterAsyncLoadCallback(
			nestedSceneAssetGuid,
			[](const Guid, const IO::PathView, Threading::JobPriority, IO::AsyncLoadCallback&& callback, const ByteView, const Math::Range<size>)
				-> Optional<Threading::Job*>
			{
				return Threading::CreateCallback(
					[callback = Forward<IO::AsyncLoadCallback>(callback)](Threading::JobRunnerThread&)
					{
						callback(nestedSceneJson.GetView());
					},
					Threading::JobPriority::LoadScene
				);
			}
		);

		{
			Threading::JobBatch jobBatch;
			Entity::SceneRegistry sceneRegistry;
			UniquePtr<Scene> pScene = UniquePtr<Scene>::Make(
				sceneRegistry,
				Optional<Entity::HierarchyComponentBase*>{},
				sceneTemplateIdentifier,
				jobBatch,
				Scene::Flags::IsDisabled
			);

			static Threading::Atomic<bool> finished = false;
			jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[](Threading::JobRunnerThread&)
				{
					finished = true;
				},
				Threading::JobPriority::LoadScene
			));

			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);

			// Run the main thread job runner until we finished loading
			RunMainThreadJobRunner(
				[]()
				{
					return !finished;
				}
			);

			EXPECT_EQ(pScene->GetGuid(), mainSceneAssetGuid);
			EXPECT_EQ(pScene->GetRootComponent().GetInstanceGuid(), "6bd881ba-0be0-4f16-bec4-6c7a19b7bcd8"_guid);
			EXPECT_EQ(pScene->GetRootComponent().GetRadius(), 4096_meters);

			{
				Serialization::Data data(rapidjson::Type::kObjectType, Serialization::ContextFlags::ToDisk);
				Serialization::Writer writer(data);
				const bool wasWritten = writer.SerializeInPlace(*pScene);
				EXPECT_TRUE(wasWritten);

				String writtenJson = data.SaveToBuffer<String>(Serialization::SavingFlags::HumanReadable);
				EXPECT_STREQ((const char*)writtenJson.GetZeroTerminated(), mainSceneJson);
			}

			System::Get<Asset::Manager>().RemoveAsyncLoadCallback(mainSceneAssetGuid);
			System::Get<Asset::Manager>().RemoveAsset(mainSceneAssetGuid);
			System::Get<Asset::Manager>().RemoveAsyncLoadCallback(nestedSceneAssetGuid);
			System::Get<Asset::Manager>().RemoveAsset(nestedSceneAssetGuid);
		}
	}

	FEATURE_TEST(Scene, SceneLoadAndSaveNestedTwice)
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();

		constexpr Asset::Guid mainSceneAssetGuid = "9bb98a4f-098a-4263-8390-06c60b4e2a08"_asset;
		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(mainSceneAssetGuid);

		constexpr Asset::Guid firstNestedSceneAssetGuid = "81a0a6c3-82a7-4596-becb-c5bb61b737be"_asset;
		constexpr Asset::Guid secondNestedSceneAssetGuid = "C8B3EE33-815B-40D0-A022-CEA8FD69B39B"_asset;

		static constexpr ConstZeroTerminatedStringView mainSceneJson = R"({
    "guid": "9bb98a4f-098a-4263-8390-06c60b4e2a08",
    "type_version": 1,
    "typeGuid": "2a2bce0a-6314-481b-a5a0-7d01e1c00b35",
    "instanceGuid": "118f6dde-c793-4938-949c-e0d3870f854c",
    "de8f8da2-d37f-41da-bc6e-580435fe605f": {
        "disabled": true
    },
    "2a2bce0a-6314-481b-a5a0-7d01e1c00b35": {
        "radius": 4096
    },
    "children": [
        {
            "typeGuid": "0c3f4f29-0d53-4878-922a-cca7d6af7bb6",
            "instanceGuid": "1f697243-3c60-a8ca-23d4-c5455fd91b7f",
            "de8f8da2-d37f-41da-bc6e-580435fe605f": {
                "position": [
                    1,
                    2,
                    3
                ]
            },
            "data_components": [
                {
                    "typeGuid": "1c6d4883-40c7-476d-95ee-90a82135974b",
                    "templateInstanceGuid": "498e237e-fa1f-406f-aa4e-08007b4aaa7c"
                }
            ],
            "0c3f4f29-0d53-4878-922a-cca7d6af7bb6": {
                "scene": "81a0a6c3-82a7-4596-becb-c5bb61b737be"
            }
        }
    ]
})";

		static constexpr ConstZeroTerminatedStringView firstNestedSceneJson = R"({
    "guid": "81a0a6c3-82a7-4596-becb-c5bb61b737be",
    "source": "../SC_Buzzy_AR.fbx",
    "assetTypeGuid": "807d69a9-56ec-4032-bb92-d081df336672",
    "typeGuid": "0c3f4f29-0d53-4878-922a-cca7d6af7bb6",
    "instanceGuid": "498e237e-fa1f-406f-aa4e-08007b4aaa7c",
    "de8f8da2-d37f-41da-bc6e-580435fe605f": {
        "disabled": true
    },
    "2a2bce0a-6314-481b-a5a0-7d01e1c00b35": {
        "radius": 4096
    },
    "children": [
        {
            "typeGuid": "0c3f4f29-0d53-4878-922a-cca7d6af7bb6",
            "instanceGuid": "{2505AFF7-F316-40A9-8AD0-D239A3E7FC64}",
            "de8f8da2-d37f-41da-bc6e-580435fe605f": {
                "position": [
                    2,
                    3,
                    4
                ]
            },
            "data_components": [
                {
                    "typeGuid": "1c6d4883-40c7-476d-95ee-90a82135974b",
                    "templateInstanceGuid": "AF83D672-42B6-464D-9CEA-A44F0BBC0FDF"
                }
            ],
            "0c3f4f29-0d53-4878-922a-cca7d6af7bb6": {
                "scene": "C8B3EE33-815B-40D0-A022-CEA8FD69B39B"
            }
        }
    ]
})";

		static constexpr ConstZeroTerminatedStringView secondNestedSceneJson = R"({
    "guid": "{C8B3EE33-815B-40D0-A022-CEA8FD69B39B}",
    "assetTypeGuid": "807d69a9-56ec-4032-bb92-d081df336672",
    "typeGuid": "0c3f4f29-0d53-4878-922a-cca7d6af7bb6",
    "instanceGuid": "{AF83D672-42B6-464D-9CEA-A44F0BBC0FDF}",
    "de8f8da2-d37f-41da-bc6e-580435fe605f": {
        "disabled": true
    },
    "2a2bce0a-6314-481b-a5a0-7d01e1c00b35": {
        "radius": 4096
    },
    "children": [
        {
            "typeGuid": "de8f8da2-d37f-41da-bc6e-580435fe605f",
            "instanceGuid": "{CAAF5DD5-5356-45D4-BBE3-04EA8557F28A}"
        }
    ]
})";

		System::Get<Asset::Manager>().RegisterAsset(mainSceneAssetGuid, Asset::DatabaseEntry{}, Asset::Identifier{});
		System::Get<Asset::Manager>().RegisterAsyncLoadCallback(
			mainSceneAssetGuid,
			[](const Guid, const IO::PathView, Threading::JobPriority, IO::AsyncLoadCallback&& callback, const ByteView, const Math::Range<size>)
				-> Optional<Threading::Job*>
			{
				return Threading::CreateCallback(
					[callback = Forward<IO::AsyncLoadCallback>(callback)](Threading::JobRunnerThread&)
					{
						callback(mainSceneJson.GetView());
					},
					Threading::JobPriority::LoadScene
				);
			}
		);

		System::Get<Asset::Manager>().RegisterAsset(firstNestedSceneAssetGuid, Asset::DatabaseEntry{}, Asset::Identifier{});
		System::Get<Asset::Manager>().RegisterAsyncLoadCallback(
			firstNestedSceneAssetGuid,
			[](const Guid, const IO::PathView, Threading::JobPriority, IO::AsyncLoadCallback&& callback, const ByteView, const Math::Range<size>)
				-> Optional<Threading::Job*>
			{
				return Threading::CreateCallback(
					[callback = Forward<IO::AsyncLoadCallback>(callback)](Threading::JobRunnerThread&)
					{
						callback(firstNestedSceneJson.GetView());
					},
					Threading::JobPriority::LoadScene
				);
			}
		);

		System::Get<Asset::Manager>().RegisterAsset(secondNestedSceneAssetGuid, Asset::DatabaseEntry{}, Asset::Identifier{});
		System::Get<Asset::Manager>().RegisterAsyncLoadCallback(
			secondNestedSceneAssetGuid,
			[](const Guid, const IO::PathView, Threading::JobPriority, IO::AsyncLoadCallback&& callback, const ByteView, const Math::Range<size>)
				-> Optional<Threading::Job*>
			{
				return Threading::CreateCallback(
					[callback = Forward<IO::AsyncLoadCallback>(callback)](Threading::JobRunnerThread&)
					{
						callback(secondNestedSceneJson.GetView());
					},
					Threading::JobPriority::LoadScene
				);
			}
		);

		{
			Threading::JobBatch jobBatch;
			Entity::SceneRegistry sceneRegistry;
			UniquePtr<Scene> pScene = UniquePtr<Scene>::Make(
				sceneRegistry,
				Optional<Entity::HierarchyComponentBase*>{},
				sceneTemplateIdentifier,
				jobBatch,
				Scene::Flags::IsDisabled
			);

			static Threading::Atomic<bool> finished = false;
			jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[](Threading::JobRunnerThread&)
				{
					finished = true;
				},
				Threading::JobPriority::LoadScene
			));

			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);

			// Run the main thread job runner until we finished loading
			RunMainThreadJobRunner(
				[]()
				{
					return !finished;
				}
			);

			EXPECT_EQ(pScene->GetGuid(), mainSceneAssetGuid);
			EXPECT_EQ(pScene->GetRootComponent().GetInstanceGuid(), "118f6dde-c793-4938-949c-e0d3870f854c"_guid);
			EXPECT_EQ(pScene->GetRootComponent().GetRadius(), 4096_meters);

			{
				Serialization::Data data(rapidjson::Type::kObjectType, Serialization::ContextFlags::ToDisk);
				Serialization::Writer writer(data);
				const bool wasWritten = writer.SerializeInPlace(*pScene);
				EXPECT_TRUE(wasWritten);

				String writtenJson = data.SaveToBuffer<String>(Serialization::SavingFlags::HumanReadable);
				EXPECT_STREQ((const char*)writtenJson.GetZeroTerminated(), mainSceneJson);
			}

			System::Get<Asset::Manager>().RemoveAsyncLoadCallback(mainSceneAssetGuid);
			System::Get<Asset::Manager>().RemoveAsset(mainSceneAssetGuid);
			System::Get<Asset::Manager>().RemoveAsyncLoadCallback(firstNestedSceneAssetGuid);
			System::Get<Asset::Manager>().RemoveAsset(firstNestedSceneAssetGuid);
			System::Get<Asset::Manager>().RemoveAsyncLoadCallback(secondNestedSceneAssetGuid);
			System::Get<Asset::Manager>().RemoveAsset(secondNestedSceneAssetGuid);
		}
	}

	FEATURE_TEST(Scene, SceneLoadAndSaveDataComponents)
	{
		UniquePtr<Entity::ComponentType<DataComponentNoProperties>> pNoPropertiesComponentType =
			UniquePtr<Entity::ComponentType<DataComponentNoProperties>>::Make();
		System::Get<Entity::Manager>().GetRegistry().Register(pNoPropertiesComponentType.Get());
		System::Get<Reflection::Registry>().RegisterDynamicType<DataComponentNoProperties>();

		UniquePtr<Entity::ComponentType<DataComponentWithProperties>> pPropertiesComponentType =
			UniquePtr<Entity::ComponentType<DataComponentWithProperties>>::Make();
		System::Get<Entity::Manager>().GetRegistry().Register(pPropertiesComponentType.Get());
		System::Get<Reflection::Registry>().RegisterDynamicType<DataComponentWithProperties>();

		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();

		constexpr Asset::Guid assetGuid = "fe79cedf-f34e-40e8-83a7-ac684e5d6a51"_asset;
		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(assetGuid);

		static constexpr ConstZeroTerminatedStringView sceneJson = R"({
    "guid": "fe79cedf-f34e-40e8-83a7-ac684e5d6a51",
    "type_version": 1,
    "typeGuid": "2a2bce0a-6314-481b-a5a0-7d01e1c00b35",
    "instanceGuid": "e49c5f4b-9b6e-4fd9-b635-7ff4507aa350",
    "de8f8da2-d37f-41da-bc6e-580435fe605f": {
        "disabled": true
    },
    "2a2bce0a-6314-481b-a5a0-7d01e1c00b35": {
        "radius": 4096
    },
    "data_components": [
        {
            "typeGuid": "748ce24b-c9b0-4b0d-98f4-83da6a77d9e1"
        },
        {
            "typeGuid": "42f66b83-8c3a-4830-bbd8-379dc4f6752d",
            "name": "TEST"
        }
    ],
    "children": [
        {
            "typeGuid": "de8f8da2-d37f-41da-bc6e-580435fe605f",
            "instanceGuid": "94bea596-05d3-4bab-b46d-1b80aedc8e44",
            "de8f8da2-d37f-41da-bc6e-580435fe605f": {
                "position": [
                    1,
                    2,
                    3
                ]
            },
            "data_components": [
                {
                    "typeGuid": "1c6d4883-40c7-476d-95ee-90a82135974b",
                    "templateInstanceGuid": "94bea596-05d3-4bab-b46d-1b80aedc8e44"
                },
                {
                    "typeGuid": "748ce24b-c9b0-4b0d-98f4-83da6a77d9e1"
                },
                {
                    "typeGuid": "42f66b83-8c3a-4830-bbd8-379dc4f6752d",
                    "name": "TEST CHILD"
                }
            ]
        }
    ]
})";

		System::Get<Asset::Manager>().RegisterAsset(assetGuid, Asset::DatabaseEntry{}, Asset::Identifier{});

		System::Get<Asset::Manager>().RegisterAsyncLoadCallback(
			assetGuid,
			[](const Guid, const IO::PathView, Threading::JobPriority, IO::AsyncLoadCallback&& callback, const ByteView, const Math::Range<size>)
				-> Optional<Threading::Job*>
			{
				return Threading::CreateCallback(
					[callback = Forward<IO::AsyncLoadCallback>(callback)](Threading::JobRunnerThread&)
					{
						callback(sceneJson.GetView());
					},
					Threading::JobPriority::LoadScene
				);
			}
		);

		{
			Threading::JobBatch jobBatch;
			Entity::SceneRegistry sceneRegistry;
			UniquePtr<Scene> pScene = UniquePtr<Scene>::Make(
				sceneRegistry,
				Optional<Entity::HierarchyComponentBase*>{},
				sceneTemplateIdentifier,
				jobBatch,
				Scene::Flags::IsDisabled
			);

			static Threading::Atomic<bool> finished = false;
			jobBatch.QueueAsNewFinishedStage(Threading::CreateCallback(
				[](Threading::JobRunnerThread&)
				{
					finished = true;
				},
				Threading::JobPriority::LoadScene
			));

			Threading::JobRunnerThread::GetCurrent()->Queue(jobBatch);

			// Run the main thread job runner until we finished loading
			RunMainThreadJobRunner(
				[]()
				{
					return !finished;
				}
			);

			EXPECT_EQ(pScene->GetGuid(), assetGuid);
			EXPECT_EQ(pScene->GetRootComponent().GetInstanceGuid(), "e49c5f4b-9b6e-4fd9-b635-7ff4507aa350"_guid);
			EXPECT_EQ(pScene->GetRootComponent().GetRadius(), 4096_meters);
			EXPECT_TRUE(pScene->GetRootComponent().HasAnyDataComponents(sceneRegistry));
			EXPECT_TRUE(pScene->GetRootComponent().HasDataComponentOfType<DataComponentNoProperties>(sceneRegistry));
			EXPECT_TRUE(pScene->GetRootComponent().HasDataComponentOfType<DataComponentWithProperties>(sceneRegistry));
			EXPECT_EQ(
				pScene->GetRootComponent().FindDataComponentOfType<DataComponentWithProperties>(sceneRegistry)->m_name,
				MAKE_UNICODE_LITERAL("TEST")
			);

			EXPECT_TRUE(pScene->GetRootComponent().HasChildren());

			Entity::Component3D& childComponent = pScene->GetRootComponent().GetChild(0);
			EXPECT_TRUE(childComponent.HasDataComponentOfType<DataComponentNoProperties>(sceneRegistry));
			EXPECT_TRUE(childComponent.HasDataComponentOfType<DataComponentWithProperties>(sceneRegistry));
			EXPECT_EQ(childComponent.FindDataComponentOfType<DataComponentWithProperties>()->m_name, MAKE_UNICODE_LITERAL("TEST CHILD"));

			{
				Serialization::Data data(rapidjson::Type::kObjectType, Serialization::ContextFlags::ToDisk);
				Serialization::Writer writer(data);
				const bool wasWritten = writer.SerializeInPlace(*pScene);
				EXPECT_TRUE(wasWritten);

				String writtenJson = data.SaveToBuffer<String>(Serialization::SavingFlags::HumanReadable);
				EXPECT_STREQ((const char*)writtenJson.GetZeroTerminated(), sceneJson);
			}
		}

		System::Get<Asset::Manager>().RemoveAsyncLoadCallback(assetGuid);
		System::Get<Asset::Manager>().RemoveAsset(assetGuid);

		System::Get<Reflection::Registry>().DeregisterDynamicType<DataComponentNoProperties>();
		System::Get<Entity::Manager>().GetRegistry().Deregister(Reflection::GetTypeGuid<DataComponentNoProperties>());
		System::Get<Reflection::Registry>().DeregisterDynamicType<DataComponentWithProperties>();
		System::Get<Entity::Manager>().GetRegistry().Deregister(Reflection::GetTypeGuid<DataComponentWithProperties>());
	}
}
