#pragma once

#include <Common/Function/Function.h>
#include <Common/Threading/AtomicBool.h>
#include <Common/IO/ForwardDeclarations/URIView.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Pair.h>
#include <Http/RequestType.h>

#include <Backend/RequestCallback.h>

namespace ngine
{
	struct Engine;
}

namespace ngine::Networking::HTTP
{
	struct Plugin;
	struct ResponseCode;
}

namespace ngine::Networking::Backend
{
	struct Plugin;

	struct Server
	{
		Server(Plugin& plugin)
			: m_backend(plugin)
		{
		}

		void RegisterSession(Engine&);
		void
		QueueRequest(const HTTP::RequestType requestType, const IO::URIView endpointName, String&& requestBody, RequestCallback&& callback);
	protected:
		friend struct Backend::Plugin;
		Threading::Atomic<bool> m_hasSessionToken = false;
		String m_sessionToken;
		Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView> m_sessionTokenHeader;

		Plugin& m_backend;
	};
}
