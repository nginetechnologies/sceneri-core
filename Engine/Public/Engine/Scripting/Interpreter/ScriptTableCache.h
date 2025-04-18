#pragma once

#include "Engine/Scripting/Parser/ScriptValue.h"
#include "Engine/Scripting/Parser/ScriptTable.h"

#include <Common/Storage/SaltedIdentifierStorage.h>
#include <Common/Storage/IdentifierArray.h>

#include <Common/Memory/UniquePtr.h>

namespace ngine
{
	extern template struct TSaltedIdentifierStorage<Scripting::ScriptTableIdentifier>;
	extern template struct TIdentifierArray<UniquePtr<Scripting::ScriptTable>, Scripting::ScriptTableIdentifier>;
	extern template struct TIdentifierArray<int32, Scripting::ScriptTableIdentifier>;
}

namespace ngine::Scripting
{
	struct ScriptTableCache
	{
	public:
		[[nodiscard]] ScriptTableIdentifier AddTable(UniquePtr<ScriptTable>&& pTable);
		[[nodiscard]] Optional<ScriptTable*> GetTable(ScriptTableIdentifier identifier) const;
		void RemoveTable(ScriptTableIdentifier identifier);
		void Clear();
	protected:
		friend class ManagedScriptTableIdentifier;
		[[nodiscard]] int32 AddReference(ScriptTableIdentifier identifier);
		[[nodiscard]] int32 RemoveReference(ScriptTableIdentifier identifier);
	private:
		TSaltedIdentifierStorage<ScriptTableIdentifier> m_identifierStorage;
		TIdentifierArray<UniquePtr<ScriptTable>, ScriptTableIdentifier> m_tables;
		TIdentifierArray<int32, ScriptTableIdentifier> m_refcounts;
	};
}
