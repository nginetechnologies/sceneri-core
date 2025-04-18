#pragma once

#include <Renderer/Stages/RenderItemStage.h>
#include <Renderer/Commands/CommandEncoderView.h>
#include <Renderer/Buffers/StorageBuffer.h>
#include <Renderer/Assets/Material/MaterialIdentifier.h>

#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Memory/Optional.h>
#include <Common/Memory/Containers/ByteView.h>

namespace ngine::Rendering
{
	struct SceneView;
	struct MaterialStage;

	struct MaterialsStage final : public RenderItemStage
	{
		inline static constexpr Guid TypeGuid = "{BBA6AD40-E1EE-4F1E-BAF8-608FF9E7D77F}"_guid;

		MaterialsStage(SceneView& sceneView);
		virtual ~MaterialsStage();

		void RegisterStage(const Rendering::MaterialIdentifier identifier, MaterialStage& stage);

		void SetForwardingStage(const Optional<RenderItemStage*> pForwardingStage)
		{
			m_pForwardingStage = pForwardingStage;
		}

		[[nodiscard]] const StorageBuffer& GetRenderItemsDataBuffer() const
		{
			return m_renderItemsDataBuffer;
		}
	protected:
		// RenderItemStage
		[[nodiscard]] virtual bool ShouldRecordCommands() const override
		{
			return false;
		}
		virtual void RecordCommands(const CommandEncoderView) override
		{
		}

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Materials Stage";
		}
#endif

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
		[[nodiscard]] virtual Threading::JobBatch LoadRenderItemsResources(const Entity::RenderItemMask& renderItems) override;
		virtual void OnVisibleRenderItemTransformsChanged(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
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
	protected:
		SceneView& m_sceneView;
		Optional<RenderItemStage*> m_pForwardingStage;

		TIdentifierArray<Optional<MaterialStage*>, MaterialIdentifier> m_materialStages{Memory::Zeroed};
		IdentifierMask<MaterialIdentifier> m_activeMaterials;
		TIdentifierArray<Rendering::MaterialIdentifier, Entity::RenderItemIdentifier> m_visibleRenderItemMaterialIdentifiers;

		struct RenderItemData
		{
			uint32 meshIndex;
			int32 diffuseTextureIndex;
			int32 roughnessTextureIndex{-1};
			// uint32 materialIndex;
			int32 padding;
		};
		StorageBuffer m_renderItemsDataBuffer;
	};
}
