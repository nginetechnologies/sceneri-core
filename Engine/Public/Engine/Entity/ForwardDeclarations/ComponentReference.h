#pragma once

namespace ngine::Entity
{
	struct HierarchyComponentBase;
	struct Component2D;
	struct Component3D;

	template<typename ComponentType>
	struct ComponentReference;
	using ComponentReference2D = ComponentReference<Component2D>;
	using ComponentReference3D = ComponentReference<Component3D>;
	using ComponentBaseReference = ComponentReference<HierarchyComponentBase>;
}
