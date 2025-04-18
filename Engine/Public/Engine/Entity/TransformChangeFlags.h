#pragma once

namespace ngine::Entity
{
	enum class TransformChangeFlags : uint8
	{
		ChangedByPhysics = 1 << 0,
		ChangedByTransformReset = 1 << 1
	};

	ENUM_FLAG_OPERATORS(TransformChangeFlags);
}
