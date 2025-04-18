#include "Engine/Scripting/Interpreter/Resolver.h"

#include "Engine/Scripting/Parser/AST/Statement.h"
#include "Engine/Scripting/Parser/AST/Expression.h"
#include "Engine/Scripting/Interpreter/Environment.h"

namespace ngine::Scripting
{
	Resolver::Resolver()
	{
	}

	SharedPtr<ResolvedVariableMap> Resolver::Resolve(const AST::Statement::Base& statement, Optional<const Environment&> pParentEnvironment)
	{
		Memory::Set(&m_state, sizeof(m_state), 0);
		m_state.pResolvedVariables = SharedPtr<ResolvedVariableMap>::Make();

		if (pParentEnvironment.IsValid())
		{
			BeginScope();
			const Environment::ScriptValueMap& values = pParentEnvironment->GetValues();
			for (auto valueIt : values)
			{
				Declare(valueIt.first);
				Define(valueIt.first);
			}
			Visit(statement);
			EndScope();
		}
		else
		{
			Visit(statement);
		}

		return m_state.pResolvedVariables;
	}

	SharedPtr<ResolvedVariableMap>
	Resolver::Resolve(const AST::Expression::Function& functionExpression, Optional<const Environment&> pParentEnvironment)
	{
		Memory::Set(&m_state, sizeof(m_state), 0);
		m_state.pResolvedVariables = SharedPtr<ResolvedVariableMap>::Make();

		if (pParentEnvironment.IsValid())
		{
			BeginScope();
			const Environment::ScriptValueMap& values = pParentEnvironment->GetValues();
			for (auto valueIt : values)
			{
				Declare(valueIt.first);
				Define(valueIt.first);
			}
			Visit(functionExpression);
			EndScope();
		}
		else
		{
			Visit(functionExpression);
		}

		return m_state.pResolvedVariables;
	}
	void Resolver::Visit(const AST::Statement::Block& block)
	{
		BeginScope();
		for (const ReferenceWrapper<const AST::Statement::Base>& pStatement : block.GetStatements())
		{
			Visit(*pStatement);
		}
		EndScope();
	}

	void Resolver::Visit(const AST::Statement::Break&)
	{
	}

	void Resolver::Visit(const AST::Statement::Expression& expression)
	{
		Visit(*expression.GetExpression());
	}

	void Resolver::Visit(const AST::Statement::If& stmtIf)
	{
		const AST::Expressions& conditions = stmtIf.GetConditions();
		const AST::Statements& thens = stmtIf.GetThens();
		auto thenIt = thens.begin();
		for (const ReferenceWrapper<const AST::Expression::Base>& pExpression : conditions)
		{
			Visit(*pExpression);
			Visit(*(*thenIt));
			++thenIt;
		}
		if (stmtIf.GetElse().IsValid())
		{
			Visit(*stmtIf.GetElse());
		}
	}

	void Resolver::Visit(const AST::Statement::Repeat& repeat)
	{
		BeginScope();
		for (const ReferenceWrapper<const AST::Statement::Base>& pStatement : repeat.GetStatements())
		{
			Visit(*pStatement);
		}
		Visit(*repeat.GetCondition());
		EndScope();
	}

	void Resolver::Visit(const AST::Statement::Return& stmtReturn)
	{
		const AST::Expressions& exprlist = stmtReturn.GetExpressions();
		for (const ReferenceWrapper<const AST::Expression::Base>& pExpression : exprlist)
		{
			Visit(*pExpression);
		}
	}

	void Resolver::Visit(const AST::Statement::While& stmtWhile)
	{
		Visit(*stmtWhile.GetCondition());
		Visit(*stmtWhile.GetBody());
	}

	void Resolver::Visit(const AST::Expression::Assignment& assignment)
	{
		const AST::Expressions& exprlist = assignment.GetExpressions();
		for (const ReferenceWrapper<const AST::Expression::Base>& pExpression : exprlist)
		{
			Visit(*pExpression);
		}

		const AST::Expressions& varlist = assignment.GetVariables();
		for (const ReferenceWrapper<const AST::Expression::Base>& pVariable : varlist)
		{
			switch (pVariable->GetType())
			{
				case AST::NodeType::Variable:
				{
					const AST::Expression::Variable& variable = static_cast<const AST::Expression::Variable&>(*pVariable);
					if (Optional<const AST::Expression::Base*> pObject = variable.GetObject())
					{
						Visit(*pObject);
					}
					if (Optional<const AST::Expression::Base*> pIndex = variable.GetIndex())
					{
						Visit(*pIndex);
					}
					ResolveLocal(uintptr(&variable), variable.GetIdentifier().identifier);
				}
				break;
				case AST::NodeType::VariableDeclaration:
				{
					const AST::Expression::VariableDeclaration& variableDeclaration =
						static_cast<const AST::Expression::VariableDeclaration&>(*pVariable);
					if (variableDeclaration.IsLocal())
					{
						Declare(variableDeclaration.GetIdentifier().identifier);
						Define(variableDeclaration.GetIdentifier().identifier);
					}
					ResolveLocal(uintptr(&variableDeclaration), variableDeclaration.GetIdentifier().identifier);
				}
				break;
				default:
					ExpectUnreachable();
			}
		}
	}

	void Resolver::Visit(const AST::Expression::Binary& binary)
	{
		Visit(*binary.GetLeft());
		Visit(*binary.GetRight());
	}

	void Resolver::Visit(const AST::Expression::Call& call)
	{
		Visit(*call.GetCallee());
		const AST::Expressions& arglist = call.GetArguments();
		for (const ReferenceWrapper<const AST::Expression::Base>& pArgExpression : arglist)
		{
			Visit(*pArgExpression);
		}
	}

	void Resolver::Visit(const AST::Expression::Function& function)
	{
		BeginScope();
		for (const Token& parameter : function.GetParameters())
		{
			Declare(parameter.identifier);
			Define(parameter.identifier);
		}
		for (const ReferenceWrapper<const AST::Statement::Base>& pStatement : function.GetStatements())
		{
			Visit(*pStatement);
		}
		EndScope();
	}

	void Resolver::Visit(const AST::Expression::Group& group)
	{
		const AST::Expressions& expressions = group.GetExpressions();
		for (const ReferenceWrapper<const AST::Expression::Base>& pExpression : expressions)
		{
			Visit(*pExpression);
		}
	}

	void Resolver::Visit(const AST::Expression::Literal&)
	{
	}

	void Resolver::Visit(const AST::Expression::Logical& logical)
	{
		Visit(*logical.GetLeft());
		Visit(*logical.GetRight());
	}

	void Resolver::Visit(const AST::Expression::Unary& unary)
	{
		Visit(*unary.GetRight());
	}

	void Resolver::Visit(const AST::Expression::VariableDeclaration& variableDeclaration)
	{
		ResolveLocal(uintptr(&variableDeclaration), variableDeclaration.GetIdentifier().identifier);
	}

	void Resolver::Visit(const AST::Expression::Variable& variable)
	{
		if (const Optional<const AST::Expression::Base*> pObject = variable.GetObject())
		{
			Visit(*pObject);
			if (const Optional<const AST::Expression::Base*> pIndex = variable.GetIndex())
			{
				Visit(*pIndex);
			}
		}
		ResolveLocal(uintptr(&variable), variable.GetIdentifier().identifier);
	}

	void Resolver::BeginScope()
	{
		m_state.scopes.EmplaceBack();
	}

	void Resolver::EndScope()
	{
		m_state.scopes.PopBack();
	}

	void Resolver::Declare(const Guid identifier)
	{
		if (m_state.scopes.HasElements())
		{
			DefinedVariableMap& map = m_state.scopes.GetLastElement();
			auto elementIt = map.Find(identifier);
			if (elementIt == map.end())
			{
				map.Emplace(Guid(identifier), false);
			}
		}
	}

	void Resolver::Define(const Guid identifier)
	{
		if (m_state.scopes.HasElements())
		{
			DefinedVariableMap& map = m_state.scopes.GetLastElement();
			auto elementIt = map.Find(identifier);
			Assert(elementIt != map.end(), "Declare before Define");
			if (elementIt != map.end())
			{
				elementIt->second = true;
			}
		}
	}

	void Resolver::ResolveLocal(uintptr astNode, const Guid guid)
	{
		const int32 scopeSize = m_state.scopes.GetSize() - 1;
		for (int32 index = scopeSize; index >= 0; --index)
		{
			auto astNodeIt = m_state.scopes[index].Find(guid);
			if (astNodeIt != m_state.scopes[index].end())
			{
				m_state.pResolvedVariables->Emplace(astNode, scopeSize - index);
				break;
			}
		}
	}
}
