#pragma once

#include "DeviceIdentifier.h"
#include "DeviceTypeIdentifier.h"
#include "InputIdentifier.h"

#include <Common/Storage/Identifier.h>
#include <Common/Guid.h>

namespace ngine::Serialization
{
	struct Reader;
}

namespace ngine::Input
{
	struct Monitor;
	struct Manager;

	struct DeviceType
	{
		DeviceType(const DeviceTypeIdentifier identifier)
			: m_identifier(identifier)
		{
		}

		DeviceType(const DeviceType&) = delete;
		DeviceType(DeviceType&&) = default;
		DeviceType& operator=(const DeviceType&) = delete;
		DeviceType& operator=(DeviceType&&) = default;
		virtual ~DeviceType() = default;

		virtual void RestoreInputState(Monitor&, const InputIdentifier) const = 0;

		[[nodiscard]] virtual InputIdentifier DeserializeDeviceInput(const Serialization::Reader&) const = 0;
	protected:
		DeviceTypeIdentifier m_identifier;
	};

	struct DeviceInstance
	{
		DeviceInstance(const DeviceIdentifier identifier, const DeviceTypeIdentifier typeIdentifier, Optional<Monitor*> pActiveMonitor)
			: m_identifier(identifier)
			, m_typeIdentifier(typeIdentifier)
			, m_pActiveMonitor(pActiveMonitor)
		{
		}
		virtual ~DeviceInstance()
		{
		}

		[[nodiscard]] DeviceIdentifier GetIdentifier() const
		{
			return m_identifier;
		}

		[[nodiscard]] DeviceTypeIdentifier GetTypeIdentifier() const
		{
			return m_typeIdentifier;
		}

		[[nodiscard]] Optional<Monitor*> GetActiveMonitor() const
		{
			return m_pActiveMonitor;
		}
		void SetActiveMonitor(Monitor* pMonitor, const DeviceType& deviceType);
		void SetActiveMonitor(Monitor& monitor, const DeviceType& deviceType)
		{
			SetActiveMonitor(&monitor, deviceType);
		}

		virtual void OnMonitorChanged([[maybe_unused]] Monitor& newMonitor, const DeviceType&, Manager&)
		{
		}

		virtual void RestoreInputState(Monitor&, const DeviceType&, const InputIdentifier, Manager&)
		{
		}
	private:
		DeviceIdentifier m_identifier;
		DeviceTypeIdentifier m_typeIdentifier;
		Optional<Monitor*> m_pActiveMonitor = nullptr;
	};

}
