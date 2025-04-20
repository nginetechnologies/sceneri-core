#pragma once

#include "Widget.h"

#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/HierarchyComponent.inl>

namespace ngine::Widgets
{
	template<typename Type>
	inline Optional<Type*> Widget::EmplaceChildWidget(typename Type::Initializer&& initializer)
	{
		static_assert(!Reflection::GetType<Type>().IsAbstract(), "Cannot instantiate abstract component type!");
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Type>& typeSceneData = *sceneRegistry.GetOrCreateComponentTypeData<Type>();
		Assert(initializer.GetParent() == this);
		return typeSceneData.CreateInstance(Forward<typename Type::Initializer>(initializer));
	}
}
