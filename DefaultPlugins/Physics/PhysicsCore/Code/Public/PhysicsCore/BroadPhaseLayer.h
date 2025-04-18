#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Physics
{
	enum class BroadPhaseLayer : uint8
	{
		Static,
		Dynamic,
		Queries,
		Triggers,
		Gravity,
		Count
	};

	enum class BroadPhaseLayerMask : uint8
	{
		None,
		Static = 1 << (uint8)BroadPhaseLayer::Static,
		Dynamic = 1 << (uint8)BroadPhaseLayer::Dynamic,
		Queries = 1 << (uint8)BroadPhaseLayer::Queries,
		Triggers = 1 << (uint8)BroadPhaseLayer::Triggers,
		Gravity = 1 << (uint8)BroadPhaseLayer::Gravity,
	};
	ENUM_FLAG_OPERATORS(BroadPhaseLayerMask);
}
