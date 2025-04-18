#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentTypeExtension.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>
#include <PhysicsCore/3rdparty/jolt/Physics/PhysicsStepListener.h>

#include <Common/Math/Acceleration.h>
#include <Common/Math/Radius.h>

namespace ngine::Physics
{
	struct SphericalGravityComponent : public Entity::Component3D, public JPH::PhysicsStepListener
	{
		static constexpr Guid TypeGuid = "11cbade4-acf3-41b4-81de-7d2d6bdf9a4a"_guid;

		using BaseType = Entity::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = Entity::Component3D::Initializer;

			using BaseType::BaseType;
			Initializer(
				BaseType&& initializer, const Math::Accelerationf acceleration = 9.81_m_per_second_squared, const Math::Radiusf radius = 20_meters
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_acceleration(acceleration)
				, m_radius(radius)
			{
			}

			Math::Accelerationf m_acceleration{9.81_m_per_second_squared};
			Math::Radiusf m_radius{20_meters};
		};

		SphericalGravityComponent(Initializer&& initializer);
		SphericalGravityComponent(const SphericalGravityComponent& templateComponent, const Cloner& cloner);
		SphericalGravityComponent(const Deserializer& deserializer);
		virtual ~SphericalGravityComponent();

		void OnCreated();
		void OnEnable();
		void OnDisable();
		friend struct Reflection::ReflectedType<Physics::SphericalGravityComponent>;

		[[nodiscard]] Math::Radiusf GetRadius() const
		{
			return m_radius;
		}
		void SetRadius(const Math::Radiusf radius)
		{
			Assert(radius.GetMeters() > 0.f);
			m_radius = radius;
		}
	protected:
		SphericalGravityComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer);

		// PhysicsStepListener
		virtual void OnStep(float inDeltaTime, JPH::PhysicsSystem& inPhysicsSystem) override;
		// ~PhysicsStepListener
	private:
		Math::Accelerationf m_acceleration{9.81_m_per_second_squared};
		Math::Radiusf m_radius{20_meters};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::SphericalGravityComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::SphericalGravityComponent>(
			Physics::SphericalGravityComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Spherical Gravity"),
			Reflection::TypeFlags(),
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Acceleration"),
					"acceleration",
					"{631F45D3-658D-480C-BEED-3A503BE72F9D}"_guid,
					MAKE_UNICODE_LITERAL("Gravity"),
					Reflection::PropertyFlags{},
					&Physics::SphericalGravityComponent::m_acceleration
				},
				Reflection::Property{
					MAKE_UNICODE_LITERAL("Radius"),
					"radius",
					"513f7429-b007-4707-b8b2-4c0587a70bb4"_guid,
					MAKE_UNICODE_LITERAL("Gravity"),
					Reflection::PropertyFlags{},
					&Physics::SphericalGravityComponent::m_radius
				}
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "ff684d25-c967-4910-02a9-9b87d7cad113"_asset, "bffd01e3-e9f8-4f24-b1e2-077e6c6528f3"_guid
				},
				Entity::IndicatorTypeExtension{"274756c3-a52a-46b0-b46a-ed77d04d50cf"_guid, "ff684d25-c967-4910-02a9-9b87d7cad113"_asset}
			}
		);
	};
}
