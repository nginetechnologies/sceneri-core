#pragma once

namespace ngine::Entity
{
	enum class ApplyAssetFlags : uint8
	{
		//! Apply asset to every state possible on the component
		Deep = 1 << 0
	};

	ENUM_FLAG_OPERATORS(ApplyAssetFlags);
}
