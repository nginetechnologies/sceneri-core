#pragma once

#include <Common/Storage/ForwardDeclarations/Identifier.h>

namespace ngine::Rendering
{
	//! Set maximum count to 5 as that's the minimum requirement for number of logical devices in the Vulkan standard
	using LogicalDeviceIdentifier = TIdentifier<uint32, 8, 255>;
}
