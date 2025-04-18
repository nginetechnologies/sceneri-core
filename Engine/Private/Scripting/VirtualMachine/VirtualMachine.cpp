#include "Engine/Scripting/VirtualMachine/VirtualMachine.h"
#include "Common/Scripting/VirtualMachine/DynamicFunction/DynamicEvent.h"

#include "Engine/Scripting/Compiler/Chunk.h"
#include "Engine/Scripting/Compiler/Opcode.h"
#include "Engine/Scripting/Compiler/Disassembler.h"
#include "Engine/Entity/HierarchyComponentBase.h"
#include "Engine/Entity/ComponentSoftReference.inl"

#include <Common/Memory/Containers/Format/String.h>
#include <Common/Time/Stopwatch.h>
#include <Common/IO/Log.h>
#include <Common/Reflection/Registry.inl>
#include <Common/Serialization/Guid.h>
#include <Common/Math/Vector4/IsEquivalentTo.h>
#include <Common/Math/Vector4/Abs.h>
#include <Common/Math/Vectorization/Acos.h>
#include <Common/Math/Vectorization/Asin.h>
#include <Common/Math/Vectorization/Atan.h>
#include <Common/Math/Vectorization/Atan2.h>
#include <Common/Math/Vectorization/Ceil.h>
#include <Common/Math/Vectorization/CubicRoot.h>
#include <Common/Math/Vectorization/Cos.h>
#include <Common/Math/Vectorization/Floor.h>
#include <Common/Math/Vectorization/Round.h>
#include <Common/Math/Vectorization/Fract.h>
#include <Common/Math/Vectorization/ISqrt.h>
#include <Common/Math/Vectorization/Log.h>
#include <Common/Math/Vectorization/Log2.h>
#include <Common/Math/Vectorization/Log10.h>
#include <Common/Math/Vector4/Max.h>
#include <Common/Math/Vector4/Min.h>
#include <Common/Math/Vectorization/MultiplicativeInverse.h>
#include <Common/Math/Vectorization/Power.h>
#include <Common/Math/Vector4/Random.h>
#include <Common/Math/Vector4/Sign.h>
#include <Common/Math/Vector4/SignNonZero.h>
#include <Common/Math/Vectorization/Sqrt.h>
#include <Common/Math/Vectorization/Tan.h>
#include <Common/Math/Vector4/Truncate.h>
#include <Common/Math/Vectorization/Mod.h>

// #define SCRIPTING_DEBUG_TRACE_EXECUTION

namespace ngine::Scripting
{
	constexpr bool EnableStressGC = false;

	VirtualMachine::VirtualMachine()
		: m_pScript(nullptr)
	{
		m_state.sp = m_state.stack.GetData();
		m_state.frameCount = 0;
		m_state.pOpenUpvalues = nullptr;
		m_state.pObjects = nullptr;
		m_state.gc = {&VirtualMachine::Reallocate, &m_state.pObjects, uint64(this)};

		m_state.memoryUsed = 0;
		m_state.memoryLimit = 512;
	}

	VirtualMachine::~VirtualMachine()
	{
		DeleteObjects(m_state.gc);
	}

	void VirtualMachine::Initialize(const FunctionObject& script)
	{
		m_pScript = script;

		Reset();
	}

	static Optional<Entity::SceneRegistry*> s_pEntitySceneRegistry;
	void VirtualMachine::SetEntitySceneRegistry(Entity::SceneRegistry& sceneRegistry)
	{
		s_pEntitySceneRegistry = sceneRegistry;
	}

	bool VirtualMachine::Execute()
	{
		ClosureObject* const pClosureObject = CreateClosureObject(m_state.gc, const_cast<FunctionObject*>(m_pScript.Get()));
		Push(RawValue((Object*)pClosureObject));
		Call(*pClosureObject, 0, 0);

		return Run();
	}

	VM::ReturnValue
	VirtualMachine::Execute(VM::Register R0, VM::Register R1, VM::Register R2, VM::Register R3, VM::Register R4, VM::Register R5)
	{
		ClosureObject* const pClosureObject = CreateClosureObject(m_state.gc, const_cast<FunctionObject*>(m_pScript.Get()));
		Push(RawValue{(Object*)pClosureObject});

		const uint8 argumentCount = m_pScript->arity;
		VM::Registers registers{R0, R1, R2, R3, R4, R5};
		Expect(argumentCount <= 6);
		for (uint8 argumentIndex = 0; argumentIndex < argumentCount; argumentIndex++)
		{
			Push(RawValue{registers.m_registers[argumentIndex]});
		}
		const uint8 coarity = m_pScript->coarity;
		VM::ReturnValue returnValue;
		if (Call(*pClosureObject, argumentCount, coarity))
		{
			if (Run())
			{
				// TODO(Ben): Is there a better way to negate the Pop(1) from return?
				++m_state.sp;
				Expect(coarity <= 4);
				uint8 index = coarity;
				while (index--)
				{
					returnValue[index] = *m_state.sp--;
				}
			}
		}
		return returnValue;
	}

	bool VirtualMachine::Execute(const ArrayView<RawValue, uint8> args, const ArrayView<RawValue, uint8> results)
	{
		ClosureObject* const pClosureObject = CreateClosureObject(m_state.gc, const_cast<FunctionObject*>(m_pScript.Get()));
		Push(RawValue{(Object*)pClosureObject});

		const uint8 argCount = uint8(args.GetSize());
		const uint8 coarity = uint8(results.GetSize());
		for (const RawValue arg : args)
		{
			Push(arg);
		}
		if (Call(*pClosureObject, argCount, coarity))
		{
			if (Run())
			{
				// TODO(Ben): Is there a better way to negate the Pop(1) from return?
				++m_state.sp;
				uint8 index = coarity;
				while (index--)
				{
					results[index] = *m_state.sp--;
				}
				return true;
			}
		}
		return false;
	}

	bool VirtualMachine::Invoke(ClosureObject& function, ArrayView<RawValue, uint8> args, ArrayView<RawValue, uint8> results)
	{
		m_state.sp = m_state.stack.GetData();
		Push(RawValue{(Object*)&function});
		const uint8 argCount = uint8(args.GetSize());
		const uint8 coarity = uint8(results.GetSize());
		for (const RawValue arg : args)
		{
			Push(arg);
		}
		if (Call(function, argCount, coarity))
		{
			if (Run())
			{
				// TODO(Ben): Is there a better way to negate the Pop(1) from return?
				++m_state.sp;
				uint8 index = coarity;
				while (index--)
				{
					results[index] = *m_state.sp--;
				}
				return true;
			}
		}
		return false;
	}

	void VirtualMachine::Reset()
	{
		m_state.globals.Clear();

		ResetStack();
		CollectGarbage();

		m_state.memoryLimit = 512;

		Assert(m_state.memoryUsed == 0);
		Assert(m_state.pObjects == nullptr);
	}

	VirtualMachine::GlobalMapType& VirtualMachine::GetGlobals()
	{
		return m_state.globals;
	}

	GC& VirtualMachine::GetGC()
	{
		return m_state.gc;
	}

	[[nodiscard]] FORCE_INLINE Value GetConstantValue(Chunk& chunk, const uint8 constantIndex)
	{
		return Value{chunk.constantValues[constantIndex], chunk.constantTypes[constantIndex]};
	}

	[[nodiscard]] FORCE_INLINE Guid GetConstantGuid(Chunk& chunk, const uint8 constantIndex)
	{
		Assert(chunk.constantTypes[constantIndex] == ValueType::Guid || chunk.constantTypes[constantIndex] == ValueType::NativeFunctionGuid);
		return chunk.constantValues[constantIndex].GetGuid();
	}

	[[nodiscard]] FORCE_INLINE StringObject* GetConstantString(Chunk& chunk, const uint8 constantIndex)
	{
		Assert(chunk.constantTypes[constantIndex] == ValueType::Object);
		const Value value{chunk.constantValues[constantIndex], ValueType::Object};
		Assert(value.GetObject()->type == ObjectType::String);
		return (StringObject*)value.GetObject();
	}

	[[nodiscard]] FORCE_INLINE FunctionObject* GetFunctionIdentifier(Chunk& chunk, const uint8 constantIndex)
	{
		Assert(chunk.constantTypes[constantIndex] == ValueType::Object);
		const Value value{chunk.constantValues[constantIndex], ValueType::Object};
		Assert(value.GetObject()->type == ObjectType::Function);
		return (FunctionObject*)value.GetObject();
	}

	bool VirtualMachine::Run()
	{
		CallFrame* pFrame = &m_state.frames[m_state.frameCount - 1];
		const uint8* ip = pFrame->ip;
		if (ip == nullptr)
		{
			return false;
		}

#define READ_BYTE() (*ip++)
#define READ_SHORT() (ip += 2, uint16((ip[-2] << 8) | ip[-1]))
#define READ_SIGNED_SHORT() (ip += 2, int16((ip[-2] << 8)) | int16(ip[-1]))
#define READ_CONSTANT() GetConstantValue(pFrame->pClosure->pFunction->chunk, READ_BYTE())
#define READ_STRING() GetConstantString(pFrame->pClosure->pFunction->chunk, READ_BYTE())
#define READ_GUID() GetConstantGuid(pFrame->pClosure->pFunction->chunk, READ_BYTE())
#define READ_FUNCTION() GetFunctionIdentifier(pFrame->pClosure->pFunction->chunk, READ_BYTE())

#ifdef SCRIPTING_DEBUG_TRACE_EXECUTION
		Disassembler disassembler;
#endif
		while (true)
		{
#ifdef SCRIPTING_DEBUG_TRACE_EXECUTION
			StringType stackString;
			stackString.Reserve(512);
			stackString.Format("Stack ({})[", m_state.frameCount - 1);
			for (RawValue* pSlot = pFrame->pSlots; pSlot < m_state.sp; ++pSlot)
			{
				stackString += ValueToString(*pSlot);
				if (pSlot != m_state.sp - 1)
				{
					stackString += SCRIPT_STRING_LITERAL(", ");
				}
			}
			stackString += SCRIPT_STRING_LITERAL("]");

			StringType upvaluesString;
			upvaluesString.Reserve(512);
			upvaluesString.Format("Upvalues ({})[", m_state.frameCount - 1);
			const uint32 upvalueCount = pFrame->pClosure->upvalues.GetSize();
			for (uint32 i = 0; i < upvalueCount; ++i)
			{
				upvaluesString += ValueToString(*pFrame->pClosure->upvalues[i]->pLocation);
				if (i != upvalueCount - 1)
				{
					upvaluesString += SCRIPT_STRING_LITERAL(", ");
				}
			}
			upvaluesString += SCRIPT_STRING_LITERAL("]");

			const int64 startOffset = ip - pFrame->pClosure->pFunction->chunk.code.GetData();
			const StringType dissasembly = disassembler.Disassemble(pFrame->pClosure->pFunction->chunk, uint32(startOffset));
			LogMessage("{} <- sp {} \n{}\n{}", stackString, (void*)m_state.sp, upvaluesString, dissasembly);
#endif
			switch (OpCode(READ_BYTE()))
			{
				case OpCode::Nop:
				{
					continue;
				}
				case OpCode::Null:
				{
					Push(RawValue{nullptr});
					continue;
				}
				case OpCode::True:
				{
					Push(RawValue{true});
					continue;
				}
				case OpCode::False:
				{
					Push(RawValue{false});
					continue;
				}
				case OpCode::Return:
				{
					const uint8 resultCount = uint8((uintptr(m_state.sp) - uintptr(pFrame->pSlots)) / sizeof(RawValue)) - 1 // function pointer
					                          - pFrame->pClosure->pFunction->locals;
					//- pFrame->pFunction->arity; // function arguments are also locals
					const uint8 coarity = pFrame->coarity == 0 ? resultCount : pFrame->coarity - 1;

					// save frame pointer to copy return values directly into previous frame
					const RawValue* const pReturnValues = m_state.sp;

					CloseUpvalues(pFrame->pSlots);

					--m_state.frameCount;
					if (UNLIKELY(m_state.frameCount == 0))
					{
						Pop(1);
						return !m_state.flags.IsSet(Flags::Error);
					}

					m_state.sp = pFrame->pSlots;

					if (resultCount == 0)
					{
						// we always expect to have one value
						Push(RawValue{nullptr});
					}
					else
					{
						for (uint8 i = 0; i < Math::Min(resultCount, coarity); ++i)
						{
							Push(pReturnValues[-resultCount + i]);
						}

						for (uint8 count = resultCount; count < coarity; ++count)
						{
							// Fill to expected values
							Push(RawValue{nullptr});
						}
					}

					pFrame = &m_state.frames[m_state.frameCount - 1];
					ip = pFrame->ip;
					continue;
				}
				case OpCode::JumpIfFalse:
				{
					const int16 offset = READ_SIGNED_SHORT();
					if (!Peek(0).AsBool())
					{
						ip += offset;
					}
					continue;
				}
				case OpCode::JumpIfTrue:
				{
					const int16 offset = READ_SIGNED_SHORT();
					if ((bool)Peek(0).AsBool())
					{
						ip += offset;
					}
					continue;
				}
				case OpCode::Jump:
				{
					const int16 offset = READ_SIGNED_SHORT();
					ip += offset;
					continue;
					continue;
				}
				case OpCode::CallNative:
				{
					const uint8 argCount = READ_BYTE();
					const uint8 coarity = READ_BYTE();

					pFrame->ip = ip;

					const RawValue functionValue = Peek(argCount);
					const VM::DynamicFunction functionPointer = functionValue.GetNativeFunctionPointer();
					Call(functionPointer, argCount, coarity);
					pFrame = &m_state.frames[m_state.frameCount - 1];
					ip = pFrame->ip;

					continue;
				}
				case OpCode::CallClosure:
				{
					const uint8 argCount = READ_BYTE();
					const uint8 coarity = READ_BYTE();

					pFrame->ip = ip;
					if (!Call(*AsClosureObject(Peek(argCount).GetObject()), argCount, coarity))
					{
						return false;
					}
					pFrame = &m_state.frames[m_state.frameCount - 1];
					ip = pFrame->ip;

					continue;
				}
				case OpCode::PushConstant:
				{
					const RawValue value = pFrame->pClosure->pFunction->chunk.constantValues[READ_BYTE()];
					Assert(!value.ValidateIsNativeFunctionGuid());
					Push(value);
					continue;
				}
				case OpCode::PushComponentSoftReference:
				{
					Assert(s_pEntitySceneRegistry.IsValid());
					if (UNLIKELY_ERROR(s_pEntitySceneRegistry.IsInvalid()))
					{
						return Error("Component soft references can't be used without a scene registry");
					}

					const RawValue value = pFrame->pClosure->pFunction->chunk.constantValues[READ_BYTE()];
					const Entity::ComponentSoftReference softReference = value.GetComponentSoftReference();
					const Optional<Entity::HierarchyComponentBase*> pComponent =
						softReference.Find<Entity::HierarchyComponentBase>(*s_pEntitySceneRegistry);
					Assert(pComponent.IsValid());
					if (UNLIKELY_ERROR(pComponent.IsInvalid()))
					{
						return Error("Component soft references failed to resolve");
					}

					Push(RawValue{pComponent.Get()});
					continue;
				}
				case OpCode::PushImmediate:
				{
					Push(RawValue{IntegerType{READ_BYTE()}});
					continue;
				}

				// Float operations
				case OpCode::NegateFloat:
				{
					RawValue& __restrict value = *(m_state.sp - 1);
					value = RawValue(-value.GetVector4f());
					continue;
				}
				case OpCode::LogicalNotFloat:
				{
					RawValue& __restrict value = *(m_state.sp - 1);
					value = RawValue(!value.GetVector4f());
					continue;
				}
				case OpCode::AddFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4f() + right.GetVector4f());
					continue;
				}
				case OpCode::SubtractFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4f() - right.GetVector4f());
					continue;
				}
				case OpCode::MultiplyFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4f() * right.GetVector4f());
					continue;
				}
				case OpCode::DivideFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4f() / right.GetVector4f());
					continue;
				}
				case OpCode::LessFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4f().x < right.GetVector4f().x});
					continue;
				}
				case OpCode::LessFloat4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4f() < right.GetVector4f());
					continue;
				}
				case OpCode::LessEqualFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4f().x <= right.GetVector4f().x});
					continue;
				}
				case OpCode::LessEqualFloat4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4f() <= right.GetVector4f());
					continue;
				}
				case OpCode::GreaterFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4f().x > right.GetVector4f().x});
					continue;
				}
				case OpCode::GreaterFloat4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4f() > right.GetVector4f());
					continue;
				}
				case OpCode::GreaterEqualFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4f().x >= right.GetVector4f().x});
					continue;
				}
				case OpCode::GreaterEqualFloat4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4f() >= right.GetVector4f());
					continue;
				}
				case OpCode::NotEqualFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4f().x != right.GetVector4f().x});
					continue;
				}
				case OpCode::NotEqualFloat4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4f() != right.GetVector4f());
					continue;
				}
				case OpCode::EqualEqualFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4f().x == right.GetVector4f().x});
					continue;
				}
				case OpCode::EqualEqualFloat4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4f() == right.GetVector4f());
					continue;
				}
				case OpCode::ModuloFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(Math::Mod(left.GetVector4f(), right.GetVector4f()));
					continue;
				}
				case OpCode::AbsFloat:
				{
					const RawValue right = Pop();
					Push(Math::Abs(right.GetVector4f()));
					continue;
				}
				case OpCode::AcosFloat:
				{
					const RawValue right = Pop();
					Push(Math::Acos(right.GetVector4f()));
					continue;
				}
				case OpCode::AsinFloat:
				{
					const RawValue right = Pop();
					Push(Math::Asin(right.GetVector4f()));
					continue;
				}
				case OpCode::AtanFloat:
				{
					const RawValue right = Pop();
					Push(Math::Atan(right.GetVector4f()));
					continue;
				}
				case OpCode::Atan2Float:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(Math::Atan2(left.GetVector4f(), right.GetVector4f()));
					continue;
				}
				case OpCode::CeilFloat:
				{
					const RawValue right = Pop();
					Push(Math::Ceil(right.GetVector4f()));
					continue;
				}
				case OpCode::CubicRootFloat:
				{
					const RawValue right = Pop();
					Push(Math::CubicRoot(right.GetVector4f()));
					continue;
				}
				case OpCode::CosFloat:
				{
					const RawValue right = Pop();
					Push(Math::Cos(right.GetVector4f()));
					continue;
				}
				case OpCode::RadiansToDegreesFloat:
				{
					const RawValue right = Pop();
					Push(right.GetVector4f() * Math::Vector4f{Math::TConstants<FloatType>::RadToDeg});
					continue;
				}
				case OpCode::ExpFloat:
				{
					const RawValue right = Pop();
					Push(Math::Exponential(right.GetVector4f()));
					continue;
				}
				case OpCode::FloorFloat:
				{
					const RawValue right = Pop();
					Push(Math::Floor(right.GetVector4f()));
					continue;
				}
				case OpCode::RoundFloat:
				{
					const RawValue right = Pop();
					Push(Math::Round(right.GetVector4f()));
					continue;
				}
				case OpCode::FractFloat:
				{
					const RawValue right = Pop();
					Push(Math::Fract(right.GetVector4f()));
					continue;
				}
				case OpCode::InverseSqrtFloat:
				{
					const RawValue right = Pop();
					Push(Math::Isqrt(right.GetVector4f()));
					continue;
				}
				case OpCode::LogFloat:
				{
					const RawValue right = Pop();
					Push(Math::Log(right.GetVector4f()));
					continue;
				}
				case OpCode::Log2Float:
				{
					const RawValue right = Pop();
					Push(Math::Log2(right.GetVector4f()));
					continue;
				}
				case OpCode::Log10Float:
				{
					const RawValue right = Pop();
					Push(Math::Log10(right.GetVector4f()));
					continue;
				}
				case OpCode::MaxFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(Math::Max(left.GetVector4f(), right.GetVector4f()));
					continue;
				}
				case OpCode::MinFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(Math::Min(left.GetVector4f(), right.GetVector4f()));
					continue;
				}
				case OpCode::MultiplicativeInverseFloat:
				{
					const RawValue right = Pop();
					Push(Math::MultiplicativeInverse(right.GetVector4f()));
					continue;
				}
				case OpCode::PowerFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(Math::Power(left.GetVector4f(), right.GetVector4f()));
					continue;
				}
				case OpCode::Power2Float:
				{
					const RawValue right = Pop();
					Push(Math::Power2(right.GetVector4f()));
					continue;
				}
				case OpCode::Power10Float:
				{
					const RawValue right = Pop();
					Push(Math::Power10(right.GetVector4f()));
					continue;
				}
				case OpCode::DegreesToRadiansFloat:
				{
					const RawValue right = Pop();
					Push(right.GetVector4f() * Math::Vector4f{Math::TConstants<FloatType>::DegToRad});
					continue;
				}
				case OpCode::RandomFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(Math::Random(left.GetVector4f(), right.GetVector4f()));
					continue;
				}
				case OpCode::SignFloat:
				{
					const RawValue right = Pop();
					Push(Math::Sign(right.GetVector4f()));
					continue;
				}
				case OpCode::SignNonZeroFloat:
				{
					const RawValue right = Pop();
					Push(Math::SignNonZero(right.GetVector4f()));
					continue;
				}
				case OpCode::SinFloat:
				{
					const RawValue right = Pop();
					Push(Math::Sin(right.GetVector4f()));
					continue;
				}
				case OpCode::SqrtFloat:
				{
					const RawValue right = Pop();
					Push(Math::Sqrt(right.GetVector4f()));
					continue;
				}
				case OpCode::TanFloat:
				{
					const RawValue right = Pop();
					Push(Math::Tan(right.GetVector4f()));
					continue;
				}
				case OpCode::TruncateFloat:
				{
					const RawValue right = Pop();
					Push(Math::Truncate<Math::Vector4i>(right.GetVector4f()));
					continue;
				}
				case OpCode::AreNearlyEqualFloat:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(Math::IsEquivalentTo(left.GetVector4f(), right.GetVector4f()));
					continue;
				}

				// Integral operations
				case OpCode::NegateInteger:
				{
					RawValue& __restrict value = *(m_state.sp - 1);
					value = RawValue(-value.GetVector4i());
					continue;
				}
				case OpCode::LogicalNotInteger:
				{
					RawValue& __restrict value = *(m_state.sp - 1);
					value = RawValue(!value.GetVector4i());
					continue;
				}
				case OpCode::TruthyNotInteger:
				{
					RawValue& __restrict value = *(m_state.sp - 1);
					value = RawValue(false);
					continue;
				}
				case OpCode::FalseyNotInteger:
				{
					RawValue& __restrict value = *(m_state.sp - 1);
					value = RawValue(true);
					continue;
				}
				case OpCode::BitwiseNotInteger:
				{
					RawValue& __restrict value = *(m_state.sp - 1);
					value = RawValue(Math::Vector4i::BoolType{~value.GetBool4i()});
					continue;
				}
				case OpCode::AddInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() + right.GetVector4i());
					continue;
				}
				case OpCode::SubtractInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() - right.GetVector4i());
					continue;
				}
				case OpCode::MultiplyInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() * right.GetVector4i());
					continue;
				}
				case OpCode::DivideInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() / right.GetVector4i());
					continue;
				}
				case OpCode::LessInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4i().x < right.GetVector4i().x});
					continue;
				}
				case OpCode::LessInteger4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() < right.GetVector4i());
					continue;
				}
				case OpCode::LessEqualInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4i().x <= right.GetVector4i().x});
					continue;
				}
				case OpCode::LessEqualInteger4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() <= right.GetVector4i());
					continue;
				}
				case OpCode::GreaterInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4i().x > right.GetVector4i().x});
					continue;
				}
				case OpCode::GreaterInteger4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() > right.GetVector4i());
					continue;
				}
				case OpCode::GreaterEqualInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4i().x >= right.GetVector4i().x});
					continue;
				}
				case OpCode::GreaterEqualInteger4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() >= right.GetVector4i());
					continue;
				}
				case OpCode::NotEqualInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4i().x != right.GetVector4i().x});
					continue;
				}
				case OpCode::NotEqualInteger4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() != right.GetVector4i());
					continue;
				}
				case OpCode::EqualEqualInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4i().x == right.GetVector4i().x});
					continue;
				}
				case OpCode::EqualEqualInteger4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() == right.GetVector4i());
					continue;
				}
				case OpCode::LeftShiftInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() << right.GetVector4i());
					continue;
				}
				case OpCode::RightShiftInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() >> right.GetVector4i());
					continue;
				}
				case OpCode::ModuloInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() % right.GetVector4i());
					continue;
				}
				case OpCode::AndInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() & right.GetVector4i());
					continue;
				}
				case OpCode::OrInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() | right.GetVector4i());
					continue;
				}
				case OpCode::ExclusiveOrInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(left.GetVector4i() ^ right.GetVector4i());
					continue;
				}
				case OpCode::AbsInteger:
				{
					const RawValue right = Pop();
					Push(Math::Abs(right.GetVector4i()));
					continue;
				}
				case OpCode::MaxInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(Math::Max(left.GetVector4i(), right.GetVector4i()));
					continue;
				}
				case OpCode::MinInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(Math::Min(left.GetVector4i(), right.GetVector4i()));
					continue;
				}
				case OpCode::RandomInteger:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(Math::Random(left.GetVector4i(), right.GetVector4i()));
					continue;
				}

				case OpCode::LengthInteger2:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector2i().GetLength()});
					continue;
				}
				case OpCode::LengthInteger3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector3i().GetLength()});
					continue;
				}
				case OpCode::LengthInteger4:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector4i().GetLength()});
					continue;
				}
				case OpCode::LengthSquaredInteger2:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector2i().GetLengthSquared()});
					continue;
				}
				case OpCode::LengthSquaredInteger3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector3i().GetLengthSquared()});
					continue;
				}
				case OpCode::LengthSquaredInteger4:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector4i().GetLengthSquared()});
					continue;
				}

				// String operations
				case OpCode::LessString:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{((StringObject*)left.GetObject())->string.GetView() < ((StringObject*)right.GetObject())->string.GetView()});
					continue;
				}
				case OpCode::LessEqualString:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{((StringObject*)left.GetObject())->string.GetView() <= ((StringObject*)right.GetObject())->string.GetView()});
					continue;
				}
				case OpCode::GreaterString:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{((StringObject*)left.GetObject())->string.GetView() > ((StringObject*)right.GetObject())->string.GetView()});
					continue;
				}
				case OpCode::GreaterEqualString:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{((StringObject*)left.GetObject())->string.GetView() >= ((StringObject*)right.GetObject())->string.GetView()});
					continue;
				}
				case OpCode::NotEqualString:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{((StringObject*)left.GetObject())->string.GetView() != ((StringObject*)right.GetObject())->string.GetView()});
					continue;
				}
				case OpCode::EqualEqualString:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{((StringObject*)left.GetObject())->string.GetView() == ((StringObject*)right.GetObject())->string.GetView()});
					continue;
				}

				// Boolean operations
				case OpCode::LogicalNotBoolean:
				{
					RawValue& __restrict value = *(m_state.sp - 1);
					value = RawValue(!value.GetBool());
					continue;
				}
				case OpCode::AnyBoolean2:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetBool2f().AreAnySet()});
					continue;
				}
				case OpCode::AnyBoolean3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetBool3f().AreAnySet()});
					continue;
				}
				case OpCode::AnyBoolean4:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetBool4f().AreAnySet()});
					continue;
				}
				case OpCode::AllBoolean2:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetBool2f().AreAllSet()});
					continue;
				}
				case OpCode::AllBoolean3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetBool3f().AreAllSet()});
					continue;
				}
				case OpCode::AllBoolean4:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetBool4f().AreAllSet()});
					continue;
				}

				// Vector operations
				case OpCode::Dot2:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector2f().Dot(right.GetVector2f())});
					continue;
				}
				case OpCode::Dot3:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector3f().Dot(right.GetVector3f())});
					continue;
				}
				case OpCode::Dot4:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector4f().Dot(right.GetVector4f())});
					continue;
				}
				case OpCode::Cross2:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector2f().Cross(right.GetVector2f())});
					continue;
				}
				case OpCode::Cross3:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector3f().Cross(right.GetVector3f())});
					continue;
				}
				case OpCode::DistanceInteger2:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{(left.GetVector2i() - right.GetVector2i()).GetLength()});
					continue;
				}
				case OpCode::DistanceInteger3:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{(left.GetVector3i() - right.GetVector3i()).GetLength()});
					continue;
				}
				case OpCode::DistanceFloat2:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{(left.GetVector2f() - right.GetVector2f()).GetLength()});
					continue;
				}
				case OpCode::DistanceFloat3:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{(left.GetVector3f() - right.GetVector3f()).GetLength()});
					continue;
				}
				case OpCode::LengthFloat2:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector2f().GetLength()});
					continue;
				}
				case OpCode::LengthFloat3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector3f().GetLength()});
					continue;
				}
				case OpCode::LengthFloat4:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector4f().GetLength()});
					continue;
				}
				case OpCode::InverseLengthFloat2:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector2f().GetInverseLength()});
					continue;
				}
				case OpCode::InverseLengthFloat3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector3f().GetInverseLength()});
					continue;
				}
				case OpCode::InverseLengthFloat4:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector4f().GetInverseLength()});
					continue;
				}
				case OpCode::LengthSquaredFloat2:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector2f().GetLengthSquared()});
					continue;
				}
				case OpCode::LengthSquaredFloat3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector3f().GetLengthSquared()});
					continue;
				}
				case OpCode::LengthSquaredFloat4:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector4f().GetLengthSquared()});
					continue;
				}
				case OpCode::Normalize2:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector2f().GetNormalizedSafe()});
					continue;
				}
				case OpCode::Normalize3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector3f().GetNormalizedSafe()});
					continue;
				}
				case OpCode::Normalize4:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetVector4f().GetNormalizedSafe()});
					continue;
				}
				case OpCode::Project2:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector2f().Project(right.GetVector2f())});
					continue;
				}
				case OpCode::Project3:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector3f().Project(right.GetVector3f())});
					continue;
				}
				case OpCode::Reflect2:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector2f().Reflect(right.GetVector2f())});
					continue;
				}
				case OpCode::Reflect3:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetVector3f().Reflect(right.GetVector3f())});
					continue;
				}
				case OpCode::Refract2:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					const RawValue eta = Pop();
					Push(RawValue{left.GetVector2f().Refract(right.GetVector2f(), eta.GetDecimal())});
					continue;
				}
				case OpCode::Refract3:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					const RawValue eta = Pop();
					Push(RawValue{left.GetVector3f().Refract(right.GetVector3f(), eta.GetDecimal())});
					continue;
				}

				// Rotation operations
				case OpCode::RightRotationDirection3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetRotation3Df().GetRightColumn()});
					continue;
				}
				case OpCode::ForwardRotationDirection2:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetRotation2Df().GetForwardColumn()});
					continue;
				}
				case OpCode::ForwardRotationDirection3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetRotation3Df().GetForwardColumn()});
					continue;
				}
				case OpCode::UpRotationDirection2:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetRotation2Df().GetUpColumn()});
					continue;
				}
				case OpCode::UpRotationDirection3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetRotation3Df().GetUpColumn()});
					continue;
				}
				case OpCode::RotateRotation2:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetRotation2Df().TransformRotation(right.GetRotation2Df())});
					continue;
				}
				case OpCode::RotateRotation3:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetRotation3Df().TransformRotation(right.GetRotation3Df())});
					continue;
				}
				case OpCode::InverseRotateRotation2:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetRotation2Df().InverseTransformRotation(right.GetRotation2Df())});
					continue;
				}
				case OpCode::InverseRotateRotation3:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetRotation3Df().InverseTransformRotation(right.GetRotation3Df())});
					continue;
				}
				case OpCode::RotateDirection2:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetRotation2Df().TransformDirection(right.GetVector2f())});
					continue;
				}
				case OpCode::RotateDirection3:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetRotation3Df().TransformDirection(right.GetVector3f())});
					continue;
				}
				case OpCode::InverseRotateDirection2:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetRotation2Df().InverseTransformDirection(right.GetVector2f())});
					continue;
				}
				case OpCode::InverseRotateDirection3:
				{
					const RawValue right = Pop();
					const RawValue left = Pop();
					Push(RawValue{left.GetRotation3Df().InverseTransformDirection(right.GetVector3f())});
					continue;
				}
				case OpCode::InverseRotation2:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetRotation2Df().GetInverted()});
					continue;
				}
				case OpCode::InverseRotation3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetRotation3Df().GetInverted()});
					continue;
				}
				case OpCode::NegateRotation3:
				{
					const RawValue right = Pop();
					Push(RawValue{-right.GetRotation3Df()});
					continue;
				}
				case OpCode::RotationEuler3:
				{
					const RawValue right = Pop();
					Push(RawValue{right.GetRotation3Df().GetEulerAngles()});
					continue;
				}

				case OpCode::Pop:
				{
					--m_state.sp;
					continue;
				}
				case OpCode::PushGlobal:
				{
					const Guid identifier = READ_GUID();
					const auto globalIt = m_state.globals.Find(identifier);
					if (globalIt == m_state.globals.end())
					{
						Push(RawValue{nullptr});
						continue;
					}
					Push(globalIt->second);
					continue;
				}
				case OpCode::SetGlobal:
				{
					const Guid identifier = READ_GUID();
					Assert(!Peek(0).ValidateIsNativeFunctionGuid());
					m_state.globals.EmplaceOrAssign(identifier, Peek(0));
					continue;
				}
				case OpCode::PushLocal:
				{
					const uint8 slot = READ_BYTE();
					Push(pFrame->pSlots[slot]);
					continue;
				}
				case OpCode::SetLocal:
				{
					const uint8 slot = READ_BYTE();
					pFrame->pSlots[slot] = Peek(0);
					continue;
				}
				case OpCode::PushClosure:
				{
					FunctionObject* const pFunction = READ_FUNCTION();
					ClosureObject* const pClosure = CreateClosureObject(m_state.gc, pFunction);
					Push(RawValue{(Object*)pClosure});
					for (UpvalueObject*& pUpvalue : pClosure->upvalues)
					{
						const uint8 isLocal = READ_BYTE();
						const uint8 index = READ_BYTE();
						if (!!isLocal)
						{
							pUpvalue = CaptureUpvalue(pFrame->pSlots + index);
						}
						else
						{
							pUpvalue = pFrame->pClosure->upvalues[index];
						}
					}
					continue;
				}
				case OpCode::PushUpvalue:
				{
					const uint8 slot = READ_BYTE();
					Push(*pFrame->pClosure->upvalues[slot]->pLocation);
					continue;
				}
				case OpCode::SetUpvalue:
				{
					const uint8 slot = READ_BYTE();
					*pFrame->pClosure->upvalues[slot]->pLocation = Peek(0);
					continue;
				}
				case OpCode::CloseUpvalue:
				{
					CloseUpvalues(m_state.sp - 1);
					continue;
				}
			}
		}
#undef READ_BYTE
#undef READ_SHORT
#undef READ_SIGNED_SHORT
#undef READ_STRING
	}

	bool VirtualMachine::Call(ClosureObject& closure, uint8 argCount, uint8 coarity)
	{
		if (UNLIKELY_ERROR(m_state.frameCount >= MaxCallStackSize))
		{
			return Error("Stack overflow");
		}

		// automatically adjust function parameters
		const FunctionObject* const pFunction = closure.pFunction;
		for (; argCount < pFunction->arity; ++argCount)
		{
			Push(RawValue{nullptr});
		}
		Pop(argCount - pFunction->arity);

		CallFrame& frame = m_state.frames[m_state.frameCount++];
		frame.ip = pFunction->chunk.code.GetData();
		frame.pSlots = m_state.sp - pFunction->arity - 1;
		frame.pClosure = &closure;
		frame.coarity = coarity;

		return frame.ip != nullptr;
	}

	void VirtualMachine::Call(const VM::DynamicFunction nativeFunctionPointer, uint8 argCount, uint8 coarity)
	{
		// TODO(Ben): How to support varargs for native functions?
		VM::Registers registers;
		const ArrayView<const RawValue, uint8> arguments{m_state.sp - argCount, argCount};
		for (uint8 argumentIndex = 0; argumentIndex < argCount; ++argumentIndex)
		{
			registers[argumentIndex] = arguments[argumentIndex];
		}

		VM::ReturnValue returnValue = nativeFunctionPointer(registers[0], registers[1], registers[2], registers[3], registers[4], registers[5]);
		const ArrayView<RawValue, uint8> returnValues{m_state.sp - argCount - 1, coarity};
		for (uint8 returnValueIndex = 0; returnValueIndex < coarity; ++returnValueIndex)
		{
			returnValues[returnValueIndex] = RawValue{returnValue[returnValueIndex]};
		}

		m_state.sp -= argCount;
	}

	void VirtualMachine::Push(RawValue value)
	{
		*m_state.sp = value;
		++m_state.sp;
	}

	RawValue VirtualMachine::Pop()
	{
		--m_state.sp;
		return *m_state.sp;
	}

	void VirtualMachine::Pop(int32 count)
	{
		m_state.sp -= count;
	}

	RawValue VirtualMachine::Peek(int32 distance)
	{
		return m_state.sp[-1 - distance];
	}

	UpvalueObject* VirtualMachine::CaptureUpvalue(RawValue* pLocal)
	{
		UpvalueObject* pPrevUpvalue = nullptr;
		UpvalueObject* pUpvalue = m_state.pOpenUpvalues;
		while (pUpvalue != nullptr && pUpvalue->pLocation > pLocal)
		{
			pPrevUpvalue = pUpvalue;
			pUpvalue = pUpvalue->pNextUpvalue;
		}

		if (pUpvalue != nullptr && pUpvalue->pLocation == pLocal)
		{
			return pUpvalue;
		}

		UpvalueObject* const pUpvalueObject = CreateUpvalueObject(m_state.gc, pLocal);
		pUpvalueObject->pNextUpvalue = pUpvalue;

		if (pPrevUpvalue == nullptr)
		{
			m_state.pOpenUpvalues = pUpvalueObject;
		}
		else
		{
			pPrevUpvalue->pNextUpvalue = pUpvalueObject;
		}

		return pUpvalueObject;
	}

	void VirtualMachine::CloseUpvalues(RawValue* pLast)
	{
		while (m_state.pOpenUpvalues != nullptr && m_state.pOpenUpvalues->pLocation >= pLast)
		{
			UpvalueObject* pUpvalue = m_state.pOpenUpvalues;
			pUpvalue->closed = *pUpvalue->pLocation;
			pUpvalue->pLocation = &pUpvalue->closed;
			m_state.pOpenUpvalues = pUpvalue->pNextUpvalue;
		}
		Pop(1);
	}

	bool VirtualMachine::Error(StringType::ConstView error)
	{
		LogMessage("{}", StringType(error));
		int32 frameIndex = m_state.frameCount;
		while (frameIndex--)
		{
			CallFrame& frame = m_state.frames[frameIndex];
			const FunctionObject* const pFunction = frame.pClosure->pFunction;

			const uint32 offset = uint32(frame.ip - pFunction->chunk.code.GetData());
#ifdef SCRIPTING_DEBUG_INFO
			System::Get<Log>().Error(
				DebugGetSourceLocation(pFunction->chunk, offset),
				"[{:d}] Before byte {:04d} in {}",
				frameIndex,
				offset,
				ValueToString(Value((Object*)frame.pClosure))
			);
#else
			LogMessage("[{:d}] Before byte {:04d} in {}", frameIndex, offset, ValueToString(ObjectValue((Object*)frame.pClosure)));
#endif
		}

		m_state.flags.Set(Flags::Error);

		ResetStack();

		return false;
	}

	void VirtualMachine::ResetStack()
	{
		m_state.sp = m_state.stack.GetData();
		m_state.frameCount = 0;
		m_state.pOpenUpvalues = nullptr;
	}

	void* VirtualMachine::Reallocate(GC& gc, void* pPointer, size oldSize, size newSize, size alignment)
	{
		if (VirtualMachine* pVm = (VirtualMachine*)gc.customData)
		{
			pVm->m_state.memoryUsed += newSize - oldSize;

			if (newSize > oldSize)
			{
				if constexpr (EnableStressGC)
				{
					pVm->CollectGarbage();
				}
				else
				{
					if (pVm->m_state.memoryUsed > pVm->m_state.memoryLimit)
					{
						pVm->CollectGarbage();
					}
				}
			}
		}

		if (newSize == 0)
		{
			Memory::Deallocate(pPointer);
			return nullptr;
		}

		return Memory::AllocateAligned(newSize, alignment);
	}

	void VirtualMachine::CollectGarbage()
	{
#ifdef SCRIPTING_DEBUG_GC_LOG
		LogMessage("- GC begin");
		const size memoryUsed = m_state.memoryUsed;
#endif
		GCMarkRoots();
		GCTrace();
		GCSweep();

		m_state.memoryLimit = m_state.memoryUsed * HeapGrowFactor;

#ifdef SCRIPTING_DEBUG_GC_LOG
		LogMessage(
			"- GC end\n collected {:d} bytes (from {:d} to {:d}) next gc {:d}",
			memoryUsed - m_state.memoryUsed,
			memoryUsed,
			m_state.memoryUsed,
			m_state.memoryLimit
		);
#endif
	}

	void VirtualMachine::GCMarkRoots()
	{
		// Mark roots
		// Assert(false, "TODO");
		for (RawValue* pSlot = m_state.stack.GetData(); pSlot < m_state.sp; ++pSlot)
		{
			MarkValue(pSlot->GetObject(), m_state.grayStack);
		}

		// Mark globals
		for (const auto& it : m_state.globals)
		{
			MarkValue(it.second.GetObject(), m_state.grayStack);
		}

		// Mark closures
		for (uint32 i = 0; i < m_state.frameCount; ++i)
		{
			MarkObject((Object*)m_state.frames[i].pClosure, m_state.grayStack);
		}

		// Mark upvalues
		for (UpvalueObject* pUpvalue = m_state.pOpenUpvalues; pUpvalue != nullptr; pUpvalue = pUpvalue->pNextUpvalue)
		{
			MarkObject((Object*)pUpvalue, m_state.grayStack);
		}
	}

	void VirtualMachine::GCTrace()
	{
		while (m_state.grayStack.HasElements())
		{
			Object* pObject = m_state.grayStack.PopAndGetBack();
			BlackenObject(pObject, m_state.grayStack);
		}
	}

	void VirtualMachine::GCSweep()
	{
		Object* pPrevious = nullptr;
		Object* pObject = m_state.gc.pObjects ? *m_state.gc.pObjects : nullptr;
		while (pObject != nullptr)
		{
			if (pObject->isMarked)
			{
				pObject->isMarked = false;
				pPrevious = pObject;
				pObject = pObject->pNext;
			}
			else
			{
				Object* pUnreached = pObject;
				pObject = pObject->pNext;
				if (pPrevious != nullptr)
				{
					pPrevious->pNext = pObject;
				}
				else
				{
					*m_state.gc.pObjects = pObject;
				}

				FreeObject(m_state.gc, pUnreached);
			}
		}
	}

	bool FunctionIdentifier::Serialize(const Serialization::Reader reader)
	{
		Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
		*this = reflectionRegistry.FindFunctionIdentifier(*reader.ReadInPlace<Guid>());
		return IsValid();
	}

	bool FunctionIdentifier::Serialize(Serialization::Writer writer) const
	{
		Reflection::Registry& reflectionRegistry = System::Get<Reflection::Registry>();
		return writer.SerializeInPlace(reflectionRegistry.FindFunctionGuid(*this));
	}

	VM::DynamicDelegate FunctionObject::CreateDelegate() const
	{
		const void* pAddress = Memory::GetAddressOf(*this);
		Register userData;
		Memory::Set(&userData, 0, sizeof(Register));
		new (&userData) const void*(pAddress);

		// Store the identifier at the end (same as the function)
		ByteType* pIdentifierAddress = reinterpret_cast<ByteType*>(&userData) + (sizeof(Register) - sizeof(void*));
		new (pIdentifierAddress) const void*(pAddress);

		return Scripting::VM::DynamicDelegate{
			userData,
			Scripting::VM::DynamicFunction{
				[](Register R0, Register R1, Register R2, Register R3, Register R4, Register R5) -> VM::ReturnValue
				{
					VM::Registers registers{R0, R1, R2, R3, R4, R5};
					const FunctionObject* pFunction = registers.ExtractArgument<0, FunctionObject*>();

					UniquePtr<Scripting::VirtualMachine> pVirtualMachine = UniquePtr<Scripting::VirtualMachine>::Make();
					pVirtualMachine->Initialize(*pFunction);
					return pVirtualMachine->Execute(R1, R2, R3, R4, R5, Register{});
				}
			}
		};
	}

	VM::DynamicDelegate FunctionObject::CreateDelegate(void* pObjectAddress) const
	{
		union Data
		{
			Register m_register;
			struct
			{
				const FunctionObject* m_pFunctionObject;
				void* m_pObjectAddress;
			};
		} data;
		static_assert(sizeof(Data) == sizeof(Register));
		Memory::Set(&data.m_register, 0, sizeof(Register));
		data.m_pFunctionObject = this;
		data.m_pObjectAddress = pObjectAddress;

		// Store the object identifier at the end
		ByteType* pIdentifierAddress = reinterpret_cast<ByteType*>(&data.m_register) + (sizeof(Register) - sizeof(void*));
		new (pIdentifierAddress) const void*(pObjectAddress);

		return Scripting::VM::DynamicDelegate{
			data.m_register,
			Scripting::VM::DynamicFunction{
				[](Register R0, Register R1, Register R2, Register R3, Register R4, Register R5) -> VM::ReturnValue
				{
					Data data;
					data.m_register = R0;

					UniquePtr<Scripting::VirtualMachine> pVirtualMachine = UniquePtr<Scripting::VirtualMachine>::Make();
					pVirtualMachine->Initialize(*data.m_pFunctionObject);
					R0 = Scripting::VM::DynamicInvoke::LoadArgument(data.m_pObjectAddress);

					return pVirtualMachine->Execute(R0, R1, R2, R3, R4, R5);
				}
			}
		};
	}

	[[maybe_unused]] const bool wasFunctionTypeRegistered = Reflection::Registry::RegisterType<VM::DynamicFunction>();
	[[maybe_unused]] const bool wasEventTypeRegistered = Reflection::Registry::RegisterType<VM::DynamicEvent>();
}

#undef SCRIPTING_BINARY_OP
#undef SCRIPTING_BINARY_OP_COMPARE

#undef SCRIPTING_DEBUG_TRACE_EXECUTION
