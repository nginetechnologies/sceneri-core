#pragma once

#include "Engine/Scripting/Parser/ScriptValue.h"

#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/UnorderedMap.h>

namespace ngine::Scripting
{
	struct ScriptTable
	{
	public:
		using SequenceType = Vector<ScriptValue>;
		using MapType = UnorderedMap<ScriptValue, ScriptValue, ScriptValue::Hash>;
	public:
		explicit ScriptTable() noexcept;

		ScriptTable(const ScriptTable& other) noexcept;
		ScriptTable(ScriptTable&& other) noexcept;

		void Set(ScriptValue&& key, ScriptValue&& value);
		ScriptValue Get(const ScriptValue& key) const;

		const MapType& Get() const;

		ScriptTable& operator=(const ScriptTable& other) noexcept;
		ScriptTable& operator=(ScriptTable&& other) noexcept;
	private:
		// SequenceType m_sequence;
		MapType m_values;
	};
}
