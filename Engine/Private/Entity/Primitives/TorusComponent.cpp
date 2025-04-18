#include "Entity/Primitives/TorusComponent.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Renderer/Assets/StaticMesh/Primitives/Torus.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity::Primitives
{
	TorusComponent::TorusComponent(const TorusComponent& templateComponent, const Cloner& cloner)
		: ProceduralStaticMeshComponent(templateComponent, cloner)
		, m_radius(templateComponent.m_radius)
		, m_thickness(templateComponent.m_thickness)
		, m_sideCount(templateComponent.m_sideCount)
	{
	}

	TorusComponent::TorusComponent(const Deserializer& deserializer)
		: TorusComponent(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<TorusComponent>().ToString().GetView()))
	{
	}

	TorusComponent::TorusComponent(const Deserializer& deserializer, [[maybe_unused]] Optional<Serialization::Reader> componentSerializer)
		: ProceduralStaticMeshComponent(deserializer)
		, m_radius(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("Radius", (Math::Lengthf)0.05_meters)
																			: (Math::Lengthf)0.05_meters
			)
		, m_thickness(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("Thickness", (Math::Lengthf)0.3_meters)
																			: (Math::Lengthf)0.3_meters
			)
		, m_sideCount(componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<uint16>("SideCount", 8u) : 8u)
	{
	}

	TorusComponent::TorusComponent(Initializer&& initializer)
		: ProceduralStaticMeshComponent(Forward<Initializer>(initializer))
		, m_radius(initializer.m_radius)
		, m_thickness(initializer.m_thickness)
		, m_sideCount(initializer.m_sideCount)
	{
	}

	void TorusComponent::OnCreated()
	{
		ProceduralStaticMeshComponent::OnCreated();

		SetMeshGeneration(
			[this](Rendering::StaticObject& object)
			{
				object = Rendering::Primitives::Torus::Create(m_radius, m_thickness, m_sideCount);
			}
		);
	}

	[[maybe_unused]] const bool wasTorusRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<TorusComponent>>::Make());
	[[maybe_unused]] const bool wasTorusTypeRegistered = Reflection::Registry::RegisterType<TorusComponent>();
}
