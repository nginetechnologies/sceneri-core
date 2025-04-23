#pragma once

#include <Backend/AssetDatabase.h>
#include <Common/Memory/Containers/Vector.h>

namespace ngine::Networking::Backend
{
	struct InventoryEntry
	{
		uint32 m_assetId;
		uint32 m_instanceId;
	};

	struct Inventory
	{
		void EmplaceEntry(InventoryEntry&& entry)
		{
			m_entries.EmplaceBack(Forward<InventoryEntry>(entry));
		}

		void Clear()
		{
			m_entries.Clear();
		}
	protected:
		Vector<InventoryEntry> m_entries;
	};
}
