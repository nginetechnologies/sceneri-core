#pragma once

#include <Common/Plugin/Plugin.h>
#include <Common/Guid.h>
#include <Common/Memory/UniquePtr.h>

#include "SkeletonCache.h"
#include "MeshSkinCache.h"
#include "AnimationCache.h"

namespace ngine::Animation
{
	struct Allocator;

	struct Plugin final : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "4CC21FD4-730F-475D-9807-FBF9E5595308"_guid;

		Plugin(Application&);
		virtual ~Plugin() = default;

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		// ~IPlugin

		[[nodiscard]] SkeletonCache& GetSkeletonCache()
		{
			return m_skeletonCache;
		}
		[[nodiscard]] const SkeletonCache& GetSkeletonCache() const
		{
			return m_skeletonCache;
		}

		[[nodiscard]] MeshSkinCache& GetMeshSkinCache()
		{
			return m_meshSkinCache;
		}
		[[nodiscard]] const MeshSkinCache& GetMeshSkinCache() const
		{
			return m_meshSkinCache;
		}

		[[nodiscard]] AnimationCache& GetAnimationCache()
		{
			return m_animationCache;
		}
		[[nodiscard]] const AnimationCache& GetAnimationCache() const
		{
			return m_animationCache;
		}

		static Plugin*& GetInstance()
		{
			static Plugin* pPlugin = nullptr;
			return pPlugin;
		}
	private:
		UniquePtr<Allocator> m_allocator;

		SkeletonCache m_skeletonCache;
		MeshSkinCache m_meshSkinCache;
		AnimationCache m_animationCache;
	};
}
