#pragma once

#include <Common/Asset/Guid.h>
#include <Common/Threading/AtomicInteger.h>
#include <Common/Math/Matrix4x4.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Storage/IdentifierMask.h>

#include <Renderer/Stages/RenderItemStage.h>
#include <Renderer/Stages/VisibleStaticMeshes.h>

#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Wrappers/ImageView.h>
#include <Renderer/Buffers/StorageBuffer.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Assets/Texture/LoadedTextureFlags.h>
#include <Renderer/Devices/PhysicalDeviceFeatures.h>

#include <DeferredShading/Pipelines/MaterialIdentifiersPipeline.h>

namespace ngine::Entity
{
	struct LightSourceComponent;
	struct SpotLightComponent;
}

namespace ngine::Threading
{
	struct EngineJobRunnerThread;
}

namespace ngine::Rendering
{
	struct SceneView;
	struct LogicalDevice;
	struct RenderTexture;

	struct MaterialIdentifiersStage final : public Rendering::RenderItemStage, protected Rendering::VisibleStaticMeshes
	{
		inline static constexpr Asset::Guid Guid = "E5A82F98-CAB4-42DD-9A93-B1BA37B04A6C"_asset;
		inline static constexpr Asset::Guid RenderTargetGuid = "5E7D7F11-6915-4EAA-90C1-2B74C8AAA049"_asset;

		MaterialIdentifiersStage(SceneView& sceneView);
		virtual ~MaterialIdentifiersStage();
	protected:
#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Material Identifiers Stage";
		}
#endif

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
		virtual void OnVisibleRenderItemTransformsChanged(
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
		[[nodiscard]] virtual Threading::JobBatch LoadRenderItemsResources(const Entity::RenderItemMask& renderItems) override;
		virtual void OnSceneUnloaded() override;
		virtual void OnActiveCameraPropertiesChanged(
			const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
		) override;

		[[nodiscard]] virtual bool ShouldRecordCommands() const override;
		virtual void RecordRenderPassCommands(
			RenderCommandEncoder&, const ViewMatrices&, const Math::Rectangleui renderArea, const uint8 subpassIndex
		) override;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
#if MATERIAL_IDENTIFIERS_DEPTH_ONLY
			return PipelineStageFlags::LateFragmentTests;
#else
			return PipelineStageFlags::ColorAttachmentOutput | PipelineStageFlags::LateFragmentTests;
#endif
		}

		virtual void OnDisabled([[maybe_unused]] Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer&) override
		{
		}
		virtual void OnEnable([[maybe_unused]] Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer&) override
		{
		}

		[[nodiscard]] virtual uint32 GetMaximumPushConstantInstanceCount() const override
		{
			return 1;
		}
		// ~RenderItemStage

		// VisibleStaticMeshes
		virtual UniquePtr<VisibleRenderItems::InstanceGroup> CreateInstanceGroup(
			LogicalDevice& logicalDevice,
			const Entity::HierarchyComponentBase& renderItem,
			Entity::SceneRegistry& sceneRegistry,
			const uint32 maximumInstanceCount
		) override;
		// ~VisibleStaticMeshes

		struct InstanceGroup final : public VisibleStaticMeshes::InstanceGroup
		{
			using BaseType = VisibleStaticMeshes::InstanceGroup;
			using BaseType::BaseType;

			virtual ~InstanceGroup() = default;
			virtual SupportResult SupportsComponent(
				const LogicalDevice& logicalDevice, const Entity::HierarchyComponentBase&, Entity::SceneRegistry& sceneRegistry
			) const override;

			bool m_isTwoSided{true};
		};
	protected:
		SceneView& m_sceneView;
		MaterialIdentifiersPipeline m_pipeline;
	};
}
