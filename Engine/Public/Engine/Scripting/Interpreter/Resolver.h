#pragma once

#include "Engine/Scripting/Parser/StringType.h"

#include "Engine/Scripting/Parser/AST/VisitNode.h"

#include "Engine/Scripting/Interpreter/Environment.h"

#include <Common/Memory/ForwardDeclarations/Optional.h>

#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/StringView.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Math/HashedObject.h>

namespace ngine::Scripting
{
	using ResolvedVariableMap = UnorderedMap<uintptr, uint32>;
	class Resolver final : public AST::NodeVisitor<Resolver>
	{
	public:
		Resolver();
		SharedPtr<ResolvedVariableMap> Resolve(const AST::Statement::Base& statement, Optional<const Environment&> pParentEnvironment = {});
		SharedPtr<ResolvedVariableMap> Resolve(const AST::Expression::Function& function, Optional<const Environment&> pParentEnvironment = {});

		using NodeVisitor::Visit;
		void Visit(const AST::Statement::Block&);
		void Visit(const AST::Statement::Break&);
		void Visit(const AST::Statement::Expression&);
		void Visit(const AST::Statement::If&);
		void Visit(const AST::Statement::Repeat&);
		void Visit(const AST::Statement::Return&);
		void Visit(const AST::Statement::While&);
		void Visit(const AST::Expression::Assignment&);
		void Visit(const AST::Expression::Binary&);
		void Visit(const AST::Expression::Call&);
		void Visit(const AST::Expression::Function&);
		void Visit(const AST::Expression::Group&);
		void Visit(const AST::Expression::Literal&);
		void Visit(const AST::Expression::Logical&);
		void Visit(const AST::Expression::Unary&);
		void Visit(const AST::Expression::VariableDeclaration&);
		void Visit(const AST::Expression::Variable&);
	private:
		void BeginScope();
		void EndScope();

		void Declare(const Guid guid);
		void Define(const Guid guid);

		void ResolveLocal(uintptr astNode, const Guid guid);
	private:
		using DefinedVariableMap = UnorderedMap<Guid, bool, Guid::Hash>;
		struct State
		{
			SharedPtr<ResolvedVariableMap> pResolvedVariables;
			Vector<DefinedVariableMap> scopes;
		};
		State m_state;
	};
}
