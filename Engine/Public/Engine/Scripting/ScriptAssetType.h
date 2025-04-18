#pragma once

#include <Common/Asset/AssetFormat.h>

namespace ngine::Scripting
{
	struct ScriptAssetType
	{
		static constexpr IO::PathView FunctionExtension = MAKE_PATH(".script.function");

		static constexpr ngine::Asset::Format AssetFormat = {
			"be3e4b1b-40cb-409f-8bec-bcd10e57a3e2"_guid, MAKE_PATH(".script.nasset"), {}, ngine::Asset::Format::Flags::IsCollection
		};
	};
}
