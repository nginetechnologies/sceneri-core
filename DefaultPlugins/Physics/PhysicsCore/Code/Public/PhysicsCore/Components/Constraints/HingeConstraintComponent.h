#pragma once

#include <PhysicsCore/ConstraintIdentifier.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentPicker.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>
#include <Common/Reflection/Type.h>

namespace ngine::Physics
{
	struct BodyComponent;
}

namespace ngine::Physics::Components
{
	struct HingeConstraint : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "6A69DEF6-FA40-4FC6-B2B1-0E3CF53E9475"_guid;

		using BaseType = Component3D;

		HingeConstraint(const HingeConstraint& templateComponent, const Cloner& cloner);
		HingeConstraint(const Deserializer& deserializer);
		HingeConstraint(Initializer&& initializer);

		void OnCreated();
		void OnDestroy();
		void OnEnable();
		void OnDisable();

		void CreateConstraint();

		virtual void OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags) override;

		void SetConstrainedComponent(const Entity::Component3DPicker constrained);
		[[nodiscard]] Entity::Component3DPicker GetConstrainedComponent() const;
	protected:
		[[nodiscard]] Optional<Entity::Component3D*> GetBodyComponent() const;
	protected:
		ConstraintIdentifier m_constraintIdentifier;
		Entity::ComponentSoftReference m_constrainedComponent;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::Components::HingeConstraint>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Components::HingeConstraint>(
			Physics::Components::HingeConstraint::TypeGuid,
			MAKE_UNICODE_LITERAL("Hinge Constraint"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Constrained Component"),
				"constrainedComponent",
				"{B156F6A0-9311-4BDC-9307-76E22732CD91}"_guid,
				MAKE_UNICODE_LITERAL("Physics Constraint"),
				&Physics::Components::HingeConstraint::SetConstrainedComponent,
				&Physics::Components::HingeConstraint::GetConstrainedComponent
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "969ab3f2-7dee-7b38-bca5-403e36e44b3b"_asset, "5edc8044-ff05-4c39-b59a-29021095f002"_guid
				},
				Entity::IndicatorTypeExtension{"c6742677-54b6-41ae-8b39-495721b423e2"_guid, "68677cbe-8b7a-2deb-7a50-50d2b77c7ff6"_asset}
			}
		);
	};
}
