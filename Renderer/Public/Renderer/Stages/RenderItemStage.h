#pragma once

#include <Engine/Entity/RenderItemIdentifier.h>
#include <Engine/Entity/RenderItemMask.h>

#include <Renderer/Stages/SceneRenderStage.h>

#include <Common/Storage/Identifier.h>
#include <Common/Threading/Jobs/JobBatch.h>
#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>

namespace ngine
{
	struct SceneBase;

	namespace Threading
	{
		struct JobBatch;
	}
}

namespace ngine::Rendering
{
	struct BufferViewWithMemory;
	struct StagingBuffer;

	//! Represents a stage that depends on a list of render items collected by octree traversal
	struct RenderItemStage : public SceneRenderStage
	{
		using SceneRenderStage::SceneRenderStage;
		virtual ~RenderItemStage() = default;

		virtual void OnRenderItemsBecomeVisible(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) = 0;
		virtual void OnVisibleRenderItemsReset(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) = 0;
		virtual void OnRenderItemsBecomeHidden(
			const Entity::RenderItemMask& renderItems,
			SceneBase& scene,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) = 0;
		virtual void OnVisibleRenderItemTransformsChanged(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) = 0;
		virtual void OnDisabled(Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer) = 0;
		virtual void OnEnable(Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer) = 0;
		[[nodiscard]] virtual Threading::JobBatch LoadRenderItemsResources(const Entity::RenderItemMask& renderItems);
	};
}
