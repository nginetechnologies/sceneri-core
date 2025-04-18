#pragma once

#include <Common/EnumFlagOperators.h>

namespace ngine::Entity
{
	enum class ComponentFlags : uint16
	{
		IsRootScene = 1 << 0,
		Is3D = 1 << 1,
		Is2D = 1 << 2,
		//! Whether this is a scene composed of multiple meshes that are considered the same as the concept of submaterials
		IsMeshScene = 1 << 3,
		IsLight = 1 << 4,
		IsDestroying = 1 << 5,
		IsDisabledWithChildren = 1 << 6,
		WasDisabledByParent = 1 << 7,
		IsDetachedFromTree = 1 << 8,
		WasDetachedFromOctreeByParent = 1 << 9,
		IsConstructing = 1 << 10,
		IsDisabled = 1 << 11,
		//! Whether simulations / updates in this component should be paused
		IsSimulationPaused = 1 << 12,
		//! Set when this component is referenced by a component soft reference
		IsReferenced = 1 << 13,
		DisableCloning = 1 << 14,
		SaveToDisk = 1 << 15,
		IsDisabledFromScene = IsDisabledWithChildren | IsDetachedFromTree,
		IsDisabledFromAnySource = IsDisabled | IsDisabledWithChildren | WasDisabledByParent,
		IsDetachedFromTreeFromAnySource = IsDetachedFromTree | WasDetachedFromOctreeByParent | IsConstructing,
		//! Flags that should be skipped when serializing to disk
		SkipSerialize = IsRootScene | Is3D | Is2D | IsMeshScene | IsLight | IsDestroying | WasDisabledByParent | WasDetachedFromOctreeByParent |
		                SaveToDisk,
		DynamicFlags = IsDestroying | IsDisabled | IsDisabledWithChildren | WasDisabledByParent | IsDetachedFromTree |
		               WasDetachedFromOctreeByParent | IsSimulationPaused | IsReferenced
	};
	ENUM_FLAG_OPERATORS(ComponentFlags);
}
