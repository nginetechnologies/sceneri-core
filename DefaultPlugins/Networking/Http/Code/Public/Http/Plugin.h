#pragma once

#include <Common/Plugin/Plugin.h>
#include <Common/Guid.h>

#include <Common/Memory/UniqueRef.h>
#include <Common/Memory/UniquePtr.h>

#include <Common/Threading/Jobs/JobPriority.h>

#define HTTP_DEBUG DEBUG_BUILD

namespace ngine::Asset
{
	struct Manager;
}

namespace ngine::Networking::HTTP
{
	struct Worker;

	struct Plugin final : public ngine::Plugin
	{
		inline static constexpr Guid Guid = "4732DB7A-619B-421E-8569-BF31F607BBB7"_guid;

		Plugin(Application&);
		Plugin(const Plugin&) = delete;
		Plugin(Plugin&&) = delete;
		Plugin& operator=(const Plugin&) = delete;
		Plugin& operator=(Plugin&&) = delete;
		virtual ~Plugin();

		// IPlugin
		virtual void OnLoaded(Application& application) override;
		// ~IPlugin

		[[nodiscard]] UniquePtr<Worker> CreateWorker(const Threading::JobPriority priority, const Asset::Manager& assetManager) const;
		[[nodiscard]] Worker& GetHighPriorityWorker() const
		{
			return *m_pHighPriorityWorker;
		}
		[[nodiscard]] Worker& GetLowPriorityWorker() const
		{
			return *m_pLowPriorityWorker;
		}
	protected:
		UniquePtr<Worker> m_pHighPriorityWorker;
		UniquePtr<Worker> m_pLowPriorityWorker;
	};
}
