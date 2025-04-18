#include "Scene/Scene.h"
#include "Scene/Scene3DAssetType.h"

#include <Engine/Asset/AssetManager.h>
#include <Engine/Entity/Scene/SceneRegistry.h>
#include <Engine/Entity/RootSceneComponent.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Data/Flags.h>
#include <Engine/Entity/ComponentTypeSceneData.h>
#include <Engine/Entity/Scene/SceneChildInstance.h>
#include <Engine/Project/Project.h>
#include <Engine/Reflection/Registry.h>

#include <Common/System/Query.h>
#include <Common/Reflection/Registry.inl>

namespace ngine
{
	Scene3D::Scene3D(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Entity::HierarchyComponentBase*> pRootParent,
		const Math::Radius<Math::WorldCoordinateUnitType> radius,
		const Asset::Guid assetGuid,
		const EnumFlags<Flags> flags,
		const uint16 maximumUpdateRate
	)
		: SceneBase(sceneRegistry, assetGuid, flags | Flags::IsDisabled)
		, m_maximumUpdateRate(maximumUpdateRate)
	{
		m_pRootComponent = sceneRegistry.GetOrCreateComponentTypeData<Entity::RootSceneComponent>()->CreateInstance(
			Entity::RootSceneComponent::Initializer{Reflection::TypeInitializer{}, pRootParent, *this, sceneRegistry, radius}
		);

		if (!flags.IsSet(Flags::IsTemplate))
		{
			Project& currentProject = System::Get<Project>();
			if (currentProject.IsValid())
			{
				Asset::Manager& assetManager = System::Get<Asset::Manager>();
				const Asset::Identifier projectRootFolderAssetIdentifier =
					assetManager.FindOrRegisterFolder(currentProject.GetInfo()->GetDirectory(), Asset::Identifier{});
				assetManager.RegisterAsset(
					assetGuid,
					Asset::DatabaseEntry{
						Scene3DAssetType::AssetFormat.assetTypeGuid,
						Reflection::GetTypeGuid<Entity::RootSceneComponent>(),
						IO::Path::Combine(
							currentProject.GetInfo()->GetDirectory(),
							currentProject.GetInfo()->GetRelativeAssetDirectory(),
							MAKE_PATH("Scenes"),
							IO::Path::Merge(MAKE_PATH("MyScene"), Scene3DAssetType::AssetFormat.metadataFileExtension)
						)
					},
					projectRootFolderAssetIdentifier
				);
			}
		}

		OnEnabledUpdate.Add(
			this,
			[](Scene3D& scene)
			{
				scene.OnEnabledInternal();
			}
		);
		OnDisabledUpdate.Add(
			this,
			[](Scene3D& scene)
			{
				scene.OnDisabledInternal();
			}
		);

		if (flags.IsNotSet(Flags::IsDisabled))
		{
			m_flags &= ~Flags::IsDisabled;
			Enable();
		}
	}

	Scene3D::Scene3D(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Entity::HierarchyComponentBase*> pRootParent,
		const Serialization::Reader reader,
		const Asset::Guid assetGuid,
		Threading::JobBatch& jobBatchOut,
		const EnumFlags<Flags> flags,
		const uint16 maximumUpdateRate
	)
		: SceneBase(sceneRegistry, assetGuid, flags | Flags::IsDisabled)
		, m_maximumUpdateRate(maximumUpdateRate)
	{
		m_pRootComponent =
			sceneRegistry.GetOrCreateComponentTypeData<Entity::RootSceneComponent>()->CreateInstance(Entity::RootSceneComponent::Deserializer{
				Reflection::TypeDeserializer{reader, System::Get<Reflection::EngineRegistry>(), &jobBatchOut},
				pRootParent,
				*this
			});

		jobBatchOut.QueueAfterStartStage(GetRootComponent().DeserializeDataComponentsAndChildren(reader));

		OnEnabledUpdate.Add(
			this,
			[](Scene3D& scene)
			{
				scene.OnEnabledInternal();
			}
		);
		OnDisabledUpdate.Add(
			this,
			[](Scene3D& scene)
			{
				scene.OnDisabledInternal();
			}
		);

		if (flags.IsNotSet(Flags::IsDisabled))
		{
			m_flags &= ~Flags::IsDisabled;
			Enable();
		}
	}

	Scene3D::Scene3D(
		Entity::SceneRegistry& sceneRegistry,
		const Optional<Entity::HierarchyComponentBase*> pRootParent,
		const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier,
		Threading::JobBatch& jobBatchOut,
		const EnumFlags<Flags> flags,
		const uint16 maximumUpdateRate
	)
		: SceneBase(
				sceneRegistry,
				System::Get<Entity::Manager>().GetComponentTemplateCache().GetAssetGuid(sceneTemplateIdentifier),
				flags | Flags::IsDisabled
			)
		, m_maximumUpdateRate(maximumUpdateRate)
	{
		m_pRootComponent =
			sceneRegistry.GetOrCreateComponentTypeData<Entity::RootSceneComponent>()->CreateInstance(*this, sceneTemplateIdentifier, pRootParent);

		if (!jobBatchOut.IsValid())
		{
			jobBatchOut = {
				Threading::CreateIntermediateStage("Start Load Scene3D Stage"),
				Threading::CreateIntermediateStage("End Load Scene3D Stage")
			};
		}

		Threading::IntermediateStage& finishedLoadingStage = Threading::CreateIntermediateStage("Finish Scene3D Loading Stage");
		finishedLoadingStage.AddSubsequentStage(jobBatchOut.GetFinishedStage());

		Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
		Threading::JobBatch sceneLoadJobBatch = sceneTemplateCache.TryLoadScene(
			sceneTemplateIdentifier,
			Entity::ComponentTemplateCache::LoadListenerData(
				*this,
				[&rootComponent = GetRootComponent(),
		     &finishedLoadingStage](Scene3D&, const Entity::ComponentTemplateIdentifier sceneTemplateIdentifier)
				{
					Entity::ComponentTemplateCache& sceneTemplateCache = System::Get<Entity::Manager>().GetComponentTemplateCache();
					const Optional<const Entity::Component3D*> pTemplateComponent =
						sceneTemplateCache.GetAssetData(sceneTemplateIdentifier).m_pRootComponent;
					Assert(pTemplateComponent.IsValid());
					if (LIKELY(pTemplateComponent.IsValid()))
					{
						rootComponent.SetInstanceGuid(pTemplateComponent->GetInstanceGuid(sceneTemplateCache.GetTemplateSceneRegistry()));

						Entity::SceneRegistry& sceneRegistry = rootComponent.GetSceneRegistry();
						const Entity::SceneRegistry& templateSceneRegistry = pTemplateComponent->GetSceneRegistry();

						// Clone root component data components
						Threading::JobBatch dataComponentsJobBatch;
						pTemplateComponent->IterateDataComponents(
							templateSceneRegistry,
							[&targetComponent = rootComponent,
				       &templateComponent = *pTemplateComponent,
				       &dataComponentsJobBatch,
				       &sceneRegistry,
				       &templateSceneRegistry](Entity::Data::Component& templateDataComponent, const Optional<Entity::ComponentTypeInterface*> pComponentTypeInfo, Entity::ComponentTypeSceneDataInterface&)
							{
								Entity::ComponentValue<Entity::Data::Component> componentValue;
								Threading::JobBatch dataComponentBatch;
								componentValue.CloneFromTemplate(
									sceneRegistry,
									templateSceneRegistry,
									*pComponentTypeInfo,
									templateDataComponent,
									templateComponent,
									targetComponent,
									dataComponentBatch
								);
								if (dataComponentBatch.IsValid())
								{
									dataComponentsJobBatch.QueueAfterStartStage(dataComponentBatch);
								}
								return Memory::CallbackResult::Continue;
							}
						);

						// Clone root component children
						using CloneComponent = void (*)(
							const Entity::Component3D& templateComponent,
							const Entity::Component3D& templateComponentParent,
							Entity::Component3D& parent,
							Entity::ComponentRegistry& componentRegistry,
							Entity::SceneRegistry& sceneRegistry,
							const Entity::SceneRegistry& templateSceneRegistry,
							Threading::JobBatch& jobBatchOut
						);
						static CloneComponent cloneComponent = [](
																										 const Entity::Component3D& templateComponent,
																										 const Entity::Component3D& templateComponentParent,
																										 Entity::Component3D& parent,
																										 Entity::ComponentRegistry& componentRegistry,
																										 Entity::SceneRegistry& sceneRegistry,
																										 const Entity::SceneRegistry& templateSceneRegistry,
																										 Threading::JobBatch& jobBatchOut
																									 )
						{
							Entity::ComponentTypeInterface& typeInterface =
								*componentRegistry.Get(componentRegistry.FindIdentifier(templateComponent.GetTypeGuid(templateSceneRegistry)));
							const Guid templateInstanceGuid = templateComponent.GetInstanceGuid(templateSceneRegistry);
							Optional<Entity::Component*> pComponent = typeInterface.CloneFromTemplateManualOnCreated(
								templateInstanceGuid, // Guid::Generate(),
								templateComponent,
								templateComponentParent,
								parent,
								sceneRegistry,
								templateSceneRegistry,
								jobBatchOut
							);

							Assert(
								pComponent.IsValid() || typeInterface.GetTypeInterface().GetFlags().IsSet(Reflection::TypeFlags::DisableDynamicCloning) ||
								!templateComponent.CanClone()
							);
							if (LIKELY(pComponent.IsValid()))
							{
								Entity::Component3D& component = static_cast<Entity::Component3D&>(*pComponent);

								/*component.CreateDataComponent<Entity::SceneChildInstance>(
						        templateInstanceGuid,
						        templateInstanceGuid
						    );*/

								for (Entity::Component3D& templateChild : templateComponent.GetChildren())
								{
									cloneComponent(
										templateChild,
										component,
										static_cast<Entity::Component3D&>(*pComponent),
										componentRegistry,
										sceneRegistry,
										templateSceneRegistry,
										jobBatchOut
									);
								}
							}

							if (pComponent)
							{
								typeInterface.OnComponentCreated(*pComponent, parent, sceneRegistry);
							}
						};

						Threading::JobBatch cloneChildJobBatch;
						Entity::ComponentRegistry& componentRegistry = System::Get<Entity::Manager>().GetRegistry();
						for (Entity::Component3D& templateChild : pTemplateComponent->GetChildren())
						{
							cloneComponent(
								templateChild,
								*pTemplateComponent,
								rootComponent,
								componentRegistry,
								sceneRegistry,
								templateSceneRegistry,
								cloneChildJobBatch
							);
						}

						if (cloneChildJobBatch.IsValid())
						{
							cloneChildJobBatch.GetFinishedStage().AddSubsequentStage(finishedLoadingStage);
						}

						if (dataComponentsJobBatch.IsValid())
						{
							dataComponentsJobBatch.GetFinishedStage().AddSubsequentStage(finishedLoadingStage);

							if (cloneChildJobBatch.IsValid())
							{
								dataComponentsJobBatch.QueueAsNewFinishedStage(cloneChildJobBatch);
							}

							Threading::JobRunnerThread::GetCurrent()->Queue(dataComponentsJobBatch);
						}
						else if (cloneChildJobBatch.IsValid())
						{
							Threading::JobRunnerThread::GetCurrent()->Queue(cloneChildJobBatch);
						}
						else
						{
							finishedLoadingStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
						}
					}
					else
					{
						finishedLoadingStage.SignalExecutionFinishedAndDestroying(*Threading::JobRunnerThread::GetCurrent());
					}
					return EventCallbackResult::Remove;
				}
			)
		);
		jobBatchOut.QueueAfterStartStage(sceneLoadJobBatch);

		OnEnabledUpdate.Add(
			this,
			[](Scene3D& scene)
			{
				scene.OnEnabledInternal();
			}
		);
		OnDisabledUpdate.Add(
			this,
			[](Scene3D& scene)
			{
				scene.OnDisabledInternal();
			}
		);

		if (flags.IsNotSet(Flags::IsDisabled))
		{
			Enable();
		}
	}

	void Scene::OnEnabledInternal()
	{
		GetRootComponent().Enable(m_sceneRegistry);
	}

	void Scene::OnDisabledInternal()
	{
		GetRootComponent().Disable(m_sceneRegistry);
	}

	Scene3D::~Scene3D()
	{
		GetRootComponent().Destroy(m_sceneRegistry);
		// Temporary workaround to ensure we don't run out of component identifiers when switching scenes
		System::Get<Entity::Manager>().GetComponentTemplateCache().Reset();

		ProcessFullDestroyedComponentsQueue();
	}

	PURE_LOCALS_AND_POINTERS Entity::RootSceneComponent& Scene3D::GetRootComponent() const
	{
		return static_cast<Entity::RootSceneComponent&>(*m_pRootComponent);
	}

	bool Scene3D::Serialize(Serialization::Writer serializer) const
	{
		serializer.Serialize("guid", m_guid);
		serializer.Serialize("type_version", Version::Latest);
		return GetRootComponent().Serialize(serializer);
	}

	void Scene3D::Remix(const Asset::Guid newGuid)
	{
		m_guid = newGuid;
		GetRootComponent().SetSceneGuid(newGuid);
		m_flags |= Flags::IsEditing;
	}

	void Scene3D::ProcessDestroyedComponentsQueueInternal(const ArrayView<ReferenceWrapper<Entity::HierarchyComponentBase>> components)
	{
		Entity::SceneRegistry& sceneRegistry = m_sceneRegistry;
		Entity::ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();

		// TODO: Batch destroy API
		for (Entity::HierarchyComponentBase& component : components)
		{
			const Optional<Entity::Component3D*> pComponent3D = component.As<Entity::Component3D>(sceneRegistry);
			if (!pComponent3D.IsValid())
			{
				continue;
			}

			if constexpr (ENABLE_ASSERTS)
			{
				[[maybe_unused]] AtomicEnumFlags<Entity::ComponentFlags>& componentFlags =
					flagsSceneData.GetComponentImplementationUnchecked(pComponent3D->GetIdentifier());
				Assert(
					componentFlags.AreAnySet(Entity::ComponentFlags::IsDisabledFromAnySource) &&
					componentFlags.IsSet(Entity::ComponentFlags::IsDestroying)
				);
			}

			Entity::Component3D::ChildContainer children = pComponent3D->StealChildren();

			using ConstChildView = Entity::Component3D::ChildContainer::ConstView;
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
					Entity::Component3D& childComponent3D = childComponent.AsExpected<Entity::Component3D>(sceneRegistry);
					Entity::Component3D::ChildContainer childChildren = childComponent3D.StealChildren();

					AtomicEnumFlags<Entity::ComponentFlags>& childFlags =
						flagsSceneData.GetComponentImplementationUnchecked(childComponent3D.GetIdentifier());
					const EnumFlags<Entity::ComponentFlags> previousFlags = childFlags.FetchOr(
						Entity::ComponentFlags::WasDisabledByParent | Entity::ComponentFlags::WasDetachedFromOctreeByParent |
						Entity::ComponentFlags::IsDestroying
					);

					if (!previousFlags.AreAnySet(Entity::ComponentFlags::IsDisabledFromAnySource))
					{
						childComponent3D.DisableInternal(sceneRegistry);
					}
					if (!previousFlags.AreAnySet(Entity::ComponentFlags::IsDetachedFromTreeFromAnySource))
					{
						childComponent3D.GetRootSceneComponent().RemoveComponent(childComponent3D);
					}
					destroyComponentChildren(childChildren, sceneRegistry, flagsSceneData);

					if (!previousFlags.AreAnySet(Entity::ComponentFlags::IsDestroying))
					{
						childComponent3D.DestroyInternal(sceneRegistry);
					}
				}
			};

			destroyComponentChildren(children, sceneRegistry, flagsSceneData);

			pComponent3D->DestroyInternal(sceneRegistry);
			// Component is invalid at this point
		}
	}

	[[maybe_unused]] const bool wasAssetTypeRegistered = Reflection::Registry::RegisterType<Scene3DAssetType>();
	[[maybe_unused]] const bool wasScene3DTypeRegistered = Reflection::Registry::RegisterType<Scene3D>();
}
