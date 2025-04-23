#include "VirtualController/VirtualControllerManager.h"

#include "GameFramework/PlayerInfo.h"

#include <Engine/Input/InputManager.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>

#include <Widgets/Widget.h>

#include <Common/System/Query.h>
#include <Common/Function/Event.h>
#include <Common/Memory/Optional.h>
#include <Common/Platform/Unused.h>

namespace ngine::GameFramework
{
	VirtualControllerManager::VirtualControllerManager()
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		if (gamepadDeviceType.IsVirtualGamepadEnabled())
		{
			EnableVirtualControllers();
		}

		gamepadDeviceType.OnVirtualGamepadEnabled.Add(*this, &VirtualControllerManager::EnableVirtualControllers);
		gamepadDeviceType.OnVirtualGamepadDisabled.Add(*this, &VirtualControllerManager::DisableVirtualControllers);
	}

	VirtualControllerManager::~VirtualControllerManager()
	{
		Input::Manager& inputManager = System::Get<Input::Manager>();
		Input::GamepadDeviceType& gamepadDeviceType =
			inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());

		gamepadDeviceType.OnVirtualGamepadEnabled.Remove(this);
		gamepadDeviceType.OnVirtualGamepadDisabled.Remove(this);
	}

	void VirtualControllerManager::CreateController(const PlayerInfo& playerInfo)
	{
		const ClientIdentifier clientIdentifer = playerInfo.GetClientIdentifier();
		m_playerInfos[clientIdentifer] = &playerInfo;
		if (m_isEnabled)
		{
			if (const Optional<Widgets::Widget*> pHUDWidget = playerInfo.GetHUD())
			{
				VirtualController& controller = m_controllers[clientIdentifer];
				Assert(!controller.IsValid(), "Controller already initialized");
				controller.Initialize(*pHUDWidget);
			}
		}
	}

	void VirtualControllerManager::DestroyController(const PlayerInfo& playerInfo)
	{
		const ClientIdentifier clientIdentifer = playerInfo.GetClientIdentifier();
		m_playerInfos[clientIdentifer] = nullptr;
		if (m_isEnabled)
		{
			VirtualController& controller = m_controllers[clientIdentifer];
			Assert(controller.IsValid(), "Controller was not initialized");
			controller.Destroy();
		}
	}

	void VirtualControllerManager::EnableVirtualControllers()
	{
		if (!m_isEnabled)
		{
			m_isEnabled = true;

			for (const PlayerInfo* pPlayerInfo : m_playerInfos.GetDynamicView())
			{
				if (pPlayerInfo)
				{
					const ClientIdentifier clientIdentifer = pPlayerInfo->GetClientIdentifier();
					if (const Optional<Widgets::Widget*> pHUDWidget = pPlayerInfo->GetHUD())
					{
						m_controllers[clientIdentifer].Initialize(*pHUDWidget);
					}
				}
			}
		}
	}

	void VirtualControllerManager::DisableVirtualControllers()
	{
		if (m_isEnabled)
		{
			m_isEnabled = false;

			for (VirtualController& controller : m_controllers.GetDynamicView())
			{
				if (controller.IsValid())
				{
					controller.Destroy();
				}
			}
		}
	}
}
