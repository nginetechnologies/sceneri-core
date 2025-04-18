#pragma once

#include "ComponentInstanceIdentifier.h"
#include <Common/Storage/ForwardDeclarations/IdentifierMask.h>

namespace ngine::Entity
{
	using ComponentInstanceMask = IdentifierMask<ComponentInstanceIdentifier>;
	using GenericComponentInstanceMask = IdentifierMask<GenericComponentInstanceIdentifier>;
}
