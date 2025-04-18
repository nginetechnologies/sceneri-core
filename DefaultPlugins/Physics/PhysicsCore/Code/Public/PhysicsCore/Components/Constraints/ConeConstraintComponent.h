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
	struct ConeConstraint : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "8C959B4E-34D4-45A6-AD84-4F6C5DF990A9"_guid;

		using BaseType = Component3D;

		ConeConstraint(const ConeConstraint& templateComponent, const Cloner& cloner);
		ConeConstraint(const Deserializer& deserializer);
		ConeConstraint(Initializer&& initializer);

		void OnCreated();
		void OnDestroy();
		void OnEnable();
		void OnDisable();

		void CreateConstraint();

		virtual void OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags) override;

		void SetConstrainedComponent(const Entity::Component3DPicker constrained);
		[[nodiscard]] Entity::Component3DPicker GetConstrainedComponent() const;
		void SetMaximumAngle(const Math::Anglef angle);
		[[nodiscard]] Math::Anglef GetMaximumAngle() const;
		[[nodiscard]] Math::Anglef GetHalfMaximumAngle() const
		{
			return m_halfMaximumConeAngle;
		}
	protected:
		[[nodiscard]] Optional<Entity::Component3D*> GetBodyComponent() const;
	protected:
		ConstraintIdentifier m_constraintIdentifier;
		Entity::ComponentSoftReference m_constrainedComponent;

		Math::Anglef m_halfMaximumConeAngle = 15_degrees;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::Components::ConeConstraint>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Components::ConeConstraint>(
			Physics::Components::ConeConstraint::TypeGuid,
			MAKE_UNICODE_LITERAL("Cone Constraint"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Constrained Component"),
					"constrainedComponent",
					"{D007A8E0-B97F-4EC1-8EB7-EFD459870FE5}"_guid,
					MAKE_UNICODE_LITERAL("Physics Constraint"),
					&Physics::Components::ConeConstraint::SetConstrainedComponent,
					&Physics::Components::ConeConstraint::GetConstrainedComponent
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Maximum Angle"),
					"maxAngle",
					"{17AFC872-8D29-4EDD-86D9-E122E709C8BF}"_guid,
					MAKE_UNICODE_LITERAL("Physics Constraint"),
					&Physics::Components::ConeConstraint::SetMaximumAngle,
					&Physics::Components::ConeConstraint::GetMaximumAngle
				)
			},
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
