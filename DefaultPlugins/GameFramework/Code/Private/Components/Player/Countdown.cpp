#include "Components/Player/Countdown.h"

#include <Common/Reflection/Registry.inl>
#include <Engine/Entity/ComponentType.h>

namespace ngine::GameFramework
{
	Countdown::Countdown(const Deserializer&)
	{
	}

	Countdown::Countdown(const Countdown& templateComponent, const Cloner&)
		: m_duration{templateComponent.m_duration}
	{
	}

	Countdown::Countdown(Initializer&&)
	{
	}

	void Countdown::ApplyDuration(const ValueType value)
	{
		if (value <= 0_seconds)
		{
			m_duration = 0_seconds;
			OnComplete();
		}
		else
		{
			m_duration = value;
		}
	}

	[[maybe_unused]] const bool wasCountdownComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Countdown>>::Make());
	[[maybe_unused]] const bool wasCountdownComponentTypeRegistered = Reflection::Registry::RegisterType<Countdown>();
}
