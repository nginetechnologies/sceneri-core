#pragma once

#include "Engine/Scripting/Compiler/Value.h"
#include "Engine/Scripting/Compiler/Object.h"
#include "Engine/Scripting/Parser/StringType.h"

#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Optional.h>
#include <Common/EnumFlags.h>
#include <Common/EnumFlagOperators.h>
#include <Common/Math/HashedObject.h>

namespace ngine::Entity
{
	struct SceneRegistry;
}

namespace ngine::Scripting
{
	struct VirtualMachine
	{
	public:
		static constexpr uint32 Version = 1;
		static constexpr uint32 MaxCallStackSize = 0x40;
		static constexpr uint32 MaxStackSize = MaxCallStackSize * 0xFF;
		static constexpr uint32 HeapGrowFactor = 2;

		using GlobalMapType = UnorderedMap<Guid, RawValue, Guid::Hash>;

		enum class Flags : uint8
		{
			Error = 1 << 0
		};
	public:
		VirtualMachine();
		~VirtualMachine();

		void Initialize(const FunctionObject& script);
		void SetEntitySceneRegistry(Entity::SceneRegistry& sceneRegistry);

		bool Execute();
		VM::ReturnValue Execute(VM::Register R0, VM::Register R1, VM::Register R2, VM::Register R3, VM::Register R4, VM::Register R5);
		bool Execute(ArrayView<RawValue, uint8> args, ArrayView<RawValue, uint8> results);
		bool Invoke(ClosureObject& function, ArrayView<RawValue, uint8> args, ArrayView<RawValue, uint8> results);
		void Reset();

		GlobalMapType& GetGlobals();
		GC& GetGC();
	protected:
		bool Run();
		bool Call(ClosureObject& closure, uint8 argCount, uint8 coarity);
		void Call(const VM::DynamicFunction nativeFunctionPointer, uint8 argCount, uint8 coarity);
	private:
		void Push(RawValue value);
		void Pop(int32 count);
		[[nodiscard]] RawValue Pop();
		[[nodiscard]] RawValue Peek(int32 distance);
		[[nodiscard]] UpvalueObject* CaptureUpvalue(RawValue* pLocal);
		void CloseUpvalues(RawValue* pLast);
		[[nodiscard]] bool Error(StringType::ConstView error);
		void ResetStack();
	private:
		static void* Reallocate(GC& gc, void*, size, size, size);
		void CollectGarbage();
		void GCMarkRoots();
		void GCTrace();
		void GCSweep();
	private:
		struct CallFrame
		{
			const uint8* ip;
			RawValue* pSlots;
			ClosureObject* pClosure;
			uint8 coarity;
		};
		struct State
		{
			RawValue* sp;
			Array<CallFrame, MaxCallStackSize, uint32, uint32> frames;
			uint32 frameCount;
			Array<RawValue, MaxStackSize, uint32, uint32> stack;
			GlobalMapType globals;
			UpvalueObject* pOpenUpvalues;
			Object* pObjects;
			GC gc;
			size memoryUsed;
			size memoryLimit;
			Vector<Object*> grayStack;
			EnumFlags<Flags> flags;
		};
		State m_state;
		Optional<const FunctionObject*> m_pScript;
	};

	ENUM_FLAG_OPERATORS(VirtualMachine::Flags);
}
