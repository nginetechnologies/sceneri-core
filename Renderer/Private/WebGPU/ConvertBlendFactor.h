#pragma once

#include <Renderer/WebGPU/Includes.h>
#include <Renderer/Wrappers/BlendFactor.h>

namespace ngine::Rendering
{
#if RENDERER_WEBGPU
	[[nodiscard]] inline constexpr WGPUBlendFactor ConvertBlendFactor(const BlendFactor blendFactor)
	{
		switch (blendFactor)
		{
			case BlendFactor::Zero:
				return WGPUBlendFactor_Zero;
			case BlendFactor::One:
				return WGPUBlendFactor_One;
			case BlendFactor::SourceColor:
				return WGPUBlendFactor_Src;
			case BlendFactor::OneMinusSourceColor:
				return WGPUBlendFactor_OneMinusSrc;
			case BlendFactor::TargetColor:
				return WGPUBlendFactor_Dst;
			case BlendFactor::OneMinusTargetColor:
				return WGPUBlendFactor_OneMinusDst;
			case BlendFactor::SourceAlpha:
				return WGPUBlendFactor_SrcAlpha;
			case BlendFactor::OneMinusSourceAlpha:
				return WGPUBlendFactor_OneMinusSrcAlpha;
			case BlendFactor::TargetAlpha:
				return WGPUBlendFactor_DstAlpha;
			case BlendFactor::OneMinusTargetAlpha:
				return WGPUBlendFactor_OneMinusDstAlpha;
		}
		ExpectUnreachable();
	}
#endif
}
