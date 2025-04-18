#pragma once

#include "InstanceView.h"

#include <Common/EnumFlagOperators.h>
#include <Common/ForwardDeclarations/EnumFlags.h>
#include <Common/Memory/Containers/ForwardDeclarations/ZeroTerminatedStringView.h>

namespace ngine
{
	struct Version;
}

namespace ngine::Rendering
{
	struct Instance : public InstanceView
	{
		enum class CreationFlags : uint8
		{
			Validation = 1 << 0,
			ExtendedValidation = 1 << 1
		};

		Instance(
			const ConstZeroTerminatedStringView applicationName,
			const Version applicationVersion,
			const ConstZeroTerminatedStringView engineName,
			const Version engineVersion,
			const EnumFlags<CreationFlags> creationFlags
		);
		~Instance();
	};
	ENUM_FLAG_OPERATORS(Instance::CreationFlags);
}
