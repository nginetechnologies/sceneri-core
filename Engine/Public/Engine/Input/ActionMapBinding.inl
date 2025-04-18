#pragma once

#include <Common/Math/Vector3.h>
#include <Common/Serialization/Reader.h>
#include <Engine/Input/ActionIdentifier.h>
#include <Engine/Input/ActionMapBinding.h>
#include <Engine/Input/Actions/BinaryAction.h>
#include <Engine/Input/Actions/Vector2DeltaAction.h>
#include <Engine/Input/Actions/Vector3Action.h>
#include <Engine/Input/InputIdentifier.h>

namespace ngine::Input
{
	template<>
	inline void ActionMapBinding<BinaryAction, bool>::CreateBinding(
		const ActionIdentifier actionIdentifier,
		const DeviceType& deviceType,
		DeviceTypeIdentifier deviceTypeIdentifier,
		InputIdentifier inputIdentifier,
		const Serialization::Reader /*reader*/
	)
	{
		ActionInfo& actionInfo = m_actionLookup[actionIdentifier];
		if (actionInfo.action.IsInvalid())
		{
			actionInfo.action.CreateInPlace();
		}

		actionInfo.action->BindInput(m_actionMonitor, deviceType, inputIdentifier);
		actionInfo.action->OnChanged = [this, actionIdentifier = actionIdentifier](DeviceIdentifier, const bool inputValue)
		{
			OnAction(actionIdentifier, inputValue);
		};

		Vector<InputIdentifier> inputIdentifiers;
		inputIdentifiers.EmplaceBack(inputIdentifier);

		Vector<ActionInputBinding>& actions = actionInfo.bindings;
		actions.EmplaceBack(ActionInputBinding{
			Move(inputIdentifiers),
			deviceTypeIdentifier,
		});
	}

	template<>
	inline bool ActionMapBinding<BinaryAction, bool>::GetActionState(ActionHandle actionHandle)
	{
		const ActionInfo& actionInfo = m_actionLookup[actionHandle.actionIdentifier];
		return actionInfo.action.IsValid() ? actionInfo.action->IsActive() : false;
	}

	template<>
	inline void ActionMapBinding<Vector3Action, Math::Vector3f>::CreateBinding(
		const ActionIdentifier actionIdentifier,
		const DeviceType& deviceType,
		DeviceTypeIdentifier deviceTypeIdentifier,
		InputIdentifier inputIdentifier,
		const Serialization::Reader bindingValue
	)
	{
		auto bindAction = [this, actionIdentifier, &deviceType, inputIdentifier, &bindingValue](Vector3Action& action)
		{
			if (Optional<Math::Vector3f> direction = bindingValue.Read<Math::Vector3f>("direction"))
			{
				action.BindAxisInput(m_actionMonitor, deviceType, inputIdentifier, *direction);
			}
			else
			{
				action.Bind2DAxisInput(m_actionMonitor, deviceType, inputIdentifier);
			}

			action.OnChanged = [this, actionIdentifier = actionIdentifier](DeviceIdentifier, const Math::Vector3f inputValue)
			{
				OnAction(actionIdentifier, inputValue);
			};
		};

		// If this device already has a binding for this action then bind the new input to that
		ActionInfo& actionInfo = m_actionLookup[actionIdentifier];
		if (actionInfo.action.IsInvalid())
		{
			actionInfo.action.CreateInPlace();
		}

		if (auto it = actionInfo.bindings.FindIf(
					[deviceTypeIdentifier](ActionInputBinding& action)
					{
						return action.deviceTypeIdentifier == deviceTypeIdentifier;
					}
				);
		    it != actionInfo.bindings.end())
		{
			it->inputIdentifiers.EmplaceBack(inputIdentifier);
			bindAction(actionInfo.action.GetReference());
		}
		else
		{
			Vector<InputIdentifier> inputIdentifiers;
			inputIdentifiers.EmplaceBack(inputIdentifier);

			bindAction(actionInfo.action.GetReference());

			actionInfo.bindings.EmplaceBack(ActionInputBinding{
				Move(inputIdentifiers),
				deviceTypeIdentifier,
			});
		}
	}

	template<>
	inline Math::Vector3f ActionMapBinding<Vector3Action, Math::Vector3f>::GetActionState(ActionHandle actionHandle)
	{
		const ActionInfo& actionInfo = m_actionLookup[actionHandle.actionIdentifier];
		return actionInfo.action.IsValid() ? actionInfo.action->GetValue() : Math::Zero;
	}

	template<>
	inline void ActionMapBinding<Vector2DeltaAction, Math::Vector2f>::CreateBinding(
		const ActionIdentifier actionIdentifier,
		const DeviceType& deviceType,
		DeviceTypeIdentifier deviceTypeIdentifier,
		InputIdentifier inputIdentifier,
		const Serialization::Reader /*bindingValue*/
	)
	{
		ActionInfo& actionInfo = m_actionLookup[actionIdentifier];
		if (actionInfo.action.IsInvalid())
		{
			actionInfo.action.CreateInPlace();
		}

		actionInfo.action->BindInput(m_actionMonitor, deviceType, inputIdentifier);
		actionInfo.action->OnChanged = [this, actionIdentifier = actionIdentifier](DeviceIdentifier, const Math::Vector2i inputValue)
		{
			OnAction(actionIdentifier, static_cast<Math::Vector2f>(inputValue));
		};

		Vector<InputIdentifier> inputIdentifiers;
		inputIdentifiers.EmplaceBack(inputIdentifier);

		actionInfo.bindings.EmplaceBack(ActionInputBinding{
			Move(inputIdentifiers),
			deviceTypeIdentifier,
		});
	}

	template<typename ActionType, typename ActionState>
	inline void ActionMapBinding<ActionType, ActionState>::DeserializeBindings(
		const ActionIdentifier actionIdentifier, const Serialization::Reader reader
	)
	{
		// For each device listed within an action
		for (Serialization::Member<Serialization::Reader> actionMember : reader.GetMemberView())
		{
			Guid deviceGuid = Guid::TryParse(actionMember.key);
			Assert(deviceGuid.IsValid());
			if (UNLIKELY_ERROR(deviceGuid.IsInvalid()))
			{
				continue;
			}
			const Input::Manager& inputManager = System::Get<Input::Manager>();
			const DeviceTypeIdentifier deviceTypeIdentifier = inputManager.GetDeviceTypeIdentifier(deviceGuid);

			Assert(deviceTypeIdentifier.IsValid());
			if (UNLIKELY_ERROR(deviceTypeIdentifier.IsInvalid()))
			{
				continue;
			}

			const DeviceType& deviceType = inputManager.GetDeviceType(deviceTypeIdentifier);

			// For each input entry for the current device
			for (const Serialization::Reader arrayValue : actionMember.value.GetArrayView())
			{
				const InputIdentifier inputIdentifier = deviceType.DeserializeDeviceInput(arrayValue);
				CreateBinding(actionIdentifier, deviceType, deviceTypeIdentifier, inputIdentifier, arrayValue);
			}
		}
	}
}
