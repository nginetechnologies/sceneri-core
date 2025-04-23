#pragma once

#include <Common/Memory/Containers/StringView.h>
#include <Common/Memory/Pair.h>
#include <Common/IO/URIView.h>
#include <Common/Platform/Environment.h>

namespace ngine::Networking::Backend
{
	struct API
	{
		ConstStringView m_apiKey;
		Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView> m_versionHeader;
	};

	using GameAPI = API;
	using ServerAPI = API;

#define INCLUDE_BACKEND_GAME_API 1
#define INCLUDE_BACKEND_SERVER_API 1

	struct Environment
	{
		IO::URIView m_backendURI;
		IO::URIView m_phpBackendURI;
		ConstZeroTerminatedStringView m_backendAPIDomainKey;
#if INCLUDE_BACKEND_GAME_API
		GameAPI m_gameAPI;
#endif
#if INCLUDE_BACKEND_SERVER_API
		ServerAPI m_serverAPI;
#endif
	};

	inline static constexpr Environment ProductionEnvironment
	{
		MAKE_URI("https://api.sceneri.com"), MAKE_URI("https://api.sceneri.com"), BACKEND_PRODUCTION_DOMAIN_KEY
#if INCLUDE_BACKEND_GAME_API
			,
			GameAPI{BACKEND_PRODUCTION_GAME_KEY, {"LL-Version", "2021-03-01"}}
#endif
#if INCLUDE_BACKEND_SERVER_API
		,
			ServerAPI
		{
			BACKEND_PRODUCTION_SERVER_KEY,
			{
				"LL-Version", "2021-03-01"
			}
		}
#endif
	};

	inline static constexpr Environment InternalDevelopmentStagingEnvironment
	{
		MAKE_URI("https://api.sceneri.com"), MAKE_URI("https://api.sceneri.com"), BACKEND_STAGING_DOMAIN_KEY
#if INCLUDE_BACKEND_GAME_API
			,
			GameAPI{BACKEND_STAGING_GAME_KEY, {"LL-Version", "2021-03-01"}}
#endif
#if INCLUDE_BACKEND_SERVER_API
		,
			ServerAPI
		{
			BACKEND_STAGING_SERVER_KEY,
			{
				"LL-Version", "2021-03-01"
			}
		}
#endif
	};

	inline static constexpr Environment LocalBackendDevelopmentEnvironment
	{
		MAKE_URI("https://api.sceneri.com"), MAKE_URI("https://api.sceneri.com"), BACKEND_LOCAL_DOMAIN_KEY
#if INCLUDE_BACKEND_GAME_API
			,
			GameAPI{BACKEND_LOCAL_GAME_KEY, {"LL-Version", "2021-03-01"}}
#endif
#if INCLUDE_BACKEND_SERVER_API
		,
			ServerAPI
		{
			BACKEND_LOCAL_SERVER_KEY,
			{
				"LL-Version", "2021-03-01"
			}
		}
#endif
	};

	[[nodiscard]] inline static const Environment& GetEnvironment()
	{
		switch (Platform::GetEnvironment())
		{
			case Platform::Environment::InternalDevelopment:
			case Platform::Environment::InternalStaging:
				return InternalDevelopmentStagingEnvironment;
			case Platform::Environment::LocalBackendDevelopment:
				return LocalBackendDevelopmentEnvironment;
			case Platform::Environment::Staging:
			case Platform::Environment::Live:
				return ProductionEnvironment;
		}
		ExpectUnreachable();
	}

	[[nodiscard]] static constexpr bool IsDevelopmentMode(const Platform::Environment environment)
	{
		switch (environment)
		{
			case Platform::Environment::InternalDevelopment:
			case Platform::Environment::LocalBackendDevelopment:
			case Platform::Environment::InternalStaging:
				return true;
			case Platform::Environment::Staging:
				return true;
			case Platform::Environment::Live:
				return false;
		}
		ExpectUnreachable();
	}
}
