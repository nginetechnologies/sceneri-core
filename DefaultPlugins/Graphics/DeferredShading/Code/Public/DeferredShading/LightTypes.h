#pragma once

namespace ngine::Entity
{
	struct LightSourceComponent;
}

namespace ngine::Rendering
{
	enum class LightTypes : uint8
	{
		First,
		PointLight = First,
		SpotLight,
		DirectionalLight,
		LastRealLight = DirectionalLight,
		EnvironmentLight,
		Last = EnvironmentLight,
		Count,
		RealLightCount = LastRealLight + 1
	};

	[[nodiscard]] LightTypes GetLightType(const Entity::LightSourceComponent& component);
}
