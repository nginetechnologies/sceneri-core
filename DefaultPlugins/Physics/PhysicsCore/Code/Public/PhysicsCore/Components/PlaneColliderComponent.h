#pragma once

#include "ColliderComponent.h"

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

namespace ngine::Physics
{
	struct BodyComponent;

	struct PlaneColliderComponent final : public ColliderComponent
	{
		static constexpr Guid TypeGuid = "4c1071cc-31cc-4dbe-a810-96f2c521a280"_guid;

		using BaseType = ColliderComponent;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = ColliderComponent::Initializer;
			Initializer(BaseType&& initializer, Math::Vector2f halfExtent = Math::Vector2f{0.5f})
				: BaseType(Forward<BaseType>(initializer))
				, m_halfExtent(halfExtent)
			{
			}
			using BaseType::BaseType;

			Math::Vector2f m_halfExtent{0.5f};
		};

		PlaneColliderComponent(Initializer&& initializer);
		PlaneColliderComponent(const PlaneColliderComponent& templateComponent, const Cloner& cloner);
		PlaneColliderComponent(const Deserializer& deserializer);

		void DeserializeCustomData(const Optional<Serialization::Reader>);
		bool SerializeCustomData(Serialization::Writer) const;

		void SetHalfExtent(const Math::Vector2f halfExtent);
		[[nodiscard]] Math::Vector2f GetHalfExtent() const;
	protected:
		friend struct Reflection::ReflectedType<Physics::PlaneColliderComponent>;
		PlaneColliderComponent(const Deserializer& deserializer, const Math::Vector2f halfExtent);
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::PlaneColliderComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::PlaneColliderComponent>(
			Physics::PlaneColliderComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Plane Collider"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Half Extent"),
				"half_extent",
				"{379FAD65-D37B-4DF3-AC85-CE9709ED0367}"_guid,
				MAKE_UNICODE_LITERAL("Plane Collider"),
				&Physics::PlaneColliderComponent::SetHalfExtent,
				&Physics::PlaneColliderComponent::GetHalfExtent
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "484572a5-8f60-43ec-0ebc-32da81ad49fc"_asset, "5edc8044-ff05-4c39-b59a-29021095f002"_guid
				},
				Entity::IndicatorTypeExtension{
					"662e1680-f285-4895-adf8-93fcadbf1de3"_guid,
				}
			}
		);
	};
}
