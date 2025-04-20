#pragma once

#include <Common/Memory/Containers/ForwardDeclarations/Vector.h>
#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>

namespace ngine::Widgets::Style
{
	struct Value;

	using Values = Vector<Value, uint16>;
	using ConstValuesView = ArrayView<const Value, uint16>;
	using ValuesView = ArrayView<Value, uint16>;
}
