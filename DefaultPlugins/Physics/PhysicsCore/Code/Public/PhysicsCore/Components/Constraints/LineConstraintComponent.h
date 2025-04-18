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
	struct LineConstraint : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "2C0260EB-01A7-4699-ABB7-B30B0456E7FC"_guid;

		using BaseType = Component3D;

		LineConstraint(const LineConstraint& templateComponent, const Cloner& cloner);
		LineConstraint(const Deserializer& deserializer);
		LineConstraint(Initializer&& initializer);

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
	struct ReflectedType<Physics::Components::LineConstraint>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Components::LineConstraint>(
			Physics::Components::LineConstraint::TypeGuid,
			MAKE_UNICODE_LITERAL("Line Constraint"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Constrained Component"),
				"constrainedComponent",
				"{5EA54B49-27E4-437F-9AA8-17B519152FD8}"_guid,
				MAKE_UNICODE_LITERAL("Physics Constraint"),
				&Physics::Components::LineConstraint::SetConstrainedComponent,
				&Physics::Components::LineConstraint::GetConstrainedComponent
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "969ab3f2-7dee-7b38-bca5-403e36e44b3b"_asset, "5edc8044-ff05-4c39-b59a-29021095f002"_guid
				},
				Entity::IndicatorTypeExtension{"86C3C30F-5F7F-40CB-AE03-CB2FA7DDF169"_guid, "68677cbe-8b7a-2deb-7a50-50d2b77c7ff6"_asset}
			}
		);
	};
}
