#pragma once

#include <Renderer/Stages/Stage.h>

#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>

namespace ngine::Rendering
{
	struct CommandEncoderView;
	struct PerFrameStagingBuffer;

	struct SceneRenderStage : public Stage
	{
		using Stage::Stage;
		virtual ~SceneRenderStage() = default;

		virtual void OnSceneUnloaded()
		{
		}

		virtual void OnActiveCameraPropertiesChanged(
			[[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
		) = 0;
	};
}
