#pragma once

#include <Common/Asset/AssetFormat.h>

namespace ngine::Scripting
{
	// TODO: Move serialization out
	struct FunctionAsset
	{
		inline static constexpr ngine::Asset::Format AssetFormat = {
			"{DCE92B86-A9A7-4676-9E5D-A09CEA4824F4}"_guid, MAKE_PATH(".function.nasset"), MAKE_PATH(".function")
		};
	};
}
