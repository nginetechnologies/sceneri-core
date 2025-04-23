#pragma once

#include <Engine/Entity/Data/Component3D.h>
#include <Engine/Entity/ComponentPicker.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

#include <Common/Asset/Picker.h>
#include <Common/Math/Vector3.h>
#include <Common/Math/ClampedValue.h>
#include <Common/Reflection/EnumTypeExtension.h>

#include <Common/Reflection/CoreTypes.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework
{
	struct LookAtComponent final : public Entity::Data::Component3D
	{
		using BaseType = Entity::Data::Component3D;
		using Initializer = Entity::Data::Component3D::DynamicInitializer;

		LookAtComponent(const LookAtComponent& templateComponent, const Cloner& cloner);
		LookAtComponent(const Deserializer& deserializer);
		LookAtComponent(Initializer&& initializer);
		virtual ~LookAtComponent();

		void OnDestroying();
		void OnSimulationResumed(Entity::Component3D& owner);
		void OnSimulationPaused(Entity::Component3D& owner);

		void SetLookAtComponent(Entity::Component3D& owner, const Entity::Component3DPicker lookAt);
		void AfterPhysicsUpdate();
	protected:
		Entity::Component3DPicker GetLookAtComponent(Entity::Component3D& owner) const;

		void RegisterForUpdate(Entity::Component3D& owner);
		void DeregisterUpdate(Entity::Component3D& owner);

		friend struct Reflection::ReflectedType<LookAtComponent>;

		Entity::Component3D& m_owner;
		Entity::ComponentSoftReference m_lookAt;
		Math::Quaternionf m_previousRotation{Math::Identity};
		Math::WorldCoordinate m_previousPosition{Math::Zero};
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::LookAtComponent>
	{
		static constexpr auto Type = Reflection::Reflect<GameFramework::LookAtComponent>(
			"02614ab2-4983-4380-930e-0e6b4ca4bd97"_guid,
			MAKE_UNICODE_LITERAL("Look At Component"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Look at"),
				"lookAt",
				"{B33CBA7C-0913-4F7E-9F05-B46C75022D96}"_guid,
				MAKE_UNICODE_LITERAL("Look At"),
				&GameFramework::LookAtComponent::SetLookAtComponent,
				&GameFramework::LookAtComponent::GetLookAtComponent
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
