#pragma once

#include <Renderer/WebGPU/Includes.h>
#include <Renderer/ShaderStage.h>

#include <Common/EnumFlags.h>

namespace ngine::Rendering
{
#if RENDERER_WEBGPU
	[[nodiscard]] inline constexpr WGPUShaderStage ConvertShaderStages(const EnumFlags<ShaderStage> flags)
	{
		int result{WGPUShaderStage_None};
		result |= WGPUShaderStage_Vertex * flags.IsSet(ShaderStage::Vertex);
		result |= WGPUShaderStage_Fragment * flags.IsSet(ShaderStage::Fragment);
		result |= WGPUShaderStage_Compute * flags.IsSet(ShaderStage::Compute);
		return static_cast<WGPUShaderStage>(result);
	}
#endif
}
