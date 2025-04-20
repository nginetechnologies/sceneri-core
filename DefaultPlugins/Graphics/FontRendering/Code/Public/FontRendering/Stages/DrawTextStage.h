#pragma once

#include <FontRendering/Stages/DrawTextPipeline.h>
#include <FontRendering/FontIdentifier.h>
#include <FontRendering/FontInstanceIdentifier.h>
#include <FontRendering/Point.h>

#include <Renderer/Stages/RenderItemStage.h>
#include <Renderer/Wrappers/RenderPass.h>
#include <Renderer/Wrappers/Framebuffer.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Wrappers/ImageView.h>
#include <Renderer/Commands/CommandBuffer.h>
#include <Renderer/Descriptors/DescriptorSet.h>
#include <Renderer/Wrappers/Sampler.h>
#include <Renderer/Assets/Texture/TextureIdentifier.h>
#include <Renderer/Devices/LogicalDeviceIdentifier.h>

#include <Common/Function/Function.h>
#include <Common/Storage/Identifier.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Math/Color.h>
#include <Common/Asset/Guid.h>
#include <Engine/Input/ScreenCoordinate.h>

namespace ngine::Rendering
{
	struct SceneView;
	struct TextureCache;
	struct RenderTexture;
}

namespace ngine::Font
{
	struct InstanceProperties;

	struct DrawTextStage final : public Rendering::RenderItemStage
	{
		inline static constexpr Asset::Guid TypeGuid = "{7F4AB7CE-FE72-4B86-BE23-C66836A0B355}"_asset;

		DrawTextStage(Rendering::SceneView& sceneView);
		virtual ~DrawTextStage();
	protected:
		// RenderItemStage
		virtual void OnBeforeRecordCommands(const Rendering::CommandEncoderView) override;
		virtual void RecordCommands(const Rendering::CommandEncoderView commandEncoder) override;
		virtual void OnAfterRecordCommands(const Rendering::CommandEncoderView) override;
		virtual bool ShouldRecordCommands() const override final;

#if STAGE_DEPENDENCY_PROFILING
		[[nodiscard]] virtual ConstZeroTerminatedStringView GetDebugName() const override
		{
			return "Draw Text Stage";
		}
#endif

		[[nodiscard]] virtual EnumFlags<Rendering::PipelineStageFlags> GetPipelineStageFlags() const override
		{
			return Rendering::PipelineStageFlags::ColorAttachmentOutput;
		}
		// ~Stage

		// RenderItemStage
		virtual void OnRenderItemsBecomeVisible(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			Rendering::PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnVisibleRenderItemsReset(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			Rendering::PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnRenderItemsBecomeHidden(
			const Entity::RenderItemMask& renderItems,
			SceneBase& scene,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			Rendering::PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnVisibleRenderItemTransformsChanged(
			const Entity::RenderItemMask& renderItems,
			const Rendering::CommandEncoderView graphicsCommandEncoder,
			Rendering::PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		[[nodiscard]] virtual Threading::JobBatch LoadRenderItemsResources(const Entity::RenderItemMask& renderItems) override;
		virtual void OnSceneUnloaded() override;
		virtual void OnActiveCameraPropertiesChanged(
			[[maybe_unused]] const Rendering::CommandEncoderView graphicsCommandEncoder, Rendering::PerFrameStagingBuffer& perFrameStagingBuffer
		) override;
		virtual void OnDisabled(Rendering::CommandEncoderView, Rendering::PerFrameStagingBuffer&) override
		{
		}
		virtual void OnEnable(Rendering::CommandEncoderView, Rendering::PerFrameStagingBuffer&) override
		{
		}
		// ~RenderItemStage

		struct RenderTargetInfo
		{
			RenderTargetInfo(const Math::Vector2ui size)
			{
				m_availableSpace.EmplaceBack(Math::Rectangleui{Math::Zero, size});
			}

			struct TextInfo
			{
				UnicodeString m_text;
				Math::Color m_color;
				Math::Rectangleui m_allocatedArea;
			};

			Rendering::TextureIdentifier m_renderTargetIdentifier;
			Rendering::ImageMapping m_renderTargetMapping;
			Optional<Rendering::RenderTexture*> m_pRenderTargetTexture;
			Threading::Atomic<uint8> m_loadedRenderTargetCount = 0;
			Rendering::Framebuffer m_framebuffer;

			// TODO; Clean up
			UnorderedMap<typename Entity::RenderItemIdentifier::IndexType, TextInfo> m_registeredTexts;

			InlineVector<Math::Rectangleui, 16> m_availableSpace;

			[[nodiscard]] Optional<Math::Rectangleui> TryEmplaceText(
				const Entity::RenderItemIdentifier renderItemIdentifier,
				const ConstUnicodeStringView text,
				const Math::Color color,
				const uint32 textWidth,
				const uint32 atlasRowHeight
			);
			void ReturnTextArea(const Math::Rectangleui textArea);

			[[nodiscard]] bool IsValid() const
			{
				return m_framebuffer.IsValid();
			}
		};

		struct AtlasInfo
		{
			Optional<const Atlas*> m_pFontAtlas;
			Rendering::ImageMapping m_fontImageMapping;
			Rendering::DescriptorSet m_fontDescriptorSet;
			Threading::JobRunnerThread* m_pDescriptorLoadingThread = nullptr;
			Rendering::Sampler m_fontSampler;

			InlineVector<UniquePtr<RenderTargetInfo>, 2> m_renderTargets;

			[[nodiscard]] bool CanRegisterRenderItems() const;
			[[nodiscard]] bool
			CanRender(Rendering::TextureCache& textureCache, const Rendering::LogicalDeviceIdentifier logicalDeviceIdentifier) const;
		};

		[[nodiscard]] Threading::JobBatch
		LoadAtlasResources(const InstanceIdentifier atlasInstanceIdentifier, const InstanceProperties properties, AtlasInfo& atlas);
		[[nodiscard]] Optional<Threading::Job*> LoadRenderTarget(RenderTargetInfo& renderTargetInfo);
	protected:
		Rendering::SceneView& m_sceneView;

		Rendering::RenderPass m_renderPass;
		DrawTextPipeline m_pipeline;

		TIdentifierArray<UniquePtr<AtlasInfo>, InstanceIdentifier> m_atlases;

		struct RenderItemInfo
		{
			Optional<RenderTargetInfo*> m_pRenderTarget;
			Math::Rectangleui m_textArea;
			uint32 m_realHeight;
		};
		TIdentifierArray<RenderItemInfo, Entity::RenderItemIdentifier> m_renderItemInfo;
	};
}
