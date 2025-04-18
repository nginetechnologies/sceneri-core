#pragma once

#include "Engine/Scripting/Parser/AST/Node.h"
#include "Engine/Scripting/Parser/Token.h"
#include "Engine/Scripting/Parser/ScriptValue.h"

#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Memory/Optional.h>
#include <Common/Memory/Variant.h>
#include <Common/Memory/Any.h>
#include <Common/Platform/LifetimeBound.h>

namespace ngine::Scripting::AST
{
	struct Graph;

	namespace Statement
	{
		struct Base : public Node
		{
			using Node::Node;
		};
	}

	namespace Expression
	{
		struct Base : public Node
		{
			using Node::Node;
		};
	}

	struct Statements
	{
		using ContainerType = Vector<ReferenceWrapper<Statement::Base>>;

		[[nodiscard]] bool operator==(const Statements& other) const
		{
			return m_statements.GetView() == other.m_statements.GetView();
		}
		[[nodiscard]] bool operator!=(const Statements& other) const
		{
			return !operator==(other);
		}

		bool Serialize(const Serialization::Reader reader, Graph& graph);
		bool Serialize(Serialization::Writer writer, const Graph& graph) const;

		[[nodiscard]] ArrayView<const ReferenceWrapper<const Statement::Base>> GetView() const
		{
			const ArrayView<const ReferenceWrapper<Statement::Base>> statements{m_statements.GetView()};
			return {reinterpret_cast<const ReferenceWrapper<const Statement::Base>*>(statements.GetData()), statements.GetSize()};
		}
		[[nodiscard]] ArrayView<ReferenceWrapper<Statement::Base>> GetView()
		{
			return m_statements.GetView();
		}
		[[nodiscard]] ContainerType::ConstIteratorType begin() const
		{
			return m_statements.begin();
		}
		[[nodiscard]] ContainerType::ConstIteratorType end() const
		{
			return m_statements.end();
		}

		[[nodiscard]] bool HasElements() const
		{
			return m_statements.HasElements();
		}
		[[nodiscard]] bool IsEmpty() const
		{
			return m_statements.IsEmpty();
		}
		[[nodiscard]] uint32 GetSize() const
		{
			return m_statements.GetSize();
		}
		[[nodiscard]] ReferenceWrapper<const Statement::Base> GetLastElement() const
		{
			return GetView().GetLastElement();
		}
		void Emplace(ContainerType::ConstPointerType where, ReferenceWrapper<Statement::Base>&& expression)
		{
			Assert(where <= m_statements.end());
			m_statements.Emplace(where, Memory::Uninitialized, Forward<ReferenceWrapper<Statement::Base>>(expression));
		}
		void EmplaceBack(ReferenceWrapper<Statement::Base>&& expression)
		{
			m_statements.EmplaceBack(Forward<ReferenceWrapper<Statement::Base>>(expression));
		}
	protected:
		ContainerType m_statements;
	};

	struct Expressions
	{
		using ContainerType = Vector<ReferenceWrapper<Expression::Base>>;

		[[nodiscard]] bool operator==(const Expressions& other) const
		{
			return m_expressions.GetView() == other.m_expressions.GetView();
		}
		[[nodiscard]] bool operator!=(const Expressions& other) const
		{
			return !operator==(other);
		}

		bool Serialize(const Serialization::Reader reader, Graph& graph);
		bool Serialize(Serialization::Writer writer, const Graph& graph) const;

		[[nodiscard]] ArrayView<const ReferenceWrapper<const Expression::Base>> GetView() const
		{
			const ArrayView<const ReferenceWrapper<Expression::Base>> expressions{m_expressions.GetView()};
			return {reinterpret_cast<const ReferenceWrapper<const Expression::Base>*>(expressions.GetData()), expressions.GetSize()};
		}
		[[nodiscard]] ArrayView<ReferenceWrapper<Expression::Base>> GetView()
		{
			return m_expressions.GetView();
		}

		[[nodiscard]] ContainerType::ConstIteratorType begin() const
		{
			return m_expressions.begin();
		}
		[[nodiscard]] ContainerType::ConstIteratorType end() const
		{
			return m_expressions.end();
		}

		[[nodiscard]] bool HasElements() const
		{
			return m_expressions.HasElements();
		}
		[[nodiscard]] bool IsEmpty() const
		{
			return m_expressions.IsEmpty();
		}
		[[nodiscard]] uint32 GetSize() const
		{
			return m_expressions.GetSize();
		}
		void EmplaceBack(ReferenceWrapper<Expression::Base>&& expression)
		{
			m_expressions.EmplaceBack(Forward<ReferenceWrapper<Expression::Base>>(expression));
		}
	protected:
		ContainerType m_expressions;
	};
}

namespace ngine::Scripting
{
	enum class PrimitiveType
	{
		Any,
		Integer,
		Integer2,
		Integer3,
		Integer4,
		Float,
		Float2,
		Float3,
		Float4,
		Boolean,
		Boolean2,
		Boolean3,
		Boolean4,
		String,
		Null
	};
	[[nodiscard]] PrimitiveType GetPrimitiveType(const Scripting::Type& type);
}

namespace ngine::Scripting::AST::Expression
{
	struct Binary final : public Expression::Base
	{
		Binary()
			: Expression::Base(NodeType::Binary)
		{
		}
		Binary(Optional<Expression::Base*> pLeft, Token&& token, const PrimitiveType primitiveType, Optional<Expression::Base*> pRight)
			: Expression::Base(NodeType::Binary)
			, m_pLeft(Move(pLeft))
			, m_operator(Move(token))
			, m_primitiveType(primitiveType)
			, m_pRight(Move(pRight))
		{
		}
		Binary(const Binary& other) = delete;
		Binary& operator=(const Binary& other) = delete;
		[[nodiscard]] Optional<const Expression::Base*> GetLeft() const LIFETIME_BOUND
		{
			return m_pLeft.Get();
		}
		void SetLeft(const Optional<Expression::Base*> pLeft)
		{
			m_pLeft = pLeft;
		}
		[[nodiscard]] const Token& GetOperator() const LIFETIME_BOUND
		{
			return m_operator;
		}
		[[nodiscard]] SourceLocation GetSourceLocation() const
		{
			return m_operator.sourceLocation;
		}
		[[nodiscard]] PrimitiveType GetPrimitiveType() const
		{
			return m_primitiveType;
		}
		[[nodiscard]] Optional<const Expression::Base*> GetRight() const LIFETIME_BOUND
		{
			return m_pRight.Get();
		}
		void SetRight(const Optional<Expression::Base*> pRight)
		{
			m_pRight = pRight;
		}
		[[nodiscard]] bool operator==(const Binary& other) const
		{
			return *m_pLeft == *other.m_pLeft && m_operator.type == other.m_operator.type && *m_pRight == *other.m_pRight;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Optional<Expression::Base*> m_pLeft;
		Token m_operator;
		PrimitiveType m_primitiveType;
		Optional<Expression::Base*> m_pRight;
	};

	struct Unary final : public Expression::Base
	{
		Unary()
			: Expression::Base(NodeType::Unary)
		{
		}
		Unary(Token&& token, const PrimitiveType primitiveType, Optional<Expression::Base*> pRight)
			: Expression::Base(NodeType::Unary)
			, m_operator(Move(token))
			, m_primitiveType(primitiveType)
			, m_pRight(Move(pRight))
		{
		}
		Unary(const Unary& other) = delete;
		Unary& operator=(const Unary& other) = delete;
		[[nodiscard]] const Token& GetOperator() const LIFETIME_BOUND
		{
			return m_operator;
		}
		[[nodiscard]] SourceLocation GetSourceLocation() const
		{
			return m_operator.sourceLocation;
		}
		[[nodiscard]] PrimitiveType GetPrimitiveType() const
		{
			return m_primitiveType;
		}
		[[nodiscard]] Optional<const Expression::Base*> GetRight() const LIFETIME_BOUND
		{
			return m_pRight.Get();
		}
		void SetRight(const Optional<Expression::Base*> pRight)
		{
			m_pRight = pRight;
		}
		[[nodiscard]] bool operator==(const Unary& other) const
		{
			return m_operator.type == other.m_operator.type && *m_pRight == *other.m_pRight;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Token m_operator;
		PrimitiveType m_primitiveType;
		Optional<Expression::Base*> m_pRight;
	};

	struct Group final : public Expression::Base
	{
		Group()
			: Expression::Base(NodeType::Group)
		{
		}
		Group(Expressions&& expressions)
			: Expression::Base(NodeType::Group)
			, m_expressions(Move(expressions))
		{
		}
		Group(const Group& other) = delete;
		Group& operator=(const Group& other) = delete;
		[[nodiscard]] const Expressions& GetExpressions() const LIFETIME_BOUND
		{
			return m_expressions;
		}
		[[nodiscard]] bool operator==(const Group& other) const
		{
			return m_expressions == other.m_expressions;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Expressions m_expressions;
	};

	struct Literal final : public Expression::Base
	{
		Literal()
			: Expression::Base(NodeType::Literal)
		{
		}
		Literal(ScriptValue& scriptValue) = delete;
		Literal(const ScriptValue& scriptValue) = delete;
		Literal(ScriptValue&& scriptValue) = delete;
		Literal(Any&& value)
			: Expression::Base(NodeType::Literal)
			, m_value(Forward<Any>(value))
		{
		}
		Literal(const Literal& other) = delete;
		Literal& operator=(const Literal& other) = delete;
		[[nodiscard]] ConstAnyView GetValue() const LIFETIME_BOUND
		{
			return m_value;
		}
		[[nodiscard]] Reflection::TypeDefinition GetValueType() const
		{
			return m_value.GetTypeDefinition();
		}
		[[nodiscard]] bool operator==(const Literal& other) const
		{
			return m_value == other.m_value;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Any m_value;
	};

	struct VariableDeclaration final : public Expression::Base
	{
		using Token = VariableToken;
		using Tokens = Vector<Token>;

		VariableDeclaration()
			: Expression::Base(NodeType::VariableDeclaration)
		{
		}
		VariableDeclaration(VariableToken&& token, const bool isLocal)
			: Expression::Base(NodeType::VariableDeclaration)
			, m_identifier(Forward<VariableToken>(token))
			, m_isLocal{isLocal}
		{
		}
		VariableDeclaration(const VariableDeclaration& other) = delete;
		VariableDeclaration& operator=(const VariableDeclaration& other) = delete;

		[[nodiscard]] const VariableToken& GetIdentifier() const LIFETIME_BOUND
		{
			return m_identifier;
		}
		void SetAllowedTypes(Types&& types)
		{
			m_identifier.m_types = Forward<Types>(types);
		}
		[[nodiscard]] SourceLocation GetSourceLocation() const
		{
			return m_identifier.sourceLocation;
		}
		[[nodiscard]] bool IsLocal() const
		{
			return m_isLocal;
		}

		[[nodiscard]] bool operator==(const VariableDeclaration& other) const
		{
			return m_identifier == other.m_identifier && m_isLocal == other.m_isLocal;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		VariableToken m_identifier;
		bool m_isLocal{false};
	};

	struct Variable final : public Expression::Base
	{
		using Token = VariableToken;
		using Tokens = Vector<Token, uint16>;

		Variable()
			: Expression::Base(NodeType::Variable)
		{
		}
		Variable(VariableToken&& token)
			: Expression::Base(NodeType::Variable)
			, m_identifier(Forward<VariableToken>(token))
		{
		}
		Variable(Optional<Expression::Base*> object, VariableToken&& token)
			: Expression::Base(NodeType::Variable)
			, m_pObject(Move(object))
			, m_identifier(Forward<VariableToken>(token))
		{
		}
		Variable(Optional<Expression::Base*> object, Optional<Expression::Base*> index, VariableToken&& token)
			: Expression::Base(NodeType::Variable)
			, m_pObject(Move(object))
			, m_pIndex(Move(index))
			, m_identifier(Forward<VariableToken>(token))
		{
		}
		Variable(const Variable& other) = delete;
		Variable& operator=(const Variable& other) = delete;
		[[nodiscard]] Optional<const Expression::Base*> GetObject() const LIFETIME_BOUND
		{
			return m_pObject.Get();
		}
		void SetObject(const Optional<Expression::Base*> pObject)
		{
			m_pObject = pObject;
		}

		[[nodiscard]] Optional<const Expression::Base*> GetIndex() const LIFETIME_BOUND
		{
			return m_pIndex.Get();
		}
		void SetIndex(const Optional<Expression::Base*> pIndex)
		{
			m_pIndex = pIndex;
		}

		[[nodiscard]] const VariableToken& GetIdentifier() const LIFETIME_BOUND
		{
			return m_identifier;
		}
		[[nodiscard]] SourceLocation GetSourceLocation() const
		{
			return m_identifier.sourceLocation;
		}
		void SetAllowedTypes(Types&& types)
		{
			m_identifier.m_types = Forward<Types>(types);
		}

		[[nodiscard]] bool operator==(const Variable& other) const
		{
			return ((m_pObject.IsInvalid() && other.m_pObject.IsInvalid()) || (*m_pObject == *other.m_pObject)) &&
			       ((m_pIndex.IsInvalid() && other.m_pIndex.IsInvalid()) || (*m_pIndex == *other.m_pIndex)) && m_identifier == other.m_identifier;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Optional<Expression::Base*> m_pObject;
		Optional<Expression::Base*> m_pIndex;
		VariableToken m_identifier;
	};

	struct Assignment final : public Expression::Base
	{
		Assignment()
			: Expression::Base(NodeType::Assignment)
		{
		}
		Assignment(AST::Expressions&& varlist, AST::Expressions&& exprlist)
			: Expression::Base(NodeType::Assignment)
			, m_varlist(Move(varlist))
			, m_exprlist(Move(exprlist))
		{
		}
		Assignment(const Assignment& other) = delete;
		Assignment& operator=(const Assignment& other) = delete;
		[[nodiscard]] const AST::Expressions& GetVariables() const LIFETIME_BOUND
		{
			return m_varlist;
		}
		[[nodiscard]] AST::Expressions& GetVariables() LIFETIME_BOUND
		{
			return m_varlist;
		}
		[[nodiscard]] const AST::Expressions& GetExpressions() const LIFETIME_BOUND
		{
			return m_exprlist;
		}
		[[nodiscard]] AST::Expressions& GetExpressions() LIFETIME_BOUND
		{
			return m_exprlist;
		}
		[[nodiscard]] bool operator==(const Assignment& other) const
		{
			return m_varlist == other.m_varlist && m_exprlist == other.m_exprlist;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		AST::Expressions m_varlist;
		AST::Expressions m_exprlist;
	};

	struct Logical final : public Expression::Base
	{
		Logical()
			: Expression::Base(NodeType::Logical)
		{
		}
		Logical(Optional<Expression::Base*> pLeft, Token&& token, Optional<Expression::Base*> pRight)
			: Expression::Base(NodeType::Logical)
			, m_pLeft(Move(pLeft))
			, m_operator(Move(token))
			, m_pRight(Move(pRight))
		{
		}
		Logical(const Logical& other) = delete;
		Logical& operator=(const Logical& other) = delete;
		[[nodiscard]] Optional<const Expression::Base*> GetLeft() const LIFETIME_BOUND
		{
			return m_pLeft.Get();
		}
		[[nodiscard]] const Token& GetOperator() const LIFETIME_BOUND
		{
			return m_operator;
		}
		[[nodiscard]] SourceLocation GetSourceLocation() const
		{
			return m_operator.sourceLocation;
		}
		[[nodiscard]] Optional<const Expression::Base*> GetRight() const LIFETIME_BOUND
		{
			return m_pRight.Get();
		}
		[[nodiscard]] bool operator==(const Logical& other) const
		{
			return *m_pLeft == *other.m_pLeft && m_operator.type == other.m_operator.type && *m_pRight == *other.m_pRight;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Optional<Expression::Base*> m_pLeft;
		Token m_operator;
		Optional<Expression::Base*> m_pRight;
	};

	struct Call final : public Expression::Base
	{
		Call()
			: Expression::Base(NodeType::Call)
		{
		}
		Call(Expression::Base& callee, AST::Expressions&& arglist, const SourceLocation sourceLocation = {})
			: Expression::Base(NodeType::Call)
			, m_pCallee(callee)
			, m_arglist(Move(arglist))
			, m_sourceLocation(sourceLocation)
		{
		}
		Call(const Call& other) = delete;
		Call& operator=(const Call& other) = delete;
		[[nodiscard]] Optional<const Expression::Base*> GetCallee() const LIFETIME_BOUND
		{
			return m_pCallee.Get();
		}
		[[nodiscard]] Optional<Expression::Base*> GetCallee() LIFETIME_BOUND
		{
			return m_pCallee.Get();
		}
		void SetCallee(const Optional<Expression::Base*> pCallee)
		{
			m_pCallee = pCallee;
		}
		[[nodiscard]] SourceLocation GetSourceLocation() const
		{
			return m_sourceLocation;
		}
		[[nodiscard]] const Expressions& GetArguments() const LIFETIME_BOUND
		{
			return m_arglist;
		}
		[[nodiscard]] Expressions& GetArguments() LIFETIME_BOUND
		{
			return m_arglist;
		}
		[[nodiscard]] bool operator==(const Call& other) const
		{
			return *m_pCallee == *other.m_pCallee && m_arglist == other.m_arglist;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Optional<Expression::Base*> m_pCallee;
		AST::Expressions m_arglist;
		SourceLocation m_sourceLocation;
	};

	struct Function final : public Expression::Base
	{
		using Parameters = Vector<VariableToken, uint16>;
		using ReturnTypes = Vector<VariableToken, uint16>;

		Function()
			: Expression::Base(NodeType::Function)
		{
		}
		Function(Parameters&& parameters, ReturnTypes&& returnTypes, Statements&& body)
			: Expression::Base(NodeType::Function)
			, m_parameters(Move(parameters))
			, m_returnTypes(Forward<ReturnTypes>(returnTypes))
			, m_body(Move(body))
		{
		}
		Function(Function&& other)
			: Expression::Base(NodeType::Function)
			, m_parameters(Move(other.m_parameters))
			, m_returnTypes(Move((other.m_returnTypes)))
			, m_body(Move((other.m_body)))
		{
		}
		Function& operator=(Function&&) = default;
		Function(const Function& other) = delete;
		Function& operator=(const Function& other) = delete;
		[[nodiscard]] const Parameters& GetParameters() const LIFETIME_BOUND
		{
			return m_parameters;
		}
		[[nodiscard]] ReturnTypes::ConstView GetReturnTypes() const LIFETIME_BOUND
		{
			return m_returnTypes;
		}
		[[nodiscard]] const Statements& GetStatements() const LIFETIME_BOUND
		{
			return m_body;
		}
		[[nodiscard]] Statements& GetStatements() LIFETIME_BOUND
		{
			return m_body;
		}
		[[nodiscard]] bool operator==(const Function& other) const
		{
			{
				if (m_parameters.GetSize() != other.m_parameters.GetSize())
				{
					return false;
				}

				for (auto it = m_parameters.begin(), endIt = m_parameters.end(), otherIt = other.m_parameters.begin(); it != endIt; ++it, ++otherIt)
				{
					if (*it != *otherIt)
					{
						return false;
					}
				}
			}

			{
				if (m_returnTypes.GetSize() != other.m_returnTypes.GetSize())
				{
					return false;
				}

				for (auto it = m_returnTypes.begin(), endIt = m_returnTypes.end(), otherIt = other.m_returnTypes.begin(); it != endIt;
				     ++it, ++otherIt)
				{
					if (*it != *otherIt)
					{
						return false;
					}
				}
			}

			return m_body == other.m_body;
		}
		bool SerializeType(Serialization::Writer writer, const Graph& graph) const;
		bool Serialize(const Serialization::Reader reader, Graph& graph);
	private:
		Parameters m_parameters;
		ReturnTypes m_returnTypes;
		Statements m_body;
	};
}
