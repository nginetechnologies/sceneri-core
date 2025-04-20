#pragma once

#include <Common/Memory/Containers/ForwardDeclarations/InlineVector.h>
#include <Common/Memory/Containers/ForwardDeclarations/FlatVector.h>
#include <Common/Memory/Containers/ForwardDeclarations/ArrayView.h>

namespace ngine::Widgets::Style
{
	struct EntryModifierMatch;

	//! Sorted vector containing indices to m_modifierValues
	//! [0] is the best match
	using MatchingEntryModifiers = InlineVector<EntryModifierMatch, 8>;
	using MatchingEntryModifiersView = ArrayView<const EntryModifierMatch, uint8>;

	// Style (from stylesheet), inline style and style override
	inline static constexpr uint8 CombinedEntrySize = 3;

	using CombinedMatchingEntryModifiers = FlatVector<MatchingEntryModifiers, CombinedEntrySize>;
	using CombinedMatchingEntryModifiersView = ArrayView<const MatchingEntryModifiers, uint8>;
}
