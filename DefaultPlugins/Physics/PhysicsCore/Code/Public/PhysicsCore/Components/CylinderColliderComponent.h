#pragma once

#include "ColliderComponent.h"

#include <Common/Math/Length.h>
#include <Common/Math/ForwardDeclarations/Radius.h>

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

namespace ngine::Physics
{
	struct BodyComponent;

	struct CylinderInitializationParametersBase
	{
		Math::Radiusf m_radius = 0.5_meters;
		Math::Lengthf m_halfHeight = 1_meters;
	};

	struct CylinderInitializationParameters : public CylinderInitializationParametersBase
	{
		const Material& m_physicalMaterial;
	};

	struct CylinderColliderComponent final : public ColliderComponent
	{
		static constexpr Guid TypeGuid = "b16da720-3f5f-4fe8-9f04-81d600ba7324"_guid;

		using BaseType = ColliderComponent;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = ColliderComponent::Initializer;
			Initializer(BaseType&& initializer, Math::Radiusf radius = 0.5_meters, Math::Lengthf halfHeight = 1_meters)
				: BaseType(Forward<BaseType>(initializer))
				, m_radius(radius)
				, m_halfHeight(halfHeight)
			{
			}
			using BaseType::BaseType;

			Math::Radiusf m_radius = 0.5_meters;
			Math::Lengthf m_halfHeight = 1_meters;
		};

		CylinderColliderComponent(Initializer&& initializer);
		CylinderColliderComponent(const CylinderColliderComponent& templateComponent, const Cloner& cloner);
		CylinderColliderComponent(const Deserializer& deserializer);

		void SetRadius(const Math::Radiusf radius);
		[[nodiscard]] Math::Radiusf GetRadius() const;

		void SetHalfHeight(const Math::Lengthf height);
		[[nodiscard]] Math::Lengthf GetHalfHeight() const;

		void DeserializeCustomData(const Optional<Serialization::Reader>);
		bool SerializeCustomData(Serialization::Writer) const;
	protected:
		friend struct Reflection::ReflectedType<Physics::CylinderColliderComponent>;
		CylinderColliderComponent(const Deserializer& deserializer, const CylinderInitializationParametersBase&);

		bool GetHeightAndRadius(Math::Radiusf& radiusOut, Math::Lengthf& lengthOut) const;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::CylinderColliderComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::CylinderColliderComponent>(
			Physics::CylinderColliderComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Cylinder Collider"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"radius",
					"{224B9B73-D3FE-4BCB-8C95-077C961A41AF}"_guid,
					MAKE_UNICODE_LITERAL("Cylinder Collider"),
					&Physics::CylinderColliderComponent::SetRadius,
					&Physics::CylinderColliderComponent::GetRadius
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Height"),
					"half_height",
					"{28BFEE31-715D-486C-BCE8-03E9CFB6AF83}"_guid,
					MAKE_UNICODE_LITERAL("Cylinder Collider"),
					&Physics::CylinderColliderComponent::SetHalfHeight,
					&Physics::CylinderColliderComponent::GetHalfHeight
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "62a86996-661f-715a-f9ca-75d38106bbc3"_asset, "5edc8044-ff05-4c39-b59a-29021095f002"_guid
				},
				Entity::IndicatorTypeExtension{
					"d75926f7-c4d4-4cd4-a8b6-ddb82816dc39"_guid,
				}
			}
		);
	};
}
