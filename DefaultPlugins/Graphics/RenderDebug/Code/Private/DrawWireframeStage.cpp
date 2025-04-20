#include "DrawWireframeStage.h"

#include <Common/Threading/Jobs/AsyncJob.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>
#include <Common/Math/Power.h>

#include <Engine/Threading/JobRunnerThread.h>

#include <Engine/Entity/Lights/SpotLightComponent.h>
#include <Engine/Entity/Lights/DirectionalLightComponent.h>
#include <Engine/Entity/Lights/PointLightComponent.h>
#include <Engine/Entity/Lights/EnvironmentLightComponent.h>
#include <Engine/Entity/CameraComponent.h>
#include <Engine/Scene/Scene.h>
#include <Common/System/Query.h>

#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Commands/RenderCommandEncoder.h>
#include <Renderer/Commands/ClearValue.h>
#include <Renderer/Scene/SceneView.h>
#include <Renderer/Scene/SceneData.h>
#include <Renderer/Renderer.h>
#include <Renderer/Stages/StartFrameStage.h>
#include <Renderer/Framegraph/Framegraph.h>
#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/RenderOutput/RenderOutput.h>

#include <Renderer/Wrappers/AttachmentReference.h>
#include <Renderer/Wrappers/AttachmentDescription.h>
#include <Renderer/Wrappers/SubpassDependency.h>

namespace ngine::Rendering::Debug
{
	DrawWireframeStage::DrawWireframeStage(SceneView& sceneView)
		: RenderItemStage(sceneView.GetLogicalDevice(), Threading::JobPriority::Draw)
		, m_stageIdentifier(sceneView.GetLogicalDevice().GetRenderer().GetStageCache().FindIdentifier(Guid))
		, m_sceneView(sceneView)
		, m_pipeline(
				m_sceneView.GetLogicalDevice(), ArrayView<const DescriptorSetLayoutView>{m_sceneView.GetTransformBufferDescriptorSetLayout()}
			)
	{
		m_sceneView.RegisterRenderItemStage(m_stageIdentifier, *this);
		m_sceneView.SetStageDependentOnCameraProperties(m_stageIdentifier);
	}

	DrawWireframeStage::~DrawWireframeStage()
	{
		LogicalDevice& logicalDevice = m_sceneView.GetLogicalDevice();

		m_pipeline.Destroy(logicalDevice);

		m_visibleStaticMeshes.Destroy(logicalDevice);

		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();
		const SceneRenderStageIdentifier stageIdentifier = stageCache.FindIdentifier(Guid);
		m_sceneView.DeregisterRenderItemStage(stageIdentifier);
	}

	void DrawWireframeStage::OnRenderItemsBecomeVisible(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		// Temporary until we refactor buffer creation to use a pool
		constexpr uint32 MaximumVisibleRenderItemCount = 512;
		m_visibleStaticMeshes.AddRenderItems(
			m_sceneView,
			*m_sceneView.GetSceneChecked(),
			m_sceneView.GetLogicalDevice(),
			m_stageIdentifier,
			renderItems,
			MaximumVisibleRenderItemCount,
			graphicsCommandEncoder,
			perFrameStagingBuffer
		);
	}

	void DrawWireframeStage::OnVisibleRenderItemsReset(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		// Temporary until we refactor buffer creation to use a pool
		constexpr uint32 MaximumVisibleRenderItemCount = 512;
		m_visibleStaticMeshes.ResetRenderItems(
			m_sceneView,
			*m_sceneView.GetSceneChecked(),
			m_sceneView.GetLogicalDevice(),
			m_stageIdentifier,
			renderItems,
			MaximumVisibleRenderItemCount,
			graphicsCommandEncoder,
			perFrameStagingBuffer
		);
	}

	void DrawWireframeStage::OnRenderItemsBecomeHidden(
		const Entity::RenderItemMask& renderItems,
		SceneBase& scene,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer
	)
	{
		m_visibleStaticMeshes
			.RemoveRenderItems(m_sceneView.GetLogicalDevice(), renderItems, scene, graphicsCommandEncoder, perFrameStagingBuffer);
	}

	void DrawWireframeStage::OnVisibleRenderItemTransformsChanged(
		const Entity::RenderItemMask& renderItems,
		const Rendering::CommandEncoderView graphicsCommandEncoder,
		PerFrameStagingBuffer& perFrameStagingBuffer

	)
	{
		UNUSED(renderItems);
		UNUSED(graphicsCommandEncoder);
		UNUSED(perFrameStagingBuffer);
	}

	Threading::JobBatch DrawWireframeStage::LoadRenderItemsResources(const Entity::RenderItemMask& renderItems)
	{
		const typename Entity::RenderItemIdentifier::IndexType maximumUsedRenderItemCount =
			m_sceneView.GetSceneChecked()->GetMaximumUsedRenderItemCount();
		Entity::SceneRegistry& sceneRegistry = m_sceneView.GetSceneChecked()->GetEntitySceneRegistry();
		for (const uint32 renderItemIndex : renderItems.GetSetBitsIterator(0, maximumUsedRenderItemCount))
		{
			const Optional<Entity::HierarchyComponentBase*> pVisibleComponent =
				m_sceneView.GetVisibleRenderItemComponent(Entity::RenderItemIdentifier::MakeFromValidIndex(renderItemIndex));
			Assert(pVisibleComponent.IsValid());
			if (UNLIKELY_ERROR(pVisibleComponent.IsInvalid()))
			{
				continue;
			}

			if (pVisibleComponent->IsStaticMesh(sceneRegistry))
			{
				return m_visibleStaticMeshes.TryLoadRenderItemResources(m_sceneView.GetLogicalDevice(), *pVisibleComponent, sceneRegistry);
			}
		}
		return {};
	}

	void DrawWireframeStage::OnSceneUnloaded()
	{
		m_visibleStaticMeshes.OnSceneUnloaded(m_sceneView.GetLogicalDevice(), *m_sceneView.GetSceneChecked());
	}

	void DrawWireframeStage::
		OnActiveCameraPropertiesChanged([[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer&)
	{
	}

	void DrawWireframeStage::OnBeforeRenderPassDestroyed()
	{
		m_pipeline.PrepareForResize(m_sceneView.GetLogicalDevice());
	}

	Threading::JobBatch DrawWireframeStage::AssignRenderPass(
		const RenderPassView renderPass, const Math::Rectangleui outputArea, const Math::Rectangleui fullRenderArea, const uint8 subpassIndex
	)
	{
		return m_pipeline.CreatePipeline(
			m_sceneView.GetLogicalDevice(),
			m_sceneView.GetLogicalDevice().GetShaderCache(),
			renderPass,
			outputArea,
			fullRenderArea,
			subpassIndex
		);
	}

	bool DrawWireframeStage::ShouldRecordCommands() const
	{
		return m_sceneView.HasActiveCamera() & m_visibleStaticMeshes.HasVisibleItems() && m_pipeline.IsValid();
	}

	void DrawWireframeStage::RecordRenderPassCommands(
		Rendering::RenderCommandEncoder& renderCommandEncoder,
		const Rendering::ViewMatrices& viewMatrices,
		[[maybe_unused]] const Math::Rectangleui renderArea,
		[[maybe_unused]] const uint8 subpassIndex
	)
	{
		renderCommandEncoder.BindPipeline(m_pipeline);

		const DescriptorSetView transformBufferDescriptorSet = m_sceneView.GetTransformBufferDescriptorSet();

		const VisibleRenderItems::VisibleInstanceGroups::ConstDynamicView instanceGroups = m_visibleStaticMeshes.GetVisibleItems();
		for (const Optional<VisibleRenderItems::InstanceGroup*> pInstanceGroup : instanceGroups)
		{
			if (pInstanceGroup == nullptr)
			{
				continue;
			}

			if (pInstanceGroup->m_instanceBuffer.GetInstanceCount() > 0)
			{
				const VisibleStaticMeshes::InstanceGroup& instanceGroup = static_cast<const VisibleStaticMeshes::InstanceGroup&>(*pInstanceGroup);
				m_pipeline.Draw(
					m_logicalDevice,
					instanceGroup.m_instanceBuffer.GetFirstInstanceIndex(),
					instanceGroup.m_instanceBuffer.GetInstanceCount(),
					viewMatrices.GetMatrix(ViewMatrices::Type::ViewProjection),
					instanceGroup.m_renderMeshView,
					instanceGroup.m_instanceBuffer.GetBuffer(),
					transformBufferDescriptorSet,
					renderCommandEncoder
				);
			}
		}
	}
}
