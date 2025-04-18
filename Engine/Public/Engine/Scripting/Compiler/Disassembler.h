#pragma once

#include "Engine/Scripting/Parser/StringType.h"

#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>
#include <Common/Memory/Containers/String.h>

#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>

namespace ngine::Scripting
{
	struct Chunk;
}

namespace ngine::Scripting
{
	struct FunctionObject;

	class Disassembler
	{
	public:
		static constexpr uint32 Version = 1;
		static constexpr int32 SpacesPerTab = 0;
		enum class Flags : uint8
		{
			Error = 1 << 0
		};
	public:
		Disassembler();
		[[nodiscard]] StringType Disassemble(const FunctionObject& function);
		[[nodiscard]] StringType Disassemble(const Chunk& chunk, uint32 offset);
	protected:
		void Initialize(const Chunk& chunk, uint32 offset);

		void DisassembleNext();
		void DisassembleInstruction(StringType::ConstView name);
		void DisassembleByteInstruction(StringType::ConstView name);
		void DisassembleTwoByteInstruction(StringType::ConstView name);
		void DisassembleJump(StringType::ConstView name);
		void DisassembleConstant(StringType::ConstView name);
		void DisassembleClosure(StringType::ConstView name);
	private:
		void Write(StringType::ConstView text);
		void WriteLine(StringType::ConstView text);
		void WriteCode(StringType::ConstView text);
	private:
		struct State
		{
			Optional<const Chunk*> pChunk;
			StringType buffer;
			uint32 offset;
			int32 depth;
			EnumFlags<Flags> flags;
		};
		State m_state;
	};

	ENUM_FLAG_OPERATORS(Disassembler::Flags);
}
