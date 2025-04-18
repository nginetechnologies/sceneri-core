#pragma once

#include "ShaderView.h"
#include <Common/Memory/Containers/ForwardDeclarations/ByteView.h>

namespace ngine::IO
{
	struct FileView;
}

namespace ngine::Rendering
{
	struct PhysicalDeviceView;
	struct LogicalDeviceView;
	struct LogicalDevice;

	struct Shader : public ShaderView
	{
		Shader() = default;
		Shader(LogicalDevice& logicalDevice, const ConstByteView data);
		Shader(const ShaderView other)
			: ShaderView(other)
		{
		}
		Shader(Shader&& other)
			: ShaderView(other)
		{
#if RENDERER_VULKAN || RENDERER_WEBGPU
			other.m_pShaderModule = 0;
#elif RENDERER_METAL
			other.m_pShaderLibrary = nullptr;
			other.m_pShaderFunction = nullptr;
#endif
		}
		Shader& operator=(Shader&& other);
		Shader(const Shader&) = delete;
		Shader& operator=(const Shader&) = delete;
		~Shader();

		void Destroy(const LogicalDeviceView logicalDevice);
	};
}
