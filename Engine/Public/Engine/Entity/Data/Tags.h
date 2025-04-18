#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>
#include <Engine/Tag/TagMaskProperty.h>
#include <Engine/Tag/TagIdentifier.h>

#include <Common/Serialization/ForwardDeclarations/Reader.h>
#include <Common/Serialization/ForwardDeclarations/Writer.h>

#include <Common/Asset/Picker.h>

namespace ngine::Entity
{
	struct HierarchyComponentBase;
}

namespace ngine::Entity::Data
{
	struct Tags : public HierarchyComponent
	{
		using InstanceIdentifier = TIdentifier<uint32, 16>;
		using BaseType = HierarchyComponent;

		struct Initializer : public BaseType::DynamicInitializer
		{
			using BaseType = HierarchyComponent::DynamicInitializer;
			using BaseType::BaseType;
			Initializer(BaseType&& initializer, const Tag::Mask mask = Tag::Mask())
				: BaseType(Forward<BaseType>(initializer))
				, m_mask(mask)
			{
			}

			Tag::Mask m_mask;
		};

		Tags(Initializer&& initializer);
		Tags(const Tags& templateComponent, const Cloner& cloner);
		Tags(const Deserializer& deserializer);

		void DeserializeCustomData(const Optional<Serialization::Reader>, Entity::HierarchyComponentBase& parent);
		bool SerializeCustomData(Serialization::Writer, const Entity::HierarchyComponentBase& parent) const;

		[[nodiscard]] bool ShouldSerialize(Serialization::Writer) const;

		void SetTags(const Entity::ComponentIdentifier componentIdentifier, Entity::SceneRegistry& sceneRegistry, const Tag::Mask mask)
		{
			m_mask |= mask;
			UpdateTreeNodeMask(componentIdentifier, sceneRegistry);
		}

		void
		SetTag(const Entity::ComponentIdentifier componentIdentifier, Entity::SceneRegistry& sceneRegistry, const Tag::Identifier tagIdentifier)
		{
			m_mask.Set(tagIdentifier);
			UpdateTreeNodeMask(componentIdentifier, sceneRegistry);
		}

		void ClearTag(
			const Entity::ComponentIdentifier componentIdentifier, Entity::SceneRegistry& sceneRegistry, const Tag::Identifier tagIdentifier
		)
		{
			m_mask.Clear(tagIdentifier);
			UpdateTreeNodeMask(componentIdentifier, sceneRegistry);
		}

		void ToggleTag(
			const Entity::ComponentIdentifier componentIdentifier, Entity::SceneRegistry& sceneRegistry, const Tag::Identifier tagIdentifier
		)
		{
			m_mask.Toggle(tagIdentifier);
			UpdateTreeNodeMask(componentIdentifier, sceneRegistry);
		}

		[[nodiscard]] Tag::Mask GetMask() const
		{
			return m_mask;
		}

		[[nodiscard]] inline bool HasTag(const Tag::Identifier tagIdentifier) const
		{
			return m_mask.IsSet(tagIdentifier);
		}
	private:
		friend struct Reflection::ReflectedType<Tags>;

		void UpdateTreeNodeMask(const Entity::ComponentIdentifier componentIdentifier, Entity::SceneRegistry& sceneRegistry);

		void SetTagMask(Entity::HierarchyComponentBase& owner, Tag::ModifiableMaskProperty mask);
		[[nodiscard]] Tag::ModifiableMaskProperty GetTagMask() const;

		Tag::Mask m_mask;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::Tags>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::Tags>(
			"{fa5e5929-fdb4-4579-8c03-f3f837cf4905}"_guid,
			MAKE_UNICODE_LITERAL("Tags"),
			TypeFlags::DisableDeletionFromUserInterface,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Tags"),
				"mask",
				"{A85A480F-5F7E-4A7F-B94F-CA4E116591B9}"_guid,
				MAKE_UNICODE_LITERAL("Tags"),
				&Entity::Data::Tags::SetTagMask,
				&Entity::Data::Tags::GetTagMask,
				Reflection::Internal::DummyFunction(),
				Reflection::PropertyFlags::NotReadFromDisk | Reflection::PropertyFlags::NotSavedToDisk
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "0ba82b10-38de-42c6-8974-7f0839fbe5fd"_asset, "5bfbc860-9009-471e-8cd5-2c7a6815a5bf"_asset
			}}
		);
	};
}
