#pragma once

#include <Engine/Tag/TagMask.h>
#include <Common/Storage/AtomicIdentifierMask.h>
#include <Common/Memory/Containers/Trees/QuadTree.h>
#include <Common/Memory/UniquePtr.h>

namespace ngine
{
	struct SceneQuadtreeNode : public ngine::QuadTreeNode<ReferenceWrapper<Entity::Component2D>>
	{
		using BaseType = ngine::QuadTreeNode<ReferenceWrapper<Entity::Component2D>>;
		using BaseType::BaseType;
		using BaseType::operator=;

		void UpdateMask(const Tag::Mask mask)
		{
			Tag::Mask previousMask;
			if (m_pTagMask != nullptr)
			{
				previousMask = *m_pTagMask |= mask;
			}
			else
			{
				m_pTagMask.CreateInPlace(mask);
			}

			OnMaskChanged(mask);
		}

		void EmplaceElement(Entity::Component2D& component, const float depth, const Math::Rectanglef contentArea, const Tag::Mask mask)
		{
			BaseType::EmplaceElement(component, depth, contentArea);
			UpdateMask(mask);
		}

		void OnMaskChanged(Tag::Mask newlySetBits)
		{
			if (newlySetBits.AreAnySet() && m_pParentNode != nullptr)
			{
				SceneQuadtreeNode& parentNode = static_cast<SceneQuadtreeNode&>(*m_pParentNode);
				if (parentNode.m_pTagMask == nullptr)
				{
					parentNode.m_pTagMask.CreateInPlace();
				}

				const Tag::Mask previousParentTagMask = parentNode.m_pTagMask->operator|=(newlySetBits);
				newlySetBits = (previousParentTagMask ^ newlySetBits) & newlySetBits;
				if (newlySetBits.AreAnySet())
				{
					parentNode.OnMaskChanged(newlySetBits);
				}
			}
		}

		[[nodiscard]] bool ContainsAnyTags(const Tag::Mask mask) const
		{
			return m_pTagMask.IsValid() && m_pTagMask->AreAnySet(mask);
		}
		[[nodiscard]] bool ContainsTag(const Tag::Identifier identifier) const
		{
			return m_pTagMask.IsValid() && m_pTagMask->IsSet(identifier);
		}
	protected:
		UniquePtr<Threading::AtomicIdentifierMask<Tag::Identifier>> m_pTagMask;
	};
	using SceneQuadtree = ngine::QuadTree<SceneQuadtreeNode>;
}
