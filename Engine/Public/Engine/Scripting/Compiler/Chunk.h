#pragma once

#include "Engine/Scripting/Compiler/Value.h"

#include <Common/Memory/Containers/Vector.h>
#include <Common/SourceLocation.h>

#define SCRIPTING_DEBUG_INFO 1

namespace ngine::Scripting
{
	struct GC;
	void DeleteObjects(GC& gc);
}

namespace ngine::Scripting
{
	struct Chunk
	{
		Chunk();
		~Chunk();

		Chunk(const Chunk& other) = delete;
		Chunk& operator=(const Chunk& other) = delete;

		Vector<uint8> code;

		// TODO: Increase maximum number of constants, currently tied to byte code limit
		using ConstantIndex = uint8;
		Vector<RawValue, ConstantIndex> constantValues;
		Vector<ValueType, ConstantIndex> constantTypes;

		Object* pObjects;

#ifdef SCRIPTING_DEBUG_INFO
		struct DebugInfo
		{
			SourceLocation sourceLocation;
			uint16 offset;
		};
		Vector<DebugInfo> debugInfo;
#endif
	};

#ifdef SCRIPTING_DEBUG_INFO
	[[nodiscard]] SourceLocation DebugGetSourceLocation(const Chunk& chunk, uint32 offset);
#endif
}
