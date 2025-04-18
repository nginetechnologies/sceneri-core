#pragma once

#include <Renderer/Wrappers/BlendOperation.h>
#include <Renderer/Wrappers/BlendFactor.h>

namespace ngine::Rendering
{
	struct AttachmentBlendState
	{
		[[nodiscard]] inline bool IsEnabled() const
		{
			return (sourceBlendFactor != BlendFactor::One && sourceBlendFactor != BlendFactor::Zero) | (targetBlendFactor != BlendFactor::Zero);
		}

		BlendFactor sourceBlendFactor{BlendFactor::One};
		BlendFactor targetBlendFactor{BlendFactor::Zero};
		BlendOperation blendOperation{BlendOperation::Add};
	};

	struct ColorAttachmentBlendState : public AttachmentBlendState
	{
		ColorAttachmentBlendState(
			const BlendFactor sourceColorBlendFactor = BlendFactor::One,
			const BlendFactor targetColorBlendFactor = BlendFactor::Zero,
			const BlendOperation colorBlendOperation = BlendOperation::Add
		)
			: AttachmentBlendState{sourceColorBlendFactor, targetColorBlendFactor, colorBlendOperation}
		{
		}
	};

	struct AlphaAttachmentBlendState : public AttachmentBlendState
	{
		AlphaAttachmentBlendState(
			const BlendFactor sourceAlphaBlendFactor = BlendFactor::One,
			const BlendFactor targetAlphaBlendFactor = BlendFactor::Zero,
			const BlendOperation alphaBlendOperation = BlendOperation::Add
		)
			: AttachmentBlendState{sourceAlphaBlendFactor, targetAlphaBlendFactor, alphaBlendOperation}
		{
		}
	};
}
