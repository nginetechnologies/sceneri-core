#pragma once

#include <Engine/Entity/Data/Component3D.h>

namespace ngine::Entity
{
	struct Component3D;
}

namespace ngine::GameFramework
{
	struct CinematicCameraComponent final : public Entity::Data::Component3D
	{
		using BaseType = Entity::Data::Component3D;
		using Initializer = Entity::Data::Component3D::DynamicInitializer;

		CinematicCameraComponent(const CinematicCameraComponent& templateComponent, const Cloner& cloner);
		CinematicCameraComponent(const Deserializer& deserializer);
		CinematicCameraComponent(Initializer&& initializer);
		virtual ~CinematicCameraComponent();
	protected:
		friend struct Reflection::ReflectedType<CinematicCameraComponent>;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<GameFramework::CinematicCameraComponent>
	{
		static constexpr auto Type = Reflection::Reflect<GameFramework::CinematicCameraComponent>(
			"2330d5dd-fe0c-46a5-87ce-0671fe63a91c"_guid,
			MAKE_UNICODE_LITERAL("Cinematic Camera"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"5b417da8-d03a-5493-9990-4cf7954aeae9"_asset,
				"3ca2d65d-1e8b-472d-b419-34982867841f"_guid,
				"6ddc6fb6-8f88-4ed4-abfb-227e86923a06"_asset
			}}
		);
	};
}
