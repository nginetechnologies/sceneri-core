#pragma once

#include <Renderer/RenderOutput/RenderOutput.h>

#include <Renderer/Assets/Texture/RenderTexture.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Constants.h>
#include <Renderer/Framegraph/SubresourceStates.h>

#include <Common/Math/Vector2.h>
#include <Common/Math/Primitives/Rectangle.h>

namespace ngine::Rendering
{
	struct RenderTexture;
	struct SemaphoreView;
	struct Surface;

	struct RenderTargetOutput final : public RenderOutput
	{
		RenderTargetOutput(
			Rendering::LogicalDevice& logicalDevice,
			const Math::Vector2ui resolution,
			const Rendering::Format format,
			const Rendering::ImageLayout presentImageLayout = Rendering::ImageLayout::ColorAttachmentOptimal
		);
		RenderTargetOutput(RenderTargetOutput&&) = delete;
		RenderTargetOutput(const RenderTargetOutput&) = delete;
		RenderTargetOutput& operator=(RenderTargetOutput&&) = delete;
		RenderTargetOutput& operator=(const RenderTargetOutput&) = delete;

		void Destroy(LogicalDevice& logicalDevice);

		virtual Optional<FrameImageId> AcquireNextImage(
			const LogicalDeviceView logicalDevice,
			const uint64 imageAvailabilityTimeoutNanoseconds,
			const SemaphoreView imageAvailableSemaphore,
			const FenceView fence
		) override;
		virtual void PresentAcquiredImage(
			LogicalDevice& logicalDevice,
			const Rendering::FrameImageId imageIndex,
			const ArrayView<const Rendering::SemaphoreView, uint8> waitSemaphores,
			const Rendering::FenceView submissionFinishedFence,
			Rendering::PresentedCallback&& presentedCallback
		) override;

		[[nodiscard]] virtual Math::Rectangleui GetOutputArea() const override
		{
			return {Math::Zero, m_resolution};
		}
		[[nodiscard]] virtual Rendering::Format GetColorFormat() const override
		{
			return m_format;
		}
		[[nodiscard]] virtual Rendering::ImageLayout GetInitialColorImageLayout() const override
		{
			return ImageLayout::Undefined;
		}
		[[nodiscard]] virtual Rendering::ImageLayout GetPresentColorImageLayout() const override
		{
			return m_presentImageLayout;
		}
		[[nodiscard]] virtual ArrayView<const Rendering::ImageMappingView, Rendering::FrameIndex> GetColorImageMappings() const override
		{
			using ViewType = typename decltype(m_textureViews)::ConstView;
			return ViewType(m_textureViews.GetView());
		}
		virtual ImageMappingView GetCurrentColorImageMapping() const override final
		{
			return m_textureViews[m_currentImageIndex];
		}

		[[nodiscard]] virtual Rendering::FrameIndex GetImageCount() const override final
		{
			return Rendering::MaximumConcurrentFrameCount;
		}
		[[nodiscard]] virtual ImageView GetCurrentColorImageView() const override
		{
			return m_images[m_currentImageIndex];
		}
		[[nodiscard]] virtual SubresourceStatesBase& GetCurrentColorSubresourceStates() override final
		{
			return m_textures[m_currentImageIndex].GetSubresourceStates();
		}
		[[nodiscard]] Rendering::RenderTexture& GetTexture(const FrameIndex index)
		{
			return m_textures[index];
		}
		[[nodiscard]] const Rendering::RenderTexture& GetTexture(const FrameIndex index) const
		{
			return m_textures[index];
		}
	protected:
		Math::Vector2ui m_resolution;
		Rendering::Format m_format;
		const Rendering::ImageLayout m_presentImageLayout;

		Array<Rendering::RenderTexture, Rendering::MaximumConcurrentFrameCount> m_textures;
		Array<Rendering::ImageView, Rendering::MaximumConcurrentFrameCount> m_images;
		Array<Rendering::ImageMapping, Rendering::MaximumConcurrentFrameCount> m_textureViews;

		FrameIndex m_currentImageIndex = 0u;
	};
}
