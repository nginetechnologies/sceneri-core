#pragma once

namespace ngine::Entity
{
	struct Component2D;
	struct Component3D;

	//! Represents a soft reference to an existing component in the scene
	//! No pointers are stored so this can be safely used to avoid keeping a reference to a dangling component
	struct ComponentSoftReference;

	struct ComponentSoftReferences;
}
