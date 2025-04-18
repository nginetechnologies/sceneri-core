#include "Entity/Data/Tags.h"

#include "Entity/HierarchyComponentBase.h"
#include "Engine/Entity/ComponentType.h"
#include "Engine/Entity/Data/OctreeNode.h"
#include "Engine/Entity/Data/QuadtreeNode.h"
#include "Engine/Scene/SceneOctreeNode.h"
#include "Engine/Scene/SceneQuadtreeNode.h"
#include <Engine/Tag/TagRegistry.h>

#include <Common/Reflection/Registry.inl>
#include <Common/Asset/TagAssetType.h>

namespace ngine::Entity::Data
{
	Tags::Tags(Initializer&& initializer)
		: m_mask(initializer.m_mask)
	{
		UpdateTreeNodeMask(initializer.GetParent().GetIdentifier(), initializer.GetSceneRegistry());
	}

	Tags::Tags(const Tags& templateComponent, const Cloner& cloner)
		: m_mask(templateComponent.m_mask)
	{
		UpdateTreeNodeMask(cloner.GetParent().GetIdentifier(), cloner.GetSceneRegistry());
	}

	Tags::Tags(const Deserializer& deserializer)
	{
		UpdateTreeNodeMask(deserializer.GetParent().GetIdentifier(), deserializer.GetSceneRegistry());
	}

	void Tags::UpdateTreeNodeMask(const Entity::ComponentIdentifier componentIdentifier, Entity::SceneRegistry& sceneRegistry)
	{
		if (const Optional<Data::OctreeNode*> pOctreeNode = sceneRegistry.GetCachedSceneData<Data::OctreeNode>().GetComponentImplementation(componentIdentifier))
		{
			pOctreeNode->Get().UpdateMask(m_mask);
		}
		else if (const Optional<Data::QuadtreeNode*> pQuadtreeNode = sceneRegistry.GetCachedSceneData<Data::QuadtreeNode>().GetComponentImplementation(componentIdentifier))
		{
			pQuadtreeNode->Get().UpdateMask(m_mask);
		}
	}

	void Tags::DeserializeCustomData(const Optional<Serialization::Reader> serializer, Entity::HierarchyComponentBase& parent)
	{
		if (serializer.IsValid())
		{
			if (serializer->Serialize("mask", m_mask, System::Get<Tag::Registry>()))
			{
				UpdateTreeNodeMask(parent.GetIdentifier(), parent.GetSceneRegistry());
			}
		}
	}

	bool Tags::SerializeCustomData(Serialization::Writer serializer, const Entity::HierarchyComponentBase&) const
	{
		Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

		Tag::Mask mask = m_mask;
		if (serializer.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::ToDisk))
		{
			for (Tag::Identifier::IndexType tagIdentifierIndex : mask.GetSetBitsIterator())
			{
				const Tag::Identifier tagIdentifier = Tag::Identifier::MakeFromValidIndex(tagIdentifierIndex);
				const EnumFlags<Tag::Flags> tagFlags = tagRegistry.GetAssetData(tagIdentifier).m_flags;
				if (tagFlags.IsSet(Tag::Flags::Transient))
				{
					mask.Clear(tagIdentifier);
				}
			}
		}

		return serializer.Serialize("mask", mask, tagRegistry);
	}

	bool Tags::ShouldSerialize(Serialization::Writer writer) const
	{
		Tag::Mask mask = m_mask;
		if (writer.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::ToDisk))
		{
			Tag::Registry& tagRegistry = System::Get<Tag::Registry>();

			for (Tag::Identifier::IndexType tagIdentifierIndex : mask.GetSetBitsIterator())
			{
				const Tag::Identifier tagIdentifier = Tag::Identifier::MakeFromValidIndex(tagIdentifierIndex);
				const EnumFlags<Tag::Flags> tagFlags = tagRegistry.GetAssetData(tagIdentifier).m_flags;
				if (tagFlags.IsSet(Tag::Flags::Transient))
				{
					mask.Clear(tagIdentifier);
				}
			}
		}

		return mask.AreAnySet();
	}

	static_assert(Reflection::HasShouldSerialize<Tags>);
	void Tags::SetTagMask(Entity::HierarchyComponentBase& owner, const Tag::ModifiableMaskProperty mask)
	{
		m_mask = mask.m_mask;
		UpdateTreeNodeMask(owner.GetIdentifier(), owner.GetSceneRegistry());
	}

	Tag::ModifiableMaskProperty Tags::GetTagMask() const
	{
		return Tag::ModifiableMaskProperty{m_mask, System::Get<Tag::Registry>()};
	}

	[[maybe_unused]] const bool wasTagsComponentRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<Tags>>::Make());
	[[maybe_unused]] const bool wasTagsComponentTypeRegistered = Reflection::Registry::RegisterType<Tags>();
}
