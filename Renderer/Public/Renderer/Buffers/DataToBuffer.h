#pragma once

#include <Common/Memory/Containers/ByteView.h>

namespace ngine::Rendering
{
	struct DataToBuffer
	{
		size targetOffset;
		ConstByteView source;
	};
}
