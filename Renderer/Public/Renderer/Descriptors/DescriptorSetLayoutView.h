#pragma once

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>
#include <Common/Memory/Containers/ForwardDeclarations/ZeroTerminatedStringView.h>

namespace ngine::Rendering
{
	struct LogicalDevice;

#if RENDERER_METAL || RENDERER_WEBGPU
	namespace Internal
	{
		struct DescriptorSetLayoutData;
	}
#endif

	struct TRIVIAL_ABI DescriptorSetLayoutView
	{

#if RENDERER_VULKAN
		[[nodiscard]] operator VkDescriptorSetLayout() const
		{
			return m_pLayout;
		}
#elif RENDERER_METAL || RENDERER_WEBGPU
		[[nodiscard]] operator Internal::DescriptorSetLayoutData*() const
		{
			return m_pLayout;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_METAL || RENDERER_WEBGPU
			return m_pLayout != 0;
#else
			return false;
#endif
		}

		void SetDebugName(const LogicalDevice& logicalDevice, const ConstZeroTerminatedStringView name);
	protected:
#if RENDERER_VULKAN
		VkDescriptorSetLayout m_pLayout = 0;
#elif RENDERER_METAL || RENDERER_WEBGPU
		Internal::DescriptorSetLayoutData* m_pLayout{nullptr};
#endif
	};
}
