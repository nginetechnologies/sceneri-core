#pragma once

#include "Engine/Scripting/Parser/StringType.h"

#include "Engine/Scripting/Parser/AST/VisitNode.h"

#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>
#include <Common/Memory/Containers/String.h>

#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::Scripting
{
	struct Unparser : public AST::NodeVisitor<Unparser>
	{
	public:
		static constexpr uint32 Version = 1;
		static constexpr int32 SpacesPerTab = 4;
		enum class Flags : uint8
		{
			LineStart = 1 << 0,
			SilentBlock = 1 << 1
		};
	public:
		Unparser();
		StringType Unparse(const AST::Statement::Base& statement);
		StringType Unparse(const AST::Expression::Base& expression);

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
		void Line();
		void Write(StringType::ConstView text);
	private:
		struct State
		{
			StringType buffer;
			int32 depth;
			EnumFlags<Flags> flags;
		};
		State m_state;
	};

	ENUM_FLAG_OPERATORS(Unparser::Flags);
}
