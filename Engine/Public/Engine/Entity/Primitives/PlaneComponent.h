#pragma once

#include <Engine/Entity/StaticMeshComponent.h>

namespace ngine::Entity::Primitives
{
	struct PlaneComponent : public StaticMeshComponent
	{
		static constexpr Guid TypeGuid = "61d9dc91-39ac-448c-b858-5fff10bf2864"_guid;
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

		PlaneComponent(const PlaneComponent& templateComponent, const Cloner& cloner);
		PlaneComponent(const Deserializer& deserializer);
		PlaneComponent(Initializer&& initializer);
		virtual ~PlaneComponent() = default;
	protected:
		friend struct Reflection::ReflectedType<Entity::Primitives::PlaneComponent>;
	private:
		PlaneComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Primitives::PlaneComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Primitives::PlaneComponent>(
			Entity::Primitives::PlaneComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Plane"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"484572a5-8f60-43ec-0ebc-32da81ad49fc"_asset,
				"9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid,
				"b9a8fc95-12c8-4a67-a428-1d0865e7aa51"_asset
			}}
		);
	};
}
