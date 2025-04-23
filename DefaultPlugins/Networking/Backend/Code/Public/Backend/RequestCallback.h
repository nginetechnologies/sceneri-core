#pragma once

#include <Common/Function/Function.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>

namespace ngine::Networking::HTTP
{
	struct ResponseCode;
}

namespace ngine::Networking::Backend
{
	struct AssetEntry;

	using RequestCallback =
		Function<void(const bool success, const HTTP::ResponseCode responseCode, const Serialization::Reader responseJson), 24>;
	using StreamResultCallback = Function<void(const Serialization::Reader objectReader, const ConstStringView objectJson), 24>;
	using AssetStreamResultCallback = Function<void(const Optional<Networking::Backend::AssetEntry*> pBackendAssetEntry), 24>;
	using StreamingFinishedCallback = Function<void(const bool success), 24>;

	struct StreamedRequest
	{
		void Parse(String& responseBody);

		uint32 checkedOffset = 0u;
		int32 indendationCount = 0u;
		bool isInString = false;
		StreamResultCallback callback;
	};
}
