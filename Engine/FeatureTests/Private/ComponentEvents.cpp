#include <Common/Memory/New.h>

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentValue.inl>
#include <Engine/Entity/Serialization/ComponentValue.h>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Event/ReflectedEvent.inl>
#include <Engine/Tests/FeatureTest.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Math/Angle.h>
#include <Common/Math/ClampedValue.h>

namespace ngine::Tests
{
	struct EventTestComponent final : public Entity::Component3D
	{
		using BaseType = Component3D;

		using BaseType::BaseType;

		ReflectedEvent<void(const Math::Vector3f a, const Math::Vector3f b)> MyEvent;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::EventTestComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::EventTestComponent>(
			"{DB43D687-F714-4D57-B63D-F303CCFF9EBB}"_guid,
			MAKE_UNICODE_LITERAL("Event Test Component"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{Reflection::Event{
				"{D1A69C75-7171-4E8B-9369-6693622537C4}"_guid,
				MAKE_UNICODE_LITERAL("My Event"),
				&Tests::EventTestComponent::MyEvent,
				Reflection::Argument{"{50B1F336-3DD7-47CF-8953-E6D390869968}"_guid, MAKE_UNICODE_LITERAL("a")},
				Reflection::Argument{"{A782BA16-1F07-49F0-B6BB-64CE52194A18}"_guid, MAKE_UNICODE_LITERAL("b")}
			}}
		);
	};
}

namespace ngine::Tests
{
	FEATURE_TEST(Components, ComponentEvents)
	{
		System::Get<Reflection::Registry>().RegisterDynamicType<EventTestComponent>();
		UniquePtr<Entity::ComponentType<EventTestComponent>> pComponentType = UniquePtr<Entity::ComponentType<EventTestComponent>>::Make();
		System::Get<Entity::Manager>().GetRegistry().Register(pComponentType.Get());

		{
			Entity::SceneRegistry sceneRegistry;
			UniquePtr<Scene> pScene = UniquePtr<Scene>::Make(
				sceneRegistry,
				Optional<Entity::HierarchyComponentBase*>{},
				1024_meters,
				"{29FB327E-7364-4162-B571-539E8DF2F4A9}"_guid,
				Scene::Flags::IsDisabled
			);

			const Entity::ComponentValue<EventTestComponent> pTestComponent{
				pScene->GetEntitySceneRegistry(),
				EventTestComponent::Initializer{pScene->GetRootComponent()}
			};
			EXPECT_TRUE(pTestComponent.IsValid());
			if (pTestComponent.IsValid())
			{
				// Make sure we can notify with no subscribers
				pTestComponent->MyEvent.NotifyAll(Math::Zero, Math::Zero);

				static uint8 fooEventCounter = 0;

				struct Foo
				{
					void OnEvent(const Math::Vector3f a, const Math::Vector3f b)
					{
						EXPECT_TRUE((a == Math::Vector3f{1, 2, 3}).AreAllSet());
						EXPECT_TRUE((b == Math::Vector3f{4, 5, 6}).AreAllSet());
						++fooEventCounter;
					}
				};
				Foo foo;
				pTestComponent->MyEvent.Subscribe<&Foo::OnEvent>(foo);

				EXPECT_EQ(fooEventCounter, 0);
				pTestComponent->MyEvent.NotifyAll(Math::Vector3f{1, 2, 3}, Math::Vector3f{4, 5, 6});
				EXPECT_EQ(fooEventCounter, 1);

				static uint8 lambdaEventCounter = 0;

				void* lambdaIdentifier = reinterpret_cast<void*>(0xDEDEDE);
				uint32 captureTest = 9001;
				pTestComponent->MyEvent.Subscribe(
					lambdaIdentifier,
					[captureTest](const Math::Vector3f a, const Math::Vector3f b)
					{
						EXPECT_EQ(captureTest, 9001u);
						EXPECT_TRUE((a == Math::Vector3f{1, 2, 3}).AreAllSet());
						EXPECT_TRUE((b == Math::Vector3f{4, 5, 6}).AreAllSet());
						++lambdaEventCounter;
					}
				);

				EXPECT_EQ(lambdaEventCounter, 0);
				EXPECT_EQ(fooEventCounter, 1);
				pTestComponent->MyEvent.NotifyAll(Math::Vector3f{1, 2, 3}, Math::Vector3f{4, 5, 6});
				EXPECT_EQ(fooEventCounter, 2);
				EXPECT_EQ(lambdaEventCounter, 1);

				pTestComponent->MyEvent.NotifyOne(foo, Math::Vector3f{1, 2, 3}, Math::Vector3f{4, 5, 6});
				EXPECT_EQ(lambdaEventCounter, 1);
				EXPECT_EQ(fooEventCounter, 3);

				const bool waLambdaUnsubscribed = pTestComponent->MyEvent.Unsubscribe(lambdaIdentifier);
				EXPECT_TRUE(waLambdaUnsubscribed);
				const bool waFooUnsubscribed = pTestComponent->MyEvent.Unsubscribe(foo);
				EXPECT_TRUE(waFooUnsubscribed);
			}
		}

		System::Get<Reflection::Registry>().DeregisterDynamicType<EventTestComponent>();
		System::Get<Entity::Manager>().GetRegistry().Deregister(Reflection::GetTypeGuid<EventTestComponent>());
	}

	struct PropertyTestComponent final : public Entity::Component3D
	{
		using BaseType = Component3D;

		using BaseType::BaseType;

		inline static constexpr Guid TypeGuid = "{D1835C83-7BD1-41DE-A9A9-0CBCC7AF6E38}"_guid;

		ReflectedEvent<void()> OnFieldOfViewChanged;
		ReflectedEvent<void()> OnNearPlaneChanged;
		ReflectedEvent<void()> OnFarPlaneChanged;
		ReflectedEvent<void(Reflection::PropertyMask)> OnPropertiesChanged;

		Math::Anglef m_fieldOfView = 60_degrees;
		Math::ClampedValuef m_nearPlane = {0.1f, 0.0001f, 4096};
		Math::ClampedValuef m_farPlane = {128.f, 1.f, 65536};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Tests::PropertyTestComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Tests::PropertyTestComponent>(
			Tests::PropertyTestComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Event Test Component"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Field of View"),
					"fieldOfView",
					"{293A8A35-284A-4C75-8AED-5C94913A2708}"_guid,
					MAKE_UNICODE_LITERAL("Camera"),
					Reflection::PropertyFlags{},
					&Tests::PropertyTestComponent::m_fieldOfView,
					&Tests::PropertyTestComponent::OnFieldOfViewChanged
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Near Plane"),
					"nearPlane",
					"{FA4C83A1-BAB8-42C0-ADE1-89E31A33600D}"_guid,
					MAKE_UNICODE_LITERAL("Camera"),
					Reflection::PropertyFlags{},
					&Tests::PropertyTestComponent::m_nearPlane,
					&Tests::PropertyTestComponent::OnNearPlaneChanged
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Far Plane"),
					"farPlane",
					"{A52F90E6-D3DC-4AFE-8143-9E40E362BD4B}"_guid,
					MAKE_UNICODE_LITERAL("Camera"),
					Reflection::PropertyFlags{},
					&Tests::PropertyTestComponent::m_farPlane,
					&Tests::PropertyTestComponent::OnFarPlaneChanged
				},
			}
		);
	};
}

namespace ngine::Tests
{

	FEATURE_TEST(Components, ComponentProperties)
	{
		System::Get<Reflection::Registry>().RegisterDynamicType<PropertyTestComponent>();
		UniquePtr<Entity::ComponentType<PropertyTestComponent>> pComponentType = UniquePtr<Entity::ComponentType<PropertyTestComponent>>::Make(
		);
		System::Get<Entity::Manager>().GetRegistry().Register(pComponentType.Get());

		{
			Entity::SceneRegistry sceneRegistry;
			UniquePtr<Scene> pScene = UniquePtr<Scene>::Make(
				sceneRegistry,
				Optional<Entity::HierarchyComponentBase*>{},
				1024_meters,
				"{F3DE2169-3172-4E56-81F1-51574920B17A}"_guid,
				Scene::Flags::IsDisabled
			);

			constexpr ConstStringView eventTestJsonInitial = R"(
{
    "typeGuid": "{D1835C83-7BD1-41DE-A9A9-0CBCC7AF6E38}",
    "d1835c83-7bd1-41de-a9a9-0cbcc7af6e38": {
        "fieldOfView": 75
    }
}
)";

			Serialization::Data eventTestDataInitial(eventTestJsonInitial);
			EXPECT_TRUE(eventTestDataInitial.IsValid());
			if (!eventTestDataInitial.IsValid())
			{
				return;
			}

			Entity::ComponentValue<PropertyTestComponent> pTestComponent;
			Threading::JobBatch jobBatch;
			const bool wasRead = pTestComponent.Serialize(
				Serialization::Reader(eventTestDataInitial),
				pScene->GetRootComponent(),
				pScene->GetEntitySceneRegistry(),
				jobBatch
			);
			EXPECT_TRUE(wasRead);
			EXPECT_TRUE(pTestComponent.IsValid());
			if (pTestComponent.IsValid())
			{
				EXPECT_EQ(pTestComponent->m_fieldOfView, 75_degrees);
				EXPECT_EQ((float)pTestComponent->m_nearPlane, 0.1f);
				EXPECT_EQ((float)pTestComponent->m_farPlane, 128.f);

				constexpr ConstStringView eventTestJsonAfter = R"(
{
    "typeGuid": "{D1835C83-7BD1-41DE-A9A9-0CBCC7AF6E38}",
    "d1835c83-7bd1-41de-a9a9-0cbcc7af6e38": {
        "fieldOfView": 54,
        "nearPlane": 1,
        "farPlane": 100
    }
}
)";

				enum class EventType : uint8
				{
					FieldOfView,
					NearPlane,
					FarPlane,
					PropertiesChanged,
					Count
				};
				struct Properties
				{
					const PropertyTestComponent& component;
					uint8 fieldOfViewSignalCount = 0;
					uint8 nearPlaneSignalCount = 0;
					uint8 farPlaneSignalCount = 0;
					uint8 propertiesChangedSignalCount = 0;
					FlatVector<EventType, (uint8)EventType::Count> eventSignalOrder;
				};
				Properties properties{*pTestComponent};

				Serialization::Data eventTestDataAfter(eventTestJsonAfter);
				EXPECT_TRUE(eventTestDataAfter.IsValid());
				if (!eventTestDataAfter.IsValid())
				{
					return;
				}

				void* identifier = reinterpret_cast<void*>(0xDEDEDE);

				pTestComponent->OnFieldOfViewChanged.Subscribe(
					identifier,
					[&properties]()
					{
						EXPECT_EQ(properties.component.m_fieldOfView, 54_degrees);
						++properties.fieldOfViewSignalCount;
						properties.eventSignalOrder.EmplaceBack(EventType::FieldOfView);
					}
				);

				pTestComponent->OnNearPlaneChanged.Subscribe(
					identifier,
					[&properties]()
					{
						EXPECT_EQ((float)properties.component.m_nearPlane, 1.f);
						++properties.nearPlaneSignalCount;
						properties.eventSignalOrder.EmplaceBack(EventType::NearPlane);
					}
				);

				pTestComponent->OnFarPlaneChanged.Subscribe(
					identifier,
					[&properties]()
					{
						EXPECT_EQ((float)properties.component.m_farPlane, 100.f);
						++properties.farPlaneSignalCount;
						properties.eventSignalOrder.EmplaceBack(EventType::FarPlane);
					}
				);

				pTestComponent->OnPropertiesChanged.Subscribe(
					identifier,
					[&properties](const Reflection::PropertyMask changedProperties)
					{
						EXPECT_EQ(properties.component.m_fieldOfView, 54_degrees);
						EXPECT_EQ((float)properties.component.m_nearPlane, 1.f);
						EXPECT_EQ((float)properties.component.m_farPlane, 100.f);
						EXPECT_EQ(changedProperties.GetNumberOfSetBits(), 3);
						EXPECT_TRUE(changedProperties.IsSet(1 << 0));
						EXPECT_TRUE(changedProperties.IsSet(1 << 1));
						EXPECT_TRUE(changedProperties.IsSet(1 << 2));
						++properties.propertiesChangedSignalCount;
						properties.eventSignalOrder.EmplaceBack(EventType::PropertiesChanged);
					}
				);
				Entity::ComponentReference<PropertyTestComponent> pTestComponentReference{*pTestComponent};

				Threading::JobBatch jobBatchAfter;
				const bool wasReadAfter = pTestComponent->Serialize(Serialization::Reader(eventTestDataAfter), jobBatchAfter);
				EXPECT_TRUE(wasReadAfter);

				EXPECT_EQ(properties.fieldOfViewSignalCount, 1);
				EXPECT_EQ(properties.nearPlaneSignalCount, 1);
				EXPECT_EQ(properties.farPlaneSignalCount, 1);
				EXPECT_EQ(properties.propertiesChangedSignalCount, 1);

				EXPECT_TRUE(properties.eventSignalOrder.GetSize() > 0 && properties.eventSignalOrder[0] == EventType::FieldOfView);
				EXPECT_TRUE(properties.eventSignalOrder.GetSize() > 1 && properties.eventSignalOrder[1] == EventType::NearPlane);
				EXPECT_TRUE(properties.eventSignalOrder.GetSize() > 2 && properties.eventSignalOrder[2] == EventType::FarPlane);
				EXPECT_TRUE(properties.eventSignalOrder.GetSize() > 3 && properties.eventSignalOrder[3] == EventType::PropertiesChanged);
			}
		}

		System::Get<Reflection::Registry>().DeregisterDynamicType<PropertyTestComponent>();
		System::Get<Entity::Manager>().GetRegistry().Deregister(Reflection::GetTypeGuid<PropertyTestComponent>());
	}
}
