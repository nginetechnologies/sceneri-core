#pragma once

#include "Engine/Scripting/Parser/ScriptValue.h"
#include "Common/Scripting/VirtualMachine/FunctionIdentifier.h"

#include <Common/Memory/Containers/String.h>
#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Optional.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Memory/SharedPtr.h>

namespace ngine::Scripting
{
	struct Token;
	struct ScriptFunction;
	struct ScriptTable;
	struct ScriptFunctionCache;
	struct ScriptTableCache;
}

namespace ngine::Scripting
{
	struct Environment
	{
	public:
		using ScriptValueMap = UnorderedMap<Guid, ScriptValue, Guid::Hash>;
		static SharedPtr<Environment> Create(ScriptFunctionCache& functionCache, ScriptTableCache& tableCache);
	public:
		Environment(ScriptFunctionCache& functions, ScriptTableCache& tables) noexcept;
		Environment(SharedPtr<Environment> pEnclosing) noexcept;
		Environment(const Environment& other) = delete;
		Environment(Environment&& other) noexcept;

		void SetValue(const Guid identifier, ScriptValue&& value);
		void SetValueAt(int32 distance, const Guid identifier, ScriptValue&& value);

		[[nodiscard]] ScriptValue GetValue(const Guid identifier);
		[[nodiscard]] ScriptValue GetValueAt(int32 distance, const Guid identifier);

		[[nodiscard]] SharedPtr<Environment> GetGlobalEnvironment() const;
		[[nodiscard]] const ScriptValueMap& GetValues() const;

		void AddFunction(
			const FunctionIdentifier identifier,
			StringType::ConstView name,
			const Guid guid,
			UniquePtr<ScriptFunction>&& pScriptFunction,
			Optional<ScriptTable*> pTable = {}
		);
		void AddFunction(const FunctionIdentifier identifier, UniquePtr<ScriptFunction>&& pScriptFunction);
		[[nodiscard]] Optional<ScriptFunction*> GetFunction(FunctionIdentifier identifier) const;
		[[nodiscard]] Optional<ScriptFunction*>
		GetFunction(StringType::ConstView name, const Guid identifier, Optional<ScriptTable*> pTable = {}) const;

		[[nodiscard]] ManagedScriptTableIdentifier AddTable(UniquePtr<ScriptTable>&& pScriptTable);
		[[nodiscard]] Optional<ScriptTable*> AddTable(StringType::ConstView name, const Guid identifier, Optional<ScriptTable*> pTable = {});
		[[nodiscard]] Optional<ScriptTable*> GetTable(ScriptTableIdentifier identifier) const;
		[[nodiscard]] Optional<ScriptTable*>
		GetTable(StringType::ConstView name, const Guid identifier, Optional<ScriptTable*> pTable = {}) const;

		Environment& operator=(const Environment& other) = delete;
		Environment& operator=(Environment&& other) = delete;
	protected:
		SharedPtr<Environment> m_pEnclosing;
	private:
		ScriptFunctionCache& m_functions;
		ScriptTableCache& m_tables;
		ScriptValueMap m_values;
	};
}
