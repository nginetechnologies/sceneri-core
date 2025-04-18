#pragma once

#include <PhysicsCore/ConstraintIdentifier.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentPicker.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>
#include <Common/Reflection/Type.h>
#include <Common/Reflection/EnumTypeExtension.h>

namespace ngine::Entity
{
	struct SplineComponent;
}

namespace ngine::Physics
{
	namespace Data
	{
		struct Body;
	}
}

namespace ngine::Physics::Components
{
	struct SplineConstraint : public Entity::Component3D
	{
		static constexpr Guid TypeGuid = "F5B291C5-7685-468A-B6B7-8248D8E14381"_guid;

		using BaseType = Component3D;

		SplineConstraint(const SplineConstraint& templateComponent, const Cloner& cloner);
		SplineConstraint(const Deserializer& deserializer);
		SplineConstraint(Initializer&& initializer);

		void OnCreated();
		void OnDestroy();
		void OnEnable();
		void OnDisable();

		void CreateConstraint();

		virtual void OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags) override;

		void SetConstrainedComponent(const Entity::Component3DPicker constrained);
		[[nodiscard]] Entity::Component3DPicker GetConstrainedComponent() const;

		enum class RotationConstraintType : uint8
		{
			Free,                    ///< Do not constrain the rotation of the constrained body at all
			ConstrainAroundTangent,  ///< Only allow rotation around the tangent vector (following the path)
			ConstrainAroundNormal,   ///< Only allow rotation around the normal vector (perpendicular to the path)
			ConstrainAroundBinormal, ///< Only allow rotation around the binormal vector (perpendicular to the path)
			ConstainToPath,   ///< Fully constrain the rotation of the constrained body to the path (follwing the tangent and normal of the path)
			FullyConstrained, ///< Fully constrain the rotation of the constrained body to the rotation of the spline body (if present)
		};

		void OnRotationConstraintTypeChanged();
	protected:
		[[nodiscard]] Optional<Entity::SplineComponent*> GetSplineComponent(Entity::SceneRegistry& sceneRegistry) const;
		[[nodiscard]] Optional<Entity::Component3D*> GetBodyComponent(Entity::SceneRegistry& sceneRegistry) const;
	protected:
		friend struct Reflection::ReflectedType<SplineConstraint>;

		ConstraintIdentifier m_constraintIdentifier;

		Entity::ComponentSoftReference m_constrainedComponent;

		RotationConstraintType m_rotationConstraintType = RotationConstraintType::Free;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::Components::SplineConstraint::RotationConstraintType>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Components::SplineConstraint::RotationConstraintType>(
			"{6BD4863F-788E-47A3-8AD9-81184551249D}"_guid,
			MAKE_UNICODE_LITERAL("Spline Rotation Constraint Type"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Reflection::EnumTypeExtension{
				Reflection::EnumTypeEntry{Physics::Components::SplineConstraint::RotationConstraintType::Free, MAKE_UNICODE_LITERAL("Free")},
				Reflection::EnumTypeEntry{
					Physics::Components::SplineConstraint::RotationConstraintType::ConstrainAroundTangent,
					MAKE_UNICODE_LITERAL("Constrain to Tangent")
				},
				Reflection::EnumTypeEntry{
					Physics::Components::SplineConstraint::RotationConstraintType::ConstrainAroundNormal, MAKE_UNICODE_LITERAL("Constrain to Normal")
				},
				Reflection::EnumTypeEntry{
					Physics::Components::SplineConstraint::RotationConstraintType::ConstrainAroundBinormal,
					MAKE_UNICODE_LITERAL("Constrain to Binormal")
				},
				Reflection::EnumTypeEntry{
					Physics::Components::SplineConstraint::RotationConstraintType::ConstainToPath, MAKE_UNICODE_LITERAL("Constrain to Spline")
				},
				Reflection::EnumTypeEntry{
					Physics::Components::SplineConstraint::RotationConstraintType::FullyConstrained, MAKE_UNICODE_LITERAL("Constrain to Spline Body")
				},
			}}
		);
	};

	template<>
	struct ReflectedType<Physics::Components::SplineConstraint>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::Components::SplineConstraint>(
			Physics::Components::SplineConstraint::TypeGuid,
			MAKE_UNICODE_LITERAL("Spline Constraint"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Constrained Component"),
					"constrainedComponent",
					"{52D1BC6F-42EC-4ED2-A1DE-62CB56439DA0}"_guid,
					MAKE_UNICODE_LITERAL("Physics Constraint"),
					&Physics::Components::SplineConstraint::SetConstrainedComponent,
					&Physics::Components::SplineConstraint::GetConstrainedComponent
				),
				Reflection::MakeProperty(
					MAKE_UNICODE_LITERAL("Rotation Constraint Mode"),
					"rotationConstraint",
					"{6D143ABF-4CE6-4098-AC2D-01A6054AEA9B}"_guid,
					MAKE_UNICODE_LITERAL("Physics Constraint"),
					Reflection::PropertyFlags{},
					&Physics::Components::SplineConstraint::m_rotationConstraintType,
					&Physics::Components::SplineConstraint::OnRotationConstraintTypeChanged
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
