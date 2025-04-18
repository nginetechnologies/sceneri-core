#include "Engine/Scripting/Parser/ScriptTable.h"

#include <Common/IO/Log.h>

namespace ngine::Scripting
{
	ScriptTable::ScriptTable() noexcept
	{
	}

	ScriptTable::ScriptTable(const ScriptTable& other) noexcept
		//: m_sequence(other.m_sequence)
		: m_values(other.m_values)
	{
	}

	ScriptTable::ScriptTable(ScriptTable&& other) noexcept
		//: m_sequence(Move(other.m_sequence))
		: m_values(Move(other.m_values))
	{
	}

	void ScriptTable::Set(ScriptValue&& key, ScriptValue&& value)
	{
		if (key.Get().Is<nullptr_type>())
		{
			LogError("'nil' can't be used as table index");
			return;
		}

		// TODO (Ben): Implement optimization for sequence
		auto valueIt = m_values.Find(key);
		if (valueIt != m_values.end())
		{
			valueIt->second = Forward<ScriptValue>(value);
		}
		else
		{
			m_values.Emplace(Forward<ScriptValue>(key), Forward<ScriptValue>(value));
		}
	}

	ScriptValue ScriptTable::Get(const ScriptValue& key) const
	{
		if (key.Get().Is<nullptr_type>())
		{
			LogError("'nil' can't be used as table index");
			return ScriptValue();
		}

		auto valueIt = m_values.Find(key);
		if (valueIt != m_values.end())
		{
			return valueIt->second;
		}
		return ScriptValue(nullptr);
	}

	const ScriptTable::MapType& ScriptTable::Get() const
	{
		return m_values;
	}

	ScriptTable& ScriptTable::operator=(const ScriptTable& other) noexcept
	{
		if (this != &other)
		{
			// m_sequence = other.m_sequence;
			m_values = other.m_values;
		}
		return *this;
	}
	ScriptTable& ScriptTable::operator=(ScriptTable&& other) noexcept
	{
		if (this != &other)
		{
			// m_sequence = Move(other.m_sequence);
			m_values = Move(other.m_values);
		}
		return *this;
	}
}
