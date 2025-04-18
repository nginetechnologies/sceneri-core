#include <Engine/Entity/Lights/LightComponentFunctions.h>
#include <Engine/Entity/Lights/DirectionalLightComponent.h>
#include <Engine/Entity/Lights/PointLightComponent.h>
#include <Engine/Entity/Lights/SpotLightComponent.h>

#include <Engine/Entity/Component3D.inl>

#include <Common/Reflection/Registry.inl>

namespace ngine::Entity
{
	void SetLightColor(Entity::Component3D& component, const Math::Color color)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Entity::DirectionalLightComponent*> pDirectionalLightComponent = component.As<Entity::DirectionalLightComponent>(sceneRegistry))
		{
			pDirectionalLightComponent->SetColor(color);
		}
		else if (const Optional<Entity::PointLightComponent*> pPointLightComponent = component.As<Entity::PointLightComponent>(sceneRegistry))
		{
			pPointLightComponent->SetColor(color);
		}
		else if (const Optional<Entity::SpotLightComponent*> pSpotLightComponent = component.As<Entity::SpotLightComponent>(sceneRegistry))
		{
			pPointLightComponent->SetColor(color);
		}
	}

	Math::Color GetLightColor(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Entity::DirectionalLightComponent*> pDirectionalLightComponent = component.As<Entity::DirectionalLightComponent>(sceneRegistry))
		{
			return pDirectionalLightComponent->GetColor();
		}
		else if (const Optional<Entity::PointLightComponent*> pPointLightComponent = component.As<Entity::PointLightComponent>(sceneRegistry))
		{
			return pPointLightComponent->GetColor();
		}
		else if (const Optional<Entity::SpotLightComponent*> pSpotLightComponent = component.As<Entity::SpotLightComponent>(sceneRegistry))
		{
			return pSpotLightComponent->GetColor();
		}
		else
		{
			return Math::Zero;
		}
	}

	void SetLightRadius(Entity::Component3D& component, const Math::Radiusf radius)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Entity::PointLightComponent*> pPointLightComponent = component.As<Entity::PointLightComponent>(sceneRegistry))
		{
			pPointLightComponent->SetInfluenceRadius(radius);
		}
		else if (const Optional<Entity::SpotLightComponent*> pSpotLightComponent = component.As<Entity::SpotLightComponent>(sceneRegistry))
		{
			pSpotLightComponent->SetInfluenceRadius(radius);
		}
	}

	Math::Radiusf GetLightRadius(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Entity::PointLightComponent*> pPointLightComponent = component.As<Entity::PointLightComponent>(sceneRegistry))
		{
			return pPointLightComponent->GetInfluenceRadius();
		}
		else if (const Optional<Entity::SpotLightComponent*> pSpotLightComponent = component.As<Entity::SpotLightComponent>(sceneRegistry))
		{
			return pSpotLightComponent->GetInfluenceRadius();
		}
		else
		{
			return Math::Radiusf::FromMeters(0.f);
		}
	}

	void SetLightFieldOfView(Entity::Component3D& component, const Math::Anglef angle)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Entity::SpotLightComponent*> pSpotLightComponent = component.As<Entity::SpotLightComponent>(sceneRegistry))
		{
			pSpotLightComponent->SetFieldOfView(angle);
		}
	}

	Math::Anglef GetLightFieldOfView(Entity::Component3D& component)
	{
		Entity::SceneRegistry& sceneRegistry = component.GetSceneRegistry();
		if (const Optional<Entity::SpotLightComponent*> pSpotLightComponent = component.As<Entity::SpotLightComponent>(sceneRegistry))
		{
			return pSpotLightComponent->GetFieldOfView();
		}
		else
		{
			return Math::Anglef::FromRadians(0.f);
		}
	}

	[[maybe_unused]] inline static const bool wasSetLightColorReflected = Reflection::Registry::RegisterGlobalFunction<&SetLightColor>();
	[[maybe_unused]] inline static const bool wasGetLightColorReflected = Reflection::Registry::RegisterGlobalFunction<&GetLightColor>();

	[[maybe_unused]] inline static const bool wasSetLightRadiusReflected = Reflection::Registry::RegisterGlobalFunction<&SetLightRadius>();
	[[maybe_unused]] inline static const bool wasGetLightRadiusReflected = Reflection::Registry::RegisterGlobalFunction<&GetLightRadius>();

	[[maybe_unused]] inline static const bool wasSetLightFieldOfViewReflected =
		Reflection::Registry::RegisterGlobalFunction<&SetLightFieldOfView>();
	[[maybe_unused]] inline static const bool wasGetLightFieldOfViewReflected =
		Reflection::Registry::RegisterGlobalFunction<&GetLightFieldOfView>();
}
