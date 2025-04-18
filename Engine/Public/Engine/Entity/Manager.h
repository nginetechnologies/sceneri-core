#pragma once

#include <Engine/Entity/ComponentRegistry.h>
#include <Engine/Entity/Scene/ComponentTemplateCache.h>

#include <Common/System/SystemType.h>

namespace ngine
{
	struct Engine;
}

namespace ngine::Entity
{
	struct Manager final
	{
		inline static constexpr System::Type SystemType = System::Type::EntityManager;

		Manager();
		~Manager();

		[[nodiscard]] PURE_LOCALS_AND_POINTERS const ComponentRegistry& GetRegistry() const
		{
			return m_registry;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS ComponentRegistry& GetRegistry()
		{
			return m_registry;
		}

		[[nodiscard]] PURE_LOCALS_AND_POINTERS ComponentTemplateCache& GetComponentTemplateCache()
		{
			return m_sceneTemplateCache;
		}
		[[nodiscard]] PURE_LOCALS_AND_POINTERS const ComponentTemplateCache& GetComponentTemplateCache() const
		{
			return m_sceneTemplateCache;
		}
	protected:
		ComponentRegistry m_registry;
		ComponentTemplateCache m_sceneTemplateCache;
	};
}
