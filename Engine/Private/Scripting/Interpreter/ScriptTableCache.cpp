#include "Engine/Scripting/Interpreter/ScriptTableCache.h"

namespace ngine::Scripting
{
	ScriptTableIdentifier ScriptTableCache::AddTable(UniquePtr<ScriptTable>&& pTable)
	{
		ScriptTableIdentifier identifier = m_identifierStorage.AcquireIdentifier();
		if (identifier.IsValid())
		{
			m_tables.Construct(identifier, Forward<UniquePtr<ScriptTable>>(pTable));
		}
		Assert(identifier.IsValid());
		return identifier;
	}

	Optional<ScriptTable*> ScriptTableCache::GetTable(ScriptTableIdentifier identifier) const
	{
		Assert(m_identifierStorage.IsIdentifierActive(identifier));
		return *m_tables[identifier];
	}

	void ScriptTableCache::RemoveTable(ScriptTableIdentifier identifier)
	{
		Assert(m_identifierStorage.IsIdentifierActive(identifier));
		m_tables[identifier] = nullptr;
		m_identifierStorage.ReturnIdentifier(identifier);
	}

	void ScriptTableCache::Clear()
	{
		m_identifierStorage.Reset();
		m_tables.DestroyAll();
	}

	int32 ScriptTableCache::AddReference(ScriptTableIdentifier identifier)
	{
		Assert(m_identifierStorage.IsIdentifierActive(identifier));
		return ++m_refcounts[identifier];
	}

	int32 ScriptTableCache::RemoveReference(ScriptTableIdentifier identifier)
	{
		Assert(m_identifierStorage.IsIdentifierActive(identifier));
		Assert(m_refcounts[identifier] > 0);
		const int32 count = --m_refcounts[identifier];
		if (count == 0)
		{
			RemoveTable(identifier);
		}
		return count;
	}
}

namespace ngine
{
	template struct TSaltedIdentifierStorage<Scripting::ScriptTableIdentifier>;
	template struct TIdentifierArray<UniquePtr<Scripting::ScriptTable>, Scripting::ScriptTableIdentifier>;
	template struct TIdentifierArray<int32, Scripting::ScriptTableIdentifier>;
}
