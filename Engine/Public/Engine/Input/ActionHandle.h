#pragma once

#include <Engine/Input/ActionIdentifier.h>
#include <Engine/Input/ActionReceiverIdentifier.h>
#include <Common/Storage/Identifier.h>

namespace ngine::Input
{
	struct ActionHandle
	{
		ActionIdentifier actionIdentifier;
		ActionReceiverIdentifier receiverIdentifier;

		bool IsValid()
		{
			return actionIdentifier.IsValid() && receiverIdentifier.IsValid();
		}

		bool IsInvalid()
		{
			return !IsValid();
		}
	};

}
