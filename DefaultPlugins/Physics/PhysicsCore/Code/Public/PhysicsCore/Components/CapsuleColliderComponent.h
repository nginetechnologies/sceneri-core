#pragma once

#include "ColliderComponent.h"

#include <Common/Math/Length.h>
#include <Common/Math/ForwardDeclarations/Radius.h>

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

namespace ngine::Physics
{
	struct BodyComponent;

	struct CapsuleInitializationParametersBase
	{
		Math::Radiusf m_radius = 0.5_meters;
		Math::Lengthf m_halfHeight = 1_meters;
	};

	struct CapsuleInitializationParameters : public CapsuleInitializationParametersBase
	{
		const Material& m_physicalMaterial;
	};

	struct CapsuleColliderComponent final : public ColliderComponent
	{
		static constexpr Guid TypeGuid = "a87d82b2-1cd5-4b81-8103-85248c61d585"_guid;

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

		CapsuleColliderComponent(Initializer&& initializer);
		CapsuleColliderComponent(const CapsuleColliderComponent& templateComponent, const Cloner& cloner);
		CapsuleColliderComponent(const Deserializer& deserializer);

		void SetRadius(const Math::Radiusf radius);
		[[nodiscard]] Math::Radiusf GetRadius() const;

		void SetHalfHeight(const Math::Lengthf height);
		[[nodiscard]] Math::Lengthf GetHalfHeight() const;
	protected:
		friend struct Reflection::ReflectedType<Physics::CapsuleColliderComponent>;
		CapsuleColliderComponent(const Deserializer& deserializer, const CapsuleInitializationParametersBase&);

		bool GetHalfHeightAndRadius(Math::Radiusf& radiusOut, Math::Lengthf& lengthOut) const;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::CapsuleColliderComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::CapsuleColliderComponent>(
			Physics::CapsuleColliderComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Capsule Collider"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Radius"),
					"radius",
					"{38C00F96-0EEF-4F9F-9C47-B3A72BF67ABB}"_guid,
					MAKE_UNICODE_LITERAL("Capsule Collider"),
					&Physics::CapsuleColliderComponent::SetRadius,
					&Physics::CapsuleColliderComponent::GetRadius
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Half Height"),
					"half_height",
					"{E687EA7B-DD6F-4FA4-81E3-76DE052C8E17}"_guid,
					MAKE_UNICODE_LITERAL("Capsule Collider"),
					&Physics::CapsuleColliderComponent::SetHalfHeight,
					&Physics::CapsuleColliderComponent::GetHalfHeight
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "10f8e621-370c-85e3-3f6d-b3e46eaa6f91"_asset, "5edc8044-ff05-4c39-b59a-29021095f002"_guid
				},
				Entity::IndicatorTypeExtension{
					"fbc7ec40-8de2-4014-9311-5a484f312d38"_guid,
				}
			}
		);
	};
}
