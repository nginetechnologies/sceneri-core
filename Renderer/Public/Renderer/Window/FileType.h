#pragma once

#include <Common/IO/PathView.h>
#include <Common/Memory/Containers/ZeroTerminatedStringView.h>

namespace ngine::Rendering
{
	struct FileType
	{
		ConstNativeZeroTerminatedStringView title;
		ConstStringView mimeType;
		IO::PathView extension;
	};
}
