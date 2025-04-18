#pragma once

#include <Common/Platform/LifetimeBound.h>
#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

namespace ngine::Rendering
{
	struct Buffer;
	struct BufferBase;
	struct BufferVulkan;
	struct BufferMetal;
	struct BufferWebGPU;
	struct LogicalDeviceView;
	struct LogicalDevice;

	struct TRIVIAL_ABI BufferView
	{
		BufferView() = default;
		BufferView(BufferView&&) = default;
		BufferView(const BufferView&) = default;
		BufferView& operator=(BufferView&&) = default;
		BufferView& operator=(const BufferView&) = default;

#if RENDERER_VULKAN
		BufferView(const VkBuffer pBuffer)
			: m_pBuffer(pBuffer)
		{
		}

		[[nodiscard]] operator VkBuffer() const LIFETIME_BOUND
		{
			return m_pBuffer;
		}
#elif RENDERER_METAL
		BufferView(const id<MTLBuffer> pBuffer)
			: m_pBuffer(pBuffer)
		{
		}

		[[nodiscard]] operator id<MTLBuffer>() const LIFETIME_BOUND
		{
			return m_pBuffer;
		}
#elif WEBGPU_INDIRECT_HANDLES
		BufferView(WGPUBuffer* buffer)
			: m_pBuffer(buffer)
		{
		}

		[[nodiscard]] operator WGPUBuffer() const LIFETIME_BOUND
		{
			return m_pBuffer != nullptr ? *m_pBuffer : nullptr;
		}
#elif RENDERER_WEBGPU
		BufferView(const WGPUBuffer buffer)
			: m_pBuffer(buffer)
		{
		}

		[[nodiscard]] operator WGPUBuffer() const LIFETIME_BOUND
		{
			return m_pBuffer;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pBuffer != 0;
#else
			return false;
#endif
		}

		[[nodiscard]] uint64 GetDeviceAddress(const LogicalDevice& logicalDevice) const;
	protected:
		friend Buffer; // TODO: use accessors
		friend BufferBase;

#if RENDERER_VULKAN
		friend BufferVulkan;
		VkBuffer m_pBuffer = 0;
#elif RENDERER_METAL
		friend BufferMetal;
		id<MTLBuffer> m_pBuffer{nullptr};
#elif RENDERER_WEBGPU
		friend BufferWebGPU;
#if WEBGPU_INDIRECT_HANDLES
		WGPUBuffer* m_pBuffer = 0;
#else
		WGPUBuffer m_pBuffer = 0;
#endif
#endif
	};
}
