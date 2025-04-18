#pragma once

#include <Common/Plugin/Plugin.h>
#include <Common/Guid.h>
#include <Common/Memory/UniquePtr.h>

#include "AudioSystem.h"
#include "AudioCache.h"

namespace ngine::Audio
{
	struct Plugin final : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "a4bbf061-5f65-43b1-acc0-711f28b3bb56"_guid;

		Plugin(Application&);
		virtual ~Plugin() = default;

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		virtual void OnUnloaded(Application& application) override;
		// ~IPlugin

		void Initialize();

		bool IsInitialized() const
		{
			return m_isInitialized;
		}

		void PlaySound();

		[[nodiscard]] Cache& GetCache()
		{
			return m_cache;
		}

		static Plugin*& GetInstance()
		{
			static Plugin* pPlugin = nullptr;
			return pPlugin;
		}
	private:
		UniquePtr<SystemInterface> m_pSystem = nullptr;
		Cache m_cache;

		bool m_isInitialized = false;
		unsigned int m_buffer;
	};
}
