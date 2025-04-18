#pragma once

#include "ColliderComponent.h"

#include <Engine/Entity/ComponentTypeExtension.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

namespace ngine::Physics
{
	struct BodyComponent;

	struct SphereColliderComponent final : public ColliderComponent
	{
		static constexpr Guid TypeGuid = "2c2ae148-5dc3-48b3-b1d6-d81913078fe5"_guid;

		using BaseType = ColliderComponent;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = ColliderComponent::Initializer;
			Initializer(BaseType&& initializer, Math::Radiusf radius = 0.5_meters)
				: BaseType(Forward<BaseType>(initializer))
				, m_radius(radius)
			{
			}
			using BaseType::BaseType;

			Math::Radiusf m_radius{0.5_meters};
		};

		SphereColliderComponent(Initializer&& initializer);
		SphereColliderComponent(const SphereColliderComponent& templateComponent, const Cloner& cloner);
		SphereColliderComponent(const Deserializer& deserializer);

#if ENABLE_ASSERTS
		virtual void OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags> flags) override;
#endif

		void SetRadius(const Math::Radiusf radius);
		[[nodiscard]] Math::Radiusf GetRadius() const;
	protected:
		friend struct Reflection::ReflectedType<Physics::SphereColliderComponent>;
		SphereColliderComponent(const Deserializer& deserializer, const Math::Radiusf radius);
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::SphereColliderComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::SphereColliderComponent>(
			Physics::SphereColliderComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Sphere Collider"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Radius"),
				"radius",
				"{A49973C4-B0BA-449B-8172-2A59F38EE997}"_guid,
				MAKE_UNICODE_LITERAL("Sphere Collider"),
				&Physics::SphereColliderComponent::SetRadius,
				&Physics::SphereColliderComponent::GetRadius
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "04155c0d-13ca-9a02-d93c-cd57afc87356"_asset, "5edc8044-ff05-4c39-b59a-29021095f002"_guid
				},
				Entity::IndicatorTypeExtension{
					"6912580c-ce45-4008-89fe-92620ba660ce"_guid,
				}
			}
		);
	};
}
