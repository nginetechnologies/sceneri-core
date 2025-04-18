#pragma once

#include <Common/Reflection/Function.h>

namespace ngine::Scripting::CoreFunctions
{
	void ReflectedAssert(const bool condition);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedFunction<&Scripting::CoreFunctions::ReflectedAssert>
	{
		static constexpr auto Function = Reflection::Function{
			"76e31226-c4fa-4c81-b34e-2878306625a0"_guid,
			MAKE_UNICODE_LITERAL("Assert"),
			&Scripting::CoreFunctions::ReflectedAssert,
			Reflection::FunctionFlags{},
			Reflection::ReturnType{},
			Reflection::Argument{"0c189c45-653b-4685-895b-18050ca14c61"_guid, MAKE_UNICODE_LITERAL("Condition")}
		};
	};
}
