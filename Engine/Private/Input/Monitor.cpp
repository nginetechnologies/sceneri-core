#include "Input/Monitor.h"
#include "Input/DeviceType.h"
#include "Input/InputManager.h"

#include <Common/System/Query.h>

namespace ngine::Input
{
	Monitor::~Monitor()
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		inputManager.OnMonitorDestroyed(*this);
	}

	void DeviceInstance::SetActiveMonitor(Monitor* pMonitor, const DeviceType& deviceType)
	{
		Assert((m_pActiveMonitor != pMonitor) | (pMonitor == nullptr));

		if (m_pActiveMonitor != nullptr)
		{
			m_pActiveMonitor->OnLoseDeviceFocus(m_identifier, m_typeIdentifier);
		}

		m_pActiveMonitor = pMonitor;

		if (pMonitor != nullptr)
		{
			pMonitor->OnReceiveDeviceFocus(m_identifier, m_typeIdentifier);
			OnMonitorChanged(*pMonitor, deviceType, System::Get<Input::Manager>());
		}
	}
}
