#pragma once

#include <Renderer/Window/SurfaceView.h>
#include <Renderer/Window/WindowHandle.h>

#include <Common/Math/ForwardDeclarations/Vector2.h>

namespace ngine::Rendering
{
	struct InstanceView;
	struct PhysicalDevice;
	struct LogicalDeviceView;

	struct Surface : public SurfaceView
	{
		Surface() = default;
		Surface(const InstanceView instance, const LogicalDeviceView logicalDevice, const SurfaceWindowHandle windowHandle);
		Surface(const SurfaceView surface)
			: SurfaceView(surface)
		{
		}
		Surface(const Surface&) = delete;
		Surface& operator=(const Surface&) = delete;
		Surface(Surface&& other) noexcept
			: SurfaceView(other)
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			other.m_pSurface = nullptr;
#endif
		}
		Surface& operator=(Surface&& other) noexcept;
		~Surface();
		void Destroy(const InstanceView instance);

#if RENDERER_VULKAN && PLATFORM_DESKTOP && !PLATFORM_APPLE
		[[nodiscard]] static Surface
		CreateDirectToDisplay(const InstanceView instance, const PhysicalDevice& physicalDevice, const Math::Vector2ui resolution);
#endif
	};
}
