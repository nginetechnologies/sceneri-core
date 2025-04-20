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

#include <RenderDebug/Pipelines/DrawNormalsPipeline.h>

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
	struct DrawNormalsStageBase : public Rendering::RenderItemStage
	{
		DrawNormalsStageBase(
			SceneView& sceneView, const DrawNormalsPipeline::Type type, const Guid guid, const Math::Color color, const Math::Lengthf length
		);
		virtual ~DrawNormalsStageBase();

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

		[[nodiscard]] virtual bool ShouldRecordCommands() const override final;
		virtual void RecordRenderPassCommands(
			Rendering::RenderCommandEncoder&, const Rendering::ViewMatrices&, const Math::Rectangleui renderArea, const uint8 subpassIndex
		) override;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::ColorAttachmentOutput | PipelineStageFlags::LateFragmentTests | PipelineStageFlags::ComputeShader;
		}

		virtual void OnActiveCameraPropertiesChanged(
			[[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnDisabled(Rendering::CommandEncoderView, PerFrameStagingBuffer&) override
		{
		}
		virtual void OnEnable(Rendering::CommandEncoderView, PerFrameStagingBuffer&) override
		{
		}

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Draw Normals Stage";
		}
#endif
		// ~RenderItemStage
	protected:
		const Guid m_guid;
		const SceneRenderStageIdentifier m_stageIdentifier;

		SceneView& m_sceneView;
		Math::Color m_color;
		Math::Lengthf m_length;

		Array<Framebuffer, Rendering::MaximumConcurrentFrameCount> m_framebuffers;
		DrawNormalsPipeline m_pipeline;

		Rendering::VisibleStaticMeshes m_visibleStaticMeshes;
	};

	struct DrawNormalsStage final : public DrawNormalsStageBase
	{
		inline static constexpr Asset::Guid Guid = "{D66609E5-8C0F-40AF-9DC3-630344E2E12A}"_asset;
		DrawNormalsStage(SceneView& sceneView)
			: DrawNormalsStageBase(sceneView, DrawNormalsPipeline::Type::Normals, Guid, "#335C9B"_colorf, 0.1_meters)
		{
		}
	};

	struct DrawTangentsStage final : public DrawNormalsStageBase
	{
		inline static constexpr Asset::Guid Guid = "{422E5D9E-C237-41FB-A5AC-BDDE6AF2C223}"_asset;
		DrawTangentsStage(SceneView& sceneView)
			: DrawNormalsStageBase(sceneView, DrawNormalsPipeline::Type::Tangents, Guid, "#9B3333"_colorf, 0.1_meters)
		{
		}
	};

	struct DrawBitangentsStage final : public DrawNormalsStageBase
	{
		inline static constexpr Asset::Guid Guid = "{4F3FF933-AA21-48EA-B751-8BBA72C36D24}"_asset;
		DrawBitangentsStage(SceneView& sceneView)
			: DrawNormalsStageBase(sceneView, DrawNormalsPipeline::Type::Bitangents, Guid, "#429B33"_colorf, 0.1_meters)
		{
		}
	};
}
