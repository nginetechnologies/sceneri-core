#pragma once

#include "Entry.h"
#include <Widgets/Style/ForwardDeclarations/ModifierMatch.h>

#include <Common/Memory/Containers/Array.h>
#include <Common/Memory/Containers/FlatVector.h>

namespace ngine::Widgets::Style
{
	struct CombinedEntry
	{
		template<typename... Entries>
		CombinedEntry(const Entries&... entries)
			: m_entries(entries...)
		{
			for (const ReferenceWrapper<const Entry>& entry : m_entries)
			{
				m_valueTypeMask |= entry->GetValueTypeMask();
				m_dynamicValueTypeMask |= entry->GetDynamicValueTypeMask();
			}
		}

		using MatchingModifiers = CombinedMatchingEntryModifiers;
		using MatchingModifiersView = CombinedMatchingEntryModifiersView;

		FlatVector<ReferenceWrapper<const Entry>, CombinedEntrySize> m_entries;
		// TODO: These masks need to be in sync with any changes to the entries
		// Currently they are not and that's why we disable the early out checks in the methods below
		Entry::ValueTypeMask m_valueTypeMask;
		Entry::ValueTypeMask m_dynamicValueTypeMask;

		[[nodiscard]] MatchingModifiers GetMatchingModifiers(const EnumFlags<Modifier> requestedModifiers) const LIFETIME_BOUND
		{
			MatchingModifiers matchingModifiers;
			matchingModifiers.Resize(m_entries.GetSize());

			for (const ReferenceWrapper<const Entry>& entry : m_entries)
			{
				const uint8 entryIndex = m_entries.GetIteratorIndex(Memory::GetAddressOf(entry));
				matchingModifiers[entryIndex] = entry->GetMatchingModifiers(requestedModifiers);
			}

			return matchingModifiers;
		}

		[[nodiscard]] bool Contains(const ValueTypeIdentifier identifier, const MatchingModifiersView matchingModifiers) const
		{
			// if (m_valueTypeMask.IsSet((uint8)identifier))
			{
				for (const ReferenceWrapper<const Entry>& entry : m_entries)
				{
					const uint8 entryIndex = m_entries.GetIteratorIndex(Memory::GetAddressOf(entry));
					if (entry->Contains(identifier, matchingModifiers[entryIndex].GetView()))
					{
						return true;
					}
				}
			}
			return false;
		}

		[[nodiscard]] bool HasAnyModifiers(const EnumFlags<Modifier> mask) const
		{
			for (const Entry& entry : m_entries)
			{
				if (entry.GetModifierMask().AreAnySet(mask))
				{
					return true;
				}
			}
			return false;
		}

		[[nodiscard]] Optional<const EntryValue*>
		Find(const ValueTypeIdentifier identifier, const MatchingModifiersView matchingModifiers) const
		{
			// if (m_valueTypeMask.IsSet((uint8)identifier))
			{
				for (const ReferenceWrapper<const Entry>& entry : m_entries)
				{
					const uint8 entryIndex = m_entries.GetIteratorIndex(Memory::GetAddressOf(entry));
					Optional<const EntryValue*> value = entry->Find(identifier, matchingModifiers[entryIndex].GetView());
					if (value.IsValid())
					{
						return value;
					}
				}
			}
			return {};
		}

		template<typename Type>
		[[nodiscard]] Type
		GetWithDefault(const ValueTypeIdentifier identifier, Type&& defaultValue, const MatchingModifiersView matchingModifiers) const
		{
			// if (m_valueTypeMask.IsSet((uint8)identifier))
			{
				if (const Optional<const EntryValue*> entryValue = Find(identifier, matchingModifiers))
				{
					if (const Optional<const Type*> value = entryValue->Get<Type>())
					{
						return *value;
					}
				}
			}
			return Forward<Type>(defaultValue);
		}

		template<typename Type>
		[[nodiscard]] Optional<const Type*> Find(const ValueTypeIdentifier identifier, const MatchingModifiersView matchingModifiers) const
		{
			if (const Optional<const EntryValue*> entryValue = Find(identifier, matchingModifiers))
			{
				return entryValue->Get<Type>();
			}

			return Invalid;
		}

		// DEPRECATED. TODO: Remove
		template<typename Type>
		[[nodiscard]] Type Get(const ValueTypeIdentifier identifier, const MatchingModifiersView matchingModifiers) const
		{
			return *Find<Type>(identifier, matchingModifiers);
		}

		[[nodiscard]] Size GetSize(const ValueTypeIdentifier identifier, const MatchingModifiersView matchingModifiers) const
		{
			return Get<Size>(identifier, matchingModifiers);
		}
		[[nodiscard]] Size GetSize(const ValueTypeIdentifier identifier, const Size size, const MatchingModifiersView matchingModifiers) const
		{
			return GetWithDefault<Size>(identifier, Size(size), matchingModifiers);
		}
		[[nodiscard]] Optional<const Size*> FindSize(const ValueTypeIdentifier identifier, const MatchingModifiersView matchingModifiers) const
		{
			return Find<Size>(identifier, matchingModifiers);
		}
	};
}
