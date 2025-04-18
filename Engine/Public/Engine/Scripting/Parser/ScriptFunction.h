#pragma once

#include "Engine/Scripting/Interpreter/Environment.h"
#include "Common/Reflection/FunctionInfo.h"
#include "Common/Memory/Containers/Vector.h"

#include <Common/Memory/SharedPtr.h>
#include <Common/Memory/OffsetOf.h>

namespace ngine::Scripting::AST::Expression
{
	struct Function;
}

namespace ngine::Scripting
{
	struct ScriptFunction
	{
		struct Argument
		{
			Argument(const Guid guid, UnicodeString&& name, const Reflection::TypeDefinition type)
				: m_name(Forward<UnicodeString>(name))
				, m_argument(guid, m_name, Reflection::ArgumentFlags{}, type)
			{
			}

			[[nodiscard]] const Reflection::Argument& GetArgument() const LIFETIME_BOUND
			{
				return m_argument;
			}
		private:
			UnicodeString m_name;
			Reflection::Argument m_argument;
		};

		using ArgumentNames = Vector<UnicodeString>;
		using Arguments = Vector<Reflection::Argument>;
		ScriptFunction(
			const Guid guid,
			UnicodeString&& displayName,
			Reflection::ReturnType&& returnType,
			ArgumentNames&& argumentNames,
			Arguments&& arguments
		)
			: m_displayName(Forward<UnicodeString>(displayName))
			, m_argumentNames(Forward<ArgumentNames>(argumentNames))
			, m_arguments(Forward<Arguments>(arguments))
			, m_functionInfo{
					guid,
					m_displayName,
					Reflection::FunctionFlags::IsScript,
					Forward<Reflection::ReturnType>(returnType),
					[](const Reflection::FunctionInfo& functionInfo) -> ArrayView<const Reflection::Argument, uint8>
					{
						const ScriptFunction& userScriptFunction = Memory::GetConstOwnerFromMember(functionInfo, &ScriptFunction::m_functionInfo);
						return userScriptFunction.m_arguments.GetView();
					}
				}
		{
		}

		[[nodiscard]] const Reflection::FunctionInfo& GetFunctionInfo() const LIFETIME_BOUND
		{
			return m_functionInfo;
		}
	private:
		UnicodeString m_displayName;
		ArgumentNames m_argumentNames;
		Arguments m_arguments;
		Reflection::FunctionInfo m_functionInfo;
	};
}
