#include "Entity/SpringComponent.h"
#include "Engine/Entity/ComponentType.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include "Engine/Entity/ComponentType.h"

#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Entity
{
	SpringComponent::SpringComponent(const SpringComponent& templateComponent, const Cloner& cloner)
		: Component3D(templateComponent, cloner)
		, m_ignoreOwnerRotation(templateComponent.m_ignoreOwnerRotation)
		, m_relativeRotation(templateComponent.m_relativeRotation)
		, m_relativeOffset(templateComponent.m_relativeOffset)
	{
	}

	SpringComponent::SpringComponent(const Deserializer& deserializer)
		: Component3D(deserializer)
		, m_relativeRotation(GetWorldRotation())
		, m_relativeOffset(GetRelativeLocation())
	{
		static_cast<Entity::ComponentTypeSceneData<SpringComponent>&>(*GetTypeSceneData()).EnableUpdate(*this);
	}

	SpringComponent::SpringComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer))
		, m_relativeRotation(GetWorldRotation())
		, m_relativeOffset(GetRelativeLocation())
	{
		static_cast<Entity::ComponentTypeSceneData<SpringComponent>&>(*GetTypeSceneData()).EnableUpdate(*this);
	}

	SpringComponent::~SpringComponent()
	{
		[[maybe_unused]] const bool wasErased =
			static_cast<Entity::ComponentTypeSceneData<SpringComponent>&>(*GetTypeSceneData()).DisableUpdate(*this);
		Assert(wasErased);
	}

	void SpringComponent::Update()
	{
		if (m_ignoreOwnerRotation)
		{
			const Math::WorldCoordinate newLocation = GetParent().GetWorldLocation() + m_relativeOffset;
			const bool locationChanged = !newLocation.IsEquivalentTo(GetWorldLocation());
			const bool rotationChanged = !m_relativeRotation.IsEquivalentTo(GetWorldRotation());
			if (locationChanged || rotationChanged)
			{
				SetWorldLocationAndRotation(newLocation, m_relativeRotation);
			}
		}
	}

	void SpringComponent::OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags>)
	{
		Update();
	}

	[[maybe_unused]] const bool wasSpringRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<SpringComponent>>::Make());
	[[maybe_unused]] const bool wasSpringTypeRegistered = Reflection::Registry::RegisterType<SpringComponent>();
}
