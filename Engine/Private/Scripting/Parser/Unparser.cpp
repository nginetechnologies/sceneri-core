#include "Engine/Scripting/Parser/Unparser.h"

#include "Engine/Scripting/Parser/AST/Statement.h"
#include "Engine/Scripting/Parser/AST/Expression.h"

#include <Common/Memory/Variant.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Reflection/GenericType.h>

namespace ngine::Scripting
{
	struct DepthScope
	{
	public:
		DepthScope(int32& depth)
			: m_depth(depth)
		{
			++m_depth;
		}
		~DepthScope()
		{
			--m_depth;
		}
	private:
		int32& m_depth;
	};

	Unparser::Unparser()
	{
	}

	StringType Unparser::Unparse(const AST::Statement::Base& statement)
	{
		m_state.buffer.Reserve(4098);
		m_state.buffer.Clear();
		m_state.depth = 0;
		m_state.flags = Flags::SilentBlock;

		Visit(statement);

		return m_state.buffer;
	}

	StringType Unparser::Unparse(const AST::Expression::Base& expression)
	{
		m_state.buffer.Reserve(1024);
		m_state.buffer.Clear();
		m_state.depth = 0;
		m_state.flags = Flags::SilentBlock;

		Visit(expression);

		return m_state.buffer;
	}

	void Unparser::Visit(const AST::Statement::Block& block)
	{
		const bool showBlock = m_state.depth > 0 && !m_state.flags.IsSet(Flags::SilentBlock);
		m_state.flags.Clear(Flags::SilentBlock);
		if (showBlock)
		{
			Write("do");
			Line();
		}

		{
			DepthScope scope(m_state.depth);
			for (const ReferenceWrapper<const AST::Statement::Base>& pStatement : block.GetStatements())
			{
				Visit(*pStatement);
				Line();
			}
		}

		if (showBlock)
		{
			Write("end");
		}
	}

	void Unparser::Visit(const AST::Statement::Break&)
	{
		Write("break");
	}

	void Unparser::Visit(const AST::Statement::Expression& expression)
	{
		Visit(*expression.GetExpression());
	}

	void Unparser::Visit(const AST::Statement::If& stmtIf)
	{
		const AST::Expressions& conditions = stmtIf.GetConditions();
		const AST::Statements& thens = stmtIf.GetThens();

		auto thenIt = thens.begin();
		for (const ReferenceWrapper<const AST::Expression::Base>& pCondition : conditions)
		{
			if (thenIt == thens.begin())
			{
				Write("if ");
			}
			else
			{
				Write("elseif ");
			}
			Visit(*pCondition);
			Write(" then");
			Line();
			m_state.flags.Set(Flags::SilentBlock);
			Visit(*thenIt);
			++thenIt;
		}
		if (stmtIf.GetElse().IsValid())
		{
			Write("else");
			Line();
			m_state.flags.Set(Flags::SilentBlock);
			Visit(*stmtIf.GetElse());
		}
		Write("end");
	}

	void Unparser::Visit(const AST::Statement::Repeat& repeat)
	{
		Write("repeat");
		{
			DepthScope scope(m_state.depth);
			const AST::Statements& statements = repeat.GetStatements();
			for (const ReferenceWrapper<const AST::Statement::Base>& pStatement : statements)
			{
				Line();
				Visit(*pStatement);
			}
		}
		Line();
		Write("until ");
		Visit(*repeat.GetCondition());
	}

	void Unparser::Visit(const AST::Statement::Return& stmtReturn)
	{
		Write("return");
		const AST::Expressions& expressions = stmtReturn.GetExpressions();
		auto expressionIt = expressions.begin();
		while (expressionIt != expressions.end())
		{
			Write(" ");
			Visit(*expressionIt);
			++expressionIt;
			if (expressionIt != expressions.end())
			{
				Write(",");
			}
		}
	}

	void Unparser::Visit(const AST::Statement::While& stmtWhile)
	{
		Write("while ");
		Visit(*stmtWhile.GetCondition());
		Write(" ");
		Visit(*stmtWhile.GetBody());
	}

	void Unparser::Visit(const AST::Expression::Assignment& assignment)
	{
		const AST::Expressions& varlist = assignment.GetVariables();
		const bool isLocal = varlist.GetView().Any(
			[](const AST::Expression::Base& variableBase)
			{
				switch (variableBase.GetType())
				{
					case AST::NodeType::VariableDeclaration:
						return static_cast<const AST::Expression::VariableDeclaration&>(variableBase).IsLocal();
					case AST::NodeType::Variable:
						return false;
					default:
						ExpectUnreachable();
				}
			}
		);
		if (isLocal)
		{
			Write("local ");
		}

		auto varIt = varlist.begin();
		while (varIt != varlist.end())
		{
			Visit(*varIt);
			++varIt;
			if (varIt != varlist.end())
			{
				Write(", ");
			}
		}

		const AST::Expressions& exprlist = assignment.GetExpressions();
		auto exprIt = exprlist.begin();
		if (exprIt != exprlist.end())
		{
			Write(" = ");
		}

		while (exprIt != exprlist.end())
		{
			Visit(*exprIt);
			++exprIt;
			if (exprIt != exprlist.end())
			{
				Write(", ");
			}
		}
	}

	void Unparser::Visit(const AST::Expression::Binary& binary)
	{
		Visit(*binary.GetLeft());
		Write(" ");
		Write(binary.GetOperator().literal);
		Write(" ");
		Visit(*binary.GetRight());
	}

	void Unparser::Visit(const AST::Expression::Call& call)
	{
		Visit(*call.GetCallee());
		Write("(");
		const AST::Expressions& expressions = call.GetArguments();
		auto expressionIt = expressions.begin();
		while (expressionIt != expressions.end())
		{
			Visit(*expressionIt);
			++expressionIt;
			if (expressionIt != expressions.end())
			{
				Write(", ");
			}
		}
		Write(")");
	}

	void Unparser::Visit(const AST::Expression::Function& function)
	{
		Write("function (");

		const AST::Expression::Function::Parameters& parameters = function.GetParameters();
		auto tokensIt = parameters.begin();
		while (tokensIt != parameters.end())
		{
			Write(tokensIt->literal);
			++tokensIt;
			if (tokensIt != parameters.end())
			{
				Write(", ");
			}
		}
		Write(")");
		{
			DepthScope scope(m_state.depth);
			const AST::Statements& body = function.GetStatements();
			for (const ReferenceWrapper<const AST::Statement::Base>& pStatement : body)
			{
				Line();
				Visit(*pStatement);
			}
		}
		Line();
		Write("end");
	}

	void Unparser::Visit(const AST::Expression::Group& group)
	{
		const AST::Expressions& expressions = group.GetExpressions();
		for (const ReferenceWrapper<const AST::Expression::Base>& pExpression : expressions)
		{
			Visit(*pExpression);
		}
	}

	void Unparser::Visit(const AST::Expression::Literal& literal)
	{
		const ScriptValue value{literal.GetValue()};
		return value.Visit(
			[this](nullptr_type)
			{
				Write("nil");
			},
			[this](bool value)
			{
				if (value)
				{
					Write("true");
				}
				else
				{
					Write("false");
				}
			},
			[this](const IntegerType value)
			{
				StringType result;
				result.Format(SCRIPT_STRING_LITERAL("{:d}"), value);
				Write(result);
			},
			[this](const FloatType value)
			{
				StringType result;
				result.Format(SCRIPT_STRING_LITERAL("{:g}"), value);
				Write(result);
			},
			[this](const StringType& value)
			{
				Write("\"");
				Write(value);
				Write("\"");
			},
			[](const FunctionIdentifier)
			{
				Assert(false, "Not supported as literal");
				ExpectUnreachable();
			},
			[](const ScriptTableIdentifier)
			{
				Assert(false, "Not supported as literal");
				ExpectUnreachable();
			},
			[](const ConstAnyView) mutable
			{
				Assert(false, "Not supported as literal");
				ExpectUnreachable();
			},
			[]()
			{
				ExpectUnreachable();
			}
		);
	}

	void Unparser::Visit(const AST::Expression::Logical& logical)
	{
		Visit(*logical.GetLeft());
		Write(" ");
		Write(logical.GetOperator().literal);
		Write(" ");
		Visit(*logical.GetRight());
	}

	void Unparser::Visit(const AST::Expression::Unary& unary)
	{
		Write(unary.GetOperator().literal);
		if (unary.GetOperator().type == TokenType::Not)
		{
			Write(" ");
		}
		Visit(*unary.GetRight());
	}

	void Unparser::Visit(const AST::Expression::VariableDeclaration& variableDeclaration)
	{
		Write(variableDeclaration.GetIdentifier().literal);
	}

	void Unparser::Visit(const AST::Expression::Variable& variable)
	{
		bool isObject = false;
		if (const Optional<const AST::Expression::Base*> pObject = variable.GetObject())
		{
			Visit(*pObject);
			isObject = true;
		}
		if (const Optional<const AST::Expression::Base*> pIndex = variable.GetIndex())
		{
			Write("[");
			Visit(*pIndex);
			Write("]");
		}
		if (isObject)
		{
			Write(".");
		}
		Write(variable.GetIdentifier().literal);
	}

	void Unparser::Line()
	{
		m_state.buffer += SCRIPT_STRING_LITERAL("\n");
		m_state.flags.Set(Flags::LineStart);
	}

	void Unparser::Write(StringType::ConstView text)
	{
		if (m_state.flags.IsSet(Flags::LineStart))
		{
			StringType output;
			output.Format("{:{}}{}", "", Math::Max(0, (m_state.depth - 1) * Unparser::SpacesPerTab), text);
			m_state.buffer += output;
			m_state.flags.Clear(Flags::LineStart);
		}
		else
		{
			m_state.buffer += text;
		}
	}
}
