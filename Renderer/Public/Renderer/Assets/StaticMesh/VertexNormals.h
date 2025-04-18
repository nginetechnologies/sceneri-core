#pragma once

#include <Common/Math/CompressedDirectionAndSign.h>

namespace ngine::Rendering
{
	struct VertexNormals
	{
		Math::CompressedDirectionAndSign normal;
		Math::CompressedDirectionAndSign tangent;
	};
}
