#pragma once

#include <Common/Plugin/Plugin.h>
#include <Common/Guid.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Asset/Guid.h>
#include <Common/Threading/Mutexes/SharedMutex.h>

#include "JobSystem.h"
#include "MaterialCache.h"
#include "MeshCache.h"

namespace ngine::Asset
{
	struct Manager;
}

#define SHOW_JOLT_DEBUG_VIEW 0
#define ENABLE_JOLT_DEBUG_RENDERER DEVELOPMENT_BUILD&& PLATFORM_WINDOWS&& SHOW_JOLT_DEBUG_VIEW

#if ENABLE_JOLT_DEBUG_RENDERER
namespace JPH
{
	class Font;
}

namespace JPH::DebugRendering
{
	class Renderer;
	class DebugRendererImp;
}
#endif

namespace ngine::Physics
{
	struct Material;

	namespace Data
	{
		struct Scene;
	}

	struct Plugin : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "F6C31290-CB03-452C-B3DE-78F19A8CF943"_guid;

		Plugin(Application&);
		virtual ~Plugin() = default;

		// IPlugin
		virtual void OnLoaded(Application&) override;
		virtual void OnUnloaded(Application&) override;
		// ~IPlugin

		[[nodiscard]] MaterialCache& GetMaterialCache()
		{
			return m_materialCache;
		}
		[[nodiscard]] const MaterialCache& GetMaterialCache() const
		{
			return m_materialCache;
		}

		[[nodiscard]] MeshCache& GetMeshCache()
		{
			return m_meshCache;
		}
		[[nodiscard]] const MeshCache& GetMeshCache() const
		{
			return m_meshCache;
		}
	protected:
		friend Data::Scene;

		JobSystem m_jobSystem;
		MaterialCache m_materialCache;
		MeshCache m_meshCache;

#if ENABLE_JOLT_DEBUG_RENDERER
		/// Debug renderer module
		JPH::DebugRendering::DebugRendererImp* m_pDebugRenderer = nullptr;

		/// Render module
		JPH::DebugRendering::Renderer* m_pRenderer = nullptr;

		JPH::Font* m_pFont;
#endif
	};
}
