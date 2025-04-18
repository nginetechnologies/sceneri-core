#include <Common/Math/Vector3.h>
#include <Common/Memory/Move.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Reflection/GetType.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Serialization/Reader.h>
#include <Common/System/Query.h>
#include <Common/Threading/Jobs/Job.h>
#include <Common/Threading/Jobs/JobRunnerThread.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

#include <Engine/Input/ActionHandle.h>
#include <Engine/Input/ActionIdentifier.h>
#include <Engine/Input/ActionMap.h>
#include <Engine/Input/ActionMapAssetType.h>
#include <Engine/Input/ActionMapBinding.inl>
#include <Engine/Input/Actions/Action.h>
#include <Engine/Input/Actions/ActionMonitor.h>
#include <Engine/Input/Actions/BinaryAction.h>
#include <Engine/Input/Actions/Vector3Action.h>
#include <Engine/Input/DeviceIdentifier.h>
#include <Engine/Input/DeviceType.h>
#include <Engine/Input/Devices/Gamepad/Gamepad.h>
#include <Engine/Input/Devices/Gamepad/GamepadInput.h>
#include <Engine/Input/Devices/Keyboard/Keyboard.h>
#include <Engine/Input/Devices/Keyboard/KeyboardInput.h>
#include <Engine/Input/InputIdentifier.h>
#include <Engine/Input/InputManager.h>
#include <Engine/Threading/JobManager.h>

namespace ngine::Input
{
	ActionMap::ActionMap(ActionMonitor& actionMonitor, const Serialization::Reader reader)
		: m_pActionMonitor{actionMonitor}
		, m_binaryActionBindings{actionMonitor}
		, m_axisActionBindings{actionMonitor}
		, m_deltaActionBindings{actionMonitor}
	{
		for (Serialization::Member<Serialization::Reader> actionMember : reader.GetMemberView())
		{
			if (Guid actionGuid = Guid::TryParse(actionMember.key); actionGuid.IsValid())
			{
				DeserializeAction(actionGuid, actionMember.value);
			}
		}
	}

	void ActionMap::DeserializeAction(const Guid actionGuid, const Serialization::Reader reader)
	{
		[[maybe_unused]] ActionIdentifier actionIdentifier = m_actionLookup.FindOrRegister(actionGuid);

		if (const Optional<Serialization::Reader> binaryBindingsReader = reader.FindSerializer("binary_bindings"))
		{
			m_binaryActionBindings.DeserializeBindings(actionIdentifier, *binaryBindingsReader);
		}

		if (const Optional<Serialization::Reader> axisBindingsReader = reader.FindSerializer("axis_bindings"))
		{
			m_axisActionBindings.DeserializeBindings(actionIdentifier, *axisBindingsReader);
		}

		if (const Optional<Serialization::Reader> deltaBindingsReader = reader.FindSerializer("delta_bindings"))
		{
			m_deltaActionBindings.DeserializeBindings(actionIdentifier, *deltaBindingsReader);
		}
	}

	ActionHandle ActionMap::BindBinaryAction(const Guid actionGuid, BinaryActionCallback&& callback)
	{
		const ActionIdentifier actionIdentifier = m_actionLookup.FindOrRegister(actionGuid);
		const ActionReceiverIdentifier receiverIdentifier = m_receiverLookup.AcquireIdentifier();
		return m_binaryActionBindings.BindAction(actionIdentifier, receiverIdentifier, Move(callback));
	}

	bool ActionMap::GetBinaryActionState(const ActionHandle actionHandle)
	{
		return m_binaryActionBindings.GetActionState(actionHandle);
	}

	void ActionMap::UnbindBinaryAction(const ActionHandle actionHandle)
	{
		return m_binaryActionBindings.UnbindAction(actionHandle);
	}

	ActionHandle ActionMap::BindAxisAction(const Guid actionGuid, AxisActionCallback&& callback)
	{
		const ActionIdentifier actionIdentifier = m_actionLookup.FindOrRegister(actionGuid);
		const ActionReceiverIdentifier receiverIdentifier = m_receiverLookup.AcquireIdentifier();
		return m_axisActionBindings.BindAction(actionIdentifier, receiverIdentifier, Move(callback));
	}

	Math::Vector3f ActionMap::GetAxisActionState(const ActionHandle actionHandle)
	{
		return m_axisActionBindings.GetActionState(actionHandle);
	}

	void ActionMap::UnbindAxisAction(const ActionHandle actionHandle)
	{
		m_axisActionBindings.UnbindAction(actionHandle);
	}

	ActionHandle ActionMap::BindDeltaAction(const Guid actionGuid, DeltaActionCallback&& callback)
	{
		const ActionIdentifier actionIdentifier = m_actionLookup.FindOrRegister(actionGuid);
		const ActionReceiverIdentifier receiverIdentifier = m_receiverLookup.AcquireIdentifier();
		return m_deltaActionBindings.BindAction(actionIdentifier, receiverIdentifier, Move(callback));
	}

	void ActionMap::UnbindDeltaAction(ActionHandle actionHandle)
	{
		m_deltaActionBindings.UnbindAction(actionHandle);
	}

	[[maybe_unused]] const bool wasAssetTypeRegistered = Reflection::Registry::RegisterType<ActionMapAssetType>();
}
