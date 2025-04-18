#include "Scene/Scene2D.h"
#include "Scene/Scene2DAssetType.h"

#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/RootSceneComponent2D.h>
#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Project/Project.h>

namespace ngine
{
	Scene2D::Scene2D(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Entity::HierarchyComponentBase*> pRootParent,
		const Guid assetGuid,
		const EnumFlags<Flags> flags
	)
		: SceneBase(sceneRegistry, assetGuid, flags)
	{
		m_pRootComponent =
			sceneRegistry.GetOrCreateComponentTypeData<Entity::RootSceneComponent2D>()->CreateInstance(pRootParent, sceneRegistry, *this);

		if (!flags.IsSet(Flags::IsTemplate))
		{
			if (const Optional<Project*> pCurrentProject = System::Find<Project>(); pCurrentProject.IsValid() && pCurrentProject->IsValid())
			{
				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				const Asset::Identifier projectRootFolderAssetIdentifier =
					assetManager.FindOrRegisterFolder(pCurrentProject->GetInfo()->GetDirectory(), Asset::Identifier{});
				assetManager.RegisterAsset(
					assetGuid,
					Asset::DatabaseEntry{
						Scene2DAssetType::AssetFormat.assetTypeGuid,
						Reflection::GetTypeGuid<Entity::RootSceneComponent2D>(),
						IO::Path::Combine(
							pCurrentProject->GetInfo()->GetDirectory(),
							pCurrentProject->GetInfo()->GetRelativeAssetDirectory(),
							MAKE_PATH("2D Scenes"),
							IO::Path::Merge(MAKE_PATH("MyScene"), Scene2DAssetType::AssetFormat.metadataFileExtension)
						)
					},
					projectRootFolderAssetIdentifier
				);
			}
		}

		OnEnabledUpdate.Add(
			this,
			[](Scene2D& scene)
			{
				scene.ModifyFrameGraph(
					[&scene]()
					{
						Threading::StageBase& quadtreeCleanupJob = scene.GetRootComponent().GetQuadtreeCleanupJob();
						scene.GetEntitySceneRegistry().GetDynamicLateUpdatesFinishedStage().AddSubsequentStage(quadtreeCleanupJob);
						quadtreeCleanupJob.AddSubsequentStage(scene.GetEndFrameStage());
					}
				);
			}
		);
		OnDisabledUpdate.Add(
			this,
			[](Scene2D& scene)
			{
				Threading::StageBase& quadtreeCleanupJob = scene.GetRootComponent().GetQuadtreeCleanupJob();
				scene.GetEntitySceneRegistry()
					.GetDynamicLateUpdatesFinishedStage()
					.RemoveSubsequentStage(quadtreeCleanupJob, Invalid, Threading::StageBase::RemovalFlags{});
				quadtreeCleanupJob.RemoveSubsequentStage(scene.GetEndFrameStage(), Invalid, Threading::StageBase::RemovalFlags{});
			}
		);
	}

	PURE_LOCALS_AND_POINTERS Entity::RootSceneComponent2D& Scene2D::GetRootComponent()
	{
		return static_cast<Entity::RootSceneComponent2D&>(SceneBase::GetRootComponent());
	}

	PURE_LOCALS_AND_POINTERS const Entity::RootSceneComponent2D& Scene2D::GetRootComponent() const
	{
		return static_cast<const Entity::RootSceneComponent2D&>(SceneBase::GetRootComponent());
	}

	void Scene2D::ProcessDestroyedComponentsQueueInternal(const ArrayView<ReferenceWrapper<Entity::HierarchyComponentBase>> components)
	{
		Entity::SceneRegistry& sceneRegistry = m_sceneRegistry;
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();

		// TODO: Batch destroy API
		for (Entity::HierarchyComponentBase& component : components)
		{
			const Optional<Entity::Component2D*> pComponent2D = component.As<Entity::Component2D>(sceneRegistry);
			Assert(pComponent2D.IsValid());
			if (UNLIKELY_ERROR(!pComponent2D.IsValid()))
			{
				continue;
			}

			if constexpr (ENABLE_ASSERTS)
			{
				[[maybe_unused]] AtomicEnumFlags<Entity::ComponentFlags>& componentFlags =
					flagsSceneData.GetComponentImplementationUnchecked(pComponent2D->GetIdentifier());
				Assert(
					componentFlags.AreAnySet(Entity::ComponentFlags::IsDisabledFromAnySource) &&
					componentFlags.IsSet(Entity::ComponentFlags::IsDestroying)
				);
			}

			Entity::Component2D::ChildContainer children = pComponent2D->StealChildren();

			using ConstChildView = Entity::Component2D::ChildContainer::ConstView;
			using DestroyChildrenFunction = void (*)(
				const ConstChildView,
				Entity::SceneRegistry& sceneRegistry,
				Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData
			);
			static DestroyChildrenFunction destroyComponentChildren = [](
																																	const ConstChildView children,
																																	Entity::SceneRegistry& sceneRegistry,
																																	Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData
																																)
			{
				for (Entity::HierarchyComponentBase& childComponent : children)
				{
					Entity::Component2D& childComponent2D = childComponent.AsExpected<Entity::Component2D>(sceneRegistry);
					Entity::Component2D::ChildContainer childChildren = childComponent2D.StealChildren();

					AtomicEnumFlags<Entity::ComponentFlags>& childFlags =
						flagsSceneData.GetComponentImplementationUnchecked(childComponent2D.GetIdentifier());
					const EnumFlags<Entity::ComponentFlags> previousFlags = childFlags.FetchOr(
						Entity::ComponentFlags::WasDisabledByParent | Entity::ComponentFlags::WasDetachedFromOctreeByParent |
						Entity::ComponentFlags::IsDestroying
					);

					if (!previousFlags.AreAnySet(Entity::ComponentFlags::IsDisabledFromAnySource))
					{
						childComponent2D.DisableInternal(sceneRegistry);
					}
					if (!previousFlags.AreAnySet(Entity::ComponentFlags::IsDetachedFromTreeFromAnySource))
					{
						childComponent2D.GetRootSceneComponent().RemoveComponent(childComponent2D);
					}
					destroyComponentChildren(childChildren, sceneRegistry, flagsSceneData);

					if (!previousFlags.AreAnySet(Entity::ComponentFlags::IsDestroying))
					{
						childComponent2D.DestroyInternal(sceneRegistry);
					}
				}
			};

			destroyComponentChildren(children, sceneRegistry, flagsSceneData);

			pComponent2D->DestroyInternal(sceneRegistry);
			// Component is invalid at this point
		}
	}
}
