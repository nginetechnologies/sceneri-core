#pragma once

#include <Common/Memory/Containers/StringView.h>
#include <Common/Serialization/SerializedData.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>
#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Undo/UndoHistory.h>
#include <Common/Function/Event.h>

#include "Scope.h"

namespace ngine
{
	struct Guid;
}

namespace ngine::Entity::Undo
{
	struct History : public ngine::Undo::History<Entry>
	{
		using BaseType = ngine::Undo::History<Entry>;

		void Undo(SceneRegistry& sceneRegistry)
		{
			BaseType::Undo().Undo(sceneRegistry);
		}

		void Redo(SceneRegistry& sceneRegistry)
		{
			BaseType::Redo().Redo(sceneRegistry);
		}

		Event<void(void*), 24> OnEntryAdded;
	protected:
		friend struct Scope;

		void AddEntry(Optional<Entry>&& entry, SceneRegistry& sceneRegistry)
		{
			AddEntry(Move(*entry), sceneRegistry);
		}

		void AddEntry(Entry&& entry, SceneRegistry& sceneRegistry)
		{
			for (Entry *pRemovedEntry = m_pNextEntry + 1, *pEnd = m_entries.end(); pRemovedEntry < pEnd; ++pRemovedEntry)
			{
				pRemovedEntry->FinalizeUndo(sceneRegistry);
			}

			BaseType::AddEntry(Forward<Entry>(entry));

			OnEntryAdded();
		}
	};
}
