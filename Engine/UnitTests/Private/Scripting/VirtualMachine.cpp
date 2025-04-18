#include "gtest/gtest.h"

#include "Scripts.h"

#include <Common/Tests/UnitTest.h>

#include <Common/Memory/Containers/String.h>

#include <Engine/Scripting/Parser/Token.h>
#include <Engine/Scripting/Parser/Lexer.h>
#include <Engine/Scripting/Parser/Parser.h>
#include <Engine/Scripting/Parser/AST/Statement.h>
#include <Engine/Scripting/Parser/AST/Expression.h>
#include <Engine/Scripting/Compiler/Compiler.h>
#include <Engine/Scripting/VirtualMachine/VirtualMachine.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/DynamicDelegate.h>
#include <Engine/Asset/AssetManager.h>
#include <Engine/Tag/TagRegistry.h>
#include <Engine/DataSource/DataSourceCache.h>

#include <Common/Memory/New.h>
#include <Common/Memory/Variant.h>
#include <Common/Reflection/GenericType.h>
#include <Common/Reflection/Registry.h>
#include <Common/IO/Path.h>
#include <Common/IO/Log.h>

namespace ngine::Tests
{
	struct Systems
	{
		Systems()
		{
			System::Get<Log>().Open(IO::Path());

			System::Query::GetInstance().RegisterSystem(m_reflectionRegistry);
		}
		~Systems()
		{
			System::Get<Log>().Close();
			System::Query::GetInstance().DeregisterSystem<Reflection::Registry>();
			m_assetManager.DestroyElement();
		}
	protected:
		Reflection::Registry m_reflectionRegistry{Reflection::Registry::Initializer::Initialize};
		DataSource::Cache m_dataSourceCache;
		Tag::Registry m_tagRegistry;
		UniquePtr<Asset::Manager> m_assetManager{UniquePtr<Asset::Manager>::Make()};
	};

	[[nodiscard]] Optional<Scripting::AST::Graph> CreateVirtualMachineAST(const Scripting::StringType::ConstView expression)
	{
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

		return Move(astGraph);
	}

	[[nodiscard]] UniquePtr<Scripting::FunctionObject> Compile(const Scripting::StringType::ConstView expression)
	{
		const Optional<Scripting::AST::Graph> astGraph = CreateVirtualMachineAST(expression);
		Assert(astGraph.IsValid());
		if (astGraph.IsInvalid())
		{
			return nullptr;
		}

		const Scripting::AST::Node& entryPointNode = astGraph->GetNodes().GetLastElement();
		EXPECT_TRUE(entryPointNode.IsStatement());
		const Scripting::AST::Statement::Base& entryPointStatement = static_cast<const Scripting::AST::Statement::Base&>(entryPointNode);

		EXPECT_TRUE(entryPointStatement.GetType() == Scripting::AST::NodeType::Block);

		Scripting::Compiler compiler;
		UniquePtr<Scripting::FunctionObject> pFunction = compiler.Compile(entryPointStatement);
		Assert(pFunction.IsValid());
		if (pFunction.IsInvalid())
		{
			return nullptr;
		}

		if (!compiler.ResolveFunction(*pFunction))
		{
			return nullptr;
		}

		return pFunction;
	}

	[[nodiscard]] UniquePtr<Scripting::FunctionObject>
	CompileStandaloneFunction(Scripting::StringType::ConstView expression, const Scripting::StringType::ConstView functionName)
	{
		const Optional<Scripting::AST::Graph> astGraph = CreateVirtualMachineAST(expression);
		Assert(astGraph.IsValid());
		if (astGraph.IsInvalid())
		{
			return nullptr;
		}

		const Optional<const Scripting::AST::Expression::Function*> pFunctionExpression =
			astGraph->FindFunction(Scripting::Token::GuidFromScriptString(functionName));
		EXPECT_TRUE(pFunctionExpression.IsValid());

		Scripting::Compiler compiler;
		UniquePtr<Scripting::FunctionObject> pFunction = compiler.Compile(*pFunctionExpression);
		Assert(pFunction.IsValid());
		if (pFunction.IsInvalid())
		{
			return nullptr;
		}

		if (!compiler.ResolveFunction(*pFunction))
		{
			return nullptr;
		}

		return pFunction;
	}

	[[nodiscard]] bool Execute(Scripting::StringType::ConstView expression)
	{
		Systems systems;
		bool wasSuccessful;
		{
			UniquePtr<Scripting::FunctionObject> pScript = Compile(expression);

			UniquePtr<Scripting::VirtualMachine> pVm = UniquePtr<Scripting::VirtualMachine>::Make();
			pVm->Initialize(*pScript);
			wasSuccessful = pVm->Execute();
		}

		return wasSuccessful;
	}

	UNIT_TEST(Scripting, InvokeExecute)
	{
		Systems systems;

		constexpr Scripting::StringType::ConstView ScriptSourceInvoke = SCRIPT_STRING_LITERAL(R"(
			local start: integer = 0
			function counter(n: integer, l: integer)
				local i = n or 0
				j = l or 1
				start = start + 1
				return start + i, j + i
			end
		)");

		UniquePtr<Scripting::FunctionObject> pScript = Compile(ScriptSourceInvoke);

		UniquePtr<Scripting::VirtualMachine> pVm = UniquePtr<Scripting::VirtualMachine>::Make();
		pVm->Initialize(*pScript);
		pVm->Execute();

		const Scripting::VirtualMachine::GlobalMapType& globals = pVm->GetGlobals();
		const auto functionIt = globals.Find(Scripting::Token::GuidFromScriptString("counter"));
		EXPECT_TRUE(functionIt != globals.end());
		if (functionIt != globals.end())
		{
			Scripting::ClosureObject* pFunction = Scripting::AsClosureObject(functionIt->second.GetObject());
			EXPECT_TRUE(pFunction != nullptr);
			if (pFunction)
			{
				Array<Scripting::RawValue, 2> results;
				pVm->Invoke(
					*pFunction,
					Array<Scripting::RawValue, 2>{Scripting::RawValue{Scripting::IntegerType(0)}, Scripting::RawValue{Scripting::IntegerType(1)}},
					results.GetView()
				);
				EXPECT_TRUE(results[0].GetInteger() == 1 && results[1].GetInteger() == 1);
				pVm->Invoke(*pFunction, Array<Scripting::RawValue, 1>{Scripting::RawValue{Scripting::IntegerType(0)}}, results.GetView());
				EXPECT_TRUE(results[0].GetInteger() == 2);
				pVm->Invoke(*pFunction, Array<Scripting::RawValue, 1>{Scripting::RawValue{Scripting::IntegerType(39)}}, results.GetView());
				EXPECT_TRUE(results[0].GetInteger() == 42);
				pVm->Invoke(*pFunction, {}, results.GetView());
				EXPECT_TRUE(results[0].GetInteger() == 4 && results[1].GetInteger() == 1);
			}
		}
	}

	UNIT_TEST(Scripting, StandaloneInvokeExecuteArgs)
	{
		Systems systems;

		{
			constexpr Scripting::StringType::ConstView ScriptSourceInvokeArgs = SCRIPT_STRING_LITERAL(R"(
                function foo(a: integer, b: integer): integer
                    assert(a == 1337);
                    assert(b == 9001);
                    return a + b
                end
            )");

			UniquePtr<Scripting::FunctionObject> pScript = CompileStandaloneFunction(ScriptSourceInvokeArgs, "foo");

			UniquePtr<Scripting::VirtualMachine> pVm = UniquePtr<Scripting::VirtualMachine>::Make();
			pVm->Initialize(*pScript);

			Array<Scripting::RawValue, 1> results;
			EXPECT_TRUE(pVm->Execute(
				Array<Scripting::RawValue, 2>{Scripting::RawValue{Scripting::IntegerType(1337)}, Scripting::RawValue{Scripting::IntegerType(9001)}},
				results
			));
			EXPECT_EQ(results[0].GetInteger(), 1337 + 9001);
		}
	}

	UNIT_TEST(Scripting, FibonacciResetExecute)
	{
		Systems systems;

		{
			UniquePtr<Scripting::FunctionObject> pScript = Compile(ScriptSourceFibonacci);

			UniquePtr<Scripting::VirtualMachine> pVm = UniquePtr<Scripting::VirtualMachine>::Make();
			pVm->Initialize(*pScript);
			EXPECT_TRUE(pVm->Execute());
			pVm->Reset();
			EXPECT_TRUE(pVm->Execute());
		}
	}

	UNIT_TEST(Scripting, FibonacciExecute)
	{
		EXPECT_TRUE(Execute(ScriptSourceFibonacci));
	}

	UNIT_TEST(Scripting, AssignmentExecute)
	{
		EXPECT_TRUE(Execute(ScriptSourceAssignment));
	}

	UNIT_TEST(Scripting, OrderOperatorsExecute)
	{
		EXPECT_TRUE(Execute(ScriptSourceOrderOperator));
	}

	UNIT_TEST(Scripting, LogicalOperatorsExecute)
	{
		EXPECT_TRUE(Execute(ScriptSourceLogicalOperators));
	}

	UNIT_TEST(Scripting, LocalVariablesExecute)
	{
		EXPECT_TRUE(Execute(ScriptSourceLocalVariables));
	}

	UNIT_TEST(Scripting, LocalFunctionsExecute)
	{
		EXPECT_TRUE(Execute(ScriptSourceLocalFunctions));
	}

	UNIT_TEST(Scripting, ScriptRepeatUntilExecute)
	{
		EXPECT_TRUE(Execute(ScriptSourceRepeatUntil));
	}

	UNIT_TEST(Scripting, ScriptBreakExecute)
	{
		EXPECT_TRUE(Execute(ScriptSourceBreak));
	}

	UNIT_TEST(Scripting, ScriptObjectFunctions)
	{
		EXPECT_TRUE(Execute(ScriptSourceObjectFunctions));
	}

	UNIT_TEST(Scripting, ScriptBraceInitialization)
	{
		EXPECT_TRUE(Execute(ScriptSourceBraceInitialization));
	}

	UNIT_TEST(Scripting, ScriptMath)
	{
		EXPECT_TRUE(Execute(ScriptSourceMath));
	}

	UNIT_TEST(Scripting, StandaloneDelegateExecuteArgs)
	{
		Systems systems;

		{
			constexpr Scripting::StringType::ConstView ScriptSourceInvokeArgs = SCRIPT_STRING_LITERAL(R"(
                function foo(a: integer, b: integer): integer
                    assert(a == 1337);
                    assert(b == 9001);
                    return a + b
                end
            )");

			UniquePtr<Scripting::FunctionObject> pScript = CompileStandaloneFunction(ScriptSourceInvokeArgs, "foo");

			Scripting::VM::NativeDelegate<Scripting::IntegerType(Scripting::IntegerType, Scripting::IntegerType)> delegate =
				pScript->CreateDelegate<Scripting::IntegerType, Scripting::IntegerType, Scripting::IntegerType>();

			const Scripting::IntegerType returnValue = delegate(1337, 9001);
			EXPECT_EQ(returnValue, 1337 + 9001);
		}
	}
}
