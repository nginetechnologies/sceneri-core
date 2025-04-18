#pragma once

#include <Common/Storage/IdentifierArray.h>
#include <Common/Storage/IdentifierMask.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include "TagIdentifier.h"

namespace ngine::Tag
{
	struct Registry;

	template<typename ElementIdentifierType>
	struct AtomicMaskContainer
	{
		using ElementMask = IdentifierMask<ElementIdentifierType>;
		using AtomicElementMask = Threading::AtomicIdentifierMask<ElementIdentifierType>;

		using IdentifierArray = TIdentifierArray<Threading::Atomic<AtomicElementMask*>, Identifier>;
		using ConstView = typename IdentifierArray::ConstView;
		using View = typename IdentifierArray::View;
		using ConstDynamicView = typename IdentifierArray::ConstDynamicView;
		using DynamicView = typename IdentifierArray::DynamicView;
		using ConstRestrictedView = typename IdentifierArray::ConstRestrictedView;

		void Destroy(const Registry& registry);

		[[nodiscard]] ConstRestrictedView GetView() const
		{
			return m_tags.GetView();
		}
		void Set(const Identifier tag, const ElementMask& mask)
		{
			Threading::Atomic<AtomicElementMask*>& tagMask = m_tags[tag];
			AtomicElementMask* pMask = tagMask;
			if (pMask != nullptr)
			{
				*pMask |= mask;
			}
			else
			{
				AtomicElementMask* pNewMask = new AtomicElementMask(mask);
				bool wasApplied = false;
				while (pMask == nullptr && !wasApplied)
				{
					wasApplied = tagMask.CompareExchangeWeak(pMask, pNewMask);
				}

				if (!wasApplied)
				{
					*pMask |= mask;
					delete pNewMask;
				}
			}
		}
		void Set(const Identifier tag, const ElementIdentifierType elementIdentifier)
		{
			Threading::Atomic<AtomicElementMask*>& tagMask = m_tags[tag];
			AtomicElementMask* pMask = tagMask;
			if (pMask != nullptr)
			{
				pMask->Set(elementIdentifier);
			}
			else
			{
				AtomicElementMask* pNewMask = new AtomicElementMask();
				pNewMask->Set(elementIdentifier);
				bool wasApplied = false;
				while (pMask == nullptr && !wasApplied)
				{
					wasApplied = tagMask.CompareExchangeWeak(pMask, pNewMask);
				}

				if (!wasApplied)
				{
					pMask->Set(elementIdentifier);
					delete pNewMask;
				}
			}
		}
		void Clear(const Tag::Identifier tag, const ElementMask& mask)
		{
			Threading::Atomic<AtomicElementMask*>& tagMask = m_tags[tag];
			AtomicElementMask* pMask = tagMask;
			if (pMask != nullptr)
			{
				pMask->Clear(mask);
			}
		}
		[[nodiscard]] bool IsSet(const Tag::Identifier tag, const ElementIdentifierType elementIdentifier) const
		{
			const Threading::Atomic<AtomicElementMask*>& tagMask = m_tags[tag];
			const AtomicElementMask* pMask = tagMask;
			if (pMask != nullptr)
			{
				return pMask->IsSet(elementIdentifier);
			}
			return false;
		}
		[[nodiscard]] bool AreAnySet(const Tag::Identifier tag) const
		{
			const Threading::Atomic<AtomicElementMask*>& tagMask = m_tags[tag];
			const AtomicElementMask* pMask = tagMask;
			if (pMask != nullptr)
			{
				return pMask->AreAnySet();
			}
			return false;
		}

		[[nodiscard]] View GetView()
		{
			return m_tags.GetView();
		}

		//! Copies the tags from the source container into the specified element
		void CopyTags(
			const DynamicView sourceTags, const ElementIdentifierType sourceElementIdentifier, const ElementIdentifierType targetElementIdentifier
		)
		{
			for (const Threading::Atomic<AtomicElementMask*>& tagMaskPointer : sourceTags)
			{
				AtomicElementMask* pTagMask = tagMaskPointer;
				if (pTagMask != nullptr)
				{
					if (pTagMask->IsSet(sourceElementIdentifier))
					{
						const Identifier tagIdentifier = Identifier::MakeFromValidIndex(sourceTags.GetIteratorIndex(&tagMaskPointer));
						Set(tagIdentifier, targetElementIdentifier);
					}
				}
			}
		}
	protected:
		IdentifierArray m_tags;
	};
}
