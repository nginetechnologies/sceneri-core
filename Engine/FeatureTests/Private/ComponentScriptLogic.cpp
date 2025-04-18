#include <Common/Memory/New.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/ComponentTypeIdentifier.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Tests/FeatureTest.h>

#include <Engine/Scripting/Parser/Token.h>
#include <Engine/Scripting/Parser/Lexer.h>
#include <Engine/Scripting/Parser/Parser.h>
#include <Engine/Scripting/Parser/AST/Statement.h>
#include <Engine/Scripting/Parser/AST/Expression.h>
#include <Engine/Scripting/Compiler/Compiler.h>
#include <Engine/Scripting/VirtualMachine/VirtualMachine.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/DynamicDelegate.h>

#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Serialization/SerializedData.h>
#include <Common/Serialization/Writer.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Tests
{
	[[nodiscard]] UniquePtr<Scripting::FunctionObject> Compile(const Scripting::AST::Graph& astGraph)
	{
		const Scripting::AST::Node& entryPointNode = astGraph.GetNodes().GetLastElement();
		EXPECT_TRUE(entryPointNode.IsStatement());
		const Scripting::AST::Statement::Base& entryPointStatement = static_cast<const Scripting::AST::Statement::Base&>(entryPointNode);

		EXPECT_TRUE(entryPointStatement.GetType() == Scripting::AST::NodeType::Block);

		Scripting::Compiler compiler;
		UniquePtr<Scripting::FunctionObject> pFunction = compiler.Compile(entryPointStatement);
		Assert(pFunction.IsValid());
		if (pFunction.IsInvalid())
		{
			return nullptr;
		}

		if (!compiler.ResolveFunction(*pFunction))
		{
			return nullptr;
		}

		return pFunction;
	}

	FEATURE_TEST(Scene, ComponentLogicScript_Simple)
	{
		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();

		constexpr Asset::Guid mainSceneAssetGuid = "309c1ecb-8003-41e5-a475-cbc01164bf05"_asset;
		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(mainSceneAssetGuid);

		static constexpr ConstZeroTerminatedStringView mainSceneJson = R"({
    "guid": "309c1ecb-8003-41e5-a475-cbc01164bf05",
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
            "typeGuid": "de8f8da2-d37f-41da-bc6e-580435fe605f",
            "instanceGuid": "5c8f80ca-e4fc-418a-a6aa-6bf110800860",
            "data_components": [
                {
                    "typeGuid": "1c6d4883-40c7-476d-95ee-90a82135974b",
                    "templateInstanceGuid": "4608f70f-ba69-4fc4-99ae-0afa786986df"
                }
            ]
        }
    ]
})";
		static constexpr ConstZeroTerminatedStringView expectedMainSceneJson = R"({
    "guid": "309c1ecb-8003-41e5-a475-cbc01164bf05",
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
            "typeGuid": "de8f8da2-d37f-41da-bc6e-580435fe605f",
            "instanceGuid": "5c8f80ca-e4fc-418a-a6aa-6bf110800860",
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
                    "templateInstanceGuid": "4608f70f-ba69-4fc4-99ae-0afa786986df"
                }
            ]
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
			EXPECT_EQ(pScene->GetRootComponent().GetChildren()[0].GetInstanceGuid(), "5c8f80ca-e4fc-418a-a6aa-6bf110800860"_guid);

			Scripting::Lexer lexer;
			lexer.EmplaceKnownIdentifier(
				"SetRelativeLocation",
				Reflection::GetFunctionGuid<(void(Entity::Component3D::*)(Math::Vector3f)) & Entity::Component3D::SetRelativeLocation>()
			);

			Scripting::TokenListType tokens;
			EXPECT_TRUE(lexer.ScanTokens(
				SCRIPT_STRING_LITERAL(R"(
				local component: component_soft_ref = component_soft_ref{"de8f8da2-d37f-41da-bc6e-580435fe605f", "5c8f80ca-e4fc-418a-a6aa-6bf110800860"};
				component.SetRelativeLocation(vec3f{1, 2, 3});
			)"),
				tokens
			));

			Scripting::Parser parser;
			Optional<Scripting::AST::Graph> astGraph = parser.Parse(tokens);
			EXPECT_TRUE(astGraph.IsValid());

			UniquePtr<Scripting::FunctionObject> pScript = Compile(*astGraph);

			UniquePtr<Scripting::VirtualMachine> pVirtualMachine = UniquePtr<Scripting::VirtualMachine>::Make();
			pVirtualMachine->SetEntitySceneRegistry(pScene->GetEntitySceneRegistry());
			pVirtualMachine->Initialize(*pScript);
			EXPECT_TRUE(pVirtualMachine->Execute());

			{
				Serialization::Data data(rapidjson::Type::kObjectType, Serialization::ContextFlags::ToDisk);
				Serialization::Writer writer(data);
				const bool wasWritten = writer.SerializeInPlace(*pScene);
				EXPECT_TRUE(wasWritten);

				String writtenJson = data.SaveToBuffer<String>(Serialization::SavingFlags::HumanReadable);
				EXPECT_STREQ((const char*)writtenJson.GetZeroTerminated(), expectedMainSceneJson);
			}

			System::Get<Asset::Manager>().RemoveAsyncLoadCallback(mainSceneAssetGuid);
			System::Get<Asset::Manager>().RemoveAsset(mainSceneAssetGuid);
		}
	}
}
