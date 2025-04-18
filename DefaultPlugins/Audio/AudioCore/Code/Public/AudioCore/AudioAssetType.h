#pragma once

#include <Common/Asset/AssetFormat.h>
#include <Common/Asset/AssetTypeTag.h>

namespace ngine::Audio
{
	inline static constexpr ngine::Asset::Format AssetFormat = {
		"{5985a03a-dfc4-4338-b0cd-09f757c4a114}"_guid, MAKE_PATH(".audio.nasset"), MAKE_PATH(".audio")
	};
}
