#pragma once

#include "Engine/Scripting/Interpreter/Resolver.h"
#include "Engine/Scripting/Interpreter/Environment.h"
#include "Engine/Scripting/Interpreter/ScriptTableCache.h"

#include "Engine/Scripting/Parser/ScriptTable.h"
#include "Engine/Scripting/Parser/ScriptValue.h"
#include "Engine/Scripting/Parser/AST/Expression.h"

#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>

#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/ForwardDeclarations/AnyView.h>
#include <Common/Memory/ForwardDeclarations/UniquePtr.h>

#include <Common/TypeTraits/IsSame.h>
#include <Common/Math/WorldCoordinate.h>

namespace ngine::Scripting
{
	struct UserScriptFunction;
	struct Environment;
}

namespace ngine::Reflection
{
	struct FunctionData;
}

namespace ngine::Scripting
{
	struct Interpreter final : public AST::NodeVisitor<Interpreter, ScriptValues>
	{
	public:
		enum class Flags : uint8
		{
			Error = 1 << 0,
			Break = 1 << 1,
			Return = 1 << 2
		};
	public:
		Interpreter(SharedPtr<Environment> pEnvironment);
		bool Interpret(const AST::Statement::Base& statement, SharedPtr<ResolvedVariableMap> pVariables = {});
		ScriptValues
		Interpret(const AST::Expression::Function& function, ScriptValues&& arguments, SharedPtr<ResolvedVariableMap> pVariables = {});
		void Error(StringType::ConstView error);

		using NodeVisitor::Visit;
		ScriptValues Visit(const AST::Expression::Binary&);
		ScriptValues Visit(const AST::Expression::Unary&);
		ScriptValues Visit(const AST::Expression::Group&);
		ScriptValues Visit(const AST::Expression::Literal&);
		ScriptValues Visit(const AST::Expression::VariableDeclaration&);
		ScriptValues Visit(const AST::Expression::Variable&);
		ScriptValues Visit(const AST::Expression::Assignment&);
		ScriptValues Visit(const AST::Expression::Call&);
		ScriptValues Visit(const AST::Expression::Function&);
		ScriptValues Visit(const AST::Statement::Block&);
		ScriptValues Visit(const AST::Statement::Break&);
		ScriptValues Visit(const AST::Statement::Expression&);
		ScriptValues Visit(const AST::Statement::If&);
		ScriptValues Visit(const AST::Statement::Repeat&);
		ScriptValues Visit(const AST::Statement::Return&);
		ScriptValues Visit(const AST::Statement::While&);
		ScriptValues Visit(const AST::Expression::Logical&);
	private:
		void InterpretStatements(const AST::Statements& statements);
	private:
		friend UserScriptFunction;
		friend Environment;

		SharedPtr<Environment> m_pEnvironment;
		SharedPtr<Environment> m_pGlobalEnvironment;
		SharedPtr<ResolvedVariableMap> m_pResolvedVariables;
		struct State
		{
			SharedPtr<Environment> pEnvironment;
			ScriptValues returnValues;
			EnumFlags<Flags> flags;
		};
		State m_state;
	};

	ENUM_FLAG_OPERATORS(Interpreter::Flags);
}
