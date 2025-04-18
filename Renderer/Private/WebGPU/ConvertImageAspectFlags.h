#pragma once

#include <Renderer/WebGPU/Includes.h>
#include <Renderer/Format.h>

namespace ngine::Rendering
{
#if RENDERER_WEBGPU
	[[nodiscard]] inline constexpr WGPUTextureAspect ConvertImageAspectFlags(const EnumFlags<ImageAspectFlags> flags)
	{
		if (flags == ImageAspectFlags::Depth)
		{
			return WGPUTextureAspect_DepthOnly;
		}
		else if (flags == ImageAspectFlags::Stencil)
		{
			return WGPUTextureAspect_StencilOnly;
		}
		return WGPUTextureAspect_All;
	}
#endif
}
