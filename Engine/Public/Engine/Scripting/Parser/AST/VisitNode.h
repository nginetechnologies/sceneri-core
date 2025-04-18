#pragma once

#include "Engine/Scripting/Parser/AST/Expression.h"
#include "Engine/Scripting/Parser/AST/Statement.h"

namespace ngine::Scripting::AST
{
	using NodeVariant = Variant<
		Expression::Binary,
		Expression::Logical,
		Expression::Unary,
		Expression::Group,
		Expression::Literal,
		Expression::VariableDeclaration,
		Expression::Variable,
		Expression::Assignment,
		Expression::Call,
		Expression::Function,
		Statement::Expression,
		Statement::Block,
		Statement::If,
		Statement::While,
		Statement::Repeat,
		Statement::Break,
		Statement::Return>;
	static_assert((uint8)NodeType::Count == NodeVariant::Size);

	template<typename Type, typename ReturnType = void>
	struct NodeVisitor
	{
		template<typename NodeType_, typename ThisType = Type>
		inline static constexpr bool CanVisit = TypeTraits::
			IsSame<decltype(TypeTraits::DeclareValue<ThisType&>().Visit(TypeTraits::DeclareValue<const NodeType_&>()), uint8()), uint8>;

		template<typename NodeType_>
		ReturnType VisitNodeInternal(const NodeType_& node)
		{
			if constexpr (CanVisit<NodeType_>)
			{
				return static_cast<Type&>(*this).Visit(node);
			}
			else if constexpr (!TypeTraits::IsSame<ReturnType, void>)
			{
				return ReturnType{};
			}
		}

		ReturnType Visit(const Node& node)
		{
			switch (node.GetType())
			{
				case NodeType::Binary:
					return VisitNodeInternal(static_cast<const Expression::Binary&>(node));
				case NodeType::Logical:
					return VisitNodeInternal(static_cast<const Expression::Logical&>(node));
				case NodeType::Unary:
					return VisitNodeInternal(static_cast<const Expression::Unary&>(node));
				case NodeType::Group:
					return VisitNodeInternal(static_cast<const Expression::Group&>(node));
				case NodeType::Literal:
					return VisitNodeInternal(static_cast<const Expression::Literal&>(node));
				case NodeType::VariableDeclaration:
					return VisitNodeInternal(static_cast<const Expression::VariableDeclaration&>(node));
				case NodeType::Variable:
					return VisitNodeInternal(static_cast<const Expression::Variable&>(node));
				case NodeType::Assignment:
					return VisitNodeInternal(static_cast<const Expression::Assignment&>(node));
				case NodeType::Call:
					return VisitNodeInternal(static_cast<const Expression::Call&>(node));
				case NodeType::Function:
					return VisitNodeInternal(static_cast<const Expression::Function&>(node));

				case NodeType::Expression:
					return VisitNodeInternal(static_cast<const Statement::Expression&>(node));
				case NodeType::Block:
					return VisitNodeInternal(static_cast<const Statement::Block&>(node));
				case NodeType::If:
					return VisitNodeInternal(static_cast<const Statement::If&>(node));
				case NodeType::While:
					return VisitNodeInternal(static_cast<const Statement::While&>(node));
				case NodeType::Repeat:
					return VisitNodeInternal(static_cast<const Statement::Repeat&>(node));
				case NodeType::Break:
					return VisitNodeInternal(static_cast<const Statement::Break&>(node));
				case NodeType::Return:
					return VisitNodeInternal(static_cast<const Statement::Return&>(node));
				case NodeType::Count:
					ExpectUnreachable();
			}
			ExpectUnreachable();
		}
	};
}
