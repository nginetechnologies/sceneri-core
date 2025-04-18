#pragma once

#include <Renderer/Metal/Includes.h>
#include <Renderer/ShaderStage.h>

namespace ngine::Rendering
{
#if RENDERER_METAL
	[[nodiscard]] inline constexpr MTLRenderStages ConvertShaderStages(const EnumFlags<ShaderStage> shaderStages)
	{
		MTLRenderStages result{0};
		result |= MTLRenderStageVertex * shaderStages.IsSet(ShaderStage::Vertex);
		result |= MTLRenderStageFragment * shaderStages.IsSet(ShaderStage::Fragment);
		result |= MTLRenderStageTile * shaderStages.IsSet(ShaderStage::Compute);
		return result;
	}
#endif
}
