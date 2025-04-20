#pragma once

#include <Common/Asset/Guid.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Math/Color.h>
#include <Common/Math/Length.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Storage/Identifier.h>

#include <Renderer/Stages/RenderItemStage.h>
#include <Renderer/Stages/VisibleStaticMeshes.h>

#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Assets/Stage/SceneRenderStageIdentifier.h>

#include <RenderDebug/Pipelines/DrawWireframePipeline.h>

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
}

namespace ngine::Rendering
{
	struct SceneView;
	struct LogicalDevice;
	struct RenderTexture;
}

namespace ngine::Rendering::Debug
{
	struct DrawWireframeStage : public Rendering::RenderItemStage
	{
		DrawWireframeStage(SceneView& sceneView);
		virtual ~DrawWireframeStage();
		inline static constexpr Asset::Guid Guid = "{F894526A-C57E-421E-A577-EB8E655DEB19}"_asset;

		inline static constexpr uint8 SamplerCount = 7;
	protected:
		// RenderItemStage
		virtual void OnBeforeRenderPassDestroyed() override;
		[[nodiscard]] virtual Threading::JobBatch AssignRenderPass(
			const RenderPassView,
			[[maybe_unused]] const Math::Rectangleui outputArea,
			[[maybe_unused]] const Math::Rectangleui fullRenderArea,
			[[maybe_unused]] const uint8 subpassIndex
		) override;

		virtual void OnRenderItemsBecomeVisible(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnVisibleRenderItemsReset(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnRenderItemsBecomeHidden(
			const Entity::RenderItemMask& renderItems,
			SceneBase& scene,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnVisibleRenderItemTransformsChanged(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		[[nodiscard]] virtual Threading::JobBatch LoadRenderItemsResources(const Entity::RenderItemMask& renderItems) override;
		virtual void OnSceneUnloaded() override final;
		virtual void OnActiveCameraPropertiesChanged(
			[[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
		) override;

		[[nodiscard]] virtual bool ShouldRecordCommands() const override final;
		virtual void RecordRenderPassCommands(
			Rendering::RenderCommandEncoder&, const Rendering::ViewMatrices&, const Math::Rectangleui renderArea, const uint8 subpassIndex
		) override;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::ColorAttachmentOutput | PipelineStageFlags::LateFragmentTests | PipelineStageFlags::ComputeShader;
		}

		virtual void OnDisabled(Rendering::CommandEncoderView, PerFrameStagingBuffer&) override
		{
		}
		virtual void OnEnable(Rendering::CommandEncoderView, PerFrameStagingBuffer&) override
		{
		}

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Draw Wireframe Stage";
		}
#endif
		// ~RenderItemStage
	protected:
		const SceneRenderStageIdentifier m_stageIdentifier;

		SceneView& m_sceneView;

		DrawWireframePipeline m_pipeline;

		Rendering::VisibleStaticMeshes m_visibleStaticMeshes;
	};
}
