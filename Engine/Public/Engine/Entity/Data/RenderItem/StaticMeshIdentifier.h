#pragma once

#include <Engine/Entity/Data/HierarchyComponent.h>
#include <Renderer/Assets/StaticMesh/StaticMeshIdentifier.h>

#include <Common/Asset/Picker.h>

namespace ngine::Entity
{
	struct Component3D;
	struct StaticMeshComponent;
}

namespace ngine::Entity::Data::RenderItem
{
	struct StaticMeshIdentifier final : public HierarchyComponent
	{
		using BaseType = HierarchyComponent;
		using InstanceIdentifier = TIdentifier<uint32, 14>;

		StaticMeshIdentifier(HierarchyComponentBase& owner, SceneRegistry& sceneRegistry, const Rendering::StaticMeshIdentifier identifier);

		void OnDestroying();

		void Set(HierarchyComponentBase& owner, SceneRegistry& sceneRegistry, const Rendering::StaticMeshIdentifier identifier);
		void Clone(HierarchyComponentBase& owner, SceneRegistry& sceneRegistry, const bool allowCpuVertexAccess = false);

		[[nodiscard]] operator Rendering::StaticMeshIdentifier() const
		{
			return m_identifier;
		}
	protected:
		friend struct Reflection::ReflectedType<Entity::Data::RenderItem::StaticMeshIdentifier>;
		friend Entity::Component3D;
		friend Entity::StaticMeshComponent;

		Asset::Picker GetFromProperty(Entity::HierarchyComponentBase&) const;
		void SetFromProperty(Entity::HierarchyComponentBase&, const Asset::Picker asset);

		void OnMeshBoundingBoxChanged(HierarchyComponentBase& owner, SceneRegistry& sceneRegistry);
	protected:
		Rendering::StaticMeshIdentifier m_identifier;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::Data::RenderItem::StaticMeshIdentifier>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::Data::RenderItem::StaticMeshIdentifier>(
			"9a97a10f-c841-4b52-9e99-154cf70af9ab"_guid,
			MAKE_UNICODE_LITERAL("Static Mesh Identifier"),
			TypeFlags::DisableDynamicInstantiation | TypeFlags::DisableDynamicCloning | TypeFlags::DisableDynamicDeserialization |
				TypeFlags::DisableWriteToDisk | TypeFlags::DisableUserInterfaceInstantiation | TypeFlags::DisableDeletionFromUserInterface,
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Mesh Part"),
				"mesh",
				"05213930-1600-4ca9-b57c-6e2de7c019cf"_guid,
				MAKE_UNICODE_LITERAL("Mesh Part"),
				PropertyFlags::Transient,
				&Entity::Data::RenderItem::StaticMeshIdentifier::SetFromProperty,
				&Entity::Data::RenderItem::StaticMeshIdentifier::GetFromProperty
			)}
		);
	};
}
