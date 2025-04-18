#pragma once

#include <Common/Math/Color.h>

#include <Renderer/Constants.h>
#include <Renderer/Stages/VisibleStaticMeshes.h>
#include <Renderer/Stages/RenderItemStage.h>

#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Assets/Material/MaterialIdentifier.h>
#include <Renderer/Assets/Material/MaterialInstanceIdentifier.h>
#include <Renderer/Assets/Material/RenderMaterial.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Commands/ClearValue.h>

#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/Variant.h>
#include <Common/Asset/Guid.h>

#if PROFILE_BUILD
#include <Common/Memory/Containers/String.h>
#endif

#include <Common/Math/Matrix4x4.h>

namespace ngine::Rendering
{
	struct SceneView;
	struct RenderMaterialInstance;
	struct MaterialsStage;
	struct RenderCommandEncoderView;

	struct MaterialStage final : public RenderItemStage, public VisibleStaticMeshes
	{
		MaterialStage(
			const MaterialIdentifier materialIdentifier,
			SceneView& sceneView,
			const float renderAreaFactor,
			const EnumFlags<Flags> flags = Flags::Enabled
		);
		virtual ~MaterialStage();

		[[nodiscard]] RenderMaterial& GetMaterial()
		{
			return m_material;
		}
	protected:
		friend MaterialsStage;

		// Stage
		virtual void OnBeforeRenderPassDestroyed() override;
		[[nodiscard]] virtual Threading::JobBatch AssignRenderPass(
			const RenderPassView,
			[[maybe_unused]] const Math::Rectangleui outputArea,
			[[maybe_unused]] const Math::Rectangleui fullRenderArea,
			[[maybe_unused]] const uint8 subpassIndex
		) override;

		virtual void RecordRenderPassCommands(
			RenderCommandEncoder&, const ViewMatrices&, const Math::Rectangleui renderArea, const uint8 subpassIndex
		) override;

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return m_debugMarkerName;
		}
#endif

		[[nodiscard]] virtual uint32 GetMaximumPushConstantInstanceCount() const override
		{
			return VisibleRenderItems::GetInstanceCount();
		}
		// ~Stage

		// RenderItemStage
		[[nodiscard]] virtual bool ShouldRecordCommands() const override;
		[[nodiscard]] virtual EnumFlags<PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return PipelineStageFlags::ColorAttachmentOutput | PipelineStageFlags::LateFragmentTests;
		} // TODO: Determine based on material

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
			[[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnDisabled(Rendering::CommandEncoderView, PerFrameStagingBuffer&) override
		{
		}
		virtual void OnEnable(Rendering::CommandEncoderView, PerFrameStagingBuffer&) override
		{
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

		[[nodiscard]] bool
		MoveMaterialInstancesFrom(MaterialStage& otherStage, const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier);

		struct InstanceGroup final : public VisibleStaticMeshes::InstanceGroup
		{
			using BaseType = VisibleStaticMeshes::InstanceGroup;

			InstanceGroup(
				LogicalDevice& logicalDevice,
				const MaterialInstanceIdentifier materialInstanceIdentifier,
				const RenderMaterialInstance& materialInstance,
				const uint32 maximumInstanceCount
			)
				: BaseType(logicalDevice, maximumInstanceCount)
				, m_materialInstanceIdentifier(materialInstanceIdentifier)
				, m_materialInstance(materialInstance)
			{
			}
			InstanceGroup(InstanceGroup&& other)
				: BaseType(static_cast<BaseType&&>(other))
				, m_materialInstanceIdentifier(other.m_materialInstanceIdentifier)
				, m_materialInstance(other.m_materialInstance)
			{
			}
			using BaseType::BaseType;
			virtual ~InstanceGroup() = default;

			virtual SupportResult SupportsComponent(
				const LogicalDevice& logicalDevice, const Entity::HierarchyComponentBase&, Entity::SceneRegistry& sceneRegistry
			) const override;

			MaterialInstanceIdentifier m_materialInstanceIdentifier;
			ReferenceWrapper<const RenderMaterialInstance> m_materialInstance;
		};

		void OnRenderItemsBecomeVisibleFromMaterialsStage(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		);
		void OnVisibleRenderItemsResetFromMaterialsStage(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		);
	protected:
		friend struct MaterialsStage;

		SceneView& m_sceneView;
		Entity::RenderItemMask m_visibleRenderItems;

		RenderMaterial& m_material;

		const float m_renderAreaFactor;

#if STAGE_DEPENDENCY_PROFILING
		String m_debugMarkerName{"Material Stage"};
#endif
	};
}
