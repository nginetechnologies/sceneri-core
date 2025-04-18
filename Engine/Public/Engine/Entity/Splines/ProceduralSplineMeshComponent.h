#pragma once

#include <Engine/Entity/Component3D.h>

#include <Renderer/Assets/Defaults.h>

#include <Common/Function/Function.h>
#include <Common/Asset/Picker.h>
#include <Renderer/Assets/StaticMesh/StaticMeshIdentifier.h>
#include <Renderer/Assets/Material/MaterialInstanceIdentifier.h>
#include <Renderer/Stages/RenderItemStageMask.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>
#include <Engine/Asset/AssetDatabase.h>
#include <Engine/Scene/Scene3DAssetType.h>

namespace ngine::Threading
{
	struct Job;
}

namespace ngine::Entity
{
	struct SplineComponent;

	struct ProceduralSplineMeshComponent : public Component3D
	{
		static constexpr Guid TypeGuid = "161cdf21-40f1-49e1-ad97-a8c1e77e1058"_guid;

		using BaseType = Component3D;
		using InstanceIdentifier = TIdentifier<uint32, 9>;

		static constexpr Asset::Guid DefaultAsset = "2174eee2-fc41-69a4-7dfd-65eb9b767291"_guid;
		inline static constexpr Array<Asset::TypeGuid, 3> SupportedAssetTypes = {
			MeshPartAssetType::AssetFormat.assetTypeGuid,
			MeshSceneAssetType::AssetFormat.assetTypeGuid,
			Scene3DAssetType::AssetFormat.assetTypeGuid
		};

		struct Initializer : public Component3D::Initializer
		{
			using BaseType = Component3D::Initializer;
			Initializer(BaseType&& initializer, const Asset::Guid assetGuid = DefaultAsset)
				: BaseType(Forward<BaseType>(initializer))
				, m_assetGuid(assetGuid)
			{
			}

			Asset::Guid m_assetGuid = Rendering::Constants::DefaultMeshSceneAssetGuid;
		};

		ProceduralSplineMeshComponent(const ProceduralSplineMeshComponent& templateComponent, const Cloner& cloner);
		ProceduralSplineMeshComponent(const Deserializer& deserializer);
		ProceduralSplineMeshComponent(Initializer&& initializer);
		virtual ~ProceduralSplineMeshComponent();
		[[nodiscard]] virtual bool CanApplyAtPoint(
			const Entity::ApplicableData& applicableData, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags
		) const override;
		virtual bool
		ApplyAtPoint(const Entity::ApplicableData&, const Math::WorldCoordinate, const EnumFlags<Entity::ApplyAssetFlags> applyFlags) override;
		virtual void
		IterateAttachedItems([[maybe_unused]] const ArrayView<const Reflection::TypeDefinition> allowedTypes, const Function<Memory::CallbackResult(ConstAnyView), 36>&)
			override;

		void OnCreated();

		[[nodiscard]] Optional<SplineComponent*> GetSpline() const;

		void SetAsset(Asset::Guid asset)
		{
			m_assetGuid = asset;
		}

		[[nodiscard]] Asset::Guid GetAsset() const
		{
			return m_assetGuid;
		}

		void CreateMesh();
		void RecreateMesh();
	protected:
		struct MeshInfo
		{
			Rendering::StaticMeshIdentifier meshIdentifier;
			Rendering::MaterialInstanceIdentifier materialInstanceIdentifier;
			Rendering::RenderItemStageMask stageMask;
		};

		static void GetStaticMeshesRecursive(const Entity::Component3D& component, Vector<MeshInfo>& meshIdentifiers);
		static float FindStartYPosition(const Vector<MeshInfo>& meshes);
		static float FindEndYPosition(const Vector<MeshInfo>& meshes);

		using OnLoadedCallback = Function<void(), 24>;
		void LoadAsset(OnLoadedCallback&& callback);
		void LoadSceneAsset(Asset::Guid guid, OnLoadedCallback&& callback);
		bool AreAssetsLoaded() const;
	protected:
		friend struct Reflection::ReflectedType<Entity::ProceduralSplineMeshComponent>;
		ProceduralSplineMeshComponent(const Deserializer& deserializer, Optional<Serialization::Reader> componentSerializer);

		using ComponentTypePicker = Asset::Picker;
		void SetComponentType(const ComponentTypePicker asset);
		ComponentTypePicker GetComponentType() const;

		Vector<MeshInfo> m_meshIdentifiers;
		Asset::Guid m_assetGuid = DefaultAsset;
	};
}

namespace ngine::Reflection
{
	template<>
	struct ReflectedType<Entity::ProceduralSplineMeshComponent>
	{
		inline static constexpr auto Type = Reflection::Reflect<Entity::ProceduralSplineMeshComponent>(
			Entity::ProceduralSplineMeshComponent::TypeGuid,
			MAKE_UNICODE_LITERAL("Procedural Spline Mesh"),
			Reflection::TypeFlags{},
			Reflection::Tags{},
			Reflection::Properties{Reflection::MakeDynamicProperty(
				MAKE_UNICODE_LITERAL("Asset"),
				"asset",
				"{7F726335-567F-452D-8B8B-9C281DE0E8EF}"_guid,
				MAKE_UNICODE_LITERAL("Procedural Spline"),
				&Entity::ProceduralSplineMeshComponent::SetComponentType,
				&Entity::ProceduralSplineMeshComponent::GetComponentType
			)},
			Reflection::Functions{},
			Reflection::Events{},
			Reflection::Extensions{Entity::ComponentTypeExtension{
				Entity::ComponentTypeFlags(),
				"31b7f11e-05ce-d156-9513-eed1d3978f78"_asset,
				"5fc365eb-e4ae-4d1b-aaa3-4d4a66d5ab69"_guid,
				"A4C2ADC8-390B-4617-B6F1-64E6055509A0"_asset
			}}
		);
	};
}
