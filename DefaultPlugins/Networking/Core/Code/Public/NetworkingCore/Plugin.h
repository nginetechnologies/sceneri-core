#pragma once

#include <Common/Plugin/Plugin.h>
#include <Common/Guid.h>

namespace ngine::Network
{
	struct Worker;
	struct LocalHost;
	struct LocalClient;

	struct Manager final : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "CBFE8887-9CB2-4703-9328-94F0B8223ECE"_guid;

		Manager(Application&);
		Manager(const Manager&) = delete;
		Manager(Manager&&) = delete;
		Manager& operator=(const Manager&) = delete;
		Manager& operator=(Manager&&) = delete;
		virtual ~Manager();

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		// ~IPlugin
	};
}
