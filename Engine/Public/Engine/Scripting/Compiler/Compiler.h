#pragma once

#include "Engine/Scripting/Compiler/Value.h"
#include "Engine/Scripting/Compiler/Object.h"

#include "Engine/Scripting/Parser/StringType.h"
#include "Engine/Scripting/Parser/Token.h"

#include "Engine/Scripting/Parser/AST/VisitNode.h"

#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Math/HashedObject.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/UnorderedSet.h>

namespace ngine::IO
{
	struct Path;
	struct File;
}

namespace ngine::Scripting
{
	struct Chunk;
}

namespace ngine::Scripting
{
	struct Compiler : public AST::NodeVisitor<Compiler>
	{
	public:
		static constexpr uint32 Version = 1;
		static constexpr uint64 Magic = 0x0000545049524353; // SCRIPT\0\0

		static constexpr uint32 MaxLocalVariableCount = 0xFF;
		static constexpr uint32 MaxUpValueCount = 0xFF;

		enum class Flags : uint8
		{
			Error = 1 << 0,
			Assignment = 1 << 1,
			Local = 1 << 2,
			SkipPop = 1 << 3,
			HasReturn = 1 << 4
		};
		enum class FunctionType : uint8
		{
			Function,
			Script
		};
	public:
		Compiler();

		[[nodiscard]] UniquePtr<FunctionObject> Compile(const AST::Statement::Base& statement);
		[[nodiscard]] UniquePtr<FunctionObject> Compile(const AST::Expression::Function& function);
		//! Resolves function pointers in the given constant, to make them executable on this machine
		[[nodiscard]] bool ResolveConstant(const Value& value) const;
		//! Resolves function pointers in the given function, to make them executable on this machine
		[[nodiscard]] bool ResolveFunction(FunctionObject& function) const;
		[[nodiscard]] UniquePtr<FunctionObject> Load(ConstByteView input) const;
		[[nodiscard]] bool Save(const FunctionObject& function, Vector<ByteType>& output) const;

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
	protected:
		void Initialize(FunctionObject& object, FunctionType type);
		[[nodiscard]] bool CompileFunction(const AST::Expression::Function& function);
		[[nodiscard]] bool LoadFunction(FunctionObject& function, ConstByteView input) const;
		[[nodiscard]] bool SaveFunction(const FunctionObject& function, Vector<ByteType>& output) const;
	private:
		[[nodiscard]] Chunk& GetCurrentChunk();

		void EmitByte(uint8 byte);
		[[nodiscard]] int32 EmitJump(uint8 byte);
		void PatchJump(int32 at, int32 to = 0);
		void EmitLoop(uint8 byte, int32 to);
		[[nodiscard]] Optional<uint8> AddConstant(Value constant);
		void AddLocalVariable(const Guid identifier);
		[[nodiscard]] int32 ResolveLocalVariable(const Guid identifier);
		[[nodiscard]] int32 AddUpvalue(uint8 index, bool isLocal);
		[[nodiscard]] int32 ResolveUpvalue(const Guid identifier);
		void BeginScope();
		void EndScope();
		void CleanScope(int32 scopeDepth, bool removeLocals);
		void Error(StringType::ConstView error);

		[[nodiscard]] bool IsLocalFunction(const Guid guid) const;

		void UpdateSourceLocation([[maybe_unused]] const SourceLocation sourceLocation)
		{
#if SCRIPTING_DEBUG_INFO
			m_state.sourceLocation = sourceLocation;
#endif
		}
	private:
		struct LocalVariable
		{
			Guid guid;
			int32 depth;
			bool isCaptured;
		};
		struct Upvalue
		{
			uint8 index;
			bool isLocal;
		};
		struct Breakpoint
		{
			int32 at;
			int32 depth;
			uint32 localCount;
		};
		struct State
		{
			Optional<FunctionObject*> pFunction;
			Optional<Compiler*> pEnclosing;
			Array<LocalVariable, MaxLocalVariableCount, uint32, uint32> locals;
			uint32 localCount;
			Array<Upvalue, MaxUpValueCount, uint32, uint32> upvalues;
			Vector<Breakpoint> breakpoints;
			int32 scopeDepth;
			int32 loopDepth;
			uint8 expectedCoarity;
			EnumFlags<Flags> flags;
			FunctionType functionType;
#ifdef SCRIPTING_DEBUG_INFO
			SourceLocation sourceLocation;
#endif
			UnorderedSet<Guid, Guid::Hash> m_localFunctionsSet;
		};
		class ScopedCallCoarity
		{
		public:
			ScopedCallCoarity(Compiler& compiler, uint8 coarity);
			~ScopedCallCoarity();
			[[nodiscard]] uint8 GetPrevious() const;
		private:
			Compiler& m_compiler;
			uint8 m_coarity;
		};
		State m_state;
	};

	ENUM_FLAG_OPERATORS(Compiler::Flags);
}
