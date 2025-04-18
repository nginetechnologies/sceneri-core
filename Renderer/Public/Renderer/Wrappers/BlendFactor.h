#pragma once

namespace ngine::Rendering
{
	enum class BlendFactor : uint32
	{
		/* Maps to VkBlendFactor */
		Zero,
		One,
		SourceColor,
		OneMinusSourceColor,
		TargetColor,
		OneMinusTargetColor,
		SourceAlpha,
		OneMinusSourceAlpha,
		TargetAlpha,
		OneMinusTargetAlpha
	};
}
