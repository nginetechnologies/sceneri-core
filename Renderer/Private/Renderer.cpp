#include "Renderer.h"
#include "Window/Window.h"

#include <Common/Memory/OffsetOf.h>

#include "Devices/PhysicalDevice.h"
#include "Devices/LogicalDevice.h"

#include <Engine/Engine.h>
#include <Engine/Threading/JobRunnerThread.h>
#include <Engine/Threading/JobManager.h>

#include <Common/System/Query.h>

namespace ngine::Rendering
{
	inline static constexpr bool EnableValidation = PROFILE_BUILD &&
	                                                (!RENDERER_VULKAN || (!PLATFORM_APPLE_IOS || PLATFORM_APPLE_MACCATALYST));
	inline static constexpr EnumFlags<Instance::CreationFlags> DefaultInstanceCreationFlags{
		Instance::CreationFlags::Validation * EnableValidation
	};

	Renderer::Renderer()
		: m_instance(
				System::Get<Engine>().GetInfo().GetAsciiName(),
				System::Get<Engine>().GetInfo().GetVersion(),
				System::Get<Engine>().GetInfo().GetAsciiName(),
				System::Get<Engine>().GetInfo().GetVersion(),
				DefaultInstanceCreationFlags
			)
		, m_physicalDevices(m_instance)
	{
		System::Get<Engine>().OnRendererInitialized();
	}

	Renderer::~Renderer()
	{
		for (Threading::JobRunnerThread& thread : System::Get<Threading::JobManager>().GetJobThreads())
		{
			static_cast<Threading::EngineJobRunnerThread&>(thread).GetRenderData().Destroy();
		}

		for (UniquePtr<LogicalDevice>& pLogicalDevice : m_logicalDeviceIdentifiers.GetValidElementView(m_logicalDevices.GetView()))
		{
			pLogicalDevice.DestroyElement();
		}
	}

	Optional<LogicalDevice*> Renderer::CreateLogicalDeviceFromPhysicalDevice(
		const PhysicalDevice& physicalDevice,
		const Optional<uint8> presentQueueIndex,
		const ArrayView<const ConstZeroTerminatedStringView, uint8> enabledExtensions
	)
	{
		LogicalDeviceIdentifier logicalDeviceIdentifier = m_logicalDeviceIdentifiers.AcquireIdentifier();
		if (LIKELY(logicalDeviceIdentifier.IsValid()))
		{
			const Rendering::LogicalDeviceView device = LogicalDevice::CreateDevice(physicalDevice, presentQueueIndex, enabledExtensions);
			if (UNLIKELY_ERROR(!device.IsValid()))
			{
				return nullptr;
			}

			m_logicalDevices[logicalDeviceIdentifier]
				.CreateInPlace(m_instance, physicalDevice, presentQueueIndex, logicalDeviceIdentifier, device);
			LogicalDevice& logicalDevice = *m_logicalDevices[logicalDeviceIdentifier];
			OnLogicalDeviceCreated(logicalDevice);
			return logicalDevice;
		}

		return Invalid;
	}

	void Renderer::DestroyLogicalDevice(LogicalDevice& logicalDevice)
	{
		const LogicalDeviceIdentifier logicalDeviceIdentifier = logicalDevice.GetIdentifier();
		m_logicalDevices[logicalDeviceIdentifier].DestroyElement();
		m_logicalDeviceIdentifiers.ReturnIdentifier(logicalDeviceIdentifier);
	}
}
