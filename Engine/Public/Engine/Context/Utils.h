#pragma once

#include <Common/Memory/Optional.h>
#include <Engine/Entity/DataComponentResult.h>
#include <Engine/Entity/ComponentIdentifier.h>

namespace ngine
{
	struct Guid;
}

namespace ngine::Entity
{
	struct HierarchyComponentBase;
	struct SceneRegistry;
}

namespace ngine::Context
{
	namespace Data
	{
		struct Component;
	}

	struct Utils
	{
		[[nodiscard]] static PURE_STATICS Entity::DataComponentResult<Data::Component>
		FindContext(Entity::HierarchyComponentBase& component, Entity::SceneRegistry& registry);
		[[nodiscard]] static PURE_STATICS Guid
		GetGuid(Guid globalGuid, const Entity::ComponentIdentifier componentIdentifier, Entity::SceneRegistry& registry);
		[[nodiscard]] static PURE_STATICS Guid
		GetGuid(Guid globalGuid, Entity::HierarchyComponentBase& component, Entity::SceneRegistry& registry);
	};
}
