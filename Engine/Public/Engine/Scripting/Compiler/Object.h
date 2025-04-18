#pragma once

#include "Engine/Scripting/Compiler/Value.h"
#include "Engine/Scripting/Compiler/Chunk.h"
#include "Engine/Scripting/Parser/StringType.h"
#include <Common/Scripting/VirtualMachine/DynamicFunction/Register.h>
#include <Common/Scripting/VirtualMachine/DynamicFunction/NativeDelegate.h>

#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Allocators/Allocate.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Math/HashedObject.h>

#include <Common/Memory/ForwardDeclarations/AnyView.h>
#include <Common/Memory/ForwardDeclarations/Any.h>

// #define SCRIPTING_DEBUG_GC_LOG

namespace ngine::Scripting
{
	using Register = VM::Register;

	enum class ObjectType : uint8
	{
		String,
		Function,
		Closure,
		Upvalue
	};

	struct alignas(Register) Object
	{
		ObjectType type;
		bool isMarked;
		Object* pNext;
	};

	[[nodiscard]] inline ObjectType GetObjectType(Value value)
	{
		return value.GetObject()->type;
	}

	[[nodiscard]] inline bool IsObjectType(Value value, ObjectType type)
	{
		return IsObject(value) && value.GetObject()->type == type;
	}

	struct GC;
	Object* AllocateObject(GC& gc, size s, size a, ObjectType type);

	struct StringObject : public Object
	{
		StringType string;
	};

	[[nodiscard]] inline bool IsStringObject(Value value)
	{
		return IsObjectType(value, ObjectType::String);
	}

	[[nodiscard]] inline StringObject* AsStringObject(Value value)
	{
		Assert(IsObjectType(value, ObjectType::String));
		return (StringObject*)value.GetObject();
	}
	[[nodiscard]] inline StringObject* AsStringObject(Object* value)
	{
		Assert(value->type == ObjectType::String);
		return (StringObject*)value;
	}

	[[nodiscard]] inline StringObject* CreateStringObject(GC& gc)
	{
		StringObject* pStringObject = (StringObject*)AllocateObject(gc, sizeof(StringObject), alignof(StringObject), ObjectType::String);
		new (&pStringObject->string) StringType();
		return pStringObject;
	}

	struct FunctionObject : public Object
	{
		[[nodiscard]] VM::DynamicDelegate CreateDelegate() const LIFETIME_BOUND;
		[[nodiscard]] VM::DynamicDelegate CreateDelegate(void* pObject) const LIFETIME_BOUND;

		template<typename ReturnType, typename... ArgumentTypes>
		[[nodiscard]] VM::NativeDelegate<ReturnType(ArgumentTypes...)> CreateDelegate() const
		{
			Assert(sizeof...(ArgumentTypes) == arity);
			Assert(coarity <= 1);
			return {CreateDelegate()};
		}

		uint16 id;
		uint8 arity;
		uint8 coarity;
		uint8 locals;
		uint8 upvalues;
		Chunk chunk;
	};

	constexpr uint32 FunctionObjectScriptId = 0xFF00;

	[[nodiscard]] inline bool IsFunctionObject(Value value)
	{
		return IsObjectType(value, ObjectType::Function);
	}

	[[nodiscard]] inline FunctionObject* AsFunctionObject(Value value)
	{
		Assert(IsObjectType(value, ObjectType::Function));
		return (FunctionObject*)value.GetObject();
	}
	[[nodiscard]] inline FunctionObject* AsFunctionObject(Object* value)
	{
		Assert(value->type == ObjectType::Function);
		return (FunctionObject*)value;
	}

	[[nodiscard]] inline FunctionObject* CreateFunctionObject(GC& gc)
	{
		FunctionObject* pFunctionObject = (FunctionObject*)
			AllocateObject(gc, sizeof(FunctionObject), alignof(FunctionObject), ObjectType::Function);

		pFunctionObject->id = 0;
		pFunctionObject->arity = 0;
		pFunctionObject->coarity = 0;
		pFunctionObject->locals = 0;
		pFunctionObject->upvalues = 0;
		new (&pFunctionObject->chunk) Chunk();

		return pFunctionObject;
	}

	struct UpvalueObject : public Object
	{
		RawValue* pLocation;
		RawValue closed;
		UpvalueObject* pNextUpvalue;
	};

	[[nodiscard]] inline bool IsUpvalueObject(Value value)
	{
		return IsObjectType(value, ObjectType::Upvalue);
	}

	[[nodiscard]] inline UpvalueObject* AsUpvalueObject(Value value)
	{
		Assert(IsObjectType(value, ObjectType::Upvalue));
		return (UpvalueObject*)value.GetObject();
	}
	[[nodiscard]] inline UpvalueObject* AsUpvalueObject(Object* value)
	{
		Assert(value->type == ObjectType::Upvalue);
		return (UpvalueObject*)value;
	}

	[[nodiscard]] inline UpvalueObject* CreateUpvalueObject(GC& gc, RawValue* pSlot)
	{
		UpvalueObject* pUpvalueObject = (UpvalueObject*)AllocateObject(gc, sizeof(UpvalueObject), alignof(UpvalueObject), ObjectType::Upvalue);

		pUpvalueObject->pLocation = pSlot;
		pUpvalueObject->closed = RawValue{nullptr};
		pUpvalueObject->pNextUpvalue = nullptr;

		return pUpvalueObject;
	}

	struct ClosureObject : public Object
	{
		FunctionObject* pFunction;
		Vector<UpvalueObject*> upvalues;
	};

	[[nodiscard]] inline bool IsClosureObject(Value value)
	{
		return IsObjectType(value, ObjectType::Closure);
	}

	[[nodiscard]] inline ClosureObject* AsClosureObject(Value value)
	{
		Assert(IsObjectType(value, ObjectType::Closure));
		return (ClosureObject*)value.GetObject();
	}
	[[nodiscard]] inline ClosureObject* AsClosureObject(Object* value)
	{
		Assert(value->type == ObjectType::Closure);
		return (ClosureObject*)value;
	}

	[[nodiscard]] inline ClosureObject* CreateClosureObject(GC& gc, FunctionObject* pFunction)
	{
		ClosureObject* pClosureObject = (ClosureObject*)AllocateObject(gc, sizeof(ClosureObject), alignof(ClosureObject), ObjectType::Closure);

		pClosureObject->pFunction = pFunction;
		new (&pClosureObject->upvalues) Vector<UpvalueObject*>();
		pClosureObject->upvalues.Resize(pFunction->upvalues);
		for (UpvalueObject*& pUpvalue : pClosureObject->upvalues)
		{
			pUpvalue = nullptr;
		}

		return pClosureObject;
	}

	// For now we use a simple mark-sweep algorithm for garbage collection
	// 1) Find all reachable objects and mark them (start with roots and traverse)
	// 2) All not marked objects are not in use anymore and deleted
	//
	// For the marking we use the tricolor abstractions:
	//  - White: Before garbage collection all objects are white
	//  - Gray : When we reach an object for the first time it is grayed
	//  - Black: If a gray object and what it references is processed it becomes black
	// At the end we are left with black (in-use) and white (free) object that can be kept and deleted accordingly
	using CustomReallocate = void* (*)(GC&, void*, size, size, size);
	struct GC
	{
		void* Reallocate(void* pPointer, size oldSize, size newSize, size alignment)
		{
			return pCustomReallocate(*this, pPointer, oldSize, newSize, alignment);
		}

		CustomReallocate pCustomReallocate;
		Object** pObjects;
		uint64 customData;
	};
	void* SimpleReallocate(GC&, void*, size, size, size);

	inline void MarkObject(Object* pObject, Vector<Object*>& grayStack)
	{
		if (pObject == nullptr)
		{
			return;
		}
		if (pObject->isMarked)
		{
			return;
		}

#ifdef SCRIPTING_DEBUG_GC_LOG
		LogMessage("{:p} mark {}", (void*)pObject, ValueToString(ObjectValue(pObject)));
#endif
		pObject->isMarked = true;
		grayStack.EmplaceBack(pObject);
	}

	inline void MarkValue(Value value, Vector<Object*>& grayStack)
	{
		if (IsObject(value))
		{
			MarkObject(value.GetObject(), grayStack);
		}
	}

	inline void BlackenObject(Object* pObject, Vector<Object*>& grayStack)
	{
#ifdef SCRIPTING_DEBUG_GC_LOG
		LogMessage("{:p} blacken {}", (void*)pObject, ValueToString(ObjectValue(pObject)));
#endif
		switch (pObject->type)
		{
			case ObjectType::String:
				break; // No outgoing references
			case ObjectType::Upvalue:
				MarkValue(((UpvalueObject*)pObject)->closed.GetObject(), grayStack);
				break;
			case ObjectType::Function:
			{
				FunctionObject* pFunction = (FunctionObject*)pObject;
				for (Chunk::ConstantIndex index = 0, count = pFunction->chunk.constantValues.GetSize(); index < count; ++index)
				{
					const Value value{pFunction->chunk.constantValues[index], pFunction->chunk.constantTypes[index]};
					MarkValue(value, grayStack);
				}
				break;
			}
			case ObjectType::Closure:
			{
				ClosureObject* pClosure = (ClosureObject*)pObject;
				MarkObject((Object*)pClosure->pFunction, grayStack);
				for (UpvalueObject* pUpvalue : pClosure->upvalues)
				{
					MarkObject((Object*)pUpvalue, grayStack);
				}
				break;
			}
		}
	}

	[[nodiscard]] inline Object* AllocateObject(GC& gc, size newSize, size alignment, ObjectType type)
	{
		Object* pObject = (Object*)gc.Reallocate(nullptr, 0, newSize, alignment);

		pObject->type = type;
		pObject->isMarked = false;

		if (gc.pObjects)
		{
			pObject->pNext = *gc.pObjects;
			*gc.pObjects = pObject;
		}

#ifdef SCRIPTING_DEBUG_GC_LOG
		LogMessage("{:p} allocate {:d} bytes ({:d})", (void*)pObject, newSize, uint8(type));
#endif

		return pObject;
	}

	inline void FreeObject(GC& gc, Object* pObject)
	{
#ifdef SCRIPTING_DEBUG_GC_LOG
		LogMessage("{:p} free ({})", (void*)pObject, ValueToString(ObjectValue(pObject)));
#endif
		switch (pObject->type)
		{
			case ObjectType::String:
				gc.Reallocate((StringObject*)pObject, sizeof(StringObject), 0, alignof(StringObject));
				break;
			case ObjectType::Function:
				gc.Reallocate((FunctionObject*)pObject, sizeof(FunctionObject), 0, alignof(FunctionObject));
				break;
			case ObjectType::Upvalue:
				gc.Reallocate((UpvalueObject*)pObject, sizeof(UpvalueObject), 0, alignof(UpvalueObject));
				break;
			case ObjectType::Closure:
				gc.Reallocate((ClosureObject*)pObject, sizeof(ClosureObject), 0, alignof(ClosureObject));
				break;
		}
	}

	inline void DeleteObjects(GC& gc)
	{
		Object* pObject = gc.pObjects ? *gc.pObjects : nullptr;
		while (pObject != nullptr)
		{
			Object* pNext = pObject->pNext;
			FreeObject(gc, pObject);
			pObject = pNext;
		}
		gc.pObjects = nullptr;
	}
}
