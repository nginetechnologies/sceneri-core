#pragma once

#include <Common/Plugin/Plugin.h>

#include "FontCache.h"

namespace ngine
{
	struct Engine;
}

namespace ngine::Font
{
	struct Manager : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "5DC6A146-4AE7-43E8-9B53-69D46FC184AF"_guid;

		Manager(Application&);
		virtual ~Manager();

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		// ~IPlugin

		[[nodiscard]] Cache& GetCache()
		{
			return m_cache;
		}
		[[nodiscard]] const Cache& GetCache() const
		{
			return m_cache;
		}
	protected:
		Cache m_cache;
	};
}
