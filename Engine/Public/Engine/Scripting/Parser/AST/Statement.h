#pragma once

#include "Engine/Scripting/Parser/Token.h"
#include "Engine/Scripting/Parser/AST/Expression.h"

#include <Common/Memory/Optional.h>
#include <Common/Memory/Any.h>
#include <Common/Platform/LifetimeBound.h>

namespace ngine::Scripting::AST::Statement
{
	struct Expression final : public Statement::Base
	{
		Expression()
			: Statement::Base(NodeType::Expression)
		{
		}
		Expression(Optional<AST::Expression::Base*> pExpression)
			: Statement::Base(NodeType::Expression)
			, m_pExpression(*pExpression)
		{
		}
		Expression(const Expression& other) = delete;
		Expression& operator=(const Expression& other) = delete;
		[[nodiscard]] Optional<const AST::Expression::Base*> GetExpression() const
		{
			return m_pExpression.Get();
		}
		[[nodiscard]] bool operator==(const Expression& other) const
		{
			return *m_pExpression == *other.m_pExpression;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Optional<AST::Expression::Base*> m_pExpression;
	};

	struct Block final : public Statement::Base
	{
		Block()
			: Statement::Base(NodeType::Block)
		{
		}
		Block(Statements&& statements)
			: Statement::Base(NodeType::Block)
			, m_statements(Move(statements))
		{
		}
		Block(const Block& other) = delete;
		Block& operator=(const Block& other) = delete;
		[[nodiscard]] const Statements& GetStatements() const
		{
			return m_statements;
		}
		[[nodiscard]] bool operator==(const Block& other) const
		{
			return m_statements == other.m_statements;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Statements m_statements;
	};

	struct If final : public Statement::Base
	{
		using Coordinates = Vector<Math::Vector2i>;

		If()
			: Statement::Base(NodeType::If)
		{
		}
		If(Expressions&& conditions, Statements&& thens, Optional<AST::Statement::Base*> pElse, Coordinates&& coordinates = {})
			: Statement::Base(NodeType::If)
			, m_conditions(Move(conditions))
			, m_thens(Move(thens))
			, m_coordinates(Forward<Coordinates>(coordinates))
			, m_pElse(Move(pElse))
		{
		}
		If(const If& other) = delete;
		If& operator=(const If& other) = delete;
		[[nodiscard]] const Expressions& GetConditions() const LIFETIME_BOUND
		{
			return m_conditions;
		}
		[[nodiscard]] Expressions& GetConditions() LIFETIME_BOUND
		{
			return m_conditions;
		}
		[[nodiscard]] const Statements& GetThens() const LIFETIME_BOUND
		{
			return m_thens;
		}
		[[nodiscard]] Statements& GetThens() LIFETIME_BOUND
		{
			return m_thens;
		}
		[[nodiscard]] Optional<const AST::Statement::Base*> GetElse() const LIFETIME_BOUND
		{
			return m_pElse.Get();
		}
		void SetElse(const Optional<AST::Statement::Base*> pElse)
		{
			m_pElse = pElse;
		}
		[[nodiscard]] ArrayView<const Math::Vector2i> GetCoordinates() const LIFETIME_BOUND
		{
			return m_coordinates;
		}
		[[nodiscard]] Coordinates& GetCoordinates() LIFETIME_BOUND
		{
			return m_coordinates;
		}
		[[nodiscard]] bool operator==(const If& other) const
		{
			return m_conditions == other.m_conditions && m_thens == other.m_thens &&
			       ((m_pElse.IsInvalid() && other.m_pElse.IsInvalid()) || (*m_pElse == *other.m_pElse));
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Expressions m_conditions;
		Statements m_thens;
		Coordinates m_coordinates;
		Optional<AST::Statement::Base*> m_pElse;
	};

	struct While final : public Statement::Base
	{
		While()
			: Statement::Base(NodeType::While)
		{
		}
		While(Optional<AST::Expression::Base*> pCondition, Optional<AST::Statement::Base*> pBody)
			: Statement::Base(NodeType::While)
			, m_pCondition(Move(pCondition))
			, m_pBody(Move(pBody))
		{
		}
		While(const While& other) = delete;
		While& operator=(const While& other) = delete;
		[[nodiscard]] Optional<const AST::Expression::Base*> GetCondition() const LIFETIME_BOUND
		{
			return m_pCondition.Get();
		}
		void SetCondition(const Optional<AST::Expression::Base*> pCondition)
		{
			m_pCondition = pCondition;
		}
		[[nodiscard]] Optional<const AST::Statement::Base*> GetBody() const LIFETIME_BOUND
		{
			return m_pBody.Get();
		}
		void SetBody(const Optional<AST::Statement::Base*> pBody)
		{
			m_pBody = pBody;
		}
		[[nodiscard]] bool operator==(const While& other) const
		{
			return *m_pCondition == *other.m_pCondition && *m_pBody == *other.m_pBody;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Optional<AST::Expression::Base*> m_pCondition;
		Optional<AST::Statement::Base*> m_pBody;
	};

	struct Repeat final : public Statement::Base
	{
		Repeat()
			: Statement::Base(NodeType::Repeat)
		{
		}
		Repeat(Statements&& statements, Optional<AST::Expression::Base*> pCondition)
			: Statement::Base(NodeType::Repeat)
			, m_statements(Move(statements))
			, m_pCondition(Move(pCondition))
		{
		}
		Repeat(const Repeat& other) = delete;
		Repeat& operator=(const Repeat& other) = delete;
		[[nodiscard]] const Statements& GetStatements() const LIFETIME_BOUND
		{
			return m_statements;
		}
		[[nodiscard]] Statements& GetStatements() LIFETIME_BOUND
		{
			return m_statements;
		}
		[[nodiscard]] Optional<const AST::Expression::Base*> GetCondition() const LIFETIME_BOUND
		{
			return m_pCondition.Get();
		}
		void SetCondition(const Optional<AST::Expression::Base*> pCondition)
		{
			m_pCondition = pCondition;
		}
		[[nodiscard]] bool operator==(const Repeat& other) const
		{
			return m_statements == other.m_statements && *m_pCondition == *other.m_pCondition;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Statements m_statements;
		Optional<AST::Expression::Base*> m_pCondition;
	};

	struct Break final : public Statement::Base
	{
		Break()
			: Statement::Base(NodeType::Break)
		{
		}
		[[nodiscard]] bool operator==(const Break&) const
		{
			return true;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	};

	struct Return final : public Statement::Base
	{
		Return()
			: Statement::Base(NodeType::Return)
		{
		}
		Return(AST::Expressions&& exprlist, const SourceLocation sourceLocation)
			: Statement::Base(NodeType::Return)
			, m_exprlist(Move(exprlist))
			, m_sourceLocation(sourceLocation)
		{
		}
		Return(const Return& other) = delete;
		Return& operator=(const Return& other) = delete;
		[[nodiscard]] SourceLocation GetSourceLocation() const
		{
			return m_sourceLocation;
		}
		[[nodiscard]] const AST::Expressions& GetExpressions() const LIFETIME_BOUND
		{
			return m_exprlist;
		}
		[[nodiscard]] AST::Expressions& GetExpressions() LIFETIME_BOUND
		{
			return m_exprlist;
		}
		[[nodiscard]] bool operator==(const Return& other) const
		{
			return m_exprlist == other.m_exprlist;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		AST::Expressions m_exprlist;
		SourceLocation m_sourceLocation;
	};
}
