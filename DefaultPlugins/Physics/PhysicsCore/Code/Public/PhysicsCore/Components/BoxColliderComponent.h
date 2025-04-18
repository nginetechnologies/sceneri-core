#pragma once

#include "ColliderComponent.h"

#include <Engine/Entity/ComponentTypeExtension.h>
#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

namespace ngine::Physics
{
	struct BodyComponent;

	struct BoxColliderComponent final : public ColliderComponent
	{
		static constexpr Guid TypeGuid = "e81f4e62-fee0-4182-9c1a-a47c053b813f"_guid;

		using BaseType = ColliderComponent;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = ColliderComponent::Initializer;
			Initializer(BaseType&& initializer, Math::Vector3f halfExtent = Math::Vector3f{0.5f})
				: BaseType(Forward<BaseType>(initializer))
				, m_halfExtent(halfExtent)
			{
			}
			using BaseType::BaseType;

			Math::Vector3f m_halfExtent{0.5f};
		};

		BoxColliderComponent(Initializer&& initializer);
		BoxColliderComponent(const BoxColliderComponent& templateComponent, const Cloner& cloner);
		BoxColliderComponent(const Deserializer& deserializer);

		void SetHalfExtent(const Math::Vector3f halfExtent);
		[[nodiscard]] Math::Vector3f GetHalfExtent() const;
		[[nodiscard]] Math::Vector3f GetSize() const
		{
			return GetHalfExtent() * 2.f;
		}
	protected:
		friend struct Reflection::ReflectedType<Physics::BoxColliderComponent>;
		BoxColliderComponent(const Deserializer& deserializer, const Math::Vector3f halfExtent);
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::BoxColliderComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::BoxColliderComponent>(
			Physics::BoxColliderComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Box Collider"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Half Extent"),
				"half_extent",
				"{C8C2CFEB-A095-41B4-ADE2-55F5F7D73336}"_guid,
				MAKE_UNICODE_LITERAL("Box Collider"),
				&Physics::BoxColliderComponent::SetHalfExtent,
				&Physics::BoxColliderComponent::GetHalfExtent
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "eebed34e-7cff-7a0b-742a-f92bef66a445"_asset, "5edc8044-ff05-4c39-b59a-29021095f002"_guid
				},
				Entity::IndicatorTypeExtension{
					"cff780bc-0536-457d-84f4-8df184e947dd"_guid,
				}
			}
		);
	};
}
