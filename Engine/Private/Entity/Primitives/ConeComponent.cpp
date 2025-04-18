#include "Entity/Primitives/ConeComponent.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Renderer/Assets/StaticMesh/Primitives/Cone.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity::Primitives
{
	ConeComponent::ConeComponent(const ConeComponent& templateComponent, const Cloner& cloner)
		: ProceduralStaticMeshComponent(templateComponent, cloner)
		, m_radius(templateComponent.m_radius)
		, m_height(templateComponent.m_height)
		, m_sideCount(templateComponent.m_sideCount)
	{
	}

	ConeComponent::ConeComponent(const Deserializer& deserializer)
		: ConeComponent(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<ConeComponent>().ToString().GetView()))
	{
	}

	ConeComponent::ConeComponent(const Deserializer& deserializer, [[maybe_unused]] Optional<Serialization::Reader> componentSerializer)
		: ProceduralStaticMeshComponent(deserializer)
		, m_radius(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("Radius", (Math::Lengthf)0.05_meters)
																			: (Math::Lengthf)0.05_meters
			)
		, m_height(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("Height", (Math::Lengthf)0.3_meters)
																			: (Math::Lengthf)0.3_meters
			)
		, m_sideCount(componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<uint16>("SideCount", 8u) : 8u)
	{
	}

	ConeComponent::ConeComponent(Initializer&& initializer)
		: ProceduralStaticMeshComponent(Forward<Initializer>(initializer))
		, m_radius(initializer.m_radius)
		, m_height(initializer.m_height)
		, m_sideCount(initializer.m_sideCount)
	{
	}

	void ConeComponent::OnCreated()
	{
		ProceduralStaticMeshComponent::OnCreated();

		SetMeshGeneration(
			[this](Rendering::StaticObject& object)
			{
				object = Rendering::Primitives::Cone::Create(m_radius, m_height, m_sideCount);
			}
		);
	}

	[[maybe_unused]] const bool wasConeRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<ConeComponent>>::Make());
	[[maybe_unused]] const bool wasConeTypeRegistered = Reflection::Registry::RegisterType<ConeComponent>();
}
