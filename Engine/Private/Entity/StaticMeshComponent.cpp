#include "Entity/StaticMeshComponent.h"

#include "Engine/Entity/ComponentTypeSceneData.h"
#include "Engine/Entity/ComponentType.h"
#include <Common/System/Query.h>
#include "Engine/Entity/ComponentSoftReference.inl"
#include "Engine/Entity/Serialization/ComponentReference.h"
#include <Engine/Entity/Data/RenderItem/StaticMeshIdentifier.h>
#include <Engine/Entity/Data/RenderItem/MaterialInstanceIdentifier.h>
#include <Engine/Entity/Data/RenderItem/StageMask.h>
#include <Engine/Entity/Data/RenderItem/VisibilityListener.h>
#include <Engine/Entity/Data/BoundingBox.h>
#include <Engine/Entity/Component3D.inl>

#include <Common/Memory/Move.h>
#include <Common/Memory/Forward.h>
#include <Common/Serialization/Reader.h>
#include <Common/Reflection/Registry.inl>

#include <Renderer/Renderer.h>
#include <Renderer/Assets/Defaults.h>
#include <Renderer/Assets/StaticMesh/StaticMesh.h>
#include <Renderer/Assets/StaticMesh/Primitives/Primitives.h>
#include <Renderer/Assets/StaticMesh/MeshAssetType.h>
#include <Renderer/Assets/Material/MaterialInstanceAssetType.h>
#include <Renderer/Assets/Material/RuntimeMaterialInstance.h>
#include <Renderer/Assets/Material/RuntimeMaterial.h>
#include <Renderer/Assets/Texture/TextureAssetType.h>
#include <Renderer/Stages/MaterialsStage.h>

namespace ngine::Entity
{
	Rendering::StaticMeshIdentifier ReadMesh(const Optional<Serialization::Reader> serializer)
	{
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		if (serializer.IsValid())
		{
			return meshCache.FindOrRegisterAsset(serializer->ReadWithDefaultValue("mesh", Asset::Guid(Rendering::Constants::DefaultMeshAssetGuid))
			);
		}
		else
		{
			return meshCache.FindOrRegisterAsset(Rendering::Constants::DefaultMeshAssetGuid);
		}
	}

	Rendering::MaterialInstanceIdentifier ReadMaterialInstance(const Optional<Serialization::Reader> serializer)
	{
		Rendering::MaterialInstanceCache& materialInstanceCache = System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache();
		if (serializer.IsValid())
		{
			return materialInstanceCache.FindOrRegisterAsset(
				serializer->ReadWithDefaultValue("material_instance", Asset::Guid(Rendering::Constants::DefaultMaterialInstanceAssetGuid))
			);
		}
		else
		{
			return materialInstanceCache.FindOrRegisterAsset(Rendering::Constants::DefaultMaterialInstanceAssetGuid);
		}
	}

	[[nodiscard]] Math::BoundingBox GetMeshBounds(const Optional<const Rendering::StaticMesh*> pMesh)
	{
		return pMesh.IsValid() && !pMesh->GetBoundingBox().IsZero() ? pMesh->GetBoundingBox() : Math::BoundingBox{0.5_meters};
	}

	StaticMeshComponent::Initializer::Initializer(BaseType&& initializer)
		: Initializer(
				Forward<BaseType>(initializer),
				System::Get<Rendering::Renderer>().GetMeshCache().FindOrRegisterAsset(Rendering::Constants::DefaultMeshAssetGuid),
				System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache().FindOrRegisterAsset(
					Rendering::Constants::DefaultMaterialInstanceAssetGuid
				)
			)
	{
	}

	StaticMeshComponent::Initializer::Initializer(
		BaseType&& initializer,
		const Rendering::StaticMeshIdentifier staticMeshIdentifier,
		const Rendering::MaterialInstanceIdentifier materialInstanceIdentifier
	)
		: Initializer(
				Forward<BaseType>(initializer),
				staticMeshIdentifier,
				*System::Get<Rendering::Renderer>().GetMeshCache().GetAssetData(staticMeshIdentifier).m_pMesh,
				materialInstanceIdentifier
			)
	{
	}

	StaticMeshComponent::StaticMeshComponent(Initializer&& initializer)
		: RenderItemComponent(initializer | GetMeshBounds(*initializer.m_staticMesh))
	{
		CreateDataComponent<Entity::Data::RenderItem::StaticMeshIdentifier>(
			initializer.GetSceneRegistry().GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>(),
			*this,
			initializer.GetSceneRegistry(),
			initializer.m_staticMeshIdentifier
		);
		CreateDataComponent<Entity::Data::RenderItem::MaterialInstanceIdentifier>(
			initializer.GetSceneRegistry().GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>(),
			initializer.m_materialInstanceIdentifier.IsValid()
				? initializer.m_materialInstanceIdentifier
				: System::Get<Rendering::Renderer>().GetMaterialCache().GetInstanceCache().FindOrRegisterAsset(
						Rendering::Constants::DefaultMaterialInstanceAssetGuid
					)
		);

		// Add materials stage by default
		Rendering::StageCache& stageCache = System::Get<Rendering::Renderer>().GetStageCache();

		Rendering::RenderItemStageMask stageMask;
		stageMask.Set(stageCache.FindIdentifier(Rendering::MaterialsStage::TypeGuid));

		EnableStages(stageMask);
	}

	StaticMeshComponent::StaticMeshComponent(const StaticMeshComponent& templateComponent, const Cloner& cloner)
		: RenderItemComponent(templateComponent, cloner)
	{
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& templateMaterialInstanceIdentifierSceneData =
			cloner.GetTemplateSceneRegistry().GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& templateStaticMeshIdentifierSceneData =
			cloner.GetTemplateSceneRegistry().GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();

		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			cloner.GetSceneRegistry().GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			cloner.GetSceneRegistry().GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();

		const Rendering::StaticMeshIdentifier templateStaticMeshIdentifier =
			templateStaticMeshIdentifierSceneData.GetComponentImplementationUnchecked(templateComponent.GetIdentifier());
		CreateDataComponent<Entity::Data::RenderItem::StaticMeshIdentifier>(
			staticMeshIdentifierSceneData,
			*this,
			cloner.GetSceneRegistry(),
			templateStaticMeshIdentifier
		);
		const Rendering::MaterialInstanceIdentifier templateMaterialInstanceIdentifier =
			templateMaterialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(templateComponent.GetIdentifier());
		CreateDataComponent<Entity::Data::RenderItem::MaterialInstanceIdentifier>(
			materialInstanceIdentifierSceneData,
			templateMaterialInstanceIdentifier
		);
	}

	StaticMeshComponent::StaticMeshComponent(const Deserializer& deserializer)
		: StaticMeshComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<StaticMeshComponent>().ToString().GetView())
			)
	{
	}

	StaticMeshComponent::StaticMeshComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer)
		: StaticMeshComponent(deserializer, ReadMesh(componentSerializer), componentSerializer)
	{
	}

	StaticMeshComponent::StaticMeshComponent(
		const Deserializer& deserializer, Rendering::StaticMeshIdentifier meshIdentifier, Optional<Serialization::Reader> componentSerializer
	)
		: StaticMeshComponent(
				deserializer,
				meshIdentifier,
				meshIdentifier.IsValid() ? System::Get<Rendering::Renderer>().GetMeshCache().GetAssetData(meshIdentifier).m_pMesh.Get()
																 : Optional<Rendering::StaticMesh*>{},
				componentSerializer
			)
	{
	}

	StaticMeshComponent::StaticMeshComponent(
		const Deserializer& deserializer,
		Rendering::StaticMeshIdentifier meshIdentifier,
		const Optional<const Rendering::StaticMesh*> pMesh,
		const Optional<Serialization::Reader> componentSerializer
	)
		: RenderItemComponent(DeserializerWithBounds{deserializer} | GetMeshBounds(pMesh))
	{
		CreateDataComponent<Entity::Data::RenderItem::StaticMeshIdentifier>(
			deserializer.GetSceneRegistry().GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>(),
			*this,
			deserializer.GetSceneRegistry(),
			meshIdentifier
		);
		CreateDataComponent<Entity::Data::RenderItem::MaterialInstanceIdentifier>(
			deserializer.GetSceneRegistry().GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>(),
			ReadMaterialInstance(componentSerializer)
		);
	}

	[[nodiscard]] Rendering::StaticMesh& CloneStaticMesh(Scene3D&, const Rendering::StaticMeshIdentifier masterMeshIdentifier)
	{
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		return *meshCache.GetAssetData(meshCache.Clone(masterMeshIdentifier)).m_pMesh;
	}

	StaticMeshComponent::~StaticMeshComponent() = default;

	void StaticMeshComponent::SetMesh(const Rendering::StaticMesh& newMesh)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();
		Entity::Data::RenderItem::StaticMeshIdentifier& meshIdentifierComponent =
			staticMeshIdentifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		meshIdentifierComponent.Set(*this, sceneRegistry, newMesh.GetIdentifier());
	}

	void StaticMeshComponent::CloneMesh(const bool allowCpuVertexAccess)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();
		Entity::Data::RenderItem::StaticMeshIdentifier& meshIdentifierComponent =
			staticMeshIdentifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		meshIdentifierComponent.Clone(*this, sceneRegistry, allowCpuVertexAccess);
	}

	void StaticMeshComponent::CloneMaterialInstance()
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();
		Entity::Data::RenderItem::MaterialInstanceIdentifier& materialIdentifierComponent =
			materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		materialIdentifierComponent.Clone(*this, sceneRegistry);
	}

	void StaticMeshComponent::SetMaterialInstanceAsset(const Asset::Picker asset)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();
		Entity::Data::RenderItem::MaterialInstanceIdentifier& materialIdentifierComponent =
			materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		materialIdentifierComponent.SetFromProperty(*this, asset);
	}

	void StaticMeshComponent::SetMaterialInstance(const Rendering::MaterialInstanceIdentifier newMaterialInstanceIdentifier)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();
		Entity::Data::RenderItem::MaterialInstanceIdentifier& materialIdentifierComponent =
			materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		materialIdentifierComponent.Set(*this, sceneRegistry, newMaterialInstanceIdentifier);
	}

	Asset::Picker StaticMeshComponent::GetMaterialInstance() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();
		Entity::Data::RenderItem::MaterialInstanceIdentifier& materialIdentifierComponent =
			materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		return materialIdentifierComponent.GetFromProperty(const_cast<StaticMeshComponent&>(*this));
	}

	void StaticMeshComponent::SetStaticMesh(const Asset::Picker asset)
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();
		Entity::Data::RenderItem::StaticMeshIdentifier& meshIdentifierComponent =
			staticMeshIdentifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		meshIdentifierComponent.SetFromProperty(*this, asset);
	}

	Asset::Picker StaticMeshComponent::GetStaticMesh() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();
		Entity::Data::RenderItem::StaticMeshIdentifier& meshIdentifierComponent =
			staticMeshIdentifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		return meshIdentifierComponent.GetFromProperty(const_cast<StaticMeshComponent&>(*this));
	}

	Rendering::StaticMeshIdentifier StaticMeshComponent::GetMeshIdentifier() const
	{
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>& staticMeshIdentifierSceneData =
			GetSceneRegistry().GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>();
		return staticMeshIdentifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());
	}

	const Rendering::StaticMesh& StaticMeshComponent::GetMesh() const
	{
		const Rendering::StaticMeshIdentifier staticMeshIdentifier = GetMeshIdentifier();
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		return *meshCache.GetAssetData(staticMeshIdentifier).m_pMesh;
	}

	Rendering::MaterialInstanceIdentifier StaticMeshComponent::GetMaterialInstanceIdentifier() const
	{
		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>& materialInstanceIdentifierSceneData =
			GetSceneRegistry().GetCachedSceneData<Entity::Data::RenderItem::MaterialInstanceIdentifier>();
		return materialInstanceIdentifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());
	}

	[[maybe_unused]] const bool wasStaticMeshComponentRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<StaticMeshComponent>>::Make());
	[[maybe_unused]] const bool wasStaticMeshComponentTypeRegistered = Reflection::Registry::RegisterType<StaticMeshComponent>();
}

namespace ngine::Entity::Data::RenderItem
{
	StaticMeshIdentifier::StaticMeshIdentifier(
		HierarchyComponentBase& owner, SceneRegistry& sceneRegistry, const Rendering::StaticMeshIdentifier identifier
	)
		: m_identifier(identifier)
	{
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		const Rendering::StaticMesh& newMesh = *meshCache.GetAssetData(identifier).m_pMesh;
		newMesh.OnBoundingBoxChanged.Add(
			*this,
			[&owner, &sceneRegistry](StaticMeshIdentifier& staticMeshIdentifier)
			{
				staticMeshIdentifier.OnMeshBoundingBoxChanged(owner, sceneRegistry);
			}
		);
	}

	void StaticMeshIdentifier::OnDestroying()
	{
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		if (const Optional<const Rendering::StaticMesh*> pMesh = meshCache.GetAssetData(m_identifier).m_pMesh.Get())
		{
			[[maybe_unused]] const bool wasRemoved = pMesh->OnBoundingBoxChanged.Remove(this);
			Assert(wasRemoved);
		}
	}

	void StaticMeshIdentifier::Set(HierarchyComponentBase& owner, SceneRegistry& sceneRegistry, Rendering::StaticMeshIdentifier newIdentifier)
	{
		const Rendering::StaticMeshIdentifier previousMeshIdentifier = m_identifier;
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		const Rendering::StaticMesh& previousMesh = *meshCache.GetAssetData(previousMeshIdentifier).m_pMesh;

		[[maybe_unused]] const bool wasRemoved = previousMesh.OnBoundingBoxChanged.Remove(this);
		Assert(wasRemoved);

		if (previousMesh.IsClone())
		{
			newIdentifier = meshCache.Clone(newIdentifier, previousMesh.GetFlags());
		}

		const bool changed = newIdentifier != previousMeshIdentifier;
		m_identifier = newIdentifier;
		if (changed)
		{
			Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StageMask>& stageMaskIdentifierSceneData =
				sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StageMask>();
			Entity::Data::RenderItem::StageMask& stageMask = stageMaskIdentifierSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier(
			));
			stageMask.ResetStages(owner);
			OnMeshBoundingBoxChanged(owner, sceneRegistry);
		}

		const Rendering::StaticMesh& newMesh = *meshCache.GetAssetData(newIdentifier).m_pMesh;
		newMesh.OnBoundingBoxChanged.Add(
			*this,
			[&owner, &sceneRegistry](StaticMeshIdentifier& staticMeshIdentifier)
			{
				staticMeshIdentifier.OnMeshBoundingBoxChanged(owner, sceneRegistry);
			}
		);
	}

	void StaticMeshIdentifier::Clone(HierarchyComponentBase& owner, SceneRegistry& sceneRegistry, const bool allowCpuVertexAccess)
	{
		const Rendering::StaticMeshIdentifier previousMeshIdentifier = m_identifier;
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		const Rendering::StaticMesh& previousMesh = *meshCache.GetAssetData(previousMeshIdentifier).m_pMesh;

		[[maybe_unused]] const bool wasRemoved = previousMesh.OnBoundingBoxChanged.Remove(this);
		Assert(wasRemoved);

		const Rendering::StaticMeshIdentifier newIdentifier =
			meshCache.Clone(previousMeshIdentifier, Rendering::StaticMeshFlags::AllowCpuVertexAccess * allowCpuVertexAccess);
		m_identifier = newIdentifier;

		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StageMask>& stageMaskIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StageMask>();
		Entity::Data::RenderItem::StageMask& stageMask = stageMaskIdentifierSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier()
		);
		stageMask.ResetStages(owner);

		const Rendering::StaticMesh& newMesh = *meshCache.GetAssetData(newIdentifier).m_pMesh;
		newMesh.OnBoundingBoxChanged.Add(
			*this,
			[&owner, &sceneRegistry](StaticMeshIdentifier& staticMeshIdentifier)
			{
				staticMeshIdentifier.OnMeshBoundingBoxChanged(owner, sceneRegistry);
			}
		);
	}

	void StaticMeshIdentifier::OnMeshBoundingBoxChanged(HierarchyComponentBase& owner, SceneRegistry& sceneRegistry)
	{
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		const Rendering::StaticMesh& mesh = *meshCache.GetAssetData(m_identifier).m_pMesh;

		ComponentTypeSceneData<Data::BoundingBox>& boundingBoxSceneData = sceneRegistry.GetCachedSceneData<Data::BoundingBox>();
		Data::BoundingBox& boundingBox = boundingBoxSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier());

		boundingBox.Set(owner, sceneRegistry, mesh.GetBoundingBox());
	}

	Asset::Picker StaticMeshIdentifier::GetFromProperty(Entity::HierarchyComponentBase&) const
	{
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		const Rendering::StaticMesh& mesh = *meshCache.GetAssetData(m_identifier).m_pMesh;
		if (mesh.IsClone())
		{
			return {meshCache.GetAssetGuid(mesh.GetTemplateIdentifier()), MeshPartAssetType::AssetFormat.assetTypeGuid};
		}
		else
		{
			return {meshCache.GetAssetGuid(m_identifier), MeshPartAssetType::AssetFormat.assetTypeGuid};
		}
	}

	void StaticMeshIdentifier::SetFromProperty(Entity::HierarchyComponentBase& owner, const Asset::Picker asset)
	{
		Rendering::MeshCache& meshCache = System::Get<Rendering::Renderer>().GetMeshCache();
		Set(owner, owner.GetSceneRegistry(), meshCache.FindOrRegisterAsset(asset.GetAssetGuid()));
	}

	void MaterialInstanceIdentifier::Set(
		HierarchyComponentBase& owner, SceneRegistry& sceneRegistry, Rendering::MaterialInstanceIdentifier newIdentifier
	)
	{
		const Rendering::MaterialInstanceIdentifier previousMaterialInstanceIdentifier = m_identifier;
		const bool changed = newIdentifier != previousMaterialInstanceIdentifier;

		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		Rendering::MaterialCache& materialCache = renderer.GetMaterialCache();
		Rendering::MaterialInstanceCache& materialInstanceCache = materialCache.GetInstanceCache();

		const Rendering::RuntimeMaterialInstance& previousMaterialInstance =
			*materialInstanceCache.GetAssetData(previousMaterialInstanceIdentifier).m_pMaterialInstance;
		if (previousMaterialInstance.IsClone())
		{
			newIdentifier = materialInstanceCache.Clone(newIdentifier);
		}

		m_identifier = newIdentifier;
		if (changed)
		{
			Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StageMask>& stageMaskIdentifierSceneData =
				sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StageMask>();
			Entity::Data::RenderItem::StageMask& stageMask = stageMaskIdentifierSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier(
			));
			stageMask.ResetStages(owner);
		}
	}

	void MaterialInstanceIdentifier::Clone(HierarchyComponentBase& owner, SceneRegistry& sceneRegistry)
	{
		Rendering::Renderer& renderer = System::Get<Rendering::Renderer>();
		Rendering::MaterialCache& materialCache = renderer.GetMaterialCache();
		Rendering::MaterialInstanceCache& materialInstanceCache = materialCache.GetInstanceCache();

		const Rendering::MaterialInstanceIdentifier previousMaterialInstanceIdentifier = m_identifier;
		const Rendering::MaterialInstanceIdentifier newMaterialInstanceIdentifier =
			materialInstanceCache.Clone(previousMaterialInstanceIdentifier);
		m_identifier = newMaterialInstanceIdentifier;

		Entity::ComponentTypeSceneData<Entity::Data::RenderItem::StageMask>& stageMaskIdentifierSceneData =
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StageMask>();
		Entity::Data::RenderItem::StageMask& stageMask = stageMaskIdentifierSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier()
		);
		stageMask.ResetStages(owner);
	}

	Asset::Picker MaterialInstanceIdentifier::GetFromProperty(Entity::HierarchyComponentBase&) const
	{
		Rendering::MaterialCache& materialCache = System::Get<Rendering::Renderer>().GetMaterialCache();
		Rendering::MaterialInstanceCache& materialInstanceCache = materialCache.GetInstanceCache();
		const Rendering::RuntimeMaterialInstance& runtimeMaterialInstance =
			*materialInstanceCache.GetAssetData(m_identifier).m_pMaterialInstance;
		if (runtimeMaterialInstance.IsClone())
		{
			return {
				materialInstanceCache.GetAssetGuid(runtimeMaterialInstance.GetTemplateMaterialInstanceIdentifier()),
				MaterialInstanceAssetType::AssetFormat.assetTypeGuid
			};
		}
		else
		{
			return {materialInstanceCache.GetAssetGuid(m_identifier), MaterialInstanceAssetType::AssetFormat.assetTypeGuid};
		}
	}

	void MaterialInstanceIdentifier::SetFromProperty(Entity::HierarchyComponentBase& owner, const Asset::Picker asset)
	{
		Rendering::MaterialCache& materialCache = System::Get<Rendering::Renderer>().GetMaterialCache();
		Rendering::MaterialInstanceCache& materialInstanceCache = materialCache.GetInstanceCache();

		Set(owner, owner.GetSceneRegistry(), materialInstanceCache.FindOrRegisterAsset(asset.GetAssetGuid()));
	}

	[[maybe_unused]] const bool wasStaticMeshIdentifierTypeRegistered = Reflection::Registry::RegisterType<StaticMeshIdentifier>();
	[[maybe_unused]] const bool wasStaticMeshIdentifierRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<StaticMeshIdentifier>>::Make());

	[[maybe_unused]] const bool wasMaterialInstanceIdentifierTypeRegistered = Reflection::Registry::RegisterType<MaterialInstanceIdentifier>(
	);
	[[maybe_unused]] const bool wasMaterialInstanceIdentifierRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<MaterialInstanceIdentifier>>::Make());

	[[maybe_unused]] const bool wasVisibilityListenerTypeRegistered = Reflection::Registry::RegisterType<VisibilityListener>();
	[[maybe_unused]] const bool wasVisibilityListenerRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<VisibilityListener>>::Make());
}
