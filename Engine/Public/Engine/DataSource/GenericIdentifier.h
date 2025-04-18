#pragma once

#include <Common/Storage/ForwardDeclarations/Identifier.h>
#include <Common/Storage/ForwardDeclarations/IdentifierMask.h>

namespace ngine::DataSource
{
	using GenericDataIdentifier = TIdentifier<uint32, 16>;
	using GenericDataMask = IdentifierMask<GenericDataIdentifier>;
}
