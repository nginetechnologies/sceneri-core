#pragma once

#include "Engine/Scripting/Parser/ScriptFunction.h"
#include "Engine/Scripting/Interpreter/Environment.h"

#include <Common/Memory/SharedPtr.h>

namespace ngine::Scripting::AST::Expression
{
	struct Function;
}

namespace ngine::Scripting
{
	struct UserScriptFunction : public ScriptFunction
	{
		UserScriptFunction(
			const Guid guid,
			UnicodeString&& displayName,
			Reflection::ReturnType&& returnType,
			ArgumentNames&& argumentNames,
			Arguments&& arguments,
			const AST::Expression::Function& function,
			SharedPtr<Environment>&& pClosure
		)
			: ScriptFunction(
					guid,
					Forward<UnicodeString>(displayName),
					Forward<Reflection::ReturnType>(returnType),
					Forward<ArgumentNames>(argumentNames),
					Forward<Arguments>(arguments)
				)
			, m_function(function)
			, m_pClosure(Forward<SharedPtr<Environment>>(pClosure))
		{
		}

		const AST::Expression::Function& m_function;
		SharedPtr<Environment> m_pClosure;
	};
}
