#pragma once

#include <Renderer/Metal/Includes.h>
#include <Renderer/Wrappers/BlendFactor.h>

namespace ngine::Rendering
{
#if RENDERER_METAL
	[[nodiscard]] inline constexpr MTLBlendFactor ConvertBlendFactor(const BlendFactor blendFactor)
	{
		switch (blendFactor)
		{
			case BlendFactor::Zero:
				return MTLBlendFactorZero;
			case BlendFactor::One:
				return MTLBlendFactorOne;
			case BlendFactor::SourceColor:
				return MTLBlendFactorSourceColor;
			case BlendFactor::OneMinusSourceColor:
				return MTLBlendFactorOneMinusSourceColor;
			case BlendFactor::TargetColor:
				return MTLBlendFactorDestinationColor;
			case BlendFactor::OneMinusTargetColor:
				return MTLBlendFactorOneMinusDestinationColor;
			case BlendFactor::SourceAlpha:
				return MTLBlendFactorSourceAlpha;
			case BlendFactor::OneMinusSourceAlpha:
				return MTLBlendFactorOneMinusSourceAlpha;
			case BlendFactor::TargetAlpha:
				return MTLBlendFactorDestinationAlpha;
			case BlendFactor::OneMinusTargetAlpha:
				return MTLBlendFactorOneMinusDestinationAlpha;
		}
	}
#endif
}
