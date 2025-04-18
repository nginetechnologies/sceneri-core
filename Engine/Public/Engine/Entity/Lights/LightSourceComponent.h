#pragma once

#include <Engine/Entity/RenderItemComponent.h>

namespace ngine::Entity
{
	struct LightSourceComponent : public RenderItemComponent
	{
		using InstanceIdentifier = TIdentifier<uint32, 10>;
		using BaseType = RenderItemComponent;
		using RenderItemComponent::RenderItemComponent;

		inline static constexpr float m_intensityCutoff = 0.04f; // below this intensity, the light attenuation is smoothly cutoff to 0
		inline static constexpr float m_gizmoIntensityCutoff = 6.0f;
		inline static constexpr float m_intensityScaling =
			0.1f; // doesn't affect the influence radius, only the final intensity (this is equivalent to changing the final exposure)
		inline static constexpr float m_lightRadius = 0.25f; // actual light size (warning, this constant must match the shader one), this value
		                                                     // might be exposed to users in the future

		LightSourceComponent(Initializer&& initializer);
		LightSourceComponent(const Deserializer& initializer);

		void RefreshLight()
		{
			const Rendering::RenderItemStageMask stages = GetStageMask();
			if (stages.AreAnySet())
			{
				ResetStages(stages);
			}
		}
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::LightSourceComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::LightSourceComponent>(
			"fdbf3e29-bab3-4481-86a4-655ec5164c41"_guid, MAKE_UNICODE_LITERAL("Light Source"), TypeFlags::IsAbstract
		);
	};
}
