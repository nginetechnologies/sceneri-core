#pragma once

#include <Engine/Entity/RenderItemComponent.h>
#include <Engine/Entity/ComponentTypeExtension.h>

#include <Renderer/Assets/Material/MaterialInstanceIdentifier.h>
#include <Renderer/Assets/StaticMesh/StaticMeshIdentifier.h>

#include <Common/Math/Primitives/ForwardDeclarations/Sphere.h>
#include <Common/Math/Primitives/ForwardDeclarations/WorldLine.h>
#include <Common/Asset/Picker.h>

#include <Renderer/Index.h>

namespace ngine
{
	struct Scene3D;
	struct SceneOctreeNode;
	struct RootSceneOctreeNode;
};

namespace ngine::Rendering
{
	struct LogicalDevice;
	struct RenderViewThreadJob;
	struct Window;
	struct StaticMesh;
}

namespace ngine::Entity
{
	struct StaticMeshComponent : public RenderItemComponent
	{
		using InstanceIdentifier = TIdentifier<uint32, 14>;
		using BaseType = RenderItemComponent;

		struct Initializer : public RenderItemComponent::Initializer
		{
			using BaseType = RenderItemComponent::Initializer;
			Initializer(BaseType&& initializer);
			Initializer(
				BaseType&& initializer,
				const Rendering::StaticMeshIdentifier staticMeshIdentifier,
				const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier
			);
			Initializer(
				BaseType&& initializer,
				const Rendering::StaticMeshIdentifier staticMeshIdentifier,
				const Rendering::StaticMesh& staticMesh,
				const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier
			)
				: BaseType(Forward<BaseType>(initializer))
				, m_staticMeshIdentifier(staticMeshIdentifier)
				, m_staticMesh(staticMesh)
				, m_materialInstanceIdentifier(materialInstanceIdentifier)
			{
			}

			Rendering::StaticMeshIdentifier m_staticMeshIdentifier;
			ReferenceWrapper<const Rendering::StaticMesh> m_staticMesh;
			Rendering::MaterialInstanceIdentifier m_materialInstanceIdentifier;
		};

		StaticMeshComponent(Initializer&& initializer);
		StaticMeshComponent(const StaticMeshComponent& templateComponent, const Cloner& cloner);
		StaticMeshComponent(const Deserializer& deserializer);
		virtual ~StaticMeshComponent();

		[[nodiscard]] Rendering::StaticMeshIdentifier GetMeshIdentifier() const;
		[[nodiscard]] const Rendering::StaticMesh& GetMesh() const;
		void SetMesh(const Rendering::StaticMesh& mesh);
		void CloneMesh(const bool allowCpuVertexAccess = false);
		void CloneMaterialInstance();

		[[nodiscard]] Rendering::MaterialInstanceIdentifier GetMaterialInstanceIdentifier() const;
		void SetMaterialInstance(const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier);
		Asset::Picker GetStaticMesh() const;
	protected:
		friend struct Reflection::ReflectedType<Entity::StaticMeshComponent>;
		StaticMeshComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer);
		StaticMeshComponent(
			const Deserializer& deserializer,
			Rendering::StaticMeshIdentifier meshIdentifier,
			const Optional<Serialization::Reader> componentSerializer
		);
		StaticMeshComponent(
			const Deserializer& deserializer,
			Rendering::StaticMeshIdentifier meshIdentifier,
			const Optional<const Rendering::StaticMesh*> pMesh,
			const Optional<Serialization::Reader> componentSerializer
		);
	protected:
		void SetMaterialInstanceAsset(const Asset::Picker asset);
		Asset::Picker GetMaterialInstance() const;
		void SetStaticMesh(const Asset::Picker asset);
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::StaticMeshComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::StaticMeshComponent>(
			"de0b2bc1-2e72-4739-a749-f5281e0de55a"_guid,
			MAKE_UNICODE_LITERAL("Mesh"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Mesh Part"),
					"mesh",
					"{CE1DC73D-A2D6-45CA-AB49-36C204756EB0}"_guid,
					MAKE_UNICODE_LITERAL("Mesh Part"),
					PropertyFlags::HideFromUI,
					&Entity::StaticMeshComponent::SetStaticMesh,
					&Entity::StaticMeshComponent::GetStaticMesh
				),
				Reflection::MakeDynamicProperty(
					MAKE_UNICODE_LITERAL("Material"),
					"material_instance",
					"{AB2187FB-C132-425F-B60D-773AAB5388DF}"_guid,
					MAKE_UNICODE_LITERAL("Mesh"),
					PropertyFlags::HideFromUI,
					&Entity::StaticMeshComponent::SetMaterialInstanceAsset,
					&Entity::StaticMeshComponent::GetMaterialInstance
				)
			},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(), "abcf5a98-c556-41bb-b7ac-a5c6ea9dd85c"_asset, "9c70b0c0-52a5-4285-87c9-1aa3f96c44f5"_guid
			}}
		);
	};
}
