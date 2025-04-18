#include "Plugin.h"

#include <Engine/Asset/AssetManager.h>

#include <Common/System/Query.h>

#include "3rdparty/ozz/base/memory/allocator.h"

namespace ngine::Animation
{
	struct Allocator final : public ozz::memory::Allocator
	{
		virtual void* Allocate(size_t size, size_t alignment) override
		{
			if (alignment > sizeof(void*))
			{
				return Memory::AllocateAligned(size, alignment);
			}
			else
			{
				return Memory::Allocate(size);
			}
		}

		virtual void Deallocate(void* pBlock, [[maybe_unused]] size_t alignment) override
		{
			if (alignment > sizeof(void*))
			{
				Memory::DeallocateAligned(pBlock, alignment);
			}
			else
			{
				Memory::Deallocate(pBlock);
			}
		}
	};

	Plugin::Plugin(Application&)
		: m_allocator(Memory::ConstructInPlace)
		, m_skeletonCache(System::Get<Asset::Manager>())
		, m_meshSkinCache(System::Get<Asset::Manager>())
		, m_animationCache(System::Get<Asset::Manager>())
	{
		GetInstance() = this;
	}

	void Plugin::OnLoaded(Application&)
	{
		ozz::memory::SetDefaulAllocator(m_allocator.Get());
	}
}

#if PLUGINS_IN_EXECUTABLE
[[maybe_unused]] static bool entryPoint = ngine::Plugin::Register<ngine::Animation::Plugin>();
#else
extern "C" ANIMATION_EXPORT_API ngine::Plugin* InitializePlugin(ngine::Application& application)
{
	return new ngine::Animation::Plugin(application);
}
#endif
