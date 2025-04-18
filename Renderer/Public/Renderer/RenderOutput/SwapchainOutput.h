#pragma once

#include <Renderer/RenderOutput/SwapchainView.h>
#include <Renderer/RenderOutput/RenderOutput.h>
#include <Renderer/Wrappers/ImageMapping.h>
#include <Renderer/Wrappers/Image.h>
#include <Renderer/UsageFlags.h>
#include <Renderer/Constants.h>
#include <Renderer/Framegraph/SubresourceStates.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Math/Vector2.h>
#include <Common/Math/Primitives/Rectangle.h>

#if PLATFORM_EMSCRIPTEN
#include <Common/Threading/Mutexes/ConditionVariable.h>
#else
#include <Common/Threading/Mutexes/Mutex.h>
#endif

namespace ngine::Rendering
{
	struct RenderTexture;
	struct SemaphoreView;
	struct Surface;
	struct SurfaceView;
	struct Window;

	struct SwapchainOutput : public RenderOutput
	{
		SwapchainOutput(
			const Math::Vector2ui resolution,
			const ArrayView<const Format, uint16> supportedFormats,
			const LogicalDevice& logicalDevice,
			const SurfaceView surface,
			const EnumFlags<UsageFlags> usageFlags = UsageFlags::ColorAttachment,
			Optional<Window*> pWindow = Invalid
		);
		SwapchainOutput(SwapchainOutput&&) = delete;
		SwapchainOutput(const SwapchainOutput&) = delete;
		SwapchainOutput& operator=(SwapchainOutput&&) = delete;
		SwapchainOutput& operator=(const SwapchainOutput&) = delete;
		virtual ~SwapchainOutput();

		void Destroy(LogicalDevice& logicalDevice);

		[[nodiscard]] virtual Optional<FrameImageId> AcquireNextImage(
			const LogicalDeviceView logicalDevice,
			const uint64 imageAvailabilityTimeoutNanoseconds,
			const SemaphoreView imageAvailableSemaphore,
			const FenceView fence
		) override;
		virtual void PresentAcquiredImage(
			LogicalDevice& logicalDevice,
			const FrameImageId imageIndex,
			const ArrayView<const SemaphoreView, uint8> waitSemaphores,
			const FenceView submissionFinishedFence,
			PresentedCallback&& presentedCallback
		) override final;

		bool CreateSwapchain(
			const LogicalDevice& logicalDevice, const SurfaceView surface, const EnumFlags<UsageFlags> usageFlags = UsageFlags::ColorAttachment
		);
		bool RecreateSwapchain(
			LogicalDevice& logicalDevice,
			const SurfaceView surface,
			const Math::Vector2ui newResolution,
			const EnumFlags<UsageFlags> usageFlags = UsageFlags::ColorAttachment
		);

		[[nodiscard]] virtual Math::Rectangleui GetOutputArea() const override final
		{
			return {Math::Zero, m_resolution};
		}
		[[nodiscard]] virtual Format GetColorFormat() const override final
		{
			return m_format;
		}
		[[nodiscard]] virtual ImageLayout GetInitialColorImageLayout() const override final
		{
			return ImageLayout::Undefined;
		}
		[[nodiscard]] virtual ImageLayout GetPresentColorImageLayout() const override final
		{
			return ImageLayout::PresentSource;
		}
		[[nodiscard]] virtual ArrayView<const ImageMappingView, FrameIndex> GetColorImageMappings() const override final
		{
			return m_swapchainImageViews.GetView();
		}

		virtual ImageMappingView GetCurrentColorImageMapping() const override final
		{
			return m_swapchainImageViews[m_frameIndex];
		}

		[[nodiscard]] virtual FrameIndex GetImageCount() const override final
		{
			return m_swapchainImageViews.GetSize();
		}

		[[nodiscard]] virtual ImageView GetCurrentColorImageView() const override final
		{
			return m_swapchainImages[m_frameIndex];
		}

		[[nodiscard]] virtual SubresourceStatesBase& GetCurrentColorSubresourceStates() override final
		{
			return m_subresourceStates[m_frameIndex];
		}

		[[nodiscard]] bool IsValid() const
		{
			return m_swapchain.IsValid();
		}

		[[nodiscard]] operator SwapchainView() const
		{
			return m_swapchain;
		}

		[[nodiscard]] virtual Optional<Window*> GetWindow() const override
		{
			return m_pWindow;
		}
	protected:
#if PLATFORM_EMSCRIPTEN
		int ProcessAnimationFrame(const FrameIndex frameIndex, const double time);
#endif
	protected:
		Math::Vector2ui m_resolution;
		Vector<Format, uint16> m_supportedFormats;
		Format m_format;

		SwapchainView m_swapchain;
		FrameIndex m_frameIndex{0};

		enum class FrameState : uint8
		{
			Inactive,
			AwaitingAnimationFrame,
			ProcessingAnimationFrame,
			FinishedCpuSubmits
		};
		InlineVector<FrameState, MaximumConcurrentFrameCount> m_frameStates;
		Threading::Mutex m_frameStateMutex;

#if RENDERER_METAL
		InlineVector<Image, MaximumConcurrentFrameCount, FrameIndex> m_swapchainImages;
#else
		InlineVector<ImageView, MaximumConcurrentFrameCount, FrameIndex> m_swapchainImages;
#endif
		InlineVector<SubresourceStatesBase, MaximumConcurrentFrameCount, FrameIndex> m_subresourceStates;
		InlineVector<ImageMapping, MaximumConcurrentFrameCount, FrameIndex> m_swapchainImageViews;

		Window* m_pWindow;

#if PLATFORM_EMSCRIPTEN
		Threading::ConditionVariable m_frameStateConditionVariable;
#endif
	};
}
