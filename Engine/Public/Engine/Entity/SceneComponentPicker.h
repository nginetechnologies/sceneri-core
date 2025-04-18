#pragma once

#include "ComponentPicker.h"

namespace ngine::Entity
{
	struct SceneComponentPicker : public ComponentPicker<Component3D>
	{
		using BaseType = ComponentPicker<Component3D>;
		using BaseType::BaseType;
		using BaseType::operator=;
	};
}
