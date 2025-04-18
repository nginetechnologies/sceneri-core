#include "Entity/Primitives/PyramidComponent.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Renderer/Assets/StaticMesh/Primitives/Pyramid.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity::Primitives
{
	PyramidComponent::PyramidComponent(const PyramidComponent& templateComponent, const Cloner& cloner)
		: ProceduralStaticMeshComponent(templateComponent, cloner)
		, m_radius(templateComponent.m_radius)
		, m_height(templateComponent.m_height)
	{
	}

	PyramidComponent::PyramidComponent(const Deserializer& deserializer)
		: PyramidComponent(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<PyramidComponent>().ToString().GetView()))
	{
	}

	PyramidComponent::PyramidComponent(const Deserializer& deserializer, [[maybe_unused]] Optional<Serialization::Reader> componentSerializer)
		: ProceduralStaticMeshComponent(deserializer)
		, m_radius(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("Radius", (Math::Lengthf)0.05_meters)
																			: (Math::Lengthf)0.05_meters
			)
		, m_height(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("Height", (Math::Lengthf)0.3_meters)
																			: (Math::Lengthf)0.3_meters
			)
	{
	}

	PyramidComponent::PyramidComponent(Initializer&& initializer)
		: ProceduralStaticMeshComponent(Forward<Initializer>(initializer))
		, m_radius(initializer.m_radius)
		, m_height(initializer.m_height)
	{
	}

	void PyramidComponent::OnCreated()
	{
		ProceduralStaticMeshComponent::OnCreated();

		SetMeshGeneration(
			[this](Rendering::StaticObject& object)
			{
				object = Rendering::Primitives::Pyramid::Create(m_radius, m_height);
			}
		);
	}

	[[maybe_unused]] const bool wasPyramidRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<PyramidComponent>>::Make()
	);
	[[maybe_unused]] const bool wasPyramidTypeRegistered = Reflection::Registry::RegisterType<PyramidComponent>();
}
