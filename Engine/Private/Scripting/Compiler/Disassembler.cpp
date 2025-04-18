#include "Engine/Scripting/Compiler/Disassembler.h"

#include "Engine/Scripting/Compiler/Chunk.h"
#include "Engine/Scripting/Compiler/Opcode.h"
#include "Engine/Scripting/Compiler/Object.h"

#include <Common/Memory/Containers/Format/String.h>
#include <Common/Memory/Containers/Format/StringView.h>

namespace ngine::Scripting
{
	Disassembler::Disassembler()
	{
		m_state.depth = 1;
	}

	StringType Disassembler::Disassemble(const FunctionObject& function)
	{
		const Chunk& chunk = function.chunk;

		Initialize(chunk, 0);
		m_state.buffer.Reserve(4098);

		StringType functionHeader;
		if (function.id == FunctionObjectScriptId)
		{
			functionHeader += SCRIPT_STRING_LITERAL("<script>");
		}
		else
		{
			functionHeader.Format("<fn {}: arity={}, locals={}, upvalues={}>", function.id, function.arity, function.locals, function.upvalues);
		}
		WriteLine(functionHeader);

		const Chunk::ConstantIndex constantCount = chunk.constantValues.GetSize();
		for (Chunk::ConstantIndex i = 0; i < constantCount; ++i)
		{
			StringType constantString;
			constantString.Format("{:04d} ", i);
			constantString += ValueToString(Value{chunk.constantValues[i], chunk.constantTypes[i]});
			WriteLine(constantString);
		}
		if (constantCount)
		{
			Write("\n");
		}

		const uint32 byteCount = chunk.code.GetSize();
		while (m_state.offset < byteCount)
		{
			DisassembleNext();
		}

		for (Chunk::ConstantIndex i = 0; i < constantCount; ++i)
		{
			const Value value{chunk.constantValues[i], chunk.constantTypes[i]};
			if (IsFunctionObject(value))
			{
				Write("\n");
				FunctionObject* const pSubFunction = AsFunctionObject(value);
				Disassembler disassembler;
				disassembler.m_state.depth = m_state.depth + 1;
				StringType functionString = disassembler.Disassemble(*pSubFunction);

				Write(functionString);
			}
		}

		return m_state.buffer;
	}

	StringType Disassembler::Disassemble(const Chunk& chunk, uint32 offset)
	{
		Initialize(chunk, offset);
		m_state.buffer.Reserve(128);

		DisassembleNext();

		return m_state.buffer;
	}

	void Disassembler::Initialize(const Chunk& chunk, uint32 offset)
	{
		m_state.pChunk = &chunk;
		m_state.offset = offset;
		m_state.flags.Clear();
		m_state.buffer.Clear();
	}

	void Disassembler::DisassembleNext()
	{
		switch (OpCode(m_state.pChunk->code[m_state.offset]))
		{
			case OpCode::Nop:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Nop"));
			case OpCode::Null:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Null"));
			case OpCode::True:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("True"));
			case OpCode::False:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("False"));
			case OpCode::Return:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Return"));
			case OpCode::JumpIfFalse:
				return DisassembleJump(SCRIPT_STRING_LITERAL("JumpIfFalse"));
			case OpCode::JumpIfTrue:
				return DisassembleJump(SCRIPT_STRING_LITERAL("JumpIfTrue"));
			case OpCode::Jump:
				return DisassembleJump(SCRIPT_STRING_LITERAL("Jump"));
			case OpCode::CallNative:
				return DisassembleTwoByteInstruction(SCRIPT_STRING_LITERAL("CallNative"));
			case OpCode::CallClosure:
				return DisassembleTwoByteInstruction(SCRIPT_STRING_LITERAL("CallClosure"));
			case OpCode::PushConstant:
				return DisassembleConstant(SCRIPT_STRING_LITERAL("PushConstant"));
			case OpCode::PushComponentSoftReference:
				return DisassembleConstant(SCRIPT_STRING_LITERAL("PushComponentSoftReference"));
			case OpCode::PushImmediate:
				return DisassembleByteInstruction(SCRIPT_STRING_LITERAL("PushImmediate"));

			// Float operations
			case OpCode::NegateFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("NegateFloat"));
			case OpCode::LogicalNotFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LogicalNotFloat"));
			case OpCode::AddFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AddFloat"));
			case OpCode::SubtractFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("SubtractFloat"));
			case OpCode::MultiplyFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("MultiplyFloat"));
			case OpCode::DivideFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("DivideFloat"));
			case OpCode::LessFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LessFloat"));
			case OpCode::LessFloat4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LessFloat4"));
			case OpCode::LessEqualFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LessEqualFloat"));
			case OpCode::LessEqualFloat4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LessEqualFloat4"));
			case OpCode::GreaterFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("GreaterFloat"));
			case OpCode::GreaterFloat4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("GreaterFloat4"));
			case OpCode::GreaterEqualFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("GreaterEqualFloat"));
			case OpCode::GreaterEqualFloat4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("GreaterEqualFloat4"));
			case OpCode::NotEqualFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("NotEqualFloat"));
			case OpCode::NotEqualFloat4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("NotEqualFloat4"));
			case OpCode::EqualEqualFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("EqualFloat"));
			case OpCode::EqualEqualFloat4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("EqualFloat4"));
			case OpCode::ModuloFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("ModuloFloat"));
			case OpCode::AbsFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AbsFloat"));
			case OpCode::AcosFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AcosFloat"));
			case OpCode::AsinFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AsinFloat"));
			case OpCode::AtanFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AtanFloat"));
			case OpCode::Atan2Float:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Atan2Float"));
			case OpCode::CeilFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("CeilFloat"));
			case OpCode::CubicRootFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("CubicRootFloat"));
			case OpCode::CosFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("CosFloat"));
			case OpCode::RadiansToDegreesFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("RadiansToDegreesFloat"));
			case OpCode::ExpFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("ExpFloat"));
			case OpCode::FloorFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("FloorFloat"));
			case OpCode::RoundFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("RoundFloat"));
			case OpCode::FractFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("FractFloat"));
			case OpCode::InverseSqrtFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("InverseSqrtFloat"));
			case OpCode::LogFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LogFloat"));
			case OpCode::Log2Float:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Log2Float"));
			case OpCode::Log10Float:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Log10Float"));
			case OpCode::MaxFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("MaxFloat"));
			case OpCode::MinFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("MinFloat"));
			case OpCode::MultiplicativeInverseFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("MultiplicativeInverseFloat"));
			case OpCode::PowerFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("PowerFloat"));
			case OpCode::Power2Float:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Power2Float"));
			case OpCode::Power10Float:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Power2Float"));
			case OpCode::DegreesToRadiansFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("DegreesToRadiansFloat"));
			case OpCode::RandomFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("RandomFloat"));
			case OpCode::SignFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("SignFloat"));
			case OpCode::SignNonZeroFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("SignNonZeroFloat"));
			case OpCode::SinFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("SinFloat"));
			case OpCode::SqrtFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("SqrtFloat"));
			case OpCode::TanFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("TanFloat"));
			case OpCode::TruncateFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("TruncateFloat"));
			case OpCode::AreNearlyEqualFloat:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AreNearlyEqualFloat"));

			// Integral operations
			case OpCode::NegateInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("NegateInteger"));
			case OpCode::LogicalNotInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LogicalNotInteger"));
			case OpCode::TruthyNotInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("TruthyNotInteger"));
			case OpCode::FalseyNotInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("FalseyNotInteger"));
			case OpCode::BitwiseNotInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("BitwiseNotInteger"));
			case OpCode::AddInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AddInteger"));
			case OpCode::SubtractInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("SubtractInteger"));
			case OpCode::MultiplyInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("MultiplyInteger"));
			case OpCode::DivideInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("DivideInteger"));
			case OpCode::LessInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LessInteger"));
			case OpCode::LessInteger4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LessInteger4"));
			case OpCode::LessEqualInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LessEqualInteger"));
			case OpCode::LessEqualInteger4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LessEqualInteger4"));
			case OpCode::GreaterInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("GreaterInteger"));
			case OpCode::GreaterInteger4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("GreaterInteger4"));
			case OpCode::GreaterEqualInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("GreaterEqualInteger"));
			case OpCode::GreaterEqualInteger4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("GreaterEqualInteger4"));
			case OpCode::NotEqualInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("NotEqualInteger"));
			case OpCode::NotEqualInteger4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("NotEqualInteger4"));
			case OpCode::EqualEqualInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("EqualInteger"));
			case OpCode::EqualEqualInteger4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("EqualInteger4"));
			case OpCode::LeftShiftInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LeftShiftInteger"));
			case OpCode::RightShiftInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("RightShiftInteger"));
			case OpCode::ModuloInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("ModuloInteger"));
			case OpCode::AndInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AndInteger"));
			case OpCode::OrInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("OrInteger"));
			case OpCode::ExclusiveOrInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("ExclusiveOrInteger"));
			case OpCode::AbsInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AbsInteger"));
			case OpCode::MaxInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("MaxInteger"));
			case OpCode::MinInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("MinInteger"));
			case OpCode::RandomInteger:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("RandomInteger"));

			case OpCode::LengthInteger2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LengthInteger2"));
			case OpCode::LengthInteger3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LengthInteger3"));
			case OpCode::LengthInteger4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LengthInteger4"));
			case OpCode::LengthSquaredInteger2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LengthSquaredInteger2"));
			case OpCode::LengthSquaredInteger3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LengthSquaredInteger3"));
			case OpCode::LengthSquaredInteger4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LengthSquaredInteger4"));

			// String operations
			case OpCode::LessString:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LessString"));
			case OpCode::LessEqualString:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LessEqualString"));
			case OpCode::GreaterString:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("GreaterString"));
			case OpCode::GreaterEqualString:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("GreaterEqualString"));
			case OpCode::NotEqualString:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("NotEqualString"));
			case OpCode::EqualEqualString:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("EqualString"));

			// Boolean operations
			case OpCode::LogicalNotBoolean:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LogicalNotBoolean"));
			case OpCode::AnyBoolean2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AnyBoolean2"));
			case OpCode::AnyBoolean3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AnyBoolean3"));
			case OpCode::AnyBoolean4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AnyBoolean4"));
			case OpCode::AllBoolean2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AllBoolean2"));
			case OpCode::AllBoolean3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AllBoolean3"));
			case OpCode::AllBoolean4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("AllBoolean4"));

			// Vector operations
			case OpCode::Dot2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Dot2"));
			case OpCode::Dot3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Dot3"));
			case OpCode::Dot4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Dot4"));
			case OpCode::Cross2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Cross2"));
			case OpCode::Cross3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Cross3"));
			case OpCode::DistanceInteger2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("DistanceInteger2"));
			case OpCode::DistanceInteger3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("DistanceInteger3"));
			case OpCode::DistanceFloat2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("DistanceFloat2"));
			case OpCode::DistanceFloat3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("DistanceFloat3"));
			case OpCode::LengthFloat2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LengthFloat2"));
			case OpCode::LengthFloat3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LengthFloat3"));
			case OpCode::LengthFloat4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LengthFloat4"));
			case OpCode::InverseLengthFloat2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("InverseLengthFloat2"));
			case OpCode::InverseLengthFloat3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("InverseLengthFloat3"));
			case OpCode::InverseLengthFloat4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("InverseLengthFloat4"));
			case OpCode::LengthSquaredFloat2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LengthSquaredFloat2"));
			case OpCode::LengthSquaredFloat3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LengthSquaredFloat3"));
			case OpCode::LengthSquaredFloat4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("LengthSquaredFloat4"));
			case OpCode::Normalize2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Normalize2"));
			case OpCode::Normalize3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Normalize3"));
			case OpCode::Normalize4:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Normalize4"));
			case OpCode::Project2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Project2"));
			case OpCode::Project3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Project3"));
			case OpCode::Reflect2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Reflect2"));
			case OpCode::Reflect3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Reflect3"));
			case OpCode::Refract2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Refract2"));
			case OpCode::Refract3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Refract3"));

			// Rotation operations
			case OpCode::RightRotationDirection3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("RightRotationDirection3"));
			case OpCode::ForwardRotationDirection2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("ForwardRotationDirection2"));
			case OpCode::ForwardRotationDirection3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("ForwardRotationDirection3"));
			case OpCode::UpRotationDirection2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("UpRotationDirection2"));
			case OpCode::UpRotationDirection3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("UpRotationDirection3"));
			case OpCode::RotateRotation2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("RotateRotation2"));
			case OpCode::RotateRotation3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("RotateRotation3"));
			case OpCode::InverseRotateRotation2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("InverseRotateRotation2"));
			case OpCode::InverseRotateRotation3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("InverseRotateRotation3"));
			case OpCode::RotateDirection2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("RotateDirection2"));
			case OpCode::RotateDirection3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("RotateDirection3"));
			case OpCode::InverseRotateDirection2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("InverseRotateDirection2"));
			case OpCode::InverseRotateDirection3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("InverseRotateDirection3"));
			case OpCode::InverseRotation2:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("InverseRotation2"));
			case OpCode::InverseRotation3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("InverseRotation3"));
			case OpCode::NegateRotation3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("NegateRotation3"));
			case OpCode::RotationEuler3:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("RotationEuler3"));

			case OpCode::Pop:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("Pop"));
			case OpCode::PushGlobal:
				return DisassembleConstant(SCRIPT_STRING_LITERAL("PushGlobal"));
			case OpCode::SetGlobal:
				return DisassembleConstant(SCRIPT_STRING_LITERAL("SetGlobal"));
			case OpCode::PushLocal:
				return DisassembleByteInstruction(SCRIPT_STRING_LITERAL("PushLocal"));
			case OpCode::SetLocal:
				return DisassembleByteInstruction(SCRIPT_STRING_LITERAL("SetLocal"));
			case OpCode::PushClosure:
				return DisassembleClosure(SCRIPT_STRING_LITERAL("PushClosure"));
			case OpCode::PushUpvalue:
				return DisassembleByteInstruction(SCRIPT_STRING_LITERAL("PushUpvalue"));
			case OpCode::SetUpvalue:
				return DisassembleByteInstruction(SCRIPT_STRING_LITERAL("SetUpvalue"));
			case OpCode::CloseUpvalue:
				return DisassembleInstruction(SCRIPT_STRING_LITERAL("CloseUpvalue"));
		}
		ExpectUnreachable();
	}

	void Disassembler::DisassembleInstruction(StringType::ConstView name)
	{
		StringType instructionString;
		const uint8 opCode = m_state.pChunk->code[m_state.offset];
		instructionString.Format("{:02X} {:5s} {}", opCode, " ", name.GetData());
		WriteCode(instructionString);
		m_state.offset += 1;
	}

	void Disassembler::DisassembleByteInstruction(StringType::ConstView name)
	{
		StringType instructionString;
		const uint8 opCode = m_state.pChunk->code[m_state.offset];
		const uint8 slot = m_state.pChunk->code[m_state.offset + 1];
		instructionString.Format("{:02X} {:02X} {:2s} {:<16s} {:04d}", opCode, slot, " ", name.GetData(), slot);
		WriteCode(instructionString);
		m_state.offset += 2;
	}

	void Disassembler::DisassembleTwoByteInstruction(StringType::ConstView name)
	{
		StringType instructionString;
		const uint8 opCode = m_state.pChunk->code[m_state.offset];
		const uint8 slot = m_state.pChunk->code[m_state.offset + 1];
		const uint8 value = m_state.pChunk->code[m_state.offset + 2];
		instructionString.Format("{:02X} {:02X} {:02X} {:<16s} {:04d}", opCode, slot, value, name.GetData(), slot);
		WriteCode(instructionString);
		m_state.offset += 3;
	}

	void Disassembler::DisassembleJump(StringType::ConstView name)
	{
		StringType jumpString;
		const uint8 opCode = m_state.pChunk->code[m_state.offset];
		const uint8 jumpHigh = m_state.pChunk->code[m_state.offset + 1];
		const uint8 jumpLow = m_state.pChunk->code[m_state.offset + 2];
		jumpString.Format(
			"{:02X} {:02X} {:02X} {:<16s} {:04d} -> {:04d}",
			opCode,
			jumpHigh,
			jumpLow,
			name.GetData(),
			m_state.offset,
			m_state.offset + 3 + (int16(jumpHigh << 8) | int16(jumpLow))
		);
		WriteCode(jumpString);
		m_state.offset += 3;
	}

	void Disassembler::DisassembleConstant(StringType::ConstView name)
	{
		StringType constantString;
		const uint8 opCode = m_state.pChunk->code[m_state.offset];
		const uint8 index = m_state.pChunk->code[m_state.offset + 1];
		constantString.Format("{:02X} {:02X} {:2s} {:<16s} {:04d} '", opCode, index, " ", name.GetData(), index);
		const Value value{m_state.pChunk->constantValues[index], m_state.pChunk->constantTypes[index]};
		constantString += ValueToString(value);
		constantString += SCRIPT_STRING_LITERAL("'");
		WriteCode(constantString);
		m_state.offset += 2;
	}

	void Disassembler::DisassembleClosure(StringType::ConstView name)
	{
		StringType instructionString;
		const uint8 opCode = m_state.pChunk->code[m_state.offset];
		const uint8 slot = m_state.pChunk->code[m_state.offset + 1];
		instructionString.Format("{:02X} {:02X} {:2s} {:<16s} {:04d}", opCode, slot, " ", name.GetData(), slot);
		WriteCode(instructionString);
		m_state.offset += 2;

		const Value value{m_state.pChunk->constantValues[slot], m_state.pChunk->constantTypes[slot]};
		FunctionObject* pFunctionObject = AsFunctionObject(value);
		for (uint32 index = 0; index < pFunctionObject->upvalues; ++index)
		{
			StringType upvalueString;
			const uint8 local = m_state.pChunk->code[m_state.offset];
			const uint8 upslot = m_state.pChunk->code[m_state.offset + 1];
			upvalueString.Format("{:02X} {:02X} {:2s} {:<16s} {:04d}", local, upslot, " ", !!local ? " +Local" : " +Upvalue", upslot);
			WriteCode(upvalueString);
			m_state.offset += 2;
		}
	}

	void Disassembler::Write(StringType::ConstView text)
	{
		m_state.buffer += text;
	}

	void Disassembler::WriteLine(StringType::ConstView text)
	{
		StringType output;
		output.Format("{:{}}{}\n", "", Math::Max(0, (m_state.depth - 1) * Disassembler::SpacesPerTab), text);
		m_state.buffer += output;
	}

	void Disassembler::WriteCode(StringType::ConstView text)
	{
		StringType output;
#ifdef SCRIPTING_DEBUG_INFO
		const uint32 line = DebugGetSourceLocation(*m_state.pChunk, m_state.offset).lineNumber;
		if (m_state.offset > 0 && line == DebugGetSourceLocation(*m_state.pChunk, m_state.offset - 1).lineNumber)
		{
			output
				.Format("{:{}}{}{:04d} {}\n", "", Math::Max(0, (m_state.depth - 1) * Disassembler::SpacesPerTab), "   | ", m_state.offset, text);
		}
		else
		{
			output
				.Format("{:{}}{:>4d} {:04d} {}\n", "", Math::Max(0, (m_state.depth - 1) * Disassembler::SpacesPerTab), line, m_state.offset, text);
		}
#else
		output.Format("{:{}}{:04d} {}\n", "", Math::Max(0, (m_state.depth - 1) * Disassembler::SpacesPerTab), m_state.offset, text);
#endif
		m_state.buffer += output;
	}
}
