#include "Entity/Lights/SpotLightComponent.h"

#include "Engine/Entity/ComponentTypeSceneData.h"
#include "Engine/Entity/ComponentType.h"

#include <Common/Memory/Move.h>
#include <Common/Memory/Forward.h>
#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>
#include <Common/Reflection/Serialization/Type.h>
#include <Common/Reflection/Serialization/Property.h>
#include <Common/Reflection/Registry.inl>

#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/Material/MaterialInstanceAsset.h>
#include <Renderer/Renderer.h>

namespace ngine::Entity
{
	SpotLightComponent::SpotLightComponent(const SpotLightComponent& templateComponent, const Cloner& cloner)
		: LightSourceComponent(templateComponent, cloner)
		, m_nearPlane(templateComponent.m_nearPlane)
		, m_influenceRadius(templateComponent.m_influenceRadius)
		, m_fieldOfView(templateComponent.m_fieldOfView)
		, m_color(templateComponent.m_color)
		, m_intensity(templateComponent.m_intensity)
	{
		DeriveIntensityFromInfluenceRadius();
	}

	SpotLightComponent::SpotLightComponent(const Deserializer& deserializer)
		: SpotLightComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<SpotLightComponent>().ToString().GetView())
			)
	{
		DeriveIntensityFromInfluenceRadius();
	}

	SpotLightComponent::SpotLightComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer)
		: SpotLightComponent(
				deserializer,
				componentSerializer.IsValid() ? Reflection::GetType<LightInstanceProperties>().DeserializeType(Reflection::TypeDeserializer{
																					*componentSerializer, System::Get<Reflection::Registry>(), *deserializer.m_pJobBatch
																				})
																			: LightInstanceProperties{}
			)
	{
		DeriveIntensityFromInfluenceRadius();
	}

	SpotLightComponent::SpotLightComponent(const Deserializer& deserializer, const LightInstanceProperties& properties)
		: LightSourceComponent(DeserializerWithBounds{deserializer} | Math::BoundingBox(properties.m_influenceRadius))
		, m_nearPlane(properties.m_nearPlane)
		, m_fieldOfView(properties.m_fieldOfView)
		, m_color(properties.m_color)
	{
		SetInfluenceRadius(properties.m_influenceRadius);
	}

	SpotLightComponent::Initializer::Initializer(
		BaseType&& initializer,
		const Math::Lengthf nearPlane,
		const Math::Radiusf radius,
		const Math::Anglef fieldOfView,
		const Math::Color color
	)
		: BaseType(Forward<BaseType>(initializer))
		, SpotLightInstanceProperties{nearPlane, radius, fieldOfView, color}
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		// Enable tile population and shadows stage by default
		m_stageMask.Set(stageCache.FindIdentifier("F141B823-5844-4FBC-B106-0635FF52199C"_asset));
		m_stageMask.Set(stageCache.FindIdentifier("dfca0ef8-eded-4660-8adb-43c0aa8f4e60"_asset));
	}

	SpotLightComponent::SpotLightComponent(Initializer&& initializer)
		: LightSourceComponent(LightSourceComponent::Initializer(initializer) | Math::BoundingBox(initializer.m_influenceRadius))
		, m_nearPlane(initializer.m_nearPlane)
		, m_fieldOfView(initializer.m_fieldOfView)
		, m_color(initializer.m_color)
	{
		SetInfluenceRadius(initializer.m_influenceRadius);
	}

	[[maybe_unused]] const bool wasSpotLightRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<SpotLightComponent>>::Make());
	[[maybe_unused]] const bool wasSpotLightTypeRegistered = Reflection::Registry::RegisterType<SpotLightComponent>();
}
