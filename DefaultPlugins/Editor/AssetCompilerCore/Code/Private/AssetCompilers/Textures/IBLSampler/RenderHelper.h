#pragma once

#include <Renderer/Instance.h>
#include <Renderer/Devices/PhysicalDevices.h>
#include <Renderer/Devices/LogicalDevice.h>
#include <Renderer/Commands/CommandPool.h>
#include <Renderer/Descriptors/DescriptorPool.h>
#include <Renderer/Descriptors/DescriptorSetView.h>
#include <Common/Memory/Containers/ArrayView.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/ByteView.h>
#include <Common/Memory/Optional.h>

namespace ngine::Rendering
{
	struct Image;
	struct CommandEncoderView;
	struct EncodedCommandBufferView;
}

namespace ngine::AssetCompiler
{
	struct RenderHelper
	{
	public:
		RenderHelper(const bool enableValidation);
		~RenderHelper();
	public:
		[[nodiscard]] bool IsValid() const
		{
			return m_instance.IsValid() && m_pPhysicalDevice.IsValid() && m_pLogicalDevice.IsValid();
		}

		// make sure there are no dependencies between command buffers. this method is blocking
		[[nodiscard]] bool executeCommandBuffers(const ngine::ArrayView<const ngine::Rendering::EncodedCommandBufferView> encodedCommandBuffers
		) const;

		ngine::Rendering::Image uploadImage(const ngine::ConstByteView inputData, const uint32_t width, const uint32_t height);

		ngine::Rendering::Instance m_instance;
		ngine::UniquePtr<ngine::Rendering::PhysicalDevices> m_pPhysicalDevices;
		ngine::Optional<ngine::Rendering::PhysicalDevice*> m_pPhysicalDevice;

		ngine::UniquePtr<ngine::Rendering::LogicalDevice> m_pLogicalDevice;
		ngine::Rendering::CommandPool m_commandPool;
		ngine::Rendering::DescriptorPool m_descriptorPool;

		bool m_debugOutputEnabled;
	};
}
