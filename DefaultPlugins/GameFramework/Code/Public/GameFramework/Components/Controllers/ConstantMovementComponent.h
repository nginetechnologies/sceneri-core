#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Math/Vector3.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework
{
	struct ConstantMovementComponent final : public Entity::Data::Component3D
	{
		using BaseType = Entity::Data::Component3D;
		using Initializer = DynamicInitializer;

		ConstantMovementComponent(const ConstantMovementComponent& templateComponent, const Cloner& cloner);
		ConstantMovementComponent(const Deserializer& deserializer);
		ConstantMovementComponent(Initializer&& initializer);
		~ConstantMovementComponent() = default;

		void OnCreated(Entity::Component3D& owner);
		void OnDestroying();
		void OnDisable(Entity::Component3D& owner);
		void OnSimulationResumed(Entity::Component3D& owner);
		void OnSimulationPaused(Entity::Component3D& owner);

		void AfterPhysicsUpdate();

		[[nodiscard]] Math::Vector3f GetMovementVector() const
		{
			return m_movementVector;
		}
	protected:
		void RegisterForUpdate(Entity::Component3D& owner);
		void DeregisterUpdate(Entity::Component3D& owner);

		friend struct Reflection::ReflectedType<ConstantMovementComponent>;

		Entity::Component3D& m_owner;
		Math::Vector3f m_movementVector;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::ConstantMovementComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::ConstantMovementComponent>(
			"cb3951bd-b4fe-40bc-b762-bd20c8a39389"_guid,
			MAKE_UNICODE_LITERAL("Constant Movement"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeProperty(
				MAKE_UNICODE_LITERAL("Movement"),
				"movement",
				"{56E2B880-933C-418D-9678-A9C2CC75890E}"_guid,
				MAKE_UNICODE_LITERAL("Constant Movement"),
				&GameFramework::ConstantMovementComponent::m_movementVector
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "bc0ced52-0a3f-d6d4-fe89-c2d733476f3d"_asset, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_guid
				},
				Entity::IndicatorTypeExtension{EnumFlags<Entity::IndicatorTypeExtension::Flags>{Entity::IndicatorTypeExtension::Flags::RequiresGhost
		    }}
			}
		);
	};
}
