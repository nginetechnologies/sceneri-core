#pragma once

#include <Engine/Entity/StaticMeshComponent.h>

namespace ngine::Entity::Primitives
{
	struct BoxComponent : public StaticMeshComponent
	{
		static constexpr Guid TypeGuid = "4b654b7c-5de5-4cf4-8dbf-70bbb0faf8c9"_guid;

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

		BoxComponent(const BoxComponent& templateComponent, const Cloner& cloner);
		BoxComponent(const Deserializer& deserializer);
		BoxComponent(Initializer&& initializer);
		virtual ~BoxComponent() = default;
	protected:
		friend struct Reflection::ReflectedType<Entity::Primitives::BoxComponent>;
	private:
		BoxComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Primitives::BoxComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Primitives::BoxComponent>(
			Entity::Primitives::BoxComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Box"),
			Reflection::TypeFlags(),
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"eebed34e-7cff-7a0b-742a-f92bef66a445"_asset,
				"9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid,
				"d5246193-102f-419b-9be2-fbe48ee5a0fc"_asset
			}}
		);
	};
}
