#include "Engine/Scripting/Interpreter/ScriptFunctionCache.h"

#include "Engine/Scripting/Interpreter/Interpreter.h"
#include "Engine/Scripting/Parser/ScriptValue.h"

#include <Common/Math/NumericLimits.h>
#include <Common/Time/Stopwatch.h>
#include <Common/Memory/Containers/InlineVector.h>
#include <Common/Math/Floor.h>
#include <Common/Math/Ceil.h>
#include <Common/Math/Constants.h>
#include <Common/Math/Random.h>
#include <Common/IO/Log.h>

namespace ngine::Scripting
{
	void ScriptFunctionCache::AddFunction(const FunctionIdentifier identifier, UniquePtr<ScriptFunction>&& pFunction)
	{
		m_functions.Construct(identifier, Forward<UniquePtr<ScriptFunction>>(pFunction));
	}

	Optional<ScriptFunction*> ScriptFunctionCache::GetFunction(FunctionIdentifier identifier) const
	{
		return m_functions[identifier].Get();
	}

	void ScriptFunctionCache::RemoveFunction(const FunctionIdentifier identifier)
	{
		m_functions[identifier] = nullptr;
	}

	void ScriptFunctionCache::Clear()
	{
		m_functions.DestroyAll();
	}

	int32 ScriptFunctionCache::AddReference(FunctionIdentifier identifier)
	{
		return ++m_refcounts[identifier];
	}

	int32 ScriptFunctionCache::RemoveReference(FunctionIdentifier identifier)
	{
		Assert(m_refcounts[identifier] > 0);
		const int32 count = --m_refcounts[identifier];
		if (count == 0)
		{
			RemoveFunction(identifier);
		}
		return count;
	}
}
