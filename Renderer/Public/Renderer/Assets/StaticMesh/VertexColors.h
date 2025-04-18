#pragma once

#include <Common/Math/Color.h>
#include <Common/Math/Vectorization/Packed.h>
#include <Common/Memory/Containers/Array.h>

namespace ngine::Rendering
{
	struct alignas(alignof(Math::Vectorization::Packed<float, 4>)) VertexColor : public Math::Color
	{
		using BaseType = Math::Color;
		using BaseType::BaseType;
		using BaseType::operator=;
	};

	using VertexColors = Array<VertexColor, 1>;
}
