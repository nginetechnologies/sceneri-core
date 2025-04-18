#pragma once

#include "ColliderComponent.h"
#include <Common/Asset/Picker.h>

#include <Engine/Entity/Indicator/IndicatorTypeExtension.h>

namespace ngine::Rendering
{
	struct StaticMesh;
}

namespace ngine::Physics
{
	struct BodyComponent;
	struct Material;
	struct TriangleMeshLoadJob;

	struct MeshColliderComponent final : public ColliderComponent
	{
		static constexpr Guid TypeGuid = "508647CA-01F3-4930-9E33-4562692B719F"_guid;
		using BaseType = ColliderComponent;
		using InstanceIdentifier = TIdentifier<uint32, 13>;

		struct Initializer : public BaseType::Initializer
		{
			using BaseType = ColliderComponent::Initializer;
			Initializer(BaseType&& initializer, Optional<const Rendering::StaticMesh*> pMesh = {})
				: BaseType(Forward<BaseType>(initializer))
				, m_pMesh(pMesh)
			{
			}
			using BaseType::BaseType;

			Optional<const Rendering::StaticMesh*> m_pMesh;
		};

		MeshColliderComponent(Initializer&& initializer);
		MeshColliderComponent(const MeshColliderComponent& templateComponent, const Cloner& cloner);
		MeshColliderComponent(const Deserializer& deserializer);

		[[nodiscard]] virtual bool CanApplyAtPoint(
			const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags
		) const override;
		virtual bool
		ApplyAtPoint(const Entity::ApplicableData&, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags) override;
		virtual void
		IterateAttachedItems([[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>&)
			override;

		[[nodiscard]] const Rendering::StaticMesh& GetMesh() const
		{
			return m_mesh;
		}
	protected:
		MeshColliderComponent(const Deserializer& deserializer, const Rendering::StaticMesh& mesh);
		MeshColliderComponent(Initializer&& initializer, const Rendering::StaticMesh& mesh);

		Threading::JobBatch TryLoadOrCreateShape(Data::Body* pBody);
		virtual void OnAttachedToNewParent() override;

		void CreateStaticMeshShape(Data::Body* pBody);
		void SetStaticMesh(const Asset::Picker asset);
		Threading::JobBatch
		SetDeserializedStaticMesh(const Asset::Picker asset, const Serialization::Reader objectReader, const Serialization::Reader typeReader);
		Asset::Picker GetStaticMesh() const;
	protected:
		friend Data::Body;
		friend TriangleMeshLoadJob;
		friend struct Reflection::ReflectedType<Physics::MeshColliderComponent>;

		ReferenceWrapper<const Rendering::StaticMesh> m_mesh;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Physics::MeshColliderComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Physics::MeshColliderComponent>(
			Physics::MeshColliderComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Mesh Collider"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Mesh"),
				"mesh",
				"{5DBFD2F2-3A5F-4177-9651-126D022F6C53}"_guid,
				MAKE_UNICODE_LITERAL("Mesh Collider"),
				&Physics::MeshColliderComponent::SetStaticMesh,
				&Physics::MeshColliderComponent::GetStaticMesh,
				&Physics::MeshColliderComponent::SetDeserializedStaticMesh
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{
				Entity::ComponentTypeExtension{
					Entity::ComponentTypeFlags(), "3418a661-1034-40c4-9e40-04221b1177df"_asset, "5edc8044-ff05-4c39-b59a-29021095f002"_guid
				} /*,
		     Entity::IndicatorTypeExtension{
		       "70e3da40-2746-457f-8cbf-d74fb699448c"_guid,
		     }*/
			}
		);
	};
}
