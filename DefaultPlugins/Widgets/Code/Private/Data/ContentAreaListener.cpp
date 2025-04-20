#include <Widgets/Data/ContentAreaListener.h>

#include <Engine/Entity/ComponentType.h>
#include <Common/Reflection/Registry.inl>

namespace ngine::Widgets::Data
{
	[[maybe_unused]] const bool wasContentAreaListenerTypeRegistered = Reflection::Registry::RegisterType<Data::ContentAreaListener>();
	[[maybe_unused]] const bool wasContentAreaListenerComponentTypeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<Entity::ComponentType<Data::ContentAreaListener>>::Make());
}
