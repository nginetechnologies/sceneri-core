#pragma once

#include "ComponentTypeIdentifier.h"
#include <Common/Storage/ForwardDeclarations/IdentifierMask.h>

namespace ngine::Entity
{
	using ComponentTypeMask = IdentifierMask<ComponentTypeIdentifier>;
}
