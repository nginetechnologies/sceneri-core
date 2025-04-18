#pragma once

#include <Common/Memory/ForwardDeclarations/Variant.h>
#include <Common/Math/ForwardDeclarations/Color.h>
#include <Common/Math/ForwardDeclarations/Vector2.h>

namespace ngine::Rendering
{
	using PushConstantValue = Variant<Math::Color, float, Math::Vector2f, int32, uint32>;
}
