#pragma once

#include <Engine/Entity/StaticMeshComponent.h>

namespace ngine::Entity::Primitives
{
	struct SphereComponent : public StaticMeshComponent
	{
		static constexpr Guid TypeGuid = "cc62e6e8-47f8-43c6-bb5c-00a0227e5daf"_guid;

		using BaseType = StaticMeshComponent;
		using InstanceIdentifier = TIdentifier<uint32, 12>;

		struct Initializer : public RenderItemComponent::Initializer
		{
			using BaseType = RenderItemComponent::Initializer;
			using BaseType::BaseType;
			Initializer(BaseType&& initializer, Rendering::MaterialInstanceIdentifier materialInstanceIdentifier)
				: BaseType(Forward<BaseType>(initializer))
				, m_materialInstanceIdentifier(materialInstanceIdentifier)
			{
			}

			Rendering::MaterialInstanceIdentifier m_materialInstanceIdentifier;
		};

		SphereComponent(const SphereComponent& templateComponent, const Cloner& cloner);
		SphereComponent(const Deserializer& deserializer);
		SphereComponent(Initializer&& initializer);
		virtual ~SphereComponent() = default;
	protected:
		friend struct Reflection::ReflectedType<Entity::Primitives::SphereComponent>;
	private:
		SphereComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Primitives::SphereComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Primitives::SphereComponent>(
			Entity::Primitives::SphereComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Sphere"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"04155c0d-13ca-9a02-d93c-cd57afc87356"_asset,
				"9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid,
				"9cf278d4-aa46-4c87-b47f-d59b5dc2bccb"_asset
			}}
		);
	};
}
