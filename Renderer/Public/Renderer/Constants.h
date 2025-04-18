#pragma once

#include <Common/Math/CoreNumericTypes.h>

namespace ngine::Rendering
{
	inline static constexpr uint8 MaximumConcurrentFrameCount = 3;

	namespace Internal
	{
		[[nodiscard]] constexpr uint8 GetAllFramesMask()
		{
			uint8 frameMask{0};
			for (uint8 frameIndex = 0; frameIndex < MaximumConcurrentFrameCount; ++frameIndex)
			{
				frameMask |= 1u << frameIndex;
			}
			return frameMask;
		}
	}

	inline static constexpr uint8 AllFramesMask = Internal::GetAllFramesMask();

#define ENABLE_NVIDIA_GPU_CHECKPOINTS (PROFILE_BUILD && PLATFORM_WINDOWS && RENDERER_VULKAN)
#define ENABLE_GPU_CHECKPOINTS ENABLE_NVIDIA_GPU_CHECKPOINTS
#define RENDERER_SUPPORTS_RAYTRACING (RENDERER_VULKAN || RENDERER_METAL)
#define RENDERER_SUPPORTS_PUSH_CONSTANTS (RENDERER_VULKAN || RENDERER_METAL)

#define RENDERER_OBJECT_DEBUG_NAMES (PROFILE_BUILD && !RENDERER_WEBGPU_WGPU_NATIVE)
}
