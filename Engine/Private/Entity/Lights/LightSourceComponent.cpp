#include "Entity/Lights/LightSourceComponent.h"

#include <Common/Serialization/Reader.h>

namespace ngine::Entity
{
	LightSourceComponent::LightSourceComponent(const Deserializer& deserializer)
		: RenderItemComponent(deserializer | ComponentFlags::IsLight)
	{
	}

	LightSourceComponent::LightSourceComponent(Initializer&& initializer)
		: RenderItemComponent(initializer | ComponentFlags::IsLight)
	{
	}
}
