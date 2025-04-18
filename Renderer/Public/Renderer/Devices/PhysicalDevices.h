#pragma once

#include "PhysicalDevice.h"

#include <Renderer/Vulkan/ForwardDeclares.h>
#include <Renderer/WebGPU/ForwardDeclares.h>

namespace ngine::Rendering
{
	struct InstanceView;

	struct PhysicalDevices
	{
		PhysicalDevices(const InstanceView instance);

		using Devices = FlatVector<PhysicalDevice, PhysicalDevice::MaximumCount>;
		using DeviceView = Devices::View;
		using ConstDeviceView = Devices::ConstView;

		[[nodiscard]] DeviceView GetView()
		{
			return {m_devices.begin(), m_devices.end()};
		}
		[[nodiscard]] ConstDeviceView GetView() const
		{
			return {m_devices.begin(), m_devices.end()};
		}

		[[nodiscard]] Devices::IteratorType begin()
		{
			return m_devices.begin();
		}
		[[nodiscard]] Devices::ConstIteratorType begin() const
		{
			return m_devices.begin();
		}
		[[nodiscard]] Devices::IteratorType end()
		{
			return m_devices.end();
		}
		[[nodiscard]] Devices::ConstIteratorType end() const
		{
			return m_devices.end();
		}

		[[nodiscard]] Optional<PhysicalDevice*> FindPhysicalDevice(const Rendering::PhysicalDeviceView device);
	private:
		Devices m_devices;
	};
};
