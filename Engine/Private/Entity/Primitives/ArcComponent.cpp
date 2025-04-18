#include "Entity/Primitives/ArcComponent.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Renderer/Assets/StaticMesh/Primitives/Arc.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity::Primitives
{
	ArcComponent::ArcComponent(const ArcComponent& templateComponent, const Cloner& cloner)
		: ProceduralStaticMeshComponent(templateComponent, cloner)
		, m_angle(templateComponent.m_angle)
		, m_halfHeight(templateComponent.m_halfHeight)
		, m_outer(templateComponent.m_outer)
		, m_inner(templateComponent.m_inner)
		, m_sideCount(templateComponent.m_sideCount)
	{
	}

	ArcComponent::ArcComponent(const Deserializer& deserializer)
		: ArcComponent(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<ArcComponent>().ToString().GetView()))
	{
	}

	ArcComponent::ArcComponent(const Deserializer& deserializer, [[maybe_unused]] Optional<Serialization::Reader> componentSerializer)
		: ProceduralStaticMeshComponent(deserializer)
		, m_angle(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Anglef>("Angle", 90_degrees)
																			: (Math::Anglef)90_degrees
			)
		, m_halfHeight(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("HalfHeight", 0.5_meters)
																			: (Math::Lengthf)0.5_meters
			)
		, m_outer(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Radiusf>("OuterRadius", 1.0_meters)
																			: (Math::Radiusf)1.0_meters
			)
		, m_inner(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Radiusf>("InnerRadius", 0.5_meters)
																			: (Math::Radiusf)0.5_meters
			)
		, m_sideCount(componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<uint16>("SideCount", 16u) : 16u)
	{
	}

	ArcComponent::ArcComponent(Initializer&& initializer)
		: ProceduralStaticMeshComponent(Forward<Initializer>(initializer))
		, m_angle(initializer.m_angle)
		, m_halfHeight(initializer.m_halfHeight)
		, m_outer(initializer.m_outer)
		, m_inner(initializer.m_inner)
		, m_sideCount(initializer.m_sideCount)
	{
	}

	void ArcComponent::OnCreated()
	{
		ProceduralStaticMeshComponent::OnCreated();

		SetMeshGeneration(
			[this](Rendering::StaticObject& object)
			{
				object = Rendering::Primitives::Arc::Create(m_angle, m_halfHeight, m_outer, m_inner, m_sideCount);
			}
		);
	}

	[[maybe_unused]] const bool wasArcRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<ArcComponent>>::Make());
	[[maybe_unused]] const bool wasArcTypeRegistered = Reflection::Registry::RegisterType<ArcComponent>();
}
