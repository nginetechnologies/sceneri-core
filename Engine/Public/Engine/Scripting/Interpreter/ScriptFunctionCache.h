#pragma once

#include "Engine/Scripting/Parser/ScriptFunction.h"
#include "Common/Scripting/VirtualMachine/FunctionIdentifier.h"

#include <Common/Storage/IdentifierArray.h>

#include <Common/Memory/UniquePtr.h>

namespace ngine::Scripting
{
	struct ScriptFunctionCache
	{
	public:
		void AddFunction(const FunctionIdentifier functionIdentifier, UniquePtr<ScriptFunction>&& pFunction);
		[[nodiscard]] Optional<ScriptFunction*> GetFunction(FunctionIdentifier identifier) const;
		void RemoveFunction(const FunctionIdentifier functionIdentifier);
		void Clear();
	protected:
		friend class ManagedScriptFunctionIdentifier;
		[[nodiscard]] int32 AddReference(const FunctionIdentifier functionIdentifier);
		[[nodiscard]] int32 RemoveReference(const FunctionIdentifier functionIdentifier);
	private:
		TIdentifierArray<UniquePtr<ScriptFunction>, FunctionIdentifier> m_functions;
		TIdentifierArray<int32, FunctionIdentifier> m_refcounts;
	};
}
