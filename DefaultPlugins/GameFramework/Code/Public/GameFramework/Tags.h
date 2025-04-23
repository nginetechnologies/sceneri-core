#pragma once

#include <Common/Guid.h>

namespace ngine::GameFramework
{
	struct GameplayTag
	{
		static constexpr Guid TypeGuid = "c642f003-618a-45dc-a031-b16f61473103"_guid;
	};

	struct CharacterTag
	{
		static constexpr Guid TypeGuid = "7f567180-238f-4a1b-bcbf-fcd740c8a9ea"_guid;
	};
}

namespace ngine::GameFramework::Tags
{
	inline static constexpr Guid PlayerTagGuid = "d6a09c51-8ccf-416e-8154-66f9404d2a44"_guid;
	inline static constexpr Guid LocalPlayerTagGuid = "44be35b5-5c1c-49b9-a203-784b5b5ea4c6"_guid;
	inline static constexpr Guid VehicleTagGuid = "34ecdec7-3446-404a-9b19-1dac6251bee4"_guid;
	inline static constexpr Guid InteractableObjectTagGuid = "A1B9D085-983F-44D8-9800-B1A26AB1D4A7"_guid;

	inline static constexpr Guid RespawnZoneTagGuid = "a2ef7cd2-0d5f-4ab3-a542-0ae14cb27a43"_guid;
	inline static constexpr Guid EnableOnStart = "21243f2e-905b-40d8-8913-5282b89ac77c"_guid;
	inline static constexpr Guid MainCamera = "b330fe2e-d724-4d92-9eb7-3489887ceacc"_guid;
}

namespace ngine::GameFramework::Tags::FlyingRings
{
	inline static constexpr Guid TileTagGuid = "8875fc7c-16b1-4ce2-8c61-23068fd43bff"_guid;
}

namespace ngine::GameFramework::Tags::TimeAttack
{
	inline static constexpr Guid Checkpoint = "dcd11d59-0a17-4eaa-a93c-60d049c75b19"_guid;
}
