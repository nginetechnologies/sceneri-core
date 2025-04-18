#pragma once

namespace ngine::Rendering
{
#define ENABLE_RENDERDOC_API RENDERER_VULKAN
	namespace RenderDoc
	{
#if ENABLE_RENDERDOC_API
		extern void StartFrameCapture();
		extern void CancelFrameCapture();
		extern void EndFrameCapture();

#endif
	};
}
