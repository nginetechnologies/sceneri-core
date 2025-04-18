#include <Engine/Entity/InputComponent.h>

#include <Common/Function/Function.h>
#include <Common/Guid.h>
#include <Common/Math/ForwardDeclarations/Vector3.h>
#include <Common/Memory/Move.h>
#include <Common/Memory/Optional.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Reflection/GetType.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Serialization/Reader.h>
#include <Common/Storage/Identifier.h>

#include <Engine/Entity/ComponentType.h>
#include <Engine/Entity/ComponentTypeSceneData.h>

#include <Engine/Input/ActionIdentifier.h>
#include <Engine/Input/ActionMap.h>
#include <Engine/Input/ActionMapAssetType.h>
#include <Engine/Input/ActionMapBinding.inl>
#include <Engine/Input/DeserializeActionMapInstance.h>
#include <Engine/Input/DeviceIdentifier.h>
#include <Engine/Input/DeviceType.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>
#include <Engine/Input/Devices/Gamepad/GamepadInput.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>
#include <Engine/Input/Devices/Keyboard/KeyboardInput.h>
#include <Engine/Input/InputIdentifier.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Input/Tags.h>

namespace ngine::Entity
{
	InputComponent::InputComponent(const InputComponent& templateComponent, const Cloner&)
		: m_actionMapAsset{templateComponent.m_actionMapAsset}
	{
	}

	InputComponent::InputComponent(const Deserializer& deserializer)
		: m_actionMapAsset(deserializer.m_reader.ReadWithDefaultValue<Asset::Guid>("actionmap", Asset::Guid{}))
	{
	}
	InputComponent::InputComponent(Initializer&& initializer)
		: m_actionMapAsset{initializer.m_actionMapAsset}
	{
	}

	void InputComponent::OnDestroying()
	{
		m_pActionMap.DestroyElement();
	}

	void InputComponent::OnEnable()
	{
		NotifyEnable();
	}

	void InputComponent::OnDisable()
	{
		NotifyDisable();
	}

	void InputComponent::UnassignMonitor()
	{
		m_pActionMap.DestroyElement();
		m_pActionMonitor = nullptr;
	}
	void InputComponent::AssignMonitor(Input::ActionMonitor& actionMonitor)
	{
		m_pActionMonitor = &actionMonitor;

		if (m_actionMapAsset.IsValid())
		{
			if (Threading::Job* pJob = Input::DeserializeInstanceAsync(m_actionMapAsset, actionMonitor,
			[this](UniquePtr<Input::ActionMap>&& pActionMap) {
				m_pActionMap = Move(pActionMap);
				NotifyAssignMonitor();
			}))
			{
				pJob->Queue(*Threading::JobRunnerThread::GetCurrent());
			}
		}
		else
		{
			NotifyAssignMonitor();
		}
	}

	Optional<Input::ActionMap*> InputComponent::GetActionMap() const
	{
		return m_pActionMap;
	}

	void InputComponent::SetActionMapAsset(const Asset::Picker asset)
	{
		m_actionMapAsset = asset.GetAssetGuid();
	}

	Asset::Picker InputComponent::GetActionMapAsset() const
	{
		return Asset::Picker{
			Asset::Reference{m_actionMapAsset, Input::ActionMapAssetType::AssetFormat.assetTypeGuid},
			{Input::Tags::ActionMapGuid}
		};
	}

	void InputComponent::NotifyAssignMonitor()
	{
		if (m_pActionMonitor.IsValid())
		{
			OnAssignedActionMonitor(*m_pActionMonitor);

			Input::Manager& inputManager = System::Get<Input::Manager>();
			Input::GamepadDeviceType& gamepadDeviceType =
				inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());
			gamepadDeviceType.OnMonitorAssigned(*m_pActionMonitor);
			gamepadDeviceType.OnMonitorAssignedEnd(*m_pActionMonitor);
		}
	}

	void InputComponent::NotifyEnable()
	{
		if (m_pActionMonitor.IsValid())
		{
			OnInputEnabled(*m_pActionMonitor);

			Input::Manager& inputManager = System::Get<Input::Manager>();
			Input::GamepadDeviceType& gamepadDeviceType =
				inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());
			gamepadDeviceType.OnInputEnabled(*m_pActionMonitor);
		}
	}

	void InputComponent::NotifyDisable()
	{
		if (m_pActionMonitor.IsValid())
		{
			OnInputDisabled(*m_pActionMonitor);

			Input::Manager& inputManager = System::Get<Input::Manager>();
			Input::GamepadDeviceType& gamepadDeviceType =
				inputManager.GetDeviceType<Input::GamepadDeviceType>(inputManager.GetGamepadDeviceTypeIdentifier());
			gamepadDeviceType.OnInputDisabled(*m_pActionMonitor);
		}
	}

	[[maybe_unused]] const bool wasInputComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<InputComponent>>::Make());
	[[maybe_unused]] const bool wasInputComponentTypeRegistered = Reflection::Registry::RegisterType<InputComponent>();
}
