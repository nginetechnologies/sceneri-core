#include "Engine/Scripting/Interpreter/Interpreter.h"
#include "Engine/Scripting/Interpreter/Environment.h"
#include "Engine/Scripting/Interpreter/UserScriptFunction.h"
#include "Engine/Scripting/Interpreter/ScriptFunctionCache.h"

#include "Engine/Scripting/Parser/AST/Statement.h"
#include "Engine/Scripting/Parser/AST/Expression.h"

#include <Common/Memory/Any.h>
#include <Common/Memory/Variant.h>
#include <Common/Memory/Optional.h>
#include <Common/Memory/Containers/FlatVector.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Reflection/GenericType.h>
#include <Common/Reflection/Registry.h>
#include <Common/IO/Log.h>

namespace ngine::Scripting
{
	Interpreter::Interpreter(SharedPtr<Environment> pEnvironment)
		: m_pEnvironment(Move(pEnvironment))
	{
		SharedPtr<Environment> pGlobalEnvironment = m_pEnvironment->GetGlobalEnvironment();
		m_pGlobalEnvironment = pGlobalEnvironment.IsValid() ? Move(pGlobalEnvironment) : m_pEnvironment;
	}

	bool Interpreter::Interpret(const AST::Statement::Base& statement, SharedPtr<ResolvedVariableMap> pVariables)
	{
		m_pResolvedVariables = Move(pVariables);

		m_state.pEnvironment = m_pEnvironment;
		m_state.returnValues.Clear();
		m_state.flags.Clear();

		Visit(statement);

		return m_state.flags.IsNotSet(Flags::Error);
	}

	ScriptValues Interpreter::Interpret(
		const AST::Expression::Function& functionExpression, ScriptValues&& arguments, SharedPtr<ResolvedVariableMap> pVariables
	)
	{
		m_pResolvedVariables = Move(pVariables);

		m_state.pEnvironment = m_pEnvironment;
		m_state.returnValues.Clear();
		m_state.flags.Clear();

		const ScriptValues functionIdentifierScriptValues = Visit(functionExpression);

		const FunctionIdentifier functionIdentifier = functionIdentifierScriptValues[0].Get().GetExpected<FunctionIdentifier>();
		Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
		const Guid functionGuid = reflectionRegistry.FindFunctionGuid(functionIdentifier);

		const Reflection::FunctionData& functionData = reflectionRegistry.FindFunction(functionGuid);

		[[maybe_unused]] const bool isScriptFunction = m_pEnvironment->GetFunction(functionIdentifier).IsValid();
		Assert(isScriptFunction);

		PUSH_GCC_WARNINGS
		DISABLE_GCC_WARNING("-Wuninitialized")
		Scripting::VM::Registers registers;

		registers.PushArgument<0, Interpreter&>(*this);
		registers.PushArgument<1, FunctionIdentifier>(functionIdentifier);
		registers.PushArgument<2, ScriptValues>(Move(arguments));

		Scripting::VM::ReturnValue returnValue =
			functionData.m_function(registers[0], registers[1], registers[2], registers[3], registers[4], registers[5]);
		m_state.flags.Clear(Flags::Return);

		registers[0] = returnValue.x;
		registers[1] = returnValue.y;
		registers[2] = returnValue.z;
		registers[3] = returnValue.w;
		return registers.ExtractArgument<0, ScriptValues>();
		POP_GCC_WARNINGS
	}

	void Interpreter::Error(StringType::ConstView error)
	{
		LogError("{}", StringType(error));
		m_state.flags.Set(Flags::Error);
	}

	ScriptValues Interpreter::Visit(const AST::Statement::Block& block)
	{
		SharedPtr<Environment> pOldEnvironment = m_state.pEnvironment;

		m_state.pEnvironment = SharedPtr<Environment>::Make(Move(m_state.pEnvironment));
		InterpretStatements(block.GetStatements());
		m_state.pEnvironment = Move(pOldEnvironment);

		return ScriptValues();
	}

	ScriptValues Interpreter::Visit(const AST::Statement::Break&)
	{
		m_state.flags.Set(Flags::Break);
		return ScriptValues();
	}

	ScriptValues Interpreter::Visit(const AST::Statement::Expression& expression)
	{
		return Visit(*expression.GetExpression());
	}

	ScriptValues Interpreter::Visit(const AST::Statement::If& stmtIf)
	{
		const AST::Expressions& conditions = stmtIf.GetConditions();
		const AST::Statements& thens = stmtIf.GetThens();

		bool needElse = stmtIf.GetElse().IsValid();
		auto thenIt = thens.begin();
		for (const ReferenceWrapper<const AST::Expression::Base>& pCondition : conditions)
		{
			if (Visit(*pCondition)[0].IsTruthy())
			{
				needElse = false;
				Visit(*(*thenIt));
				break;
			}
			++thenIt;
		}

		if (needElse)
		{
			Visit(*stmtIf.GetElse());
		}
		return ScriptValues();
	}

	ScriptValues Interpreter::Visit(const AST::Statement::Repeat& repeat)
	{
		SharedPtr<Environment> pOldEnvironment = m_state.pEnvironment;

		const AST::Expression::Base& condition = *repeat.GetCondition();
		const AST::Statements& statements = repeat.GetStatements();

		m_state.pEnvironment = SharedPtr<Environment>::Make(Move(m_state.pEnvironment));
		do
		{
			InterpretStatements(statements);
			if (m_state.flags.AreAnySet(Flags::Break | Flags::Return))
			{
				m_state.flags.Clear(Flags::Break);
				break;
			}
		} while (!Visit(condition)[0].IsTruthy());
		m_state.pEnvironment = Move(pOldEnvironment);

		return ScriptValues();
	}

	ScriptValues Interpreter::Visit(const AST::Statement::Return& stmtReturn)
	{
		m_state.flags.Set(Flags::Return);
		m_state.returnValues.Clear();

		const AST::Expressions& exprlist = stmtReturn.GetExpressions();
		for (const ReferenceWrapper<const AST::Expression::Base>& pExpression : exprlist)
		{
			ScriptValues values = Visit(*pExpression);
			m_state.returnValues.MoveEmplaceRangeBack(values.GetView());
		}
		return ScriptValues();
	}

	ScriptValues Interpreter::Visit(const AST::Statement::While& stmtWhile)
	{
		const AST::Expression::Base& condition = *stmtWhile.GetCondition();
		const AST::Statement::Base& body = *stmtWhile.GetBody();
		while (Visit(condition)[0].IsTruthy())
		{
			Visit(body);
			if (m_state.flags.AreAnySet(Flags::Break | Flags::Return))
			{
				m_state.flags.Clear(Flags::Break);
				break;
			}
		}
		return ScriptValues();
	}

	ScriptValues Interpreter::Visit(const AST::Expression::Assignment& assignment)
	{
		const AST::Expressions& varlist = assignment.GetVariables();
		const AST::Expressions& exprlist = assignment.GetExpressions();

		ScriptValues values;
		uint8 valueIndex = 0;
		uint8 exprIndex = 0;
		const uint8 exprlistCount = uint8(exprlist.GetSize());

		auto valueExpressionIt = exprlist.begin();
		for (const ReferenceWrapper<AST::Expression::Base>& pVariable : varlist)
		{
			ScriptValue value(nullptr);
			if (exprIndex++ < exprlistCount)
			{
				if (exprIndex == exprlistCount)
				{
					ScriptValues newValues = Visit(*(*valueExpressionIt));
					if (newValues.HasElements())
					{
						values.MoveEmplaceRangeBack(newValues.GetView());
						value = Move(values[valueIndex++]);
					}
				}
				else
				{
					value = Visit(*(*valueExpressionIt++))[0];
				}
			}
			else if (valueIndex < values.GetSize())
			{
				value = Move(values[valueIndex++]);
			}

			switch (pVariable->GetType())
			{
				case AST::NodeType::Variable:
				{
					AST::Expression::Variable& variable = static_cast<AST::Expression::Variable&>(*pVariable);
					auto distanceIt = m_pResolvedVariables->Find(uintptr(&variable));
					if (distanceIt != m_pResolvedVariables->end())
					{
						const int32 distance = distanceIt->second;
						m_state.pEnvironment->SetValueAt(distance, variable.GetIdentifier().identifier, Move(value));
					}
					else
					{
						m_pGlobalEnvironment->SetValue(variable.GetIdentifier().identifier, Move(value));
					}
				}
				break;
				case AST::NodeType::VariableDeclaration:
				{
					AST::Expression::VariableDeclaration& variableDeclaration = static_cast<AST::Expression::VariableDeclaration&>(*pVariable);

					auto distanceIt = m_pResolvedVariables->Find(uintptr(&variableDeclaration));
					if (distanceIt != m_pResolvedVariables->end())
					{
						const int32 distance = distanceIt->second;
						m_state.pEnvironment->SetValueAt(distance, variableDeclaration.GetIdentifier().identifier, Move(value));
					}
					else
					{
						m_pGlobalEnvironment->SetValue(variableDeclaration.GetIdentifier().identifier, Move(value));
					}
				}
				break;
				default:
					ExpectUnreachable();
			}
		}
		return ScriptValues();
	}

	ScriptValues Interpreter::Visit(const AST::Expression::Binary& binary)
	{
		const ScriptValue left = Visit(*binary.GetLeft())[0];
		const ScriptValue right = Visit(*binary.GetRight())[0];
		switch (binary.GetOperator().type)
		{
			case TokenType::Star:
				return ScriptValue(left * right);
			case TokenType::Slash:
				return ScriptValue(left / right);
			case TokenType::Minus:
				return ScriptValue(left - right);
			case TokenType::Plus:
				return ScriptValue(left + right);

			case TokenType::Less:
				return ScriptValue(left < right);
			case TokenType::LessEqual:
				return ScriptValue(left <= right);
			case TokenType::Greater:
				return ScriptValue(left > right);
			case TokenType::GreaterEqual:
				return ScriptValue(left >= right);

			case TokenType::NotEqual:
				return ScriptValue(left != right);
			case TokenType::EqualEqual:
				return ScriptValue(left == right);

			case TokenType::Ampersand:
				return ScriptValue(left & right);
			case TokenType::Pipe:
				return ScriptValue(left | right);
			case TokenType::Circumflex:
				return ScriptValue(left ^ right);

			default:
				return ScriptValues();
		}
	}

	void LoadArgument(Scripting::VM::Registers& registers, const uint8 index, const ScriptValue& value, SharedPtr<Environment>&& pEnvironment)
	{
		value.Visit(
			[&registers, index](nullptr_type)
			{
				registers.PushArgument(index, nullptr);
			},
			[&registers, index](const bool value)
			{
				registers.PushArgument(index, value);
			},
			[&registers, index](const IntegerType value)
			{
				registers.PushArgument(index, value);
			},
			[&registers, index](const FloatType value)
			{
				registers.PushArgument(index, value);
			},
			[&registers, index](const StringType& value)
			{
				registers.PushArgument(index, value);
			},
			[&registers, index](const FunctionIdentifier value)
			{
				registers.PushArgument(index, value);
			},
			[&registers, index, pEnvironment = Forward<SharedPtr<Environment>>(pEnvironment)](const ScriptTableIdentifier value)
			{
				if (Optional<ScriptTable*> pTable = pEnvironment->GetTable(value))
				{
					ScriptValue typeValue = pTable->Get(ScriptValue(StringType(SCRIPT_STRING_LITERAL("_type"))));
					if (typeValue.Get().Is<StringType>())
					{
						const StringType& typeName = typeValue.Get().GetExpected<StringType>();
						if (typeName.GetView().EqualsCaseInsensitive(StringType::ConstView(SCRIPT_STRING_LITERAL("vec3"))))
						{
							const float x = float(pTable->Get(ScriptValue(StringType(SCRIPT_STRING_LITERAL("x")))).Get().GetExpected<FloatType>());
							const float y = float(pTable->Get(ScriptValue(StringType(SCRIPT_STRING_LITERAL("y")))).Get().GetExpected<FloatType>());
							const float z = float(pTable->Get(ScriptValue(StringType(SCRIPT_STRING_LITERAL("z")))).Get().GetExpected<FloatType>());

							registers.PushArgument(index, Math::Vector3f(x, y, z));
						}
					}
					registers.PushArgument(index, pTable.Get());
				}
			},
			[&registers, index](const ConstAnyView data)
			{
				registers.PushArgument(index, data.GetData());
			},
			[]()
			{
				ExpectUnreachable();
			}
		);
	}

	[[nodiscard]] ScriptValues
	ConvertResult(const Scripting::VM::ReturnValue returnValue, const Reflection::ReturnType& reflectedReturnTypeDefinition)
	{
		ScriptValues values;
		UNUSED(returnValue);
		UNUSED(reflectedReturnTypeDefinition);
		Assert(false, "TODO");
		return values;
	}

	ScriptValues Interpreter::Visit(const AST::Expression::Call& call)
	{
		ScriptValue calleValue = Visit(*call.GetCallee())[0];
		if (LIKELY(calleValue.Get().Is<FunctionIdentifier>()))
		{
			FunctionIdentifier functionIdentifier = calleValue.Get().GetExpected<FunctionIdentifier>();
			Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
			const Guid functionGuid = reflectionRegistry.FindFunctionGuid(functionIdentifier);
			const Optional<const Reflection::FunctionInfo*> pFunctionDefinition = reflectionRegistry.FindFunctionDefinition(functionGuid);
			Assert(pFunctionDefinition.IsValid());
			if (LIKELY(pFunctionDefinition.IsValid()))
			{
				const Reflection::FunctionData& functionData = reflectionRegistry.FindFunction(functionGuid);

				const bool isScriptFunction = m_pEnvironment->GetFunction(functionIdentifier).IsValid();
				if (isScriptFunction)
				{
					ScriptValues providedArguments;
					switch (call.GetCallee()->GetType())
					{
						case AST::NodeType::VariableDeclaration:
							break;
						case AST::NodeType::Variable:
						{
							const AST::Expression::Variable& calleeVariable = static_cast<const AST::Expression::Variable&>(*call.GetCallee());
							if (calleeVariable.GetObject().IsValid())
							{
								ScriptValues newArguments = Visit(*calleeVariable.GetObject());
								providedArguments.MoveEmplaceRangeBack(newArguments.GetView());
							}
						}
						break;
						default:
							ExpectUnreachable();
					}

					const AST::Expressions& argslist = call.GetArguments();

					for (const ReferenceWrapper<AST::Expression::Base>& pArgExpression : argslist)
					{
						ScriptValues newArguments = Visit(*pArgExpression);
						providedArguments.MoveEmplaceRangeBack(newArguments.GetView());
					}

					const uint8 count = providedArguments.GetSize();

					ScriptValues arguments;
					arguments.Reserve(count);
					for (uint8 index = 0; index < count; ++index)
					{
						if (index < providedArguments.GetSize())
						{
							arguments.EmplaceBack(Move(providedArguments[index]));
						}
						else
						{
							arguments.EmplaceBack(ScriptValue(nullptr));
						}
					}

					Scripting::VM::Registers registers;

					registers.PushArgument<0, Interpreter&>(*this);
					registers.PushArgument<1, FunctionIdentifier>(functionIdentifier);
					registers.PushArgument<2, ScriptValues>(Move(arguments));

					Scripting::VM::ReturnValue returnValue =
						functionData.m_function(registers[0], registers[1], registers[2], registers[3], registers[4], registers[5]);
					m_state.flags.Clear(Flags::Return);

					registers[0] = returnValue.x;
					registers[1] = returnValue.y;
					registers[2] = returnValue.z;
					registers[3] = returnValue.w;
					return registers.ExtractArgument<0, ScriptValues>();
				}
				else
				{
					const ArrayView<const Reflection::Argument, uint8> functionArgumentDefinitions =
						pFunctionDefinition->m_getArgumentsFunction(*pFunctionDefinition);

					const AST::Expressions& argslist = call.GetArguments();

					Scripting::VM::Registers registers;

					Assert(argslist.GetSize() == functionArgumentDefinitions.GetSize());
					if (UNLIKELY_ERROR(argslist.GetSize() != functionArgumentDefinitions.GetSize()))
					{
						Error("Calling function with wrong number of arguments");
						return {};
					}

					uint8 nextIndex{0};
					switch (call.GetCallee()->GetType())
					{
						case AST::NodeType::VariableDeclaration:
							break;
						case AST::NodeType::Variable:
						{
							const AST::Expression::Variable& calleeVariable = static_cast<const AST::Expression::Variable&>(*call.GetCallee());
							if (calleeVariable.GetObject().IsValid())
							{
								ScriptValues newArguments = Visit(*calleeVariable.GetObject());
								for (const ScriptValue& value : newArguments)
								{
									LoadArgument(registers, nextIndex, value, SharedPtr<Environment>{m_pEnvironment});
								}
								nextIndex++;
							}
						}
						break;
						default:
							ExpectUnreachable();
					}

					for (const ReferenceWrapper<AST::Expression::Base>& pArgExpression : argslist)
					{
						ScriptValues newArguments = Visit(*pArgExpression);
						for (const ScriptValue& value : newArguments)
						{
							LoadArgument(registers, nextIndex, value, SharedPtr<Environment>{m_pEnvironment});
							nextIndex++;
						}
					}

					Scripting::VM::ReturnValue returnValue =
						functionData.m_function(registers[0], registers[1], registers[2], registers[3], registers[4], registers[5]);
					m_state.flags.Clear(Flags::Return);

					if (pFunctionDefinition->m_returnType.m_type.IsValid())
					{
						return ConvertResult(returnValue, pFunctionDefinition->m_returnType);
					}
					else
					{
						return {};
					}
				}
			}
		}
		Assert(false, "Trying to call non function");
		Error("Trying to call non function");
		return ScriptValues();
	}

	ScriptValues Interpreter::Visit(const AST::Expression::Function& function)
	{
		Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();

		ScriptFunction::ArgumentNames argumentNames;
		ScriptFunction::Arguments arguments;

		UniquePtr<UserScriptFunction> pUserScriptFunction = UniquePtr<UserScriptFunction>::Make(
			Guid::Generate(),
			UnicodeString{MAKE_UNICODE_LITERAL("Dynamic Function")},
			Reflection::ReturnType{},
			Move(argumentNames),
			Move(arguments),
			function,
			SharedPtr<Environment>{m_state.pEnvironment}
		);

		static constexpr auto nativeFunction = +[](
																							const Scripting::VM::Register R0,
																							const Scripting::VM::Register R1,
																							const Scripting::VM::Register R2,
																							const Scripting::VM::Register R3,
																							const Scripting::VM::Register R4,
																							const Scripting::VM::Register R5
																						) -> Scripting::VM::ReturnValue
		{
			Scripting::VM::Registers registers{R0, R1, R2, R3, R4, R5};
			Interpreter& interpreter = registers.ExtractArgument<0, Interpreter&>();
			const FunctionIdentifier functionIdentifier = registers.ExtractArgument<1, FunctionIdentifier>();
			ScriptValues arguments = registers.ExtractArgument<2, ScriptValues>();
			const UserScriptFunction& userScriptFunction =
				static_cast<const UserScriptFunction&>(*interpreter.m_pEnvironment->GetFunction(functionIdentifier));

			SharedPtr<Environment> pEnvironment = SharedPtr<Environment>::Make(userScriptFunction.m_pClosure);

			uint8 index = 0;
			for (const VariableToken& parameter : userScriptFunction.m_function.GetParameters())
			{
				if (index < arguments.GetSize())
				{
					pEnvironment->SetValue(parameter.identifier, static_cast<ScriptValue&&>(arguments[index++]));
				}
				else
				{
					// Nullptr indicates that a parameter is optional
					Assert(parameter.m_types.GetSize() == 1);
					if (parameter.m_types[0].IsOrSupports<nullptr_type>())
					{
						pEnvironment->SetValue(parameter.identifier, ScriptValue{nullptr});
					}
					else
					{
						interpreter.Error("Missing parameter value!");
						return Scripting::VM::ReturnValue{0, 0, 0, 0};
					}
				}
			}

			SharedPtr<Environment> pOldEnvironment = Move(interpreter.m_state.pEnvironment);
			interpreter.m_state.pEnvironment = Move(pEnvironment);
			interpreter.InterpretStatements(userScriptFunction.m_function.GetStatements());
			interpreter.m_state.pEnvironment = Move(pOldEnvironment);

			registers.PushArgument<0, ScriptValues>(Move(interpreter.m_state.returnValues));

			return Scripting::VM::ReturnValue{registers[0], registers[1], registers[2], registers[3]};
		};

		FunctionIdentifier functionIdentifier = reflectionRegistry.RegisterDynamicGlobalFunction(
			pUserScriptFunction->GetFunctionInfo(),
			Scripting::VM::DynamicFunction::Make<nativeFunction>()
		);

		m_state.pEnvironment->AddFunction(functionIdentifier, Move(pUserScriptFunction));
		return ScriptValue(functionIdentifier);
	}

	ScriptValues Interpreter::Visit(const AST::Expression::Group& group)
	{
		const AST::Expressions& expressions = group.GetExpressions();
		ScriptValues result;
		for (const ReferenceWrapper<const AST::Expression::Base>& pExpression : expressions)
		{
			ScriptValues values = Visit(*pExpression);
			result.MoveEmplaceRangeBack(values.GetView());
		}
		return result;
	}

	ScriptValues Interpreter::Visit(const AST::Expression::Literal& literal)
	{
		ScriptValues result;
		result.EmplaceBack(literal.GetValue());
		return result;
	}

	ScriptValues Interpreter::Visit(const AST::Expression::Logical& logical)
	{
		ScriptValue value = Visit(*logical.GetLeft())[0];
		switch (logical.GetOperator().type)
		{
			case TokenType::Or:
			{
				if (value.IsTruthy())
				{
					return value;
				}
				return Visit(*logical.GetRight());
			}
			case TokenType::And:
			{
				if (value.IsFalsey())
				{
					return value;
				}
				return Visit(*logical.GetRight());
			}
			default:
				ExpectUnreachable();
		}
	}

	ScriptValues Interpreter::Visit(const AST::Expression::Unary& unary)
	{
		ScriptValue scriptValue = Visit(*unary.GetRight())[0];
		switch (unary.GetOperator().type)
		{
			case TokenType::Minus:
				if (scriptValue.Get().Is<IntegerType>())
				{
					return ScriptValue(scriptValue.Get().GetExpected<IntegerType>() * IntegerType(-1));
				}
				if (scriptValue.Get().Is<FloatType>())
				{
					return ScriptValue(scriptValue.Get().GetExpected<FloatType>() * FloatType(-1.0));
				}
				return ScriptValues();
			case TokenType::Not:
				return ScriptValue(scriptValue.Not());
			case TokenType::Exclamation:
				return ScriptValue(!scriptValue);
			case TokenType::Tilde:
				return ScriptValue(~scriptValue);
			default:
				return ScriptValues();
		}
	}

	ScriptValues Interpreter::Visit(const AST::Expression::VariableDeclaration& variable)
	{
		ScriptValues result;
		auto distanceIt = m_pResolvedVariables->Find(uintptr(&variable));
		if (distanceIt != m_pResolvedVariables->end())
		{
			const int32 distance = distanceIt->second;
			result.EmplaceBack(m_state.pEnvironment->GetValueAt(distance, variable.GetIdentifier().identifier));
		}
		else
		{
			result.EmplaceBack(m_pGlobalEnvironment->GetValue(variable.GetIdentifier().identifier));
		}
		return result;
	}

	ScriptValues Interpreter::Visit(const AST::Expression::Variable& variable)
	{
		ScriptValues result;
		auto distanceIt = m_pResolvedVariables->Find(uintptr(&variable));
		if (distanceIt != m_pResolvedVariables->end())
		{
			const int32 distance = distanceIt->second;
			result.EmplaceBack(m_state.pEnvironment->GetValueAt(distance, variable.GetIdentifier().identifier));
		}
		else
		{
			result.EmplaceBack(m_pGlobalEnvironment->GetValue(variable.GetIdentifier().identifier));
		}
		return result;
	}

	void Interpreter::InterpretStatements(const AST::Statements& statements)
	{
		for (const ReferenceWrapper<const AST::Statement::Base>& pStatement : statements)
		{
			Visit(*pStatement);
			if (m_state.flags.AreAnySet(Flags::Break | Flags::Return))
			{
				break;
			}
		}
	}
}
