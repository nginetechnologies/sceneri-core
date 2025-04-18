#include "Entity/Primitives/ArrowComponent.h"

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Renderer/Assets/StaticMesh/Primitives/Arrow.h>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity::Primitives
{
	ArrowComponent::ArrowComponent(const ArrowComponent& templateComponent, const Cloner& cloner)
		: ProceduralStaticMeshComponent(templateComponent, cloner)
		, m_shaftRadius(templateComponent.m_shaftRadius)
		, m_shaftHeight(templateComponent.m_shaftHeight)
		, m_tipRadius(templateComponent.m_tipRadius)
		, m_tipHeight(templateComponent.m_tipHeight)
		, m_sideCount(templateComponent.m_sideCount)
	{
	}

	ArrowComponent::ArrowComponent(const Deserializer& deserializer)
		: ArrowComponent(deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<ArrowComponent>().ToString().GetView()))
	{
	}

	ArrowComponent::ArrowComponent(const Deserializer& deserializer, [[maybe_unused]] Optional<Serialization::Reader> componentSerializer)
		: ProceduralStaticMeshComponent(deserializer)
		, m_shaftRadius(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("ShaftRadius", (Math::Lengthf)0.05_meters)
																			: (Math::Lengthf)0.05_meters
			)
		, m_shaftHeight(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("ShaftHeight", (Math::Lengthf)0.75_meters)
																			: (Math::Lengthf)0.75_meters
			)
		, m_tipRadius(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("TipRadius", (Math::Lengthf)0.3_meters)
																			: (Math::Lengthf)0.3_meters
			)
		, m_tipHeight(
				componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Math::Lengthf>("TipHeight", (Math::Lengthf)0.05_meters)
																			: (Math::Lengthf)0.05_meters
			)
		, m_sideCount(componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<uint16>("SideCount", 8u) : 8u)
	{
	}

	ArrowComponent::ArrowComponent(Initializer&& initializer)
		: ProceduralStaticMeshComponent(Forward<Initializer>(initializer))
		, m_shaftRadius(initializer.m_shaftRadius)
		, m_shaftHeight(initializer.m_shaftHeight)
		, m_tipRadius(initializer.m_tipRadius)
		, m_tipHeight(initializer.m_tipHeight)
		, m_sideCount(initializer.m_sideCount)
	{
	}

	void ArrowComponent::OnCreated()
	{
		ProceduralStaticMeshComponent::OnCreated();

		SetMeshGeneration(
			[this](Rendering::StaticObject& object)
			{
				object = Rendering::Primitives::Arrow::Create(m_shaftRadius, m_tipRadius, m_shaftHeight, m_tipHeight, m_sideCount);
			}
		);
	}

	[[maybe_unused]] const bool wasArrowRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<ArrowComponent>>::Make());
	[[maybe_unused]] const bool wasArrowTypeRegistered = Reflection::Registry::RegisterType<ArrowComponent>();
}
