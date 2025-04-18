#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentTypeExtension.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>
#include <PhysicsCore/3rdparty/jolt/Physics/PhysicsStepListener.h>

#include <Common/Math/Acceleration.h>

namespace ngine::Entity
{
	struct SplineComponent;
}

namespace ngine::Physics
{
	struct SplineGravityComponent : public Entity::Component3D, public JPH::PhysicsStepListener
	{
		static constexpr Guid TypeGuid = "{41B647E8-F4B6-4375-ACA5-CE3B5C31035F}"_guid;

		using BaseType = Entity::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = Entity::Component3D::Initializer;

			using BaseType::BaseType;
			Initializer(BaseType&& initializer, const Math::Accelerationf acceleration = 9.81_m_per_second_squared)
				: BaseType(Forward<BaseType>(initializer))
				, m_acceleration(acceleration)
			{
			}

			Math::Accelerationf m_acceleration{9.81_m_per_second_squared};
		};

		SplineGravityComponent(Initializer&& initializer);
		SplineGravityComponent(const SplineGravityComponent& templateComponent, const Cloner& cloner);
		SplineGravityComponent(const Deserializer& deserializer);
		virtual ~SplineGravityComponent();

		void OnCreated();
		void OnEnable();
		void OnDisable();
		friend struct Reflection::ReflectedType<Physics::SplineGravityComponent>;
	protected:
		SplineGravityComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer);

		// PhysicsStepListener
		virtual void OnStep(float inDeltaTime, JPH::PhysicsSystem& inPhysicsSystem) override;
		// ~PhysicsStepListener

		[[nodiscard]] Optional<Entity::SplineComponent*> GetSplineComponent(Entity::SceneRegistry& sceneRegistry) const;
	private:
		Math::Accelerationf m_acceleration{9.81_m_per_second_squared};
		Math::Radiusf m_radius{0.5_meters};
		Math::Ratiof m_damping{100_percent};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::SplineGravityComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::SplineGravityComponent>(
			Physics::SplineGravityComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Spline Gravity"),
			Reflection::TypeFlags(),
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Acceleration"),
					"acceleration",
					"{6DB3F7CA-E1AE-4D04-A9D0-148C4758B204}"_guid,
					MAKE_UNICODE_LITERAL("Gravity"),
					Reflection::PropertyFlags{},
					&Physics::SplineGravityComponent::m_acceleration
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Radius"),
					"radius",
					"{06B36432-C144-4C2B-9FDF-57D1C325A5AA}"_guid,
					MAKE_UNICODE_LITERAL("Gravity"),
					Reflection::PropertyFlags{},
					&Physics::SplineGravityComponent::m_radius
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Damping"),
					"damping",
					"{80CE0DF6-AE7D-4249-8EEE-9849C9A9ABCC}"_guid,
					MAKE_UNICODE_LITERAL("Gravity"),
					Reflection::PropertyFlags{},
					&Physics::SplineGravityComponent::m_damping
				}
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "fa774cb0-bbaa-5c5c-712f-a62faffffe7e"_asset, "bffd01e3-e9f8-4f24-b1e2-077e6c6528f3"_guid
				},
				Entity::IndicatorTypeExtension{"c6742677-54b6-41ae-8b39-495721b423e2"_guid, "315f31e1-5e11-087c-0fe0-5d697865ed1a"_asset}
			}
		);
	};
}
