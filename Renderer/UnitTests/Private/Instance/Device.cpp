#include <Common/Memory/New.h>

#include <Common/Tests/UnitTest.h>
#include <Common/Version.h>
#include <Common/Memory/Containers/ZeroTerminatedStringView.h>
#include <Common/EnumFlags.h>

#include <Renderer/Instance.h>
#include <Renderer/Devices/PhysicalDevices.h>
#include <Renderer/Devices/PhysicalDevice.h>
#include <Renderer/Devices/LogicalDevice.h>

namespace ngine::Rendering::Tests
{
	UNIT_TEST(Device, AcquireLogicalDevice)
	{
		Instance instance("Sceneri unit test app", Version(1, 0, 0), "Sceneri unit test engine", Version(1, 0, 0), Instance::CreationFlags{});
		EXPECT_TRUE(instance.IsValid());
		if (!instance.IsValid())
		{
			return;
		}

		PhysicalDevices physicalDevices(instance);

		const PhysicalDevices::DeviceView physicalDevicesView = physicalDevices.GetView();
		EXPECT_GT(physicalDevicesView.GetSize(), 0);

		const PhysicalDevice& physicalDevice = physicalDevicesView[0];
		EXPECT_TRUE(physicalDevice.IsValid());

		const LogicalDeviceIdentifier logicalDeviceIdentifier = LogicalDeviceIdentifier::MakeFromValidIndex(0);
		UniquePtr<LogicalDevice> pLogicalDevice(
			Memory::ConstructInPlace,
			instance,
			physicalDevice,
			Invalid,
			logicalDeviceIdentifier,
			LogicalDevice::CreateDevice(physicalDevice, Invalid)
		);
		EXPECT_TRUE(pLogicalDevice.IsValid());
		EXPECT_TRUE(pLogicalDevice->IsValid());
	}
}
