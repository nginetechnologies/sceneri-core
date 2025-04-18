#include <Engine/Scripting/CoreFunctions.h>
#include <Engine/Scripting/Parser/Token.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Assert/Assert.h>

namespace ngine::Scripting::CoreFunctions
{
	void ReflectedAssert([[maybe_unused]] const bool condition)
	{
		Assert(condition);
	}
	bool ReflectedScriptAssert([[maybe_unused]] const bool condition)
	{
		Assert(condition);
		return condition;
	}

	[[maybe_unused]] inline static const bool wasAssertReflected = Reflection::Registry::RegisterGlobalFunction<&ReflectedAssert>();
	[[maybe_unused]] inline static const bool wasScriptAssertReflected = Reflection::Registry::RegisterGlobalFunction<&ReflectedScriptAssert>(
	);
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedFunction<&Scripting::CoreFunctions::ReflectedScriptAssert>
	{
		static constexpr auto Function = Reflection::Function{
			Scripting::Token::GuidFromScriptString(SCRIPT_STRING_LITERAL("assert")),
			MAKE_UNICODE_LITERAL("Assert"),
			&Scripting::CoreFunctions::ReflectedScriptAssert,
			Reflection::FunctionFlags{},
			Reflection::Argument{"4ea7116c-cdeb-4aad-8088-4c78c0c7aeb2"_guid, MAKE_UNICODE_LITERAL("Result")},
			Reflection::Argument{"0c189c45-653b-4685-895b-18050ca14c61"_guid, MAKE_UNICODE_LITERAL("Condition")}
		};
	};
}
