#include "Assets/Shader/Shader.h"

#include <Common/Assert/Assert.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/IO/FileView.h>

#include <Renderer/Devices/PhysicalDeviceView.h>
#include <Renderer/Devices/LogicalDeviceView.h>
#include <Renderer/Devices/LogicalDevice.h>

#include <Renderer/Vulkan/Includes.h>
#include <Renderer/Metal/Includes.h>
#include <Renderer/WebGPU/Includes.h>
#include <Renderer/Window/Window.h>

#include <Common/Threading/Atomics/Exchange.h>
#include <Common/Threading/Atomics/Load.h>
#include <Common/Memory/Containers/ByteView.h>

namespace ngine::Rendering
{
	Shader::Shader(LogicalDevice& logicalDevice, const ConstByteView data)
	{
#if RENDERER_VULKAN
		const VkShaderModuleCreateInfo createInfo = {
			VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
			nullptr,
			0,
			static_cast<size_t>(data.GetDataSize()),
			reinterpret_cast<const uint32*>(data.GetData())
		};

		vkCreateShaderModule(logicalDevice, &createInfo, nullptr, &m_pShaderModule);
#elif RENDERER_METAL
		id<MTLDevice> device = logicalDevice;
		dispatch_data_t dispatchData = dispatch_data_create(data.GetData(), data.GetDataSize(), dispatch_get_main_queue(), nil);

		NSError* error{nil};
		m_pShaderLibrary = [device newLibraryWithData:dispatchData error:&error];
		Assert(error == nil);
		if (LIKELY(error == nil))
		{
			// TODO: Refactor shader setup to support libraries so we don't need to hardcode this.
			m_pShaderFunction = [m_pShaderLibrary newFunctionWithName:@"main0"];
		}
#elif RENDERER_WEBGPU
#if RENDERER_WEBGPU_DAWN
		WGPUShaderSourceWGSL wgslDescriptor{};
		wgslDescriptor.code = WGPUStringView { reinterpret_cast<const char*>(data.GetData()), data[data.GetDataSize() - 1] == '\0' ? data.GetDataSize() - 1 : data.GetDataSize() };
		wgslDescriptor.chain.sType = WGPUSType_ShaderSourceWGSL;
#else
		WGPUShaderModuleWGSLDescriptor wgslDescriptor{};
		wgslDescriptor.chain.sType = WGPUSType_ShaderModuleWGSLDescriptor;
		wgslDescriptor.code = reinterpret_cast<const char*>(data.GetData());
#endif
		WGPUShaderModuleDescriptor shaderDescriptor = {};
		shaderDescriptor.nextInChain = reinterpret_cast<WGPUChainedStruct*>(&wgslDescriptor);

		const LogicalDeviceView logicalDeviceView = logicalDevice;
		WGPUShaderModule pShaderModule;
		Rendering::Window::ExecuteImmediatelyOnWindowThread(
			[logicalDevice = logicalDeviceView, &pShaderModule, shaderDescriptor]()
			{
				pShaderModule = wgpuDeviceCreateShaderModule(logicalDevice, &shaderDescriptor);
#if RENDERER_WEBGPU_DAWN
				wgpuShaderModuleAddRef(pShaderModule);
#else
				wgpuShaderModuleReference(pShaderModule);
#endif
			}
		);
		m_pShaderModule = pShaderModule;
#else
		Assert(false, "TODO");
#endif
	}

	Shader& Shader::operator=([[maybe_unused]] Shader&& other)
	{
		Assert(!IsValid(), "Destroy must have been called!");
#if RENDERER_VULKAN || RENDERER_WEBGPU
		m_pShaderModule = other.m_pShaderModule;
		other.m_pShaderModule = 0;
#elif RENDERER_METAL
		m_pShaderLibrary = other.m_pShaderLibrary;
		other.m_pShaderLibrary = nullptr;
		m_pShaderFunction = other.m_pShaderFunction;
		other.m_pShaderFunction = nullptr;
#endif
		return *this;
	}

	Shader::~Shader()
	{
		Assert(!IsValid(), "Destroy must have been called!");
	}

	void Shader::Destroy([[maybe_unused]] const LogicalDeviceView logicalDevice)
	{
#if RENDERER_VULKAN
		vkDestroyShaderModule(logicalDevice, m_pShaderModule, nullptr);
		m_pShaderModule = 0;
#elif RENDERER_METAL
		m_pShaderLibrary = nullptr;
		m_pShaderFunction = nullptr;
#elif RENDERER_WEBGPU
		if (m_pShaderModule != nullptr)
		{
			Rendering::Window::QueueOnWindowThread(
				[pShaderModule = m_pShaderModule]()
				{
					wgpuShaderModuleRelease(pShaderModule);
				}
			);
		}
#else
		Assert(false, "TODO");
#endif
	}
}
