#pragma once

#include <Common/Storage/ForwardDeclarations/Identifier.h>

namespace ngine::Entity
{
	using ComponentInstanceIdentifier = TIdentifier<uint32, 14>;
	using GenericComponentInstanceIdentifier = TIdentifier<uint64, 17>;
}
