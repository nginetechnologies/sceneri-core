#pragma once

#include "Engine/Scripting/Parser/Lexer.h"
#include "Engine/Scripting/Parser/AST/Expression.h"
#include "Engine/Scripting/Parser/AST/Graph.h"

#include <Common/IO/PathView.h>
#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Reflection/TypeDefinition.h>

namespace ngine::Scripting
{
	namespace AST::Statement
	{
		struct Block;
		struct Return;
		struct Break;
		struct While;
		struct Repeat;
		struct If;
	}

	namespace AST::Expression
	{
		struct Function;
	}

	// A simple RD (Recursive Descent) parser
	// for https://www.lua.org/manual/5.4/manual.html#9
	// with eliminated left-recursions and optimisations
	class Parser
	{
	public:
		static constexpr uint32 Version = 1;
		enum class Flags : uint8
		{
			Error = 1 << 0,
			Synchronize = 1 << 1
		};
	public:
		[[nodiscard]] Optional<AST::Graph> Parse(const TokenListType& tokens);
	protected:
		Optional<AST::Statement::Block*> StatementChunk();
		Optional<AST::Statement::Block*> StatementBlock();
		Optional<AST::Statement::Return*> StatementReturn();
		Optional<AST::Statement::Base*> Statement();
		Optional<AST::Statement::Break*> StatementBreak();
		Optional<AST::Statement::Base*> StatementDo();
		Optional<AST::Statement::While*> StatementWhile();
		Optional<AST::Statement::Repeat*> StatementRepeat();
		Optional<AST::Statement::If*> StatementIf();

		Optional<AST::Expression::Base*> Prefix();
		Optional<AST::Expression::Base*>
		Expression(const Optional<const AST::Expression::Base*> pVariableExpression = Invalid, uint8 precedence = 0);
		[[nodiscard]] Optional<AST::Expression::Base*> BraceInitializer(const TokenType variableType);
		[[nodiscard]] Optional<AST::Expression::Base*> BraceInitializer(const AST::Expression::Base& variableExpression);
		[[nodiscard]] AST::Expression::Function FunctionBody();

		void ParseBlock(AST::Statements& stmtlist);
		void ParseExprlist(AST::Expressions& exprlist);
		void ParseExprlist(AST::Expressions& exprlist, const ArrayView<ReferenceWrapper<AST::Expression::Base>> varlist);
		void ParseArgslist(AST::Expressions& argslist);
		void ParseNamelist(AST::Expression::Variable::Tokens& namelist, Optional<Token>&& pName = {});
		void ParseReturnTypes(AST::Expression::Variable::Tokens& returnTypes);
		[[nodiscard]] Scripting::Type ParseVariableType();

		void Advance();
		void Reverse();
		bool Match(TokenType type);
		bool Match(ArrayView<TokenType, uint8> types);
		[[nodiscard]] bool Check(TokenType type) const;
		[[nodiscard]] bool Check(ArrayView<TokenType, uint8> types) const;
		void Synchronize();
	private:
		[[nodiscard]] bool ReachedEnd() const;
		[[nodiscard]] const Token& Peek() const;
		[[nodiscard]] const Token& PeekNext() const;
		[[nodiscard]] const Token& Previous() const;
		void Error(StringType::ConstView error);

		[[nodiscard]] Types GetPossibleExpressionReturnTypes(const AST::Expression::Base& expression) const;

		struct DeclareVariableResult
		{
			const Types& types;
			bool wasDeclared;
		};

		DeclareVariableResult EmplaceGlobalVariableType(const Guid identifier, Types&& types);
		void UpdateGlobalVariableType(const Guid identifier, Types&& types);
		const Types& DeclareLocalVariableType(const Guid identifier, Types&& types);
		DeclareVariableResult EmplaceLocalVariableType(const Guid identifier, Types&& types);
		void UpdateLocalVariableType(const Guid identifier, Types&& types);
		void UpdateVariableType(const Guid identifier, Types&& types);
	private:
		AST::Graph m_graph;

		struct State;

		struct StateView
		{
			[[nodiscard]] bool ReachedEnd() const
			{
				return current == end;
			}
			[[nodiscard]] const Token& Peek() const;
			[[nodiscard]] const Token& PeekNext() const;
			[[nodiscard]] const Token& Previous() const;

			void Advance();
			void Reverse();
			bool Match(TokenType type);
			bool Match(ArrayView<TokenType, uint8> types);
			[[nodiscard]] bool Check(TokenType type) const;
			[[nodiscard]] bool Check(ArrayView<TokenType, uint8> types) const;

			Optional<State*> pEnclosingState;
			TokenListType::const_iterator begin{nullptr};
			TokenListType::const_iterator end{nullptr};
			TokenListType::const_iterator current{nullptr};
			uint32 loopDepth{0};
			EnumFlags<Flags> flags;
		};

		struct State : public StateView
		{
			State() = default;
			State& operator=(const StateView state)
			{
				StateView::operator=(state);
				return *this;
			}

			[[nodiscard]] Optional<const Types*> FindVariableTypes(const Guid variableIdentifier) const;

			UnorderedMap<Guid, Types, Guid::Hash> m_localVariableTypes;
			UnorderedMap<Guid, Types, Guid::Hash> m_functionReturnTypes;
		};
		State m_state;
		UnorderedMap<Guid, Types, Guid::Hash> m_globalVariableTypes;
	};

	ENUM_FLAG_OPERATORS(Parser::Flags);
}
