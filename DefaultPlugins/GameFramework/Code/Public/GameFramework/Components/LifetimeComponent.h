#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Math/Vector3.h>
#include <Common/Reflection/CoreTypes.h>
#include <Common/Time/Duration.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework
{
	struct LifetimeComponent final : public Entity::Data::Component3D
	{
		static constexpr Guid TypeGuid = "d56be76f-6d78-449c-82db-49b485f2764b"_guid;

		using BaseType = Entity::Data::Component3D;

		struct Initializer : public BaseType::DynamicInitializer
		{
			using BaseType = Entity::Data::Component3D::DynamicInitializer;
			using BaseType::BaseType;
			Initializer(BaseType&& initializer, const Time::Durationf duration = 5_seconds)
				: BaseType(Forward<BaseType>(initializer))
				, m_duration(duration)
			{
			}

			Time::Durationf m_duration{5_seconds};
		};

		LifetimeComponent(const LifetimeComponent& templateComponent, const Cloner& cloner);
		LifetimeComponent(const Deserializer& deserializer);
		LifetimeComponent(Initializer&& initializer);
		~LifetimeComponent() = default;

		void OnCreated(Entity::Component3D& parent);
		void OnDestroying(Entity::Component3D& owner);
		void OnEnable(Entity::Component3D& owner);
		void OnDisable(Entity::Component3D& owner);
		void OnSimulationResumed(Entity::Component3D& owner);
		void OnSimulationPaused(Entity::Component3D& owner);

		void Update();
	protected:
		void RegisterForUpdate(Entity::Component3D& owner);
		void DeregisterUpdate(Entity::Component3D& owner);

		friend struct Reflection::ReflectedType<LifetimeComponent>;

		Entity::Component3D& m_owner;
		float m_duration{5.f};
		Time::Durationf m_current{0_seconds};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::LifetimeComponent>
	{
		static constexpr auto Type = Reflection::Reflect<GameFramework::LifetimeComponent>(
			GameFramework::LifetimeComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Lifetime"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Duration"),
				"duration",
				"{F74C4964-7075-4F9E-BCD4-7324A0542ED7}"_guid,
				MAKE_UNICODE_LITERAL("Lifetime"),
				&GameFramework::LifetimeComponent::m_duration
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "6334d2e8-0c15-ed0e-95cd-a48d864fa4d8"_asset, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_guid
				},
				Entity::IndicatorTypeExtension{EnumFlags<Entity::IndicatorTypeExtension::Flags>{Entity::IndicatorTypeExtension::Flags::RequiresGhost
		    }}
			}
		);
	};
}
