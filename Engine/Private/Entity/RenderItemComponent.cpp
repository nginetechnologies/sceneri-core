#include "Entity/RenderItemComponent.h"
#include "Entity/Data/Tags.h"

#include <Engine/Scene/Scene.h>
#include <Engine/Entity/Component3D.inl>
#include <Engine/Entity/Data/RenderItem/StageMask.h>
#include <Engine/Entity/Data/RenderItem/TransformChangeTracker.h>
#include <Engine/Entity/Data/RenderItem/Identifier.h>
#include <Common/System/Query.h>
#include <Engine/Tag/TagRegistry.h>

#include <Common/Serialization/Reader.h>
#include <Common/Serialization/Guid.h>
#include <Common/Reflection/Registry.inl>

#include <Renderer/Renderer.h>
#include <Renderer/Stages/Serialization/RenderItemStageMask.h>
#include <Renderer/Stages/RenderItemStagesProperty.h>

namespace ngine::Rendering
{
	bool RenderItemStagesProperty::Serialize(const Serialization::Reader serializer)
	{
		return m_mask.Serialize(serializer, m_stageCache);
	}

	bool RenderItemStagesProperty::Serialize(Serialization::Writer serializer) const
	{
		return m_mask.Serialize(serializer, m_stageCache);
	}
}

namespace ngine::Entity
{
	RenderItemComponent::RenderItemComponent(const RenderItemComponent& templateComponent, const Cloner& cloner)
		: Component3D(templateComponent, cloner)
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		ComponentTypeSceneData<Data::RenderItem::Identifier>& identifierSceneData =
			sceneRegistry.GetCachedSceneData<Data::RenderItem::Identifier>();
		ComponentTypeSceneData<Data::RenderItem::TransformChangeTracker>& transformChangeSceneData =
			sceneRegistry.GetCachedSceneData<Data::RenderItem::TransformChangeTracker>();
		ComponentTypeSceneData<Data::Tags>& tagsSceneData = sceneRegistry.GetCachedSceneData<Data::Tags>();

		[[maybe_unused]] const Optional<Data::RenderItem::StageMask*> pStageMask =
			CreateDataComponent<Data::RenderItem::StageMask>(stageMaskSceneData, templateComponent.GetStageMask());
		Assert(pStageMask.IsValid());
		[[maybe_unused]] const Optional<Data::RenderItem::TransformChangeTracker*> pTransformChangeTracker =
			CreateDataComponent<Data::RenderItem::TransformChangeTracker>(transformChangeSceneData);
		Assert(pTransformChangeTracker.IsValid());
		[[maybe_unused]] const Optional<Data::RenderItem::Identifier*> pIdentifier =
			CreateDataComponent<Data::RenderItem::Identifier>(identifierSceneData, scene.AcquireNewRenderItemIdentifier());
		Assert(pIdentifier.IsValid());

		Optional<Data::Tags*> pTagComponent = FindDataComponentOfType<Data::Tags>(tagsSceneData);
		if (pTagComponent.IsInvalid())
		{
			pTagComponent = CreateDataComponent<Data::Tags>(
				tagsSceneData,
				Data::Tags::Initializer{Entity::Data::Component3D::DynamicInitializer{*this, cloner.GetSceneRegistry()}}
			);
		}

		const Tag::Identifier renderItemTagIdentifier =
			System::Get<Tag::Registry>().FindOrRegister("{13CA71B8-D868-4F7C-A0BB-C7585DB11327}"_asset, Tag::Flags::Transient);

		Assert(pTagComponent.IsValid());
		pTagComponent->SetTag(GetIdentifier(), cloner.GetSceneRegistry(), renderItemTagIdentifier);
	}

	RenderItemComponent::RenderItemComponent(const Deserializer& deserializer)
		: RenderItemComponent(
				deserializer, deserializer.m_reader.FindSerializer(Reflection::GetTypeGuid<RenderItemComponent>().ToString().GetView())
			)
	{
	}

	RenderItemComponent::RenderItemComponent(const Deserializer& deserializer, const Optional<Serialization::Reader> componentSerializer)
		: Component3D(deserializer)
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		ComponentTypeSceneData<Data::RenderItem::Identifier>& identifierSceneData =
			sceneRegistry.GetCachedSceneData<Data::RenderItem::Identifier>();
		ComponentTypeSceneData<Data::Tags>& tagsSceneData = sceneRegistry.GetCachedSceneData<Data::Tags>();
		ComponentTypeSceneData<Data::RenderItem::TransformChangeTracker>& transformChangeSceneData =
			sceneRegistry.GetCachedSceneData<Data::RenderItem::TransformChangeTracker>();

		[[maybe_unused]] const Optional<Data::RenderItem::StageMask*> pStageMask = CreateDataComponent<Data::RenderItem::StageMask>(
			stageMaskSceneData,
			componentSerializer.IsValid() ? componentSerializer->ReadWithDefaultValue<Rendering::RenderItemStageMask>(
																				"stages",
																				Rendering::RenderItemStageMask(),
																				System::Get<Rendering::Renderer>().GetStageCache()
																			)
																		: Rendering::RenderItemStageMask{}
		);
		Assert(pStageMask.IsValid());

		[[maybe_unused]] const Optional<Data::RenderItem::TransformChangeTracker*> pTransformChangeTracker =
			CreateDataComponent<Data::RenderItem::TransformChangeTracker>(transformChangeSceneData);
		Assert(pTransformChangeTracker.IsValid());

		[[maybe_unused]] const Optional<Data::RenderItem::Identifier*> pIdentifier =
			CreateDataComponent<Data::RenderItem::Identifier>(identifierSceneData, scene.AcquireNewRenderItemIdentifier());
		Assert(pIdentifier.IsValid());

		Optional<Data::Tags*> pTagComponent = FindDataComponentOfType<Data::Tags>(tagsSceneData);
		if (pTagComponent.IsInvalid())
		{
			pTagComponent = CreateDataComponent<Data::Tags>(
				tagsSceneData,
				Data::Tags::Initializer{Entity::Data::Component3D::DynamicInitializer{*this, deserializer.GetSceneRegistry()}}
			);
		}

		const Tag::Identifier renderItemTagIdentifier =
			System::Get<Tag::Registry>().FindOrRegister("{13CA71B8-D868-4F7C-A0BB-C7585DB11327}"_asset, Tag::Flags::Transient);

		Assert(pTagComponent.IsValid());
		pTagComponent->SetTag(GetIdentifier(), deserializer.GetSceneRegistry(), renderItemTagIdentifier);
	}

	RenderItemComponent::RenderItemComponent(Initializer&& initializer)
		: Component3D(Forward<Initializer>(initializer))
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		ComponentTypeSceneData<Data::RenderItem::Identifier>& identifierSceneData =
			sceneRegistry.GetCachedSceneData<Data::RenderItem::Identifier>();
		ComponentTypeSceneData<Data::Tags>& tagsSceneData = sceneRegistry.GetCachedSceneData<Data::Tags>();
		ComponentTypeSceneData<Data::RenderItem::TransformChangeTracker>& transformChangeSceneData =
			sceneRegistry.GetCachedSceneData<Data::RenderItem::TransformChangeTracker>();

		[[maybe_unused]] const Optional<Data::RenderItem::StageMask*> pStageMask =
			CreateDataComponent<Data::RenderItem::StageMask>(stageMaskSceneData, initializer.m_stageMask);
		Assert(pStageMask.IsValid());

		[[maybe_unused]] const Optional<Data::RenderItem::TransformChangeTracker*> pTransformChangeTracker =
			CreateDataComponent<Data::RenderItem::TransformChangeTracker>(transformChangeSceneData);
		Assert(pTransformChangeTracker.IsValid());

		[[maybe_unused]] const Optional<Data::RenderItem::Identifier*> pIdentifier =
			CreateDataComponent<Data::RenderItem::Identifier>(identifierSceneData, scene.AcquireNewRenderItemIdentifier());
		Assert(pIdentifier.IsValid());

		Optional<Data::Tags*> pTagComponent = FindDataComponentOfType<Data::Tags>(tagsSceneData);
		if (pTagComponent.IsInvalid())
		{
			pTagComponent = CreateDataComponent<Data::Tags>(
				tagsSceneData,
				Data::Tags::Initializer{Entity::Data::Component3D::DynamicInitializer{*this, initializer.GetSceneRegistry()}}
			);
		}

		const Tag::Identifier renderItemTagIdentifier =
			System::Get<Tag::Registry>().FindOrRegister("{13CA71B8-D868-4F7C-A0BB-C7585DB11327}"_asset, Tag::Flags::Transient);

		Assert(pTagComponent.IsValid());
		pTagComponent->SetTag(GetIdentifier(), initializer.GetSceneRegistry(), renderItemTagIdentifier);
	}

	RenderItemComponent::~RenderItemComponent()
	{
	}

	namespace Data::RenderItem
	{
		void Identifier::OnDestroying(Entity::HierarchyComponentBase& owner)
		{
			if (owner.Is3D())
			{
				owner.AsExpected<Entity::RenderItemComponent>().GetRootScene().GetRenderData().OnRenderItemRemoved(m_identifier);
			}
		}
	}

	void RenderItemComponent::OnCreated()
	{
		GetRootScene().GetRenderData().OnRenderItemAdded(*this);
	}

	void RenderItemComponent::OnEnable()
	{
		GetRootScene().GetRenderData().OnRenderItemEnabled(*this);
	}

	void RenderItemComponent::OnDisable()
	{
		GetRootScene().GetRenderData().OnRenderItemDisabled(GetRenderItemIdentifier());
	}

	void RenderItemComponent::OnWorldTransformChanged(const EnumFlags<Entity::TransformChangeFlags>)
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::TransformChangeTracker>& transformChangeSceneData =
			sceneRegistry.GetCachedSceneData<Data::RenderItem::TransformChangeTracker>();
		Entity::Data::RenderItem::TransformChangeTracker& __restrict transformChangeTracker =
			transformChangeSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		transformChangeTracker.OnTransformChanged();
		scene.GetRenderData().OnRenderItemTransformChanged(*this);
	}

	PURE_STATICS RenderItemIdentifier RenderItemComponent::GetRenderItemIdentifier() const
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::Identifier>& identifierSceneData =
			sceneRegistry.GetCachedSceneData<Data::RenderItem::Identifier>();
		return identifierSceneData.GetComponentImplementationUnchecked(GetIdentifier());
	}

	PURE_STATICS const Rendering::RenderItemStageMask& RenderItemComponent::GetStageMask() const
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		const Rendering::AtomicRenderItemStageMask& __restrict renderItemStageMask =
			stageMaskSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		return reinterpret_cast<const Rendering::RenderItemStageMask&>(renderItemStageMask);
	}

	void RenderItemComponent::EnableStages(const Rendering::RenderItemStageMask& newMask)
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		Data::RenderItem::StageMask& stageMask = stageMaskSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		stageMask.EnableStages(*this, newMask);
	}

	void RenderItemComponent::DisableStages(const Rendering::RenderItemStageMask& newMask)
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		Data::RenderItem::StageMask& stageMask = stageMaskSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		stageMask.DisableStages(*this, newMask);
	}

	void RenderItemComponent::EnableStage(const Rendering::SceneRenderStageIdentifier stageIdentifier)
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		Data::RenderItem::StageMask& stageMask = stageMaskSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		stageMask.EnableStage(*this, stageIdentifier);
	}

	void RenderItemComponent::DisableStage(const Rendering::SceneRenderStageIdentifier stageIdentifier)
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		Data::RenderItem::StageMask& stageMask = stageMaskSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		stageMask.DisableStage(*this, stageIdentifier);
	}

	void RenderItemComponent::ResetStage(const Rendering::SceneRenderStageIdentifier stageIdentifier)
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		Data::RenderItem::StageMask& stageMask = stageMaskSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		stageMask.ResetStage(*this, stageIdentifier);
	}

	void RenderItemComponent::ResetStages(const Rendering::RenderItemStageMask& resetStages)
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		Data::RenderItem::StageMask& stageMask = stageMaskSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		stageMask.ResetStages(*this, resetStages);
	}

	bool RenderItemComponent::IsStageEnabled(const Rendering::SceneRenderStageIdentifier stageIdentifier) const
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		Data::RenderItem::StageMask& stageMask = stageMaskSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		return stageMask.IsStageEnabled(stageIdentifier);
	}

	Rendering::RenderItemStagesProperty RenderItemComponent::GetRenderStages() const
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		const Data::RenderItem::StageMask& stageMask = stageMaskSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		return stageMask.GetFromProperty(const_cast<RenderItemComponent&>(*this));
	}

	void RenderItemComponent::SetRenderStages(Rendering::RenderItemStagesProperty stages)
	{
		Scene& scene = GetRootScene();
		SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();
		ComponentTypeSceneData<Data::RenderItem::StageMask>& stageMaskSceneData = sceneRegistry.GetCachedSceneData<Data::RenderItem::StageMask>(
		);
		Data::RenderItem::StageMask& stageMask = stageMaskSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		stageMask.SetFromProperty(*this, stages);
	}
}

namespace ngine::Entity::Data::RenderItem
{
	void StageMask::EnableStages(HierarchyComponentBase& owner, const Rendering::RenderItemStageMask& newMask)
	{
		const Rendering::RenderItemStageMask previousMask = (m_stageMask |= newMask);
		const Rendering::RenderItemStageMask setBits = (previousMask ^ newMask) & newMask;
		if (setBits.AreAnySet())
		{
			if (owner.Is3D())
			{
				Entity::Component3D& owner3D = static_cast<Entity::Component3D&>(owner);
				Scene3D& scene = owner3D.GetRootScene();
				Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

				ComponentTypeSceneData<Data::RenderItem::Identifier>& renderItemIdentifierSceneData =
					sceneRegistry.GetCachedSceneData<Data::RenderItem::Identifier>();
				const Entity::RenderItemIdentifier renderItemIdentifier =
					renderItemIdentifierSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier());

				scene.GetRenderData().OnRenderItemStageMaskEnabled(renderItemIdentifier, setBits);
			}
		}
	}

	void StageMask::DisableStages(HierarchyComponentBase& owner, const Rendering::RenderItemStageMask& newMask)
	{
		const Rendering::RenderItemStageMask previousMask = (m_stageMask &= ~newMask);
		const Rendering::RenderItemStageMask clearedBits = (previousMask ^ newMask) & newMask;
		if (clearedBits.AreAnySet())
		{
			if (owner.Is3D())
			{
				Entity::Component3D& owner3D = static_cast<Entity::Component3D&>(owner);
				Scene3D& scene = owner3D.GetRootScene();
				Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

				ComponentTypeSceneData<Data::RenderItem::Identifier>& renderItemIdentifierSceneData =
					sceneRegistry.GetCachedSceneData<Data::RenderItem::Identifier>();
				const Entity::RenderItemIdentifier renderItemIdentifier =
					renderItemIdentifierSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier());

				scene.GetRenderData().OnRenderItemStageMaskDisabled(renderItemIdentifier, clearedBits);
			}
		}
	}

	void StageMask::EnableStage(HierarchyComponentBase& owner, const Rendering::SceneRenderStageIdentifier stageIdentifier)
	{
		Assert(stageIdentifier.IsValid());
		if (m_stageMask.Set(stageIdentifier))
		{
			if (owner.Is3D())
			{
				Entity::Component3D& owner3D = static_cast<Entity::Component3D&>(owner);
				Scene3D& scene = owner3D.GetRootScene();
				Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

				ComponentTypeSceneData<Data::RenderItem::Identifier>& renderItemIdentifierSceneData =
					sceneRegistry.GetCachedSceneData<Data::RenderItem::Identifier>();
				const Entity::RenderItemIdentifier renderItemIdentifier =
					renderItemIdentifierSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier());

				scene.GetRenderData().OnRenderItemStageEnabled(renderItemIdentifier, stageIdentifier);
			}
		}
	}

	void StageMask::DisableStage(HierarchyComponentBase& owner, const Rendering::SceneRenderStageIdentifier stageIdentifier)
	{
		Assert(stageIdentifier.IsValid());
		if (m_stageMask.Clear(stageIdentifier))
		{
			if (owner.Is3D())
			{
				Entity::Component3D& owner3D = static_cast<Entity::Component3D&>(owner);
				Scene3D& scene = owner3D.GetRootScene();
				Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

				ComponentTypeSceneData<Data::RenderItem::Identifier>& renderItemIdentifierSceneData =
					sceneRegistry.GetCachedSceneData<Data::RenderItem::Identifier>();
				const Entity::RenderItemIdentifier renderItemIdentifier =
					renderItemIdentifierSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier());

				scene.GetRenderData().OnRenderItemStageDisabled(renderItemIdentifier, stageIdentifier);
			}
		}
	}

	void StageMask::ResetStage(HierarchyComponentBase& owner, const Rendering::SceneRenderStageIdentifier stageIdentifier)
	{
		if (owner.Is3D())
		{
			Entity::Component3D& owner3D = static_cast<Entity::Component3D&>(owner);
			Scene3D& scene = owner3D.GetRootScene();
			Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

			ComponentTypeSceneData<Data::RenderItem::Identifier>& renderItemIdentifierSceneData =
				sceneRegistry.GetCachedSceneData<Data::RenderItem::Identifier>();
			const Entity::RenderItemIdentifier renderItemIdentifier =
				renderItemIdentifierSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier());

			Assert(stageIdentifier.IsValid());
			Assert(m_stageMask.IsSet(stageIdentifier), "Attempting to reset inactive stage");
			scene.GetRenderData().OnRenderItemStageReset(renderItemIdentifier, stageIdentifier);
		}
	}

	void StageMask::ResetStages(HierarchyComponentBase& owner, const Rendering::RenderItemStageMask& resetStages)
	{
		const Rendering::RenderItemStageMask resetBits = m_stageMask & resetStages;
		if (resetBits.AreAnySet())
		{
			if (owner.Is3D())
			{
				Entity::Component3D& owner3D = static_cast<Entity::Component3D&>(owner);
				Scene3D& scene = owner3D.GetRootScene();
				Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

				ComponentTypeSceneData<Data::RenderItem::Identifier>& renderItemIdentifierSceneData =
					sceneRegistry.GetCachedSceneData<Data::RenderItem::Identifier>();
				const Entity::RenderItemIdentifier renderItemIdentifier =
					renderItemIdentifierSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier());

				scene.GetRenderData().OnRenderItemStageMaskReset(renderItemIdentifier, resetBits);
			}
		}
	}

	void StageMask::ResetStages(HierarchyComponentBase& owner)
	{
		if (m_stageMask.AreAnySet())
		{
			if (owner.Is3D())
			{
				Entity::Component3D& owner3D = static_cast<Entity::Component3D&>(owner);
				Scene3D& scene = owner3D.GetRootScene();
				Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

				ComponentTypeSceneData<Data::RenderItem::Identifier>& renderItemIdentifierSceneData =
					sceneRegistry.GetCachedSceneData<Data::RenderItem::Identifier>();
				const Entity::RenderItemIdentifier renderItemIdentifier =
					renderItemIdentifierSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier());

				const IdentifierMask<Rendering::SceneRenderStageIdentifier> stageMask = m_stageMask;
				scene.GetRenderData().OnRenderItemStageMaskReset(renderItemIdentifier, stageMask);
			}
		}
	}

	Rendering::RenderItemStagesProperty StageMask::GetFromProperty(Entity::HierarchyComponentBase&) const
	{
		return {reinterpret_cast<const Rendering::RenderItemStageMask&>(m_stageMask), System::Get<Rendering::Renderer>().GetStageCache()};
	}

	void StageMask::SetFromProperty(Entity::HierarchyComponentBase& owner, Rendering::RenderItemStagesProperty stages)
	{
		const Rendering::RenderItemStageMask previousMask = (m_stageMask = stages.m_mask);
		const Rendering::RenderItemStageMask setBits = (previousMask ^ stages.m_mask) & stages.m_mask;
		const Rendering::RenderItemStageMask clearedBits = previousMask & ~stages.m_mask;

		if (owner.GetFlags().IsSet(ComponentFlags::Is3D))
		{
			Scene& scene = static_cast<Entity::Component3D&>(owner).GetRootScene();
			Rendering::SceneData& sceneData = scene.GetRenderData();
			Entity::SceneRegistry& sceneRegistry = scene.GetEntitySceneRegistry();

			ComponentTypeSceneData<Data::RenderItem::Identifier>& renderItemIdentifierSceneData =
				sceneRegistry.GetCachedSceneData<Data::RenderItem::Identifier>();
			const Entity::RenderItemIdentifier renderItemIdentifier =
				renderItemIdentifierSceneData.GetComponentImplementationUnchecked(owner.GetIdentifier());

			if (setBits.AreAnySet())
			{
				sceneData.OnRenderItemStageMaskEnabled(renderItemIdentifier, setBits);
			}

			if (clearedBits.AreAnySet())
			{
				sceneData.OnRenderItemStageMaskDisabled(renderItemIdentifier, clearedBits);
			}
		}
	}

	[[maybe_unused]] const bool wasStageMaskTypeRegistered = Reflection::Registry::RegisterType<StageMask>();
	[[maybe_unused]] const bool wasStageMaskRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<StageMask>>::Make());

	[[maybe_unused]] const bool wasTransformChangeTypeRegistered = Reflection::Registry::RegisterType<TransformChangeTracker>();
	[[maybe_unused]] const bool wasTransformChangeRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<TransformChangeTracker>>::Make());

	[[maybe_unused]] const bool wasIdentifierTypeRegistered = Reflection::Registry::RegisterType<Identifier>();
	[[maybe_unused]] const bool wasIdentifierRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<Identifier>>::Make());
}
