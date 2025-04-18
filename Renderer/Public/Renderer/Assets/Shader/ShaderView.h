#pragma once

#include <Common/Platform/ForceInline.h>
#include <Common/Platform/TrivialABI.h>

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/Metal/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

namespace ngine::Rendering
{
	struct TRIVIAL_ABI ShaderView
	{
		ShaderView() = default;

#if RENDERER_VULKAN
		ShaderView(const VkShaderModule pModule)
			: m_pShaderModule(pModule)
		{
		}
		[[nodiscard]] operator VkShaderModule() const
		{
			return m_pShaderModule;
		}
#elif RENDERER_METAL
		ShaderView(id<MTLLibrary> library, id<MTLFunction> function)
			: m_pShaderLibrary(library)
			, m_pShaderFunction(function)
		{
		}
		[[nodiscard]] operator id<MTLLibrary>() const
		{
			return m_pShaderLibrary;
		}
		[[nodiscard]] operator id<MTLFunction>() const
		{
			return m_pShaderFunction;
		}
#elif RENDERER_WEBGPU
		ShaderView(const WGPUShaderModule pModule)
			: m_pShaderModule(pModule)
		{
		}
		[[nodiscard]] operator WGPUShaderModule() const
		{
			return m_pShaderModule;
		}
#endif

		[[nodiscard]] bool IsValid() const
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU
			return m_pShaderModule != 0;
#elif RENDERER_METAL
			return m_pShaderFunction != nullptr;
#else
			return false;
#endif
		}
	protected:
#if RENDERER_VULKAN
		VkShaderModule m_pShaderModule = 0;
#elif RENDERER_METAL
		// TODO: Introduce a library abstraction, and move this there.
		id<MTLLibrary> m_pShaderLibrary{nullptr};
		id<MTLFunction> m_pShaderFunction{nullptr};
#elif RENDERER_WEBGPU
		WGPUShaderModule m_pShaderModule = nullptr;
#endif
	};
}
