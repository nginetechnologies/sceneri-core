#include "Components/CinematicCameraComponent.h"

#include <Common/Reflection/Registry.inl>

#include <Engine/Entity/Scene/SceneComponent.h>
#include <Engine/Entity/Component3D.h>
#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/Splines/SplineComponent.h>
#include <Engine/Entity/RootSceneComponent.h>

namespace ngine::GameFramework
{
	CinematicCameraComponent::CinematicCameraComponent(const CinematicCameraComponent& templateComponent, const Cloner& cloner)
		: Entity::Data::Component3D(templateComponent, cloner)
	{
	}

	CinematicCameraComponent::CinematicCameraComponent(Initializer&& initializer)
		: Entity::Data::Component3D(initializer)
	{
	}

	CinematicCameraComponent::CinematicCameraComponent(const Entity::Data::Component3D::Deserializer& deserializer)
		: Entity::Data::Component3D(deserializer)
	{
	}

	CinematicCameraComponent::~CinematicCameraComponent() = default;

	[[maybe_unused]] const bool wasCinematicCameraRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<CinematicCameraComponent>>::Make());
	[[maybe_unused]] const bool wasCinematicCameraTypeRegistered = Reflection::Registry::RegisterType<CinematicCameraComponent>();
}
