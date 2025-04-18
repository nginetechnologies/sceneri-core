
#pragma once

#include "Engine/Input/Actions/BinaryAction.h"
#include <Common/Function/Function.h>
#include <Common/Guid.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/PersistentIdentifierStorage.h>
#include <Common/System/Query.h>

#include <Engine/Input/ActionHandle.h>
#include <Engine/Input/ActionIdentifier.h>
#include <Engine/Input/Actions/ActionMonitor.h>
#include <Engine/Input/InputManager.h>

namespace ngine::Input
{
	template<typename ActionType, typename ActionState>
	struct ActionMapBinding
	{
		using ActionCallback = Function<void(ActionState), 24>;

		explicit ActionMapBinding(ActionMonitor& actionMonitor)
			: m_actionMonitor{actionMonitor}
		{
		}

		~ActionMapBinding()
		{
			for (const ActionInfo& actionInfo : m_actionLookup.GetView())
			{
				for (const ActionInputBinding& actionInputBinding : actionInfo.bindings)
				{
					for (InputIdentifier inputIdentifier : actionInputBinding.inputIdentifiers)
					{
						actionInfo.action->UnbindInput(m_actionMonitor, inputIdentifier);
					}
				}
			}
		}

		void
		CreateBinding(ActionIdentifier, const DeviceType&, DeviceTypeIdentifier, InputIdentifier, Serialization::Reader); // Must be specialized

		void DeserializeBindings(const ActionIdentifier actionIdentifier, const Serialization::Reader reader);

		[[nodiscard]] ActionHandle
		BindAction(const ActionIdentifier actionIdentifier, ActionReceiverIdentifier receiverIdentifier, ActionCallback&& callback)
		{
			ActionCallbackArray& receivers = m_actionReceivers[actionIdentifier];
			receivers[receiverIdentifier] = Move(callback);
			return {actionIdentifier, receiverIdentifier};
		}

		void UnbindAction(ActionHandle actionHandle)
		{
			if (actionHandle.IsInvalid())
			{
				return;
			}

			ActionCallbackArray& receivers = m_actionReceivers[actionHandle.actionIdentifier];
			receivers[actionHandle.receiverIdentifier].Unbind();
		}

		[[nodiscard]] ActionState GetActionState(ActionHandle); // Must be specialized
	private:
		struct ActionInputBinding
		{
			Vector<InputIdentifier> inputIdentifiers;
			DeviceTypeIdentifier deviceTypeIdentifier;
		};

		ActionMonitor& m_actionMonitor;

		struct ActionInfo
		{
			UniquePtr<ActionType> action;
			Vector<ActionInputBinding> bindings;
		};

		TIdentifierArray<ActionInfo, ActionIdentifier> m_actionLookup;

		using ActionCallbackArray = TIdentifierArray<ActionCallback, ActionReceiverIdentifier>;
		TIdentifierArray<ActionCallbackArray, ActionIdentifier> m_actionReceivers;

		void OnAction(ActionIdentifier actionIdentifier, ActionState actionState)
		{
			ActionCallbackArray& receivers = m_actionReceivers[actionIdentifier];
			for (ActionCallback& callback : receivers.GetView())
			{
				if (callback.IsValid())
				{
					callback(actionState);
				}
			}
		}
	};
}
