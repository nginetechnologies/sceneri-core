#pragma once

#include <Renderer/FrameImageId.h>
#include <Renderer/Assets/Texture/RenderTargetCache.h>
#include <Renderer/Jobs/PresentedCallback.h>
#include <Renderer/Format.h>
#include <Renderer/ImageLayout.h>

#include <Common/EnumFlags.h>
#include <Common/Math/Primitives/ForwardDeclarations/Rectangle.h>
#include <Common/Memory/ForwardDeclarations/Optional.h>
#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct LogicalDeviceView;
	struct FrameImageId;
	struct FenceView;
	struct SemaphoreView;
	struct Window;
	struct ImageMappingView;
	struct ImageView;
	struct SubresourceStatesBase;

	struct RenderOutput
	{
		enum class Flags : uint8
		{
			SupportsAcquireImageSemaphore = 1 << 0,
			SupportsPresentImageSemaphore = 1 << 1,
			RequiresPresentFence = 1 << 2
		};

		RenderOutput(const EnumFlags<Flags> flags = {})
			: m_flags(flags)
		{
		}
		virtual ~RenderOutput() = default;

		[[nodiscard]] virtual Optional<FrameImageId> AcquireNextImage(
			const LogicalDeviceView logicalDevice,
			const uint64 imageAvailabilityTimeoutNanoseconds,
			const SemaphoreView imageAvailableSemaphore,
			const FenceView fence
		) = 0;
		virtual void PresentAcquiredImage(
			LogicalDevice& logicalDevice,
			const FrameImageId imageId,
			const ArrayView<const SemaphoreView, uint8> waitSemaphores,
			const FenceView submissionFinishedFence,
			PresentedCallback&& presentedCallback = {}
		) = 0;

		[[nodiscard]] virtual Math::Rectangleui GetOutputArea() const = 0;
		[[nodiscard]] virtual Format GetColorFormat() const = 0;
		[[nodiscard]] virtual ImageLayout GetInitialColorImageLayout() const = 0;
		[[nodiscard]] virtual ImageLayout GetPresentColorImageLayout() const = 0;
		[[nodiscard]] virtual ArrayView<const ImageMappingView, FrameIndex> GetColorImageMappings() const = 0;
		[[nodiscard]] virtual FrameIndex GetImageCount() const = 0;
		[[nodiscard]] virtual ImageMappingView GetCurrentColorImageMapping() const = 0;
		[[nodiscard]] virtual ImageView GetCurrentColorImageView() const = 0;
		[[nodiscard]] virtual SubresourceStatesBase& GetCurrentColorSubresourceStates() = 0;

		[[nodiscard]] inline bool SupportsAcquireImageSemaphore() const
		{
			return m_flags.IsSet(Flags::SupportsAcquireImageSemaphore);
		}
		[[nodiscard]] inline bool SupportsPresentImageSemaphore() const
		{
			return m_flags.IsSet(Flags::SupportsPresentImageSemaphore);
		}
		[[nodiscard]] inline bool RequiresPresentFence() const
		{
			return m_flags.IsSet(Flags::RequiresPresentFence);
		}

		[[nodiscard]] virtual Optional<Window*> GetWindow() const
		{
			return Invalid;
		}

		[[nodiscard]] RenderTargetCache& GetRenderTargetCache()
		{
			return m_renderTargetCache;
		}
		[[nodiscard]] const RenderTargetCache& GetRenderTargetCache() const
		{
			return m_renderTargetCache;
		}
	private:
		EnumFlags<Flags> m_flags;
		RenderTargetCache m_renderTargetCache;
	};

	ENUM_FLAG_OPERATORS(RenderOutput::Flags);
}
