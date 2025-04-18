#include "Entity/Primitives/CylinderComponent.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Renderer/Assets/StaticMesh/Primitives/Cylinder.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity::Primitives
{
	CylinderComponent::CylinderComponent(const CylinderComponent& templateComponent, const Cloner& cloner)
		: ProceduralStaticMeshComponent(templateComponent, cloner)
		, m_radius(templateComponent.m_radius)
		, m_height(templateComponent.m_height)
		, m_sideCount(templateComponent.m_sideCount)
		, m_segmentCount(templateComponent.m_segmentCount)
	{
	}

	CylinderComponent::CylinderComponent(const Deserializer& deserializer)
		: CylinderComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<CylinderComponent>().ToString().GetView())
			)
	{
	}

	CylinderComponent::CylinderComponent(
		const Deserializer& deserializer, [[maybe_unused]] Optional<Serialization::Reader> componentSerializer
	)
		: ProceduralStaticMeshComponent(deserializer)
		, m_radius(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("Radius", (Math::Lengthf)0.5_meters)
																			: (Math::Lengthf)0.5_meters
			)
		, m_height(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("Height", (Math::Lengthf)1_meters)
																			: (Math::Lengthf)1_meters
			)
		, m_sideCount(componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<uint16>("SideCount", 8u) : 8u)
		, m_segmentCount(componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<uint16>("SegmentCount", 2u) : 2u)
	{
	}

	CylinderComponent::CylinderComponent(Initializer&& initializer)
		: ProceduralStaticMeshComponent(Forward<Initializer>(initializer))
		, m_radius(initializer.m_radius)
		, m_height(initializer.m_height)
		, m_sideCount(initializer.m_sideCount)
		, m_segmentCount(initializer.m_segmentCount)
	{
	}

	void CylinderComponent::OnCreated()
	{
		ProceduralStaticMeshComponent::OnCreated();

		SetMeshGeneration(
			[this](Rendering::StaticObject& object)
			{
				object = Rendering::Primitives::Cylinder::Create(m_radius, m_height * 0.5f, m_segmentCount, m_sideCount);
			}
		);
	}

	[[maybe_unused]] const bool wasCylinderRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<CylinderComponent>>::Make(
	));
	[[maybe_unused]] const bool wasCylinderTypeRegistered = Reflection::Registry::RegisterType<CylinderComponent>();
}
