#pragma once

#include "ComponentIdentifier.h"
#include <Common/Storage/ForwardDeclarations/IdentifierMask.h>

namespace ngine::Entity
{
	using ComponentMask = IdentifierMask<ComponentIdentifier>;
}
