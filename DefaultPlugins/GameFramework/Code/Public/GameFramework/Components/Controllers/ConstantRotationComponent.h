#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/Serialization/ComponentReference.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Math/Angle3.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework
{
	struct ConstantRotationComponent final : public Entity::Data::Component3D
	{
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		using BaseType = Entity::Data::Component3D;
		using OwnerComponentReference = Entity::ComponentReference<Entity::Component3D>;
		using ConstOwnerComponentReference = Entity::ComponentReference<const Entity::Component3D>;

		using Initializer = BaseType::DynamicInitializer;

		ConstantRotationComponent(const ConstantRotationComponent& templateComponent, const Cloner& cloner);
		ConstantRotationComponent(const Deserializer& deserializer);
		ConstantRotationComponent(Initializer&& initializer);
		~ConstantRotationComponent();

		void OnCreated(Entity::Component3D& owner);
		void OnDestroying();
		void OnEnable(Entity::Component3D& owner);
		void OnDisable(Entity::Component3D& owner);
		void OnSimulationResumed(Entity::Component3D& owner);
		void OnSimulationPaused(Entity::Component3D& owner);

		void AfterPhysicsUpdate();
	protected:
		void RegisterForUpdate(Entity::Component3D& owner);
		void DeregisterUpdate(Entity::Component3D& owner);

		friend struct Reflection::ReflectedType<ConstantRotationComponent>;
		[[nodiscard]] Math::Angle3f GetRotation() const
		{
			return m_rotation;
		}
		void SetRotation(const Math::Angle3f rotation)
		{
			m_rotation = Math::YawPitchRollf(rotation);
		}

		Entity::Component3D& m_owner;
		Math::YawPitchRollf m_rotation = Math::YawPitchRollf(0_degrees, 0_degrees, 16_degrees);
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::ConstantRotationComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<GameFramework::ConstantRotationComponent>(
			"a718cdb2-ef62-47c4-a968-8862f78cc08c"_guid,
			MAKE_UNICODE_LITERAL("Constant Rotation"),
			Reflection::TypeFlags::DisableUserInterfaceInstantiation,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Rotation"),
				"rotation",
				"{513061E2-6BF2-45EB-8DC6-06CFADEA2DB3}"_guid,
				MAKE_UNICODE_LITERAL("Constant Rotation"),
				&GameFramework::ConstantRotationComponent::SetRotation,
				&GameFramework::ConstantRotationComponent::GetRotation
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{Entity::ComponentTypeFlags(), "bc0ced52-0a3f-d6d4-fe89-c2d733476f3d"_asset},
				Entity::IndicatorTypeExtension{EnumFlags<Entity::IndicatorTypeExtension::Flags>{Entity::IndicatorTypeExtension::Flags::RequiresGhost
		    }}
			}
		);
	};
}
