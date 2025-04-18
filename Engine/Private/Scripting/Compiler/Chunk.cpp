#include "Engine/Scripting/Compiler/Chunk.h"

#include "Engine/Scripting/Compiler/Object.h"

namespace ngine::Scripting
{
	Chunk::Chunk()
		: pObjects(nullptr)
	{
	}

	Chunk::~Chunk()
	{
		GC gc{&SimpleReallocate, &pObjects, 0};
		DeleteObjects(gc);
	}

	void* SimpleReallocate(GC& gc, void* pPointer, size oldSize, size newSize, size alignment)
	{
		UNUSED(gc);
		UNUSED(oldSize);
		if (newSize == 0)
		{
			Memory::Deallocate(pPointer);
			return nullptr;
		}
		return Memory::AllocateAligned(newSize, alignment);
	}

#ifdef SCRIPTING_DEBUG_INFO
	SourceLocation DebugGetSourceLocation(const Chunk& chunk, uint32 offset)
	{
		const uint32 debugInfoCount = chunk.debugInfo.GetSize();
		for (uint32 i = 0; i < debugInfoCount; ++i)
		{
			if (i == debugInfoCount - 1 || offset < chunk.debugInfo[i + 1].offset)
			{
				return chunk.debugInfo[i].sourceLocation;
			}
		}
		return SourceLocation{"Unknown", 0, 0};
	}
#endif
}
