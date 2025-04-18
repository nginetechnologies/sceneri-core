#include "Engine/Scripting/Compiler/Compiler.h"

#include "Engine/Scripting/Compiler/Chunk.h"
#include "Engine/Scripting/Compiler/Opcode.h"
#include "Engine/Scripting/Parser/AST/Statement.h"
#include "Engine/Scripting/Parser/AST/Expression.h"

#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/ComponentSoftReference.h>
#include <Engine/Entity/Manager.h>
#include <Engine/Tag/TagIdentifier.h>
#include <Engine/Tag/TagRegistry.h>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/Stage/SceneRenderStageIdentifier.h>
#include <Renderer/Assets/Stage/SceneRenderStageGuid.h>
#include <Renderer/Assets/Texture/TextureGuid.h>

#include <Common/Asset/Reference.h>
#include <Common/Memory/Variant.h>
#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>
#include <Common/Math/Angle3.h>
#include <Common/Math/Color.h>
#include <Common/Reflection/GenericType.h>
#include <Common/Platform/Unreachable.h>
#include <Common/Time/Timestamp.h>
#include <Common/IO/Log.h>
#include <Common/Reflection/Registry.h>

namespace ngine::Scripting
{
	// coarity = 0: function specifies how many return values are saved
	// coarity > 0: coarity - 1 return values are saved
	Compiler::ScopedCallCoarity::ScopedCallCoarity(Compiler& compiler, uint8 coarity)
		: m_compiler(compiler)
		, m_coarity(compiler.m_state.expectedCoarity)
	{
		m_compiler.m_state.expectedCoarity = coarity;
	}

	Compiler::ScopedCallCoarity::~ScopedCallCoarity()
	{
		m_compiler.m_state.expectedCoarity = m_coarity;
	}

	uint8 Compiler::ScopedCallCoarity::GetPrevious() const
	{
		return m_coarity;
	}

	Compiler::Compiler()
	{
	}

	void Compiler::Initialize(FunctionObject& function, FunctionType type)
	{
		m_state.pFunction = &function;
		m_state.pEnclosing = nullptr;

		m_state.localCount = 0;
		m_state.expectedCoarity = 0;
		m_state.scopeDepth = 0;
		m_state.loopDepth = 0;
		m_state.flags.Clear();
		m_state.functionType = type;

		UpdateSourceLocation(function.chunk.debugInfo.HasElements() ? function.chunk.debugInfo[0].sourceLocation : SourceLocation{});

		LocalVariable& localVariable = m_state.locals[m_state.localCount++];
		localVariable.depth = 0;
		localVariable.isCaptured = false;
	}

	UniquePtr<FunctionObject> Compiler::Compile(const AST::Statement::Base& statement)
	{
		GC gc{&SimpleReallocate, nullptr, 0};
		FunctionObject* pFunction = CreateFunctionObject(gc);
		pFunction->id = FunctionObjectScriptId;

		Initialize(*pFunction, FunctionType::Script);

		Visit(statement);

		if (!m_state.flags.IsSet(Flags::HasReturn))
		{
			EmitByte(uint8(OpCode::Return));
		}
		return m_state.flags.IsSet(Flags::Error) ? nullptr : UniquePtr<FunctionObject>::FromRaw(pFunction);
	}

	UniquePtr<FunctionObject> Compiler::Compile(const AST::Expression::Function& function)
	{
		GC gc{&SimpleReallocate, nullptr, 0};
		FunctionObject* pFunction = CreateFunctionObject(gc);
		pFunction->id = FunctionObjectScriptId;

		Initialize(*pFunction, FunctionType::Function);

		if (!CompileFunction(function))
		{
			Error("Failed to compile function");
		}

		return m_state.flags.IsSet(Flags::Error) ? nullptr : UniquePtr<FunctionObject>::FromRaw(pFunction);
	}

	void Compiler::Visit(const AST::Statement::Block& block)
	{
		BeginScope();
		for (const ReferenceWrapper<const AST::Statement::Base>& pStatement : block.GetStatements())
		{
			Visit(*pStatement);
		}
		EndScope();
	}

	void Compiler::Visit(const AST::Statement::Break&)
	{
		const int32 scopeDepth = m_state.loopDepth;
		const int32 jumpOffset = EmitJump(uint8(OpCode::Jump));

		uint32 localCount = 0;
		uint32 localIndex = m_state.localCount;
		while (m_state.localCount > 0 && m_state.locals[localIndex - 1].depth >= scopeDepth)
		{
			++localCount;
			--localIndex;
		}

		Breakpoint breakpoint{jumpOffset, scopeDepth, localCount};
		m_state.breakpoints.EmplaceBack(Move(breakpoint));
	}

	void Compiler::Visit(const AST::Statement::Expression& expression)
	{
		Visit(*expression.GetExpression());

		if (m_state.flags.IsSet(Flags::SkipPop))
		{
			m_state.flags.Clear(Flags::SkipPop);
		}
		else
		{
			EmitByte(uint8(OpCode::Pop));
		}
	}

	void Compiler::Visit(const AST::Statement::If& stmtIf)
	{
		const AST::Expressions& conditions = stmtIf.GetConditions();
		const AST::Statements& thens = stmtIf.GetThens();

		Vector<int32> offsets;
		auto thenIt = thens.begin();
		for (const ReferenceWrapper<const AST::Expression::Base>& pCondition : conditions)
		{
			Visit(*pCondition);

			const int32 thenJump = EmitJump(uint8(OpCode::JumpIfFalse));
			EmitByte(uint8(OpCode::Pop));

			Visit(*thenIt);
			const int32 endOffset = EmitJump(uint8(OpCode::Jump));
			offsets.EmplaceBack(endOffset);
			PatchJump(thenJump);
			EmitByte(uint8(OpCode::Pop));

			++thenIt;
		}

		if (stmtIf.GetElse().IsValid())
		{
			Visit(*stmtIf.GetElse());
		}

		for (int32 offset : offsets)
		{
			PatchJump(offset);
		}
	}

	void Compiler::Visit(const AST::Statement::Repeat& repeat)
	{
		const int32 loopDepth = m_state.loopDepth;
		m_state.loopDepth = m_state.scopeDepth + 1;

		BeginScope();

		const int32 loopStart = GetCurrentChunk().code.GetSize();

		const AST::Statements& statements = repeat.GetStatements();
		for (const ReferenceWrapper<const AST::Statement::Base>& pStatement : statements)
		{
			Visit(*pStatement);
		}
		Visit(*repeat.GetCondition());

		const int32 loopEnd = EmitJump(uint8(OpCode::JumpIfTrue));

		EmitByte(uint8(OpCode::Pop)); // until condition
		CleanScope(m_state.scopeDepth - 1, false);

		EmitLoop(uint8(OpCode::Jump), loopStart);
		PatchJump(loopEnd);

		EmitByte(uint8(OpCode::Pop)); // until condition
		EndScope();

		m_state.loopDepth = loopDepth;
	}

	void Compiler::Visit(const AST::Statement::Return& stmtReturn)
	{
		UpdateSourceLocation(stmtReturn.GetSourceLocation());

		if (m_state.functionType == FunctionType::Script)
		{
			Error("Can't return from top level script");
			return;
		}

		const AST::Expressions& expressions = stmtReturn.GetExpressions();
		const uint32 exprCount = expressions.GetSize();

		uint32 curExprCount = 0;
		auto expressionIt = expressions.begin();
		while (expressionIt != expressions.end())
		{
			const bool canReturnMultiple = exprCount == 1 || curExprCount == exprCount - 1;
			ScopedCallCoarity callCoarity(*this, canReturnMultiple ? 0 : 2);

			Visit(*expressionIt);
			++expressionIt;
			++curExprCount;
		}
		m_state.flags.Set(Flags::HasReturn);

		EmitByte(uint8(OpCode::Return));
	}

	void Compiler::Visit(const AST::Statement::While& stmtWhile)
	{
		const int32 loopDepth = m_state.loopDepth;
		m_state.loopDepth = m_state.scopeDepth + 1;

		const int32 loopStart = GetCurrentChunk().code.GetSize();

		Visit(*stmtWhile.GetCondition());

		const int32 loopEnd = EmitJump(uint8(OpCode::JumpIfFalse));
		EmitByte(uint8(OpCode::Pop));

		BeginScope();

		// We need control over the block in order for breaks to work
		// Visit(*stmtWhile.GetBody());
		const AST::Statement::Block& block = static_cast<const AST::Statement::Block&>(*stmtWhile.GetBody());
		const AST::Statements& statements = block.GetStatements();
		for (const ReferenceWrapper<const AST::Statement::Base>& pStatement : statements)
		{
			Visit(*pStatement);
		}

		// TODO(Ben): We could optimize this by only emitting second cleanup if there is a break inside the loop
		CleanScope(m_state.scopeDepth - 1, false);
		EmitLoop(uint8(OpCode::Jump), loopStart);

		PatchJump(loopEnd);
		EmitByte(uint8(OpCode::Pop));

		EndScope();

		m_state.loopDepth = loopDepth;
	}

	void Compiler::Visit(const AST::Expression::Assignment& assignment)
	{
		const AST::Expressions& varlist = assignment.GetVariables();
		const AST::Expressions& exprlist = assignment.GetExpressions();

		const uint32 varCount = varlist.GetSize();
		const uint32 exprCount = exprlist.GetSize();

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

		// In case of recursive functions and self assignment
		// we need to make sure to declare local variables already
		// this shouldn't emit any code and therefore can be done before
		if (isLocal)
		{
			m_state.flags.Set(Flags::Local | Flags::Assignment);

			auto varIt = varlist.begin();
			while (varIt != varlist.end())
			{
				Visit(*varIt);
				++varIt;
			}

			m_state.flags.Clear(Flags::SkipPop | Flags::Assignment | Flags::Local);
		}

		uint32 curExprCount = 0;
		auto exprIt = exprlist.begin();
		for (uint32 count = 0; count < varCount; ++count)
		{
			if (exprIt != exprlist.end())
			{
				if ((*exprIt)->GetType() == AST::NodeType::Call)
				{
					// if this is the only or last expression instruct function to return enough values
					const bool returnMultiple = exprCount == 1 || curExprCount + 1 == exprCount;
					ScopedCallCoarity callCoarity(*this, returnMultiple ? uint8(varCount - curExprCount + 1) : 2);

					Visit(*exprIt);
					curExprCount = varCount;
				}
				else if ((*exprIt)->GetType() == AST::NodeType::Function)
				{
					const AST::Expression::Base& variableExpression = *varlist.GetView()[count];
					switch (variableExpression.GetType())
					{
						case AST::NodeType::VariableDeclaration:
						{
							const AST::Expression::VariableDeclaration& variable =
								static_cast<const AST::Expression::VariableDeclaration&>(variableExpression);
							m_state.m_localFunctionsSet.Emplace(Guid{variable.GetIdentifier().identifier});
						}
						break;
						case AST::NodeType::Variable:
						{
							const AST::Expression::Variable& variable = static_cast<const AST::Expression::Variable&>(variableExpression);
							m_state.m_localFunctionsSet.Emplace(Guid{variable.GetIdentifier().identifier});
						}
						break;
						default:
							ExpectUnreachable();
					}

					Visit(*exprIt);
					++curExprCount;
				}
				else
				{
					Visit(*exprIt);
					++curExprCount;
				}
				++exprIt;
			}
			else if (exprCount == 0 || curExprCount < varCount)
			{
				// No expression can produce values, fill with nil
				EmitByte(uint8(OpCode::Null));
				++curExprCount;
			}
		}

		// If there are expressions left evaluate them and throw away the result
		while (exprIt != exprlist.end())
		{
			ScopedCallCoarity callCoarity(*this, 2);
			Visit(*exprIt);
			EmitByte(uint8(OpCode::Pop));
			++exprIt;
		}

		if (isLocal)
		{
			// We already declared locals before expressions
			m_state.flags.Set(Flags::SkipPop);
		}
		else
		{
			m_state.flags.Set(Flags::Assignment);

			// global values pop from the stack switch order
			auto varIt = varlist.end() - 1;
			auto endIt = varlist.begin() - 1;
			while (varIt != endIt)
			{
				Visit(*varIt);
				--varIt;
				if (varIt != endIt)
				{
					// values for globals need to be consumed, except if it is the last
					// because expression statements already pop after finished
					EmitByte(uint8(OpCode::Pop));
				}
			}

			m_state.flags.Clear(Flags::Assignment);
		}
	}

	void Compiler::Visit(const AST::Expression::Binary& binary)
	{
		UpdateSourceLocation(binary.GetSourceLocation());

		ScopedCallCoarity callCoarity(*this, 2);
		Visit(*binary.GetLeft());
		Visit(*binary.GetRight());

		switch (binary.GetOperator().type)
		{
			case TokenType::Plus:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::AddInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::AddFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't add non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Minus:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::SubtractInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::SubtractFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't subtract non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Star:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::MultiplyInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::MultiplyFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't multiply non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Slash:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::DivideInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::DivideFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't divide non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Atan2:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::Atan2Float));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't add non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Max:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::MaxInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::MaxFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't add non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Min:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::MinInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::MinFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't add non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Random:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::RandomInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::RandomFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't add non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Power:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::PowerFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't add non-numeric types");
						break;
				}
			}
			break;
			case TokenType::AreNearlyEqual:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::Null:
						EmitByte(uint8(OpCode::EqualEqualInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::AreNearlyEqualFloat));
						break;
					case PrimitiveType::String:
						EmitByte(uint8(OpCode::EqualEqualString));
						break;
					case PrimitiveType::Any:
						Error("Can't compare non-numeric types");
						break;
				}
			}
			break;

			case TokenType::Less:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::LessInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::LessFloat));
						break;
					case PrimitiveType::String:
						EmitByte(uint8(OpCode::LessString));
						break;

					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't compare non-numeric types");
						break;
				}
			}
			break;
			case TokenType::LessEqual:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::LessEqualInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::LessEqualFloat));
						break;
					case PrimitiveType::String:
						EmitByte(uint8(OpCode::LessEqualString));
						break;

					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't compare non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Greater:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::GreaterInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::GreaterFloat));
						break;
					case PrimitiveType::String:
						EmitByte(uint8(OpCode::GreaterString));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't compare non-numeric types");
						break;
				}
			}
			break;
			case TokenType::GreaterEqual:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::GreaterEqualInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::GreaterEqualFloat));
						break;
					case PrimitiveType::String:
						EmitByte(uint8(OpCode::GreaterEqualString));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't compare non-numeric types");
						break;
				}
			}
			break;
			case TokenType::NotEqual:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						EmitByte(uint8(OpCode::NotEqualInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::NotEqualFloat));
						break;
					case PrimitiveType::String:
						EmitByte(uint8(OpCode::NotEqualString));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
						Error("Can't compare non-numeric types");
						break;
				}
			}
			break;
			case TokenType::EqualEqual:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::Null:
						EmitByte(uint8(OpCode::EqualEqualInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::EqualEqualFloat));
						break;
					case PrimitiveType::String:
						EmitByte(uint8(OpCode::EqualEqualString));
						break;
					case PrimitiveType::Any:
						Error("Can't compare non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Percent:
			case TokenType::Mod:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::ModuloInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::ModuloFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't compare non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Dot:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Cast
						Error("Can't dot integers");
						break;
					case PrimitiveType::Float2:
						EmitByte(uint8(OpCode::Dot2));
						break;
					case PrimitiveType::Float3:
						EmitByte(uint8(OpCode::Dot3));
						break;
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::Dot4));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't get dot product of non-vector types");
						break;
				}
			}
			break;
			case TokenType::Cross:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Cast
						Error("Can't cross integers");
						break;
					case PrimitiveType::Float2:
						EmitByte(uint8(OpCode::Cross2));
						break;
					case PrimitiveType::Float3:
						EmitByte(uint8(OpCode::Cross3));
						break;
					case PrimitiveType::Float4:
						Error("Can't cross 4D vector");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't get cross product of non-vector types");
						break;
				}
			}
			break;
			case TokenType::Distance:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Float:
						break;
					case PrimitiveType::Integer2:
						EmitByte(uint8(OpCode::DistanceInteger2));
						break;
					case PrimitiveType::Integer3:
						EmitByte(uint8(OpCode::DistanceInteger3));
						break;
					case PrimitiveType::Float2:
						EmitByte(uint8(OpCode::DistanceFloat2));
						break;
					case PrimitiveType::Float3:
						EmitByte(uint8(OpCode::DistanceFloat3));
						break;
					case PrimitiveType::Float4:
					case PrimitiveType::Integer4:
						Error("Can't distance 4D vector");
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't get cross product of non-vector types");
						break;
				}
			}
			break;
			case TokenType::Project:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Cast
						Error("Can't project integers");
						break;
					case PrimitiveType::Float2:
						EmitByte(uint8(OpCode::Project2));
						break;
					case PrimitiveType::Float3:
						EmitByte(uint8(OpCode::Project3));
						break;
					case PrimitiveType::Float4:
						Error("Can't project 4D vector");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't project non-vector types");
						break;
				}
			}
			break;
			case TokenType::Reflect:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Cast
						Error("Can't reflect integers");
						break;
					case PrimitiveType::Float2:
						EmitByte(uint8(OpCode::Reflect2));
						break;
					case PrimitiveType::Float3:
						EmitByte(uint8(OpCode::Reflect3));
						break;
					case PrimitiveType::Float4:
						Error("Can't reflect 4D vector");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't reflect non-vector types");
						break;
				}
			}
			break;
			case TokenType::Refract:
			{
				switch (binary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Cast
						Error("Can't refract integers");
						break;
					case PrimitiveType::Float2:
						EmitByte(uint8(OpCode::Refract2));
						break;
					case PrimitiveType::Float3:
						EmitByte(uint8(OpCode::Refract3));
						break;
					case PrimitiveType::Float4:
						Error("Can't refract 4D vector");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't refract non-vector types");
						break;
				}
			}
			break;
			case TokenType::LessLess:
				Assert(binary.GetPrimitiveType() == PrimitiveType::Integer || binary.GetPrimitiveType() == PrimitiveType::Integer4);
				if (binary.GetPrimitiveType() != PrimitiveType::Integer && binary.GetPrimitiveType() != PrimitiveType::Integer4)
				{
					Error("Left shift is only possible on integral types");
				}
				EmitByte(uint8(OpCode::LeftShiftInteger));
				break;
			case TokenType::GreaterGreater:
				Assert(binary.GetPrimitiveType() == PrimitiveType::Integer || binary.GetPrimitiveType() == PrimitiveType::Integer4);
				if (binary.GetPrimitiveType() != PrimitiveType::Integer && binary.GetPrimitiveType() != PrimitiveType::Integer4)
				{
					Error("Right shift is only possible on integral types");
				}
				EmitByte(uint8(OpCode::RightShiftInteger));
				break;

			case TokenType::Ampersand:
				Assert(
					binary.GetPrimitiveType() == PrimitiveType::Integer || binary.GetPrimitiveType() == PrimitiveType::Integer4 ||
					binary.GetPrimitiveType() == PrimitiveType::Boolean || binary.GetPrimitiveType() == PrimitiveType::Boolean4
				);
				if (binary.GetPrimitiveType() != PrimitiveType::Integer && binary.GetPrimitiveType() != PrimitiveType::Integer4 && binary.GetPrimitiveType() != PrimitiveType::Boolean && binary.GetPrimitiveType() != PrimitiveType::Boolean4)
				{
					Error("AND is only possible on integral types");
				}
				EmitByte(uint8(OpCode::AndInteger));
				break;
			case TokenType::Pipe:
				Assert(
					binary.GetPrimitiveType() == PrimitiveType::Integer || binary.GetPrimitiveType() == PrimitiveType::Integer4 ||
					binary.GetPrimitiveType() == PrimitiveType::Boolean || binary.GetPrimitiveType() == PrimitiveType::Boolean4
				);
				if (binary.GetPrimitiveType() != PrimitiveType::Integer && binary.GetPrimitiveType() != PrimitiveType::Integer4 && binary.GetPrimitiveType() != PrimitiveType::Boolean && binary.GetPrimitiveType() != PrimitiveType::Boolean4)
				{
					Error("OR is only possible on integral types");
				}
				EmitByte(uint8(OpCode::OrInteger));
				break;
			case TokenType::Circumflex:
				Assert(
					binary.GetPrimitiveType() == PrimitiveType::Integer || binary.GetPrimitiveType() == PrimitiveType::Integer4 ||
					binary.GetPrimitiveType() == PrimitiveType::Boolean || binary.GetPrimitiveType() == PrimitiveType::Boolean4
				);
				if (binary.GetPrimitiveType() != PrimitiveType::Integer && binary.GetPrimitiveType() != PrimitiveType::Integer4 && binary.GetPrimitiveType() != PrimitiveType::Boolean && binary.GetPrimitiveType() != PrimitiveType::Boolean4)
				{
					Error("Exclusive Or is only possible on integral types");
				}
				EmitByte(uint8(OpCode::ExclusiveOrInteger));
				break;
			default:
				ExpectUnreachable();
		}
	}

	void Compiler::Visit(const AST::Expression::Call& call)
	{
		UpdateSourceLocation(call.GetSourceLocation());

		Visit(*call.GetCallee());

		Optional<const Reflection::FunctionInfo*> pFunctionInfo;

		uint8 functionConstantOpcodeOffset{2};
		bool hasObjectArgument{false};
		bool isNativeFunction{false};
		switch (call.GetCallee()->GetType())
		{
			case AST::NodeType::VariableDeclaration:
				break;
			case AST::NodeType::Variable:
			{
				const AST::Expression::Variable& calleeVariable = static_cast<const AST::Expression::Variable&>(*call.GetCallee());

				const Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
				pFunctionInfo = reflectionRegistry.FindFunctionDefinition(calleeVariable.GetIdentifier().identifier);

				isNativeFunction = pFunctionInfo.IsValid() && pFunctionInfo->m_flags.IsNotSet(Reflection::FunctionFlags::IsScript);

				if (calleeVariable.GetObject().IsValid())
				{
					Visit(*calleeVariable.GetObject());

					switch (calleeVariable.GetObject()->GetType())
					{
						case AST::NodeType::VariableDeclaration:
						case AST::NodeType::Variable:
							functionConstantOpcodeOffset += 2; // Two opcodes from pushing the object
							break;
						case AST::NodeType::Literal:
						{
							const AST::Expression::Literal& literal = static_cast<const AST::Expression::Literal&>(*calleeVariable.GetObject());
							if (literal.GetValueType().Is<Entity::ComponentSoftReference>())
							{
								functionConstantOpcodeOffset += 2;
							}
							else
							{
								functionConstantOpcodeOffset += 1;
							}
						}
						break;
						default:
							ExpectUnreachable();
					}
					hasObjectArgument = true;
				}
			}
			break;
			default:
				ExpectUnreachable();
		}

		const Chunk& chunk = GetCurrentChunk();
		[[maybe_unused]] const OpCode functionConstantOpCode = (OpCode) * (chunk.code.end() - functionConstantOpcodeOffset);
		Assert(
			functionConstantOpCode == OpCode::PushConstant || functionConstantOpCode == OpCode::PushGlobal ||
			functionConstantOpCode == OpCode::PushLocal || functionConstantOpCode == OpCode::PushUpvalue ||
			functionConstantOpCode == OpCode::PushImmediate
		);
		[[maybe_unused]] const uint8 functionConstantIndex = *(chunk.code.end() - (functionConstantOpcodeOffset - 1));

		// TODO(Ben): Implement upport for varargs here
		ScopedCallCoarity callCoarity(*this, 1);

		uint16 argCount{hasObjectArgument};
		const AST::Expressions& expressions = call.GetArguments();

		if (pFunctionInfo.IsValid())
		{
			ArrayView<const Reflection::Argument> functionArguments = pFunctionInfo->m_getArgumentsFunction(*pFunctionInfo);
			if (pFunctionInfo->m_flags.IsNotSet(Reflection::FunctionFlags::IsMemberFunction))
			{
				functionArguments += hasObjectArgument;
			}
			for (const Reflection::Argument& functionArgument : functionArguments)
			{
				const uint32 argumentIndex = functionArguments.GetIteratorIndex(&functionArgument);
				if (argumentIndex >= expressions.GetSize() || expressions.GetView()[argumentIndex]->GetType() == AST::NodeType::Group)
				{
					Error("Must specify all arguments for native functions");
					return;
				}
			}
		}

		auto expressionIt = expressions.begin();
		while (expressionIt != expressions.end())
		{
			Visit(*expressionIt);
			++argCount;
			if (argCount > 0xFF)
			{
				Error("Can't compile more than 255 arguments");
				break;
			}
			++expressionIt;
		}

		Assert(!isNativeFunction || chunk.constantTypes[functionConstantIndex] == ValueType::NativeFunctionGuid);
		EmitByte(uint8(isNativeFunction ? OpCode::CallNative : OpCode::CallClosure));

		EmitByte(uint8(argCount));
		EmitByte(callCoarity.GetPrevious());
	}

	bool Compiler::CompileFunction(const AST::Expression::Function& function)
	{
		BeginScope();
		{
			const AST::Expression::Function::Parameters& parameters = function.GetParameters();
			auto tokensIt = parameters.begin();
			while (tokensIt != parameters.end())
			{
				AddLocalVariable(tokensIt->identifier);
				++tokensIt;
				++m_state.pFunction->arity;
			}
		}
		{
			const AST::Expression::Function::ReturnTypes& returnTypes = function.GetReturnTypes();
			auto tokensIt = returnTypes.begin();
			while (tokensIt != returnTypes.end())
			{
				tokensIt++;
				++m_state.pFunction->coarity;
			}
		}

		const AST::Statements& body = function.GetStatements();
		for (const ReferenceWrapper<const AST::Statement::Base>& pStatement : body)
		{
			Visit(*pStatement);
		}

		if (!m_state.flags.IsSet(Flags::HasReturn))
		{
			EmitByte(uint8(OpCode::Return));
		}

		m_state.pFunction->locals = uint8(m_state.localCount - 1);
		return !m_state.flags.IsSet(Flags::Error);
	}

	void Compiler::Visit(const AST::Expression::Function& function)
	{
		GC gc{&SimpleReallocate, &m_state.pFunction->chunk.pObjects, 0};
		FunctionObject* pFunction = CreateFunctionObject(gc);

		Compiler compiler;
		compiler.Initialize(*pFunction, FunctionType::Function);
		compiler.m_state.pEnclosing = this;

		if (compiler.CompileFunction(function))
		{
			uint8 depth = 0;
			const Compiler* pCompiler = &compiler;
			while (pCompiler->m_state.pEnclosing)
			{
				pCompiler = pCompiler->m_state.pEnclosing;
				++depth;
			}

			const Optional<Chunk::ConstantIndex> index = AddConstant(Value((Object*)pFunction));
			if (index.IsValid())
			{
				static_assert(TypeTraits::IsSame<Chunk::ConstantIndex, uint8>, "TODO: Support for more constants");
				pFunction->id = uint16((uint16(depth - uint8(1)) << uint16(8)) | uint8(*index));

				EmitByte(uint8(OpCode::PushClosure));
				EmitByte(uint8(*index));

				for (uint32 upvalueIndex = 0; upvalueIndex < pFunction->upvalues; ++upvalueIndex)
				{
					EmitByte(compiler.m_state.upvalues[upvalueIndex].isLocal ? uint8(1) : uint8(0));
					EmitByte(compiler.m_state.upvalues[upvalueIndex].index);
				}
			}
		}
		else
		{
			Error("Failed to compile function");
		}
	}

	void Compiler::Visit(const AST::Expression::Group& group)
	{
		const AST::Expressions& expressions = group.GetExpressions();
		for (const ReferenceWrapper<const AST::Expression::Base>& pExpression : expressions)
		{
			Visit(*pExpression);
		}
	}

	void Compiler::Visit(const AST::Expression::Literal& literal)
	{
		const ConstAnyView value = literal.GetValue();
		Optional<Chunk::ConstantIndex> index;
		if (value.Is<nullptr_type>())
		{
			EmitByte(uint8(OpCode::Null));
		}
		else if (value.Is<bool>())
		{
			EmitByte(uint8(value.GetExpected<bool>() ? OpCode::True : OpCode::False));
		}
		else if (value.Is<Math::Vector4f::BoolType>())
		{
			index = AddConstant(Value(value.GetExpected<Math::Vector4f::BoolType>()));
		}
		else if (value.Is<Math::Vector4i::BoolType>())
		{
			index = AddConstant(Value(value.GetExpected<Math::Vector4i::BoolType>()));
		}
		else if (value.Is<IntegerType>())
		{
			const IntegerType integer = value.GetExpected<IntegerType>();
			if (integer >= 0 && integer <= 0xFF)
			{
				EmitByte(uint8(OpCode::PushImmediate));
				EmitByte(uint8(integer));
			}
			else
			{
				index = AddConstant(Value(integer));
			}
		}
		else if (value.Is<Math::Vector2i>())
		{
			index = AddConstant(Value(value.GetExpected<Math::Vector2i>()));
		}
		else if (value.Is<Math::Vector3i>())
		{
			index = AddConstant(Value(value.GetExpected<Math::Vector3i>()));
		}
		else if (value.Is<Math::Vector4i>())
		{
			index = AddConstant(Value(value.GetExpected<Math::Vector4i>()));
		}
		else if (value.Is<FloatType>())
		{
			index = AddConstant(Value(value.GetExpected<FloatType>()));
		}
		else if (value.Is<Math::Vector2f>())
		{
			index = AddConstant(Value(value.GetExpected<Math::Vector2f>()));
		}
		else if (value.Is<Math::Vector3f>())
		{
			index = AddConstant(Value(value.GetExpected<Math::Vector3f>()));
		}
		else if (value.Is<Math::Vector4f>())
		{
			index = AddConstant(Value(value.GetExpected<Math::Vector4f>()));
		}
		else if (value.Is<Math::Color>())
		{
			index = AddConstant(Value(value.GetExpected<Math::Color>()));
		}
		else if (value.Is<Math::Anglef>())
		{
			index = AddConstant(Value(value.GetExpected<Math::Anglef>()));
		}
		else if (value.Is<Math::Angle3f>())
		{
			index = AddConstant(Value(value.GetExpected<Math::Angle3f>()));
		}
		else if (value.Is<StringType>())
		{
			GC gc{&SimpleReallocate, &GetCurrentChunk().pObjects, 0};
			if (StringObject* const pStringObject = CreateStringObject(gc))
			{
				pStringObject->string = value.GetExpected<StringType>();
				index = AddConstant(Value((Object*)pStringObject));
			}
		}
		else if (value.Is<Guid>())
		{
			index = AddConstant(Value(value.GetExpected<Guid>()));
		}
		else if (value.Is<FunctionIdentifier>())
		{
			Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
			const Guid functionGuid = reflectionRegistry.FindFunctionGuid(value.GetExpected<FunctionIdentifier>());
			index = AddConstant(Value(functionGuid, ValueType::NativeFunctionGuid));
		}
		else if (value.Is<Tag::Guid>())
		{
			const Guid tagGuid = value.GetExpected<Tag::Guid>();
			index = AddConstant(Value(tagGuid, ValueType::TagGuid));
		}
		else if (value.Is<Tag::Identifier>())
		{
			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();
			const Guid tagGuid = tagRegistry.GetAssetGuid(value.GetExpected<Tag::Identifier>());
			index = AddConstant(Value(tagGuid, ValueType::TagGuid));
		}
		else if (value.Is<Rendering::StageGuid>())
		{
			const Guid stageGuid = value.GetExpected<Rendering::StageGuid>();
			index = AddConstant(Value(stageGuid, ValueType::RenderStageGuid));
		}
		else if (value.Is<Rendering::SceneRenderStageIdentifier>())
		{
			Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
			const Guid stageGuid = renderer.GetStageCache().GetAssetGuid(value.GetExpected<Rendering::SceneRenderStageIdentifier>());
			index = AddConstant(Value(stageGuid, ValueType::RenderStageGuid));
		}
		else if (value.Is<Asset::Guid>())
		{
			const Guid assetGuid = value.GetExpected<Asset::Guid>();
			index = AddConstant(Value(assetGuid, ValueType::AssetGuid));
		}
		else if (value.Is<Asset::Identifier>())
		{
			Asset::Manager& assetManager = System::Get<Asset::Manager>();
			const Guid assetGuid = assetManager.GetAssetGuid(value.GetExpected<Asset::Identifier>());
			index = AddConstant(Value(assetGuid, ValueType::AssetGuid));
		}
		else if (value.Is<Asset::Reference>())
		{
			const Guid assetGuid = value.GetExpected<Asset::Reference>().GetAssetGuid();
			index = AddConstant(Value(assetGuid, ValueType::AssetGuid));
		}
		else if (value.Is<Rendering::TextureGuid>())
		{
			const Guid assetGuid = value.GetExpected<Rendering::TextureGuid>();
			index = AddConstant(Value(assetGuid, ValueType::AssetGuid));
		}
		else if (value.Is<Rendering::TextureIdentifier>())
		{
			Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
			const Guid assetGuid = renderer.GetTextureCache().GetAssetGuid(value.GetExpected<Rendering::TextureIdentifier>());
			index = AddConstant(Value(assetGuid, ValueType::AssetGuid));
		}
		else if (value.Is<Entity::ComponentSoftReference>())
		{
			index = AddConstant(Value(value.GetExpected<Entity::ComponentSoftReference>()));
			EmitByte(uint8(OpCode::PushComponentSoftReference));
			EmitByte(uint8(*index));
			return;
		}
		else if (value.GetTypeDefinition().IsTriviallyCopyable())
		{
			const ConstByteView data{value.GetByteView()};
			if (data.GetDataSize() <= 16)
			{
				Value constant;
				constant.m_value = VM::DynamicInvoke::LoadDynamicArgumentZeroed(data);
				index = AddConstant(constant);
			}
			else
			{
				Error("Constants must be <=16 bytes");
			}
		}
		else
		{
			Error("Constants must be trivially copyable");
		}

		if (index.IsValid())
		{
			static_assert(TypeTraits::IsSame<Chunk::ConstantIndex, uint8>, "TODO: Support for more constants");
			EmitByte(uint8(OpCode::PushConstant));
			EmitByte(uint8(*index));
		}
	}

	void Compiler::Visit(const AST::Expression::Logical& logical)
	{
		UpdateSourceLocation(logical.GetSourceLocation());

		switch (logical.GetOperator().type)
		{
			case TokenType::And:
			{
				Visit(*logical.GetLeft());

				const OpCode jumpOpCode = OpCode::JumpIfFalse;
				const int32 endJump = EmitJump(uint8(jumpOpCode));
				EmitByte(uint8(OpCode::Pop));

				Visit(*logical.GetRight());

				PatchJump(endJump);
			}
			break;
			case TokenType::Or:
			{
				Visit(*logical.GetLeft());

				const OpCode jumpOpCode = OpCode::JumpIfTrue;
				const int32 endJump = EmitJump(uint8(jumpOpCode));
				EmitByte(uint8(OpCode::Pop));

				Visit(*logical.GetRight());

				PatchJump(endJump);
			}
			break;
			default:
				ExpectUnreachable();
		}
	}

	void Compiler::Visit(const AST::Expression::Unary& unary)
	{
		UpdateSourceLocation(unary.GetSourceLocation());

		const TokenType type = unary.GetOperator().type;
		switch (type)
		{
			case TokenType::Minus:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::NegateInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::NegateFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::String:
						Error("Can't negate non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Not:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
					case PrimitiveType::String:
						EmitByte(uint8(OpCode::TruthyNotInteger));
						break;
					case PrimitiveType::Null:
						EmitByte(uint8(OpCode::FalseyNotInteger));
						break;
					case PrimitiveType::Boolean:
						EmitByte(uint8(OpCode::LogicalNotBoolean));
						break;
					case PrimitiveType::Any:
						Error("Can't Not non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Exclamation:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						EmitByte(uint8(OpCode::LogicalNotInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::LogicalNotFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
						Error("Can't logical NOT non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Tilde:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						EmitByte(uint8(OpCode::BitwiseNotInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						Error("Can't bitwise NOT non-integral types");
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
						Error("Can't NOT non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Abs:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::AbsInteger));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::AbsFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Acos:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::AcosFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Asin:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::AsinFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Atan:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::AtanFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Ceil:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::CeilFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::CubicRoot:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::CubicRootFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Cos:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::CosFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Deg:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::RadiansToDegreesFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Exp:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::ExpFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Floor:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::FloorFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Round:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::RoundFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Fract:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::FractFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::InverseSqrt:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::InverseSqrtFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Log:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::LogFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Log2:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::Log2Float));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Log10:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::Log10Float));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::MultiplicativeInverse:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::MultiplicativeInverseFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Power2:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::Power2Float));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Power10:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::Power10Float));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Rad:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::DegreesToRadiansFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Sign:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::SignFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::SignNonZero:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::SignNonZeroFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Sin:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::SinFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Sqrt:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::SqrtFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Tan:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::TanFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Truncate:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Insert cast
						Error("Expected float");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::TruncateFloat));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't math non-numeric types");
						break;
				}
			}
			break;
			case TokenType::Length:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer2:
						EmitByte(uint8(OpCode::LengthInteger2));
						break;
					case PrimitiveType::Integer3:
						EmitByte(uint8(OpCode::LengthInteger3));
						break;
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::LengthInteger4));
						break;
					case PrimitiveType::Float2:
						EmitByte(uint8(OpCode::LengthFloat2));
						break;
					case PrimitiveType::Float3:
						EmitByte(uint8(OpCode::LengthFloat3));
						break;
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::LengthFloat4));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Integer:
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't get length of non-vector types");
						break;
				}
			}
			break;
			case TokenType::LengthSquared:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer2:
						EmitByte(uint8(OpCode::LengthSquaredInteger2));
						break;
					case PrimitiveType::Integer3:
						EmitByte(uint8(OpCode::LengthSquaredInteger3));
						break;
					case PrimitiveType::Integer4:
						EmitByte(uint8(OpCode::LengthSquaredInteger4));
						break;
					case PrimitiveType::Float2:
						EmitByte(uint8(OpCode::LengthSquaredFloat2));
						break;
					case PrimitiveType::Float3:
						EmitByte(uint8(OpCode::LengthSquaredFloat3));
						break;
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::LengthSquaredFloat4));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Integer:
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't get length of non-vector types");
						break;
				}
			}
			break;
			case TokenType::InverseLength:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer2:
						// TODO: Cast
						Error("Can't get inverse length of integral types");
						break;
					case PrimitiveType::Integer3:
						// TODO: Cast
						Error("Can't get inverse length of integral types");
						break;
					case PrimitiveType::Integer4:
						// TODO: Cast
						Error("Can't get inverse length of integral types");
						break;
					case PrimitiveType::Float2:
						EmitByte(uint8(OpCode::InverseLengthFloat2));
						break;
					case PrimitiveType::Float3:
						EmitByte(uint8(OpCode::InverseLengthFloat3));
						break;
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::InverseLengthFloat4));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Integer:
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't get length of non-vector types");
						break;
				}
			}
			break;
			case TokenType::Normalize:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer2:
						// TODO: Cast
						Error("Expected float type");
						break;
					case PrimitiveType::Integer3:
						// TODO: Cast
						Error("Expected float type");
						break;
					case PrimitiveType::Integer4:
						// TODO: Cast
						Error("Expected float type");
						break;
					case PrimitiveType::Float2:
						EmitByte(uint8(OpCode::Normalize2));
						break;
					case PrimitiveType::Float3:
						EmitByte(uint8(OpCode::Normalize3));
						break;
					case PrimitiveType::Float4:
						EmitByte(uint8(OpCode::Normalize4));
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Integer:
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
					case PrimitiveType::Boolean:
					case PrimitiveType::Boolean2:
					case PrimitiveType::Boolean3:
					case PrimitiveType::Boolean4:
						Error("Can't get length of non-vector types");
						break;
				}
			}
			break;
			case TokenType::Any:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Cast
						Error("Expected boolean type");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						// TODO: Cast
						Error("Expected boolean type");
						break;
					case PrimitiveType::Boolean:
						break;
					case PrimitiveType::Boolean2:
						EmitByte(uint8(OpCode::AnyBoolean2));
						break;
					case PrimitiveType::Boolean3:
						EmitByte(uint8(OpCode::AnyBoolean3));
						break;
					case PrimitiveType::Boolean4:
						EmitByte(uint8(OpCode::AnyBoolean4));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
						Error("Can't check any for non-numeric types");
						break;
				}
			}
			break;
			case TokenType::All:
			{
				Visit(*unary.GetRight());
				switch (unary.GetPrimitiveType())
				{
					case PrimitiveType::Integer:
					case PrimitiveType::Integer2:
					case PrimitiveType::Integer3:
					case PrimitiveType::Integer4:
						// TODO: Cast
						Error("Expected boolean type");
						break;
					case PrimitiveType::Float:
					case PrimitiveType::Float2:
					case PrimitiveType::Float3:
					case PrimitiveType::Float4:
						// TODO: Cast
						Error("Expected boolean type");
						break;
					case PrimitiveType::Boolean:
						break;
					case PrimitiveType::Boolean2:
						EmitByte(uint8(OpCode::AllBoolean2));
						break;
					case PrimitiveType::Boolean3:
						EmitByte(uint8(OpCode::AllBoolean3));
						break;
					case PrimitiveType::Boolean4:
						EmitByte(uint8(OpCode::AllBoolean4));
						break;
					case PrimitiveType::Any:
					case PrimitiveType::Null:
					case PrimitiveType::String:
						Error("Can't check any for non-numeric types");
						break;
				}
			}
			break;
			default:
				ExpectUnreachable();
		}
	}

	void Compiler::Visit(const AST::Expression::VariableDeclaration& variableDeclaration)
	{
		UpdateSourceLocation(variableDeclaration.GetSourceLocation());

		if (m_state.flags.IsSet(Flags::Local))
		{
			AddLocalVariable(variableDeclaration.GetIdentifier().identifier);
			m_state.flags.Set(Flags::SkipPop); // keep locals on the stack
			return;                            // no need to self assign we just keep the value on the stack
		}

		int32 index = ResolveLocalVariable(variableDeclaration.GetIdentifier().identifier);
		if (index != -1)
		{
			EmitByte(uint8(m_state.flags.IsSet(Flags::Assignment) ? OpCode::SetLocal : OpCode::PushLocal));
			EmitByte(uint8(index));
		}
		else
		{
			index = ResolveUpvalue(variableDeclaration.GetIdentifier().identifier);
			if (index != -1)
			{
				EmitByte(uint8(m_state.flags.IsSet(Flags::Assignment) ? OpCode::SetUpvalue : OpCode::PushUpvalue));
				EmitByte(uint8(index));
			}
			else
			{
				const Optional<Chunk::ConstantIndex> constantIndex = AddConstant(variableDeclaration.GetIdentifier().identifier);
				Assert(constantIndex.IsValid());
				if (LIKELY(constantIndex.IsValid()))
				{
					static_assert(TypeTraits::IsSame<Chunk::ConstantIndex, uint8>, "TODO: Support for more constants");
					EmitByte(uint8(m_state.flags.IsSet(Flags::Assignment) ? OpCode::SetGlobal : OpCode::PushGlobal));
					EmitByte(uint8(*constantIndex));
				}
				else
				{
					Error("Failed to create variable constant");
				}
			}
		}
	}

	bool Compiler::IsLocalFunction(const Guid guid) const
	{
		if (m_state.m_localFunctionsSet.Contains(guid))
		{
			return true;
		}
		else if (m_state.pEnclosing.IsValid())
		{
			return m_state.pEnclosing->IsLocalFunction(guid);
		}
		return false;
	}

	void Compiler::Visit(const AST::Expression::Variable& variable)
	{
		// Object and index don't have assignment
		const bool isAssignment = m_state.flags.IsSet(Flags::Assignment);
		m_state.flags.Clear(Flags::Assignment);
		const Optional<const AST::Expression::Base*> pIndex = variable.GetIndex();
		if (pIndex.IsValid())
		{
			Visit(*pIndex);
		}
		m_state.flags.Set(Flags::Assignment, isAssignment);

		if (!pIndex.IsValid())
		{
			UpdateSourceLocation(variable.GetSourceLocation());

			if (m_state.flags.IsSet(Flags::Local))
			{
				AddLocalVariable(variable.GetIdentifier().identifier);
				m_state.flags.Set(Flags::SkipPop); // keep locals on the stack
				return;                            // no need to self assign we just keep the value on the stack
			}

			int32 index = ResolveLocalVariable(variable.GetIdentifier().identifier);
			if (index != -1)
			{
				EmitByte(uint8(m_state.flags.IsSet(Flags::Assignment) ? OpCode::SetLocal : OpCode::PushLocal));
				EmitByte(uint8(index));
			}
			else
			{
				index = ResolveUpvalue(variable.GetIdentifier().identifier);
				if (index != -1)
				{
					EmitByte(uint8(m_state.flags.IsSet(Flags::Assignment) ? OpCode::SetUpvalue : OpCode::PushUpvalue));
					EmitByte(uint8(index));
				}
				else
				{
					Assert(variable.GetIdentifier().m_types.GetSize() == 1);
					const Scripting::Type& variableType = variable.GetIdentifier().m_types[0];
					if (variableType.Is<VM::DynamicFunction>() && !IsLocalFunction(variable.GetIdentifier().identifier))
					{
						Optional<Chunk::ConstantIndex> constantIndex = AddConstant(
							Value{RawValue{variable.GetIdentifier().identifier, ValueType::NativeFunctionGuid}, ValueType::NativeFunctionGuid}
						);
						if (LIKELY(constantIndex.IsValid()))
						{
							Assert(m_state.flags.IsNotSet(Flags::Assignment));
							static_assert(TypeTraits::IsSame<Chunk::ConstantIndex, uint8>, "TODO: Support for more constants");
							EmitByte(uint8(OpCode::PushConstant));
							EmitByte(uint8(*constantIndex));
						}
						else
						{
							Error("Failed to create variable constant");
						}
					}
					else
					{
						const Optional<Chunk::ConstantIndex> constantIndex = AddConstant(variable.GetIdentifier().identifier);
						Assert(constantIndex.IsValid());
						if (LIKELY(constantIndex.IsValid()))
						{
							static_assert(TypeTraits::IsSame<Chunk::ConstantIndex, uint8>, "TODO: Support for more constants");
							EmitByte(uint8(m_state.flags.IsSet(Flags::Assignment) ? OpCode::SetGlobal : OpCode::PushGlobal));
							EmitByte(uint8(*constantIndex));
						}
						else
						{
							Error("Failed to create variable constant");
						}
					}
				}
			}
		}
	}

	Chunk& Compiler::GetCurrentChunk()
	{
		Assert(m_state.pFunction);
		return m_state.pFunction->chunk;
	}

	void Compiler::EmitByte(uint8 byte)
	{
		Chunk& chunk = GetCurrentChunk();
		chunk.code.EmplaceBack(byte);

#ifdef SCRIPTING_DEBUG_INFO
		const uint32 debugInfoCount = chunk.debugInfo.GetSize();
		if (debugInfoCount > 0 && chunk.debugInfo[debugInfoCount - 1].sourceLocation.lineNumber == m_state.sourceLocation.lineNumber)
		{
			return;
		}
		chunk.debugInfo.EmplaceBack(Chunk::DebugInfo{m_state.sourceLocation, uint16(chunk.code.GetSize() - 1)});
#endif
	}

	int32 Compiler::EmitJump(uint8 byte)
	{
		EmitByte(byte);
		EmitByte(0xFF);
		EmitByte(0xFF);
		return GetCurrentChunk().code.GetSize() - 2;
	}

	void Compiler::PatchJump(int32 at, int32 to)
	{
		Chunk& chunk = GetCurrentChunk();
		const int32 jumpOffset = to != 0 ? to : chunk.code.GetSize() - at - 2;
		if (jumpOffset > 0x7FFF)
		{
			Error("Jump can't reach code");
		}

		chunk.code[at] = (jumpOffset >> 8) & 0xFF;
		chunk.code[at + 1] = jumpOffset & 0xFF;
	}

	void Compiler::EmitLoop(uint8 byte, int32 to)
	{
		EmitByte(byte);

		const int32 jumpOffset = (GetCurrentChunk().code.GetSize() - to + 2) * -1;
		if (jumpOffset < -0x8000)
		{
			Error("Jump can't reach code");
		}

		EmitByte((jumpOffset >> 8) & 0xFF);
		EmitByte(jumpOffset & 0xFF);
	}

	Optional<Chunk::ConstantIndex> Compiler::AddConstant(Value constant)
	{
		Chunk& chunk = GetCurrentChunk();
		Assert(!chunk.constantValues.ReachedTheoreticalCapacity());
		if (LIKELY(!chunk.constantValues.ReachedTheoreticalCapacity()))
		{
			const Chunk::ConstantIndex constantCount = chunk.constantValues.GetSize();
			for (Chunk::ConstantIndex i = 0; i < constantCount; ++i)
			{
				if (ValueEquals(constant, Value{chunk.constantValues[i], chunk.constantTypes[i]}))
				{
					return i;
				}
			}
			Assert(chunk.constantValues.GetSize() == chunk.constantTypes.GetSize());
			chunk.constantValues.EmplaceBack(constant);
			chunk.constantTypes.EmplaceBack(constant.type);
			return constantCount;
		}
		return Invalid;
	}

	void Compiler::AddLocalVariable(const Guid identifier)
	{
		if (m_state.localCount < MaxLocalVariableCount)
		{
			LocalVariable& localVariable = m_state.locals[m_state.localCount++];
			localVariable.guid = identifier;
			localVariable.depth = m_state.scopeDepth;
			localVariable.isCaptured = false;
		}
		else
		{
			Error("Only 256 local variables supported");
		}
	}

	int32 Compiler::ResolveLocalVariable(const Guid identifier)
	{
		int32 curIndex = m_state.localCount;
		while (curIndex--)
		{
			if (m_state.locals[curIndex].guid == identifier)
			{
				return curIndex;
			}
		}
		return -1;
	}

	int32 Compiler::AddUpvalue(uint8 index, bool isLocal)
	{
		const int32 upvalueCount = m_state.pFunction->upvalues;
		for (int32 i = 0; i < upvalueCount; ++i)
		{
			const Upvalue& upvalue = m_state.upvalues[i];
			if (upvalue.index == index && upvalue.isLocal == isLocal)
			{
				return i;
			}
		}

		if (upvalueCount == 0xFF)
		{
			Error("Only 256 closure variables supported");
			return 0;
		}

		m_state.upvalues[upvalueCount].index = index;
		m_state.upvalues[upvalueCount].isLocal = isLocal;

		return m_state.pFunction->upvalues++;
	}

	int32 Compiler::ResolveUpvalue(const Guid identifier)
	{
		if (m_state.pEnclosing == nullptr)
		{
			return -1;
		}

		const int32 local = m_state.pEnclosing->ResolveLocalVariable(identifier);
		if (local != -1)
		{
			m_state.pEnclosing->m_state.locals[local].isCaptured = true;
			return AddUpvalue(uint8(local), true);
		}

		const int32 upvalue = m_state.pEnclosing->ResolveUpvalue(identifier);
		if (upvalue != -1)
		{
			return AddUpvalue(uint8(upvalue), false);
		}

		return -1;
	}

	void Compiler::BeginScope()
	{
		++m_state.scopeDepth;
	}

	void Compiler::EndScope()
	{
		--m_state.scopeDepth;
		CleanScope(m_state.scopeDepth, true);
	}

	void Compiler::CleanScope(int32 scopeDepth, bool removeLocals)
	{
		int32 localIndex = m_state.localCount;
		while (localIndex--)
		{
			LocalVariable& localVariable = m_state.locals[localIndex];
			if (removeLocals)
			{
				int32 breakpointIndex = m_state.breakpoints.GetSize();
				while (breakpointIndex--)
				{
					Breakpoint& breakpoint = m_state.breakpoints[breakpointIndex];
					if (localVariable.depth > scopeDepth && breakpoint.depth >= localVariable.depth && breakpoint.localCount == uint32(localIndex))
					{
						PatchJump(breakpoint.at);
						m_state.breakpoints.PopBack();
					}
				}
			}

			if (localVariable.depth <= scopeDepth)
			{
				break;
			}

			if (localVariable.isCaptured)
			{
				EmitByte(uint8(OpCode::CloseUpvalue));
			}
			else
			{
				EmitByte(uint8(OpCode::Pop));
			}

			if (removeLocals)
			{
				--m_state.localCount;
			}
		}
	}

	void Compiler::Error(StringType::ConstView error)
	{
		LogError("{}", error);
		m_state.flags.Set(Flags::Error);
	}

	bool Compiler::Save(const FunctionObject& function, Vector<ByteType>& output) const
	{
		output.Reserve(1024);
		output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&Magic), sizeof(Magic)});
		output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&Version), sizeof(Version)});

		const Time::Timestamp timestamp = Time::Timestamp::GetCurrent();
		output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&timestamp), sizeof(timestamp)});

		return SaveFunction(function, output);
	}

	bool Compiler::SaveFunction(const FunctionObject& function, Vector<ByteType>& output) const
	{
		Entity::Manager& entityManager = System::Get<Entity::Manager>();

		Vector<const FunctionObject*> pFunctions;

		const Chunk::ConstantIndex constantsCount = function.chunk.constantValues.GetSize();
		output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&constantsCount), sizeof(constantsCount)});
		for (Chunk::ConstantIndex i = 0; i < constantsCount; ++i)
		{
			const RawValue rawValue = function.chunk.constantValues[i];
			const ValueType valueType = function.chunk.constantTypes[i];
			output.EmplaceBack((uint8)valueType);
			switch (valueType)
			{
				case ValueType::Unknown:
					ExpectUnreachable();
				case ValueType::Null:
					// No data to write
					break;
				case ValueType::Boolean:
				{
					const Value value{rawValue, valueType};
					const bool typeValue = value.GetBool();
					output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&typeValue), sizeof(typeValue)});
				}
				break;
				case ValueType::Boolean4:
				{
					const Value value{rawValue, valueType};
					const Math::Vector4f::BoolType typeValue = value.GetBool4();
					output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&typeValue), sizeof(typeValue)});
				}
				break;
				case ValueType::Integer:
				{
					const Value value{rawValue, valueType};
					const IntegerType typeValue = value.GetInteger();
					output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&typeValue), sizeof(typeValue)});
				}
				break;
				case ValueType::Integer4:
				{
					const Value value{rawValue, valueType};
					const Math::Vector4i typeValue = value.GetVector4i();
					output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&typeValue), sizeof(typeValue)});
				}
				break;
				case ValueType::Decimal:
				{
					const Value value{rawValue, valueType};
					const FloatType typeValue = value.GetDecimal();
					output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&typeValue), sizeof(typeValue)});
				}
				break;
				case ValueType::Decimal4:
				{
					const Value value{rawValue, valueType};
					const Math::Vector4f typeValue = value.GetVector4f();
					output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&typeValue), sizeof(typeValue)});
				}
				break;
				case ValueType::Guid:
				case ValueType::NativeFunctionGuid:
				case ValueType::TagGuid:
				case ValueType::RenderStageGuid:
				case ValueType::AssetGuid:
				{
					const Value value{rawValue, valueType};
					const Guid typeValue = value.GetGuid();
					output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&typeValue), sizeof(typeValue)});
				}
				break;
				case ValueType::ComponentSoftReference:
				{
					const Value value{rawValue, valueType};
					const Entity::ComponentSoftReference componentSoftReference = value.GetComponentSoftReference();

					const Entity::ComponentTypeIdentifier componentTypeIdentifier = componentSoftReference.GetTypeIdentifier();
					const Guid componentTypeGuid = componentTypeIdentifier.IsValid() ? entityManager.GetRegistry().GetGuid(componentTypeIdentifier)
					                                                                 : Guid{};

					const Guid instanceGuid = componentSoftReference.GetInstanceGuid();

					output.CopyEmplaceRangeBack(
						ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&componentTypeGuid), sizeof(componentTypeGuid)}
					);
					output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&instanceGuid), sizeof(instanceGuid)});
				}
				break;
				case ValueType::NativeFunctionPointer:
				case ValueType::TagIdentifier:
				case ValueType::AssetIdentifier:
				case ValueType::ComponentPointer:
				case ValueType::RenderStageIdentifier:
					ExpectUnreachable();
				case ValueType::Object:
				{
					const Value value{rawValue, valueType};
					const ObjectType objectType = GetObjectType(value);
					output.EmplaceBack((uint8)objectType);
					switch (objectType)
					{
						case ObjectType::String:
						{
							StringObject* const pStringObject = AsStringObject(value);
							Assert(pStringObject->string.GetSize() <= Math::NumericLimits<uint16>::Max);
							if (UNLIKELY_ERROR(pStringObject->string.GetSize() > Math::NumericLimits<uint16>::Max))
							{
								return false;
							}

							const uint16 stringSize = uint16(pStringObject->string.GetSize());
							output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&stringSize), sizeof(stringSize)});
							output.CopyEmplaceRangeBack(
								ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(pStringObject->string.GetData()), stringSize}
							);
						}
						break;
						case ObjectType::Function:
						{
							FunctionObject* const pFunctionObject = AsFunctionObject(value);
							pFunctions.EmplaceBack(pFunctionObject);
						}
						break;
						case ObjectType::Closure:
						case ObjectType::Upvalue:
						{
							AssertMessage(false, "Trying to write unsupported constant of type {}", (uint32)value.GetObject()->type);
							return false;
						}
					}
				}
				break;
			}
		}

		output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&function.id), sizeof(function.id)});
		output.EmplaceBack((uint8)function.arity);
		output.EmplaceBack((uint8)function.coarity);
		output.EmplaceBack((uint8)function.locals);
		output.EmplaceBack((uint8)function.upvalues);

		const uint32 codeSize = function.chunk.code.GetSize();
		output.CopyEmplaceRangeBack(ArrayView<const ByteType>{reinterpret_cast<const ByteType*>(&codeSize), sizeof(codeSize)});
		output.CopyEmplaceRangeBack(function.chunk.code.GetView());

		bool wasSuccessful = true;
		for (const FunctionObject* pFunctionObject : pFunctions)
		{
			wasSuccessful &= SaveFunction(*pFunctionObject, output);
		}
		return wasSuccessful;
	}

	[[nodiscard]] UniquePtr<FunctionObject> Compiler::Load(ConstByteView input) const
	{
		if (Optional<const uint64*> pMagic = input.ReadAndSkip<uint64>())
		{
			if (*pMagic != Magic)
			{
				LogError("Trying to load non SCRIPT");
				return nullptr;
			}

			if (Optional<const uint32*> pVersion = input.ReadAndSkip<uint32>())
			{
				if (*pVersion != Version)
				{
					// This is the place to potentially patch older formats
					LogError("Can't load file version '{}', only '{}' is supported", *pVersion, Version);
				}
				if (Optional<const uint64*> pTimestamp = input.ReadAndSkip<uint64>())
				{
					GC gc{&SimpleReallocate, nullptr, 0};
					FunctionObject* pFunction = CreateFunctionObject(gc);
					pFunction->id = FunctionObjectScriptId;
					return LoadFunction(*pFunction, input) ? UniquePtr<FunctionObject>::FromRaw(pFunction) : nullptr;
				}
			}
		}
		return nullptr;
	}

	[[nodiscard]] bool Compiler::LoadFunction(FunctionObject& function, ConstByteView input) const
	{
		GC gc{&SimpleReallocate, &function.chunk.pObjects, 0};
		Vector<FunctionObject*> pFunctions;

		const Optional<const Chunk::ConstantIndex*> pConstantCount = input.ReadAndSkip<Chunk::ConstantIndex>();
		Assert(pConstantCount.IsValid());
		if (UNLIKELY_ERROR(pConstantCount.IsInvalid()))
		{
			return false;
		}

		Entity::Manager& entityManager = System::Get<Entity::Manager>();

		Vector<RawValue, Chunk::ConstantIndex>& constantValues = function.chunk.constantValues;
		Vector<ValueType, Chunk::ConstantIndex>& constantTypes = function.chunk.constantTypes;
		constantValues.Resize(*pConstantCount);
		constantTypes.Resize(*pConstantCount);
		for (Chunk::ConstantIndex i = 0, n = *pConstantCount; i < n; ++i)
		{
			const Optional<const uint8*> pValueType = input.ReadAndSkip<uint8>();
			Assert(pValueType.IsValid());
			if (UNLIKELY_ERROR(pValueType.IsInvalid()))
			{
				return false;
			}

			constantTypes[i] = (ValueType)*pValueType;
			switch ((ValueType)*pValueType)
			{
				case ValueType::Unknown:
					ExpectUnreachable();
				case ValueType::Null:
					constantValues[i] = RawValue{nullptr};
					break;
				case ValueType::Boolean:
				{
					const Optional<const bool*> pValue = input.ReadAndSkip<bool>();
					Assert(pValue.IsValid());
					if (UNLIKELY_ERROR(pValue.IsInvalid()))
					{
						return false;
					}

					constantValues[i] = RawValue{*pValue};
				}
				break;
				case ValueType::Boolean4:
				{
					const Optional<const Math::Vector4f::BoolType*> pValue = input.ReadAndSkip<Math::Vector4f::BoolType>();
					Assert(pValue.IsValid());
					if (UNLIKELY_ERROR(pValue.IsInvalid()))
					{
						return false;
					}

					constantValues[i] = RawValue{*pValue};
				}
				break;
				case ValueType::Integer:
				{
					const Optional<const IntegerType*> pValue = input.ReadAndSkip<IntegerType>();
					Assert(pValue.IsValid());
					if (UNLIKELY_ERROR(pValue.IsInvalid()))
					{
						return false;
					}

					constantValues[i] = RawValue{*pValue};
				}
				break;
				case ValueType::Integer4:
				{
					const Optional<const Math::Vector4i*> pValue = input.ReadAndSkip<Math::Vector4i>();
					Assert(pValue.IsValid());
					if (UNLIKELY_ERROR(pValue.IsInvalid()))
					{
						return false;
					}

					constantValues[i] = RawValue{*pValue};
				}
				break;
				case ValueType::Decimal:
				{
					const Optional<const FloatType*> pValue = input.ReadAndSkip<FloatType>();
					Assert(pValue.IsValid());
					if (UNLIKELY_ERROR(pValue.IsInvalid()))
					{
						return false;
					}

					constantValues[i] = RawValue{*pValue};
				}
				break;
				case ValueType::Decimal4:
				{
					const Optional<const Math::Vector4f*> pValue = input.ReadAndSkip<Math::Vector4f>();
					Assert(pValue.IsValid());
					if (UNLIKELY_ERROR(pValue.IsInvalid()))
					{
						return false;
					}

					constantValues[i] = RawValue{*pValue};
				}
				break;
				case ValueType::Guid:
				case ValueType::NativeFunctionGuid:
				case ValueType::TagGuid:
				case ValueType::RenderStageGuid:
				case ValueType::AssetGuid:
				{
					const Optional<const Guid*> pGuid = input.ReadAndSkip<Guid>();
					Assert(pGuid.IsValid());
					if (UNLIKELY_ERROR(pGuid.IsInvalid()))
					{
						return false;
					}

					constantValues[i] = *pGuid;
				}
				break;
				case ValueType::ComponentSoftReference:
				{
					const Optional<const Guid*> pComponentTypeIdentifier = input.ReadAndSkip<Guid>();
					const Optional<const Guid*> pComponentInstanceGuid = input.ReadAndSkip<Guid>();
					Assert(pComponentTypeIdentifier.IsValid() && pComponentInstanceGuid.IsValid());
					if (UNLIKELY_ERROR(pComponentTypeIdentifier.IsInvalid() || pComponentInstanceGuid.IsInvalid()))
					{
						constantValues[i] = RawValue{Entity::ComponentSoftReference{}};
						continue;
					}

					const Entity::ComponentTypeIdentifier componentTypeIdentifier =
						entityManager.GetRegistry().FindIdentifier(*pComponentTypeIdentifier);
					constantValues[i] = RawValue{
						Entity::ComponentSoftReference{componentTypeIdentifier, Entity::ComponentSoftReference::Instance{*pComponentInstanceGuid}}
					};
				}
				break;
				case ValueType::NativeFunctionPointer:
				case ValueType::TagIdentifier:
				case ValueType::AssetIdentifier:
				case ValueType::ComponentPointer:
				case ValueType::RenderStageIdentifier:
					ExpectUnreachable();
				case ValueType::Object:
				{
					const Optional<const uint8*> pObjectType = input.ReadAndSkip<uint8>();
					Assert(pObjectType.IsValid());
					if (UNLIKELY_ERROR(pObjectType.IsInvalid()))
					{
						return false;
					}

					switch ((ObjectType)*pObjectType)
					{
						case ObjectType::String:
						{
							const Optional<const uint16*> pStringSize = input.ReadAndSkip<uint16>();
							Assert(pStringSize.IsValid());
							if (UNLIKELY_ERROR(pStringSize.IsInvalid()))
							{
								return false;
							}

							StringObject* const pStringObject = CreateStringObject(gc);
							pStringObject->string.Resize(*pStringSize);
							const bool wasSuccessful = input.ReadIntoViewAndSkip(pStringObject->string.GetView());
							Assert(wasSuccessful);
							if (UNLIKELY_ERROR(wasSuccessful))
							{
								return false;
							}
							constantValues[i] = RawValue{(Object*)pStringObject};
						}
						break;
						case ObjectType::Function:
						{
							FunctionObject* const pFunctionObject = CreateFunctionObject(gc);
							constantValues[i] = RawValue{(Object*)pFunctionObject};
							pFunctions.EmplaceBack(pFunctionObject);
						}
						break;
						case ObjectType::Closure:
						case ObjectType::Upvalue:
							AssertMessage(false, "Trying to read unsupported constant of type {}", *pObjectType);
							return false;
					}
				}
				break;
			}
		}

		{
			const Optional<const uint16*> pId = input.ReadAndSkip<uint16>();
			Assert(pId.IsValid());
			if (UNLIKELY_ERROR(pId.IsInvalid()))
			{
				return false;
			}
			function.id = *pId;
		}

		{
			const Optional<const uint8*> pArity = input.ReadAndSkip<uint8>();
			Assert(pArity.IsValid());
			if (UNLIKELY_ERROR(pArity.IsInvalid()))
			{
				return false;
			}
			function.arity = *pArity;
		}

		{
			const Optional<const uint8*> pCoarity = input.ReadAndSkip<uint8>();
			Assert(pCoarity.IsValid());
			if (UNLIKELY_ERROR(pCoarity.IsInvalid()))
			{
				return false;
			}
			function.coarity = *pCoarity;
		}

		{
			const Optional<const uint8*> pLocals = input.ReadAndSkip<uint8>();
			Assert(pLocals.IsValid());
			if (UNLIKELY_ERROR(pLocals.IsInvalid()))
			{
				return false;
			}
			function.locals = *pLocals;
		}

		{
			const Optional<const uint8*> pUpvalues = input.ReadAndSkip<uint8>();
			Assert(pUpvalues.IsValid());
			if (UNLIKELY_ERROR(pUpvalues.IsInvalid()))
			{
				return false;
			}
			function.upvalues = *pUpvalues;
		}

		{
			const Optional<const uint32*> pChunkSize = input.ReadAndSkip<uint32>();
			Assert(pChunkSize.IsValid());
			if (UNLIKELY_ERROR(pChunkSize.IsInvalid()))
			{
				return false;
			}
			function.chunk.code.Resize(*pChunkSize);
			[[maybe_unused]] const bool wasSuccessful = input.ReadIntoViewAndSkip(function.chunk.code.GetView());
			Assert(wasSuccessful);
		}

		for (FunctionObject* pFunctionObject : pFunctions)
		{
			if (UNLIKELY_ERROR(!LoadFunction(*pFunctionObject, input)))
			{
				return false;
			}
		}
		return ResolveFunction(function);
	}

	[[nodiscard]] bool Compiler::ResolveFunction(FunctionObject& function) const
	{
		Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
		const Optional<Tag::Registry*> pTagRegistry = System::Find<Tag::Registry>();
		const Optional<Rendering::Renderer*> pRenderer = System::Find<Rendering::Renderer>();
		const Optional<Asset::Manager*> pAssetManager = System::Find<Asset::Manager>();
		for (Chunk::ConstantIndex i = 0, n = function.chunk.constantValues.GetSize(); i < n; ++i)
		{
			switch ((ValueType)function.chunk.constantTypes[i])
			{
				case ValueType::Unknown:
					ExpectUnreachable();
				case ValueType::Null:
				case ValueType::Boolean:
				case ValueType::Boolean4:
				case ValueType::Integer:
				case ValueType::Integer4:
				case ValueType::Decimal:
				case ValueType::Decimal4:
				case ValueType::Guid:
				case ValueType::ComponentSoftReference:
					// No resolving to be done
					break;
				case ValueType::NativeFunctionGuid:
				{
					const Guid functionGuid = function.chunk.constantValues[i].GetGuid();
					const Optional<const Reflection::FunctionData*> pFunctionData = reflectionRegistry.FindFunction(functionGuid);
					Assert(pFunctionData.IsValid());
					if (UNLIKELY(pFunctionData.IsInvalid()))
					{
						return false;
					}

					Assert(pFunctionData->m_function.IsValid());
					if (UNLIKELY(!pFunctionData->m_function.IsValid()))
					{
						return false;
					}

					function.chunk.constantValues[i] = RawValue{pFunctionData->m_function};
					function.chunk.constantTypes[i] = ValueType::NativeFunctionPointer;
				}
				break;
				case ValueType::TagGuid:
				{
					const Guid tagGuid = function.chunk.constantValues[i].GetGuid();
					const Tag::Identifier tagIdentifier = pTagRegistry->FindOrRegister(tagGuid);
					function.chunk.constantValues[i] = RawValue{tagIdentifier};
					function.chunk.constantTypes[i] = ValueType::TagIdentifier;
				}
				break;
				case ValueType::RenderStageGuid:
				{
					const Guid stageGuid = function.chunk.constantValues[i].GetGuid();
					const Rendering::SceneRenderStageIdentifier stageIdentifier = pRenderer->GetStageCache().FindOrRegisterAsset(stageGuid);
					function.chunk.constantValues[i] = RawValue{stageIdentifier};
					function.chunk.constantTypes[i] = ValueType::RenderStageIdentifier;
				}
				break;
				case ValueType::AssetGuid:
				{
					const Guid assetGuid = function.chunk.constantValues[i].GetGuid();
					const Asset::Identifier assetIdentifier = pAssetManager->GetAssetIdentifier(assetGuid);
					function.chunk.constantValues[i] = RawValue{assetIdentifier};
					function.chunk.constantTypes[i] = ValueType::AssetIdentifier;
				}
				break;
				case ValueType::NativeFunctionPointer:
				case ValueType::TagIdentifier:
				case ValueType::AssetIdentifier:
				case ValueType::ComponentPointer:
				case ValueType::RenderStageIdentifier:
					ExpectUnreachable();
				case ValueType::Object:
				{
					const RawValue value = function.chunk.constantValues[i];
					Object* object = value.GetObject();
					switch (object->type)
					{
						case ObjectType::String:
						case ObjectType::Upvalue:
							// No resolving to be done
							break;
						case ObjectType::Function:
							if (!ResolveFunction(*(FunctionObject*)object))
							{
								return false;
							}
							break;
						case ObjectType::Closure:
						{
							ClosureObject* closureObject = (ClosureObject*)object;
							if (!ResolveFunction(*closureObject->pFunction))
							{
								return false;
							}
						}
						break;
					}
				}
				break;
			}
		}
		return true;
	}
}
