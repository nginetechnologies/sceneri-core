#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>
#include <Renderer/Assets/Material/MaterialInstanceIdentifier.h>

#include <Common/Asset/Picker.h>

namespace ngine::Entity
{
	struct Component3D;
	struct StaticMeshComponent;
}

namespace ngine::Entity::Data::RenderItem
{
	struct MaterialInstanceIdentifier final : public HierarchyComponent
	{
		using BaseType = HierarchyComponent;
		using InstanceIdentifier = TIdentifier<uint32, 14>;

		MaterialInstanceIdentifier(const Rendering::MaterialInstanceIdentifier identifier)
			: m_identifier(identifier)
		{
		}

		void Set(HierarchyComponentBase& owner, SceneRegistry& sceneRegistry, const Rendering::MaterialInstanceIdentifier identifier);
		void Clone(HierarchyComponentBase& owner, SceneRegistry& sceneRegistry);

		[[nodiscard]] operator Rendering::MaterialInstanceIdentifier() const
		{
			return m_identifier;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::RenderItem::MaterialInstanceIdentifier>;
		friend Entity::Component3D;
		friend Entity::StaticMeshComponent;

		Asset::Picker GetFromProperty(Entity::HierarchyComponentBase&) const;
		void SetFromProperty(Entity::HierarchyComponentBase&, const Asset::Picker asset);
	protected:
		Rendering::MaterialInstanceIdentifier m_identifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::RenderItem::MaterialInstanceIdentifier>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::RenderItem::MaterialInstanceIdentifier>(
			"6459bde8-78f6-48ac-858c-d41e02da0eec"_guid,
			MAKE_UNICODE_LITERAL("Material Instance Identifier"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Material"),
				"material_instance",
				"a9b8b1fd-0b66-4516-8816-386660675199"_guid,
				MAKE_UNICODE_LITERAL("Mesh"),
				PropertyFlags::Transient,
				&Entity::Data::RenderItem::MaterialInstanceIdentifier::SetFromProperty,
				&Entity::Data::RenderItem::MaterialInstanceIdentifier::GetFromProperty
			)}
		);
	};
}
