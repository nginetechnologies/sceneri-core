#include "Engine/Scripting/Parser/AST/Expression.h"
#include "Engine/Scripting/Parser/AST/Statement.h"
#include "Engine/Scripting/Parser/AST/VisitNode.h"
#include "Engine/Scripting/Parser/AST/Graph.h"

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Writer.h>
#include <Common/Serialization/Guid.h>
#include <Common/Memory/Containers/Serialization/Vector.h>
#include <Common/Memory/Serialization/UniquePtr.h>
#include <Common/Reflection/Registry.h>
#include <Common/Reflection/GenericType.h>
#include <Common/System/Query.h>

namespace ngine::Scripting::AST
{
	Graph::Graph(Graph&&) = default;
	Graph& Graph::operator=(Graph&&) = default;
	Graph::~Graph() = default;

	bool Graph::Serialize(const Serialization::Reader reader)
	{
		if (const Optional<Serialization::Reader> nodesReader = reader.FindSerializer("nodes"))
		{
			m_nodes.Reserve((uint32)nodesReader->GetArraySize());

			// First pass: Simply create each node, since we need index based lookup during serialization
			for (const Serialization::Reader nodeReader : nodesReader->GetArrayView())
			{
				switch (*nodeReader.Read<NodeType>("type"))
				{
					case NodeType::Binary:
						EmplaceNode<Expression::Binary>();
						break;
					case NodeType::Logical:
						EmplaceNode<Expression::Logical>();
						break;
					case NodeType::Unary:
						EmplaceNode<Expression::Unary>();
						break;
					case NodeType::Group:
						EmplaceNode<Expression::Group>();
						break;
					case NodeType::Literal:
						EmplaceNode<Expression::Literal>();
						break;
					case NodeType::VariableDeclaration:
						EmplaceNode<Expression::VariableDeclaration>();
						break;
					case NodeType::Variable:
						EmplaceNode<Expression::Variable>();
						break;
					case NodeType::Assignment:
						EmplaceNode<Expression::Assignment>();
						break;
					case NodeType::Call:
						EmplaceNode<Expression::Call>();
						break;
					case NodeType::Function:
						EmplaceNode<Expression::Function>();
						break;

					case NodeType::Expression:
						EmplaceNode<Statement::Expression>();
						break;
					case NodeType::Block:
						EmplaceNode<Statement::Block>();
						break;
					case NodeType::If:
						EmplaceNode<Statement::If>();
						break;
					case NodeType::While:
						EmplaceNode<Statement::While>();
						break;
					case NodeType::Repeat:
						EmplaceNode<Statement::Repeat>();
						break;
					case NodeType::Break:
						EmplaceNode<Statement::Break>();
						break;
					case NodeType::Return:
						EmplaceNode<Statement::Return>();
						break;
					case NodeType::Count:
						ExpectUnreachable();
				}
			}

			// Second pass: serialize the node info
			NodeIndexType nodeIndex{0};
			for (const Serialization::Reader nodeReader : nodesReader->GetArrayView())
			{
				const Optional<Node*> pNode = GetNode(nodeIndex);
				Assert(pNode.IsValid());
				if (UNLIKELY(pNode.IsInvalid()))
				{
					nodeIndex++;
					continue;
				}

				switch (*nodeReader.Read<NodeType>("type"))
				{
					case NodeType::Binary:
						static_cast<Expression::Binary&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Logical:
						static_cast<Expression::Logical&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Unary:
						static_cast<Expression::Unary&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Group:
						static_cast<Expression::Group&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Literal:
						static_cast<Expression::Literal&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::VariableDeclaration:
						static_cast<Expression::VariableDeclaration&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Variable:
						static_cast<Expression::Variable&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Assignment:
						static_cast<Expression::Assignment&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Call:
						static_cast<Expression::Call&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Function:
						static_cast<Expression::Function&>(*pNode).Serialize(nodeReader, *this);
						break;

					case NodeType::Expression:
						static_cast<Statement::Expression&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Block:
						static_cast<Statement::Block&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::If:
						static_cast<Statement::If&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::While:
						static_cast<Statement::While&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Repeat:
						static_cast<Statement::Repeat&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Break:
						static_cast<Statement::Break&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Return:
						static_cast<Statement::Return&>(*pNode).Serialize(nodeReader, *this);
						break;
					case NodeType::Count:
						ExpectUnreachable();
				}
				nodeIndex++;
			}

			return true;
		}
		else
		{
			return false;
		}
	}

	bool Graph::Serialize(Serialization::Writer writer) const
	{
		if (m_nodes.HasElements())
		{
			const ArrayView<const UniquePtr<Node>> nodes = m_nodes;
			return writer.SerializeArrayWithCallback(
				"nodes",
				[nodes, this](Serialization::Writer writer, const NodeIndexType index)
				{
					writer.GetValue().SetObject();
					return nodes[index]->Serialize(writer, *this);
				},
				nodes.GetSize()
			);
		}
		else
		{
			return false;
		}
	}

	Optional<const Expression::Function*> Graph::FindFunction(const Guid functionGuid) const
	{
		for (const Node& node : GetNodes())
		{
			// Find all assignments that declare function variables
			if (node.GetType() == NodeType::Assignment)
			{
				const Expression::Assignment& assignmentStatement = static_cast<const Expression::Assignment&>(node);
				// Only consider assignments to one specific variable
				if (assignmentStatement.GetVariables().GetSize() != 1 || assignmentStatement.GetExpressions().GetSize() != 1)
				{
					continue;
				}

				// We detect function entry points as function variable declarations
				const Expression::Base& variableExpression = assignmentStatement.GetVariables().GetView()[0];
				switch (variableExpression.GetType())
				{
					case NodeType::VariableDeclaration:
					{
						const Expression::VariableDeclaration& variableDeclaration =
							static_cast<const Expression::VariableDeclaration&>(variableExpression);

						const Expression::Base& expression = assignmentStatement.GetExpressions().GetView()[0];
						if (expression.GetType() != NodeType::Function)
						{
							continue;
						}

						if (variableDeclaration.GetIdentifier().identifier == functionGuid)
						{
							return static_cast<const Expression::Function&>(expression);
						}
					}
					break;
					case Scripting::AST::NodeType::Variable:
						continue;
					default:
						Assert(false, "Unexpected node type");
						return Invalid;
				}
			}
		}
		return Invalid;
	}

	void Graph::IterateFunctions(FunctionCallback&& callback) const
	{
		for (const Node& node : GetNodes())
		{
			// Find all assignments that declare function variables
			if (node.GetType() == NodeType::Assignment)
			{
				const Expression::Assignment& assignmentStatement = static_cast<const Expression::Assignment&>(node);
				// Only consider assignments to one specific variable
				if (assignmentStatement.GetVariables().GetSize() != 1 || assignmentStatement.GetExpressions().GetSize() != 1)
				{
					continue;
				}

				const Expression::Base& expression = assignmentStatement.GetExpressions().GetView()[0];
				if (expression.GetType() != NodeType::Function)
				{
					continue;
				}

				// We detect function entry points as function variable declarations
				const Expression::Base& variableExpression = assignmentStatement.GetVariables().GetView()[0];
				switch (variableExpression.GetType())
				{
					case NodeType::VariableDeclaration:
					{
						const Expression::VariableDeclaration& variableDeclaration =
							static_cast<const Expression::VariableDeclaration&>(variableExpression);

						callback(variableDeclaration, static_cast<const Expression::Function&>(expression));
					}
					break;
					case Scripting::AST::NodeType::Variable:
					{
						const Expression::Variable& variable = static_cast<const Expression::Variable&>(variableExpression);

						callback(variable, static_cast<const Expression::Function&>(expression));
					}
					break;
					default:
						ExpectUnreachable();
				}
			}
		}
	}

	bool Statements::Serialize(const Serialization::Reader reader, Graph& graph)
	{
		m_statements.Reserve((uint32)reader.GetArraySize());
		for (const Serialization::Reader statementReader : reader.GetArrayView())
		{
			const Graph::NodeIndexType nodeIndex = *statementReader.ReadInPlace<Graph::NodeIndexType>();
			const Optional<Node*> pNode = graph.GetNode(nodeIndex);
			Assert(pNode.IsValid());
			if (LIKELY(pNode.IsValid()))
			{
				m_statements.EmplaceBack(static_cast<Statement::Base&>(*pNode));
			}
		}
		return true;
	}

	bool Statements::Serialize(Serialization::Writer writer, const Graph& graph) const
	{
		const ArrayView<const ReferenceWrapper<const Statement::Base>> statements = GetView();
		writer.GetValue().SetArray();
		return writer.SerializeArrayCallbackInPlace(
			[statements, &graph](Serialization::Writer writer, const uint32 index)
			{
				const Optional<Graph::NodeIndexType> nodeIndex = graph.GetNodeIndex(statements[index]);
				return nodeIndex.IsValid() && writer.SerializeInPlace(*nodeIndex);
			},
			statements.GetSize()
		);
	}

	bool Expressions::Serialize(const Serialization::Reader reader, Graph& graph)
	{
		m_expressions.Reserve((uint32)reader.GetArraySize());
		for (const Serialization::Reader expressionReader : reader.GetArrayView())
		{
			const Optional<Graph::NodeIndexType> nodeIndex = expressionReader.ReadInPlace<Graph::NodeIndexType>();
			if (nodeIndex.IsValid())
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_expressions.EmplaceBack(static_cast<Expression::Base&>(*pNode));
				}
			}
		}
		return true;
	}

	bool Expressions::Serialize(Serialization::Writer writer, const Graph& graph) const
	{
		const ArrayView<const ReferenceWrapper<const Expression::Base>> expressions = GetView();
		writer.GetValue().SetArray();
		return writer.SerializeArrayCallbackInPlace(
			[expressions, &graph](Serialization::Writer writer, const uint32 index)
			{
				const Optional<Graph::NodeIndexType> nodeIndex = graph.GetNodeIndex(expressions[index]);
				return nodeIndex.IsValid() && writer.SerializeInPlace(*nodeIndex);
			},
			expressions.GetSize()
		);
	}

	bool Node::operator==(const Node& other) const
	{
		if (m_type != other.m_type)
		{
			return false;
		}

		switch (m_type)
		{
			case NodeType::Binary:
				return static_cast<const Expression::Binary&>(*this) == static_cast<const Expression::Binary&>(other);
			case NodeType::Logical:
				return static_cast<const Expression::Logical&>(*this) == static_cast<const Expression::Logical&>(other);
			case NodeType::Unary:
				return static_cast<const Expression::Unary&>(*this) == static_cast<const Expression::Unary&>(other);
			case NodeType::Group:
				return static_cast<const Expression::Group&>(*this) == static_cast<const Expression::Group&>(other);
			case NodeType::Literal:
				return static_cast<const Expression::Literal&>(*this) == static_cast<const Expression::Literal&>(other);
			case NodeType::VariableDeclaration:
				return static_cast<const Expression::VariableDeclaration&>(*this) == static_cast<const Expression::VariableDeclaration&>(other);
			case NodeType::Variable:
				return static_cast<const Expression::Variable&>(*this) == static_cast<const Expression::Variable&>(other);
			case NodeType::Assignment:
				return static_cast<const Expression::Assignment&>(*this) == static_cast<const Expression::Assignment&>(other);
			case NodeType::Call:
				return static_cast<const Expression::Call&>(*this) == static_cast<const Expression::Call&>(other);
			case NodeType::Function:
				return static_cast<const Expression::Function&>(*this) == static_cast<const Expression::Function&>(other);

			case NodeType::Expression:
				return static_cast<const Statement::Expression&>(*this) == static_cast<const Statement::Expression&>(other);
			case NodeType::Block:
				return static_cast<const Statement::Block&>(*this) == static_cast<const Statement::Block&>(other);
			case NodeType::If:
				return static_cast<const Statement::If&>(*this) == static_cast<const Statement::If&>(other);
			case NodeType::While:
				return static_cast<const Statement::While&>(*this) == static_cast<const Statement::While&>(other);
			case NodeType::Repeat:
				return static_cast<const Statement::Repeat&>(*this) == static_cast<const Statement::Repeat&>(other);
			case NodeType::Break:
				return static_cast<const Statement::Break&>(*this) == static_cast<const Statement::Break&>(other);
			case NodeType::Return:
				return static_cast<const Statement::Return&>(*this) == static_cast<const Statement::Return&>(other);
			case NodeType::Count:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	bool Node::IsStatement() const
	{
		switch (m_type)
		{
			case NodeType::Binary:
			case NodeType::Logical:
			case NodeType::Unary:
			case NodeType::Group:
			case NodeType::Literal:
			case NodeType::Variable:
			case NodeType::VariableDeclaration:
			case NodeType::Assignment:
			case NodeType::Call:
			case NodeType::Function:
				return false;

			case NodeType::Expression:
			case NodeType::Block:
			case NodeType::If:
			case NodeType::While:
			case NodeType::Repeat:
			case NodeType::Break:
			case NodeType::Return:
				return true;

			case NodeType::Count:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	bool Node::Serialize(const Serialization::Reader reader, Graph&)
	{
		reader.Serialize("coordinate", m_coordinate);
		return true;
	}

	bool Node::Serialize(Serialization::Writer writer, const Graph& graph) const
	{
		[[maybe_unused]] const bool wroteType = writer.Serialize("type", m_type);
		Assert(wroteType);

		writer.Serialize("coordinate", m_coordinate);

		switch (m_type)
		{
			case NodeType::Binary:
				return static_cast<const Expression::Binary&>(*this).SerializeType(writer, graph);
			case NodeType::Logical:
				return static_cast<const Expression::Logical&>(*this).SerializeType(writer, graph);
			case NodeType::Unary:
				return static_cast<const Expression::Unary&>(*this).SerializeType(writer, graph);
			case NodeType::Group:
				return static_cast<const Expression::Group&>(*this).SerializeType(writer, graph);
			case NodeType::Literal:
				return static_cast<const Expression::Literal&>(*this).SerializeType(writer, graph);
			case NodeType::VariableDeclaration:
				return static_cast<const Expression::VariableDeclaration&>(*this).SerializeType(writer, graph);
			case NodeType::Variable:
				return static_cast<const Expression::Variable&>(*this).SerializeType(writer, graph);
			case NodeType::Assignment:
				return static_cast<const Expression::Assignment&>(*this).SerializeType(writer, graph);
			case NodeType::Call:
				return static_cast<const Expression::Call&>(*this).SerializeType(writer, graph);
			case NodeType::Function:
				return static_cast<const Expression::Function&>(*this).SerializeType(writer, graph);

			case NodeType::Expression:
				return static_cast<const Statement::Expression&>(*this).SerializeType(writer, graph);
			case NodeType::Block:
				return static_cast<const Statement::Block&>(*this).SerializeType(writer, graph);
			case NodeType::If:
				return static_cast<const Statement::If&>(*this).SerializeType(writer, graph);
			case NodeType::While:
				return static_cast<const Statement::While&>(*this).SerializeType(writer, graph);
			case NodeType::Repeat:
				return static_cast<const Statement::Repeat&>(*this).SerializeType(writer, graph);
			case NodeType::Break:
				return static_cast<const Statement::Break&>(*this).SerializeType(writer, graph);
			case NodeType::Return:
				return static_cast<const Statement::Return&>(*this).SerializeType(writer, graph);
			case NodeType::Count:
				ExpectUnreachable();
		}
		ExpectUnreachable();
	}

	namespace Expression
	{
		bool Binary::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			if (m_pLeft.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("left", graph.GetNodeIndex(*m_pLeft));
				Assert(wasWritten);
			}
			[[maybe_unused]] const bool wroteOperator = writer.Serialize("operator", m_operator.type);
			Assert(wroteOperator);
			if (m_pRight.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("right", graph.GetNodeIndex(*m_pRight));
				Assert(wasWritten);
			}
			return true;
		}

		bool Logical::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			if (m_pLeft.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("left", graph.GetNodeIndex(*m_pLeft));
				Assert(wasWritten);
			}
			[[maybe_unused]] const bool wroteOperator = writer.Serialize("operator", m_operator.type);
			Assert(wroteOperator);
			if (m_pRight.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("right", graph.GetNodeIndex(*m_pRight));
				Assert(wasWritten);
			}
			return true;
		}

		bool Unary::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			[[maybe_unused]] const bool wroteOperator = writer.Serialize("operator", m_operator.type);
			Assert(wroteOperator);
			if (m_pRight.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("right", graph.GetNodeIndex(*m_pRight));
				Assert(wasWritten);
			}
			return true;
		}

		bool Group::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			return writer.Serialize("expressions", m_expressions, graph);
		}

		bool Literal::SerializeType(Serialization::Writer writer, const Graph&) const
		{
			if (m_value.Is<nullptr_type>())
			{
				return writer.Serialize("value", nullptr);
			}
			else if (m_value.Is<bool>())
			{
				return writer.Serialize("value", m_value.GetExpected<bool>());
			}
			else if (m_value.Is<IntegerType>())
			{
				return writer.Serialize("value", m_value.GetExpected<IntegerType>());
			}
			else if (m_value.Is<FloatType>())
			{
				return writer.Serialize("value", m_value.GetExpected<FloatType>());
			}
			else if (m_value.Is<StringType>())
			{
				return writer.Serialize("value", m_value.GetExpected<StringType>());
			}
			else
			{
				return writer.Serialize("value", m_value);
			}
		}

		bool VariableDeclaration::SerializeType(Serialization::Writer writer, const Graph&) const
		{
			if (m_identifier.type == TokenType::Identifier)
			{
				writer.Serialize("identifier", m_identifier.identifier);
				if (m_identifier.literal.HasElements())
				{
					writer.Serialize("name", m_identifier.literal);
				}
			}
			writer.Serialize("local", m_isLocal);
			writer.Serialize("variable_type", m_identifier.m_types);
			return true;
		}

		bool Variable::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			if (m_pObject.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("object", graph.GetNodeIndex(*m_pObject));
				Assert(wasWritten);
			}
			if (m_pIndex.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("index", graph.GetNodeIndex(*m_pIndex));
				Assert(wasWritten);
			}
			if (m_identifier.type == TokenType::Identifier)
			{
				writer.Serialize("identifier", m_identifier.identifier);
				if (m_identifier.literal.HasElements())
				{
					writer.Serialize("name", m_identifier.literal);
				}
			}
			writer.Serialize("variable_type", m_identifier.m_types);
			return true;
		}

		bool Assignment::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			writer.Serialize("variables", m_varlist, graph);
			writer.Serialize("expressions", m_exprlist, graph);
			return true;
		}

		bool Call::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			if (m_pCallee.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("callee", graph.GetNodeIndex(*m_pCallee));
				Assert(wasWritten);
			}
			writer.Serialize("args", m_arglist, graph);
			return true;
		}

		bool Function::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			writer.SerializeArrayWithCallback(
				"parameters",
				[parameters = m_parameters.GetView()](Serialization::Writer writer, const uint16 index)
				{
					writer.GetValue().SetObject();
					const VariableToken& token = parameters[index];
					Assert(token.type == TokenType::Identifier);
					writer.Serialize("name", token.literal);
					if (!writer.Serialize("identifier", token.identifier))
					{
						return false;
					}
					if (!writer.Serialize("type", token.m_types))
					{
						return false;
					}
					return true;
				},
				m_parameters.GetSize()
			);
			writer.Serialize("body", m_body, graph);
			writer.SerializeArrayWithCallback(
				"return_types",
				[returnTypes = m_returnTypes.GetView()](Serialization::Writer writer, const uint16 index)
				{
					writer.GetValue().SetObject();

					const VariableToken& token = returnTypes[index];
					writer.Serialize("name", token.literal);
					writer.Serialize("identifier", token.identifier);
					return writer.Serialize("type", token.m_types);
				},
				m_returnTypes.GetSize()
			);
			return true;
		}
	}

	namespace Statement
	{
		bool Expression::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			if (m_pExpression.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("expression", graph.GetNodeIndex(*m_pExpression));
				Assert(wasWritten);
			}
			return true;
		}

		bool Block::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			return writer.Serialize("statements", m_statements, graph);
		}

		bool If::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			writer.Serialize("conditions", m_conditions, graph);
			writer.Serialize("thens", m_thens, graph);
			writer.Serialize("coordinates", m_coordinates);
			if (m_pElse.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("else", graph.GetNodeIndex(*m_pElse));
				Assert(wasWritten);
			}
			return true;
		}

		bool While::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			if (m_pCondition.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("condition", graph.GetNodeIndex(*m_pCondition));
				Assert(wasWritten);
			}
			if (m_pBody.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("body", graph.GetNodeIndex(*m_pBody));
				Assert(wasWritten);
			}
			return true;
		}

		bool Repeat::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			writer.Serialize("statements", m_statements, graph);
			if (m_pCondition.IsValid())
			{
				[[maybe_unused]] const bool wasWritten = writer.Serialize("condition", graph.GetNodeIndex(*m_pCondition));
				Assert(wasWritten);
			}
			return true;
		}

		bool Break::SerializeType(Serialization::Writer, const Graph&) const
		{
			return true;
		}

		bool Return::SerializeType(Serialization::Writer writer, const Graph& graph) const
		{
			writer.Serialize("expressions", m_exprlist, graph);
			return true;
		}
	}

	namespace Expression
	{
		bool Binary::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("left"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pLeft = static_cast<Expression::Base&>(*pNode);
				}
			}
			reader.Serialize("operator", m_operator.type);
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("right"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pRight = static_cast<Expression::Base&>(*pNode);
				}
			}
			return true;
		}

		bool Logical::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("left"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pLeft = static_cast<Expression::Base&>(*pNode);
				}
			}
			reader.Serialize("operator", m_operator.type);
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("right"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pRight = static_cast<Expression::Base&>(*pNode);
				}
			}
			return true;
		}

		bool Unary::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			reader.Serialize("operator", m_operator.type);
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("right"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pRight = static_cast<Expression::Base&>(*pNode);
				}
			}
			return true;
		}

		bool Group::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			reader.Serialize("expressions", m_expressions, graph);
			return true;
		}

		bool Literal::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			const Optional<Serialization::Reader> valueReader = reader.FindSerializer("value");
			if (valueReader->GetValue().IsNull())
			{
				m_value = nullptr;
				return true;
			}
			else if (valueReader->GetValue().IsBool())
			{
				m_value = *valueReader->ReadInPlace<bool>();
				return true;
			}
			else if (valueReader->GetValue().IsDouble())
			{
				m_value = *valueReader->ReadInPlace<FloatType>();
				return true;
			}
			else if (valueReader->GetValue().IsNumber())
			{
				m_value = *valueReader->ReadInPlace<IntegerType>();
				return true;
			}
			else if (valueReader->GetValue().IsString())
			{
				m_value = *valueReader->ReadInPlace<StringType>();
				return true;
			}
			else
			{
				return valueReader->SerializeInPlace(m_value);
			}
		}

		bool VariableDeclaration::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);

			if (const Optional<Serialization::Reader> identifierReader = reader.FindSerializer("identifier"))
			{
				m_identifier.type = TokenType::Identifier;
				identifierReader->SerializeInPlace(m_identifier.identifier);
				reader.Serialize("name", m_identifier.literal);
			}
			reader.Serialize("local", m_isLocal);
			reader.Serialize("variable_type", m_identifier.m_types);
			return true;
		}

		bool Variable::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("object"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pObject = static_cast<Expression::Base&>(*pNode);
				}
			}
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("index"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pIndex = static_cast<Expression::Base&>(*pNode);
				}
			}

			if (const Optional<Serialization::Reader> identifierReader = reader.FindSerializer("identifier"))
			{
				m_identifier.type = TokenType::Identifier;
				identifierReader->SerializeInPlace(m_identifier.identifier);
				reader.Serialize("name", m_identifier.literal);
			}

			reader.Serialize("variable_type", m_identifier.m_types);
			return true;
		}

		bool Assignment::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			reader.Serialize("variables", m_varlist, graph);
			reader.Serialize("expressions", m_exprlist, graph);
			return true;
		}

		bool Call::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("callee"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pCallee = static_cast<Expression::Base&>(*pNode);
				}
			}
			reader.Serialize("args", m_arglist, graph);
			return true;
		}

		bool Function::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			if (const Optional<Serialization::Reader> parametersReader = reader.FindSerializer("parameters"))
			{
				m_parameters.Clear();
				m_parameters.Reserve((uint16)parametersReader->GetArraySize());
				for (const Serialization::Reader parameterReader : parametersReader->GetArrayView())
				{
					VariableToken variableToken{
						Token{
							TokenType::Identifier,
							StringType{*parameterReader.Read<ConstStringView>("name")},
							*parameterReader.Read<Guid>("identifier")
						},
						Scripting::Types{}
					};
					parameterReader.Serialize("type", variableToken.m_types);

					m_parameters.EmplaceBack(Move(variableToken));
				}
			}

			reader.Serialize("body", m_body, graph);

			m_returnTypes.Clear();
			if (const Optional<Serialization::Reader> returnTypesReader = reader.FindSerializer("return_types"))
			{
				m_returnTypes.Reserve((uint16)returnTypesReader->GetArraySize());
				for (const Serialization::Reader returnTypeReader : returnTypesReader->GetArrayView())
				{
					if (Optional<Scripting::Types> type = returnTypeReader.Read<Scripting::Types>("type"))
					{
						m_returnTypes.EmplaceBack(VariableToken{
							Token{
								TokenType::Identifier,
								returnTypeReader.ReadWithDefaultValue<String>("name", String{}),
								returnTypeReader.ReadWithDefaultValue<Guid>("identifier", Guid{})
							},
							Move(*type)
						});
					}
				}
			}
			return true;
		}
	}

	namespace Statement
	{
		bool Expression::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("expression"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pExpression = static_cast<AST::Expression::Base&>(*pNode);
				}
			}
			return true;
		}

		bool Block::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			reader.Serialize("statements", m_statements, graph);
			return true;
		}

		bool If::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			reader.Serialize("conditions", m_conditions, graph);
			reader.Serialize("thens", m_thens, graph);
			reader.Serialize("coordinates", m_coordinates);
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("else"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pElse = static_cast<Expression::Base&>(*pNode);
				}
			}
			return true;
		}

		bool While::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("condition"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pCondition = static_cast<AST::Expression::Base&>(*pNode);
				}
			}
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("body"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pBody = static_cast<Expression::Base&>(*pNode);
				}
			}
			return true;
		}

		bool Repeat::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			reader.Serialize("statements", m_statements, graph);
			if (const Optional<Graph::NodeIndexType> nodeIndex = reader.Read<Graph::NodeIndexType>("condition"))
			{
				const Optional<Node*> pNode = graph.GetNode(*nodeIndex);
				Assert(pNode.IsValid());
				if (LIKELY(pNode.IsValid()))
				{
					m_pCondition = static_cast<AST::Expression::Base&>(*pNode);
				}
			}
			return true;
		}

		bool Break::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			return true;
		}

		bool Return::Serialize(const Serialization::Reader reader, Graph& graph)
		{
			Node::Serialize(reader, graph);
			reader.Serialize("expressions", m_exprlist, graph);
			return true;
		}
	}
}
