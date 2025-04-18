#pragma once

#include <Common/Asset/Picker.h>

namespace ngine::Animation
{
	struct Skeleton;

	struct AnimationPicker : public Asset::Picker
	{
		inline static constexpr Guid TypeGuid = "dc24a313-c4b7-4b7b-aafb-24f374b8ddd1"_guid;

		Optional<const Skeleton*> m_pSkeleton;
	};
}
