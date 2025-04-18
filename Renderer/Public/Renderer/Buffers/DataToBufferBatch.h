#pragma once

#include <Renderer/Buffers/BufferView.h>
#include <Renderer/Buffers/DataToBuffer.h>

namespace ngine::Rendering
{
	struct DataToBufferBatch
	{
		BufferView targetBuffer;
		ArrayView<const DataToBuffer, uint16> regions;
	};
}
