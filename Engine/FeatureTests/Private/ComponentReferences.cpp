#include <Common/Memory/New.h>

#include <Engine/Entity/ComponentReference.h>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Entity/ComponentSoftReference.inl>
#include <Engine/Entity/ComponentValue.inl>
#include <Engine/Entity/Serialization/ComponentValue.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Scene/SceneChildInstance.h>
#include <Engine/Tests/FeatureTest.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/RootSceneComponent.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Tests
{
	struct ReferencesTestComponent final : public Entity::Component3D
	{
		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		using BaseType::BaseType;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::ReferencesTestComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::ReferencesTestComponent>(
			"{D63C89EC-9702-41F3-A673-253B5CC33928}"_guid, MAKE_UNICODE_LITERAL("References Test Component")
		);
	};
}

namespace ngine::Tests
{
	FEATURE_TEST(Components, ComponentReferences)
	{
		UniquePtr<Entity::ComponentType<ReferencesTestComponent>> pReferencesComponentType =
			UniquePtr<Entity::ComponentType<ReferencesTestComponent>>::Make();
		System::Get<Entity::Manager>().GetRegistry().Register(pReferencesComponentType.Get());
		System::Get<Reflection::Registry>().RegisterDynamicType<ReferencesTestComponent>();

		{
			Entity::SceneRegistry sceneRegistry;
			UniquePtr<Scene> pScene = UniquePtr<Scene>::Make(
				sceneRegistry,
				Optional<Entity::HierarchyComponentBase*>{},
				1024_meters,
				"{99578F73-64BB-40CA-BBAB-891378B21701}"_guid,
				Scene::Flags::IsDisabled
			);

			const Entity::ComponentValue<ReferencesTestComponent> pTestComponent{
				sceneRegistry,
				ReferencesTestComponent::Initializer{pScene->GetRootComponent()}
			};
			EXPECT_TRUE(pTestComponent.IsValid());
			if (pTestComponent.IsValid())
			{
				// Write to buffer
				{
					Serialization::Data serializedData(Serialization::ContextFlags::UseWithinSessionInstance);
					{
						const Entity::ComponentReference<ReferencesTestComponent> pReferencedComponent = *pTestComponent;
						EXPECT_TRUE(pReferencedComponent.IsValid());
						EXPECT_EQ(pReferencedComponent.Get(), pTestComponent.Get());

						Serialization::Writer writer(serializedData);
						const bool wroteComponent = writer.SerializeInPlace(pReferencedComponent);
						EXPECT_TRUE(wroteComponent);
					}

					Serialization::Reader reader(serializedData);
					const Optional<Entity::ComponentReference<ReferencesTestComponent>> pReadComponent =
						reader.ReadInPlace<Entity::ComponentReference<ReferencesTestComponent>>(sceneRegistry);
					EXPECT_TRUE(pReadComponent.IsValid());
					if (pReadComponent.IsValid())
					{
						EXPECT_EQ(pReadComponent->Get(), pTestComponent.Get());
					}
				}

				// Write to disk
				{
					Serialization::Data serializedData(Serialization::ContextFlags::ToDisk | Serialization::ContextFlags::FromDisk);
					{
						const Entity::ComponentReference<ReferencesTestComponent> pReferencedComponent = *pTestComponent;
						EXPECT_TRUE(pReferencedComponent.IsValid());
						EXPECT_EQ(pReferencedComponent.Get(), pTestComponent.Get());

						Serialization::Writer writer(serializedData);
						const bool wroteComponent = writer.SerializeInPlace(pReferencedComponent);
						EXPECT_TRUE(wroteComponent);
					}

					Serialization::Reader reader(serializedData);
					const Optional<Entity::ComponentReference<ReferencesTestComponent>> pReadComponent =
						reader.ReadInPlace<Entity::ComponentReference<ReferencesTestComponent>>(sceneRegistry);
					EXPECT_TRUE(pReadComponent.IsValid());
					if (pReadComponent.IsValid())
					{
						EXPECT_EQ(pReadComponent->Get(), pTestComponent.Get());
					}
				}
			}
		}

		System::Get<Reflection::Registry>().DeregisterDynamicType<ReferencesTestComponent>();
		System::Get<Entity::Manager>().GetRegistry().Deregister(Reflection::GetTypeGuid<ReferencesTestComponent>());
	}

	struct ValuesTestComponent final : public Entity::Component3D
	{
		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		using BaseType::BaseType;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::ValuesTestComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::ValuesTestComponent>(
			"{0C7853FB-E409-4052-9FDE-B54E0BFCD6BA}"_guid, MAKE_UNICODE_LITERAL("Values Test Component")
		);
	};
}

namespace ngine::Tests
{
	FEATURE_TEST(Components, ComponentValues)
	{
		System::Get<Reflection::Registry>().RegisterDynamicType<ValuesTestComponent>();
		UniquePtr<Entity::ComponentType<ValuesTestComponent>> pComponentType = UniquePtr<Entity::ComponentType<ValuesTestComponent>>::Make();
		System::Get<Entity::Manager>().GetRegistry().Register(pComponentType.Get());

		{
			Entity::SceneRegistry sceneRegistry;
			UniquePtr<Scene> pScene = UniquePtr<Scene>::Make(
				sceneRegistry,
				Optional<Entity::HierarchyComponentBase*>{},
				1024_meters,
				"{F67FC77D-A7BD-45CB-9DD2-A1D20ADF6EE0}"_guid,
				Scene::Flags::IsDisabled
			);
			Entity::SceneRegistry readerSceneRegistry;
			UniquePtr<Scene> pReaderScene = UniquePtr<Scene>::Make(
				readerSceneRegistry,
				Optional<Entity::HierarchyComponentBase*>{},
				1024_meters,
				"{609FD442-31E4-43A7-8AA1-630093B3F8C5}"_guid,
				Scene::Flags::IsDisabled
			);
			Entity::SceneRegistry writerSceneRegistry;
			UniquePtr<Scene> pWriterScene = UniquePtr<Scene>::Make(
				writerSceneRegistry,
				Optional<Entity::HierarchyComponentBase*>{},
				1024_meters,
				"{382C713C-7C45-48F0-9655-93FC21072F41}"_guid,
				Scene::Flags::IsDisabled
			);

			const Entity::ComponentValue<ValuesTestComponent> pTestComponent{
				sceneRegistry,
				ValuesTestComponent::Initializer{pScene->GetRootComponent()}
			};
			EXPECT_TRUE(pTestComponent.IsValid());
			if (pTestComponent.IsValid())
			{
				pTestComponent->EnableSaveToDisk();

				// Writer to buffer
				{
					Serialization::Data serializedData(Serialization::ContextFlags::UseWithinSessionInstance);
					{
						Serialization::Writer writer(serializedData);
						const bool wroteComponent = writer.SerializeInPlace(pTestComponent);
						EXPECT_TRUE(wroteComponent);
					}

					Serialization::Reader reader(serializedData);
					const Optional<Entity::ComponentValue<ValuesTestComponent>> pReadComponent =
						reader.ReadInPlace<Entity::ComponentValue<ValuesTestComponent>>(
							pReaderScene->GetRootComponent(),
							pReaderScene->GetEntitySceneRegistry()
						);
					EXPECT_TRUE(pReadComponent.IsValid());
					if (pReadComponent.IsValid())
					{
						EXPECT_TRUE(pReadComponent->IsValid());
						EXPECT_NE(pReadComponent->Get(), pTestComponent.Get());
						EXPECT_EQ(pReadComponent->Get()->GetTypeGuid(), pTestComponent->GetTypeGuid());
					}
				}

				// Writer to disk
				{
					Serialization::Data serializedData(Serialization::ContextFlags::ToDisk | Serialization::ContextFlags::FromDisk);
					{
						Serialization::Writer writer(serializedData);
						const bool wroteComponent = writer.SerializeInPlace(pTestComponent);
						EXPECT_TRUE(wroteComponent);
					}

					Serialization::Reader reader(serializedData);
					const Optional<Entity::ComponentValue<ValuesTestComponent>> pReadComponent =
						reader.ReadInPlace<Entity::ComponentValue<ValuesTestComponent>>(
							pWriterScene->GetRootComponent(),
							pWriterScene->GetEntitySceneRegistry()
						);
					EXPECT_TRUE(pReadComponent.IsValid());
					if (pReadComponent.IsValid())
					{
						EXPECT_TRUE(pReadComponent->IsValid());
						EXPECT_NE(pReadComponent->Get(), pTestComponent.Get());
						EXPECT_EQ(pReadComponent->Get()->GetTypeGuid(), pTestComponent->GetTypeGuid());
					}
				}
			}
		}

		System::Get<Reflection::Registry>().DeregisterDynamicType<ValuesTestComponent>();
		System::Get<Entity::Manager>().GetRegistry().Deregister(Reflection::GetTypeGuid<ValuesTestComponent>());
	}

	struct SoftReferencesTestComponent3D final : public Entity::Component3D
	{
		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		using BaseType::BaseType;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::SoftReferencesTestComponent3D>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::SoftReferencesTestComponent3D>(
			"{C0B64B84-DF24-4588-96DC-64EF92A89E7C}"_guid,
			MAKE_UNICODE_LITERAL("Soft References Test Component 3D"),
			TypeFlags::DisableDynamicCloning
		);
	};
}

namespace ngine::Tests
{
	FEATURE_TEST(Components, ComponentSoftReference3D)
	{
		System::Get<Reflection::Registry>().RegisterDynamicType<SoftReferencesTestComponent3D>();
		UniquePtr<Entity::ComponentType<SoftReferencesTestComponent3D>> pComponentType =
			UniquePtr<Entity::ComponentType<SoftReferencesTestComponent3D>>::Make();
		System::Get<Entity::Manager>().GetRegistry().Register(pComponentType.Get());

		{
			Entity::SceneRegistry sceneRegistry;
			UniquePtr<Scene> pScene = UniquePtr<Scene>::Make(
				sceneRegistry,
				Optional<Entity::HierarchyComponentBase*>{},
				1024_meters,
				"{148DF33D-2862-4B3A-9615-88A4624BB7A1}"_guid,
				Scene::Flags::IsDisabled
			);

			Entity::ComponentValue<SoftReferencesTestComponent3D> pTestComponent{
				sceneRegistry,
				SoftReferencesTestComponent3D::Initializer{pScene->GetRootComponent()}
			};
			EXPECT_TRUE(pTestComponent.IsValid());
			if (pTestComponent.IsValid())
			{
				// Serialize to / from buffer
				{
					Serialization::Data serializedData(Serialization::ContextFlags::UseWithinSessionInstance);
					{
						const Entity::ComponentSoftReference componentReference{*pTestComponent, sceneRegistry};
						EXPECT_TRUE(componentReference.IsPotentiallyValid());

						const Optional<SoftReferencesTestComponent3D*> pComponent = componentReference.Find<SoftReferencesTestComponent3D>(sceneRegistry
						);
						EXPECT_TRUE(pComponent.IsValid());
						EXPECT_EQ(pComponent.Get(), pTestComponent.Get());

						Serialization::Writer writer(serializedData);
						const bool wroteComponent = writer.SerializeInPlace(componentReference);
						EXPECT_TRUE(wroteComponent);
					}

					Serialization::Reader reader(serializedData);
					const Optional<Entity::ComponentSoftReference> pReadComponentSoftReference =
						reader.ReadInPlace<Entity::ComponentSoftReference>(sceneRegistry);
					EXPECT_TRUE(pReadComponentSoftReference.IsValid());
					if (pReadComponentSoftReference.IsValid())
					{
						EXPECT_TRUE(pReadComponentSoftReference->IsPotentiallyValid());

						const Optional<SoftReferencesTestComponent3D*> pComponent =
							pReadComponentSoftReference->Find<SoftReferencesTestComponent3D>(sceneRegistry);
						EXPECT_TRUE(pComponent.IsValid());
						EXPECT_EQ(pComponent.Get(), pTestComponent.Get());
					}
				}

				// Serialize to / from disk
				{
					Serialization::Data serializedData(Serialization::ContextFlags::ToDisk | Serialization::ContextFlags::FromDisk);
					{
						const Entity::ComponentSoftReference componentReference{*pTestComponent, sceneRegistry};
						EXPECT_TRUE(componentReference.IsPotentiallyValid());

						const Optional<SoftReferencesTestComponent3D*> pComponent = componentReference.Find<SoftReferencesTestComponent3D>(sceneRegistry
						);
						EXPECT_TRUE(pComponent.IsValid());
						EXPECT_EQ(pComponent.Get(), pTestComponent.Get());

						Serialization::Writer writer(serializedData);
						const bool wroteComponent = writer.SerializeInPlace(componentReference);
						EXPECT_TRUE(wroteComponent);
					}

					Serialization::Reader reader(serializedData);
					const Optional<Entity::ComponentSoftReference> pReadComponentSoftReference =
						reader.ReadInPlace<Entity::ComponentSoftReference>(sceneRegistry);
					EXPECT_TRUE(pReadComponentSoftReference.IsValid());
					if (pReadComponentSoftReference.IsValid())
					{
						EXPECT_TRUE(pReadComponentSoftReference->IsPotentiallyValid());

						const Optional<SoftReferencesTestComponent3D*> pComponent =
							pReadComponentSoftReference->Find<SoftReferencesTestComponent3D>(sceneRegistry);
						EXPECT_TRUE(pComponent.IsValid());
						EXPECT_EQ(pComponent.Get(), pTestComponent.Get());
					}
				}
			}
		}

		System::Get<Reflection::Registry>().DeregisterDynamicType<SoftReferencesTestComponent3D>();
		System::Get<Entity::Manager>().GetRegistry().Deregister(Reflection::GetTypeGuid<SoftReferencesTestComponent3D>());
	}

	struct CrossRegistrySoftReferencesTestDataComponent final : public Entity::Data::Component
	{
		using BaseType = Entity::Data::Component;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		CrossRegistrySoftReferencesTestDataComponent(const Deserializer& deserializer)
			: m_softReference(*deserializer.m_reader.Read<Entity::ComponentSoftReference>("soft_reference", deserializer.GetSceneRegistry()))
		{
			EXPECT_TRUE(m_softReference.IsPotentiallyValid());
		}

		CrossRegistrySoftReferencesTestDataComponent(
			const CrossRegistrySoftReferencesTestDataComponent& templateComponent, const Cloner& cloner
		)
			: m_softReference{
					templateComponent.m_softReference,
					Entity::ComponentSoftReference::Cloner{cloner.GetTemplateSceneRegistry(), cloner.GetSceneRegistry()}
				}
		{
			EXPECT_TRUE(m_softReference.IsPotentiallyValid());
		}

		Entity::ComponentSoftReference m_softReference;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::CrossRegistrySoftReferencesTestDataComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::CrossRegistrySoftReferencesTestDataComponent>(
			"0a874119-0984-4779-8255-023e3c76146e"_guid,
			MAKE_UNICODE_LITERAL("Soft References Test Data Component"),
			TypeFlags::DisableDynamicInstantiation

		);
	};
}

namespace ngine::Tests
{
	FEATURE_TEST(Components, ComponentSoftReference3DCrossRegistry)
	{
		System::Get<Reflection::Registry>().RegisterDynamicType<SoftReferencesTestComponent3D>();
		UniquePtr<Entity::ComponentType<SoftReferencesTestComponent3D>> p3DComponentType =
			UniquePtr<Entity::ComponentType<SoftReferencesTestComponent3D>>::Make();
		System::Get<Entity::Manager>().GetRegistry().Register(p3DComponentType.Get());

		System::Get<Reflection::Registry>().RegisterDynamicType<CrossRegistrySoftReferencesTestDataComponent>();
		UniquePtr<Entity::ComponentType<CrossRegistrySoftReferencesTestDataComponent>> pComponentType =
			UniquePtr<Entity::ComponentType<CrossRegistrySoftReferencesTestDataComponent>>::Make();
		System::Get<Entity::Manager>().GetRegistry().Register(pComponentType.Get());

		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();

		constexpr Asset::Guid mainSceneAssetGuid = "7672a026-8b3b-48bf-8a76-e79bf8c1a8ed"_asset;
		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier = sceneTemplateCache.FindOrRegister(mainSceneAssetGuid);

		constexpr Asset::Guid nestedSceneAssetGuid = "9f9d7990-be73-4be8-81dc-be442a466e4e"_asset;

		static constexpr ConstZeroTerminatedStringView mainSceneJson = R"({
    "guid": "7672a026-8b3b-48bf-8a76-e79bf8c1a8ed",
    "type_version": 1,
    "typeGuid": "2a2bce0a-6314-481b-a5a0-7d01e1c00b35",
    "instanceGuid": "4e99a7db-9063-489b-826d-2ca937465270",
    "children": [
        {
            "typeGuid": "C0B64B84-DF24-4588-96DC-64EF92A89E7C"
        },
        {
            "typeGuid": "0c3f4f29-0d53-4878-922a-cca7d6af7bb6",
            "instanceGuid": "92a2cab0-43a5-4a09-aa7b-6f3d41306c43",
            "0c3f4f29-0d53-4878-922a-cca7d6af7bb6": {
                "scene": "9f9d7990-be73-4be8-81dc-be442a466e4e"
            },
            "data_components": [
                {
                    "typeGuid": "0a874119-0984-4779-8255-023e3c76146e",
                    "soft_reference": {
                        "typeGuid": "2a2bce0a-6314-481b-a5a0-7d01e1c00b35",
                        "instanceGuid": "4e99a7db-9063-489b-826d-2ca937465270"
                    }
                }
            ]
        }
    ]
})";

		static constexpr ConstZeroTerminatedStringView nestedSceneJson = R"({
    "guid": "9f9d7990-be73-4be8-81dc-be442a466e4e",
    "assetTypeGuid": "807d69a9-56ec-4032-bb92-d081df336672",
    "typeGuid": "0c3f4f29-0d53-4878-922a-cca7d6af7bb6",
    "instanceGuid": "2ef88c13-336a-49db-89a3-adb6a04d9d95",
    "children": [
        {
            "typeGuid": "C0B64B84-DF24-4588-96DC-64EF92A89E7C"
        },
        {
            "typeGuid": "de8f8da2-d37f-41da-bc6e-580435fe605f",
            "instanceGuid": "3acb25f7-e1eb-483c-aac8-4078242a13bd",
            "data_components": [
                {
                    "typeGuid": "0a874119-0984-4779-8255-023e3c76146e",
                    "soft_reference": {
                        "typeGuid": "0c3f4f29-0d53-4878-922a-cca7d6af7bb6",
                        "instanceGuid": "92a2cab0-43a5-4a09-aa7b-6f3d41306c43"
                    }
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

			const Entity::Component3D& rootComponent = pScene->GetRootComponent();
			const Entity::Component3D& firstSoftReferenceOwner = rootComponent.GetChild(0);
			const Optional<Entity::SceneChildInstance*> pFirstSceneChildInstance =
				firstSoftReferenceOwner.FindDataComponentOfType<Entity::SceneChildInstance>();
			EXPECT_EQ(pFirstSceneChildInstance->m_sourceTemplateInstanceGuid, "92a2cab0-43a5-4a09-aa7b-6f3d41306c43"_guid);
			// EXPECT_NE(pFirstSceneChildInstance->m_sourceTemplateInstanceGuid, firstSoftReferenceOwner.GetInstanceGuid());
			const Optional<CrossRegistrySoftReferencesTestDataComponent*> pFirstSoftReferenceDataComponent =
				firstSoftReferenceOwner.FindDataComponentOfType<CrossRegistrySoftReferencesTestDataComponent>();
			EXPECT_TRUE(pFirstSoftReferenceDataComponent->m_softReference.IsPotentiallyValid());
			EXPECT_EQ(pFirstSoftReferenceDataComponent->m_softReference.Find<Entity::Component3D>(sceneRegistry), &rootComponent);

			const Entity::Component3D& secondSoftReferenceOwner = firstSoftReferenceOwner.GetChild(0);
			const Optional<Entity::SceneChildInstance*> pSecondSceneChildInstance =
				secondSoftReferenceOwner.FindDataComponentOfType<Entity::SceneChildInstance>();
			EXPECT_EQ(pSecondSceneChildInstance->m_sourceTemplateInstanceGuid, "3acb25f7-e1eb-483c-aac8-4078242a13bd"_guid);
			EXPECT_NE(pSecondSceneChildInstance->m_sourceTemplateInstanceGuid, secondSoftReferenceOwner.GetInstanceGuid());
			const Optional<CrossRegistrySoftReferencesTestDataComponent*> pSecondSoftReferenceDataComponent =
				secondSoftReferenceOwner.FindDataComponentOfType<CrossRegistrySoftReferencesTestDataComponent>();
			EXPECT_TRUE(pSecondSoftReferenceDataComponent->m_softReference.IsPotentiallyValid());
			EXPECT_EQ(pSecondSoftReferenceDataComponent->m_softReference.Find<Entity::Component3D>(sceneRegistry), &firstSoftReferenceOwner);
		}

		System::Get<Asset::Manager>().RemoveAsyncLoadCallback(mainSceneAssetGuid);
		System::Get<Asset::Manager>().RemoveAsset(mainSceneAssetGuid);
		System::Get<Asset::Manager>().RemoveAsyncLoadCallback(nestedSceneAssetGuid);
		System::Get<Asset::Manager>().RemoveAsset(nestedSceneAssetGuid);

		System::Get<Reflection::Registry>().DeregisterDynamicType<SoftReferencesTestComponent3D>();
		System::Get<Entity::Manager>().GetRegistry().Deregister(Reflection::GetTypeGuid<SoftReferencesTestComponent3D>());
		System::Get<Reflection::Registry>().DeregisterDynamicType<CrossRegistrySoftReferencesTestDataComponent>();
		System::Get<Entity::Manager>().GetRegistry().Deregister(Reflection::GetTypeGuid<CrossRegistrySoftReferencesTestDataComponent>());
	}

	struct SoftReferencesTestDataComponent final : public Entity::Data::Component
	{
		using BaseType = Entity::Data::Component;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

		using BaseType::BaseType;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::SoftReferencesTestDataComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::SoftReferencesTestDataComponent>(
			"dce21d44-fd65-4982-b2f5-9f3b5c90d8c5"_guid, MAKE_UNICODE_LITERAL("Soft References Test Data Component")
		);
	};
}

namespace ngine::Tests
{
	FEATURE_TEST(Components, ComponentSoftReferenceData)
	{
		System::Get<Reflection::Registry>().RegisterDynamicType<SoftReferencesTestDataComponent>();
		UniquePtr<Entity::ComponentType<SoftReferencesTestDataComponent>> pComponentType =
			UniquePtr<Entity::ComponentType<SoftReferencesTestDataComponent>>::Make();
		System::Get<Entity::Manager>().GetRegistry().Register(pComponentType.Get());

		{
			Entity::SceneRegistry sceneRegistry;
			UniquePtr<Scene> pScene = UniquePtr<Scene>::Make(
				sceneRegistry,
				Optional<Entity::HierarchyComponentBase*>{},
				1024_meters,
				"{148DF33D-2862-4B3A-9615-88A4624BB7A1}"_guid,
				Scene::Flags::IsDisabled
			);

			Entity::ComponentValue<Entity::Component3D> pOwningComponent{
				sceneRegistry,
				Entity::Component3D::Initializer{pScene->GetRootComponent()}
			};
			EXPECT_TRUE(pOwningComponent.IsValid());
			if (pOwningComponent.IsValid())
			{
				const Optional<SoftReferencesTestDataComponent*> pDataComponent =
					pOwningComponent->CreateDataComponent<SoftReferencesTestDataComponent>(
						SoftReferencesTestDataComponent::Initializer{*pOwningComponent, sceneRegistry}
					);
				EXPECT_TRUE(pDataComponent.IsValid());

				// TODO: Support data compoennts in soft references

				// Serialize to / from buffer
				/*{
				  Serialization::Data serializedData(Serialization::ContextFlags::UseWithinSessionInstance);
				  {
				    const Entity::ComponentSoftReference componentReference{*pDataComponent, sceneRegistry};
				    EXPECT_TRUE(componentReference.IsPotentiallyValid());

				    const Optional<Entity::Component3D*> pComponent =
				      componentReference.Find<Entity::Component3D>(sceneRegistry);
				    EXPECT_TRUE(pComponent.IsValid());
				    EXPECT_EQ(pComponent.Get(), pOwningComponent.Get());

				    Serialization::Writer writer(serializedData);
				    const bool wroteComponent = writer.SerializeInPlace(componentReference, System::Get<Entity::Manager>().GetRegistry());
				    EXPECT_TRUE(wroteComponent);
				  }

				  Serialization::Reader reader(serializedData);
				  const Optional<Entity::ComponentSoftReference> pReadComponentSoftReference = reader.ReadInPlace<Entity::ComponentSoftReference>(
				    System::Get<Entity::Manager>().GetRegistry(),
				    sceneRegistry
				  );
				  EXPECT_TRUE(pReadComponentSoftReference.IsValid());
				  if (pReadComponentSoftReference.IsValid())
				  {
				    EXPECT_TRUE(pReadComponentSoftReference->IsPotentiallyValid());

				    const Optional<Entity::Component3D*> pComponent =
				      pReadComponentSoftReference->Find<Entity::Component3D>(sceneRegistry);
				    EXPECT_TRUE(pComponent.IsValid());
				    EXPECT_EQ(pComponent.Get(), pOwningComponent.Get());
				  }
				}

				// Serialize to / from disk
				{
				  Serialization::Data serializedData(Serialization::ContextFlags::ToDisk | Serialization::ContextFlags::FromDisk);
				  {
				    const Entity::ComponentSoftReference componentReference{*pOwningComponent, sceneRegistry};
				    EXPECT_TRUE(componentReference.IsPotentiallyValid());

				    const Optional<Entity::Component3D*> pComponent =
				      componentReference.Find<Entity::Component3D>(sceneRegistry);
				    EXPECT_TRUE(pComponent.IsValid());
				    EXPECT_EQ(pComponent.Get(), pOwningComponent.Get());

				    Serialization::Writer writer(serializedData);
				    const bool wroteComponent = writer.SerializeInPlace(componentReference, System::Get<Entity::Manager>().GetRegistry());
				    EXPECT_TRUE(wroteComponent);
				  }

				  Serialization::Reader reader(serializedData);
				  const Optional<Entity::ComponentSoftReference> pReadComponentSoftReference = reader.ReadInPlace<Entity::ComponentSoftReference>(
				    System::Get<Entity::Manager>().GetRegistry(),
				    sceneRegistry
				  );
				  EXPECT_TRUE(pReadComponentSoftReference.IsValid());
				  if (pReadComponentSoftReference.IsValid())
				  {
				    EXPECT_TRUE(pReadComponentSoftReference->IsPotentiallyValid());

				    const Optional<Entity::Component3D*> pComponent =
				      pReadComponentSoftReference->Find<Entity::Component3D>(sceneRegistry);
				    EXPECT_TRUE(pComponent.IsValid());
				    EXPECT_EQ(pComponent.Get(), pOwningComponent.Get());
				  }
				}*/
			}
		}

		System::Get<Reflection::Registry>().DeregisterDynamicType<SoftReferencesTestDataComponent>();
		System::Get<Entity::Manager>().GetRegistry().Deregister(Reflection::GetTypeGuid<SoftReferencesTestDataComponent>());
	}
}
