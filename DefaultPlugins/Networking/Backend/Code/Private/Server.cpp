#include "Server.h"
#include "Common.h"

#include <Engine/Engine.h>

#include <Common/Serialization/Writer.h>
#include <Common/Serialization/Guid.h>
#include <Common/Memory/Containers/Serialization/ArrayView.h>
#include <Common/Memory/Containers/Serialization/UnorderedMap.h>
#include <Common/Serialization/Guid.h>
#include <Common/Serialization/Version.h>
#include <Common/IO/URI.h>
#include <Common/IO/Log.h>

#include <Common/System/Query.h>

#include <Backend/Plugin.h>
#include <Http/ResponseCode.h>

namespace ngine::Networking::Backend
{
	void Server::RegisterSession(Engine& engine)
	{
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		Serialization::Writer writer(serializedData);
		writer.Serialize("is_development", IsDevelopmentMode(Platform::GetEnvironment()));
		writer.Serialize("game_version", engine.GetInfo().GetVersion());

		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			HTTP::RequestType::Post,
			MAKE_URI("server/session"),
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 3>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{
					ConstZeroTerminatedStringView("Content-Type"),
					ConstZeroTerminatedStringView("application/json")
				},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{environment.m_serverAPI.m_versionHeader},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{
					ConstZeroTerminatedStringView("x-server-key"),
					String().Format("{}", String(environment.m_serverAPI.m_apiKey))
				}
			}.GetView(),
			writer.SaveToBuffer<String>(),
			[this]([[maybe_unused]] const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader responseReader)
			{
				if (success && responseReader.IsObject())
				{
					m_sessionToken = *responseReader.Read<String>("token");
					m_sessionTokenHeader = Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{
						ConstZeroTerminatedStringView("x-auth-token"),
						ConstZeroTerminatedStringView(m_sessionToken)
					};
					m_hasSessionToken = true;
				}
				else
				{
					String responseData = responseReader.GetValue().SaveToReadableBuffer<String>();
					LogError("Couldn't register server session! Response code: {}, Data: {}", responseCode.m_code, responseData);
				}
			}
		);
	}

	void Server::QueueRequest(
		const HTTP::RequestType requestType, const IO::URIView endpointName, String&& requestBody, RequestCallback&& callback
	)
	{
		Assert(m_hasSessionToken);
		const Environment& environment = GetEnvironment();
		m_backend.QueueRequestInternal(
			requestType,
			endpointName,
			Array<Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>, 3>{
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{"Content-Type", "application/json"},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{environment.m_serverAPI.m_versionHeader},
				Pair<ConstZeroTerminatedStringView, ConstZeroTerminatedStringView>{m_sessionTokenHeader}
			}.GetView(),
			Forward<String>(requestBody),
			Forward<RequestCallback>(callback)
		);
	}
}
