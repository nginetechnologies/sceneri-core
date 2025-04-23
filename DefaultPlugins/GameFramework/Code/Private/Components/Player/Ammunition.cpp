#include <GameFramework/Components/Player/Ammunition.h>

#include <Common/Reflection/Registry.inl>
#include <Engine/Entity/ComponentType.h>

namespace ngine::GameFramework
{

	[[maybe_unused]] const bool wasAmmunitionRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Ammunition>>::Make());
	[[maybe_unused]] const bool wasAmmunitionTypeRegistered = Reflection::Registry::RegisterType<Ammunition>();
}
