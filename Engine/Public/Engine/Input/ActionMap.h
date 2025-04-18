#pragma once

#include <Common/Function/Function.h>
#include <Common/Guid.h>
#include <Common/Math/ForwardDeclarations/Vector2.h>
#include <Common/Math/ForwardDeclarations/Vector3.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Reflection/GetType.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/PersistentIdentifierStorage.h>

#include <Engine/Input/ActionHandle.h>
#include <Engine/Input/ActionIdentifier.h>
#include <Engine/Input/ActionMapBinding.h>
#include <Engine/Input/Actions/ActionMonitor.h>
#include <Engine/Input/Actions/Vector2DeltaAction.h>
#include <Engine/Input/Actions/Vector3Action.h>
#include <Engine/Input/Actions/BinaryAction.h>

namespace ngine::Input
{
	struct BinaryAction;
	struct Vector3Action;
	struct Vector2DeltaAction;

	struct ActionMap;

	using BinaryActionCallback = Function<void(bool), 24>;
	using AxisActionCallback = Function<void(Math::Vector3f), 24>;
	using DeltaActionCallback = Function<void(Math::Vector2f), 24>;

	using ActionMapLoadedCallback = Function<void(void), 24>;

	struct ActionMap
	{
		ActionMap(ActionMonitor& actionMonitor, Serialization::Reader reader);
		ActionMap(const ActionMap&) = delete;
		ActionMap& operator=(const ActionMap&) = delete;
		ActionMap(ActionMap&&) = delete;
		ActionMap& operator=(ActionMap&&) = delete;
		~ActionMap() = default;

		/**
		    Returns a handle that can be used to retrieve the created binary action.  It is the user's responsibility to store this handle and
		   unbind the input when they are done with it.
		*/
		[[nodiscard]] ActionHandle BindBinaryAction(Guid actionGuid, BinaryActionCallback&& callback);
		[[nodiscard]] bool GetBinaryActionState(ActionHandle actionHandle);
		void UnbindBinaryAction(ActionHandle actionHandle);

		/**
		    Returns a handle that can be used to retrieve the created axis action.  It is the user's responsibility to store this handle and
		   unbind the input when they are done with it.
		*/
		[[nodiscard]] ActionHandle BindAxisAction(Guid actionGuid, AxisActionCallback&& callback);
		[[nodiscard]] Math::Vector3f GetAxisActionState(ActionHandle actionHandle);
		void UnbindAxisAction(ActionHandle actionHandle);

		[[nodiscard]] ActionHandle BindDeltaAction(Guid actionGuid, DeltaActionCallback&& callback);
		void UnbindDeltaAction(ActionHandle actionHandle);
	private:
		void DeserializeAction(Guid actionGuid, Serialization::Reader reader);
		void DeserializeBinaryBindings(ActionIdentifier actionIdentifier, Serialization::Reader reader);
		void DeserializeAxisBindings(ActionIdentifier actionIdentifier, Serialization::Reader reader);

		Optional<ActionMonitor*> m_pActionMonitor;

		ActionMapBinding<BinaryAction, bool> m_binaryActionBindings;
		ActionMapBinding<Vector3Action, Math::Vector3f> m_axisActionBindings;
		ActionMapBinding<Vector2DeltaAction, Math::Vector2f> m_deltaActionBindings;

		PersistentIdentifierStorage<ActionIdentifier> m_actionLookup;
		TSaltedIdentifierStorage<ActionReceiverIdentifier> m_receiverLookup;
	};

}
