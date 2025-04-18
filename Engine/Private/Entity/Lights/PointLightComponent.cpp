#include "Entity/Lights/PointLightComponent.h"

#include "Engine/Entity/ComponentTypeSceneData.h"
#include "Engine/Entity/ComponentType.h"

#include <Common/Math/Power.h>
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
	PointLightComponent::PointLightComponent(const PointLightComponent& templateComponent, const Cloner& cloner)
		: LightSourceComponent(templateComponent, cloner)
		, m_influenceRadius(templateComponent.m_influenceRadius)
		, m_color(templateComponent.m_color)
		, m_intensity(templateComponent.m_intensity)
		, m_gizmoRadius(templateComponent.m_gizmoRadius)
	{
		ComputeDerivedQuantitiesFromInfluenceRadius();
	}

	PointLightComponent::PointLightComponent(const Deserializer& deserializer)
		: PointLightComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<PointLightComponent>().ToString().GetView())
			)
	{
		ComputeDerivedQuantitiesFromInfluenceRadius();
	}

	PointLightComponent::PointLightComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer)
		: PointLightComponent(
				deserializer,
				componentSerializer.IsValid() ? Reflection::GetType<LightInstanceProperties>().DeserializeType(Reflection::TypeDeserializer{
																					*componentSerializer, System::Get<Reflection::Registry>(), *deserializer.m_pJobBatch
																				})
																			: LightInstanceProperties{}
			)
	{
		ComputeDerivedQuantitiesFromInfluenceRadius();
	}

	PointLightComponent::PointLightComponent(const Deserializer& deserializer, const LightInstanceProperties& properties)
		: LightSourceComponent(DeserializerWithBounds{deserializer} | Math::BoundingBox(properties.GetInfluenceRadius()))
		, m_influenceRadius(properties.m_influenceRadius)
		, m_color(properties.m_color)
		, m_intensity(ComputeIntensity())
		, m_gizmoRadius(ComputeGizmoRadius())
	{
		SetInfluenceRadius(properties.m_influenceRadius);
	}

	PointLightComponent::Initializer::Initializer(BaseType&& initializer, const Math::Radiusf radius, const Math::Color color)
		: BaseType(Forward<BaseType>(initializer))
		, PointLightInstanceProperties{radius, color}
	{
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		// Enable tile population and shadows stage by default
		m_stageMask.Set(stageCache.FindIdentifier("F141B823-5844-4FBC-B106-0635FF52199C"_asset));
		m_stageMask.Set(stageCache.FindIdentifier("dfca0ef8-eded-4660-8adb-43c0aa8f4e60"_asset));
	}

	PointLightComponent::PointLightComponent(Initializer&& initializer)
		: LightSourceComponent(LightSourceComponent::Initializer(initializer) | Math::BoundingBox(initializer.GetInfluenceRadius()))
		, m_color(initializer.m_color)
	{
		SetInfluenceRadius(initializer.m_influenceRadius);
	}

	float PointLightComponent::ComputeIntensity() const
	{
		return m_intensityCutoff * Math::Power(m_influenceRadius.GetUnits() / m_lightRadius, 2.0f);
	}

	Math::Radiusf PointLightComponent::ComputeGizmoRadius() const
	{
		const float a = Math::Sqrt((m_intensity / m_intensityScaling) / m_gizmoIntensityCutoff);
		return Math::Radiusf::FromMeters(a * m_lightRadius);
	}

	void PointLightComponent::ComputeDerivedQuantitiesFromInfluenceRadius()
	{
		// derive light intensity based on influence radius, based on https://imdoingitwrong.wordpress.com/2011/01/31/light-attenuation/
		m_intensity = ComputeIntensity();
		Assert(m_gizmoRadius.GetMeters() > 0.f);
		m_gizmoRadius = ComputeGizmoRadius();
	}

	void PointLightComponent::ComputeDerivedQuantitiesFromGizmoRadius()
	{
		// derive light intensity based on influence radius, based on https://imdoingitwrong.wordpress.com/2011/01/31/light-attenuation/
		const float a = 1.0f + (m_gizmoRadius - m_lightRadius) / m_lightRadius;
		m_intensity = m_intensityScaling * m_gizmoIntensityCutoff * a * a;
		m_influenceRadius = Math::Radiusf::FromMeters(m_lightRadius * Math::Sqrt(m_intensity / m_intensityCutoff));
	}

	[[maybe_unused]] const bool wasPointLightRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<PointLightComponent>>::Make());
	[[maybe_unused]] const bool wasPointLightTypeRegistered = Reflection::Registry::RegisterType<PointLightComponent>();
}
