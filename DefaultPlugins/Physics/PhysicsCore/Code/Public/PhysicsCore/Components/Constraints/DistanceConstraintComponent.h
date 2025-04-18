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
	struct DistanceConstraint : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "3130519D-FACB-43EC-A592-59CDB02CE45B"_guid;

		using BaseType = Component3D;

		DistanceConstraint(const DistanceConstraint& templateComponent, const Cloner& cloner);
		DistanceConstraint(const Deserializer& deserializer);
		DistanceConstraint(Initializer&& initializer);

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
	struct ReflectedType<Physics::Components::DistanceConstraint>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Components::DistanceConstraint>(
			Physics::Components::DistanceConstraint::TypeGuid,
			MAKE_UNICODE_LITERAL("Distance Constraint"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Constrained Component"),
				"constrainedComponent",
				"{2A8C758E-F69F-4510-870D-6A9D701119B4}"_guid,
				MAKE_UNICODE_LITERAL("Physics Constraint"),
				&Physics::Components::DistanceConstraint::SetConstrainedComponent,
				&Physics::Components::DistanceConstraint::GetConstrainedComponent
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
