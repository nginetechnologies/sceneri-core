#pragma once

#include <Common/Memory/Containers/UnorderedMap.h>
#include <Common/Memory/Containers/Vector.h>
#include <Common/Memory/Containers/RestrictedArrayView.h>
#include <Common/Memory/Bitset.h>
#include <Common/Memory/UniquePtr.h>
#include <Common/Algorithms/Sort.h>

#include <Widgets/Style/Value.h>
#include <Widgets/Style/Values.h>
#include <Widgets/Style/Size.h>
#include <Widgets/Style/Modifier.h>
#include <Widgets/Style/ForwardDeclarations/ModifierMatch.h>

namespace ngine::Widgets::Style
{
	struct ComputedStylesheet;

	using ModifierIndexType = uint8;
	struct TRIVIAL_ABI alignas(uint16) EntryModifierMatch
	{
		EntryModifierMatch(const ModifierIndexType index, const ModifierIndexType matchCount)
			: m_index(index)
			, m_matchCount(matchCount)
		{
		}

		[[nodiscard]] bool operator>(const EntryModifierMatch& other) const
		{
			// Prefer matches later in the vector
			if (m_matchCount == other.m_matchCount)
			{
				return m_index > other.m_index;
			}

			return m_matchCount > other.m_matchCount;
		}

		struct
		{
			ModifierIndexType m_index;
			ModifierIndexType m_matchCount;
		};
	};

	using EntryValueIdentifier = ValueTypeIdentifier;

	struct Entry
	{
		using ModifierMatch = EntryModifierMatch;
		using MatchingModifiers = MatchingEntryModifiers;
		using MatchingModifiersView = MatchingEntryModifiersView;

		using Value = EntryValue;
		using ValueIdentifier = EntryValueIdentifier;
		using ValueContainer = Array<UniquePtr<Value>, (uint8)ValueIdentifier::Count, ValueIdentifier>;
		using ValueView = ValueContainer::View;
		using ConstValueView = ValueContainer::ConstView;

		Entry()
		{
		}
		Entry(const Entry&) = default;
		Entry& operator=(const Entry&) = default;
		Entry(Entry&&) = default;
		Entry& operator=(Entry&&) = default;
		Entry(const ConstValuesView values)
		{
			if (values.HasElements())
			{
				ModifierValues& modifierValues = m_modifierValues.EmplaceBack(ModifierValues{Modifier::None});
				for (const Style::Value& __restrict value : values)
				{
					modifierValues.Emplace(Style::Value{value});
				}

				m_valueTypeMask = modifierValues.GetValueTypeMask();
				m_dynamicValueTypeMask = modifierValues.GetDynamicValueTypeMask();
			}
		}
		template<uint32 ElementCount>
		Entry(const FixedArrayView<const Value, ElementCount> values)
		{
			if (values.HasElements())
			{
				ModifierValues& modifierValues = m_modifierValues.EmplaceBack(ModifierValues{Modifier::None});
				for (const Style::Value& __restrict value : values)
				{
					modifierValues.Emplace(Style::Value{value});
				}

				m_valueTypeMask = modifierValues.GetValueTypeMask();
				m_dynamicValueTypeMask = modifierValues.GetDynamicValueTypeMask();
			}
		}

		[[nodiscard]] static const Entry& GetDummy()
		{
			static const Entry dummyEntry;
			return dummyEntry;
		}

		[[nodiscard]] bool Contains(const ValueTypeIdentifier identifier, const MatchingModifiersView matchingModifiers) const
		{
			if (m_valueTypeMask.IsSet((uint8)identifier))
			{
				const RestrictedArrayView<const ModifierValues, ModifierIndexType> modifierValues = m_modifierValues.GetView();
				for (const ModifierMatch modifierMatch : matchingModifiers)
				{
					if (modifierValues[modifierMatch.m_index].Contains(identifier))
					{
						return true;
					}
				}
			}
			return false;
		}

		[[nodiscard]] Optional<const EntryValue*>
		Find(const ValueTypeIdentifier identifier, const MatchingModifiersView matchingModifiers) const LIFETIME_BOUND
		{
			if (m_valueTypeMask.IsSet((uint8)identifier))
			{
				const RestrictedArrayView<const ModifierValues, ModifierIndexType> modifierValues = m_modifierValues.GetView();
				for (const ModifierMatch modifierMatch : matchingModifiers)
				{
					if (Optional<const EntryValue*> pValue = modifierValues[modifierMatch.m_index].Find(identifier))
					{
						return pValue;
					}
				}
			}
			return Invalid;
		}
		[[nodiscard]] Optional<EntryValue*>
		Find(const ValueTypeIdentifier identifier, const MatchingModifiersView matchingModifiers) LIFETIME_BOUND
		{
			if (m_valueTypeMask.IsSet((uint8)identifier))
			{
				const RestrictedArrayView<ModifierValues, ModifierIndexType> modifierValues = m_modifierValues.GetView();
				for (const ModifierMatch modifierMatch : matchingModifiers)
				{
					if (Optional<EntryValue*> pValue = modifierValues[modifierMatch.m_index].Find(identifier))
					{
						return pValue;
					}
				}
			}
			return Invalid;
		}

		template<typename Type>
		[[nodiscard]] Type
		GetWithDefault(const ValueTypeIdentifier identifier, Type&& defaultValue, const MatchingModifiersView matchingModifiers) const
		{
			if (Optional<const EntryValue*> value = Find(identifier, matchingModifiers))
			{
				return *value->Get<Type>();
			}
			return Forward<Type>(defaultValue);
		}

		template<typename Type>
		[[nodiscard]] Optional<const Type*> Find(const ValueTypeIdentifier identifier, const MatchingModifiersView matchingModifiers) const
		{
			if (Optional<const EntryValue*> entryValue = Find(identifier, matchingModifiers))
			{
				if (const Optional<const Type*> value = entryValue->Get<Type>())
				{
					return value;
				}
			}
			return {};
		}

		template<typename Type>
		[[nodiscard]] Optional<Type*> Find(const ValueTypeIdentifier identifier, const MatchingModifiersView matchingModifiers)
		{
			if (Optional<EntryValue*> entryValue = Find(identifier, matchingModifiers))
			{
				if (const Optional<Type*> value = entryValue->Get<Type>())
				{
					return value;
				}
			}
			return Invalid;
		}

		// DEPRECATED. TODO: Remove
		template<typename Type>
		[[nodiscard]] Type Get(const ValueTypeIdentifier identifier, const MatchingModifiersView matchingModifiers) const
		{
			return *Find<Type>(identifier, matchingModifiers);
		}

		bool Serialize(const Serialization::Reader reader);

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

		using ValueTypeMask = Bitset<(uint8)ValueTypeIdentifier::Count>;

		struct ModifierValues
		{
			ModifierValues(const EnumFlags<Modifier> requiredModifiers)
				: m_requiredModifiers(requiredModifiers)
			{
			}
			ModifierValues(const ModifierValues& other)
				: m_requiredModifiers(other.m_requiredModifiers)
				, m_valueTypeMask(other.m_valueTypeMask)
				, m_dynamicValueTypeMask(other.m_dynamicValueTypeMask)
			{
				for (const UniquePtr<Value>& pValue : other.m_values)
				{
					if (pValue.IsValid())
					{
						const ValueIdentifier index = other.m_values.GetIteratorIndex(&pValue);
						m_values[index] = UniquePtr<Value>::Make(*pValue);
					}
				}
			}
			ModifierValues& operator=(const ModifierValues& other)
			{
				m_requiredModifiers = other.m_requiredModifiers;
				for (const UniquePtr<Value>& pValue : other.m_values)
				{
					if (pValue.IsValid())
					{
						const ValueIdentifier index = other.m_values.GetIteratorIndex(&pValue);
						m_values[index] = UniquePtr<Value>::Make(*pValue);
					}
				}
				m_valueTypeMask = other.m_valueTypeMask;
				m_dynamicValueTypeMask = other.m_dynamicValueTypeMask;
				return *this;
			}
			ModifierValues(ModifierValues&&) = default;
			ModifierValues& operator=(ModifierValues&&) = default;

			void ParseFromCSS(const ConstStringView styleString);
			[[nodiscard]] String ToString() const;

			[[nodiscard]] Optional<EntryValue*> Find(const ValueTypeIdentifier identifier) LIFETIME_BOUND
			{
				return m_values[identifier].Get();
			}
			[[nodiscard]] Optional<const EntryValue*> Find(const ValueTypeIdentifier identifier) const LIFETIME_BOUND
			{
				return m_values[identifier].Get();
			}

			[[nodiscard]] bool Contains(const ValueTypeIdentifier identifier) const
			{
				return m_valueTypeMask.IsSet((uint8)identifier);
			}
			[[nodiscard]] bool IsDynamic(const ValueTypeIdentifier identifier) const
			{
				return m_dynamicValueTypeMask.IsSet((uint8)identifier);
			}

			template<typename Type>
			[[nodiscard]] Optional<Type*> Find(const ValueTypeIdentifier identifier) LIFETIME_BOUND
			{
				if (Optional<EntryValue*> value = Find(identifier))
				{
					if (Optional<Type*> pValue = value->Get<Type>())
					{
						return *pValue;
					}
				}
				return Invalid;
			}

			template<typename Type>
			[[nodiscard]] Optional<Type> Find(const ValueTypeIdentifier identifier) const
			{
				if (Optional<const EntryValue*> value = Find(identifier))
				{
					if (Optional<const Type*> pValue = value->Get<Type>())
					{
						return *pValue;
					}
				}
				return Invalid;
			}

			template<typename Type>
			void Emplace(const ValueTypeIdentifier identifier, Type&& value)
			{
				m_values[identifier] = UniquePtr<Value>::Make(Forward<Type>(value));
				m_valueTypeMask.Set((uint8)identifier);

				if constexpr (TypeTraits::IsSame<TypeTraits::WithoutConst<TypeTraits::WithoutReference<Type>>, DataSource::PropertyIdentifier>)
				{
					m_dynamicValueTypeMask.Set((uint8)identifier);
				}
			}

			void Emplace(Style::Value&& value)
			{
				const ValueTypeIdentifier typeIdentifier = value.GetTypeIdentifier();
				if (value.Get().Is<DataSource::PropertyIdentifier>())
				{
					m_dynamicValueTypeMask.Set((uint8)typeIdentifier);
				}

				m_values[typeIdentifier] = UniquePtr<Value>::Make(Move(value.Get()));
				m_valueTypeMask.Set((uint8)typeIdentifier);
			}

			bool Clear(const ValueTypeIdentifier identifier)
			{
				if (m_valueTypeMask.IsSet((uint8)identifier))
				{
					m_values[identifier] = {};
					m_valueTypeMask.Clear((uint8)identifier);
					m_dynamicValueTypeMask.Clear((uint8)identifier);
					return true;
				}
				return false;
			}

			void OnValueTypeAdded(const ValueTypeIdentifier identifier)
			{
				m_valueTypeMask.Set((uint8)identifier);
			}
			void OnDynamicValueTypeAdded(const ValueTypeIdentifier identifier)
			{
				m_dynamicValueTypeMask.Set((uint8)identifier);
				m_valueTypeMask.Set((uint8)identifier);
			}
			void OnValueTypeRemoved(const ValueTypeIdentifier identifier)
			{
				m_valueTypeMask.Clear((uint8)identifier);
			}
			void OnDynamicValueTypeRemoved(const ValueTypeIdentifier identifier)
			{
				m_dynamicValueTypeMask.Clear((uint8)identifier);
			}

			[[nodiscard]] EnumFlags<Modifier> GetRequiredModifiers() const
			{
				return m_requiredModifiers;
			}
			[[nodiscard]] ValueTypeMask GetValueTypeMask() const
			{
				return m_valueTypeMask;
			}
			[[nodiscard]] ValueTypeMask GetDynamicValueTypeMask() const
			{
				return m_dynamicValueTypeMask;
			}
			[[nodiscard]] ValueContainer::View GetValues()
			{
				return m_values;
			}
			[[nodiscard]] ValueContainer::ConstView GetValues() const
			{
				return m_values;
			}
		protected:
			EnumFlags<Modifier> m_requiredModifiers;
			// TODO: Can do a sparse / dense setup with another vector to save memory
			ValueContainer m_values;
			ValueTypeMask m_valueTypeMask;
			ValueTypeMask m_dynamicValueTypeMask;
		};

		//! Get a value with a set of modifiers, to for example get the hover effect for a style entry
		//! Multiple modifiers can be specified at the same time, and the best match will be returned
		// i.e. Get(Modifier::Hover)
		// or Get(Modifier::Hover | Modifier::Disabled)
		[[nodiscard]] MatchingModifiers GetMatchingModifiers(const EnumFlags<Modifier> requestedModifiers) const LIFETIME_BOUND
		{
			const RestrictedArrayView<const ModifierValues, ModifierIndexType> modifierValues = m_modifierValues.GetView();
			// Start by checking which modifiers fulfill the minimum requirement
			constexpr ModifierIndexType MaximumModifierCount = 16;
			Bitset<MaximumModifierCount> validModifiers;
			for (const ModifierValues& __restrict modifierValue : modifierValues)
			{
				if (requestedModifiers.AreAllSet(modifierValue.GetRequiredModifiers()))
				{
					validModifiers.Set(modifierValues.GetIteratorIndex(&modifierValue));
				}
			}

			const uint8 matchCount = validModifiers.GetNumberOfSetBits();
			if (matchCount > 0)
			{
				MatchingModifiers matchingModifiers;
				matchingModifiers.Reserve(matchCount);

				for (const uint8 matchIndex : validModifiers.GetSetBitsIterator())
				{
					const ModifierValues& __restrict modifierValue = modifierValues[matchIndex];
					matchingModifiers.EmplaceBack(
						ModifierMatch{matchIndex, (ModifierIndexType)modifierValue.GetRequiredModifiers().GetNumberOfSetFlags()}
					);
				}

				Algorithms::Sort(
					(ModifierMatch*)matchingModifiers.begin(),
					(ModifierMatch*)matchingModifiers.end(),
					[](const ModifierMatch& __restrict left, const ModifierMatch& __restrict right)
					{
						return left > right;
					}
				);

				return matchingModifiers;
			}

			return {};
		}

		//! Gets a mask containing all the masks used in this entry
		[[nodiscard]] EnumFlags<Modifier> GetModifierMask() const
		{
			return m_modifierMask;
		}

		[[nodiscard]] bool HasElements() const
		{
			return m_modifierValues.HasElements();
		}

		[[nodiscard]] Optional<ModifierValues*> FindExactModifierMatch(const EnumFlags<Modifier> requiredModifiers) LIFETIME_BOUND
		{
			OptionalIterator<ModifierValues> pMatch = m_modifierValues.FindIf(
				[requiredModifiers](const ModifierValues& modifierValue)
				{
					return requiredModifiers == modifierValue.GetRequiredModifiers();
				}
			);
			if (pMatch.IsValid())
			{
				return *pMatch;
			}
			return Invalid;
		}

		[[nodiscard]] Optional<ModifierValues*> FindExactModifierMatch(const EnumFlags<Modifier> requiredModifiers) const LIFETIME_BOUND
		{
			return const_cast<Entry&>(*this).FindExactModifierMatch(requiredModifiers);
		}

		[[nodiscard]] ModifierValues& EmplaceExactModifierMatch(const EnumFlags<Modifier> requiredModifiers) LIFETIME_BOUND
		{
			Assert(!m_modifierValues.ContainsIf(
				[requiredModifiers](const ModifierValues& modifierValue)
				{
					return requiredModifiers == modifierValue.GetRequiredModifiers();
				}
			))
			m_modifierMask |= requiredModifiers;
			return m_modifierValues.EmplaceBack(requiredModifiers);
		}

		[[nodiscard]] ModifierValues& FindOrEmplaceExactModifierMatch(const EnumFlags<Modifier> requiredModifiers) LIFETIME_BOUND
		{
			OptionalIterator<ModifierValues> pMatch = m_modifierValues.FindIf(
				[requiredModifiers](const ModifierValues& modifierValue)
				{
					return requiredModifiers == modifierValue.GetRequiredModifiers();
				}
			);
			if (pMatch.IsValid())
			{
				return *pMatch;
			}
			m_modifierMask |= requiredModifiers;
			return m_modifierValues.EmplaceBack(requiredModifiers);
		}

		void OnValueTypeAdded(const ValueTypeIdentifier identifier)
		{
			m_valueTypeMask.Set((uint8)identifier);
		}
		void OnValueTypeRemoved(const ValueTypeIdentifier identifier)
		{
			m_valueTypeMask.Clear((uint8)identifier);
		}
		void OnValueTypesAdded(const ValueTypeMask& mask)
		{
			m_valueTypeMask |= mask;
		}
		void OnDynamicValueTypeAdded(const ValueTypeIdentifier identifier)
		{
			m_valueTypeMask.Set((uint8)identifier);
			m_dynamicValueTypeMask.Set((uint8)identifier);
		}
		void OnDynamicValueTypesAdded(const ValueTypeMask& mask)
		{
			m_valueTypeMask |= mask;
			m_dynamicValueTypeMask |= mask;
		}
		void OnDynamicValueTypeRemoved(const ValueTypeIdentifier identifier)
		{
			m_dynamicValueTypeMask.Clear((uint8)identifier);
		}
		[[nodiscard]] const ValueTypeMask& GetValueTypeMask() const
		{
			return m_valueTypeMask;
		}
		[[nodiscard]] const ValueTypeMask& GetDynamicValueTypeMask() const
		{
			return m_dynamicValueTypeMask;
		}
	private:
		friend ComputedStylesheet;
		Vector<ModifierValues, ModifierIndexType> m_modifierValues;
		ValueTypeMask m_valueTypeMask;
		ValueTypeMask m_dynamicValueTypeMask;
		EnumFlags<Modifier> m_modifierMask;
	};
}
