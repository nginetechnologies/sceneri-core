#include "Entity/HierarchyComponentBase.h"
#include "Entity/Data/ParentComponent.h"
#include "Entity/Data/Component.inl"
#include "Entity/Data/TypeIndex.h"
#include "Entity/Data/Flags.h"
#include "Entity/Data/InstanceGuid.h"
#include "Entity/Data/PreferredIndex.h"
#include "Entity/Data/ExternalScene.h"
#include "Entity/Data/Tags.h"
#include "Entity/Data/RenderItem/Identifier.h"
#include "Entity/Data/RenderItem/StaticMeshIdentifier.h"
#include "Entity/HierarchyComponent.inl"
#include "Entity/ComponentTypeSceneData.h"
#include "Entity/RootSceneComponent.h"
#include "Entity/Scene/SceneComponent.h"
#include "Entity/Serialization/ComponentReference.h"

#include <Common/Reflection/Registry.inl>
#include <Common/Memory/Serialization/ReferenceWrapper.h>
#include <Common/Threading/Jobs/JobRunnerThread.inl>

namespace ngine::Entity
{
	HierarchyComponentBase::HierarchyComponentBase(
		const ComponentIdentifier identifier,
		const Optional<HierarchyComponentBase*> pParent,
		Entity::SceneRegistry& sceneRegistry,
		EnumFlags<Flags> flags,
		const Guid instanceGuid
	)
		: DataComponentOwner(identifier)
		, m_pParent(pParent)
	{
		if (LIKELY(pParent != nullptr))
		{
			ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

			[[maybe_unused]] const Optional<Data::Parent*> pParentData =
				CreateDataComponent<Data::Parent>(parentSceneData, pParent->GetIdentifier().GetFirstValidIndex());
			Assert(pParentData.IsValid());
		}

		{
			ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();

			const EnumFlags<Flags> parentFlags = pParent.IsValid()
			                                       ? (EnumFlags<Flags>)flagsSceneData.GetComponentImplementationUnchecked(pParent->GetIdentifier()
			                                         )
			                                       : EnumFlags<Flags>{};

			flags |= Flags::IsConstructing |
			         Flags::WasDisabledByParent * parentFlags.AreAnySet(Flags::IsDisabledWithChildren | Flags::WasDisabledByParent) |
			         Flags::WasDetachedFromOctreeByParent *
			           parentFlags.AreAnySet(Flags::IsDetachedFromTree | Flags::WasDetachedFromOctreeByParent);

			// here
			[[maybe_unused]] const Optional<Data::Flags*> pFlags = CreateDataComponent<Data::Flags>(flagsSceneData, flags);
			Assert(pFlags.IsValid());
		}

		{
			// here
			ComponentTypeSceneData<Data::InstanceGuid>& instanceGuidSceneData = sceneRegistry.GetCachedSceneData<Data::InstanceGuid>();
			[[maybe_unused]] const Optional<Data::InstanceGuid*> pInstanceGuid =
				CreateDataComponent<Data::InstanceGuid>(instanceGuidSceneData, instanceGuid);
			Assert(pInstanceGuid.IsValid());
		}
	}

	HierarchyComponentBase::HierarchyComponentBase(
		const Optional<HierarchyComponentBase*> pParent,
		Entity::SceneRegistry& sceneRegistry,
		const EnumFlags<Flags> flags,
		const Guid instanceGuid
	)
		: HierarchyComponentBase(sceneRegistry.AcquireNewComponentIdentifier(), pParent, sceneRegistry, flags, instanceGuid)
	{
	}

	HierarchyComponentBase::HierarchyComponentBase(Initializer&& initializer)
		: HierarchyComponentBase(
				initializer.GetSceneRegistry().AcquireNewComponentIdentifier(),
				initializer.GetParent(),
				initializer.GetSceneRegistry(),
				initializer.GetFlags(),
				initializer.GetInstanceGuid()
			)
	{
	}

	HierarchyComponentBase::HierarchyComponentBase(const Deserializer& deserializer)
		: HierarchyComponentBase(
				deserializer.GetSceneRegistry().AcquireNewComponentIdentifier(),
				deserializer.GetParent(),
				deserializer.GetSceneRegistry(),
				deserializer.GetFlags() | Flags::SaveToDisk
			)
	{
	}

	[[nodiscard]] EnumFlags<ComponentFlags> GetClonedFlags(
		const Entity::SceneRegistry& templateSceneRegistry,
		Entity::SceneRegistry& sceneRegistry,
		const HierarchyComponentBase& templateComponent,
		const Optional<const HierarchyComponentBase*> pParentComponent
	)
	{
		ComponentTypeSceneData<Entity::Data::Flags>& templateFlagsSceneData = templateSceneRegistry.GetCachedSceneData<Entity::Data::Flags>();
		const EnumFlags<ComponentFlags> templateFlags =
			templateFlagsSceneData.GetComponentImplementationUnchecked(templateComponent.GetIdentifier());
		ComponentTypeSceneData<Entity::Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Entity::Data::Flags>();
		const EnumFlags<ComponentFlags> parentFlags =
			pParentComponent.IsValid()
				? (EnumFlags<ComponentFlags>)flagsSceneData.GetComponentImplementationUnchecked(pParentComponent->GetIdentifier())
				: EnumFlags<ComponentFlags>{};

		return (templateFlags &
		        ~(ComponentFlags::IsDestroying | ComponentFlags::WasDisabledByParent | ComponentFlags::WasDetachedFromOctreeByParent)) |
		       ComponentFlags::IsConstructing |
		       ComponentFlags::WasDisabledByParent * parentFlags.IsSet(ComponentFlags::IsDisabledWithChildren) |
		       ComponentFlags::WasDetachedFromOctreeByParent *
		         parentFlags.AreAnySet(ComponentFlags::IsDetachedFromTree | ComponentFlags::WasDetachedFromOctreeByParent);
	}

	HierarchyComponentBase::HierarchyComponentBase(const HierarchyComponentBase& templateComponent, const Cloner& cloner)
		: HierarchyComponentBase(
				cloner.GetSceneRegistry().AcquireNewComponentIdentifier(),
				cloner.GetParent(),
				cloner.GetSceneRegistry(),
				GetClonedFlags(cloner.GetTemplateSceneRegistry(), cloner.GetSceneRegistry(), templateComponent, cloner.GetParent()),
				cloner.GetInstanceGuid()
			)
	{
		Assert(!templateComponent.GetFlags(cloner.GetTemplateSceneRegistry()).IsSet(Flags::DisableCloning));

		if (cloner.GetPreferredChildIndex().IsValid())
		{
			Entity::ComponentTypeSceneData<Entity::Data::PreferredIndex>& preferredIndexSceneData =
				cloner.GetSceneRegistry().GetCachedSceneData<Entity::Data::PreferredIndex>();
			[[maybe_unused]] const Optional<Entity::Data::PreferredIndex*> pPreferredIndex =
				CreateDataComponent<Entity::Data::PreferredIndex>(preferredIndexSceneData, *cloner.GetPreferredChildIndex());
			Assert(pPreferredIndex.IsValid());
		}

		ComponentTypeSceneData<Data::Tags>& templateTagsSceneData = cloner.GetTemplateSceneRegistry().GetCachedSceneData<Data::Tags>();
		if (Optional<const Data::Tags*> pTemplateTags = templateComponent.FindDataComponentOfType<Data::Tags>(templateTagsSceneData))
		{
			// here
			ComponentTypeSceneData<Data::Tags>& tagsSceneData = cloner.GetSceneRegistry().GetCachedSceneData<Data::Tags>();
			Threading::JobBatch jobBatch;
			Optional<Data::Tags*> pTags = CreateDataComponent<Data::Tags>(
				tagsSceneData,
				*pTemplateTags,
				Data::Tags::Cloner{jobBatch, *this, templateComponent, cloner.GetSceneRegistry(), cloner.GetTemplateSceneRegistry()}
			);
			Assert(pTags.IsValid());
		}
	}

	void HierarchyComponentBase::OnConstructed()
	{
		SceneRegistry& sceneRegistry = GetSceneRegistry();
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();

		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		EnumFlags<Flags> parentFlags;
		const Optional<Entity::HierarchyComponentBase*> pParent = GetParentSafe();
		if (pParent.IsValid())
		{
			ChildIndex index;

			ComponentTypeSceneData<Data::PreferredIndex>& preferredIndexSceneData = sceneRegistry.GetCachedSceneData<Data::PreferredIndex>();
			if (const Optional<Data::PreferredIndex*> pPreferredIndex = preferredIndexSceneData.GetComponentImplementation(GetIdentifier()))
			{
				index = pPreferredIndex->Get();
			}
			else
			{
				index = pParent->GetNextAvailableChildIndex();
			}

			pParent->AttachChild(*this, index, sceneRegistry);
			parentFlags = flagsSceneData.GetComponentImplementationUnchecked(pParent->GetIdentifier());
		}

		const EnumFlags<Flags> newFlags =
			(Flags::WasDetachedFromOctreeByParent * parentFlags.AreAnySet(Flags::IsDetachedFromTree | Flags::WasDetachedFromOctreeByParent)) |
			(Flags::WasDisabledByParent * parentFlags.AreAnySet(Flags::IsDisabledWithChildren | Flags::WasDisabledByParent));
		flags |= newFlags;

		if (!IsRootSceneComponent(sceneRegistry))
		{
			const bool isDetachedFromOctree = flags.AreAnySet(Flags::IsDetachedFromTree | Flags::WasDetachedFromOctreeByParent);
			if (!isDetachedFromOctree && pParent)
			{
				Optional<ComponentTypeSceneDataInterface*> pSceneData = GetTypeSceneData();
				pSceneData->AttachInstanceToTree(*this, pParent);
			}
		}

		flags &= ~Flags::IsConstructing;
	}

	bool HierarchyComponentBase::Destroy(SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		const Optional<Data::Flags*> pFlags = flagsSceneData.GetComponentImplementation(GetIdentifier());
		if (LIKELY(pFlags.IsValid()))
		{
			AtomicEnumFlags<Flags>& flags = *pFlags;

			if (flags.TrySetFlags(Flags::IsDestroying))
			{
				{
					const EnumFlags<ComponentFlags> previousFlags = flags.FetchOr(ComponentFlags::IsDisabledWithChildren);
					if (!previousFlags.AreAnySet(Flags::IsDisabledFromAnySource))
					{
						DisableInternal(sceneRegistry);
					}
				}

				{
					const EnumFlags<ComponentFlags> previousFlags = flags.FetchOr(ComponentFlags::IsDetachedFromTree);

					if (!previousFlags.AreAnySet(Flags::IsDetachedFromTreeFromAnySource))
					{
						Optional<ComponentTypeSceneDataInterface*> pSceneData = GetTypeSceneData();
						pSceneData->DetachInstanceFromTree(*this, GetParentSafe());
					}

					if (const Optional<HierarchyComponentBase*> pParent = GetParentSafe(); pParent.IsValid())
					{
						if (previousFlags.IsNotSet(ComponentFlags::IsDetachedFromTree))
						{
							OnBeforeDetachFromParent();
							pParent->RemoveChildAndClearParent(*this, sceneRegistry);
							pParent->OnChildDetached(*this);
						}
						Assert(!pParent->GetChildren().Contains(*this));
					}
				}

				while (HasChildren())
				{
					HierarchyComponentBase& child = GetChildren()[0];
					child.Destroy(sceneRegistry);
				}

				DestroyInternal(sceneRegistry);

				// Component is invalid at this point
				return true;
			}
			else
			{
				return false;
			}
		}
		else
		{
			return false;
		}
	}

	void HierarchyComponentBase::DestroyInternal(SceneRegistry& sceneRegistry)
	{
		if constexpr (ENABLE_ASSERTS)
		{
			[[maybe_unused]] const EnumFlags<Entity::ComponentFlags> flags = GetFlags(sceneRegistry);
			Assert(!flags.IsSet(Flags::IsConstructing));
			Assert(flags.IsSet(Entity::ComponentFlags::IsRootScene) || flags.AreAnySet(ComponentFlags::IsDetachedFromTreeFromAnySource));
			Assert(flags.AreAnySet(Entity::ComponentFlags::IsDisabledFromAnySource));
			Assert(GetChildren().IsEmpty());
		}

		Optional<ComponentTypeSceneDataInterface*> pSceneData = GetTypeSceneData(sceneRegistry);
		Assert(pSceneData.IsValid());
		if (LIKELY(pSceneData.IsValid()))
		{
			Optional<HierarchyComponentBase*> pParent = GetParentSafe();
			pSceneData->OnBeforeRemoveInstance(*this, pParent);
			pSceneData->RemoveInstance(*this, pParent);
		}

		// Component is invalid at this point
	}

	Guid HierarchyComponentBase::GetInstanceGuid() const
	{
		return GetInstanceGuid(GetSceneRegistry());
	}
	Guid HierarchyComponentBase::GetInstanceGuid(const Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::InstanceGuid>& instanceGuidSceneData = sceneRegistry.GetCachedSceneData<Data::InstanceGuid>();
		const Optional<Data::InstanceGuid*> pInstanceGuid = instanceGuidSceneData.GetComponentImplementation(GetIdentifier());
		return pInstanceGuid.IsValid() ? Guid(*pInstanceGuid) : Guid();
	}

	ComponentIdentifier HierarchyComponentBase::GetParentIdentifier(const SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();
		if (Optional<Data::Parent*> pParentData = parentSceneData.GetComponentImplementation(GetIdentifier()))
		{
			return ComponentIdentifier::MakeFromValidIndex(pParentData->Get());
		}
		else
		{
			return {};
		}
	}

	void HierarchyComponentBase::SetParent(HierarchyComponentBase& parent, const Entity::SceneRegistry& sceneRegistry)
	{
		if constexpr (DEBUG_BUILD)
		{
			for (HierarchyComponentBase* pParent2 = &parent; pParent2 != nullptr; pParent2 = pParent2->GetParentSafe())
			{
				Assert(pParent2 != this, "Attempting to create a hierarchy loop!");
			}
		}

		Assert(!IsParentOf(parent));
		Assert(&parent != this);
		m_pParent = &parent;

		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();
		if (Optional<Data::Parent*> pParentData = parentSceneData.GetComponentImplementation(GetIdentifier()))
		{
			*pParentData = parent.GetIdentifier().GetFirstValidIndex();
		}
		else
		{
			pParentData = CreateDataComponent<Data::Parent>(parentSceneData, parent.GetIdentifier().GetFirstValidIndex());
			Assert(pParentData.IsValid());
		}
	}

	void HierarchyComponentBase::ClearParent(const Entity::SceneRegistry& sceneRegistry)
	{
		Assert(m_pParent != nullptr);
		if (LIKELY(m_pParent != nullptr))
		{
			m_pParent = nullptr;
			ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();
			RemoveDataComponentOfType(parentSceneData);
		}
	}

	void
	HierarchyComponentBase::AttachChild(HierarchyComponentBase& newChildComponent, ChildIndex index, Entity::SceneRegistry& sceneRegistry)
	{
		newChildComponent.SetParent(*this, sceneRegistry);

		const ChildIndex preferredChildIndex = index;
		index = Math::Min(index, m_children.GetNextAvailableIndex());

		const EnumFlags<Flags> childFlags = newChildComponent.GetFlags(sceneRegistry);

		if (!childFlags.IsSet(ComponentFlags::IsDetachedFromTree))
		{
			AddChild(newChildComponent, index, sceneRegistry);
		}

		OnChildAttached(newChildComponent, index, preferredChildIndex);

		if (childFlags.IsNotSet(ComponentFlags::IsConstructing))
		{
			newChildComponent.OnAttachedToNewParent();
		}
	}

	void HierarchyComponentBase::AttachTo(HierarchyComponentBase& newParent, Entity::SceneRegistry& sceneRegistry)
	{
		newParent.AttachChild(*this, newParent.GetNextAvailableChildIndex(), sceneRegistry);
	}

	void HierarchyComponentBase::AttachToNewParent(HierarchyComponentBase& newParent, Entity::SceneRegistry& sceneRegistry)
	{
		AttachToNewParent(newParent, newParent.GetNextAvailableChildIndex(), sceneRegistry);
	}

	void
	HierarchyComponentBase::AttachToNewParent(HierarchyComponentBase& newParent, const ChildIndex index, Entity::SceneRegistry& sceneRegistry)
	{
		HierarchyComponentBase& previousParent = GetParent();

		OnBeforeDetachFromParent();
		previousParent.RemoveChildAndClearParent(*this, sceneRegistry);
		previousParent.OnChildDetached(*this);

		newParent.AttachChild(*this, index, sceneRegistry);
	}

	HierarchyComponentBase::ChildIndex HierarchyComponentBase::GetParentChildIndex() const
	{
		if (const HierarchyComponentBase* pParent = GetParentSafe())
		{
			const ChildView parentChildView = pParent->GetChildren();
			if (const auto it = parentChildView.Find(*this); it != parentChildView.end())
			{
				return parentChildView.GetIteratorIndex(it);
			}
			else
			{
				return 0;
			}
		}
		else
		{
			return 0;
		}
	}

	PURE_STATICS ComponentTypeIdentifier HierarchyComponentBase::GetTypeIdentifier(const SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::TypeIndex>& typeIndexSceneData = sceneRegistry.GetCachedSceneData<Data::TypeIndex>();
		return typeIndexSceneData.GetComponentImplementationUnchecked(GetIdentifier());
	}

	PURE_STATICS Guid HierarchyComponentBase::GetTypeGuid(const SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::TypeIndex>& typeIndexSceneData = sceneRegistry.GetCachedSceneData<Data::TypeIndex>();
		const ComponentTypeIdentifier componentTypeIdentifier = typeIndexSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		const Entity::Manager& entityManager = System::Get<Entity::Manager>();
		return entityManager.GetRegistry().GetGuid(componentTypeIdentifier);
	}

	PURE_STATICS Optional<ComponentTypeSceneDataInterface*> HierarchyComponentBase::GetTypeSceneData(const SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::TypeIndex>& typeIndexSceneData = sceneRegistry.GetCachedSceneData<Data::TypeIndex>();
		const ComponentTypeIdentifier componentTypeIdentifier = typeIndexSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		return sceneRegistry.FindComponentTypeData(componentTypeIdentifier);
	}

	PURE_STATICS Optional<ComponentTypeInterface*> HierarchyComponentBase::GetTypeInfo(const SceneRegistry& sceneRegistry) const
	{
		return System::Get<Entity::Manager>().GetRegistry().Get(GetTypeIdentifier(sceneRegistry));
	}

	PURE_STATICS Optional<const Reflection::TypeInterface*> HierarchyComponentBase::GetTypeInterface(const SceneRegistry& sceneRegistry) const
	{
		return System::Get<Reflection::Registry>().FindTypeInterface(GetTypeGuid(sceneRegistry));
	}

	PURE_STATICS bool HierarchyComponentBase::Is(const Guid otherTypeGuid, const SceneRegistry& sceneRegistry) const
	{
		return GetTypeGuid(sceneRegistry) == otherTypeGuid;
	}

	PURE_STATICS bool HierarchyComponentBase::Implements(const Guid otherTypeGuid, const SceneRegistry& sceneRegistry) const
	{
		if (Optional<const Reflection::TypeInterface*> typeInterface = GetTypeInterface(sceneRegistry))
		{
			return typeInterface->Implements(otherTypeGuid);
		}
		return false;
	}

	PURE_STATICS bool HierarchyComponentBase::IsOrImplements(const Guid otherTypeGuid, const SceneRegistry& sceneRegistry) const
	{
		const Guid typeGuid = GetTypeGuid(sceneRegistry);
		if (typeGuid == otherTypeGuid)
		{
			return true;
		}

		if (Optional<const Reflection::TypeInterface*> typeInterface = System::Get<Reflection::Registry>().FindTypeInterface(typeGuid))
		{
			return typeInterface->Implements(otherTypeGuid);
		}
		return false;
	}

	bool HierarchyComponentBase::IsParentOf(const ChildType& child) const
	{
		return this == child.GetParentSafe();
	}

	bool HierarchyComponentBase::IsParentOfRecursive(const ChildType& child) const
	{
		const HierarchyComponentBase* pChildParent = child.GetParentSafe();
		while (pChildParent != nullptr)
		{
			if (pChildParent == this)
			{
				return true;
			}
			pChildParent = pChildParent->GetParentSafe();
		}

		return false;
	}

	bool HierarchyComponentBase::IsChildOf(const ParentType& parent) const
	{
		return m_pParent == &parent;
	}

	bool HierarchyComponentBase::IsChildOfRecursive(const ParentType& parent) const
	{
		return parent.IsParentOfRecursive(*this);
	}

	void HierarchyComponentBase::RotateChildren(ChildIndex n)
	{
		Threading::UniqueLock lock(m_childMutex);

		auto reverse = [](typename ChildContainer::iterator first, typename ChildContainer::iterator last)
		{
			while (first != last && first != --last)
			{
				auto tmp(Move(*first));
				*first = Move(*last);
				*last = Move(tmp);
				first++;
			}
		};

		n %= m_children.GetSize();
		reverse(m_children.begin(), m_children.end());
		reverse(m_children.begin(), m_children.begin() + n);
		reverse(m_children.begin() + n, m_children.end());
	}

	bool HierarchyComponentBase::ShouldSerialize(Serialization::Writer serializer) const
	{
		if ((!ShouldSaveToDisk()) & (serializer.GetData().GetContextFlags().IsSet(Serialization::ContextFlags::ToDisk)))
		{
			return false;
		}

		return true;
	}

	bool HierarchyComponentBase::SerializeDataComponents(Serialization::Writer serializer) const
	{
		SceneRegistry& sceneRegistry = GetSceneRegistry();
		const Entity::ComponentIdentifier identifier = GetIdentifier();
		const SceneRegistry::DataComponentsBitIndexType dataComponentCount = sceneRegistry.GetDataComponentCount(identifier);
		if (dataComponentCount > 0)
		{
			Serialization::Array dataComponents(Memory::Reserve, dataComponentCount, serializer.GetDocument());

			const SceneRegistry::DataComponentIterator dataComponentIterator = sceneRegistry.GetDataComponentIterator(identifier);
			for (const SceneRegistry::DataComponentsBitIndexType dataComponentIndex : dataComponentIterator)
			{
				const Optional<ComponentTypeSceneDataInterface*> pComponentTypeSceneData =
					sceneRegistry.FindComponentTypeData(ComponentTypeIdentifier::MakeFromValidIndex(dataComponentIndex));
				Assert(pComponentTypeSceneData.IsValid());
				if (LIKELY(pComponentTypeSceneData.IsValid()))
				{
					const Optional<Component*> pComponent = pComponentTypeSceneData->GetDataComponent(identifier);
					if (LIKELY(pComponent.IsValid()))
					{
						Entity::ComponentTypeInterface& typeInterface = pComponentTypeSceneData->GetTypeInterface();

						Serialization::Object serializedValue;
						Serialization::Writer dataComponentWriter(serializedValue, serializer.GetData());

						ComponentValue<Data::Component> dataComponent = static_cast<Data::Component&>(*pComponent);
						if (dataComponentWriter.SerializeInPlace<
									ComponentValue<Data::Component>,
									const ComponentTypeInterface&,
									const Optional<const HierarchyComponentBase*>>(
									dataComponent,
									typeInterface,
									Optional<const HierarchyComponentBase*>(this)
								))
						{
							dataComponents.PushBack(Move(serializedValue), serializer.GetDocument());
						}
					}
				}
			}

			if (dataComponents.HasElements())
			{
				Serialization::Value& dataComponentsValue = dataComponents;
				serializer.GetAsObject().RemoveMember("data_components");
				serializer.AddMember("data_components", Move(dataComponentsValue));
				return true;
			}
		}
		return false;
	}

	Threading::JobBatch HierarchyComponentBase::DeserializeDataComponentsAndChildren(const Serialization::Reader serializer)
	{
		Threading::JobBatch batch;
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();

		if (const Optional<Serialization::Reader> dataComponentsReader = serializer.FindSerializer("data_components"))
		{
			Threading::JobBatch dataComponentBatch;
			for (const Serialization::Reader dataComponentReader : dataComponentsReader->GetArrayView())
			{
				[[maybe_unused]] Optional<ComponentValue<Data::Component>> componentValue =
					dataComponentReader.ReadInPlace<ComponentValue<Data::Component>>(*this, sceneRegistry, dataComponentBatch);
			}
			batch.QueueAfterStartStage(dataComponentBatch);
		}
		if (const Optional<Serialization::Reader> childrenReader = serializer.FindSerializer("children"))
		{
			ReserveAdditionalChildren(static_cast<ChildIndex>(childrenReader->GetArraySize()));

			for (const Serialization::Reader childReader : childrenReader->GetArrayView())
			{
				// Start by attempting to find the existing child instance
				if (const Optional<ComponentSoftReference> softComponentReference = childReader.ReadInPlace<ComponentSoftReference>(sceneRegistry))
				{
					if (const Optional<HierarchyComponentBase*> pChildComponent = softComponentReference->Find<HierarchyComponentBase>(sceneRegistry))
					{
						Threading::JobBatch childComponentBatch = pChildComponent->DeserializeDataComponentsAndChildren(childReader);
						batch.QueueAfterStartStage(childComponentBatch);
						continue;
					}
				}

				// Spawn a new instance from the read data
				Threading::JobBatch childComponentBatch;
				[[maybe_unused]] const bool read =
					childReader.ReadInPlace<ComponentValue<HierarchyComponentBase>>(*this, sceneRegistry, childComponentBatch).IsValid();
				batch.QueueAfterStartStage(childComponentBatch);
			}
		}

		return batch;
	}

	bool HierarchyComponentBase::SerializeChildren(Serialization::Writer serializer) const
	{
		ChildView children = GetChildren();
		const ChildIndex childCount = children.GetSize();
		if (serializer.GetData().GetContextFlags().IsNotSet(Serialization::ContextFlags::UseWithinSessionInstance))
		{
			return serializer.SerializeArrayWithCallback(
				"children",
				[children = Move(children)](Serialization::Writer writer, const ChildIndex index) -> bool
				{
					return writer.SerializeInPlace(children[index]);
				},
				childCount
			);
		}
		else
		{
			return serializer.SerializeArrayWithCallback(
				"children",
				[children = Move(children)](Serialization::Writer writer, const ChildIndex index) -> bool
				{
					ComponentReference<HierarchyComponentBase> reference(children[index]);
					return writer.SerializeInPlace(reference);
				},
				childCount
			);
		}
	}

	bool HierarchyComponentBase::SerializeDataComponentsAndChildren(Serialization::Writer serializer) const
	{
		const bool wroteAnyDataComponents = SerializeDataComponents(serializer);
		const bool wroteAnyChildren = SerializeChildren(serializer);
		return wroteAnyDataComponents | wroteAnyChildren;
	}

	void HierarchyComponentBase::DeserializeCustomData(const Optional<Serialization::Reader> serializer)
	{
		[[maybe_unused]] const EnumFlags<Flags> changedFlags = DeserializeCustomDataInternal(serializer);
	}

	EnumFlags<ComponentFlags> HierarchyComponentBase::DeserializeCustomDataInternal(const Optional<Serialization::Reader> serializer)
	{
		SceneRegistry& sceneRegistry = GetSceneRegistry();

		AtomicEnumFlags<Flags>& flags = *FindDataComponentOfType<Data::Flags>(sceneRegistry.GetCachedSceneData<Data::Flags>());

		EnumFlags<ComponentFlags> newDynamicFlags;
		if (serializer.IsValid())
		{
			newDynamicFlags |= serializer.Get().ReadWithDefaultValue<ComponentFlags>("flags", ComponentFlags());
			newDynamicFlags |= Flags::IsDisabledWithChildren * serializer->ReadWithDefaultValue<bool>("disabled_with_children", false);
			newDynamicFlags |= Flags::IsDisabled * serializer->ReadWithDefaultValue<bool>("disabled", false);
			newDynamicFlags |= Flags::IsDetachedFromTree * serializer->ReadWithDefaultValue<bool>("detached_from_octree", false);
		}

		const EnumFlags<ComponentFlags> changedFlags = newDynamicFlags ^ (flags & Flags::DynamicFlags);
		Assert(!changedFlags.AreAnySet(Flags::IsDestroying), "Can't undo destroyed state!");

		if (changedFlags.IsSet(Flags::IsDisabledWithChildren))
		{
			if (newDynamicFlags.IsSet(Flags::IsDisabledWithChildren))
			{
				DisableWithChildren(sceneRegistry);
			}
			else
			{
				EnableWithChildren(sceneRegistry);
			}
		}

		if (changedFlags.IsSet(Flags::IsDisabled))
		{
			if (newDynamicFlags.IsSet(Flags::IsDisabled))
			{
				Disable(sceneRegistry);
			}
			else
			{
				Enable(sceneRegistry);
			}
		}

		if (changedFlags.IsSet(Flags::IsDetachedFromTree) && HasParent())
		{
			if (newDynamicFlags.IsSet(Flags::IsDetachedFromTree))
			{
				DetachFromOctree(sceneRegistry);
			}
			else
			{
				AttachToOctree(sceneRegistry);
			}
		}

		if (serializer.IsValid())
		{
			const bool isReferencedExternally = serializer->ReadWithDefaultValue<bool>("referenced", false);
			if (isReferencedExternally)
			{
				flags |= ComponentFlags::IsReferenced;
			}
			else
			{
				flags.ClearFlags(ComponentFlags::IsReferenced);
			}
		}

		return changedFlags;
	}

	bool HierarchyComponentBase::SerializeCustomData(Serialization::Writer serializer) const
	{
		bool serializedAny = false;

		const EnumFlags<Flags> flags = GetFlags();
		if (flags.IsSet(Flags::IsDisabledWithChildren))
		{
			serializedAny |= serializer.Serialize("disabled_with_children", true);
		}

		if (flags.IsSet(Flags::IsDisabled))
		{
			serializedAny |= serializer.Serialize("disabled", true);
		}

		if (flags.IsSet(Flags::IsDetachedFromTree))
		{
			serializedAny |= serializer.Serialize("detached_from_octree", true);
		}

		if (flags.IsSet(Flags::IsReferenced))
		{
			serializedAny |= serializer.Serialize("referenced", true);
		}

		return serializedAny;
	}

	bool HierarchyComponentBase::Serialize(Serialization::Writer serializer) const
	{
		const Optional<const ComponentTypeInterface*> pComponentTypeInfo = GetTypeInfo();
		Assert(pComponentTypeInfo.IsValid());
		if (LIKELY(pComponentTypeInfo.IsValid()))
		{
			return pComponentTypeInfo->SerializeInstanceWithChildren(serializer, *this, GetParentSafe());
		}

		return false;
	}

	bool HierarchyComponentBase::Serialize(const Serialization::Reader serializer, Threading::JobBatch& jobBatchOut)
	{
		const Optional<ComponentTypeInterface*> pComponentTypeInfo = GetTypeInfo();
		Assert(pComponentTypeInfo.IsValid());
		if (LIKELY(pComponentTypeInfo.IsValid()))
		{
			jobBatchOut = pComponentTypeInfo->SerializeInstanceWithChildren(serializer, *this, GetParentSafe());
			return true;
		}
		return false;
	}

	bool HierarchyComponentBase::Serialize(const Serialization::Reader serializer)
	{
		Threading::JobBatch jobBatch;
		const bool success = Serialize(serializer, jobBatch);
		if (jobBatch.IsValid())
		{
			if (const Optional<Threading::JobRunnerThread*> pThread = Threading::JobRunnerThread::GetCurrent())
			{
				pThread->Queue(jobBatch);
			}
			else
			{
				System::Get<Threading::JobManager>().Queue(jobBatch, Threading::JobPriority::LoadScene);
			}
		}
		return success;
	}

	EnumFlags<ComponentFlags> HierarchyComponentBase::GetFlags(const SceneRegistry& sceneRegistry) const
	{
		Optional<Data::Flags*> pFlags = FindDataComponentOfType<Data::Flags>(sceneRegistry.GetCachedSceneData<Data::Flags>());
		return pFlags.IsValid() ? EnumFlags<Flags>{*pFlags} : EnumFlags<Flags>{Flags::IsDisabled | Flags::IsDestroying};
	}

	EnumFlags<ComponentFlags> HierarchyComponentBase::GetFlags() const
	{
		SceneRegistry& sceneRegistry = GetSceneRegistry();
		return GetFlags(sceneRegistry);
	}

	HierarchyComponentBase& HierarchyComponentBase::GetRootSceneComponent() const
	{
		return GetRootSceneComponent(GetSceneRegistry());
	}

	HierarchyComponentBase& HierarchyComponentBase::GetRootSceneComponent(Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		ReferenceWrapper<HierarchyComponentBase> component = const_cast<HierarchyComponentBase&>(*this);
		do
		{
			const AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(component->GetIdentifier());
			if (flags.IsSet(Flags::IsRootScene))
			{
				return component;
			}

			const Optional<HierarchyComponentBase*> pParent = component->GetParentSafe();
			if (pParent.IsValid())
			{
				component = *pParent;
			}
			else
			{
				return component;
			}
		} while (true);
		ExpectUnreachable();
	}

	void HierarchyComponentBase::SetIsReferenced(bool isReferenced, Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		if (isReferenced)
		{
			flags |= ComponentFlags::IsReferenced;
		}
		else
		{
			flags.ClearFlags(ComponentFlags::IsReferenced);
		}
	}

	void HierarchyComponentBase::EnableSaveToDisk()
	{
		EnableSaveToDisk(GetSceneRegistry());
	}
	void HierarchyComponentBase::EnableSaveToDisk(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		flags |= ComponentFlags::SaveToDisk;
	}

	void HierarchyComponentBase::DisableSaveToDisk()
	{
		DisableSaveToDisk(GetSceneRegistry());
	}
	void HierarchyComponentBase::DisableSaveToDisk(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		flags &= ~ComponentFlags::SaveToDisk;
	}

	void HierarchyComponentBase::EnableCloning()
	{
		EnableCloning(GetSceneRegistry());
	}
	void HierarchyComponentBase::EnableCloning(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		flags &= ~ComponentFlags::DisableCloning;
	}

	void HierarchyComponentBase::DisableCloning()
	{
		DisableCloning(GetSceneRegistry());
	}
	void HierarchyComponentBase::DisableCloning(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		flags |= ComponentFlags::DisableCloning;
	}

	void HierarchyComponentBase::SetIsMeshScene(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		flags |= ComponentFlags::IsMeshScene;
	}
	void HierarchyComponentBase::ClearIsMeshScene(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		flags &= ~ComponentFlags::IsMeshScene;
	}
	void HierarchyComponentBase::ToggleIsMeshScene(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		flags ^= ComponentFlags::IsMeshScene;
	}

	void HierarchyComponentBase::PauseSimulation(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		const EnumFlags<ComponentFlags> previousFlags = flags.FetchOr(Flags::IsSimulationPaused);
		if (!previousFlags.AreAnySet(Flags::IsSimulationPaused))
		{
			Optional<ComponentTypeSceneDataInterface*> pSceneData = GetTypeSceneData();
			if (LIKELY(pSceneData.IsValid()))
			{
				pSceneData->PauseInstanceSimulation(*this, GetParentSafe());
			}

			// Notify data components
			const ComponentIdentifier identifier = GetIdentifier();

			for (const SceneRegistry::DataComponentsBitIndexType index : sceneRegistry.GetDataComponentIterator(identifier))
			{
				const Optional<ComponentTypeSceneDataInterface*> pComponentTypeInterface =
					sceneRegistry.FindComponentTypeData(ComponentTypeIdentifier::MakeFromValidIndex(index));
				Assert(pComponentTypeInterface.IsValid());
				if (LIKELY(pComponentTypeInterface.IsValid()))
				{
					Optional<Component*> pComponent = pComponentTypeInterface->GetDataComponent(identifier);
					if (LIKELY(pComponent.IsValid()))
					{
						pComponentTypeInterface->PauseInstanceSimulation(*pComponent, this);
					}
				}
			}
		}
	}

	void HierarchyComponentBase::ResumeSimulation(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		const EnumFlags<ComponentFlags> previousFlags = flags.FetchAnd(~Flags::IsSimulationPaused);
		if (previousFlags.AreAnySet(Flags::IsSimulationPaused))
		{
			Optional<ComponentTypeSceneDataInterface*> pSceneData = GetTypeSceneData();
			if (LIKELY(pSceneData.IsValid()))
			{
				pSceneData->ResumeInstanceSimulation(*this, GetParentSafe());
			}

			// Notify data components
			const ComponentIdentifier identifier = GetIdentifier();

			for (const SceneRegistry::DataComponentsBitIndexType index : sceneRegistry.GetDataComponentIterator(identifier))
			{
				const Optional<ComponentTypeSceneDataInterface*> pComponentTypeInterface =
					sceneRegistry.FindComponentTypeData(ComponentTypeIdentifier::MakeFromValidIndex(index));
				Assert(pComponentTypeInterface.IsValid());
				if (LIKELY(pComponentTypeInterface.IsValid()))
				{
					Optional<Component*> pComponent = pComponentTypeInterface->GetDataComponent(identifier);
					if (LIKELY(pComponent.IsValid()))
					{
						pComponentTypeInterface->ResumeInstanceSimulation(*pComponent, this);
					}
				}
			}
		}
	}

	void HierarchyComponentBase::DisableInternal(SceneRegistry& sceneRegistry)
	{
		Assert(GetFlags(sceneRegistry).AreAnySet(ComponentFlags::IsDisabledFromAnySource));
		Optional<ComponentTypeSceneDataInterface*> pSceneData = GetTypeSceneData(sceneRegistry);
		if (LIKELY(pSceneData.IsValid()))
		{
			pSceneData->DisableInstance(*this, GetParentSafe());
		}
	}

	void HierarchyComponentBase::EnableInternal(SceneRegistry& sceneRegistry)
	{
		Assert(GetFlags(sceneRegistry).AreNoneSet(ComponentFlags::IsDisabledFromAnySource));
		Optional<ComponentTypeSceneDataInterface*> pSceneData = GetTypeSceneData(sceneRegistry);
		if (LIKELY(pSceneData.IsValid()))
		{
			pSceneData->EnableInstance(*this, GetParentSafe());
		}
	}

	void HierarchyComponentBase::Disable()
	{
		Disable(GetSceneRegistry());
	}
	void HierarchyComponentBase::Disable(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		const EnumFlags<ComponentFlags> previousFlags = flags.FetchOr(Flags::IsDisabled);
		if (!previousFlags.AreAnySet(Flags::IsDisabledFromAnySource))
		{
			DisableInternal(sceneRegistry);
		}
	}

	void HierarchyComponentBase::DisableWithChildren()
	{
		DisableWithChildren(GetSceneRegistry());
	}
	void HierarchyComponentBase::DisableWithChildren(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		const EnumFlags<ComponentFlags> previousFlags = flags.FetchOr(Flags::IsDisabledWithChildren);
		if (!previousFlags.AreAnySet(Flags::IsDisabledWithChildren))
		{
			DisableInternal(sceneRegistry);

			using DisableChildrenFunction = void (*)(
				HierarchyComponentBase& component,
				Entity::SceneRegistry& sceneRegistry,
				ComponentTypeSceneData<Data::Flags>& flagsSceneData
			);
			static DisableChildrenFunction disableChildren =
				[](HierarchyComponentBase& parent, Entity::SceneRegistry& sceneRegistry, ComponentTypeSceneData<Data::Flags>& flagsSceneData)
			{
				for (Entity::HierarchyComponentBase& component : parent.GetChildren())
				{
					AtomicEnumFlags<Flags>& childFlags = flagsSceneData.GetComponentImplementationUnchecked(component.GetIdentifier());

					const EnumFlags<ComponentFlags> previousChildFlags = childFlags.FetchOr(Flags::WasDisabledByParent);
					if (!previousChildFlags.AreAnySet(Flags::IsDisabledFromAnySource))
					{
						component.DisableInternal(sceneRegistry);
					}

					if (!previousChildFlags.AreAnySet(Flags::WasDisabledByParent | Flags::IsDisabledWithChildren))
					{
						disableChildren(component, sceneRegistry, flagsSceneData);
					}
				}
			};

			disableChildren(*this, sceneRegistry, flagsSceneData);
		}
	}

	void HierarchyComponentBase::Enable()
	{
		Enable(GetSceneRegistry());
	}
	void HierarchyComponentBase::Enable(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		if (flags.IsSet(ComponentFlags::WasDisabledByParent) && GetParent().IsEnabled())
		{
			flags &= ~ComponentFlags::WasDisabledByParent;
		}

		const EnumFlags<ComponentFlags> previousFlags = flags.FetchAnd(~Flags::IsDisabled);
		if (previousFlags.IsSet(Flags::IsDisabled) & (!previousFlags.AreAnySet(Flags::IsDisabledFromAnySource & ~Flags::IsDisabled)))
		{
			EnableInternal(sceneRegistry);
		}
	}

	void HierarchyComponentBase::EnableWithChildren()
	{
		EnableWithChildren(GetSceneRegistry());
	}
	void HierarchyComponentBase::EnableWithChildren(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		ComponentTypeSceneData<Data::Parent>& parentSceneData = sceneRegistry.GetCachedSceneData<Data::Parent>();

		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		const Entity::ComponentIdentifier parentIdentifier =
			Entity::ComponentIdentifier::MakeFromValidIndex(parentSceneData.GetComponentImplementationUnchecked(GetIdentifier()).Get());
		const EnumFlags<Flags> parentFlags = flagsSceneData.GetComponentImplementationUnchecked(parentIdentifier);

		if (flags.IsSet(ComponentFlags::WasDisabledByParent) && !parentFlags.AreAnySet(Flags::IsDisabledWithChildren | Flags::WasDisabledByParent))
		{
			flags &= ~ComponentFlags::WasDisabledByParent;
		}

		const EnumFlags<ComponentFlags> previousFlags = flags.FetchAnd(~Flags::IsDisabledWithChildren);
		if (previousFlags.IsSet(Flags::IsDisabledWithChildren) & (!previousFlags.AreAnySet(Flags::IsDisabledFromAnySource & ~Flags::IsDisabledWithChildren)))
		{
			EnableInternal(sceneRegistry);

			using EnableChildrenFunction = void (*)(
				HierarchyComponentBase& component,
				Entity::SceneRegistry& sceneRegistry,
				ComponentTypeSceneData<Data::Flags>& flagsSceneData
			);
			static EnableChildrenFunction enableChildren =
				[](HierarchyComponentBase& parent, Entity::SceneRegistry& sceneRegistry, ComponentTypeSceneData<Data::Flags>& flagsSceneData)
			{
				for (Entity::HierarchyComponentBase& component : parent.GetChildren())
				{
					AtomicEnumFlags<Flags>& childFlags = flagsSceneData.GetComponentImplementationUnchecked(component.GetIdentifier());
					const EnumFlags<ComponentFlags> previousChildFlags = childFlags.FetchAnd(~Flags::WasDisabledByParent);
					if (previousChildFlags.IsSet(Flags::WasDisabledByParent))
					{
						if (!previousChildFlags.AreAnySet(Flags::IsDisabledFromAnySource & ~Flags::WasDisabledByParent))
						{
							component.EnableInternal(sceneRegistry);
						}

						if (!previousChildFlags.IsSet(Flags::IsDisabledWithChildren))
						{
							enableChildren(component, sceneRegistry, flagsSceneData);
						}
					}
				}
			};

			enableChildren(*this, sceneRegistry, flagsSceneData);
		}
	}

	void HierarchyComponentBase::DetachFromOctree()
	{
		DetachFromOctree(GetSceneRegistry());
	}
	void HierarchyComponentBase::DetachFromOctree(Entity::SceneRegistry& sceneRegistry)
	{
		const Optional<HierarchyComponentBase*> pParent = GetParentSafe();
		Assert(pParent.IsValid());
		if (UNLIKELY_ERROR(pParent.IsInvalid()))
		{
			return;
		}

		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());
		const EnumFlags<ComponentFlags> previousFlags = flags.FetchOr(ComponentFlags::IsDetachedFromTree);
		if (!previousFlags.IsSet(Flags::IsDetachedFromTree))
		{
			pParent->RemoveChild(*this);
		}

		if (!previousFlags.AreAnySet(Flags::IsDetachedFromTreeFromAnySource))
		{
			Optional<ComponentTypeSceneDataInterface*> pSceneData = GetTypeSceneData();
			pSceneData->DetachInstanceFromTree(*this, GetParentSafe());

			using RemoveHierarchyFromOctree =
				void (*)(Entity::HierarchyComponentBase& component, ComponentTypeSceneData<Data::Flags>& flagsSceneData);
			static RemoveHierarchyFromOctree removeHierarchyFromOctree =
				[](Entity::HierarchyComponentBase& component, ComponentTypeSceneData<Data::Flags>& flagsSceneData)
			{
				AtomicEnumFlags<Flags>& childFlags = flagsSceneData.GetComponentImplementationUnchecked(component.GetIdentifier());
				const EnumFlags<ComponentFlags> previousChildFlags = childFlags.FetchOr(Flags::WasDetachedFromOctreeByParent);
				Assert(!previousChildFlags.IsSet(Flags::WasDetachedFromOctreeByParent));
				if (!previousChildFlags.AreAnySet(Flags::IsDetachedFromTreeFromAnySource))
				{
					Optional<ComponentTypeSceneDataInterface*> pSceneData = component.GetTypeSceneData();
					pSceneData->DetachInstanceFromTree(component, component.GetParentSafe());

					for (Entity::HierarchyComponentBase& child : component.GetChildren())
					{
						removeHierarchyFromOctree(child, flagsSceneData);
					}
				}
			};

			for (Entity::HierarchyComponentBase& child : GetChildren())
			{
				removeHierarchyFromOctree(child, flagsSceneData);
			}
		}
	}

	void HierarchyComponentBase::AttachToOctree()
	{
		AttachToOctree(GetSceneRegistry());
	}
	void HierarchyComponentBase::AttachToOctree(Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Flags>& flagsSceneData = sceneRegistry.GetCachedSceneData<Data::Flags>();
		AtomicEnumFlags<Flags>& flags = flagsSceneData.GetComponentImplementationUnchecked(GetIdentifier());

		const Optional<HierarchyComponentBase*> pParent = GetParentSafe();
		Assert(pParent.IsValid());
		if (UNLIKELY_ERROR(pParent.IsInvalid()))
		{
			return;
		}

		const EnumFlags<Flags> parentFlags = flagsSceneData.GetComponentImplementationUnchecked(pParent->GetIdentifier());

		if (flags.IsSet(ComponentFlags::WasDetachedFromOctreeByParent) && !parentFlags.AreAnySet(Flags::IsDetachedFromTree | Flags::WasDetachedFromOctreeByParent))
		{
			[[maybe_unused]] const EnumFlags<ComponentFlags> previousFlags = flags.FetchAnd(~Flags::WasDetachedFromOctreeByParent);
			Assert(previousFlags.IsSet(ComponentFlags::IsDetachedFromTree));
		}

		const EnumFlags<ComponentFlags> previousFlags = flags.FetchAnd(~Flags::IsDetachedFromTree);
		if (previousFlags.IsSet(ComponentFlags::IsDetachedFromTree))
		{
			pParent->AddChild(*this, sceneRegistry);
		}

		if (previousFlags.IsSet(ComponentFlags::IsDetachedFromTree) & !previousFlags.AreAnySet(Flags::IsDetachedFromTreeFromAnySource & ~Flags::IsDetachedFromTree))
		{
			Optional<ComponentTypeSceneDataInterface*> pSceneData = GetTypeSceneData();
			pSceneData->AttachInstanceToTree(*this, GetParentSafe());

			using AddHierarchyToOctree = void (*)(Entity::HierarchyComponentBase& component, ComponentTypeSceneData<Data::Flags>& flagsSceneData);
			static AddHierarchyToOctree addHierarchyToOctree =
				[](Entity::HierarchyComponentBase& component, ComponentTypeSceneData<Data::Flags>& flagsSceneData)
			{
				Assert(component.IsDetachedFromTree());
				AtomicEnumFlags<Flags>& childFlags = flagsSceneData.GetComponentImplementationUnchecked(component.GetIdentifier());
				const EnumFlags<ComponentFlags> previousChildFlags = childFlags.FetchAnd(~Flags::WasDetachedFromOctreeByParent);
				Assert(previousChildFlags.IsSet(Flags::WasDetachedFromOctreeByParent));

				if (previousChildFlags.IsSet(Flags::WasDetachedFromOctreeByParent) & (!previousChildFlags.AreAnySet(Flags::IsDetachedFromTreeFromAnySource & ~Flags::WasDetachedFromOctreeByParent)))
				{
					Optional<ComponentTypeSceneDataInterface*> pSceneData = component.GetTypeSceneData();
					pSceneData->AttachInstanceToTree(component, component.GetParentSafe());

					for (Entity::HierarchyComponentBase& child : component.GetChildren())
					{
						addHierarchyToOctree(child, flagsSceneData);
					}
				}
			};

			for (Entity::HierarchyComponentBase& child : GetChildren())
			{
				addHierarchyToOctree(child, flagsSceneData);
			}
		}
	}

	PURE_STATICS Optional<HierarchyComponentBase*> HierarchyComponentBase::GetParentSceneComponent() const
	{
		Entity::SceneRegistry& sceneRegistry = GetSceneRegistry();
		if (LIKELY(!GetFlags(sceneRegistry).IsSet(ComponentFlags::IsRootScene)))
		{
			ReferenceWrapper<HierarchyComponentBase> closestSceneComponent = GetParent();
			while (!closestSceneComponent->IsScene(sceneRegistry))
			{
				closestSceneComponent = closestSceneComponent->GetParent();
			}

			Assert(closestSceneComponent->IsScene(sceneRegistry))
			return *closestSceneComponent;
		}
		else
		{
			return Invalid;
		}
	}

	bool HierarchyComponentBase::IsRenderItem(const SceneRegistry& sceneRegistry) const
	{
		return HasDataComponentOfType(sceneRegistry, sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::Identifier>().GetIdentifier());
	}
	bool HierarchyComponentBase::IsRenderItem() const
	{
		return IsRenderItem(GetSceneRegistry());
	}

	bool HierarchyComponentBase::IsStaticMesh(const SceneRegistry& sceneRegistry) const
	{
		return HasDataComponentOfType(
			sceneRegistry,
			sceneRegistry.GetCachedSceneData<Entity::Data::RenderItem::StaticMeshIdentifier>().GetIdentifier()
		);
	}
	bool HierarchyComponentBase::IsStaticMesh() const
	{
		return IsStaticMesh(GetSceneRegistry());
	}

	bool HierarchyComponentBase::IsScene(const SceneRegistry& sceneRegistry) const
	{
		return Is<RootSceneComponent>(sceneRegistry) || Is<SceneComponent>(sceneRegistry) ||
		       HasDataComponentOfType<Data::ExternalScene>(sceneRegistry);
	}
	bool HierarchyComponentBase::IsScene() const
	{
		return IsScene(GetSceneRegistry());
	}

	void HierarchyComponentBase::AddTag(const Tag::Identifier tag, Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Tags>& tagsSceneData = sceneRegistry.GetCachedSceneData<Data::Tags>();
		Optional<Data::Tags*> pTags = FindDataComponentOfType<Data::Tags>(tagsSceneData);
		if (pTags.IsValid())
		{
			pTags->SetTag(GetIdentifier(), sceneRegistry, tag);
		}
		else
		{
			Tag::Mask tagMask;
			tagMask.Set(tag);
			pTags = CreateDataComponent<Data::Tags>(
				tagsSceneData,
				Data::Tags::Initializer{Data::Tags::BaseType::DynamicInitializer{*this, sceneRegistry}, tagMask}
			);
			Assert(pTags.IsValid());
		}
	}

	void HierarchyComponentBase::AddTag(const Tag::Identifier tag)
	{
		AddTag(tag, GetSceneRegistry());
	}

	void HierarchyComponentBase::RemoveTag(const Tag::Identifier tag, Entity::SceneRegistry& sceneRegistry)
	{
		ComponentTypeSceneData<Data::Tags>& tagsSceneData = sceneRegistry.GetCachedSceneData<Data::Tags>();
		Optional<Data::Tags*> pTags = FindDataComponentOfType<Data::Tags>(tagsSceneData);
		if (pTags.IsValid())
		{
			pTags->ClearTag(GetIdentifier(), sceneRegistry, tag);
		}
	}

	void HierarchyComponentBase::RemoveTag(const Tag::Identifier tag)
	{
		RemoveTag(tag, GetSceneRegistry());
	}

	bool HierarchyComponentBase::HasTag(const Tag::Identifier tag, Entity::SceneRegistry& sceneRegistry) const
	{
		ComponentTypeSceneData<Data::Tags>& tagsSceneData = sceneRegistry.GetCachedSceneData<Data::Tags>();
		Optional<Data::Tags*> pTags = FindDataComponentOfType<Data::Tags>(tagsSceneData);
		if (pTags.IsValid())
		{
			return pTags->HasTag(tag);
		}
		else
		{
			return false;
		}
	}

	bool HierarchyComponentBase::HasTag(const Tag::Identifier tag) const
	{
		return HasTag(tag, GetSceneRegistry());
	}

	[[maybe_unused]] const bool wasHierarchyComponentBaseRegistered = Reflection::Registry::RegisterType<HierarchyComponentBase>();

	[[maybe_unused]] const bool wasTypeIndexTypeRegistered = Reflection::Registry::RegisterType<Data::TypeIndex>();
	[[maybe_unused]] const bool wasTypeIndexRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<Data::TypeIndex>>::Make()
	);

	[[maybe_unused]] const bool wasFlagsTypeRegistered = Reflection::Registry::RegisterType<Data::Flags>();
	[[maybe_unused]] const bool wasFlagsRegistered = Entity::ComponentRegistry::Register(UniquePtr<ComponentType<Data::Flags>>::Make());

	[[maybe_unused]] const bool wasPreferredIndexTypeRegistered = Reflection::Registry::RegisterType<Data::PreferredIndex>();
	[[maybe_unused]] const bool wasPreferredIndexRegistered =
		Entity::ComponentRegistry::Register(UniquePtr<ComponentType<Data::PreferredIndex>>::Make());
}
