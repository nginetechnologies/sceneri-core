#include "gtest/gtest.h"

#include "Scripts.h"

#include <Common/Tests/UnitTest.h>

#include <Common/Memory/Containers/String.h>

#include <Engine/Scripting/Parser/Token.h>
#include <Engine/Scripting/Parser/Lexer.h>
#include <Engine/Scripting/Parser/Parser.h>
#include <Engine/Scripting/Parser/AST/Statement.h>
#include <Engine/Scripting/Parser/AST/Expression.h>
#include <Engine/Scripting/Interpreter/Resolver.h>
#include <Engine/Scripting/Interpreter/Interpreter.h>
#include <Engine/Scripting/Interpreter/Environment.h>
#include <Engine/Scripting/Interpreter/ScriptFunctionCache.h>

#include <Common/Serialization/Serialize.h>
#include <Common/Serialization/Deserialize.h>

#include <Common/Memory/New.h>
#include <Common/Memory/Variant.h>
#include <Common/Reflection/GenericType.h>
#include <Common/Reflection/Registry.h>
#include <Common/System/Query.h>
#include <Common/IO/Path.h>
#include <Common/IO/Log.h>

namespace ngine::Tests
{
	[[nodiscard]] Optional<Scripting::AST::Graph> CreateInterpreterAST(const Scripting::StringType::ConstView expression)
	{
		System::Get<Log>().Open(IO::Path());

		Scripting::Lexer lexer;
		Scripting::TokenListType tokens;
		if (!lexer.ScanTokens(expression, tokens))
		{
			return {};
		}

		Scripting::Parser parser;
		Optional<Scripting::AST::Graph> astGraph = parser.Parse(tokens);
		if (astGraph.IsInvalid())
		{
			return {};
		}

		System::Get<Log>().Close();

		return Move(astGraph);
	}

	[[nodiscard]] bool Interpret(const Scripting::StringType::ConstView expression)
	{
		static Reflection::Registry reflectionRegistry{Reflection::Registry::Initializer::Initialize};
		System::Query::GetInstance().RegisterSystem(reflectionRegistry);

		const Optional<Scripting::AST::Graph> parsedAstGraph = CreateInterpreterAST(expression);
		Serialization::Data serializedData(rapidjson::kObjectType, Serialization::ContextFlags::ToBuffer);
		[[maybe_unused]] const bool wasSerialized = Serialization::Serialize(serializedData, parsedAstGraph);
		EXPECT_TRUE(wasSerialized);

		Scripting::AST::Graph deserializedGraph;
		[[maybe_unused]] const bool wasRead = deserializedGraph.Serialize(Serialization::Reader(serializedData));
		Assert(wasRead);
		EXPECT_TRUE(deserializedGraph.IsValid());
		EXPECT_EQ(parsedAstGraph->GetNodes(), deserializedGraph.GetNodes());

		const Scripting::AST::Node& entryPointNode = deserializedGraph.GetNodes().GetLastElement();
		EXPECT_TRUE(entryPointNode.IsStatement());
		const Scripting::AST::Statement::Base& entryPointStatement = static_cast<const Scripting::AST::Statement::Base&>(entryPointNode);

		Scripting::Resolver resolver;
		SharedPtr<Scripting::ResolvedVariableMap> pVariables = resolver.Resolve(entryPointStatement);

		UniquePtr<Scripting::ScriptTableCache> pScriptTableCache = UniquePtr<Scripting::ScriptTableCache>::Make();
		UniquePtr<Scripting::ScriptFunctionCache> pScriptFunctionCache = UniquePtr<Scripting::ScriptFunctionCache>::Make();
		SharedPtr<Scripting::Environment> pEnvironment = Scripting::Environment::Create(*pScriptFunctionCache, *pScriptTableCache);
		Scripting::Interpreter interpreter(Move(pEnvironment));
		const bool result = interpreter.Interpret(entryPointStatement, Move(pVariables));

		System::Query::GetInstance().DeregisterSystem<Reflection::Registry>();
		return result;
	}

	UNIT_TEST(Scripting, StandaloneInvokeInterpretArgs)
	{
		constexpr Scripting::StringType::ConstView ScriptSourceInvokeArgs = SCRIPT_STRING_LITERAL(R"(
			function foo(a: integer, b: integer): integer
				assert(a == 1337);
                assert(b == 9001);
				return a + b
			end
		)");

		static Reflection::Registry reflectionRegistry{Reflection::Registry::Initializer::Initialize};
		System::Query::GetInstance().RegisterSystem(reflectionRegistry);

		const Optional<Scripting::AST::Graph> parsedAstGraph = CreateInterpreterAST(ScriptSourceInvokeArgs);

		const Optional<const Scripting::AST::Expression::Function*> pFunctionExpression =
			parsedAstGraph->FindFunction(Scripting::Token::GuidFromScriptString("foo"));
		EXPECT_TRUE(pFunctionExpression.IsValid());

		System::Get<Log>().Open(IO::Path());

		Scripting::Resolver resolver;
		SharedPtr<Scripting::ResolvedVariableMap> pVariables = resolver.Resolve(*pFunctionExpression);

		UniquePtr<Scripting::ScriptTableCache> pScriptTableCache = UniquePtr<Scripting::ScriptTableCache>::Make();
		UniquePtr<Scripting::ScriptFunctionCache> pScriptFunctionCache = UniquePtr<Scripting::ScriptFunctionCache>::Make();
		SharedPtr<Scripting::Environment> pEnvironment = Scripting::Environment::Create(*pScriptFunctionCache, *pScriptTableCache);
		Scripting::Interpreter interpreter(Move(pEnvironment));

		Scripting::ScriptValues arguments;
		arguments.EmplaceBack(Scripting::ScriptValue(Scripting::IntegerType{1337}));
		arguments.EmplaceBack(Scripting::ScriptValue(Scripting::IntegerType{9001}));
		const Scripting::ScriptValues results = interpreter.Interpret(*pFunctionExpression, Move(arguments), Move(pVariables));
		EXPECT_EQ(results.GetSize(), 1);
		EXPECT_TRUE(results[0].Get().Is<Scripting::IntegerType>());
		const Scripting::IntegerType& resultInteger = results[0].Get().GetExpected<Scripting::IntegerType>();
		EXPECT_EQ(resultInteger, 1337 + 9001);

		System::Get<Log>().Close();
		System::Query::GetInstance().DeregisterSystem<Reflection::Registry>();
	}

	UNIT_TEST(Scripting, FibonacciInterpret)
	{
		EXPECT_TRUE(Interpret(ScriptSourceFibonacci));
	}

	UNIT_TEST(Scripting, AssignmentInterpret)
	{
		EXPECT_TRUE(Interpret(ScriptSourceAssignment));
	}

	UNIT_TEST(Scripting, OrderOperatorsInterpret)
	{
		EXPECT_TRUE(Interpret(ScriptSourceOrderOperator));
	}

	UNIT_TEST(Scripting, LogicalOperatorsInterpret)
	{
		EXPECT_TRUE(Interpret(ScriptSourceLogicalOperators));
	}

	UNIT_TEST(Scripting, LocalVariablesInterpret)
	{
		EXPECT_TRUE(Interpret(ScriptSourceLocalVariables));
	}

	UNIT_TEST(Scripting, LocalFunctionsInterpret)
	{
		EXPECT_TRUE(Interpret(ScriptSourceLocalFunctions));
	}

	UNIT_TEST(Scripting, ScriptRepeatUntilInterpret)
	{
		EXPECT_TRUE(Interpret(ScriptSourceRepeatUntil));
	}

	UNIT_TEST(Scripting, ScriptBreakInterpret)
	{
		EXPECT_TRUE(Interpret(ScriptSourceBreak));
	}

	UNIT_TEST(Scripting, ScriptObjectFunctionsInterpret)
	{
		EXPECT_TRUE(Interpret(ScriptSourceObjectFunctions));
	}

	/*UNIT_TEST(Scripting, ScriptBraceInitializationInterpreter)
	{
	  EXPECT_TRUE(Interpret(ScriptSourceBraceInitialization));
	}*/
}
