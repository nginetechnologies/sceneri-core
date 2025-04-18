#pragma once

#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>
#include <PhysicsCore/3rdparty/jolt/Physics/PhysicsStepListener.h>

#include <Common/Math/Acceleration.h>

namespace ngine::Physics
{
	struct DirectionalGravityComponent : public Entity::Component3D, public JPH::PhysicsStepListener
	{
		static constexpr Guid TypeGuid = "37000929-8ed6-4f15-a9ec-4f7d15302f02"_guid;

		using BaseType = Entity::Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 4>;

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

		DirectionalGravityComponent(Initializer&& initializer);
		DirectionalGravityComponent(const DirectionalGravityComponent& templateComponent, const Cloner& cloner);
		DirectionalGravityComponent(const Deserializer& deserializer);
		virtual ~DirectionalGravityComponent();

		void OnCreated();
		void OnEnable();
		void OnDisable();
		friend struct Reflection::ReflectedType<Physics::DirectionalGravityComponent>;
	protected:
		DirectionalGravityComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> typeSerializer);

		virtual void OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags) override;

		// PhysicsStepListener
		virtual void OnStep(float inDeltaTime, JPH::PhysicsSystem& inPhysicsSystem) override;
		// ~PhysicsStepListener
	private:
		Math::Accelerationf m_acceleration{9.81_m_per_second_squared};
		Math::Vector3f m_previousGravity{Math::Zero};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::DirectionalGravityComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::DirectionalGravityComponent>(
			Physics::DirectionalGravityComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Directional Gravity"),
			Reflection::TypeFlags(),
			Reflection::Tags{},
			Reflection::Properties{Reflection::Property{
				MAKE_UNICODE_LITERAL("Acceleration"),
				"acceleration",
				"{631F45D3-658D-480C-BEED-3A503BE72F9D}"_guid,
				MAKE_UNICODE_LITERAL("Gravity"),
				Reflection::PropertyFlags{},
				&Physics::DirectionalGravityComponent::m_acceleration
			}},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(),
					"eb56b5c2-5a6e-6b7e-ea12-75bd28630015"_asset,
					"bffd01e3-e9f8-4f24-b1e2-077e6c6528f3"_guid,
					"37000929-8ed6-4f15-a9ec-4f7d15302f02"_asset
				},
				Entity::IndicatorTypeExtension{"c6742677-54b6-41ae-8b39-495721b423e2"_guid, "21200312-8aa9-9673-7e58-096b595ff35d"_asset}
			}
		);
	};
}
