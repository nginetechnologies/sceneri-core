#pragma once

#include "Registry.h"
#include <Common/Reflection/Registry.inl>

namespace ngine::Reflection
{
	template<typename Type>
	inline void EngineRegistry::RegisterDynamicType()
	{
		constexpr auto& reflectedType = GetType<Type>();
		if constexpr (reflectedType.HasFunctions)
		{
			Internal::RegisterDynamicFunctions<0, Type>(*this);
		}

		RegisterDynamicType(reflectedType.GetGuid(), reflectedType);
	}
}
