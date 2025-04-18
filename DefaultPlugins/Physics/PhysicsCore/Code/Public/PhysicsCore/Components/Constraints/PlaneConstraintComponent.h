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
	struct PlaneConstraint : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "4877948e-e1dd-4eae-ae0b-9deed6bba861"_guid;

		using BaseType = Component3D;

		PlaneConstraint(const PlaneConstraint& templateComponent, const Cloner& cloner);
		PlaneConstraint(const Deserializer& deserializer);
		PlaneConstraint(Initializer&& initializer);

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
	struct ReflectedType<Physics::Components::PlaneConstraint>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Components::PlaneConstraint>(
			Physics::Components::PlaneConstraint::TypeGuid,
			MAKE_UNICODE_LITERAL("Plane Constraint"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Constrained Component"),
				"constrainedComponent",
				"{DF45150C-16D7-4B71-A885-67C9DC3C0F56}"_guid,
				MAKE_UNICODE_LITERAL("Physics Constraint"),
				&Physics::Components::PlaneConstraint::SetConstrainedComponent,
				&Physics::Components::PlaneConstraint::GetConstrainedComponent
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "969ab3f2-7dee-7b38-bca5-403e36e44b3b"_asset, "5edc8044-ff05-4c39-b59a-29021095f002"_guid
				},
				Entity::IndicatorTypeExtension{"1F9B6C58-D797-4D96-BB5C-CAC7878BAD75"_guid, "68677cbe-8b7a-2deb-7a50-50d2b77c7ff6"_asset}
			}
		);
	};
}
